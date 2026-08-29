#include "Transfer.h"
#include "arinc_words.h"

#define RETRY_INTERVAL_MS   40u     /* повтор msg1 каждые 40±5мс */
#define RETRY_MAX_COUNT     5u      /* не более 5 повторов (п.1.2.4) */

#define TRANSFER_MFI_LEFT   0u   /* ЛС4: передача в левый МФИ-12Т */
#define TRANSFER_MFI_RIGHT  1u   /* ЛС5: передача в правый МФИ-12Т */

LineId   activeLine_ = LINE_ID_LEFT;    /* выбранная линия для msg1 (табл.12) */

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

static PendingAck pending_ack = {0};

/* ---------------------------------------------------------------------
 * Получить активную линию для отправки данных на выбранный МФИ
 * --------------------------------------------------------------------- */
static uint8_t active_line_to_tx_channel(void)
{
    return (activeLine_ == LINE_ID_RIGHT) ? TRANSFER_MFI_RIGHT : TRANSFER_MFI_LEFT;
}

static uint8_t active_line_to_recipient(void)
{
    return (activeLine_ == LINE_ID_RIGHT) ? (uint8_t)ID_MFI_RIGHT : (uint8_t)ID_MFI_LEFT;
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

    // send_word(pending_ack.tx_channel, pending_ack.word); // TODO: Как лучше поступить с send_word?
    pending_ack.retries_left--;
    pending_ack.retry_timer_ms = RETRY_INTERVAL_MS;
}

void TransferSetActiveLine(LineId line)
{
    activeLine_ = line;
}

LineId TransferGetActiveLine(void)
{
    return activeLine_;
}

void TransferSetPending(uint8_t pending)
{
    pending_ack.pending = pending;
}

uint8_t TransferIsPending(void)
{
    return pending_ack.pending;
}

uint8_t TransferGetPendingChannel(void)
{
    return pending_ack.tx_channel;
}

/* =====================================================================
 * Отправка событий клавиатуры (msg1) с повтором
 * ===================================================================== */

void TransferKeyEvent(uint8_t key_code)
{
    uint8_t recipient = active_line_to_recipient();
    uint8_t tx_ch     = active_line_to_tx_channel();

    uint8_t word[4];
    ARINC_BuildKeyMsg(recipient, key_code, current_matrix(), word);

    // send_word(tx_ch, word); // // TODO: Как лучше поступить с send_word?

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