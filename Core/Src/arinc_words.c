#include "arinc_words.h"
#include <stddef.h>

static uint8_t matrix_to_raw(ArincMatrix matrix, ArincWordType type)
{
    if (type == ARINC_WORD_DK) {
        switch (matrix) {
        case ARINC_MATRIX_FAULT:   return 0u; /* 00 */
        case ARINC_MATRIX_NO_DATA: return 1u; /* 01 */
        case ARINC_MATRIX_TEST:    return 2u; /* 10 */
        case ARINC_MATRIX_NORMAL:  return 3u; /* 11 */
        default:                   return 0u;
        }
    }

    /* Для К: 00 - норма, 01 - нет данных, 10 - тест, 11 - отказ. */
    switch (matrix) {
    case ARINC_MATRIX_FAULT:   return 3u;
    case ARINC_MATRIX_NO_DATA: return 1u;
    case ARINC_MATRIX_TEST:    return 2u;
    case ARINC_MATRIX_NORMAL:  return 0u;
    default:                   return 0u;
    }
}

static ArincMatrix raw_to_matrix(uint8_t raw, ArincWordType type)
{
    raw &= 0x03u;

    if (type == ARINC_WORD_DK) {
        switch (raw) {
        case 0u: return ARINC_MATRIX_FAULT;
        case 1u: return ARINC_MATRIX_NO_DATA;
        case 2u: return ARINC_MATRIX_TEST;
        case 3u: return ARINC_MATRIX_NORMAL;
        default: return ARINC_MATRIX_FAULT;
        }
    }

    switch (raw) {
    case 0u: return ARINC_MATRIX_NORMAL;
    case 1u: return ARINC_MATRIX_NO_DATA;
    case 2u: return ARINC_MATRIX_TEST;
    case 3u: return ARINC_MATRIX_FAULT;
    default: return ARINC_MATRIX_FAULT;
    }
}

void ARINC_PackWord(uint8_t address,
                    uint32_t data21,
                    ArincMatrix matrix,
                    ArincWordType type,
                    uint8_t out[4])
{
    uint8_t raw_matrix = matrix_to_raw(matrix, type);

    data21 &= 0x1FFFFFu;

    out[0] = address;
    out[1] = (uint8_t)(data21 & 0xFFu);             /* bits 9..16 */
    out[2] = (uint8_t)((data21 >> 8) & 0xFFu);      /* bits 17..24 */
    out[3] = (uint8_t)(((data21 >> 16) & 0x1Fu) |  /* bits 25..29 */
                       ((uint8_t)raw_matrix << 5));

    /* bit 32 (out[3] bit 7) формируется HI-3220 при передаче. */
    out[3] &= 0x7Fu;
}

bool ARINC_UnpackWord(const uint8_t word[4],
                      uint8_t *address,
                      uint32_t *data21,
                      ArincMatrix *matrix,
                      ArincWordType type)
{
    if (word == NULL) {
        return false;
    }

    if (address != NULL) {
        *address = word[0];
    }

    if (data21 != NULL) {
        *data21 = ((uint32_t)word[1]) |
                  ((uint32_t)word[2] << 8) |
                  ((uint32_t)(word[3] & 0x1Fu) << 16);
    }

    if (matrix != NULL) {
        *matrix = raw_to_matrix((uint8_t)((word[3] >> 5) & 0x03u), type);
    }

    return true;
}

bool ARINC_ParseSu1(const uint8_t word[4], ArincSu1 *out)
{
    uint8_t address;
    uint32_t data;
    ArincMatrix matrix;

    if (out == NULL || !ARINC_UnpackWord(word, &address, &data, &matrix, ARINC_WORD_K)) {
        return false;
    }

    if (address != ARINC_ADDR_SU1) {
        return false;
    }

    /* bits 19..29 (data bits 10..20) -- резерв, должны быть 0. */
    if ((data & 0x1FFC00u) != 0u) {
        return false;
    }

    out->mode = (uint8_t)(data & 0x01u);
    out->backlight_auto = (uint8_t)((data >> 1) & 0x01u);
    out->brightness = (uint8_t)((data >> 2) & 0xFFu);
    out->matrix = matrix;
    return true;
}

bool ARINC_ParseSd1(const uint8_t word[4], ArincSd1 *out)
{
    uint8_t address;
    uint32_t data;
    ArincMatrix matrix;

    if (out == NULL || !ARINC_UnpackWord(word, &address, &data, &matrix, ARINC_WORD_K)) {
        return false;
    }

    if (address != ARINC_ADDR_SD1) {
        return false;
    }

    /* bits 25..29 (data bits 16..20) -- резерв, должны быть 0. */
    if ((data & 0x1F0000u) != 0u) {
        return false;
    }

    out->key_code = (uint8_t)(data & 0xFFu);
    out->key_id = (uint8_t)((data >> 8) & 0xFFu);
    out->matrix = matrix;
    return true;
}

void ARINC_BuildSs1(uint8_t ready,
                    uint8_t layout_rus,
                    uint8_t sw_version,
                    uint8_t ls1_ok,
                    uint8_t ls2_ok,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    uint32_t data = ((uint32_t)(ready & 0x01u)) |
                    ((uint32_t)(layout_rus & 0x01u) << 1) |
                    ((uint32_t)sw_version << 2) |
                    ((uint32_t)(ls1_ok & 0x01u) << 10) |
                    ((uint32_t)(ls2_ok & 0x01u) << 11);

    ARINC_PackWord(ARINC_ADDR_SS1, data, matrix, ARINC_WORD_K, out);
}

void ARINC_BuildSd2(uint8_t backlight_auto,
                    uint8_t backlight_level,
                    uint8_t illumination_level,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    uint32_t data = ((uint32_t)(backlight_auto & 0x01u)) |
                    ((uint32_t)backlight_level << 1) |
                    ((uint32_t)illumination_level << 9);

    ARINC_PackWord(ARINC_ADDR_SD2, data, matrix, ARINC_WORD_K, out);
}

void ARINC_BuildSd3(uint32_t uptime_minutes,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    /* bits 9..28 = 20-bit uptime, bit 29 (sign) = 0. */
    uint32_t data = uptime_minutes & 0x000FFFFFu;
    ARINC_PackWord(ARINC_ADDR_SD3, data, matrix, ARINC_WORD_DK, out);
}

void ARINC_BuildSd4(uint16_t crc16,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    /* bits 9..12 reserve = 0; bits 13..28 = CRC-16; bit 29 sign = 0. */
    uint32_t data = ((uint32_t)crc16 << 4);
    ARINC_PackWord(ARINC_ADDR_SD4, data, matrix, ARINC_WORD_DK, out);
}

void ARINC_BuildSd5(uint8_t key_code,
                    uint8_t key_id,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    uint32_t data = ((uint32_t)key_code) |
                    ((uint32_t)key_id << 8);

    ARINC_PackWord(ARINC_ADDR_SD5, data, matrix, ARINC_WORD_K, out);
}

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
                    uint8_t out[4])
{
    uint32_t data = ((uint32_t)(layout_rus & 0x01u)) |
                    ((uint32_t)sw_version << 1) |
                    ((uint32_t)(backlight_ok & 0x01u) << 9) |
                    ((uint32_t)(keypad_ok & 0x01u) << 10) |
                    ((uint32_t)(illumination_ok & 0x01u) << 11) |
                    ((uint32_t)(power_ok & 0x01u) << 12) |
                    ((uint32_t)(arinc_ok & 0x01u) << 13) |
                    ((uint32_t)(ls1_ok & 0x01u) << 14) |
                    ((uint32_t)(ls2_ok & 0x01u) << 15);

    ARINC_PackWord(ARINC_ADDR_SS2, data, matrix, ARINC_WORD_K, out);
}

void ARINC_BuildSd6(uint16_t voltage_3v3_centi_volt,
                    uint16_t voltage_5v_centi_volt,
                    ArincMatrix matrix,
                    uint8_t out[4])
{
    uint32_t data = ((uint32_t)(voltage_3v3_centi_volt & 0x03FFu)) |
                    ((uint32_t)(voltage_5v_centi_volt & 0x03FFu) << 10);

    ARINC_PackWord(ARINC_ADDR_SD6, data, matrix, ARINC_WORD_K, out);
}
