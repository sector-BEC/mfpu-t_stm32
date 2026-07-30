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

#include <stdint.h>

// Инициализация модуля при старте
void BacklightInit(void);

// Текущий признак исправности
uint8_t BacklightGetOperability(void);

// Выполнение регулярных задач модуля
void BacklightUpdate(void);

// Режим яркости подсветки
uint8_t BacklightGetMode(void);

// Установить режим подсветки
void BacklightSetMode(uint8_t lang);

// Текущая освещенность окружающей среды
uint16_t BacklightGetBrightness(void);

// Текущий уровень освещенности окружающей среды
uint16_t BacklightGetLightLevel(void);


#endif /* INC_BACKLIGHT_H_ */
