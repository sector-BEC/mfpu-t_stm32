#ifndef INC_ARINC_WORDS_H_
#define INC_ARINC_WORDS_H_

#include <stdint.h>
#include <stdbool.h>

/* ISS2, табл. 3, 7, 13. */
#define ARINC_ADDR_SU1 190u
#define ARINC_ADDR_SD1 191u
#define ARINC_ADDR_SS1 192u
#define ARINC_ADDR_SD2 193u
#define ARINC_ADDR_SD3 194u
#define ARINC_ADDR_SD4 195u
#define ARINC_ADDR_SD5 196u
#define ARINC_ADDR_SS2 197u
#define ARINC_ADDR_SD6 198u

typedef enum {
    ARINC_MODE_WORK = 0,
    ARINC_MODE_TEST_CONTROL = 1
} ArincMode;

typedef enum {
    ARINC_MATRIX_FAULT = 0,
    ARINC_MATRIX_NO_DATA,
    ARINC_MATRIX_TEST,
    ARINC_MATRIX_NORMAL
} ArincMatrix;

typedef enum {
    ARINC_WORD_K = 0,
    ARINC_WORD_DK = 1
} ArincWordType;

/* 21 бит: разряды 9..29, bit0 соответствует разряду 9. */
void ARINC_PackWord(uint8_t address,
                    uint32_t data21,
                    ArincMatrix matrix,
                    ArincWordType type,
                    uint8_t out[4]);

bool ARINC_UnpackWord(const uint8_t word[4],
                      uint8_t *address,
                      uint32_t *data21,
                      ArincMatrix *matrix,
                      ArincWordType type);

/* Массив М1: СУ1 + СД1. */
typedef struct {
    uint8_t mode;
    uint8_t backlight_auto;
    uint8_t brightness;
    ArincMatrix matrix;
} ArincSu1;

typedef struct {
    uint8_t key_code;
    uint8_t key_id;
    ArincMatrix matrix;
} ArincSd1;

bool ARINC_ParseSu1(const uint8_t word[4], ArincSu1 *out);
bool ARINC_ParseSd1(const uint8_t word[4], ArincSd1 *out);

/* М2. */
void ARINC_BuildSs1(uint8_t ready,
                    uint8_t layout_rus,
                    uint8_t sw_version,
                    uint8_t ls1_ok,
                    uint8_t ls2_ok,
                    ArincMatrix matrix,
                    uint8_t out[4]);

void ARINC_BuildSd2(uint8_t backlight_auto,
                    uint8_t backlight_level,
                    uint8_t illumination_level,
                    ArincMatrix matrix,
                    uint8_t out[4]);

void ARINC_BuildSd3(uint32_t uptime_minutes,
                    ArincMatrix matrix,
                    uint8_t out[4]);

void ARINC_BuildSd4(uint16_t crc16,
                    ArincMatrix matrix,
                    uint8_t out[4]);

void ARINC_BuildSd5(uint8_t key_code,
                    uint8_t key_id,
                    ArincMatrix matrix,
                    uint8_t out[4]);

/* М3. */
void ARINC_BuildSs2(uint8_t layout_rus,
                    uint8_t sw_version,
                    uint8_t backlight_ok,
                    uint8_t keypad_ok,
                    uint8_t illumination_ok,
                    uint8_t power_ok,
                    uint8_t arinc_ok,
                    uint8_t ls1_ok,
                    uint8_t ls2_ok,
                    ArincMatrix matrix,
                    uint8_t out[4]);

/* Напряжения передаются в сотых долях вольта: 3.30 В -> 330. */
void ARINC_BuildSd6(uint16_t voltage_3v3_centi_volt,
                    uint16_t voltage_5v_centi_volt,
                    ArincMatrix matrix,
                    uint8_t out[4]);

#endif
