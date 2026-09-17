/***************************************************************************
 **
 **
 **    Master include file
 **
 **    Used with ARM IAR C/C++ Compiler
 **
 **    (c) Copyright IAR Systems 2007
 **
 **    $Revision: 1.0 $
 **
 ***************************************************************************/

#ifndef __INCLUDES_H
#define __INCLUDES_H

#include    <stdbool.h>
#include    <inarm.h>
#include    <iolpc2364.h>
#include    "arm_comm.h"
#include    "board.h"
#include    "uart.h"


#define UART0DIR (1UL <<  0) //DIR0 P1.00/ №хх ножка порта
#define UART1DIR (1UL << 17) //DIR1 P0.17/ №хх ножка порта
#define UART2DIR (1UL << 26) //DIR2 P3.26/ №хх ножка порта
#define UART3DIR (1UL <<  4) //DIR3 P0.04/ №хх ножка порта


#define WRK_LED       (1UL << 23) //HL4 P0.23
#define ETHERNET_LED  (1UL << 24) //HL3 P0.24
#define RS485_JP2_LED (1UL << 25) //HL2 P0.25
#define RS485_JP1_LED (1UL << 26) //HL1 P0.26


#define EINT0 (1UL << 10) //LED2 P2.10/ №74 ножка порта

#endif  // __INCLUDES_H
