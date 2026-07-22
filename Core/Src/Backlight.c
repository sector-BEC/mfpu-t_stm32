/*
 * Backlight.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */


#include "Backlight.h"

#define BL_MANUAL_OPERATION_MODE 0U;
#define BL_AUTO_OPERATION_MODE 1U;

uint8_t lightMode_ = BL_AUTO_OPERATION_MODE;
uint16_t brightness_ = 0;
uint16_t lightLevel_ = 0;

// Инициализация модуля при старте
void init()
{
	//
}

// Текущий признак исправности
uint8_t getOperability()
{
	return 1;
}

// Выполнение регулярных задач модуля
void update()
{
	//
}

// Режим яркости подсветки
uint8_t getMode()
{
	return lightMode_;
}

// Текущая освещенность окружающей среды
uint16_t getBrightness()
{
	return brightness_;
}

// Текущий уровень освещенности окружающей среды
uint16_t getLightLevel()
{
	return lightLevel_;
}

// Установить режим подсветки
void setMode(uint8_t mode)
{
	if(mode_ != mode)
	{
		mode_ = mode;
	}
}
