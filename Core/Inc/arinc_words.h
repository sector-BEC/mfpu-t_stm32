
#ifndef INC_ARINC_WORDS_H_
#define INC_ARINC_WORDS_H_

#include <stdint.h>
#include <stdbool.h>

/* =====================================================================
 * Пакет протокольных структур для обмена МФПУ-Т <-> МФИ-12Т / ПУИ-Т
 * согласно ПИВ. Все ссылки на "таблица N" далее -- это таблицы
 * из документа ПИВ.
 * =====================================================================
 *
 * Формат 32-разрядного слова (табл.1):
 *   биты 1-8    Адрес (Label)             -> byte0 (передаётся как есть,
 *                                             HI-3220 сам "переворачивает"
 *                                             метку, т.к. FLIP=1 в MCR)
 *   биты 9-11   Идентификатор получателя  -> младшие 3 бита byte1
 *   биты 12-29  Данные (18 бит)           -> см. упаковку ниже
 *   биты 30-31  Матрица состояния (2 бита)-> старшие 2 бита byte3 (5,6)
 *   бит  32     Чётность                  -> старший бит byte3 (7),
 *                                             ВСТАВЛЯЕТСЯ АППАРАТНО HI-3220
 *                                             при передаче (PARITY/DATA=1),
 *                                             программно не считается.
 *
 * Раскладка данных (18 бит, биты12-29) внутри байтов памяти HI-3220
 * (байты хранятся little-endian: byte0=адрес/статус, byte1=биты9-16,
 *  byte2=биты17-24, byte3=биты25-32):
 *
 *   byte1 = [ data_lsb[4:0] << 3 ] | recipient[2:0]
 *   byte2 = data[12:5]
 *   byte3 = [ parity(hw) <<7 ] | [ matrix[1:0] <<5 ] | data[17:13]
 *
 * где data18 - 18-битное значение, в котором bit0 соответствует биту12
 * слова, а bit17 -- биту29 слова.
 */

/* ---------- Адреса сообщений (Label, десятичный, табл.4-22) ---------- */
#define ADDR_MSG1   188U  /* код нажатой клавиши (МФПУ-Т -> МФИ) */
// #define ADDR_MSG2   189U  /* код отжатой клавиши (МФПУ-Т -> МФИ) */
#define ADDR_MSG2   190U  /* раскладка/подсветка/освещённость (broadcast) */
#define ADDR_MSG3   191U  /* статус МФПУ-Т, версия ПО, активная линия (broadcast, режим "работа") */
#define ADDR_MSG4   192U  /* наработка (broadcast) */
#define ADDR_MSG5   193U  /* контрольная сумма ПО (broadcast) */
#define ADDR_MSG6   194U  /* команда управления подсветкой (входящее) */
#define ADDR_MSG7   195U  /* команда выбора активной линии (входящее, от ПУИ-Т) */
#define ADDR_MSG8   196U  /* подтверждение приёма (в обе стороны) */
#define ADDR_MSG9   197U  /* статус МФПУ-Т (broadcast, режим "тест-контроль") */

/* ---------- Идентификаторы КИ БРЭО (табл.2) ---------- */
typedef enum {
    ID_MFI_LEFT  = 1,
    ID_MFI_RIGHT = 2,
    ID_MFPU      = 3,
    ID_PUI       = 4,
    ID_BROADCAST = 7
} DeviceId;

/* ---------- Матрица состояния (табл.3), значение = (b31<<1)|b30 ---------- */
typedef enum {
    MATRIX_FAULT   = 0x0, /* 00 - предупреждение об отказе (высший приоритет) */
    MATRIX_TEST    = 0x1, /* 01 - функциональный тест */
    MATRIX_NO_DATA = 0x2, /* 10 - нет вычисленных данных */
    MATRIX_NORMAL  = 0x3  /* 11 - нормальная работа (низший приоритет) */
} MatrixStatus;

/* ---------- Идентификаторы линии связи с МФИ (табл.12), в 2 битах ---------- */
typedef enum {
    LINE_ID_LEFT  = 0x1, /* 01 - ЛС1, левый МФИ-12Т */
    LINE_ID_RIGHT = 0x2  /* 10 - ЛС2, правый МФИ-12Т */
} LineId;

/* ---------- Статус передачи сообщения №8 (табл.18), в 2 битах ---------- */
typedef enum {
    XFER_OK    = 0x1, /* 01 - успешная передача */
    XFER_ERROR = 0x2  /* 10 - ошибка передачи */
} XferStatus;

/* =====================================================================
 * Низкоуровневая упаковка/распаковка
 * ===================================================================== */

/* Собрать 4 байта ARINC-слова (little-endian, как требует HI-3220).
 * data18 - значение бит12..29 (bit0=бит12 ... bit17=бит29).
 * Бит чётности (byte3 бит7) не заполняется -- аппаратно вставляется HI-3220
 * при передаче (см. TXCn.PARITY/DATA=1). При прямой записи в память
 * (не через TRANSFER_SendImmediate) чётность нужно посчитать самому. */
void ARINC_PackWord(uint8_t address, uint8_t recipient3, uint32_t data18,
                     MatrixStatus matrix, uint8_t out[4]);

/* Разобрать 4 байта в поля. Возвращает адрес (сверка не выполняется). */
void ARINC_UnpackWord(const uint8_t word[4], uint8_t *address,
                      uint8_t *recipient3, uint32_t *data18,
                      MatrixStatus *matrix);

/* Определить итоговую матрицу состояния с учётом приоритета
 * (табл.3: отказ > нет данных > тест > норма). */
MatrixStatus ARINC_ResolveMatrix(bool fault, bool no_computed_data, bool test_mode);

/* =====================================================================
 * Сборщики исходящих сообщений (МФПУ-Т -> КИ БРЭО)
 * ===================================================================== */

/* Сообщение №1: код клавиши. msg1 (нажатие),
 * recipient - ID_MFI_LEFT или ID_MFI_RIGHT. */
void ARINC_BuildKeyMsg(uint8_t recipient, uint8_t key_code,
                        MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №2: раскладка клавиатуры, режим/уровень подсветки, освещённость.
 * Всегда broadcast (id=7). */
void ARINC_BuildMsg2(uint8_t layout_rus, uint8_t backlight_auto,
                      uint8_t backlight_level, uint8_t illum_level,
                      MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №3: статус МФПУ-Т в режиме "работа". Broadcast. */
void ARINC_BuildMsg3(uint8_t ready, uint8_t healthy, uint8_t test_mode,
                      uint8_t sw_version, LineId active_line,
                      MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №4: наработка (18-бит счётчик, единица - 3 мин). Broadcast. */
void ARINC_BuildMsg4(uint32_t uptime_3min, MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №5: контрольная сумма ПО (CRC-16-CCITT). Broadcast. */
void ARINC_BuildMsg5(uint16_t crc16, MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №8: подтверждение приёма. recipient - кому шлём ответ,
 * sender - наш ID (обычно ID_MFPU). */
void ARINC_BuildMsg8(uint8_t recipient, uint8_t sender, XferStatus status,
                      MatrixStatus matrix, uint8_t out[4]);

/* Сообщение №9: статус МФПУ-Т в режиме "тест-контроль". Broadcast. */
void ARINC_BuildMsg9(uint8_t ready, uint8_t healthy, uint8_t test_mode,
                       uint8_t sw_version, LineId active_line,
                       uint8_t backlight_ok, uint8_t keypad_ok, uint8_t illum_ok,
                       MatrixStatus matrix, uint8_t out[4]);

/* =====================================================================
 * Разборщики входящих сообщений (КИ БРЭО -> МФПУ-Т)
 * ===================================================================== */

typedef struct {
    uint8_t recipient;       /* кому адресовано (табл.2) */
    uint8_t sender;          /* кто отправил (табл.2) */
    uint8_t auto_mode;       /* бит15 как есть: 0-ручной, 1-автоматический */
    uint8_t brightness;      /* 0..255, используется только если auto_mode==0 */
    uint8_t test_control;    /* 0-работа,1-тест-контроль */
    MatrixStatus matrix;
} Msg6_KeyboardBacklight;

typedef struct {
    uint8_t recipient;
    uint8_t sender;
    LineId  active_line;
    MatrixStatus matrix;
} Msg7_SelectLine;

typedef struct {
    uint8_t recipient;
    uint8_t sender;
    XferStatus status;
    MatrixStatus matrix;
} Msg8_Ack;

/* Возвращают false, если структура сообщения некорректна (для msg7/msg8:
 * зарезервированные поля не равны 0 -> формируется ошибка по протоколу). */
bool ARINC_ParseMsg6(const uint8_t word[4], Msg6_KeyboardBacklight *out);
bool ARINC_ParseMsg7(const uint8_t word[4], Msg7_SelectLine *out);
bool ARINC_ParseMsg8(const uint8_t word[4], Msg8_Ack *out);

#endif /* INC_ARINC_WORDS_H_ */
