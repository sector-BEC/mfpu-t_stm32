/*
 * Backlight.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */


#include "Backlight.h"

uint8_t lightWork_ = BL_WORK_FAILURE;
uint8_t lightMode_ = BL_AUTO_OPERATION_MODE;
uint16_t brightness_ = 0;
uint16_t lightLevel_ = 0;
uint16_t needLightLevel_ = 0;
TIM_HandleTypeDef* lightTimer;
I2C_HandleTypeDef* backLightHi2c2;

// Возвращает 1 если датчик отвечает, 0 если нет
uint8_t BH1780_CheckPresence(void) {
	return (HAL_I2C_IsDeviceReady(&backLightHi2c2, BH1780_ADDR_WRITE, 2, 100) == HAL_OK);
}

// Инициализация модуля при старте
void BacklightInit(I2C_HandleTypeDef* hi2c2, TIM_HandleTypeDef* htim3)
{
	backLightHi2c2 = hi2c;
	lightTimer = htim3;
	if(BH1780_CheckPresence() > 0)
	{
		lightWork_ = BL_WORK_OK;
	}
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
		if(lightLevel_ > needLightLevel_)
		{
			if(lightLevel_ + 100 > needLightLevel_)
			{
				OnBacklightChangeLightLevel(lightLevel_ + 100);
			}
			else
			{
				OnBacklightChangeLightLevel(needLightLevel_);
			}
		}
		else if(lightLevel_ < needLightLevel_)
		{
			if(lightLevel_ - 100 < needLightLevel_)
			{
				OnBacklightChangeLightLevel(lightLevel_ - 100);
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
	return lightLevel_;
}

void OnBacklightChangeLightLevel(uint16_t light)
{
	lightLevel_ = light;
	//TIM->CCR = lightLevel_
}

void BacklightSetLightLevel(uint16_t light)
{
 	if(needLightLevel_ != light)
 	{
 		needLightLevel_ = light;
 	}
}

