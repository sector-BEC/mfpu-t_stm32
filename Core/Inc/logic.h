/*
 * logic.h
 *
 *  Created on: Jun 8, 2026
 *      Author: vbuser
 */

#ifndef INC_LOGIC_H_
#define INC_LOGIC_H_

// Инициализация модуля логики (вызывает настройку HI-3220)
void Logic_Init(void);

// Периодический вызов в суперцикле (опрос FIFO, отправка)
void Logic_Process(void);

// Отправить слово (поместить в исходящую очередь)
bool Logic_SendMessage(uint8_t channel, const uint8_t *word);

// Получить следующее принятое слово из входящей очереди
// Возвращает true, если слово получено, и заполняет channel и word
bool Logic_GetRecievedMessage(uint8_t *channel, uint8_t *word);

#endif /* INC_LOGIC_H_ */
