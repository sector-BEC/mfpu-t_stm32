/*
 * SRAM.h
 *
 *  Created on: Aug 13, 2026
 */

#ifndef SRAM_H_
#define SRAM_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

// Инициализация модуля при старте
void SRAMInit(void);

// Получить время работы устройства
uint16_t GetWorkTimeSRAM();

// Записать время работы устройства
void SetWorkTimeSRAM(uint16_t wtime);

// Получить версию прошивки
uint8_t GetSWVersion(void);

// Получить CRC16 прошивки
uint16_t GetSWCheckSum(void);


#endif /* SRAM_H_ */
