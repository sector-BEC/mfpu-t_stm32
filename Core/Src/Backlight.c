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

// Текущий уровень освещенности окружающей среды
uint16_t BacklightGetLightLevel()
{
	return lightLevel_;
}

