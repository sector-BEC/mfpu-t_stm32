/*
 * Backlight.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */


#include "Backlight.h"

uint8_t lightMode_ = BL_AUTO_OPERATION_MODE;
uint16_t brightness_ = 0;
uint16_t lightLevel_ = 0;
uint16_t needLightLevel_ = 0;

// Инициализация модуля при старте
void BacklightInit()
{
	//
}

// Текущий признак исправности
uint8_t BacklightGetOperability()
{
	return BL_WORK_OK;
}

// Выполнение регулярных задач модуля
void BacklightUpdate()
{
	//
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

