/*************************************************************************
 *
 *    Used with ICCARM and AARM.
 *
 *    (c) Copyright IAR Systems 2006
 *
 *    File name   : uart.c
 *    Description : UARTs module
 *
 *    History :
 *    1. Date        : July 6, 2006
 *       Author      : Stanimir Bonev
 *       Description : Create
 *
 *    $Revision: 1.3.2.2 $
 **************************************************************************/
#define UART_GLOBAL
#include "uart.h"
const UartLineCoding_t UartLineCoding_115200   =
{
  115200,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t UartLineCoding_57600   =
{
  57600,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t  UartLineCoding_19200   =
{
  19200,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t  UartLineCoding_9600   =
{
  9600,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t  UartLineCoding_4800   =
{
  4800,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t  UartLineCoding_2400   =
{
  2400,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

const UartLineCoding_t  UartLineCoding_1200   =
{
  1200,UART_ONE_STOP_BIT,UART_NO_PARITY, UART_WORD_WIDTH_8
};

extern Int32U SYS_GetFpclk(Int32U Periphery);

Int8U U3Data;//данные принимаемые/передаваемые по UART3

void uRXproc(Int8U uData){}
Boolean uTXproc(pInt8U pData){ return 0;}

void (*u0RXproc)(Int8U uData) = 0;
Boolean (*u0TXproc)(pInt8U pData) = 0;
void (*u1RXproc)(Int8U uData) = 0;
Boolean (*u1TXproc)(pInt8U pData) = 0;
void (*u2RXproc)(Int8U uData) = 0;
Boolean (*u2TXproc)(pInt8U pData) = 0;
void (*u3RXproc)(Int8U uData) = 0;
Boolean (*u3TXproc)(pInt8U pData) = 0;

// Hold UART0 Evens (PE, BI, FE, OE)
UartLineEvents_t Uart0LineEvents;

// Hold UART1 Evens (PE, BI, FE, OE)
UartLineEvents_t Uart1LineEvents;

// Hold UART2 Evens (PE, BI, FE, OE)
UartLineEvents_t Uart2LineEvents;

// Hold UART3 Evens (PE, BI, FE, OE)
UartLineEvents_t Uart3LineEvents;

#if UART1_MODEM_STAT_ENA > 0
// Hold the Modem Lines States of UART1
// ( Delta CTS, Delta DSR, Delta RI, Delta DCD,
//  CTS, DSR, RI and DCD )
UartModemEvents_t Uart1ModemEvents;
#endif // UART1_MODEM_STAT_ENA > 0

/*************************************************************************
 * Function Name: Uart0Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 0 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart0Isr(void)
{
Int32U UartIntId = U0IIR;
Int8U Data;
  // Recognize the interrupt event
  switch (UartIntId & 0xF)
  {
  case RDA_INTR_ID: // Receive Data Available
    // Read the line state of the UART
      U0LSR;
      Data = U0RBR;
      // вызываю процедуру u0RXproc которая ложит байт
      // из приёмника в буфер пользователя
      if (u0RXproc !=0){
        u0RXproc(Data);
      }
      //U0THR = Data;//эхо для проверки
    break;
  case THRE_INTR_ID:  // THRE Interrupt
      // вызывается u0TXproc, если есть что передавать то возвращает true
      // и соответственно в Data передаётся "валидное" значение
      // если передавать нечего, то возвращает false
      if (u0TXproc !=0)
      {
        if(u0TXproc(&Data))
        {
          U0THR = Data;
        }
      }
      U0LSR;
    break;
    default:
      U0LSR;
      U0RBR;
  }
  VICADDRESS = 0;  // Clear interrupt in VIC.
}

/*************************************************************************
 * Function Name: Uart1Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART 1 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart1Isr(void)
{
Int32U UartIntId = U1IIR;
Int8U Data;
  // Recognize the interrupt event
  switch (UartIntId & 0xF)
  {
  case RDA_INTR_ID: // Receive Data Available
    // Read the line state of the UART
      U1LSR;
      Data = U1RBR;
      // вызываю процедуру u0RXproc которая ложит байт
      // из приёмника в буфер пользователя
      if (u1RXproc !=0){
        u1RXproc(Data);
      }
    break;
  case THRE_INTR_ID:  // THRE Interrupt
      // вызывается u2TXproc, если есть что передавать то возвращает true
      // и соответственно в Data передаётся "валидное" значение
      // если передавать нечего, то возвращает false
      if (u1TXproc !=0)
      {
        if(u1TXproc(&Data))
        {
          U1THR = Data;
        }
      }
      U1LSR;
    break;
    default:
      U1LSR;
      U1RBR;
  }
  VICADDRESS = 0;  // Clear interrupt in VIC.
}

/*************************************************************************
 * Function Name: Uart2Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART2 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart2Isr(void)
{
Int32U UartIntId = U2IIR;
Int8U Data;
  // Recognize the interrupt event
  switch (UartIntId & 0xF)
  {
  case RDA_INTR_ID: // Receive Data Available
    // Read the line state of the UART
      U2LSR;
      Data = U2RBR;
      // вызываю процедуру u0RXproc которая ложит байт
      // из приёмника в буфер пользователя
      if (u2RXproc !=0){
        u2RXproc(Data);
      }
    break;
  case THRE_INTR_ID:  // THRE Interrupt
      // вызывается u2TXproc, если есть что передавать то возвращает true
      // и соответственно в Data передаётся "валидное" значение
      // если передавать нечего, то возвращает false
      if (u2TXproc !=0)
      {
        if(u2TXproc(&Data))
        {
          U2THR = Data;
        }
      }
      U2LSR;
    break;
  default:
    U2LSR;
    U2RBR;
  }
  VICADDRESS = 0;  // Clear interrupt in VIC.
}

/*************************************************************************
 * Function Name: Uart3Isr
 * Parameters: none
 *
 * Return: none
 *
 * Description: UART3 interrupt routine
 *
 *************************************************************************/
__irq __arm void Uart3Isr(void)
{
Int32U UartIntId = U3IIR;

  // Recognize the interrupt event
  switch (UartIntId & 0xF)
  {
  case RDA_INTR_ID: // Receive Data Available
    // Read the line state of the UART
      U3LSR;
      U3Data = U3RBR;
      // вызываю процедуру u0RXproc которая ложит байт
      // из приёмника в буфер пользователя
      if (u3RXproc !=0){
        u3RXproc(U3Data);
      }
      //U3LSR;
    break;
  case THRE_INTR_ID:  // THRE Interrupt
      // вызывается u2TXproc, если есть что передавать то возвращает true
      // и соответственно в Data передаётся "валидное" значение
      // если передавать нечего, то возвращает false
      if (u3TXproc !=0)
      {
        if(u3TXproc(&U3Data))
        {
          U3THR = U3Data;
        }
      }
      U3LSR;
    break;
  default:
    U3IIR;
    U3LSR;
    U3RBR;
  }
  VICADDRESS = 0;  // Clear interrupt in VIC.
}

/*************************************************************************
 * Function Name: UartInit
 * Parameters: UartNum_t Uart, Int32U IrqSlot, UartMode_t UartMode
 *
 * Return: Boolean
 *
 * Description: Init UART
 *
 *************************************************************************/
Boolean UartInit(UartNum_t Uart,Int32U IrqSlot, UartMode_t UartMode)
{
volatile Int8U Tmp;
  switch (Uart)
  {
  case UART_0:
    // Enable UART0
    PCONP_bit.PCUART0 = 1;
    // Assign Port 0,1 to UART0
    // TX
    PINSEL0_bit.P0_2 = 1;
    // RX
    PINSEL0_bit.P0_3 = 1;

    U0LCR = 0x03; // Word Length =8, no parity , 1 stop
    U0FCR = 0x06;  // Enable and Clear the UART0 FIFO, Set RX FIFO interrupt level - 1 char
    // Transmit enable
    U0TER_bit.TXEN = 1;
    Tmp = U0IER;  // Clear pending interrupts
    // enable RBR Interrupt, THRE Interrupt
    U0IER = 0x03;

    _VIC_SetVectoredIRQ((u32)&Uart0Isr,IrqSlot,VIC_UART0);
    VICINTENABLE |= 1<<VIC_UART0;
    break;
  case UART_1:
    // Enable UART1
    PCONP_bit.PCUART1 = 1;
    // TX
    PINSEL0_bit.P0_15 = 1;
    //PINSEL4_bit.P2_0 = 2;
    // RX
    PINSEL1_bit.P0_16 = 1;
    //PINSEL4_bit.P2_1 = 2;


    U1LCR = 0x03; // Word Length =8, no parity , 1 stop
    U1MCR = 0;    // Word Length =8, no parity , 1 stop
    U1FCR = 0x6;  // Enable and Clear the UART1 FIFO, Set RX FIFO interrupt level - 1 char
    // Transmit enable
    U1TER_bit.TXEN = 1;
    Tmp = U1IER;  // Clear pending interrupts

    // enable RBR Interrupt, THRE Interrupt, RX Line Status Interrupt
    // Modem Status Interrupt, CTS Interrupt Enable
    U1IER = 0x03; //| (UART1_MODEM_STAT_ENA?0x88:0);

    _VIC_SetVectoredIRQ((u32)&Uart1Isr,IrqSlot,VIC_UART1);
    VICINTENABLE |= 1<<VIC_UART1;
    break;
  case UART_2:
    // Enable UART2
    PCONP_bit.PCUART2 = 1;
    // TX
    PINSEL0_bit.P0_10 = 1;
    //PINSEL4_bit.P2_8 = 2;
    //PINMODE4_bit.P2_8 = 3;
    // RX
    PINSEL0_bit.P0_11 = 1;
    //PINSEL4_bit.P2_9 = 2;

    U2LCR = 0x03; // Word Length =8, no parity , 1 stop
    U2FCR = 0x6;  // Enable and Clear the UART2 FIFO, Set RX FIFO interrupt level - 1 char
    // Transmit enable
    U2TER_bit.TXEN = 1;
    Tmp = U2IER;  // Clear pending interrupts
    // enable RBR Interrupt, THRE Interrupt
    U2IER = 0x00;

    _VIC_SetVectoredIRQ((u32)&Uart2Isr,IrqSlot,VIC_UART2);
    VICINTENABLE |= 1<<VIC_UART2;
    break;
  case UART_3:
    // Enable UART3
    PCONP_bit.PCUART3 = 1;
    // TX
    PINSEL9_bit.P4_28 = 3;
    // RX
    PINSEL9_bit.P4_29 = 3;

    U3LCR = 0x03; // Word Length =8, no parity , 1 stop
    U3FCR = 0x0;  // Enable and Clear the UART3 FIFO, Set RX FIFO interrupt level - 1 char
    // Enable/Disable IrDA mode
    //U3ICR_bit.IRDAEN = 0;
    U3ICR = 0;
    // Transmit enable
    U3TER_bit.TXEN = 1;
    Tmp = U3IER;  // Clear pending interrupts
    // enable RBR Interrupt, THRE Interrupt, RX Line Status Interrupt
    U3IER = 0x00;//3;

    _VIC_SetVectoredIRQ((u32)&Uart3Isr,IrqSlot,VIC_UART3);
    VICINTENABLE |= 1<<VIC_UART3;
    break;
  }
  return(TRUE);
}

/*************************************************************************
 * Function Name: UartCalcDivider
 * Parameters:  Int32U Freq, Int32U Baud
 *              pInt32U pDiv, pInt32U pAddDiv, pInt32U pMul
 *
 * Return: None
 *
 * Description: Calculate the coefficients of the UART baudrate generator
 *
 *************************************************************************/
static
void UartCalcDivider(Int32U Freq, Int32U Baud,
                     pInt32U pDiv, pInt32U pAddDiv, pInt32U pMul)
{
Int32U Temp, Error = (Int32U)-1;
Int32U K1, K2, K3, Baudrate;
Int32U DivTemp, MulTemp, AddDivTemp;

  //
  for(MulTemp = 1; MulTemp < 16; ++MulTemp)
  {
    K1 = Freq*MulTemp;
    for(AddDivTemp = 1; AddDivTemp < 16; ++AddDivTemp)
    {
      K3 = (MulTemp + AddDivTemp)<<4;
      K2 =  K3 * Baud;
      DivTemp = K1/K2;
      // if DIVADDVAL>0, UnDL must be UnDL >= 0x0002 or the UART will
      // not operate at the desired baud-rate!
      if(DivTemp < 2)
      {
        continue;
      }
      Baudrate = DivTemp * K3;
      Baudrate = K1/Baudrate;
      Temp = (Baudrate > Baud)? \
                 (Baudrate - Baud): \
                 (Baud - Baudrate);
      if (Temp < Error)
      {
        Error = Temp;
        *pDiv = DivTemp;
        *pMul = MulTemp;
        *pAddDiv = AddDivTemp;
        if(Error == 0)
        {
          return;
        }
      }
    }
  }
}

/*************************************************************************
 * Function Name: UartSetLineCoding
 * Parameters:  UartNum_t Uart,UartLineCoding_t pUartCoding
 *
 * Return: None
 *
 * Description: Init UART Baud rate, Word width, Stop bits, Parity type
 *
 *************************************************************************/
void UartSetLineCoding(UartNum_t Uart,UartLineCoding_t UartCoding)
{
Int32U Mul, Div, AddDiv, Freq;

  // Check parameters
  if ((UartCoding.dwDTERate == 0) || (UartCoding.dwDTERate > UART_MAX_BAUD_RATE))
  {
    return;
  }
  switch (Uart)
  {
  case UART_0:
    Freq = SYS_GetFpclk(UART0_PCLK_OFFSET);
    UartCalcDivider(Freq,UartCoding.dwDTERate,&Div,&AddDiv,&Mul);
    U0LCR_bit.WLS = UartCoding.bDataBits;
    U0LCR_bit.SBS = UartCoding.bStopBitsFormat;
    U0LCR_bit.PE  =(UartCoding.bParityType == UART_NO_PARITY)?0:1;
    U0LCR_bit.PS  = UartCoding.bParityType;
    U0LCR_bit.DLAB = 1;
    U0DLL = Div & 0xFF;
    U0DLM = (Div >> 8) & 0xFF;
    U0FDR = AddDiv + (Mul << 4);
    U0LCR_bit.DLAB = 0;
    break;
  case UART_1:
    Freq = SYS_GetFpclk(UART1_PCLK_OFFSET);
    UartCalcDivider(Freq,UartCoding.dwDTERate,&Div,&AddDiv,&Mul);
    U1LCR_bit.WLS = UartCoding.bDataBits;
    U1LCR_bit.SBS = UartCoding.bStopBitsFormat;
    U1LCR_bit.PE  =(UartCoding.bParityType == UART_NO_PARITY)?0:1;
    U1LCR_bit.PS  = UartCoding.bParityType;
    U1LCR_bit.DLAB = 1;
    U1DLL = Div & 0xFF;
    U1DLM = (Div >> 8) & 0xFF;
    U1FDR = AddDiv + (Mul << 4);
    U1LCR_bit.DLAB = 0;
    break;
  case UART_2:
    Freq = SYS_GetFpclk(UART2_PCLK_OFFSET);
    UartCalcDivider(Freq,UartCoding.dwDTERate,&Div,&AddDiv,&Mul);
    U2LCR_bit.WLS = UartCoding.bDataBits;
    U2LCR_bit.SBS = UartCoding.bStopBitsFormat;
    U2LCR_bit.PE  =(UartCoding.bParityType == UART_NO_PARITY)?0:1;
    U2LCR_bit.PS  = UartCoding.bParityType;
    U2LCR_bit.DLAB = 1;
    U2DLL = Div & 0xFF;
    U2DLM = (Div >> 8) & 0xFF;
    U2FDR = AddDiv + (Mul << 4);
    U2LCR_bit.DLAB = 0;
    break;
  case UART_3:
    Freq = SYS_GetFpclk(UART3_PCLK_OFFSET);
    UartCalcDivider(Freq,UartCoding.dwDTERate,&Div,&AddDiv,&Mul);
    U3LCR_bit.WLS = UartCoding.bDataBits;
    U3LCR_bit.SBS = UartCoding.bStopBitsFormat;
    U3LCR_bit.PE  =(UartCoding.bParityType == UART_NO_PARITY)?0:1;
    U3LCR_bit.PS  = UartCoding.bParityType;
    U3LCR_bit.DLAB = 1;
    U3DLL = Div & 0xFF;
    U3DLM = (Div >> 8) & 0xFF;
    U3FDR = AddDiv + (Mul << 4);
    U3LCR_bit.DLAB = 0;
    break;
  }
}

/*************************************************************************
 * Function Name: UartSetUartLineState
 * Parameters:  UartNum_t Uart UartNum_t Uart, Boolean Break
 *
 * Return: none
 *
 * Description: Set Uart Break Event
 *
 *************************************************************************/
void UartSetUartLineState (UartNum_t Uart, Boolean Break)
{
  switch (Uart)
  {
  case UART_0:
    U0LCR_bit.BC  = Break;
    break;
  case UART_1:
    U1LCR_bit.BC  = Break;
    break;
  case UART_2:
    U2LCR_bit.BC  = Break;
    break;
  case UART_3:
    U3LCR_bit.BC  = Break;
  }
}

#if UART1_MODEM_STAT_ENA > 0
/*************************************************************************
 * Function Name: Uart1SetModemLineState
 * Parameters:  UartModemLineState_t UartModemLineState
 *
 * Return: none
 *
 * Description: Init UART1 Modem lines state (RTS, DTR)
 *
 *************************************************************************/
void Uart1SetModemLineState(UartModemLineState_t UartModemLineState)
{
  U1MCR_bit.DTR = UartModemLineState.bDTR;
  U1MCR_bit.RTS = UartModemLineState.bRTS;
}

/*************************************************************************
 * Function Name: Uart1GetUartModemEvents
 * Parameters:  none
 *
 * Return: UartModemEvents_t
 *
 * Description: Get Uart1 Modem lines events (DCD,DSR,CTS,RI)
 *
 *************************************************************************/
UartModemEvents_t Uart1GetUartModemEvents (void)
{
UartModemEvents_t  ModemEvents;
  ModemEvents.Data = 0;
  ModemEvents.Data = AtomicExchange(ModemEvents.Data,(pInt32U)&Uart1ModemEvents);
  return(ModemEvents);
}
#endif // UART1_MODEM_STAT_ENA > 0

