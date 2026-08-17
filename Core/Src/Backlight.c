/*
 * Backlight.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */


#include "Backlight.h"
#include "PCF8575.h"

uint8_t lightWork_ = BL_WORK_FAILURE;
uint8_t lightMode_ = BL_AUTO_OPERATION_MODE;
uint16_t backlightBrightness_ = 0;
uint16_t lightLevel_ = 0;
uint16_t needLightLevel_ = 0;
TIM_HandleTypeDef* lightTimer;
I2C_HandleTypeDef* backlightHi2c2;
lcd1602_HandleTypeDef* backlightLCD1602_Handle;

// Адрес датчика (0x23 << 1 = 0x46), так как HAL требует 7-битный адрес со сдвигом влево
#define BH1750_ADDR         0x46
#define BH1750_ADDR2     	0x23
// Команды датчика
#define BH1750_POWER_ON     0x01
#define BH1750_RESET        0x07
#define BH1750_CONT_H_RES   0x10  // Непрерывный режим, высокая точность (1 лк)

//light
//uint8_t P0[2] = {0X01, 0x00};
uint8_t P0[2] = {0X00, 0x00};
uint8_t P1[2] = {0X02, 0x00};

uint8_t P2[2] = {0X04, 0x00};
uint8_t P3[2] = {0X08, 0x00};

uint8_t P4[2] = {0X10, 0x00};
uint8_t P5[2] = {0X20, 0x00};

uint8_t P6[2] = {0X40, 0x00};
uint8_t P7[2] = {0X80, 0x00};

uint8_t P8 [2] = {0X00, 0x01};
uint8_t P9 [2] = {0X00, 0x02};

uint8_t P10[2] = {0X00, 0x04};
uint8_t P11[2] = {0X00, 0x08};

uint8_t P12[2] = {0X00, 0x10};
uint8_t P13[2] = {0X00, 0x20};

uint8_t P14[2] = {0X00, 0x40};
uint8_t P15[2] = {0X00, 0x80};

uint8_t B1_W[2] = {0X00, 0x40};
uint8_t B1_G[2] = {0X00, 0xC0};

uint8_t B2_W[2] = {0X00, 0x10};
uint8_t B2_G[2] = {0X00, 0x30};

uint8_t B3_W[2] = {0X00, 0x04};
uint8_t B3_G[2] = {0X00, 0x0C};

uint8_t B4_W[2] = {0X00, 0x35};
uint8_t B4_G[2] = {0X00, 0x36};

uint8_t B6_W[2] = {0X10, 0x00};
uint8_t B6_G[2] = {0X30, 0x00};

// Функция отправки команды
void BH1750_SendCommand(uint8_t cmd) {
    HAL_I2C_Master_Transmit(backlightHi2c2, BH1750_ADDR, &cmd, 1, HAL_MAX_DELAY);
}

// Функция чтения данных (освещенность в Люксах)
uint16_t BH1750_ReadLight(void) {
    uint8_t data[2] = {0};

    // 1. Включить датчик
    BH1750_SendCommand(BH1750_POWER_ON);
    HAL_Delay(10); // Нужно время на запуск

    // 2. Отправить команду начала измерения (H-Resolution Mode)
    BH1750_SendCommand(BH1750_CONT_H_RES);

    // 3. Ждем окончания измерения (~180 мс для высокого разрешения)
    HAL_Delay(200);

    // 4. Читаем 2 байта данных из датчика (регистр данных)
    HAL_I2C_Master_Receive(backlightHi2c2, BH1750_ADDR, data, 2, HAL_MAX_DELAY);

    // 5. Переводим 2 байта в значение (старший байт + младший)
    uint16_t lux = (data[0] << 8) | data[1];

    // Для режима H-Resolution результат нужно разделить на 1.2
    // (умножаем на 100 и делим на 120, чтобы избежать float)
    lux = (lux * 100) / 120;

    return lux;
}

// Возвращает 1 если датчик отвечает, 0 если нет
uint8_t BH1780_CheckPresence(void) {
	if (HAL_I2C_IsDeviceReady(backlightHi2c2, (BH1750_ADDR2 << 1), 2, 10) == HAL_OK)
	{
		lightWork_ = BL_WORK_OK;
		return 1;
	}
	else
	{
		lightWork_ = BL_WORK_FAILURE;
	}
	return 0;
	//return (HAL_I2C_IsDeviceReady(backlightHi2c2, BH1780_ADDR_WRITE, 2, 100) == HAL_OK);
}

// Инициализация модуля при старте
void BacklightInit(I2C_HandleTypeDef* hi2c2, TIM_HandleTypeDef* htim3, lcd1602_HandleTypeDef* lcd1602_Handle)
{
	//hi2c
	backlightHi2c2 = hi2c2;
	BH1780_CheckPresence();

	//TIM3
	lightTimer = htim3;
	HAL_TIM_PWM_Start_IT(lightTimer, TIM_CHANNEL_1);
	TIM1->CCR1 = needLightLevel_;

	backlightLCD1602_Handle = lcd1602_Handle;

	PCF8575_Init(0x40, 100);
	PCF8575_Reset(backlightHi2c2);

}

// Текущий признак исправности
uint8_t BacklightGetOperability()
{
	return lightWork_;
}

// Выполнение регулярных задач модуля
void BacklightUpdate()
{
	// Проверка на работоспособность
	BH1780_CheckPresence();

	if(lightWork_ == BL_WORK_OK)
	{
		if(lightMode_ == BL_AUTO_OPERATION_MODE)
		{
			// Получение значения освещенности окружающей среды
			backlightBrightness_ = BH1750_ReadLight();

			uint8_t str[6];
			lcd1602_SetCursor(backlightLCD1602_Handle, 0, 1);
			lcd1602_Print(backlightLCD1602_Handle, "______");
			snprintf((char*)str, sizeof(str), "%lu", backlightBrightness_);
			lcd1602_SetCursor(backlightLCD1602_Handle, 0, 1);
			lcd1602_Print(backlightLCD1602_Handle, str);

			if(TIM1->CCR1 > needLightLevel_)
			{
				if(TIM1->CCR1 + 100 > needLightLevel_)
				{
					OnBacklightChangeLightLevel(TIM1->CCR1 + 100);
				}
				else
				{
					OnBacklightChangeLightLevel(needLightLevel_);
				}
			}
			else if(TIM1->CCR1 < needLightLevel_)
			{
				if(TIM1->CCR1 - 100 < needLightLevel_)
				{
					OnBacklightChangeLightLevel(TIM1->CCR1 - 100);
				}
				else
				{
					OnBacklightChangeLightLevel(needLightLevel_);
				}
			}
		}
	}
	else
	{
		lcd1602_SetCursor(backlightLCD1602_Handle, 0, 1);
		lcd1602_Print(backlightLCD1602_Handle, "______");
	}
}

// Режим работы подсветки
uint8_t BacklightGetMode()
{
	return lightMode_;
}

// Установить режим подсветки
void BacklightSetMode(uint8_t mode)
{
	if(lightMode_ != mode)
	{
		lightMode_ = mode;
	}
}

// Текущая освещенность окружающей среды
uint16_t BacklightGetBrightness()
{
	return backlightBrightness_;
}

// Текущий уровень подсветки
uint16_t BacklightGetLightLevel()
{
	return TIM1->CCR1;
}

void OnBacklightChangeLightLevel(uint16_t light)
{
	TIM1->CCR1 = light;
	HAL_Delay(100);
	PCF8575_write(backlightHi2c2, B1_W);
}

void BacklightSetLightLevel(uint16_t light)
{
 	if(needLightLevel_ != light)
 	{
 		needLightLevel_ = light;
 	}
}

