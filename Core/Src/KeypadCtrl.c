/*
 * KeypadCtrl.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#include "KeypadCtrl.h"

uint16_t lastKeyPress_ = 0;
uint8_t lang_ = KEYPAD_LANG_ENG;

static I2C_HandleTypeDef* hi2c2;

uint8_t intTCA8418	= 0;							// флаг прерывания от клавиатур
uint8_t regValueKey = 0;
uint8_t regAddresValueKEY = 0x04;
uint16_t TCA8418_I2C_ADDR = 0x34;

static const uint8_t ASCII [] = {
	0x62, 0x63, 0x64, 0x65, 0x38, 0x37, 0x39, 0x66, 0x30, 0x26,
	0x5A, 0x58, 0x43, 0x56, 0x42, 0x4E, 0x4D, 0x61, 0x4C, 0x25,
	0x41, 0x53,	0x44, 0x46, 0x47, 0x48, 0x4A, 0x4B, 0x50, 0x22,
	0x51, 0x57, 0x45,	0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x24,
	0x31,	0x32,	0x33, 0x34,	0x35, 0x36, 0x70, 0x72, 0x71, 0x23,
	0x67, 0x68, 0x69, 0x6A, 0x6B,	0x6C, 0x6D, 0x6F, 0x6E, 0x21
};

uint8_t message_KEY_DOWN[]	={0x24,	0x30,	0x31,	0x3A,	0x30,	0x33,	0x3A,	0x4B,	0x45, 0x59, 0x3A, 0x7E, 0x00,	0x0A};
//															$			0			1			:			0			3			:			K			E			Y			:			~		ASSCI		\n
uint8_t message_KEY_UP[]		={0x24,	0x30,	0x31,	0x3A,	0x30,	0x33,	0x3A,	0x4B,	0x45, 0x59, 0x3A, 0x00,	0x0A};
//															$			0			1			:			0			3			:			K			E			Y			:		ASSCI		\n

// Инициализация модуля при старте
void KeypadCtrlInit(I2C_HandleTypeDef* hi2c2_origin)
{
	hi2c2 = hi2c2_origin;
// I2C Okay?
	HAL_I2C_IsDeviceReady(hi2c2, (uint16_t)0x68, 1, 100);
// TCA8418 init
	uint8_t TCA8418_COL7[2] = {0x1D, 0x3F};
	HAL_I2C_Master_Transmit(hi2c2, 0x68, TCA8418_COL7, sizeof(TCA8418_COL7), 1);
	uint8_t TCA8418_COL9[2] = {0x1E, 0xFF};
	HAL_I2C_Master_Transmit(hi2c2, 0x68, TCA8418_COL9, sizeof(TCA8418_COL9), 1);
	uint8_t TCA8418_ROW5[2] = {0x1F, 0x03};
	HAL_I2C_Master_Transmit(hi2c2, 0x68, TCA8418_ROW5, sizeof(TCA8418_ROW5), 1);
	HAL_Delay(2);
// enable interrup TCA8418
	uint8_t outbuffer_4[2] = {0x01, 0x91};
	HAL_I2C_Master_Transmit(hi2c2, 0x68, outbuffer_4, sizeof(outbuffer_4), 1);
}

// Текущий признак исправности
uint8_t KeypadCtrlGetOperability()
{
	return KEYPAD_WORK_OK;
}


// Выполнение регулярных задач модуля
void KeypadCtrlUpdate()
{
	/******************************************************************************************/
	/* Отработка нажатий кнопок TCA8418																												*/
	/******************************************************************************************/
	if(intTCA8418 == 1)
	{
		HAL_I2C_Master_Transmit(&hi2c2, (uint16_t)(TCA8418_I2C_ADDR << 1), &regAddresValueKEY, 1, 1);
		HAL_I2C_Master_Receive(&hi2c2, (uint16_t)(TCA8418_I2C_ADDR << 1), &regValueKey, 1, 1);
		uint8_t *ValueKey = &regValueKey;
		if (*ValueKey != 0)
		{
			if (((*ValueKey) & (1 << 7)) == 0)
			{
				message_KEY_DOWN[12] = ASCII[*ValueKey-1];                // reset bit 7
				messageTX();																							// отправка кода отпущенной кнопки
			}
			else if (((*ValueKey) & (1 << 7)) != 0)
			{
				message_KEY_UP[11] = ASCII[((*ValueKey) & 0x7F)-1];
				messageTX();																			// отправка кода нажатой кнопки
			}
		}
		intTCA8418=0;																											// сбросили флаг обработки прерывания
		uint8_t clearInterrup[2] = {0x02, 0x1F};
		HAL_I2C_Master_Transmit(&hi2c2, (uint16_t)(TCA8418_I2C_ADDR << 1), clearInterrup, sizeof(clearInterrup), 1);
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
