#include "lnkmngr.h"
#include "lnkmngru2.h"
#include "crc16.h"
#include "bastypes.h"
#include "mbtypes.h"
#include "uart.h"
#include "memutil.h"
#include "ramdata.h"
#include "f020.h"
#include "uart3rs485.h"


char _lnk2_slot_idx = 0;//текущий слот

__no_init TLnkSlot lnks2_RAM_READ   @ "USB_DMA_RAM";
__no_init TLnkSlot lnks2_RTR_WRITE  @ "USB_DMA_RAM";//ретранслятор
__no_init TLnkSlot lnks2_FLASH_READ @ "USB_DMA_RAM";
//TLnkSlot lnks_FLASH_WRITE;
__no_init TLnkSlot lnks2_ALB        @ "USB_DMA_RAM";//чтение буфера событий F020

TLnkSlot *pLnk2Slots[lnk2_slots_usage];

void Lnk2Init (void);//инициализация структур сервера
void Lnk2Start (void);//запуск сервера
/////////////////////////////////////////////
//собственно сервер работает распределённо в прерывании... таймера и уарта
void lnks2_RAM_READ_END (void);//получены данные RAM от F020
void lnks2_RAM_WRITE_END (void);//записаны данные в RAM от F020
void lnks2_FLASH_READ_END (void);//получены данные FLASH от F020
////////////////////////////////////////////////////////////////////////////////
//функции аппаратного обеспечения сервера
void u2RXuser(Int8U uData);
Boolean u2TXuser(pInt8U pData);
////////////////////////////////////////////////////////////////////////////////
void Lnk2Start (void){//запуск сервера
  //после того как всё инициализировано, начинает отправку данных первого слота
  T2TCR = 1;//TIMER0_START;
  //собственно всё начинается с запуска таймера 3. он сработает через 3байта
  //и вызовет обработчик TIMER3_ISR... вот тут то всё и зашевелится
}

void Lnk2SetTimer (unsigned int Delay){
  T2TCR = 0;//TIMER3 STOP;
  T2IR = 0xff;
  T2TC = 0;
  T2MR0 = Delay ;
  T2TCR = 1;//TIMER3 START;
}

void u2RXuser(Int8U uData){
  TLnkSlot *pSlot = pLnk2Slots[_lnk2_slot_idx];//при входе получаю ссылку на действующий слот
  //поступил какой-то байт от девайса, нужно положить его в InBuf, ++ счётчик
  //остановить таймер тайм аута
  //загрузить в таймер уставку ожидания конца пакета FrameEnd ,взвести флаг WaitFrameEnd
  pSlot->InBuf[pSlot->InptCnt] = uData;//кладу принятый байт в буфер приёмника
  pSlot->InptCnt++;
  pSlot->Status.bits.WaitTimeOut = 0;//больше не жду тайм аута
  pSlot->Status.bits.FrameEnd = 1;//а жду конца пакета
  Lnk2SetTimer(pSlot->FrameEnd);//уставка Фрейм Енда загружается в таймер (тем самым таймер ресетится после каждого байта)
}

Boolean u2TXuser(pInt8U pData){
  TLnkSlot *pSlot = pLnk2Slots[_lnk2_slot_idx];//при входе получаю ссылку на действующий слот
  //слот занят передачей данных, записываю данные в буфер передатчика до передачи последнего байта
  //Когда передан последний байт, сбрасываю признак SendBytes
  //и далее смотрю, требует ли ответа данный слот (значение NoRespond)
  //если не требует NoRespond = 1, переход к следующему слоту
  //если требует (NoRespond = 0), то жду ответа от девайса, для чего загружаю в таймер уставку тайм аута
  //запрещаю прерывания передатчика и разрешаю прерывания приёмника и WaitRespond=1 и чищу счётчик приёмника
  if (pSlot->_Cnt != pSlot->OutCnt) {//проверяю всё ли передано
    //RAM_DATA.CNT1++; //!!!!!!!!!!!!!! DEBUG !!!!!!
      *pData = pSlot->OutBuf[pSlot->_Cnt++];//
      return(true);//ещё есть что передавать
    }
    else {//данные переданы.
      //pSlot->_Cnt = 0;//буфер начать с начала
      //U3IER;//clear pending interrupt
      //U3IER = 0x01;//заблокировать передатчик. разрешить приём.
//      FIO0SET |= WRK_LED; //LED1 пакет передан, надо погасить светодиод;
      //проверка, требует ли ответа засланный пакет
      if (!pSlot->Status.bits.NoRespond){//посмотрел - все нуждаются в респонде
        //пакет требует ответа
        //запустить таймер на отсчёт времени переключения драйвера
        //запретить UART
        U2IER;//почистить отложенные прерывания
        U2IER = 0;//остановить уарт
        U2RBR;//хрен знает ещё и прочитаю рег данных
        //FIO1CLR |= UART3DIR;//включу драйвер на приём //07.12.10
        pSlot->InptCnt = 0;
        pSlot->Status.bits.DIR_Tougle = 1;//таймер считает время переключения драйвера на приём
        Lnk2SetTimer(pSlot->DIRTougleTime);//уставка переключения загружается в таймер (на ожидание ответа)
        return(false); //передавать больше нечего
        //пакет требует ответа
        //pSlot->InptCnt = 0;
        //pSlot->Status.bits.WaitTimeOut = 1;
        //FIO1CLR |= UART3DIR;//включу драйвер на приём //07.12.10
        //LnkSetTimer(pSlot->TimeOut);//уставка тайм аута загружается в таймер (на ожидание ответа)
        //return(false); //передавать больше нечего
      }
      else {
        //пакет не требует ответа, переключится на другой слот
        //запустить таймер, по прерыванию от которого будет переключен слот
        if (pSlot->OnRead){ pSlot->OnRead();}//14.11.12 запущу обработчик
        pSlot->Status.bits.change = 1;//если у слота состояние 0 - значит его нужно переключить
        //FIO0SET |= UART2DIR;//включу драйвер на передачу //10.12.10
        Lnk2SetTimer(pSlot->FrameEnd);//через заданное время сработает
        return(false); //передавать больше нечего
      }
    }
}

//Timer2 interrupt handler

__irq __nested __arm  void TIMER2_ISR(void) {
  T2IR = 0xff;//сброс всех флагов прерывания таймера
  T2TCR = 0; //и его остановка
  T2TC  = 0;

  TLnkSlot *pSlot;
  pSlot  = pLnk2Slots[_lnk2_slot_idx];//при входе получаю ссылку на действующий слот

  if (pSlot->Status.bits.change) {
    pSlot->Status.bits.change = 0;
    //нужно сделать активным следующий слот
    _lnk2_slot_idx++;
    if (_lnk2_slot_idx == lnk2_slots_usage) {_lnk2_slot_idx = 0;}
    pSlot = pLnk2Slots[_lnk2_slot_idx];//установлен следующий слот
    //чищу его флаги
    pSlot->Status.bits.WaitTimeOut = 0;
    pSlot->Status.bits.FrameEnd    = 0;
    pSlot->Status.bits.DIR_Tougle  = 0;
    //проверить вдруг он скипуется
    if (pSlot->Status.bits.Skip){
      //пропустить этот слот
      pSlot->Status.bits.change = 1;//если у слота состояние change=1 - значит его нужно переключить
      //LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
      Lnk2SetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
      goto Exit;
    }
    //не скипуется
    //и передать ему управление, но перед этим нужно присвоить ему флаг состояния с которым он должен
    //войти в обработчик слота, как правило это SendBytes, для него нужно обнулить pSlot->_Cnt
    //и передать первый байт
    pSlot->_Cnt = 0;
    //FIO0SET |= UART2DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    U2IER;//почистить отложенные прерывания
    U2IER = 0x02;// разрешить прерывания передатчикa, блокировать приёмник
    U2THR = pSlot->OutBuf[pSlot->_Cnt++];//
    goto Exit;
  }
  //проверяю что делал слот, возможные действия:
  //0) ждал переключения драйвера на приём
  //1) ждал тайм аута WaitTimeOut, и раз он сюда пришёл значит дождался
  //2) ждал конца пакета FrameEnd, соответсвенно дождался.
  //ждал переключения драйвера на приём?
  if (pSlot->Status.bits.DIR_Tougle) {
    //если попал в эту ветку, значит драйвер переключился на приём, можно разрешать принимать данные
    //нужно выставить ошибку ErrTimeOut, вызвать обработчик слота (пусть знает что его посыл не дошёл до адресата)
    //перейти к следующему слоту
    //FIO0CLR |= UART2DIR;//включу драйвер на приём //07.12.10
    pSlot->Status.bits.DIR_Tougle = 0;//
    pSlot->Status.bits.WaitTimeOut = 1;//теперь жду тайм аута
    pSlot->InptCnt = 0;
    U2IER;//почистить отложенные прерывания
    U2IER = 0;//запретить всё.
    U2RBR;//хрен знает ещё и прочитаю рег данных
    U2IER = 1;//разрешить приём. заблокировать передатчик.
    Lnk2SetTimer(pSlot->TimeOut); //уставка тайм аута загружается в таймер (на ожидание ответа)
    goto Exit;
  };
  //И так если таймер ждал тайм аута и дождался, то ничего не делая, вызываю сервер
  if (pSlot->Status.bits.WaitTimeOut) {
    //RAM_DATA.CNT0++;//!!!!!!!!!!!!!! DEBUG !!!!!!
    //если попал в эту ветку, значит ни одного байта в ответ не поступило, сработал таймер таймуата
    //нужно выставить ошибку ErrTimeOut, вызвать обработчик слота (пусть знает что его посыл не дошёл до адресата)
    //перейти к следующему слоту
    pSlot->Status.bits.WaitTimeOut = 0;
    pSlot->Status.bits.ErrTimeOut = 1;//установлю индикатор "НЕТ СВЯЗИ!!!"
    U2IER;//почистить отложенные прерывания
    U2IER = 0;//остановить уарт
    U2RBR;//хрен знает ещё и прочитаю рег данных
 //   FIO0SET |= WRK_LED; //LED3 надо погасить светодиод;
    //FIO0SET |= UART2DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    __enable_interrupt(); // handles nested interrupt
    if (pSlot->OnRead){ pSlot->OnRead();}
    pSlot->Status.bits.change = 1;//езначит его нужно переключить слот
    Lnk2SetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
    goto Exit;
  };
  //Если ждал конца пакета FrameEnd, то нужно сбросить флаг WaitRespond (а то подумает что байт принял) и вызвать сервер
  //таймер всегда входит в сервер остановленным, если сервер решит, то запустит
  if (pSlot->Status.bits.FrameEnd){
    U2IER;//почистить отложенные прерывания
    U2RBR;//хрен знает ещё и прочитаю рег данных
    U2IER = 0;//что-то до конца принято, остановить уарт
    //после принятия последнего в пакете байта, прошло FrameEnd времени что говорит мне об успешном принятии пакета
    //нужно запретить прерывания приёма и передатчика, остановить таймер, а все флаги состояния почистить
    //выполнить процедуру разбора принятого, после чего, перейти к следующему слоту,
    //чищу флаги
    pSlot->Status.bits.WaitTimeOut = 0;
    pSlot->Status.bits.ErrTimeOut  = 0;//пока сброшу "НЕТ СВЯЗИ!!!", но решать это нужно в OnRead (там происходит проверка принятого пакета)
    pSlot->Status.bits.FrameEnd    = 0;

//    FIO0CLR |= WRK_LED; //LED3 принят пакет, надо зажечь светодиод
    //FIO0SET |= UART2DIR;//DIR = 1 включу драйвер на передачу //10.12.10 ???
    __enable_interrupt(); // handles nested interrupt
    if (pSlot->OnRead){ pSlot->OnRead();}
    pSlot->Status.bits.change = 1;//переключить слот
    //LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
    Lnk2SetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
    goto Exit;
  }
  Exit:
    VICADDRESS = 0;//значит было неопределённое состояние!!!
}

void lnks2_RAM_READ_END (void){//получены данные RAM от F020
  static u8 ALB_READ_START = 32;//каждые 32 чтения RAM будет вызывться чтение ALB
  //RAM_DATA.i[0] = lnks1_RAM_READ.InptCnt;
  //RAM_DATA.i[1] = lnks1_RAM_READ.Status.bits.ErrTimeOut;
  //RAM_DATA.i[2] = lnks1_RAM_READ.Status.bits.CRC_Error;
  lnks2_RAM_READ.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks2_RAM_READ.InBuf[0], lnks2_RAM_READ.InptCnt) != 0);
  if ((lnks2_RAM_READ.Status.bits.ErrTimeOut) || (lnks2_RAM_READ.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тайм аут то не обрабатываюю данные
    return;
  }
  //сваплю и копирую в образ памяти RAM 020
  swp_copy_u16( &lnks2_RAM_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &F020_Reg,
                     lnks2_RAM_READ.InBuf[_u_byte_cnt] >> 1);
  //RAM_DATA.i[3]++;
  //проверю, установлен ли FLASH_WRITED //для внешней системы даю знать что флэш прочитан
  //FLASH_WRITED bit     reg_0001L.2;
    if (F020_Reg.r0001 & r0001_FLASH_WRITED){ 
    //да, есть признак того что, данные во флэшке изменены, и требуется обновить данные в буфере LPC
    //для этого запускаю слот чтения флэша (вернее разрешаю переключится на него)
    lnks2_FLASH_READ._Cnt =    0;//кол-во переданных символов
    lnks2_FLASH_READ.Status.bits.Skip = 0;
  }
  //проверка активации слота чтения ALB
  if (ALB_READ_START-- == 0) {
    ALB_READ_START = 32;
    //запускаю слот чтения ALB (вернее разрешаю переключится на него)
      lnks2_ALB._Cnt =    0;//кол-во переданных символов
      lnks2_ALB.Status.bits.Skip = 0;
  }
  //далее активация слота USER который передаст Ereg дублирующему регулятору
  /*if ((RAM_DATA.FLAGS & FLAGS_SELECT)){//1-данный RAM первого МБВ (на U1)
    lnks3_USER_WRITE_Fill();
    lnks3_USER._Cnt = 0;//чтобы начать передачу сначала
    lnks3_USER.Status.bits.Skip = 0;
  }*/
    if (U3RstTOut-- == 0){
        U3RstTOut = U3_Time_Out;//заряжаю счётчик тайм-аута
        U3_ResetModbusTimerForWaitReceive(); 
    }
}

void lnks2_RTR_WRITE_END (void){//ретрансляция пакета завершена, и на него даже получен ответ!
  bauint w;
  static u8 TryCnt = 0;
  //проверка поступивших данных
  lnks2_RTR_WRITE.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks2_RTR_WRITE.InBuf[0], lnks2_RTR_WRITE.InptCnt) != 0);
  //от 020 должно прийти подтверждение записи данных
  if ((lnks2_RTR_WRITE.Status.bits.ErrTimeOut) || (lnks2_RTR_WRITE.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тай аут то
    //перезапускаю слот на повторно выполнение перевыполнение, но не более 3-х попыток
    if (TryCnt == 0) {
      TryCnt = 3;
      lnks2_RTR_WRITE.Status.bits.Skip = 1;
    }
    TryCnt--;
    lnks2_RTR_WRITE._Cnt = 0;//чтобы начать передачу сначала
    return;
  }

  //если подтверждение получено, то останавливаю слот до следующей записи
  lnks2_RTR_WRITE.Status.bits.Skip = 1;
  TryCnt = 3;
  //ретранслировать обратно ответ устройства, если требуется
  if (lnks2_RTR_WRITE.Status.bits.WaitRespond) {
    lnks2_RTR_WRITE.Status.bits.WaitRespond = 0;//отправляю один раз
    //в ответ нужно "подмешать" адрес интерфейса от которого пришёл запрос (у меня F020 -  всегда "1",
    //а интрефейсы могут быть от 1 до 255)
    lnks2_RTR_WRITE.InBuf[0] = lnks2_RTR_WRITE.pC->DevAddr;//Buffer[0];
    //теперь вычисляю новую контрольную сумму
    w.i = GetCRC16(&lnks2_RTR_WRITE.InBuf[0], (lnks2_RTR_WRITE.InptCnt - 2));//подсчет CRC буфера кроме уже имеющейся CRC
    //подставляю новую CRC
    lnks2_RTR_WRITE.InBuf[lnks2_RTR_WRITE.InptCnt - 2] = w.b[1];//high byte CRC
    lnks2_RTR_WRITE.InBuf[lnks2_RTR_WRITE.InptCnt - 1] = w.b[0];//low byte CRC
    //копирую полученный слотом данные в буфер передатчика интерфейса
    u8_mem_cpy(&lnks2_RTR_WRITE.InBuf[0], &lnks2_RTR_WRITE.pC->Buffer[0], lnks2_RTR_WRITE.InptCnt);

    //RAM_DATA.i[2] = lnks_RTR_WRITE.InptCnt;
    //u8_mem_cpy(&lnks_RTR_WRITE.InBuf[0], (u8 *)&RAM_DATA.i[3], (u8) (lnks_RTR_WRITE.InptCnt & 0x0000007F));

    //сообщаю сколько байт нужно передать (в общем готовлю к передаче)
    lnks2_RTR_WRITE.pC->TXCount = lnks2_RTR_WRITE.InptCnt;
    lnks2_RTR_WRITE.pC->Idx = 0;
    lnks2_RTR_WRITE.pC->OnCallBackReceive();
    //lnks_RTR_WRITE.pC->OnCallBack();
  }
}


void lnks2_FLASH_READ_END (void){//получены данные FLASH от F020
  //bauint w;

  //RAM_DATA.i[6]++;
  if (lnks2_FLASH_READ.Status.bits.ErrTimeOut) {
    return; //если был тай аут то не обновляю не проверяю данные
  }
  if (GetCRC16_WaitZero(&lnks2_FLASH_READ.InBuf[0], lnks2_FLASH_READ.InptCnt)) {
    //если пакет принят сбойный - сделать повтор чтения
    return;
  }
    swp_copy_u16( &lnks2_FLASH_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &F020_FLASH_Reg.F020[0],
                     lnks2_FLASH_READ.InBuf[_u_byte_cnt] >> 1);
  //тут я должен сравнить настройки связи порта (скорость и адрес)
  //если они отличаются в LPC и 020, то сделать реинициализацию RS485_1 интерфейса (uart2rs485)
  //uart3rs485_Reinit (F020_FLASH_Reg.F020[141],//SBR
  //                   F020_FLASH_Reg.F020[140]); //SAAddr
  //mem_cpy (&RAM_DATA.i[0], &F020_FLASH_DATA[0], 32);
  //останавливаю слот до следующей записи
  lnks2_FLASH_READ.Status.bits.Skip = 1;
}

void lnks2_ALB_READ_END (void){//получены данные ALB от F020
  if (lnks2_ALB.Status.bits.ErrTimeOut) {
    return; //если был тайм аут то не обновляю не проверяю данные
  }
  if (GetCRC16_WaitZero(&lnks2_ALB.InBuf[0], lnks2_ALB.InptCnt)) {
    //если пакет принят сбойный - сделать повтор чтения
    return;
  }
   swp_copy_u16( &lnks2_ALB.InBuf[_u_data_section_cm03],
                  (u16 *) &ALB_DATA,
                     lnks2_ALB.InBuf[_u_byte_cnt] >> 1);
  //u8_mem_cpy((u8 *)&ALB, (u8 *)&RAM_DATA.i[0], 32);
  lnks2_ALB.Status.bits.Skip = 1;//жду следующей активации
}

#define Timer2_PR 168//prescaler на сколько делится частота питающая Т3
//но и все остальные уставки нужно поделить на 168
//если не делить PR=0
//то (1/22118400    )*65535 =  0,002.9  ~0,03 сек (а надо до 0,5 довести -> в 168 раз частота должна быть меньше)
//   (1/22118400/168)*65535 = 0,497.77 ~0,50 сек
#define DevNotResp_TimeOut_500ms  0x8000 //0,5 sec
#define DevNotResp_TimeOut_1000ms 0xF000 //1 sec

void Timer2_Init(void)
{
  PCONP_bit.PCTIM2 = 1;
  T2IR = 0xff;
  T2TCR = 0; //Stop timer
  T2TC	= 0; //
  T2PR	= Timer2_PR; //
  T2PC	= 0; //Prescaler is set to no division.
  T1CCR = 0; //Capture is disabled
  T2EMR = 0; //No external match output
  T2MR0 = _15bits_Delay_115200bps / Timer2_PR; //Count up to this value
  T2MCR = 3; // Reset/Stop/interrupt on MR0 (match register 0).
  //установка обработчика прерываний
  _VIC_SetVectoredIRQ((u32)&TIMER2_ISR,3,VIC_TIMER2);
  VICINTENABLE |= 1<< VIC_TIMER2;
  //первым сработает тайме и запустит всю конструкцию в работу
}

void uart2_init (void){
  //сет хардваре
  UartInit(UART_2,0,NORM);
  UartSetLineCoding(UART_2,UartLineCoding_115200);
  U2IER = 0x01;//заблокировать передатчик. разрешить приём.
  //PINSEL0  &= 0xFFFFFFF0;
  //PINSEL0  |= (0x0A << 0) ; //TX3->P0.0, RX3->P0.1
  //PINSEL9 |= (0x0F << 24);//TX3->P4.28, RX3->P4.29
  //сет фирмваре
  u2RXproc = u2RXuser;
  u2TXproc = u2TXuser;
}

void onRAM2_READ(void){};
void onRAM2_WRITE(void){};
void onFLASH2_READ(void){};

#define __CTS_Delay 1

void Lnk2Init (void){//инициализация структур сервера
  bauint crc;
  
  lnks2_RAM_READ.Status.bits.ErrTimeOut = 1;
    _lnk2_slot_idx = 0;//текущий слот lnks_RAM_READ
  //заполннение слота чтения RAM f020
    lnks2_RAM_READ.OutBuf[0] = 0x01;//device_address;
    lnks2_RAM_READ.OutBuf[1] = 0x03;//command_code;
    lnks2_RAM_READ.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks2_RAM_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks2_RAM_READ.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks2_RAM_READ.OutBuf[5] = 0x1D;//0x3F;//num_reg_Lo;  0x3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks2_RAM_READ.OutBuf[0], 6);
    lnks2_RAM_READ.OutBuf[6]=crc.b[1];//high byte CRC
    lnks2_RAM_READ.OutBuf[7]=crc.b[0];//low byte CRC
    lnks2_RAM_READ.OutCnt =  8;//кол-во символов в пакете
    lnks2_RAM_READ.InptCnt = 0;//пока ничего не приняли
    lnks2_RAM_READ._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks2_RAM_READ.Status.Stat = 0;
    lnks2_RAM_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks2_RAM_READ.Status.bits.change = 1;//!!! зайдя в него - меняет слот, только для пуска
    lnks2_RAM_READ.FrameEnd = 120;//_15bits_Delay_115200bps / 40;//48;// / Timer3_PR; 32!
    //почему 120? это 1,8мс (1800мкс) = быстрее F020 отвечать ну просто не может!
    lnks2_RAM_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks2_RAM_READ.DIRTougleTime = __CTS_Delay * 2;
    //вешаю обработчик
    lnks2_RAM_READ.OnRead = &lnks2_RAM_READ_END;
    ///////////////////////////////////////////////////////

    //заполннение слота Записи RAM f020
    lnks2_RTR_WRITE.OutBuf[0] = 0x01;//device_address;
    lnks2_RTR_WRITE.OutBuf[1] = 0x03;//command_code;
    lnks2_RTR_WRITE.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks2_RTR_WRITE.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks2_RTR_WRITE.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks2_RTR_WRITE.OutBuf[5] = 0x08;//num_reg_Lo; 3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks2_RTR_WRITE.OutBuf[0], 6);
    lnks2_RTR_WRITE.OutBuf[6]=crc.b[1];//high byte CRC
    lnks2_RTR_WRITE.OutBuf[7]=crc.b[0];//low byte CRC
    lnks2_RTR_WRITE.OutCnt  = 8;//кол-во символов в пакете
    lnks2_RTR_WRITE.InptCnt = 0;//пока ничего не приняли
    lnks2_RTR_WRITE._Cnt    = 0;//кол-во переданных символов
    //свойства слота
    lnks2_RTR_WRITE.Status.Stat = 0;
    lnks2_RTR_WRITE.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks2_RTR_WRITE.Status.bits.Skip = 1;//слот выполняется тогда когда нужно
    lnks2_RTR_WRITE.FrameEnd = 120;//_15bits_Delay_115200bps / 40;// / Timer3_PR;
    lnks2_RTR_WRITE.TimeOut  = DevNotResp_TimeOut_1000ms;//DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks2_RTR_WRITE.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks2_RTR_WRITE.OnRead = &lnks2_RTR_WRITE_END;
    ///////////////////////////////////////////////////////

    //заполннение слота чтения FLASH f020
    lnks2_FLASH_READ.OutBuf[0] = 0x01;//device_address;
    lnks2_FLASH_READ.OutBuf[1] = 0x03;//command_code;
    lnks2_FLASH_READ.OutBuf[2] = 0x20;//reg_addr_Hi;
    lnks2_FLASH_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks2_FLASH_READ.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks2_FLASH_READ.OutBuf[5] = 0x70;//num_reg_Lo;
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks2_FLASH_READ.OutBuf[0], 6);
    lnks2_FLASH_READ.OutBuf[6]=crc.b[1];//high byte CRC
    lnks2_FLASH_READ.OutBuf[7]=crc.b[0];//low byte CRC
    lnks2_FLASH_READ.OutCnt =  8;//кол-во символов в пакете
    lnks2_FLASH_READ.InptCnt = 0;//пока ничего не приняли
    lnks2_FLASH_READ._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks2_FLASH_READ.Status.Stat = 0;
    lnks2_FLASH_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks2_FLASH_READ.Status.bits.Skip = 0;//слот выполняется, потом себя закроет
    lnks2_FLASH_READ.FrameEnd = 120;//_15bits_Delay_115200bps / Timer3_PR;
    lnks2_FLASH_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks2_FLASH_READ.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks2_FLASH_READ.OnRead = &lnks2_FLASH_READ_END;
    ///////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////
    //Чтение 1 раз в сек. данных часов и буфера статистики (ALB) из F020
    //заполннение слота чтения RAM f020
    //читаю 0x27 регистров от 0x409F до 0x40С6
    lnks2_ALB.OutBuf[0] = 0x01;//device_address;
    lnks2_ALB.OutBuf[1] = 0x03;//command_code;
    lnks2_ALB.OutBuf[2] = 0x40;//reg_addr_Hi;
    lnks2_ALB.OutBuf[3] = 0x9D;//9F;//reg_addr_Lo;
    lnks2_ALB.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks2_ALB.OutBuf[5] = 0x29;//num_reg_Lo;  0x3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks2_ALB.OutBuf[0], 6);
    lnks2_ALB.OutBuf[6]=crc.b[1];//high byte CRC
    lnks2_ALB.OutBuf[7]=crc.b[0];//low byte CRC
    lnks2_ALB.OutCnt =  8;//кол-во символов в пакете
    lnks2_ALB.InptCnt = 0;//пока ничего не приняли
    lnks2_ALB._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks2_ALB.Status.Stat = 0;
    lnks2_ALB.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks2_ALB.Status.bits.change = 1;//!!! зайдя в него - меняет слот, только для пуска
    lnks2_ALB.Status.bits.Skip = 0;//слот выполняется, потом себя закроет
    lnks2_ALB.FrameEnd = 120;//_15bits_Delay_115200bps / 48;//40;//48;// / Timer3_PR; 32!
    lnks2_ALB.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks2_ALB.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks2_ALB.OnRead = &lnks2_ALB_READ_END;
    ///////////////////////////////////////////////////////

    /////////////////////////////////////////////////
    pLnk2Slots[0] = &lnks2_RAM_READ;  //постоянно чтение RAM
    pLnk2Slots[1] = &lnks2_RTR_WRITE; //ретрансоятор
    pLnk2Slots[2] = &lnks2_FLASH_READ;//чтение FLASH по необходимости
    pLnk2Slots[3] = &lnks2_ALB;       //чтение ALB из F020


    ////////////////////////////////////////////////////////////////
    uart2_init();//инициализация уарта 3 (подключен к F020)
    Timer2_Init();//инициализация таймера
}