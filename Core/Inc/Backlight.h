/*
 * Backlight.h
 *
 *  Created on: Jul 22, 2026
 *      Author: Win10
 */

#ifndef INC_BACKLIGHT_H_
#define INC_BACKLIGHT_H_

#define BL_MANUAL_OPERATION_MODE 0U
#define BL_AUTO_OPERATION_MODE 1U
#define BL_WORK_FAILURE 0
#define BL_WORK_OK 1

#define BH1780_ADDR_WRITE 0U

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "lcd1602_i2c.h"

// Инициализация модуля при старте
void BacklightInit(I2C_HandleTypeDef* hi2c2, TIM_HandleTypeDef* htim3, lcd1602_HandleTypeDef* lcd1602_Handle);

// Текущий признак исправности
uint8_t BacklightGetOperability(void);

// Выполнение регулярных задач модуля
void BacklightUpdate(void);

// Режим подсветки
uint8_t BacklightGetMode(void);

// Установить режим подсветки
void BacklightSetMode(uint8_t lang);

// Текущий уровень освещенности окружающей среды
uint16_t BacklightGetBrightness(void);

// Текущий уровень подсветки
uint16_t BacklightGetLightLevel(void);

void OnBacklightChangeLightLevel(uint16_t light);

// Текущий уровень подсветки
void BacklightSetLightLevel(uint16_t light);


#endif /* INC_BACKLIGHT_H_ */
