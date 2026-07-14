/*
 * logic.c
 *
 *  Created on: Jun 8, 2026
 *      Author: vbuser
 */


#include "logic.h"
#include "transfer_holt.h"


// Размер очередей (количество сообщений)
#define RX_QUEUE_SIZE  64
#define TX_QUEUE_SIZE  32

// Количество слов читаемых из очереди
#define MAX_WORDS 4

// Кольцевой буфер для входящих сообщений
static struct {
    uint8_t channel[RX_QUEUE_SIZE];
    uint8_t word[RX_QUEUE_SIZE][4];
    uint16_t head;
    uint16_t tail;
} rx_queue;

// Исходящий буфер
static struct {
    uint8_t channel[TX_QUEUE_SIZE];
    uint8_t word[TX_QUEUE_SIZE][4];
    uint16_t head;
    uint16_t tail;
} tx_queue;


// Вспомогательные функции для работы с очередями
static bool queue_push(uint8_t *queue_ch, uint8_t queue_word[][4],
					   uint16_t *head, uint16_t *tail,
					   uint16_t size, uint8_t ch, const uint8_t *word) {
    uint16_t next = (*head + 1) % size;
    if (next == *tail) return false; // переполнение
    queue_ch[*head] = ch;
    memcpy(queue_word[*head], word, 4);
    *head = next;
    return true;
}

static bool queue_pop(uint8_t *queue_ch, uint8_t queue_word[][4],
					  uint16_t *head, uint16_t *tail,
					  uint16_t size, uint8_t *ch, uint8_t *word) {
    if (*head == *tail) return false; // пусто
    *ch = queue_ch[*tail];
    memcpy(word, queue_word[*tail], 4);
    *tail = (*tail + 1) % size;
    return true;
}

// Callback для приёма слов из FIFO
static void rx_callback(uint8_t channel, const uint8_t *word) {
    // Можно добавить парсинг, фильтрацию и т.п.
    // Сейчас просто кладём в очередь
    queue_push(rx_queue.channel, rx_queue.word, &rx_queue.head, &rx_queue.tail,
               RX_QUEUE_SIZE, channel, word);
}

void Logic_Init(void)
{
	// Обнулить очереди
	rx_queue.head = rx_quque.tail = 0;
	tx_queue.head = tx_quque.tail = 0;
}

void Logic_Process(void)
{
	// 1. Опрос приёмных FIFO с колбэком
	TRANSFER_PollRxFifos(rx_callback, MAX_WORDS);

	//  2. Отправка из исходящей очереди (если есть)
	uint8_t ch, word[4];
	while(queue_pop(tx_queue.channel, tx_queue.word, &tx_queue.head, &tx_queue.tail,
			TX_QUEUE_SIZE, &ch, word))
	{
		TRANSFER_SendImmediate(ch, word);
	}
}

bool Logic_SendMessage(uint8_t channel, const uint8_t *word)
{
	return queue_push(tx_queue.channel, tx_queue.word, &tx_queue.head, &tx_queue.tail,
			TX_QUEUE_SIZE, channel, word);
}

bool Logic_GetRecievedMessage(uint8_t *channel, uint8_t *word)
{
	return queue_pop(rx_queue.channel, rx_queue.word, &rx_queue.head, &rx_queue.tail,
			RX_QUEUE_SIZE, channel, word);
}
