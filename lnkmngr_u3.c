#include "lnkmngr.h"
#include "lnkmngr_u3.h"
#include "crc16.h"
#include "bastypes.h"
#include "mbtypes.h"
#include "uart.h"
#include "memutil.h"
#include "ramdata.h"
#include "flash_data.h"
#include "DExS_mem_struct.h"
#include <stdio.h>
#include "uart1rs485.h"
//UIP
#include "tcp_ctrl.h"
char _lnk3_slot_idx = 0;//текущий слот

__no_init TLnkSlot lnks3_RAM_READ   @ "USB_DMA_RAM";
__no_init TLnkSlot lnks3_RTR_DEXS  @ "USB_DMA_RAM";//ретранслятор
__no_init TLnkSlot lnks3_FLASH_READ @ "USB_DMA_RAM";

// новый слот для работы с командой EC 
__no_init TLnkSlot lnks3_EC         @ "USB_DMA_RAM";//чтение буфера событий 

TLnkSlot *pLnk3Slots[lnk3_slots_usage];

void Lnk3Init (void);//инициализация структур сервера
void Lnk3Start (void);//запуск сервера
/////////////////////////////////////////////
//собственно сервер работает распределённо в прерывании... таймера и уарта
void lnks3_RAM_READ_END (void);//получены данные RAM  
void lnks3_RAM_WRITE_END (void);//записаны данные в RAM 
void lnks3_FLASH_READ_END (void);//получены данные FLASH
void lnks3_EC_END (void);//получены данные FLASH 
////////////////////////////////////////////////////////////////////////////////
//функции аппаратного обеспечения сервера
void u3RXuser(Int8U uData);
Boolean u3TXuser(pInt8U pData);
////////////////////////////////////////////////////////////////////////////////
void Lnk3Start (void){//запуск сервера
  //после того как всё инициализировано, начинает отправку данных первого слота
  T3TCR = 1;//TIMER0_START;
  //собственно всё начинается с запуска таймера 3. он сработает через 3байта
  //и вызовет обработчик TIMER3_ISR... вот тут то всё и зашевелится
}
#define MAX_REG_SIZE 125
void Lnk3ReInit()
{
  bauint crc;
  //заменим только часть в запросе
  //для РАМки
  if(DEXS_RAM_SIZE > MAX_REG_SIZE) lnks3_RAM_READ.OutBuf[5] = MAX_REG_SIZE;//см коммент ниже
  else lnks3_RAM_READ.OutBuf[5] = (u8)DEXS_RAM_SIZE;
  //printf("DEXS SIZE %d", lnks3_RAM_READ.OutBuf[5]);
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_RAM_READ.OutBuf[0], 6);
  lnks3_RAM_READ.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_RAM_READ.OutBuf[7]=crc.b[0];//low byte CRC
  
  //для ФЛЕШки
  if(DEXS_FLASH_SIZE > MAX_REG_SIZE) lnks3_FLASH_READ.OutBuf[5] = MAX_REG_SIZE;//см коммент ниже
  else lnks3_FLASH_READ.OutBuf[5] = (u8)DEXS_FLASH_SIZE;
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_FLASH_READ.OutBuf[0], 6);
  lnks3_FLASH_READ.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_FLASH_READ.OutBuf[7]=crc.b[0];//low byte CRC
  
  /*
  MAX_REG_SIZE 125 регистров - 250 байт
    почему такие условия?
    потому что помимо регистров данных, летит еще CRC(2байта),и заголовок(3байта) в пакете
    буфер расчитан на 255 байт, получается данные(250)+заголовок(3)+CRC(2)
  */
  //для RTR  
  if(DEXS_RAM_SIZE > MAX_REG_SIZE) lnks3_RTR_DEXS.OutBuf[5] = MAX_REG_SIZE;//см коммент 
  else lnks3_RTR_DEXS.OutBuf[5] = (u8)DEXS_RAM_SIZE;
  
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_RTR_DEXS.OutBuf[0], 6);
  lnks3_RTR_DEXS.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_RTR_DEXS.OutBuf[7]=crc.b[0];//low byte CRC
  
  
  //printf("RAM_outbuf size = %i \r\nFLASH_outbuf size = %i\r\n",lnks3_RAM_READ.OutBuf[5], lnks3_FLASH_READ.OutBuf[5]);
}
void Lnk3SetTimer (unsigned int Delay){
  T3TCR = 0;//TIMER3 STOP;
  T3IR = 0xff;
  T3TC = 0;
  T3MR0 = Delay ;
  T3TCR = 1;//TIMER3 START;
}

void u3RXuser(Int8U uData){
  TLnkSlot *pSlot = pLnk3Slots[_lnk3_slot_idx];//при входе получаю ссылку на действующий слот
  //поступил какой-то байт от девайса, нужно положить его в InBuf, ++ счётчик
  //остановить таймер тайм аута
  //загрузить в таймер уставку ожидания конца пакета FrameEnd ,взвести флаг WaitFrameEnd
  pSlot->InBuf[pSlot->InptCnt] = uData;//кладу принятый байт в буфер приёмника
  pSlot->InptCnt++;
  pSlot->Status.bits.WaitTimeOut = 0;//больше не жду тайм аута
  pSlot->Status.bits.FrameEnd = 1;//а жду конца пакета
  Lnk3SetTimer(pSlot->FrameEnd);//уставка Фрейм Енда загружается в таймер (тем самым таймер ресетится после каждого байта)
}

Boolean u3TXuser(pInt8U pData){
  TLnkSlot *pSlot = pLnk3Slots[_lnk3_slot_idx];//при входе получаю ссылку на действующий слот
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
        U3IER;//почистить отложенные прерывания
        U3IER = 0;//остановить уарт
        U3RBR;//хрен знает ещё и прочитаю рег данных
        //FIO1CLR |= UART3DIR;//включу драйвер на приём //07.12.10
        pSlot->InptCnt = 0;
        pSlot->Status.bits.DIR_Tougle = 1;//таймер считает время переключения драйвера на приём
        Lnk3SetTimer(pSlot->DIRTougleTime);//уставка переключения загружается в таймер (на ожидание ответа)
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
        Lnk3SetTimer(pSlot->FrameEnd);//через заданное время сработает
        return(false); //передавать больше нечего
      }
    }
}

//Timer2 interrupt handler

__irq __nested __arm  void TIMER3_ISR(void) {
  T3IR = 0xff;//сброс всех флагов прерывания таймера
  T3TCR = 0; //и его остановка
  T3TC  = 0;

  TLnkSlot *pSlot;
  pSlot  = pLnk3Slots[_lnk3_slot_idx];//при входе получаю ссылку на действующий слот

    if (pSlot->Status.bits.change) {
    pSlot->Status.bits.change = 0;
    //нужно сделать активным следующий слот
    _lnk3_slot_idx++;
    if (_lnk3_slot_idx == lnk3_slots_usage) {_lnk3_slot_idx = 0;}
    pSlot = pLnk3Slots[_lnk3_slot_idx];//установлен следующий слот
    //чищу его флаги
    pSlot->Status.bits.WaitTimeOut = 0;
    pSlot->Status.bits.FrameEnd    = 0;
    pSlot->Status.bits.DIR_Tougle  = 0;
    //проверить вдруг он скипуется
    if (pSlot->Status.bits.Skip){
      //пропустить этот слот
      pSlot->Status.bits.change = 1;//если у слота состояние change=1 - значит его нужно переключить
      //LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
      Lnk3SetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
      goto Exit;
    }
    //не скипуется
    //и передать ему управление, но перед этим нужно присвоить ему флаг состояния с которым он должен
    //войти в обработчик слота, как правило это SendBytes, для него нужно обнулить pSlot->_Cnt
    //и передать первый байт
    pSlot->_Cnt = 0;
    //FIO0SET |= UART2DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    U3IER;//почистить отложенные прерывания
    U3IER = 0x02;// разрешить прерывания передатчикa, блокировать приёмник
    if (pSlot == &lnks3_RTR_DEXS) {
        printf(">>> RTR START TX, OutCnt=%d\r\n", pSlot->OutCnt);
    }
    U3THR = pSlot->OutBuf[pSlot->_Cnt++];//
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
    U3IER;//почистить отложенные прерывания
    U3IER = 0;//запретить всё.
    U3RBR;//хрен знает ещё и прочитаю рег данных
    U3IER = 1;//разрешить приём. заблокировать передатчик.
    Lnk3SetTimer(pSlot->TimeOut); //уставка тайм аута загружается в таймер (на ожидание ответа)
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
    U3IER;//почистить отложенные прерывания
    U3IER = 0;//остановить уарт
    U3RBR;//хрен знает ещё и прочитаю рег данных
 //   FIO0SET |= WRK_LED; //LED3 надо погасить светодиод;
    //FIO0SET |= UART2DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    __enable_interrupt(); // handles nested interrupt
    if (pSlot->OnRead){ pSlot->OnRead();}
    pSlot->Status.bits.change = 1;//езначит его нужно переключить слот
    Lnk3SetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
    goto Exit;
  };
  //Если ждал конца пакета FrameEnd, то нужно сбросить флаг WaitRespond (а то подумает что байт принял) и вызвать сервер
  //таймер всегда входит в сервер остановленным, если сервер решит, то запустит
  if (pSlot->Status.bits.FrameEnd){
    U3IER;//почистить отложенные прерывания
    U3RBR;//хрен знает ещё и прочитаю рег данных
    U3IER = 0;//что-то до конца принято, остановить уарт
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
    Lnk3SetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
    goto Exit;
  }
  Exit:
    VICADDRESS = 0;//значит было неопределённое состояние!!!
}

void lnks3_RAM_READ_END (void){//получены данные RAM
 //  printf("lnks3_RAM_READ_END \r\n");
 
   static u16 lnk_cnt = 0;
  lnk_cnt++;
  //printf("RAM READ CNT: %i\r\n",lnk_cnt);
  //RAM_DATA.i[0] = lnks1_RAM_READ.InptCnt;
  //RAM_DATA.i[1] = lnks1_RAM_READ.Status.bits.ErrTimeOut;
  //RAM_DATA.i[2] = lnks1_RAM_READ.Status.bits.CRC_Error;
  lnks3_RAM_READ.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks3_RAM_READ.InBuf[0], lnks3_RAM_READ.InptCnt) != 0);
  if ((lnks3_RAM_READ.Status.bits.ErrTimeOut) || (lnks3_RAM_READ.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тайм аут то не обрабатываюю данные
    return;
  }
 // printf("RAM READ OK: %i\r\n",lnk_cnt);
  //сваплю и копирую в образ памяти RAM 020
  swp_copy_u16( &lnks3_RAM_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &DEXS_RAM_DATA,
                     lnks3_RAM_READ.InBuf[_u_byte_cnt] >> 1);
  
  //каждый 10ый раз чтения рамки вызываем чтение флешки
 //  printf("lnk_cnt = %d",lnk_cnt);
  if (lnk_cnt > 50){
    lnk_cnt = 0;
    lnks3_FLASH_READ.Status.bits.Skip = 0;
  }
}

void lnks3_FLASH_READ_END (void){//получены данные FLASH 

 // printf("lnks3_FLASH_READ_END \r\n");

 
//  if (lnks3_FLASH_READ.Status.bits.ErrTimeOut) {
//    return; //если был тай аут то не обновляю не проверяю данные
//  }
//  if (GetCRC16_WaitZero(&lnks3_FLASH_READ.InBuf[0], lnks3_FLASH_READ.InptCnt)) {
//    //если пакет принят сбойный - сделать повтор чтения
//    return;
//  }
  lnks3_FLASH_READ.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks3_FLASH_READ.InBuf[0], lnks3_FLASH_READ.InptCnt) != 0);
  if ((lnks3_FLASH_READ.Status.bits.ErrTimeOut) || (lnks3_FLASH_READ.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тайм аут то не обрабатываюю данные
    return;
  }
    swp_copy_u16( &lnks3_FLASH_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &DEXS_FLASH_DATA,
                     lnks3_FLASH_READ.InBuf[_u_byte_cnt] >> 1);
  


  //останавливаю слот до следующей записи
  lnks3_FLASH_READ.Status.bits.Skip = 1;

  //ip.b[1] = ipaddr2 = F020_FLASH_Reg.F020[141];//SBR - скорость
  //ip.b[0] = ipaddr3 = F020_FLASH_Reg.F020[140];//SAD - адрес
}



void lnks3_RTR_DEXS_END (void){//ретрансляция пакета завершена, и на него даже получен ответ!
  
 // printf("lnks3_RTR_DEXS_END \r\n");
    printf("RTR_END: called\r\n");


  bauint w;
  static u8 TryCnt = 0;
  //проверка поступивших данных
  lnks3_RTR_DEXS.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks3_RTR_DEXS.InBuf[0], lnks3_RTR_DEXS.InptCnt) != 0);
  //от DexS должно прийти подтверждение записи данных
  printf("RTR_END: ErrTimeOut=%d, CRC_Error=%d, InptCnt=%d, InBuf[0..3]=%02X %02X %02X %02X\r\n",
       lnks3_RTR_DEXS.Status.bits.ErrTimeOut,
       lnks3_RTR_DEXS.Status.bits.CRC_Error,
       lnks3_RTR_DEXS.InptCnt,
       lnks3_RTR_DEXS.InBuf[0], lnks3_RTR_DEXS.InBuf[1],
       lnks3_RTR_DEXS.InBuf[2], lnks3_RTR_DEXS.InBuf[3]);
  if ((lnks3_RTR_DEXS.Status.bits.ErrTimeOut) || (lnks3_RTR_DEXS.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тай аут то
    //перезапускаю слот на повторно выполнение перевыполнение, но не более 3-х попыток
    if (TryCnt == 0) {
      TryCnt = 3;
      lnks3_RTR_DEXS.Status.bits.Skip = 1;
    }
    TryCnt--;
    lnks3_RTR_DEXS._Cnt = 0;//чтобы начать передачу сначала
    return;
  }

  //если подтверждение получено, то останавливаю слот до следующей записи
  lnks3_RTR_DEXS.Status.bits.Skip = 1;
  TryCnt = 3;
  //ретранслировать обратно ответ устройства, если требуется
  if (lnks3_RTR_DEXS.Status.bits.WaitRespond) {
    lnks3_RTR_DEXS.Status.bits.WaitRespond = 0;//отправляю один раз
    //в ответ нужно "подмешать" адрес интерфейса от которого пришёл запрос (у меня DEXS -  всегда "1",
    //а интрефейсы могут быть от 1 до 255)
    lnks3_RTR_DEXS.InBuf[0] = lnks3_RTR_DEXS.pC->DevAddr;//Buffer[0];
    //теперь вычисляю новую контрольную сумму
    w.i = GetCRC16(&lnks3_RTR_DEXS.InBuf[0], (lnks3_RTR_DEXS.InptCnt - 2));//подсчет CRC буфера кроме уже имеющейся CRC
    //подставляю новую CRC
    lnks3_RTR_DEXS.InBuf[lnks3_RTR_DEXS.InptCnt - 2] = w.b[1];//high byte CRC
    lnks3_RTR_DEXS.InBuf[lnks3_RTR_DEXS.InptCnt - 1] = w.b[0];//low byte CRC
    //копирую полученный слотом данные в буфер передатчика интерфейса
    u8_mem_cpy(&lnks3_RTR_DEXS.InBuf[0], &lnks3_RTR_DEXS.pC->Buffer[0], lnks3_RTR_DEXS.InptCnt);

    //RAM_DATA.i[2] = lnks_RTR_WRITE.InptCnt;
    //u8_mem_cpy(&lnks_RTR_WRITE.InBuf[0], (u8 *)&RAM_DATA.i[3], (u8) (lnks_RTR_WRITE.InptCnt & 0x0000007F));

    //сообщаю сколько байт нужно передать (в общем готовлю к передаче)
    lnks3_RTR_DEXS.pC->TXCount = lnks3_RTR_DEXS.InptCnt;
    lnks3_RTR_DEXS.pC->Idx = 0;
    lnks3_RTR_DEXS.pC->OnCallBackReceive();
    //lnks_RTR_WRITE.pC->OnCallBack();
  }
}


  
   ////////////////////////////////////////////////////////////////////

void lnks3_EC_END (void){//получены данные по команде EC
  
  //printf("lnks3_EC_END \r\n");

  
  lnks3_EC.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks3_EC.InBuf[0], lnks3_EC.InptCnt) != 0);
  if ((lnks3_EC.Status.bits.ErrTimeOut) || (lnks3_EC.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тайм аут то не обрабатываюю данные
    return;
  }
  /* 0x6c = 0xFF(0xEC + 0x80(error code))
  that means DExS unknown command
  */
  if(lnks3_EC.InBuf[1] == 0x6C)return;


  //брать 6+2 байт из ответа с адреса 3
  //ложить во временную переменную
  u16 InBuff[4];  
  swp_copy_u16( &lnks3_EC.InBuf[2],  (u16 *) &InBuff,4);
  //  FLASH_DATA.Ethernet_IPaddr = 1;
  //сравниваем уставки связи по RS485
  //printf("%d %d %d",InBuff[0],InBuff[1],InBuff[2]);
//  printf("Command 0xEC detected, new settings is: \r\n");
    bauint w;
  if(RAM_DATA.uart1SET != InBuff[0]){
    
    //if(FLASH_DATA.RS485_Settings != InBuff[0]){//если изменились уставки связи по 485
    
    w.i = InBuff[0];
    printf("uart1 BR: %i \r\n",w.b[0]);
    printf("uart1 Addr: %i  \r\n",w.b[1]);
    uart1rs485_Reinit (w.b[0],                       w.b[1]);//SAAddr    
    
    RAM_DATA.uart1SET = InBuff[0];
  }
  
// //сравниваем уставки связи TCP
  baulong ip;

  ip.a[0] = (InBuff[1] >> 8) | ((InBuff[1] & 0xFF)<<8);
  ip.a[1] = (InBuff[2] >> 8) | ((InBuff[2] & 0xFF)<<8);
  
  if(RAM_DATA.IP_ADDR != ip.i){//если изменились уставки связи по TCP
    printf("IP_ADDR %i.%i.%i.%i\r\n",ip.b[3], ip.b[2], ip.b[1], ip.b[0]);
    RAM_DATA.IP_ADDR = ip.i; //полученный IP
    tcp_reinit();//перезапуск инициализации TCP/IP
  }
  //сравниваем размер флешки и рамки для чтения
   w.i = InBuff[3]; 
  if((w.b[0] != RAM_DATA.FLASH_SIZE) || (w.b[1] != RAM_DATA.RAM_SIZE))
  {
    printf("ReSize settings detected: \r\n");
    printf("RAM size = %i \r\nFLASH size = %i\r\n",w.b[1], w.b[0]);
    RAM_DATA.FLASH_SIZE = w.b[0];
    RAM_DATA.RAM_SIZE = w.b[1];

    Lnk3ReInit();
  }
  /*
  //тут я должен сравнить настройки связи порта (скорость и адрес)
  //если они отличаются в LPC и DEXS, то сделать реинициализацию RS485_1 интерфейса (uart2rs485)
  //void uart2rs485_Reinit (u8 BPS, u8 DevAddr){
  //настройка связи порта 2
  bauint w;
  w.i = InBuff[0];
  printf("ADDR.BPS %i.%i\r\n",w.b[0],w.b[1]);
  /////////////////////////////////////////////////////  
  uart2rs485_Reinit (w.b[1],//SBR
                     w.b[0]);//SAAddr
  
  //ага, теперь треба и IP адрес он-лайн поменять
  //желательно не менять его агульно, т.к. это длительый процесс переинициализации
  //а сравнить текущий 	IP с параметрами во флэшке, и если они не отличаются то ничего не делать
  //а если отличаются то делать
  //uip_ipaddr_t ipaddr;
  baulong ip;
  ip.a[0] = InBuff[1];
  ip.a[1] = InBuff[2];

  RAM_DATA.mIP_ADDR = ip.i; //полученный IP
  printf("IPADDR %i.%i.%i.%i\r\n",ip.b[3], ip.b[2], ip.b[1], ip.b[0]);
  tcp_reinit();//перезапуск инициализации TCP/IP
  
  */
  
  u8 nmb = 0;
  
  while(nmb <3)
  {
  //printf("InBuff : %i \r\n",InBuff[nmb]);
        nmb++; 
  }
  // скипать слот,чтобы читать по запросу
 // lnks3_EC.Status.bits.Skip = 1;//ждать следующей активации
 

}



#define Timer3_PR 168//prescaler на сколько делится частота питающая Т3
//но и все остальные уставки нужно поделить на 168
//если не делить PR=0
//то (1/22118400    )*65535 =  0,002.9  ~0,03 сек (а надо до 0,5 довести -> в 168 раз частота должна быть меньше)
//   (1/22118400/168)*65535 = 0,497.77 ~0,50 сек
#define DevNotResp_TimeOut_500ms  0x8000 //0,5 sec
#define DevNotResp_TimeOut_1000ms 0xF000 //1 sec

void Timer3_Init(void)
{
  PCONP_bit.PCTIM3 = 1;
  T3IR = 0xff;
  T3TCR = 0; //Stop timer
  T3TC	= 0; //
  T3PR	= Timer3_PR; //
  T3PC	= 0; //Prescaler is set to no division.
  T3CCR = 0; //Capture is disabled
  T3EMR = 0; //No external match output
  T3MR0 = _15bits_Delay_115200bps / Timer3_PR; //Count up to this value
  T3MCR = 3; // Reset/Stop/interrupt on MR0 (match register 0).
  //установка обработчика прерываний
  _VIC_SetVectoredIRQ((u32)&TIMER3_ISR,3,VIC_TIMER3);
  VICINTENABLE |= 1<< VIC_TIMER3;
  //первым сработает тайме и запустит всю конструкцию в работу
}

void uart3_init (void){
  //сет хардваре
  UartInit(UART_3,0,NORM);
  UartSetLineCoding(UART_3,UartLineCoding_115200);
  U3IER = 0x01;//заблокировать передатчик. разрешить приём.
  //сет фирмваре
  u3RXproc = u3RXuser;
  u3TXproc = u3TXuser;
}
//
//void onRAM3_READ(void){};
//void onRAM3_WRITE(void){};
//void onFLASH3_READ(void){};

#define __CTS_Delay 1

void Lnk3HardwareInit()
{
  uart3_init();//инициализация уарта 3 (подключен к DEXS)
  Timer3_Init();//инициализация таймера
}
void Lnk3Init (void){//инициализация структур сервера
  bauint crc;
  
  lnks3_RAM_READ.Status.bits.ErrTimeOut = 1;
  _lnk3_slot_idx = 0;//текущий слот lnks_RAM_READ
  
  //заполннение слота чтения RAM 
  lnks3_RAM_READ.OutBuf[0] = 0x01;//device_address;
  lnks3_RAM_READ.OutBuf[1] = 0x03;//command_code;
  lnks3_RAM_READ.OutBuf[2] = 0x00;//reg_addr_Hi;
  lnks3_RAM_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
  lnks3_RAM_READ.OutBuf[4] = 0x00;//num_reg_Hi;
  lnks3_RAM_READ.OutBuf[5] = DEXS_RAM_SIZE;//0x28;//1D;//0x3F;//num_reg_Lo;  0x3F
  printf("DEXS SIZE %d\r\n", lnks3_RAM_READ.OutBuf[5]);
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_RAM_READ.OutBuf[0], 6);
  lnks3_RAM_READ.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_RAM_READ.OutBuf[7]=crc.b[0];//low byte CRC
  lnks3_RAM_READ.OutCnt =  8;//кол-во символов в пакете
  lnks3_RAM_READ.InptCnt = 0;//пока ничего не приняли
  lnks3_RAM_READ._Cnt =    0;//кол-во переданных символов
  //свойства слота
  lnks3_RAM_READ.Status.Stat = 0;
  lnks3_RAM_READ.Status.bits.Skip = 0;// используем всегда, тянем всю рамку
  lnks3_RAM_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
  lnks3_RAM_READ.Status.bits.change = 1;//!!! зайдя в него - меняет слот, только для пуска
  lnks3_RAM_READ.FrameEnd = 120;//_15bits_Delay_115200bps / 40;//48;// / Timer3_PR; 32!
  //почему 120? это 1,8мс (1800мкс) = быстрее F020 отвечать ну просто не может!
  lnks3_RAM_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
  lnks3_RAM_READ.DIRTougleTime = __CTS_Delay;// * 2;
  //вешаю обработчик
  lnks3_RAM_READ.OnRead = &lnks3_RAM_READ_END;
  ///////////////////////////////////////////////////////
  
  
  //заполннение слота чтения FLASH
  lnks3_FLASH_READ.OutBuf[0] = 0x01;//device_address;
  lnks3_FLASH_READ.OutBuf[1] = 0x03;//command_code;
  lnks3_FLASH_READ.OutBuf[2] = 0x20;//reg_addr_Hi;
  lnks3_FLASH_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
  lnks3_FLASH_READ.OutBuf[4] = 0x00;//num_reg_Hi;
  lnks3_FLASH_READ.OutBuf[5] = DEXS_FLASH_SIZE;//num_reg_Lo;
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_FLASH_READ.OutBuf[0], 6);
  lnks3_FLASH_READ.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_FLASH_READ.OutBuf[7]=crc.b[0];//low byte CRC
  lnks3_FLASH_READ.OutCnt =  8;//кол-во символов в пакете
  lnks3_FLASH_READ.InptCnt = 0;//пока ничего не приняли
  lnks3_FLASH_READ._Cnt =    0;//кол-во переданных символов
  //свойства слота
  lnks3_FLASH_READ.Status.Stat = 0;
  lnks3_FLASH_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
  lnks3_FLASH_READ.Status.bits.Skip = 0;//выполнится и сам себя закроет
  lnks3_FLASH_READ.FrameEnd = 120;//_15bits_Delay_115200bps / Timer3_PR;
  lnks3_FLASH_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
  lnks3_FLASH_READ.DIRTougleTime = __CTS_Delay;
  //вешаю обработчик
  lnks3_FLASH_READ.OnRead = &lnks3_FLASH_READ_END;
  ///////////////////////////////////////////////////////
  
  
  //заполннение слота ретранслятора
  lnks3_RTR_DEXS.OutBuf[0] = 0x01;//device_address;
  lnks3_RTR_DEXS.OutBuf[1] = 0x03;//command_code;
  lnks3_RTR_DEXS.OutBuf[2] = 0x00;//reg_addr_Hi;
  lnks3_RTR_DEXS.OutBuf[3] = 0x00;//reg_addr_Lo;
  lnks3_RTR_DEXS.OutBuf[4] = 0x00;//num_reg_Hi;
  lnks3_RTR_DEXS.OutBuf[5] = DEXS_RAM_SIZE;//num_reg_Lo; 3F
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_RTR_DEXS.OutBuf[0], 6);
  lnks3_RTR_DEXS.OutBuf[6]=crc.b[1];//high byte CRC
  lnks3_RTR_DEXS.OutBuf[7]=crc.b[0];//low byte CRC
  lnks3_RTR_DEXS.OutCnt  = 8;//кол-во символов в пакете
  lnks3_RTR_DEXS.InptCnt = 0;//пока ничего не приняли
  lnks3_RTR_DEXS._Cnt    = 0;//кол-во переданных символов
  //свойства слота
  lnks3_RTR_DEXS.Status.Stat = 0;
  lnks3_RTR_DEXS.Status.bits.NoRespond = 0;//0-ожидает ответа
  lnks3_RTR_DEXS.Status.bits.Skip = 1;//слот выполняется тогда когда нужно
  lnks3_RTR_DEXS.FrameEnd = 120;//_15bits_Delay_115200bps / 40;// / Timer3_PR;
  lnks3_RTR_DEXS.TimeOut  = DevNotResp_TimeOut_1000ms;//DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
  lnks3_RTR_DEXS.DIRTougleTime = __CTS_Delay;
  //вешаю обработчик
  lnks3_RTR_DEXS.OnRead = &lnks3_RTR_DEXS_END;
  ///////////////////////////////////////////////////////
  

  
  //команда для запроса настроек связи внешних интерфейсов
  
  
  lnks3_EC.OutBuf[0] = 0x01;//device_address;
  lnks3_EC.OutBuf[1] = 0xEC;//command_code;
  //теперь считаем КС отправляемого пакета
  crc.i = GetCRC16(&lnks3_EC.OutBuf[0], 2);
  lnks3_EC.OutBuf[2]=crc.b[1];//high byte CRC
  lnks3_EC.OutBuf[3]=crc.b[0];//low byte CRC
  lnks3_EC.OutCnt =  4;//кол-во символов в пакете
  lnks3_EC.InptCnt = 0;//пока ничего не приняли
  lnks3_EC._Cnt =    0;//кол-во переданных символов
  //свойства слота
  lnks3_EC.Status.Stat = 0;
  lnks3_EC.Status.bits.NoRespond = 0;//0-ожидает ответа
  lnks3_EC.Status.bits.Skip = 0;//1;//не выполнять
  lnks3_EC.FrameEnd = 120;//_15bits_Delay_115200bps / 40;//48;// / Timer3_PR; 32!
  //почему 120? это 1,8мс (1800мкс) = быстрее F020 отвечать ну просто не может!
  lnks3_EC.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
  lnks3_EC.DIRTougleTime = __CTS_Delay;// * 2;
  //вешаю обработчик
  lnks3_EC.OnRead = &lnks3_EC_END;
  
  /////////////////////////////////////////////////
  pLnk3Slots[0] = &lnks3_RAM_READ;  //постоянно чтение RAM
  pLnk3Slots[1] = &lnks3_RTR_DEXS; //ретранслятор
  pLnk3Slots[2] = &lnks3_FLASH_READ;//чтение FLASH по необходимости    
  pLnk3Slots[3] = &lnks3_EC;        //
  
  
  ////////////////////////////////////////////////////////////////
  Lnk3HardwareInit();
  
}