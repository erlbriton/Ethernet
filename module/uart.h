/*************************************************************************
 *
 *    Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2006
 *
 *    File name   : uart.h
 *    Description : UARTs module header
 *
 *    History :
 *    1. Date        : July 6, 2006
 *       Author      : Stanimir Bonev
 *       Description : Create
 *
 *    $Revision: 1.3.2.2 $
 **************************************************************************/
#ifndef __UART_H
#define __UART_H
#include "includes.h"

#define CCLK 22118400 //частота кварца √ц
#define XTALFREQ CCLK * 1 //22118400
#define PeriphClock XTALFREQ //Hz

extern Int8U U3Data;//данные принимаемые/передаваемые по UART3

void uRXproc(Int8U uData );
Boolean uTXproc(pInt8U pData);

extern void (*u0RXproc)(Int8U uData);
extern Boolean (*u0TXproc)(pInt8U pData);
extern void (*u1RXproc)(Int8U uData);
extern Boolean (*u1TXproc)(pInt8U pData);
extern void (*u2RXproc)(Int8U uData);
extern Boolean (*u2TXproc)(pInt8U pData);
extern void (*u3RXproc)(Int8U uData);
extern Boolean (*u3TXproc)(pInt8U pData);

#define UIER_RECEIVE_ENABLE          (1 << 0) // Enable Receive Data Available Interrupt
#define UIER_TRANSMIT_ENABLE         (1 << 1) //Enable THRE interrupt (буфер передатчика пуст)

//#define _4800 0
#define _9600 3
#define _19200 4
#define _57600 5
#define _115200 6




#define _15bits_Delay_115200bps ( (PeriphClock / 115200) * 15 )
#define _15bits_Delay_57600bps  ( (PeriphClock / 57600) *  15 )
#define _15bits_Delay_19200bps  ( (PeriphClock / 19200) *  15 )
#define _15bits_Delay_9600bps   ( (PeriphClock / 9600) *   15 )
#define _15bits_Delay_4800bps   ( (PeriphClock / 4800) *   15 )//not valid on 22.1184 MHz
#define _15bits_Delay_2400bps   ( (PeriphClock / 2400) *   15 )//not valid on 22.1184 MHz
#define _15bits_Delay_1200bps   ( (PeriphClock / 1200) *   15 )//not valid on 22.1184 MHz
#define _CTS_Delay 		( 0.0001 * PeriphClock) //врем€ на переключение драйвера rs485

#ifdef UART_GLOBAL
#define UART_EXTERN
#else
#define UART_EXTERN  extern
#endif

#define UART1_MODEM_STAT_ENA  0

#define UART_FIFO_SIZE        100

#define UART_MAX_BAUD_RATE    256000

#define RLS_INTR_ID           0x6
#define RDA_INTR_ID           0x4
#define CDI_INTR_ID           0xC
#define THRE_INTR_ID          0x2
#define MODEM_INTR_ID         0x0

#define RLS_ReceiverDataReady (1UL<<0)
#define RLS_OverrunError      (1UL<<1)
#define RLS_ParityError       (1UL<<2)
#define RLS_FramingError      (1UL<<3)
#define RLS_BreakInterruptr   (1UL<<4)
#define RLS_TranHoldRegEmpty  (1UL<<5)
#define RLS_TranShiftRegEmpty (1UL<<6)
#define RLS_RxFifoError       (1UL<<7)


// Define UARTs
typedef enum _UartNum_t
{
  UART_0 = 0, UART_1, UART_2, UART_3,
} UartNum_t;

typedef enum _UartMode_t
{
  NORM = 0, IRDA
} UartMode_t;

typedef enum _UartParity_t
{
  UART_ODD_PARITY = 0, UART_EVEN_PARITY,
  UART_FORCE_1_PARITY, UART_FORCE_0_PARITY,
  UART_NO_PARITY
} UartParity_t;

typedef enum _UartStopBits_t
{
  UART_ONE_STOP_BIT = 0, UART_TWO_STOP_BIT,
} UartStopBits_t;

typedef enum _UartWordWidth_t
{
  UART_WORD_WIDTH_5 = 0, UART_WORD_WIDTH_6,
  UART_WORD_WIDTH_7, UART_WORD_WIDTH_8
} UartWordWidth_t;

typedef struct _UartFifo_t
{
  Int32U PushIndx;
  Int32U PopIndx;
  Int8U  Buffer[UART_FIFO_SIZE];
} UartFifo_t, *pUartFifo_t;

typedef union _UartModemLineState_t
{
  Int32U Data;
  struct {
    Int32U  bDTR          : 1;
    Int32U  bRTS          : 1;
    Int32U                :30;
  };
} UartModemLineState_t, *pUartModemLineState_t;

typedef struct _UartLineCoding_t
{
  Int32U          dwDTERate;
  UartStopBits_t  bStopBitsFormat;
  UartParity_t    bParityType;
  UartWordWidth_t bDataBits;
} UartLineCoding_t, * pUartLineCoding_t;

typedef union _UartLineEvents_t
{
  Int32U Data;
  struct {
    Int32U              : 1;
    Int32U  bOE         : 1;
    Int32U  bPE         : 1;
    Int32U  bFE         : 1;
    Int32U  bBI         : 1;
    Int32U              :27;
  };
} UartLineEvents_t, *pUartLineEvents_t;

typedef union _UartModemEvents_t
{
  Int32U Data;
  struct {
    Int32U  bDCTS       : 1;
    Int32U  bDDSR       : 1;
    Int32U  bDRI        : 1;
    Int32U  bDDCD       : 1;
    Int32U  bCTS        : 1;
    Int32U  bDSR        : 1;
    Int32U  bRI         : 1;
    Int32U  bDCD        : 1;
    Int32U              :24;
  };
} UartModemEvents_t, *pUartModemEvents_t;

extern const UartLineCoding_t UartLineCoding_115200;
extern const UartLineCoding_t UartLineCoding_57600;
extern const UartLineCoding_t  UartLineCoding_19200;
extern const UartLineCoding_t  UartLineCoding_9600;
extern const UartLineCoding_t  UartLineCoding_4800;
extern const UartLineCoding_t  UartLineCoding_2400;
extern const UartLineCoding_t  UartLineCoding_1200;

extern Int32U SYS_GetFpclk(Int32U Periphery);

/*************************************************************************
 * Function Name: Uart0Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 0 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart0Isr(void);

/*************************************************************************
 * Function Name: Uart1Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 1 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart1Isr(void);

/*************************************************************************
 * Function Name: Uart2Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 2 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart2Isr(void);

/*************************************************************************
 * Function Name: Uart3Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 3 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart3Isr(void);

/*************************************************************************
 * Function Name: UartInit
 * Parameters: UartNum_t Uart, Int32U IrqSlot, UartMode_t UartMode
 *
 * Return: Boolean
 *
 * Description: Init UART
 *
 *************************************************************************/
Boolean UartInit(UartNum_t Uart,Int32U IrqSlot, UartMode_t UartMode);

/*************************************************************************
 * Function Name: UartSetLineCoding
 * Parameters:  UartNum_t Uart,UartLineCoding_t pUartCoding
 *
 * Return: None
 *
 * Description: Init UART Baud rate, Word width, Stop bits, Parity type
 *
 *************************************************************************/
void UartSetLineCoding(UartNum_t Uart,UartLineCoding_t UartCoding);

/*************************************************************************
 * Function Name: UartGetUartEvents
 * Parameters:  UartNum_t Uart
 *
 * Return: UartLineEvents_t
 *
 * Description: Get Uart Line events (PE,OE, FE, BI)
 *
 *************************************************************************/
UartLineEvents_t UartGetUartLineEvents (UartNum_t Uart);

/*************************************************************************
 * Function Name: UartSetUartLineState
 * Parameters:  UartNum_t Uart UartNum_t Uart, Boolean Break
 *
 * Return: none
 *
 * Description: Set Uart Break Event
 *
 *************************************************************************/
void UartSetUartLineState (UartNum_t Uart, Boolean Break);

#if UART1_MODEM_STAT_ENA > 0
/*************************************************************************
 * Function Name: UartSetModemLineState
 * Parameters:  UartModemLineState_t UartModemLineState
 *
 * Return: none
 *
 * Description: Init UART1 lines state (RTS, DTR)
 *
 *************************************************************************/
void Uart1SetModemLineState(UartModemLineState_t UartModemLineState);

/*************************************************************************
 * Function Name: Uart1GetUartModemEvents
 * Parameters:  none
 *
 * Return: UartModemEvents_t
 *
 * Description: Get Uart Line events (DCD,DSR,CTS,RI)
 *
 *************************************************************************/
UartModemEvents_t Uart1GetUartModemEvents (void);



#endif // UART1_MODEM_STAT_ENA > 0

#endif // __UART_H
