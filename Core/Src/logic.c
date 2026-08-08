#include "logic.h"
#include "transfer_holt.h"
#include "arinc_words.h"
#include <string.h>

/* =====================================================================
 * Маппинг линий связи ЛС1-6 (см. п.1 ПИВ) на каналы HI-3220.
 * ===================================================================== */
#define RX_CH_MFI_LEFT   0u   /* ЛС1: приём от левого МФИ-12Т */
#define RX_CH_MFI_RIGHT  1u   /* ЛС2: приём от правого МФИ-12Т */
#define RX_CH_PUI        2u   /* ЛС3: приём от ПУИ-Т */
#define TX_CH_MFI_LEFT   0u   /* ЛС4: передача в левый МФИ-12Т */
#define TX_CH_MFI_RIGHT  1u   /* ЛС5: передача в правый МФИ-12Т */
#define TX_CH_PUI        2u   /* ЛС6: передача в ПУИ-Т */

#define RX_QUEUE_SIZE       64u
#define TX_QUEUE_SIZE       16u
#define MAX_WORDS_PER_POLL  4u

#define SELFTEST_MS         15000u  /* самопроверка не более 15с (п.1) */
#define WORK_BROADCAST_MS   200u    /* 5 Гц, режим "работа" */
#define TEST_BROADCAST_MS   100u    /* 10 Гц, режим "тест-контроль" */
#define RETRY_INTERVAL_MS   40u     /* повтор msg1 каждые 40±5мс */
#define RETRY_MAX_COUNT     5u      /* не более 5 повторов (п.1.2.4) */
#define UPTIME_UNIT_MS      180000u /* 1 единица наработки = 3 мин (табл.13) */
#define PROC_PERIOD_MS      10u     /* Logic_Process вызывается каждые 10мс */

typedef enum { OPMODE_WORK = 0, OPMODE_TEST_CONTROL = 1 } OpMode;

/* ---------------------------------------------------------------------
 * Очереди сырых 4-байтовых ARINC-слов (аналог исходного logic.c,
 * с исправлением опечатки rx_quque/tx_quque и добавлением memcpy/memset).
 * --------------------------------------------------------------------- */
static struct {
    uint8_t  channel[RX_QUEUE_SIZE];
    uint8_t  word[RX_QUEUE_SIZE][4];
    uint16_t head, tail;
} rx_queue;

static struct {
    uint8_t  channel[TX_QUEUE_SIZE];
    uint8_t  word[TX_QUEUE_SIZE][4];
    uint16_t head, tail;
} tx_queue;

static bool queue_push(uint8_t *qch, uint8_t qw[][4], uint16_t *head, uint16_t *tail,
                        uint16_t size, uint8_t ch, const uint8_t *word)
{
    uint16_t next = (uint16_t)((*head + 1u) % size);
    if (next == *tail) return false; /* переполнение */
    qch[*head] = ch;
    memcpy(qw[*head], word, 4);
    *head = next;
    return true;
}

static bool queue_pop(uint8_t *qch, uint8_t qw[][4], uint16_t *head, uint16_t *tail,
                       uint16_t size, uint8_t *ch, uint8_t *word)
{
    if (*head == *tail) return false; /* пусто */
    *ch = qch[*tail];
    memcpy(word, qw[*tail], 4);
    *tail = (uint16_t)((*tail + 1u) % size);
    return true;
}

static void rx_callback(uint8_t channel, const uint8_t *word)
{
    queue_push(rx_queue.channel, rx_queue.word, &rx_queue.head, &rx_queue.tail,
               RX_QUEUE_SIZE, channel, word);
}

/* ---------------------------------------------------------------------
 * Состояние изделия МФПУ-Т
 * --------------------------------------------------------------------- */
typedef struct {
    OpMode   mode;
    uint8_t  ready;          /* признак готовности (табл.11/21) */
    uint8_t  healthy;        /* признак исправности изделия */
    uint8_t  sw_version;     /* номер версии ПО, 0..127 */
    uint16_t sw_checksum;    /* CRC-16-CCITT прошивки, см. TODO в Logic_Init */

    LineId   active_line;    /* выбранная линия для msg1 (табл.12) */

    /* локальные данные для сообщения №2 */
    uint8_t  layout_rus;
    uint8_t  backlight_auto;
    uint8_t  backlight_level;
    uint8_t  illum_level;

    /* состояние компонентов для сообщения №9 (тест-контроль, табл.21) */
    uint8_t  backlight_ctrl_ok;
    uint8_t  keypad_ctrl_ok;
    uint8_t  illum_sensor_ok;

    uint32_t uptime_3min;    /* наработка, ед. = 3 мин, 18 бит (табл.13) */
} MfpuState;

static MfpuState mfpuState;

/* ---------------------------------------------------------------------
 * Таймеры (обновляются: 1мс-поле -- в Logic_Tick1ms, 10мс-поля -- в
 * Logic_Process, т.к. именно там детерминированно тикает обмен по сети).
 * --------------------------------------------------------------------- */
static volatile uint32_t selftest_timer_ms  = 0;
static uint32_t broadcast_timer_ms = 0;
static uint32_t uptime_accum_ms    = 0;

/* ---------------------------------------------------------------------
 * Ожидание подтверждения (msg8) для отправленного msg1
 * --------------------------------------------------------------------- */
typedef struct {
    bool     pending;
    uint8_t  tx_channel;
    uint8_t  retries_left;
    uint32_t retry_timer_ms;
    uint8_t  word[4];
} PendingAck;

static PendingAck pending_ack;

/* ---------------------------------------------------------------------
 * Вспомогательные функции маппинга
 * --------------------------------------------------------------------- */
static int device_to_tx_channel(uint8_t device_id)
{
    switch (device_id) {
        case ID_MFI_LEFT:  return (int)TX_CH_MFI_LEFT;
        case ID_MFI_RIGHT: return (int)TX_CH_MFI_RIGHT;
        case ID_PUI:       return (int)TX_CH_PUI;
        default:           return -1; /* неизвестный/некорректный отправитель */
    }
}

static uint8_t active_line_to_tx_channel(void)
{
    return (mfpuState.active_line == LINE_ID_RIGHT) ? TX_CH_MFI_RIGHT : TX_CH_MFI_LEFT;
}

static uint8_t active_line_to_recipient(void)
{
    return (mfpuState.active_line == LINE_ID_RIGHT) ? (uint8_t)ID_MFI_RIGHT : (uint8_t)ID_MFI_LEFT;
}

/* Итоговая матрица состояния для всех исходящих слов, с учётом приоритета */
static MatrixStatus current_matrix(void)
{
    bool fault    = (mfpuState.healthy == 0);
    bool no_data  = (mfpuState.ready == 0);
    bool testmode = (mfpuState.mode == OPMODE_TEST_CONTROL);
    return ARINC_ResolveMatrix(fault, no_data, testmode);
}

/* Постановка слова в исходящую очередь (реальная отправка по SPI -- в
 * Logic_Process, чтобы всё общение с HI-3220 было в одном детерминированном
 * по времени слоте, как того требует принятая архитектура main.c). */
static void send_word(uint8_t tx_channel, const uint8_t word[4])
{
    queue_push(tx_queue.channel, tx_queue.word, &tx_queue.head, &tx_queue.tail,
               TX_QUEUE_SIZE, tx_channel, word);
}

/* =====================================================================
 * Обработка входящих сообщений
 * ===================================================================== */

static void handle_msg6(const uint8_t word[4])
{
    Msg6_KeyboardBacklight message6;
    bool ok = ARINC_ParseMsg6(word, &message6);

    /* Смена режима "работа"<->"тест-контроль" обрабатывается всегда,
     * даже находясь в тест-контроле (п.1.3.2: "выполняет обработку только
     * команды на изменение режима работы"). */
    if (ok) {
        mfpuState.mode = message6.test_control ? OPMODE_TEST_CONTROL : OPMODE_WORK;
    }

    /* Команда яркости подсветки -- только в режиме "работа" */
    if (ok && mfpuState.mode == OPMODE_WORK) {
        mfpuState.backlight_auto = message6.auto_mode;
        if (!mfpuState.backlight_auto) {
            mfpuState.backlight_level = message6.brightness;
        }
        /* при backlight_auto==1 уровень подсветки вычисляется отдельным
         * алгоритмом по датчику освещённости -- не описан в ПИВ (TODO)
         * алгоритм Ивана45 вызвать тут*/
    }

    int tx_ch = device_to_tx_channel(message6.sender);
    if (tx_ch >= 0) {
        uint8_t ack[4];
        ARINC_BuildMsg8(message6.sender, (uint8_t)ID_MFPU,
                         ok ? XFER_OK : XFER_ERROR, current_matrix(), ack);
        send_word((uint8_t)tx_ch, ack);
    }
}

static void handle_msg7(const uint8_t word[4])
{
    Msg7_SelectLine message7;
    bool ok = ARINC_ParseMsg7(word, &message7);

    /* "Обработка сообщений №8 не выполняется в режиме тест-контроль" (п.1.3.2) */
    if (ok && mfpuState.mode == OPMODE_WORK) {
        mfpuState.active_line = message7.active_line;
    }

    int tx_ch = device_to_tx_channel(message7.sender);
    if (tx_ch >= 0) {
        uint8_t ack[4];
        ARINC_BuildMsg7(message7.sender, (uint8_t)ID_MFPU,
                         ok ? XFER_OK : XFER_ERROR, current_matrix(), ack);
        send_word((uint8_t)tx_ch, ack);
    }
}

static void handle_msg8(const uint8_t word[4])
{
    Msg8_Ack message8;
    if (!ARINC_ParseMsg8(word, &message8)) return; /* некорректный формат -- игнор */
    if (!pending_ack.pending) return;       /* мы ничего не ждём -- игнор */

    int from_ch = device_to_tx_channel(message8.sender);
    if (from_ch != (int)pending_ack.tx_channel) return; /* ack не от того МФИ */

    /* И успех, и ошибка формата снимают наше ожидание: повторная отправка
     * того же слова не исправит ошибку формата на приёмной стороне.
     * Если требуется другая трактовка XFER_ERROR -- уточнить. */
    pending_ack.pending = false;
}

static void dispatch_incoming(uint8_t rx_channel, const uint8_t word[4])
{
    (void)rx_channel;
    switch (word[0] /* адрес/метка */) {
        case ADDR_MSG6: handle_msg6(word); break;
        case ADDR_MSG7: handle_msg7(word); break;
        case ADDR_MSG8: handle_msg8(word); break;
        default:
            /* Прочие метки для МФПУ-Т не определены как входящие -- игнор.
             * Проверка чётности выполняется аппаратно HI-3220 (RXCn.PARITYEN),
             * проверка идентификатора получателя (Broadcast/МФПУ-Т) здесь
             * не выполняется отдельно, т.к. в шаблонном случае каналы и так
             * содержат только адресованный МФПУ-Т трафик. */
            break;
    }
}

/* =====================================================================
 * Отправка событий клавиатуры (msg1) с повтором
 * ===================================================================== */

void Logic_KeyEvent(uint8_t key_code)
{
    uint8_t recipient = active_line_to_recipient();
    uint8_t tx_ch     = active_line_to_tx_channel();

    uint8_t word[4];
    ARINC_BuildKeyMsg(recipient, key_code, current_matrix(), word);

    send_word(tx_ch, word);

    /* Одновременно ожидаем подтверждение только для одного события --
     * ПИВ описывает события клавиатуры как "по готовности" (нечастые
     * относительно окна ретраев 40мс x 5). Если нужно поддержать очередь
     * из нескольких одновременно неподтверждённых событий -- расширить
     * PendingAck до массива.
     * Определённо нужен массив, но пока это лишь набросок*/
    pending_ack.pending        = true;
    pending_ack.tx_channel     = tx_ch;
    pending_ack.retries_left   = RETRY_MAX_COUNT - 1u; /* первая попытка уже отправлена */
    pending_ack.retry_timer_ms = RETRY_INTERVAL_MS;
    memcpy(pending_ack.word, word, 4);
}

static void process_retry(uint32_t elapsed_ms)
{
    if (!pending_ack.pending) return;

    if (pending_ack.retry_timer_ms > elapsed_ms) {
        pending_ack.retry_timer_ms -= elapsed_ms;
        return;
    }

    if (pending_ack.retries_left == 0u) {
        pending_ack.pending = false; /* попытки исчерпаны (п.1.2.4) */
        return;
    }

    send_word(pending_ack.tx_channel, pending_ack.word);
    pending_ack.retries_left--;
    pending_ack.retry_timer_ms = RETRY_INTERVAL_MS;
}

/* =====================================================================
 * Периодическая широковещательная рассылка
 * (msg2,4,5,3 -- режим "работа" 5Гц; msg2,4,5,9 -- "тест-контроль" 10Гц)
 * Сообщения массива рассылаются по одному за вызов Logic_Process, чтобы
 * разнести их внутри периода (конкретный тайминг РТМ 1495-75 в
 * ПИВ не приведён -- при необходимости скорректировать очередность/паузы).
 * ===================================================================== */
static void send_next_broadcast_word(void)
{
    uint8_t word[4];
    bool test_mode = (mfpuState.mode == OPMODE_TEST_CONTROL);

    /* msg2 */
    ARINC_BuildMsg2(mfpuState.layout_rus, mfpuState.backlight_auto, mfpuState.backlight_level,
                                 mfpuState.illum_level, current_matrix(), word);
    send_word(TX_CH_MFI_LEFT, word);
    send_word(TX_CH_MFI_RIGHT, word);

    /* msg4 */
    ARINC_BuildMsg4(mfpuState.uptime_3min, current_matrix(), word);
    send_word(TX_CH_MFI_LEFT, word);
    send_word(TX_CH_MFI_RIGHT, word);

    /* msg5 */
    ARINC_BuildMsg5(mfpuState.sw_checksum, current_matrix(), word);
    send_word(TX_CH_MFI_LEFT, word);
    send_word(TX_CH_MFI_RIGHT, word);

    /* msg3 или msg9 */
    if (test_mode)
    {
		ARINC_BuildMsg9(mfpuState.ready, mfpuState.healthy, 1u, mfpuState.sw_version, mfpuState.active_line,
						  mfpuState.backlight_ctrl_ok, mfpuState.keypad_ctrl_ok, mfpuState.illum_sensor_ok,
						  current_matrix(), word);
	} else {
		ARINC_BuildMsg3(mfpuState.ready, mfpuState.healthy, 0u, mfpuState.sw_version, mfpuState.active_line,
						 current_matrix(), word);
	}
    send_word(TX_CH_MFI_LEFT, word);
    send_word(TX_CH_MFI_RIGHT, word);

    /* Broadcast (id=7) адресован обоим МФИ-12Т сразу -- HI-3220 не
     * дублирует данные между Tx-каналами автоматически, поэтому одно и то
     * же слово ставится в очередь на оба канала (ЛС4 и ЛС5). */
}

/* =====================================================================
 * Публичный API
 * ===================================================================== */

void Logic_Init(void)
{
    memset(&rx_queue, 0, sizeof(rx_queue));
    memset(&tx_queue, 0, sizeof(tx_queue));
    memset(&pending_ack, 0, sizeof(pending_ack));
    memset(&mfpuState, 0, sizeof(mfpuState));

    mfpuState.mode        = OPMODE_WORK;
    mfpuState.ready       = 0;  /* поднимется по завершении самопроверки */
    mfpuState.healthy     = 0;
    mfpuState.sw_version  = 1;  /* TODO: подставить реальный номер версии ПО */
    mfpuState.sw_checksum = 0;  /* TODO: CRC-16-CCITT образа прошивки (константа
                          * сборки либо расчёт по флеш-región при старте) */
    mfpuState.active_line = LINE_ID_LEFT; /* "при подаче питания -- взаимодействие
                                     * с левым МФИ-12Т по ЛС1" (п.1) */

    mfpuState.backlight_ctrl_ok = 1;
    mfpuState.keypad_ctrl_ok    = 1;
    mfpuState.illum_sensor_ok   = 1;

    selftest_timer_ms  = 0;
    broadcast_timer_ms = 0;
    uptime_accum_ms    = 0;

    /* ВАЖНО: для точного соответствия протоколу нужно поправить
     * конфигурацию HI-3220 в Init_Holt() (transfer_holt.c):
     *  - HI3220_RxConfig.parity_en  = 1  (бит32 станет флагом чётности)
     *  - HI3220_TxConfig.parity_en  = 1, even_odd = 0  (нечётная чётность
     *    вставляется аппаратно -- этот модуль бит32 не считает)
     * Каналы 0/1/2 используются как ЛС1/ЛС2/ЛС3 (Rx) и ЛС4/ЛС5/ЛС6 (Tx). */
}

/*
void Logic_Tick1ms(void)
{
    /* Самопроверка при включении: длится не более 15с, пока не завершится
     * ready=0 (значит и признак готовности в сообщении №4/10 -- "0"). */
    if (!mfpuState.ready) {
        selftest_timer_ms++;
        if (selftest_timer_ms >= SELFTEST_MS) {
            /* TODO: заменить на реальную проверку CRC ПО (сравнение с
             * sw_checksum) и опрос исправности узлов (подсветка,
             * контроллер клавиатуры, датчик освещённости). */
            mfpuState.healthy = 1;
            mfpuState.ready   = 1;
        }
    }

    /* TODO: неблокирующее сканирование матрицы клавиатуры должно быть
     * здесь (короткий, детерминированный по времени слот, <=500мкс).
     * При обнаружении события вызывать Logic_KeyEvent(code).
     * HI-3220 передаёт только уже сформированные ARINC-слова. */
}
*/

void Logic_Process(void)
{
    /* 1. Опрос приёмных FIFO HI-3220, слова складываются в rx_queue */
    TRANSFER_PollRxFifos(rx_callback, MAX_WORDS_PER_POLL);

    /* 2. Диспетчеризация всех полученных слов */
    uint8_t ch, word[4];
    while (queue_pop(rx_queue.channel, rx_queue.word, &rx_queue.head, &rx_queue.tail,
                     RX_QUEUE_SIZE, &ch, word)) {
        dispatch_incoming(ch, word);
    }

    /* 3. Повтор неподтверждённых сообщений №1/№2 */
    process_retry(PROC_PERIOD_MS);

    /* 4. Периодическая рассылка: 5Гц ("работа") / 10Гц ("тест-контроль") */
    uint32_t period = (mfpuState.mode == OPMODE_TEST_CONTROL) ? TEST_BROADCAST_MS
                                                          : WORK_BROADCAST_MS;
    broadcast_timer_ms += PROC_PERIOD_MS;
    if (broadcast_timer_ms >= period) {
        broadcast_timer_ms -= period;
        send_next_broadcast_word();
    }

    /* 5. Учёт наработки (единица -- 3 минуты, табл.14) */
    uptime_accum_ms += PROC_PERIOD_MS;
    if (uptime_accum_ms >= UPTIME_UNIT_MS) {
        uptime_accum_ms -= UPTIME_UNIT_MS;
        if (mfpuState.uptime_3min < 0x3FFFFu) {
            mfpuState.uptime_3min++;
        }
    }

    /* 6. Реальная отправка накопленной исходящей очереди по SPI.
     *    Всё обращение к HI-3220 сосредоточено в этом детерминированном
     *    по времени слоте (вызывается каждые 10мс из main.c). */
    uint8_t tx_ch, tx_word[4];
    while (queue_pop(tx_queue.channel, tx_queue.word, &tx_queue.head, &tx_queue.tail,
                     TX_QUEUE_SIZE, &tx_ch, tx_word)) {
        TRANSFER_SendImmediate(tx_ch, tx_word);
    }
}