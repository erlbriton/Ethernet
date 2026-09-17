#ifndef __UART1RS485_H
#define __UART1RS485_H
#include "includes.h"

extern void uart1rs485_init (void);
void uart1rs485_Reinit (u8 BPS, u8 DevAddr);
#endif
