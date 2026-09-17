//для связи с QRC (основная операция запись регистров)
//modbus rtu интерфейс организующий доступ к параметрам процессора через
//RS485 интерфейс
//Используется:
//UART2 (RX, TX, DIR, скорость фиксированная 115200)
//Timer2 (отсчёт временных интервалов Modbus RTU)

//вот тут аккуратнее, так как QRC кидает свои данные на адреса 0000...0003, и их надо разделить с данными F020
//причём:
//1) пакет QRC не требует ответа
//2) если данные от QRC не поступают (за какое-то время), то в регистры F020 передаются значения запрешающие регулирование
//3) в общем у QRC свой парсер который проверяет контрольную сумму
#include "uart3rs485.h"
#include "crc16.h"
#include "bastypes.h"
#include "ramdata.h"
#include "modbus.h"
#include "flash_data.h"
#include "memutil.h"

__no_init TClient uart3data @ "USB_DMA_RAM";


u8 U3RstTOut;//если снизится до 0 то ресет U3. А тут заряжать таймер.

void u3RXuser(Int8U uData);
Boolean u3TXuser(pInt8U pData);
void Timer3_Init(void);
__irq __nested __arm void TIMER3_ISR(void);
void U3_ResetModbusTimer(void);
void U3_ResetModbusState(void);
void U3_ResetModbusTimerForWaitTransmit(void);
void U3_ResetModbusTimerForWaitReceive(void);
void uart3rs485_Reinit (u8 BPS, u8 DevAddr);


void uart3rs485_init (void){
  UartInit(UART_3,0,NORM);
  UartSetLineCoding(UART_3,UartLineCoding_115200);
  u3RXproc = u3RXuser;
  u3TXproc = u3TXuser;
  U3IER = 0x01;//заблокировать передатчик. разрешить приём.
  uart3data.DevAddr =1;//адрес устройства в сети модбас
  uart3data.Idx = 0;//буфер начать с начала
  uart3data.TXCount = 0;
  uart3data.ClntTimeOut = _15bits_Delay_115200bps;
  uart3data.OnCallBack = U3_ResetModbusTimerForWaitTransmit;
  uart3data.ID = 3;//номер интерфейса
  /////////////////////////////////
  Timer3_Init();
  //uart3rs485_Reinit (_nv_array.BPS3, _nv_array.DevAddr3);
  //DIR на приём
  FIO0DIR |= UART3DIR;//P1.17 - выход
  // r003 FIO3CLR |= UART2DIR;//направление "приём" DIR2 P3.26/ №26 ножка порта
  FIO0CLR |= UART3DIR;//направление "приём" DIR2 P2.07/ №26 ножка порта
  FIO0SET |= RS485_JP2_LED; //P0.25 LED2 погасить светодиод;
}


void uart3rs485_Reinit (u8 BPS, u8 DevAddr){
  //bauint w;
  //w.b[0] = BPS;
  //w.b[1] = DevAddr;
  //  
    switch (BPS)
      { case _115200://6
            UartSetLineCoding(UART_3,UartLineCoding_115200);
            uart3data.ClntTimeOut = _15bits_Delay_115200bps;
            break;
      case _57600://5
            UartSetLineCoding(UART_3,UartLineCoding_57600);
            uart3data.ClntTimeOut = _15bits_Delay_57600bps;
            break;
      case _19200://4
            UartSetLineCoding(UART_3,UartLineCoding_19200);
            uart3data.ClntTimeOut = _15bits_Delay_19200bps;
            break;
      case _9600:
            UartSetLineCoding(UART_3,UartLineCoding_9600);
            uart3data.ClntTimeOut = 0xFFFF;//_15bits_Delay_9600bps;
            break;
      case _4800:
            UartSetLineCoding(UART_3,UartLineCoding_4800);
            uart3data.ClntTimeOut = 0xFFFF;
            break;
      case _1200:
            UartSetLineCoding(UART_3,UartLineCoding_1200);
            uart3data.ClntTimeOut = 0xFFFF;
            break;
      }
  uart3data.DevAddr = DevAddr;//адрес устройства в сети модбас
}

void Timer3_Init(void)
{
  PCONP_bit.PCTIM3 = 1;
  T3TCR = 0; //Stop timer
  T3PR	= 0; //
  T3TC	= 0; //
  T3PC	= 0; //Prescaler is set to no division.
  T3CCR = 0; //Capture is disabled
  T3EMR = 0; //No external match output
  T3MR0 = uart3data.ClntTimeOut ;//Count up to this value
  T3MCR = 3; // Reset/Stop/interrupt on MR0 (match register 0).
  _VIC_SetVectoredIRQ((u32)&TIMER3_ISR,0,VIC_TIMER3);
  VICINTENABLE |= 1<< VIC_TIMER3;
  U3_ResetModbusState();
}

void U3_ResetModbusTimerForWaitTransmit(void)
{
  FIO0SET |= UART3DIR;//DIR = 1 включу драйвер не передачу
  uart3data.Idx = 0;//индекс на начало буфера
  uart3data.Status.bits.WaitForTransmit = 1;//ждать переключения CTS
  uart3data.Status.bits.WaitForReceive = 0;
  T3IR = 0xff;
  T3TC   = 0;
  T3MR0 = _CTS_Delay*2;//скока ждать переключения CTS
  T3TCR = 1;//TIMER2_START;
}

void U3_ResetModbusTimerForWaitReceive(void)
{
  FIO0CLR |= UART3DIR;//включу драйвер не приём
  uart3data.Idx = 0;//индекс на начало буфера
  uart3data.Status.bits.WaitForTransmit = 0;
  uart3data.Status.bits.WaitForReceive = 1;//ждать переключения CTS
  T3IR = 0xff;
  T3TC   = 0;
  T3MR0 = _CTS_Delay*2;//скока ждать переключения CTS
  T3TCR = 1;//TIMER2_START;
}

void U3_ResetModbusTimer(void)
{ //готовлю таймер к приходу нового байта
  T3IR = 0xff;
  T3TC = 0;
  T3MR0 = uart3data.ClntTimeOut;//Clients[ClntRS232].ClntTimeOut;//timer0->MR0   = _15bits_Delay_115200bps;
  T3TCR = 1;//timer0->TCR.d = 1; //TIMER0_START;
}

void U3_ResetModbusState(void)
{
  FIO0CLR |= UART3DIR;//включу драйвер не приём
  uart3data.Idx = 0;//индекс на начало буфера
  uart3data.Status.bits.WaitForTransmit = 0;
  uart3data.Status.bits.WaitForReceive = 0;
  //U3IER = 0x00;// 
  U3IIR;//почистить отложенные прерывания
  U3RBR;//хрен знает ещё и прочитаю рег данных
  U3LSR;
  U3IER = 0x01;//разрешить приём. заблокировать передатчик.
}

// Timer3 interrupt handler
__irq __nested __arm void TIMER3_ISR(void){
  //////////////////////////////////
  T3IR  = 0xff;
  T3TCR = 0; //Stop timer
  T3TC  = 0;
  //////////////////////////////////
  if (uart3data.Status.bits.WaitForTransmit)//переключен CTS. начать передачу
    {   uart3data.Status.bits.WaitForTransmit = 0;
	//начать передачу
        //U3IER = 0x00;// 
        U3IIR;//почистить отложенные прерывания
        U3RBR;//хрен знает ещё и прочитаю рег данных
        U3LSR;
        U3IER = 0x02;// разрешить прерывания передатчикa, блокировать приёмник
	///////////////////////////////////////////////
	U3THR = uart3data.Buffer[uart3data.Idx++];
	uart3data.TXCount--;//ModbusBufferCount--;//длина пакета
        goto Exit;
    }
  if (uart3data.Status.bits.WaitForReceive)//переключен CTS. начать приём
    {   uart3data.Status.bits.WaitForReceive = 0;
        U3RstTOut = U3_Time_Out;//заряжаю счётчик тайм-аута
        ////U3IER = 0x00;// 
        U3IIR;//почистить отложенные прерывания
        U3RBR;//хрен знает ещё и прочитаю рег данных
        U3LSR;
        U3IER = 0x01;//разрешить приём. заблокировать передатчик.
        goto Exit;
    }
  //запретить прерывания от UART
  U3IIR;//почистить отложенные прерывания
  U3LSR;
  U3RBR;//хрен знает ещё и прочитаю рег данных
  U3IER = 0; //запретить передачу и приём !!!!
  __enable_interrupt(); // handles nested interrupt
  //вдруг будет отложенная передача
  uart3data.OnCallBackReceive = &U3_ResetModbusTimerForWaitTransmit;
  //декодирование команды
  FIO0CLR |= RS485_JP2_LED; //LED2 принят пакет, надо зажечь светодиод
  //покажу себе принятый пакет
  //RAM_DATA.i[2] = uart2data.Idx;
  //u8_mem_cpy(&uart2data.Buffer[0], (u8 *)&RAM_DATA.i[3], (u8) (uart2data.Idx & 0x0000007F));
  if (command_decode(&uart3data))//тут проверить контрольную сумму, совпала? идём дальше
    {
      U3_ResetModbusTimerForWaitTransmit();
    }
  else //команда не опознана, или просто нельзя отвечать
    { U3_ResetModbusState();
      FIO0SET |= RS485_JP2_LED; //LED2 команда не опознана, надо погасить светодиод;
    };
  Exit:
    VICADDRESS = 0;
}

void u3RXuser(Int8U uData){
  //полученный байт помещается в Buffer
  //при этом счётчик Index инкрементируется
  uart3data.Buffer[uart3data.Idx++] = uData;
  uart3data.Idx &= 0xff; //чтоб по кольцу гонял
  //после приёма байта взводится таймер (в нашем случае Таймер0)
  //выдержка времени = 1.5 байта при заданной скорости
  U3_ResetModbusTimer();
  //каждый принятый байт перезапускает таймер
  //при срабатывании таймера запускается процедура декодирования
  //принятой информации, прерывания таймера запрещаются
}

Boolean u3TXuser(pInt8U pData){
  //перед началом передачи обнулять ModbusBufferIndex
  if ((uart3data.TXCount--) != 0) {
    *pData = uart3data.Buffer[uart3data.Idx++];
    uart3data.Idx &= 0xff;
    return(true);//ещё есть что передавать
  }
  else {
    //данные переданы
    //U3IER = 0x00;// 
    U3IIR;  // Clear pending interrupts
    U3RBR;//хрен знает ещё и прочитаю рег данных
    U3LSR;
    U3IER = 0x01;//данные переданы. заблокировать передатчик. разрешить приём.
    U3_ResetModbusTimerForWaitReceive();
    FIO0SET |= RS485_JP2_LED; //LED2 пакет передан, надо погасить светодиод
    return(false); //передавать больше нечего
  }
}
