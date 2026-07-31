/*
 * KeypadCtrl.h
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#ifndef INC_KEYPADCTRL_H_
#define INC_KEYPADCTRL_H_

#define KEYPAD_LANG_ENG 0U
#define KEYPAD_LANG_RU 1U
#define KEYPAD_WORK_FAILURE 0U
#define KEYPAD_WORK_OK 1U

#include <stdint.h>
#include "stm32f4xx_hal.h"

// Инициализация модуля при старте
void KeypadCtrlInit(I2C_HandleTypeDef* hi2c2_origin);

// Текущий признак исправности
uint8_t KeypadCtrlGetOperability(void);

// Выполнение регулярных задач модуля
void KeypadCtrlUpdate(void);

// Последняя нажатая кнопка
uint16_t KeypadCtrlGetKey(void);

// Текущий язык ввода на клавиатуре
uint8_t KeypadCtrlGetLanguage(void);

// Установить язык ввода на клавиатуре
void KeypadCtrlSetLanguage(uint8_t lang);

#endif /* INC_KEYPADCTRL_H_ */
