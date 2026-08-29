
#ifndef INC_TRANSFER_H_
#define INC_TRANSFER_H_

#include "arinc_words.h"

void TransferSetActiveLine(LineId line);

LineId TransferGetActiveLine(void);

void TransferSetPending(uint8_t pending);

uint8_t TransferIsPending(void);

uint8_t TransferGetPendingChannel(void);

/* Событие клавиатуры. Ставит в очередь сообщение №1 на активную линию. */
void TransferKeyEvent(uint8_t key_code);

#endif /* INC_TRANSFER_H_ */
