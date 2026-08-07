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
uint16_t brightness_ = 0;
uint16_t lightLevel_ = 0;
uint16_t needLightLevel_ = 0;
TIM_HandleTypeDef* lightTimer;
I2C_HandleTypeDef* backlightHi2c2;

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

// Возвращает 1 если датчик отвечает, 0 если нет
uint8_t BH1780_CheckPresence(void) {
	return (HAL_I2C_IsDeviceReady(backlightHi2c2, BH1780_ADDR_WRITE, 2, 100) == HAL_OK);
}

// Инициализация модуля при старте
void BacklightInit(I2C_HandleTypeDef* hi2c2, TIM_HandleTypeDef* htim3)
{
	//hi2c
	backlightHi2c2 = hi2c2;
	if(BH1780_CheckPresence() > 0)
	{
		lightWork_ = BL_WORK_OK;
	}

	//TIM3
	lightTimer = htim3;
	HAL_TIM_PWM_Start_IT(lightTimer, TIM_CHANNEL_1);

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
	if(lightMode_ == BL_AUTO_OPERATION_MODE)
	{
		// Получение значения освещенности окружающей среды
		//HAL_ADC_Start(&hadc1); // Запуск
		if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) { // Ожидание (таймаут 10мс)
			brightness_ = HAL_ADC_GetValue(&hadc1); // Считываем результат
			//needLightLevel_ = ?
		}
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
	return brightness_;
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

