/*
 * KeypadCtrl.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#include "KeypadCtrl.h"
#include "SRAM.h"
#include "Backlight.h"

uint8_t keypadCtrlWork_ = KEYPAD_WORK_FAILURE;
uint16_t lastKeyPress_ = 0;
uint8_t lang_ = KEYPAD_LANG_ENG;

static I2C_HandleTypeDef* keypadHi2c2;

uint8_t intTCA8418	= 0;							// флаг прерывания от клавиатур
uint8_t regValueKey = 0;
uint8_t regAddresValueKEY = 0x04;
uint16_t TCA8418_I2C_ADDR = 0x68;
uint8_t data_to_write_1[10] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
uint8_t data_to_write_2[10] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

// Код клавишь которые должны возвращать при нажатии клавишь. Реальные коды ниже
static const uint8_t hexKeyPadCode[] = {
	//
};

// Код клавишь которые нажимает оператор 1-0, A-Z, А-Я
static const uint8_t keyPadCodeASCII [] = {
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, // 1-0
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, // A-O
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A // P-Z
};

uint8_t test_read;

void messageTX(void)
{
	//
}

void KeypadCtrlVerifyWork()
{
	keypadCtrlWork_ = KEYPAD_WORK_FAILURE;
	if(HAL_I2C_IsDeviceReady(keypadHi2c2, (uint16_t)0x68, 1, 100))
	{
		keypadCtrlWork_ = KEYPAD_WORK_OK;
		if(KeypadCtrlRegisterRwTest() == 0)
		{
			keypadCtrlWork_ = KEYPAD_WORK_FAILURE;
		}
	}
}

// Инициализация модуля при старте
void KeypadCtrlInit(I2C_HandleTypeDef* hi2c2_origin)
{
	keypadHi2c2 = hi2c2_origin;
// I2C Okay?
	KeypadCtrlVerifyWork();
	if(keypadCtrlWork_ == KEYPAD_WORK_FAILURE)
	{
		return;
	}
// TCA8418 init
	uint8_t TCA8418_COL7[2] = {0x1D, 0x3F};
	HAL_I2C_Master_Transmit(keypadHi2c2, 0x68, TCA8418_COL7, sizeof(TCA8418_COL7), 1);
	uint8_t TCA8418_COL9[2] = {0x1E, 0xFF};
	HAL_I2C_Master_Transmit(keypadHi2c2, 0x68, TCA8418_COL9, sizeof(TCA8418_COL9), 1);
	uint8_t TCA8418_ROW5[2] = {0x1F, 0x03};
	HAL_I2C_Master_Transmit(keypadHi2c2, 0x68, TCA8418_ROW5, sizeof(TCA8418_ROW5), 1);
	HAL_Delay(2);
// enable interrup TCA8418
	uint8_t outbuffer_4[2] = {0x01, 0x91};
	HAL_I2C_Master_Transmit(keypadHi2c2, 0x68, outbuffer_4, sizeof(outbuffer_4), 1);


	// Клавиатурный матричный контроллер
    uint8_t data[3];

    // 1. Настройка GPIO: выбираем, какие пины будут строками и столбцами
    // Регистр KP_GPIO1 (0x2C): биты 0-7 для COL0-7, биты 0-3 для ROW0-3
    data[0] = 0xFF; // Все COL0-7 как столбцы
    data[1] = 0x07; // ROW0-2 как строки (биты 0,1,2)
    data[2] = 0x00; // ROW3-7 как GPIO (если не используются)
    HAL_I2C_Mem_Write(keypadHi2c2, 0x34, 0x2C, 1, data, 3, 100);

    // 2. Настройка прерываний
    data[0] = 0x91; // Включить прерывания по нажатию/отпусканию
    HAL_I2C_Mem_Write(keypadHi2c2, 0x34, 0x01, 1, data, 1, 100);
}


// Проверим, что мы можем изменять конфигурацию и что работает механизм автоинкремента адреса (бит 7 регистра `CFG`).
uint8_t KeypadCtrlRegisterRwTest()
{
    uint8_t tx_data, rx_data;

    // 1. Записываем и читаем регистр GPIO_DIR1 (0x23)
    tx_data = 0xA5; 
    HAL_I2C_Mem_Write(keypadHi2c2, TCA8418_I2C_ADDR, 0x23, I2C_MEMADD_SIZE_8BIT, &tx_data, 1, I2C_TIMEOUT);
    HAL_I2C_Mem_Read(keypadHi2c2, TCA8418_I2C_ADDR, 0x23, I2C_MEMADD_SIZE_8BIT, &rx_data, 1, I2C_TIMEOUT);
    if (rx_data != 0xA5) return 0;

    // 2. Включаем Auto-Increment (бит 7 в регистре CFG = 1)
    tx_data = 0x80; 
    HAL_I2C_Mem_Write(hi2c, TCA8418_I2C_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, &tx_data, 1, I2C_TIMEOUT);
    
    // Читаем 3 байта начиная с 0x01. Должны прочитать CFG, INT_STAT, KEY_LCK_EC
    uint8_t buffer[3] = {0};
    HAL_I2C_Mem_Read(hi2c, TCA8418_I2C_ADDR, 0x01, I2C_MEMADD_SIZE_8BIT, buffer, 3, I2C_TIMEOUT);
    
    if (buffer[0] != 0x80) return 0; // CFG
    // buffer[1] и buffer[2] должны быть 0x00 (если не было нажатий)
    
    return 1;
}

// Текущий признак исправности
uint8_t KeypadCtrlGetOperability()
{
	return keypadCtrlWork_;
}


// Выполнение регулярных задач модуля
void KeypadCtrlUpdate()
{
	KeypadCtrlVerifyWork();
	if(keypadCtrlWork_ == KEYPAD_WORK_FAILURE)
	{
		return;
	}

	/******************************************************************************************/
	/* Отработка нажатий кнопок TCA8418																												*/
	/******************************************************************************************/
    uint8_t int_status = 0;
    uint8_t event_count = 0;
    uint8_t key_event = 0;

    // 1. Читаем статус прерываний
    HAL_I2C_Mem_Read(keypadHi2c2, TCA8418_I2C_ADDR, 0x02, 1, &int_status, 1, 100);

    if (int_status) {
        // 2. Читаем количество событий в очереди
        HAL_I2C_Mem_Read(keypadHi2c2, TCA8418_I2C_ADDR, 0x03, 1, &event_count, 1, 100);

        // 3. Читаем событие (если есть)
        if (event_count > 0) {
            HAL_I2C_Mem_Read(keypadHi2c2, TCA8418_I2C_ADDR, 0x04, 1, &key_event, 1, 100);
            // Анализируем key_event: бит 7 = 1 (нажатие), 0 (отпускание)
            // биты 0-6 = код клавиши
        }

        if(key_event == 129 || key_event == 1)
        {
        	SRAMSetBuffer(data_to_write_1);
        	HAL_Delay(10);
			//SRAMRead();
			BacklightUpdate();
        }
        else
		if(key_event == 139 || key_event == 11)
		{
			SRAMSetBuffer(data_to_write_2);
			HAL_Delay(10);
			//SRAMRead();
			BacklightUpdate();
		}

        // 4. Очищаем флаг прерывания
        uint8_t clear_cmd = 0x01;
        HAL_I2C_Mem_Write(keypadHi2c2, TCA8418_I2C_ADDR, 0x02, 1, &clear_cmd, 1, 100);
    }
}

// Последняя нажатая кнопка
uint16_t KeypadCtrlGetKey()
{
	uint16_t result = lastKeyPress_;
	lastKeyPress_ = 0;
	return result;
}

// Текущий язык ввода на клавиатуре
uint8_t KeypadCtrlGetLanguage()
{
	return lang_;
}

// Установить язык ввода на клавиатуре
void KeypadCtrlSetLanguage(uint8_t lang)
{
	if(lang_ != lang)
	{
		lang_ = lang;
	}
}
