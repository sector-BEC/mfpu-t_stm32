#ifndef INC_LOGIC_H_
#define INC_LOGIC_H_

#include <stdint.h>
#include <stdbool.h>

void Logic_Init(void);
void Logic_Process(void);

/* Вызывается модулем клавиатуры при достоверном нажатии клавиши. */
void Logic_KeyPressed(uint8_t key_code);

#endif
