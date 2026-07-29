
#include "arinc_words.h"

/* ---------------------------------------------------------------------
 * Низкий уровень
 * --------------------------------------------------------------------- */

void ARINC_PackWord(uint8_t address, uint8_t recipient3, uint32_t data18,
                     MatrixStatus matrix, uint8_t out[4])
{
    out[0] = address;
    out[1] = (uint8_t)((recipient3 & 0x07) | ((data18 & 0x1F) << 3));
    out[2] = (uint8_t)((data18 >> 5) & 0xFF);
    /* бит7 (чётность) оставляем 0 -- вставляется аппаратно HI-3220 при
     * передаче через TRANSFER_SendImmediate/scheduler, если в TXCn
     * настроено PARITY/DATA=1. Если слово пишется напрямую в память
     * (TRANSFER_WriteWord) чётность нужно досчитать отдельно. */
    out[3] = (uint8_t)(((data18 >> 13) & 0x1F) | ((matrix & 0x03) << 5));
}

void ARINC_UnpackWord(const uint8_t word[4], uint8_t *address,
                      uint8_t *recipient3, uint32_t *data18,
                      MatrixStatus *matrix)
{
    if (address)    *address = word[0];
    if (recipient3) *recipient3 = word[1] & 0x07;
    if (data18) {
        uint32_t lo  = (word[1] >> 3) & 0x1F;        /* биты12-16 */
        uint32_t mid = word[2];                       /* биты17-24 */
        uint32_t hi  = word[3] & 0x1F;                /* биты25-29 */
        *data18 = lo | (mid << 5) | (hi << 13);
    }
    if (matrix) *matrix = (MatrixStatus)((word[3] >> 5) & 0x03);
}

MatrixStatus ARINC_ResolveMatrix(bool fault, bool no_computed_data, bool test_mode)
{
    /* Приоритет (высший -> низший): отказ, нет данных, тест, норма (см. п.1.1) */
    if (fault)            return MATRIX_FAULT;
    if (no_computed_data) return MATRIX_NO_DATA;
    if (test_mode)        return MATRIX_TEST;
    return MATRIX_NORMAL;
}

/* ---------------------------------------------------------------------
 * Сборщики исходящих сообщений
 * --------------------------------------------------------------------- */

void ARINC_BuildKeyMsg(bool is_release, uint8_t recipient, uint8_t key_code,
                        MatrixStatus matrix, uint8_t out[4])
{
    /* табл.8 / табл.9: биты12-19 - код клавиши (8 бит), биты20-29 - резерв(0) */
    uint32_t data18 = (uint32_t)key_code; /* младшие 8 бит data18 = биты12-19 */
    uint8_t addr = is_release ? ADDR_MSG2 : ADDR_MSG1;
    ARINC_PackWord(addr, recipient, data18, matrix, out);
}

void ARINC_BuildMsg3(uint8_t layout_rus, uint8_t backlight_auto,
                      uint8_t backlight_level, uint8_t illum_level,
                      MatrixStatus matrix, uint8_t out[4])
{
    /* табл.11: бит12-раскладка(1), бит13-режим подсветки(1),
     * биты14-21-яркость(8), биты22-29-освещённость(8) */
    uint32_t data18 = (uint32_t)(layout_rus & 0x1)
                     | ((uint32_t)(backlight_auto & 0x1) << 1)
                     | ((uint32_t)backlight_level << 2)
                     | ((uint32_t)illum_level << 10);
    ARINC_PackWord(ADDR_MSG3, ID_BROADCAST, data18, matrix, out);
}

void ARINC_BuildMsg4(uint8_t ready, uint8_t healthy, uint8_t test_mode,
                      uint8_t sw_version, LineId active_line,
                      MatrixStatus matrix, uint8_t out[4])
{
    /* табл.12: 12-готов,13-исправен,14-режим,15-21-версия(7б),
     * 22-23-акт.линия(2б),24-29-резерв(0) */
    uint32_t data18 = (uint32_t)(ready & 0x1)
                     | ((uint32_t)(healthy & 0x1) << 1)
                     | ((uint32_t)(test_mode & 0x1) << 2)
                     | ((uint32_t)(sw_version & 0x7F) << 3)
                     | ((uint32_t)(active_line & 0x3) << 10);
    ARINC_PackWord(ADDR_MSG4, ID_BROADCAST, data18, matrix, out);
}

void ARINC_BuildMsg5(uint32_t uptime_3min, MatrixStatus matrix, uint8_t out[4])
{
    /* табл.14: 12-29 - наработка, 18 бит целиком */
    ARINC_PackWord(ADDR_MSG5, ID_BROADCAST, uptime_3min & 0x3FFFF, matrix, out);
}

void ARINC_BuildMsg6(uint16_t crc16, MatrixStatus matrix, uint8_t out[4])
{
    /* табл.15: 12-27 - CRC (16 бит), 28-29 - резерв(0) */
    ARINC_PackWord(ADDR_MSG6, ID_BROADCAST, (uint32_t)crc16, matrix, out);
}

void ARINC_BuildMsg9(uint8_t recipient, uint8_t sender, XferStatus status,
                      MatrixStatus matrix, uint8_t out[4])
{
    /* табл.18: 12-14-отправитель(3б),15-16-статус(2б),17-29-резерв(0) */
    uint32_t data18 = (uint32_t)(sender & 0x07)
                     | ((uint32_t)(status & 0x03) << 3);
    ARINC_PackWord(ADDR_MSG9, recipient, data18, matrix, out);
}

void ARINC_BuildMsg10(uint8_t ready, uint8_t healthy, uint8_t test_mode,
                       uint8_t sw_version, LineId active_line,
                       uint8_t backlight_ok, uint8_t keypad_ok, uint8_t illum_ok,
                       MatrixStatus matrix, uint8_t out[4])
{
    /* табл.22 */
    uint32_t data18 = (uint32_t)(ready & 0x1)
                     | ((uint32_t)(healthy & 0x1) << 1)
                     | ((uint32_t)(test_mode & 0x1) << 2)
                     | ((uint32_t)(sw_version & 0x7F) << 3)
                     | ((uint32_t)(active_line & 0x3) << 10)
                     | ((uint32_t)(backlight_ok & 0x1) << 12)
                     | ((uint32_t)(keypad_ok & 0x1) << 13)
                     | ((uint32_t)(illum_ok & 0x1) << 14);
    ARINC_PackWord(ADDR_MSG10, ID_BROADCAST, data18, matrix, out);
}

/* ---------------------------------------------------------------------
 * Разборщики входящих сообщений
 * --------------------------------------------------------------------- */

bool ARINC_ParseMsg7(const uint8_t word[4], Msg7_KeyboardBacklight *out)
{
    uint8_t recipient; uint32_t data18; MatrixStatus matrix;
    ARINC_UnpackWord(word, NULL, &recipient, &data18, &matrix);

    out->recipient    = recipient;
    out->sender       = (uint8_t)(data18 & 0x07);
    out->auto_mode    = (uint8_t)((data18 >> 3) & 0x1); /* бит15: 0-ручной,1-авто (табл.16) */
    out->brightness   = (uint8_t)((data18 >> 4) & 0xFF);
    out->test_control = (uint8_t)((data18 >> 12) & 0x1);
    out->matrix       = matrix;

    uint32_t reserved = (data18 >> 13) & 0x1F; /* биты25-29 */
    return (reserved == 0);
}

bool ARINC_ParseMsg8(const uint8_t word[4], Msg8_SelectLine *out)
{
    uint8_t recipient; uint32_t data18; MatrixStatus matrix;
    ARINC_UnpackWord(word, NULL, &recipient, &data18, &matrix);

    out->recipient    = recipient;
    out->sender       = (uint8_t)(data18 & 0x07);
    out->active_line  = (LineId)((data18 >> 3) & 0x03);
    out->matrix       = matrix;

    uint32_t reserved = (data18 >> 5) & 0x1FFF; /* биты17-29 */
    bool line_valid = (out->active_line == LINE_ID_LEFT) || (out->active_line == LINE_ID_RIGHT);
    return (reserved == 0) && line_valid;
}

bool ARINC_ParseMsg9(const uint8_t word[4], Msg9_Ack *out)
{
    uint8_t recipient; uint32_t data18; MatrixStatus matrix;
    ARINC_UnpackWord(word, NULL, &recipient, &data18, &matrix);

    out->recipient = recipient;
    out->sender    = (uint8_t)(data18 & 0x07);
    out->status    = (XferStatus)((data18 >> 3) & 0x03);
    out->matrix    = matrix;

    uint32_t reserved = (data18 >> 5) & 0x1FFF; /* биты17-29 */
    return (reserved == 0);
}
