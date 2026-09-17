#ifndef __UART3RS485_H
#define __UART3RS485_H
#include "includes.h"
#include "mbtypes.h"

extern TClient uart3data;

extern void uart3rs485_init (void);
extern void uart3rs485_Reinit(u8 BPS, u8 DevAddr);
extern void U3_ResetModbusTimerForWaitReceive(void);
#define U3_Time_Out 20
extern u8 U3RstTOut;

#endif
