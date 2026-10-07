#include "logic.h"
#include "DriverArinc.h"
#include "arinc_words.h"
#include "Backlight.h"
#include "KeypadCtrl.h"
#include "SRAM.h"
#include <string.h>

/*
 * Logic_Process() должен вызываться каждые 5 мс.
 * В режиме "работа" М2 формируется раз в 50 мс, в "тест-контроль" — раз в 5 мс.
 */
#define LOGIC_TICK_MS              5u
#define WORK_PERIOD_MS             50u
#define TEST_PERIOD_MS              5u
#define M1_PERIOD_MS               50u
#define LINE_MISS_LIMIT             3u
#define KEY_SEND_LIMIT              5u
#define UPTIME_UNIT_MS           60000u
#define UPTIME_MAX_MINUTES    1048575u

#define RX_CH_LS1                   0u
#define RX_CH_LS2                   1u
#define TX_CH_LS3                   0u
#define TX_CH_LS4                   1u
#define RX_QUEUE_SIZE              64u
#define TX_QUEUE_SIZE              32u
#define MAX_WORDS_PER_POLL          8u

typedef enum {
    OPMODE_WORK = 0,
    OPMODE_TEST_CONTROL = 1
} OpMode;

typedef struct {
    bool su1_valid;
    bool sd1_valid;
    ArincSu1 su1;
    ArincSd1 sd1;
} M1Assembly;

typedef struct {
    OpMode mode;
    uint8_t ready;
    uint8_t healthy;
    uint8_t sw_version;
    uint16_t sw_checksum;
} MfpuState;

typedef struct {
    uint8_t pending;
    uint8_t key_code;
    uint8_t key_id;
    uint8_t periods_sent;
} KeyTransfer;

static struct {
    uint8_t channel[RX_QUEUE_SIZE];
    uint8_t word[RX_QUEUE_SIZE][4];
    uint16_t head;
    uint16_t tail;
} rx_queue = {0};

static struct {
    uint8_t channel[TX_QUEUE_SIZE];
    uint8_t word[TX_QUEUE_SIZE][4];
    uint16_t head;
    uint16_t tail;
} tx_queue = {0};

static MfpuState mfpu;
static M1Assembly m1[2] = {0};
static uint8_t line_miss_count[2] = {0};
static uint8_t line_seen[2] = {0};
static uint8_t m1_assembly_age[2] = {0};
static KeyTransfer key_transfer = {0};

static uint32_t m1_period_elapsed_ms;
static uint32_t broadcast_elapsed_ms;
static uint32_t uptime_elapsed_ms;

/*
 * Пока заглушки, нужно, чтобы Иван реализовал функции эти
 */
#ifndef LOGIC_ILLUMINATION_OK
#define LOGIC_ILLUMINATION_OK()       (1u)
#endif
#ifndef LOGIC_POWER_OK
#define LOGIC_POWER_OK()              (1u)
#endif
#ifndef LOGIC_ARINC_OK
#define LOGIC_ARINC_OK()              (1u)
#endif
#ifndef LOGIC_3V3_CENTI_VOLT
#define LOGIC_3V3_CENTI_VOLT()        (330u)
#endif
#ifndef LOGIC_5V_CENTI_VOLT
#define LOGIC_5V_CENTI_VOLT()         (500u)
#endif

static bool queue_push(uint8_t *qch,
                       uint8_t qword[][4],
                       uint16_t *head,
                       uint16_t *tail,
                       uint16_t size,
                       uint8_t channel,
                       const uint8_t word[4])
{
    uint16_t next = (uint16_t)((*head + 1u) % size);
    if (next == *tail) {
        return false;
    }

    qch[*head] = channel;
    memcpy(qword[*head], word, 4u);
    *head = next;
    return true;
}

static bool queue_pop(uint8_t *qch,
                      uint8_t qword[][4],
                      uint16_t *head,
                      uint16_t *tail,
                      uint16_t size,
                      uint8_t *channel,
                      uint8_t word[4])
{
    if (*head == *tail) {
        return false;
    }

    *channel = qch[*tail];
    memcpy(word, qword[*tail], 4u);
    *tail = (uint16_t)((*tail + 1u) % size);
    return true;
}

static void rx_callback(uint8_t channel, const uint8_t *word)
{
    (void)queue_push(rx_queue.channel,
                     rx_queue.word,
                     &rx_queue.head,
                     &rx_queue.tail,
                     RX_QUEUE_SIZE,
                     channel,
                     word);
}

static void send_word(uint8_t channel, const uint8_t word[4])
{
    (void)queue_push(tx_queue.channel,
                     tx_queue.word,
                     &tx_queue.head,
                     &tx_queue.tail,
                     TX_QUEUE_SIZE,
                     channel,
                     word);
}

static ArincMatrix normal_or_fault_matrix(void)
{
    return mfpu.healthy ? ARINC_MATRIX_NORMAL : ARINC_MATRIX_FAULT;
}


static void update_m1_assembly_age(void)
{
    for (uint8_t i = 0u; i < 2u; ++i) {
        if (m1[i].su1_valid || m1[i].sd1_valid) {
            if (m1_assembly_age[i] < M1_PERIOD_MS) {
                m1_assembly_age[i] += LOGIC_TICK_MS;
            }
            if (m1_assembly_age[i] >= M1_PERIOD_MS) {
                m1[i].su1_valid = false;
                m1[i].sd1_valid = false;
                m1_assembly_age[i] = 0u;
            }
        } else {
            m1_assembly_age[i] = 0u;
        }
    }
}

static void update_line_period(void)
{
    if (m1_period_elapsed_ms < M1_PERIOD_MS) {
        return;
    }

    m1_period_elapsed_ms -= M1_PERIOD_MS;

    for (uint8_t i = 0u; i < 2u; ++i) {
        if (line_seen[i]) {
            line_seen[i] = 0u;
            line_miss_count[i] = 0u;
        } else if (line_miss_count[i] < LINE_MISS_LIMIT) {
            ++line_miss_count[i];
        }
    }
}

static bool ls1_ok(void)
{
    return line_miss_count[0] < LINE_MISS_LIMIT;
}

static bool ls2_ok(void)
{
    return line_miss_count[1] < LINE_MISS_LIMIT;
}

static void process_m1_array(uint8_t rx_channel)
{
    M1Assembly *a;
    uint8_t line_index;

    if (rx_channel == RX_CH_LS1) {
        a = &m1[0];
        line_index = 0u;
    } else if (rx_channel == RX_CH_LS2) {
        a = &m1[1];
        line_index = 1u;
    } else {
        return;
    }

    if (!a->su1_valid || !a->sd1_valid) {
        return;
    }

    /* Оба слова должны иметь нормальную матрицу. */
    if (a->su1.matrix != ARINC_MATRIX_NORMAL ||
        a->sd1.matrix != ARINC_MATRIX_NORMAL) {
        a->su1_valid = false;
        a->sd1_valid = false;
        return;
    }

    /* Достоверный М1 получен — линия считается исправной. */
    line_seen[line_index] = 1u;
    m1_assembly_age[line_index] = 0u;

    if (mfpu.mode == OPMODE_TEST_CONTROL) {
        /* В тест-контроле обрабатывается только разряд 9 СУ1. */
        mfpu.mode = a->su1.mode ? OPMODE_TEST_CONTROL : OPMODE_WORK;
    } else {
        /* В режиме "работа" обрабатываются режим, подсветка и её уровень. */
        mfpu.mode = a->su1.mode ? OPMODE_TEST_CONTROL : OPMODE_WORK;

        if (a->su1.backlight_auto) {
            BacklightSetMode(a->su1.backlight_auto);
        } else {
            BacklightSetMode(a->su1.backlight_auto);
            BacklightSetLightLevel(a->su1.brightness);
        }

        /* СД1 подтверждает событие клавиши только при полном совпадении. */
        if (key_transfer.pending &&
            a->sd1.key_code == key_transfer.key_code &&
            a->sd1.key_id == key_transfer.key_id) {
            key_transfer.pending = 0u;
            key_transfer.periods_sent = 0u;
        }
    }

    a->su1_valid = false;
    a->sd1_valid = false;
}

static void dispatch_incoming(uint8_t rx_channel, const uint8_t word[4])
{
    uint8_t address = word[0];

    if (rx_channel != RX_CH_LS1 && rx_channel != RX_CH_LS2) {
        return;
    }

    switch (address) {
    case ARINC_ADDR_SU1:
        if (ARINC_ParseSu1(word, &m1[rx_channel].su1)) {
            m1[rx_channel].su1_valid = true;
            process_m1_array(rx_channel);
        } else {
            m1[rx_channel].su1_valid = false;
            m1[rx_channel].sd1_valid = false;
        }
        break;

    case ARINC_ADDR_SD1:
        if (ARINC_ParseSd1(word, &m1[rx_channel].sd1)) {
            m1[rx_channel].sd1_valid = true;
            process_m1_array(rx_channel);
        } else {
            m1[rx_channel].su1_valid = false;
            m1[rx_channel].sd1_valid = false;
        }
        break;

    default:
        break;
    }
}

static void accumulate_uptime(void)
{
    uptime_elapsed_ms += LOGIC_TICK_MS;

    if (uptime_elapsed_ms >= UPTIME_UNIT_MS) {
        uptime_elapsed_ms -= UPTIME_UNIT_MS;

        uint32_t minutes = SRAMGetWorkTimeSRAM();
        if (minutes < UPTIME_MAX_MINUTES) {
            SRAMSetWorkTimeSRAM(minutes + 1u);
        }
    }
}

static void build_and_send_m2(void)
{
    uint8_t word[4];
    bool healthy = (mfpu.healthy != 0u);
    bool illum_ok = (LOGIC_ILLUMINATION_OK() != 0u);
    bool backlight_ok = (BacklightGetOperability() != 0u);
    ArincMatrix state_matrix = healthy ? ARINC_MATRIX_NORMAL : ARINC_MATRIX_FAULT;

    KeypadCtrlUpdate();
    BacklightUpdate();

    /* СС1 — К, при отказе изделия матрица "Отказ". */
    ARINC_BuildSs1(mfpu.ready,
                   KeypadCtrlGetLanguage(),
                   mfpu.sw_version,
                   ls1_ok(),
                   ls2_ok(),
                   state_matrix,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    /* СД2: при неисправности датчика/контроллера — "Нет вычисленных данных". */
    ArincMatrix sd2_matrix = (illum_ok && backlight_ok)
                           ? ARINC_MATRIX_NORMAL
                           : ARINC_MATRIX_NO_DATA;
    ARINC_BuildSd2(BacklightGetMode(),
                   BacklightGetLightLevel(),
                   BacklightGetBrightness(),
                   sd2_matrix,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    /* СД3 — ДК, наработка в минутах. */
    ARINC_BuildSd3(SRAMGetWorkTimeSRAM(),
                   ARINC_MATRIX_NORMAL,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    /* СД4 — ДК, CRC-16-CCITT, нормальная матрица. */
    ARINC_BuildSd4(mfpu.sw_checksum,
                   ARINC_MATRIX_NORMAL,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    /* СД5 — К. Без события: 0/0 + "Нет вычисленных данных". */
    if (key_transfer.pending) {
        ARINC_BuildSd5(key_transfer.key_code,
                       key_transfer.key_id,
                       ARINC_MATRIX_NORMAL,
                       word);

        ++key_transfer.periods_sent;
        if (key_transfer.periods_sent >= KEY_SEND_LIMIT) {
            key_transfer.pending = 0u;
            key_transfer.periods_sent = 0u;
        }
    } else {
        ARINC_BuildSd5(0u,
                       0u,
                       ARINC_MATRIX_NO_DATA,
                       word);
    }
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);
}

static void build_and_send_m3(void)
{
    uint8_t word[4];
    bool illum_ok = (LOGIC_ILLUMINATION_OK() != 0u);
    bool backlight_ok = (BacklightGetOperability() != 0u);
    bool keypad_ok = (KeypadCtrlGetOperability() != 0u);
    bool power_ok = (LOGIC_POWER_OK() != 0u);
    bool arinc_ok = (LOGIC_ARINC_OK() != 0u);
    bool healthy = backlight_ok && keypad_ok && illum_ok && power_ok && arinc_ok;
    ArincMatrix matrix = healthy ? ARINC_MATRIX_NORMAL : ARINC_MATRIX_FAULT;

    KeypadCtrlUpdate();
    BacklightUpdate();

    ARINC_BuildSs2(KeypadCtrlGetLanguage(),
                   mfpu.sw_version,
                   backlight_ok,
                   keypad_ok,
                   illum_ok,
                   power_ok,
                   arinc_ok,
                   ls1_ok(),
                   ls2_ok(),
                   matrix,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    /* При неисправности датчика/контроллера SD2 должен быть "нет данных". */
    ArincMatrix sd2_matrix = (illum_ok && backlight_ok)
                           ? ARINC_MATRIX_NORMAL
                           : ARINC_MATRIX_NO_DATA;
    ARINC_BuildSd2(BacklightGetMode(),
                   BacklightGetLightLevel(),
                   BacklightGetBrightness(),
                   sd2_matrix,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    ARINC_BuildSd3(SRAMGetWorkTimeSRAM(), ARINC_MATRIX_NORMAL, word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    ARINC_BuildSd4(mfpu.sw_checksum, ARINC_MATRIX_NORMAL, word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);

    ARINC_BuildSd6((uint16_t)LOGIC_3V3_CENTI_VOLT(),
                   (uint16_t)LOGIC_5V_CENTI_VOLT(),
                   ARINC_MATRIX_NORMAL,
                   word);
    send_word(TX_CH_LS3, word);
    send_word(TX_CH_LS4, word);
}

void Logic_Init(void)
{
    mfpu.mode = OPMODE_WORK;
    mfpu.ready = 0u;
    mfpu.healthy = 0u;
    mfpu.sw_version = SRAMGetSWVersion();
    mfpu.sw_checksum = SRAMGetSWCheckSum();

    m1_period_elapsed_ms = 0u;
    broadcast_elapsed_ms = 0u;
    uptime_elapsed_ms = 0u;
}

void Logic_KeyPressed(uint8_t key_code)
{
    /* ISS2: в режиме тест-контроль нажатия клавиш не обрабатываются. */
    if (mfpu.mode == OPMODE_TEST_CONTROL || !mfpu.ready) {
        return;
    }

    ++key_transfer.key_id; /* uint8_t: 255 -> 0 */
    key_transfer.key_code = key_code;
    key_transfer.pending = 1u;
    key_transfer.periods_sent = 0u;
}

void Logic_Process(void)
{
    DriverArinc_PollRxFifos(rx_callback, MAX_WORDS_PER_POLL);

    uint8_t channel;
    uint8_t word[4];
    while (queue_pop(rx_queue.channel,
                     rx_queue.word,
                     &rx_queue.head,
                     &rx_queue.tail,
                     RX_QUEUE_SIZE,
                     &channel,
                     word)) {
        dispatch_incoming(channel, word);
    }

    m1_period_elapsed_ms += LOGIC_TICK_MS;
    broadcast_elapsed_ms += LOGIC_TICK_MS;

    update_m1_assembly_age();
    update_line_period();
    accumulate_uptime();

    uint32_t period = (mfpu.mode == OPMODE_TEST_CONTROL)
                    ? TEST_PERIOD_MS
                    : WORK_PERIOD_MS;

    if (broadcast_elapsed_ms >= period) {
        broadcast_elapsed_ms -= period;

        if (mfpu.mode == OPMODE_TEST_CONTROL) {
            build_and_send_m3();
        } else {
            build_and_send_m2();
        }
    }

    while (queue_pop(tx_queue.channel,
                     tx_queue.word,
                     &tx_queue.head,
                     &tx_queue.tail,
                     TX_QUEUE_SIZE,
                     &channel,
                     word)) {
        DriverArinc_SendImmediate(channel, word);
    }
}
