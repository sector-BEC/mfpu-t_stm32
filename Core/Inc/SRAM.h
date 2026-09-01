/*
 * SRAM.h
 *
 *  Created on: Aug 13, 2026
 */

#ifndef SRAM_H_
#define SRAM_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

// Определяем адреса (для варианта, когда A0=A1=A2=GND)
#define FM24CL64B_WRITE_ADDR  0xA0U
#define FM24CL64B_READ_ADDR   0xA1U

#define SRAM_TIME_ADDRESS 		0x0U
#define SRAM_VERSION_ADDRESS	0x2U
#define SRAM_CHECKSUM_ADDRESS 	0x6U

// Инициализация модуля при старте
void SRAMInit(I2C_HandleTypeDef* hi2c2);

// Получить время работы устройства
uint16_t GetWorkTimeSRAM();

// Записать время работы устройства
void SetWorkTimeSRAM(uint16_t wtime);

// Получить версию прошивки
uint32_t GetSWVersion(void);

// Записать версию прошивки
void SetSWVersion(uint32_t version);

// Записать CRC16 прошивки
void SetSWCheckSum(uint16_t crc16);

// Получить CRC16 прошивки
uint16_t GetSWCheckSum(void);

// запись в память
//void SRAMWrite(void);

// запись блока в память
void SRAMWriteBlock(uint16_t address, uint8_t* cData);

// чтение из памяти
//void SRAMRead(void);

// чтение блока из памяти
uint8_t* SRAMReadBlock(uint16_t address, uint8_t size);

uint8_t* GetBuffer(void);
void SetBuffer(uint8_t data[]);


#endif /* SRAM_H_ */
