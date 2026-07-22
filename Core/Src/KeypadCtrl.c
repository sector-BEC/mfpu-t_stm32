/*
 * KeypadCtrl.c
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#include "KeypadCtrl.h"

#define KYEPAD_LANG_ENG 0U;
#define KYEPAD_LANG_RU 1U;

uint16_t lastKeyPress_ = 0;
uint8_t lang_ = KYEPAD_LANG_ENG;

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

// Последняя нажатая кнопка
uint16_t getKey()
{
	uint16_t result = lastKeyPress_;
	lastKeyPress_ = 0;
	return result;
}

// Текущий язык ввода на клавиатуре
uint8_t getLanguage()
{
	return lang_;
}

// Установить язык ввода на клавиатуре
void setLanguage(uint8_t lang)
{
	if(lang_ != lang)
	{
		lang_ = lang;
	}
}
