/*
 * Backlight.h
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#ifndef INC_BACKLIGHT_H_
#define INC_BACKLIGHT_H_

#define BL_MANUAL_OPERATION_MODE 0U;
#define BL_AUTO_OPERATION_MODE 1U;
#define BL_WORK_FAILURE 0;
#define BL_WORK_OK 1;

// Инициализация модуля при старте
void init(void);

// Текущий признак исправности
uint8_t getOperability(void);

// Выполнение регулярных задач модуля
void update(void);

// Режим яркости подсветки
uint8_t getMode(void);

// Текущая освещенность окружающей среды
uint16_t getBrightness(void);

// Текущий уровень освещенности окружающей среды
uint16_t getLightLevel(void);

// Установить режим подсветки
void setMode(uint8_t lang);

#endif /* INC_BACKLIGHT_H_ */
