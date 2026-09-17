//modbus rtu интерфейс организующий доступ к параметрам процессора через
//RS485 интерфейс #1
//Используется:
//UART1 (RX, TX, DIR, скорость фиксированная 115200)
//Timer1 (отсчёт временных интервалов Modbus RTU)
//светодиод: зажигается при получении запроса, гаснет в конце ответа
#include "uart1rs485.h"
#include "mbtypes.h"
#include "crc16.h"
#include "bastypes.h"
#include "ramdata.h"
#include "flash_data.h"
#include "modbus.h"
#include "memutil.h"

TClient uart1data;

void u1RXuser(Int8U uData);
Boolean u1TXuser(pInt8U pData);
void Timer1_Init(void);
__irq __nested __arm void TIMER1_ISR(void);
void U1_ResetModbusTimer(void);
void U1_ResetModbusState(void);
void U1_ResetModbusTimerForWaitTransmit(void);
void U1_ResetModbusTimerForWaitReceive(void);
void uart1rs485_Reinit (u8 BPS, u8 DevAddr);

void uart1rs485_init (void){
  UartInit(UART_1,0,NORM);
  UartSetLineCoding(UART_1,UartLineCoding_115200);
  u1RXproc = u1RXuser;
  u1TXproc = u1TXuser;
  U1IER = 0x01;//заблокировать передатчик. разрешить приём.
  uart1data.DevAddr = 1;//адрес устройства в сети модбас
  uart1data.Idx = 0;//буфер начать с начала
  uart1data.TXCount = 0;
  uart1data.ClntTimeOut = _15bits_Delay_115200bps;
  uart1data.OnCallBack  = U1_ResetModbusTimerForWaitTransmit;
  uart1data.ID = 1;//номер интерфейса
  //uart1rs485_Reinit (_nv_array.BPS1, _nv_array.DevAddr1);
  Timer1_Init();
  //DIR на приём
  FIO0DIR |= UART1DIR;//P0.9 - выход
  FIO0CLR  = UART1DIR;//направление "приём" DIR1 P0.9
  FIO0SET  = RS485_JP1_LED; //LED1 погасить светодиод;
}

void uart1rs485_Reinit (u8 BPS, u8 DevAddr){

    switch (BPS)
    { 
    case _115200:
      UartSetLineCoding(UART_1,UartLineCoding_115200);
      uart1data.ClntTimeOut = _15bits_Delay_115200bps;
      break;
    case _57600:
      UartSetLineCoding(UART_1,UartLineCoding_57600);
      uart1data.ClntTimeOut = _15bits_Delay_57600bps;
      break;
    case _19200:
      UartSetLineCoding(UART_1,UartLineCoding_19200);
      uart1data.ClntTimeOut = _15bits_Delay_19200bps;
      break;
    case _9600:
      UartSetLineCoding(UART_1,UartLineCoding_9600);
      uart1data.ClntTimeOut = _15bits_Delay_9600bps;//0xFFFF;//_15bits_Delay_9600bps;
      break;     
   /* case _4800:
      UartSetLineCoding(UART_1,UartLineCoding_4800);
      uart1data.ClntTimeOut = _15bits_Delay_4800bps;
      break;*/
    default:
      UartSetLineCoding(UART_1,UartLineCoding_19200);
      uart1data.ClntTimeOut = _15bits_Delay_19200bps;
      break;
    }
  uart1data.DevAddr = DevAddr;//адрес устройства в сети модбас
}

void Timer1_Init(void)
{
  PCONP_bit.PCTIM1 = 1;
  T1TCR = 0; //Stop timer
  T1PR	= 0; //
  T1TC	= 0; //
  T1PC	= 0; //Prescaler is set to no division.
  T1CCR = 0; //Capture is disabled
  T1EMR = 0; //No external match output
  T1MR0 = uart1data.ClntTimeOut ;//Count up to this value
  T1MCR = 3; // Reset/Stop/interrupt on MR0 (match register 0).
  //VIC_SetVectoredIRQ(TIMER2_ISR,0,VIC_TIMER2);
  _VIC_SetVectoredIRQ((u32)&TIMER1_ISR,3,VIC_TIMER1);
  VICINTENABLE |= 1<< VIC_TIMER1;
  U1_ResetModbusState();
}

void U1_ResetModbusTimerForWaitTransmit(void)
{
  FIO0SET = UART1DIR;//DIR = 1 включу драйвер на передачу
  uart1data.Idx = 0;//индекс на начало буфера
  uart1data.Status.bits.WaitForTransmit = 1;//ждать переключения CTS
  uart1data.Status.bits.WaitForReceive = 0;
  T1IR = 0xff;
  T1TC   = 0;
  T1MR0 = _CTS_Delay;//скока ждать переключения CTS
  T1TCR = 1;//TIMER2_START;
}

void U1_ResetModbusTimerForWaitReceive(void)
{
  FIO0CLR = UART1DIR;//включу драйвер на приём
  uart1data.Idx = 0;//индекс на начало буфера
  uart1data.Status.bits.WaitForTransmit = 0;
  uart1data.Status.bits.WaitForReceive = 1;//ждать переключения CTS
  T1IR = 0xff;
  T1TC   = 0;
  T1MR0 = _CTS_Delay;//скока ждать переключения CTS
  T1TCR = 1;//TIMER2_START;
}

void U1_ResetModbusTimer(void)
{ //готовлю таймер к приходу нового байта
  T1IR = 0xff;
  T1TC = 0;
  T1MR0 = uart1data.ClntTimeOut;//Clients[ClntRS232].ClntTimeOut;//timer0->MR0   = _15bits_Delay_115200bps;
  T1TCR = 1;//timer0->TCR.d = 1; //TIMER0_START;
}

void U1_ResetModbusState(void)
{
  FIO0CLR = UART1DIR;//включу драйвер на приём
  uart1data.Idx = 0;//индекс на начало буфера
  uart1data.Status.bits.WaitForTransmit = 0;
  uart1data.Status.bits.WaitForReceive = 0;
  U1IIR;  // Clear pending interrupts
  U1IER = 0x01;//разрешить приём. заблокировать передатчик.
}

// Timer1 interrupt handler
__irq __nested __arm void TIMER1_ISR(void){
  //////////////////////////////////
  T1IR = 0xff;
  T1TCR = 0; //Stop timer
  T1TC   = 0;
  //////////////////////////////////
  if (uart1data.Status.bits.WaitForTransmit)//переключен CTS. начать передачу
    {   uart1data.Status.bits.WaitForTransmit = 0;
	//начать передачу
        U1IER = 0x02;// разрешить прерывания передатчикa, блокировать приёмник
	///////////////////////////////////////////////
	U1THR = uart1data.Buffer[uart1data.Idx++];
	uart1data.TXCount--;//ModbusBufferCount--;//длина пакета
        goto Exit;
    }
  if (uart1data.Status.bits.WaitForReceive)//переключен CTS. начать приём
    {   uart1data.Status.bits.WaitForReceive = 0;
        U1IER = 0x01;//разрешить приём. заблокировать передатчик.
        goto Exit;
    }
  //запретить прерывания от UART
  U1IER = 0; //запретить передачу и приём
  __enable_interrupt(); // handles nested interrupt
  //вдруг будет отложенная передача
  uart1data.OnCallBackReceive = &U1_ResetModbusTimerForWaitTransmit;
  FIO0CLR = RS485_JP1_LED; //LED1 принят пакет, надо зажечь светодиод

  //RAM_DATA.i[2] = uart1data.Idx;
  //u8_mem_cpy(&uart1data.Buffer[0], (u8 *)&RAM_DATA.i[3], (u8) (uart1data.Idx & 0x0000007F));

  if (command_decode(&uart1data))
    { U1_ResetModbusTimerForWaitTransmit();}
  else //команда не опознана
    { U1_ResetModbusState();
      FIO0SET = RS485_JP1_LED; //LED1 команда не опознана, надо погасить светодиод;
    }
  Exit:
    VICADDRESS = 0;
}

void u1RXuser(Int8U uData){
  //полученный байт помещается в Buffer
  //при этом счётчик Index инкрементируется
  uart1data.Buffer[uart1data.Idx++] = uData;
  uart1data.Idx &= 0xff; //чтоб по кольцу гонял
  //после приёма байта взводится таймер (в нашем случае Таймер0)
  //выдержка времени = 1.5 байта при заданной скорости
  //FIO0SET |= P0i26;
  U1_ResetModbusTimer();
  //каждый принятый байт перезапускает таймер
  //при срабатывании таймера запускается процедура декодирования
  //принятой информации, прерывания таймера запрещаются
}

Boolean u1TXuser(pInt8U pData){
  //перед началом передачи обнулять ModbusBufferIndex
  if ((uart1data.TXCount--) != 0) {
    *pData = uart1data.Buffer[uart1data.Idx++];
    uart1data.Idx &= 0xff;
    return(true);//ещё есть что передавать
  }
  else {
    U1IIR;  // Clear pending interrupts
    U1IER = 0x01;//данные переданы. заблокировать передатчик. разрешить приём.
    U1_ResetModbusTimerForWaitReceive();
    FIO0SET = RS485_JP1_LED; //LED1 пакет передан, надо погасить светодиод;
    return(false); //передавать больше нечего
  }
}
