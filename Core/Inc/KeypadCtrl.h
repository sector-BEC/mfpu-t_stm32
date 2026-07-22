/*
 * KeypadCtrl.h
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#ifndef INC_KEYPADCTRL_H_
#define INC_KEYPADCTRL_H_

#define KEYPAD_LANG_ENG 0U;
#define KEYPAD_LANG_RU 1U;
#define KEYPAD_WORK_FAILURE 0;
#define KEYPAD_WORK_OK 1;

// Инициализация модуля при старте
void init(void);

// Текущий признак исправности
uint8_t getOperability(void);

// Выполнение регулярных задач модуля
void update(void);

// Последняя нажатая кнопка
uint16_t getKey(void);

// Текущий язык ввода на клавиатуре
uint8_t getLanguage(void);

// Установить язык ввода на клавиатуре
void setLanguage(uint8_t lang);

#endif /* INC_KEYPADCTRL_H_ */
