/*
Из доки на МБВ
//и так до 1023 (0х3FF)
//далее следуют сегменты (всего 31) по 1024 байта содержащие запись событий

//для удобства счёта, делаю ёмкость одного сегмента 0x3FF (1024 байта)
//внури сегмент поделен на 16 записей по 64 байта
//формат записи:
//7  байт  - время
//55 байт  - дамп RAM памяти от 0х22-0х59
//2  байта - флаги (и вообще всякие признаки)
//запись в сегмент происходит каждую секунду,

Требуется читать из МБВ данные регистратора событий.
Сейчас читаем данные ALB а там время и статистика.
Нужно организовать свобобный вычит данных из ФРАМ айСиЭм

1) Юзер делает запрос на чтение регистров от  A000 до A1F0
   (всего 496 записей по 64 байта)
   с запросом 64 байт данных (32 регистра)
2) В ответ, получает 32 регистра в формате:
      0-6 дата/время
        reg0: 0-секунд
              1-минут
        reg1: 2-часов.
              3-***.
        reg2: 4-число
              5-месяц
        reg3: 6-год
              х-***.
        reg4-reg30
      7-61:
        55 байт дамп памяти МБВ (секция RAM)
      62-63 флаги и другие признаки
        reg31: 0-тип события
                  bit0: 'ОТКЛ';    //выключение двигателя
                  bit1: 'ABAР';    //сработала авария
                  bit2: 'CPU ON';  //включение питания контроллера
                  bit3: 'CPU OFF'; //выключение питания контроллера
                  bit4: 'ВКЛ ';    //включение двигателя
              1-аварии
                  bit0: 'МАКС. ТОК';
                  bit1: 'АСИНХРОННЫЙ ХОД';
                  bit2: 'ПРОБОЙ ИЗОЛЯЦИИ';
                  bit3: 'ОБРЫВ ПОЛЯ';
                  bit4: 'НЕИСПРАВНОСТЬ БК ВЫКЛЮЧАТЕЛЯ';
                  bit5: 'ГЕНЕРАТОРНЫЙ РЕЖИМ';
                  bit6: 'Аварийный стоп (кнопка)';
                  bit7: 'Технологическая авария';


Порядок действий:
1) Юзер делает запрос на чтение регистров от  A000 до A1F0
   (всего 496 записей по 64 байта)
   с запросом 10 байт данных (5 регистров)
2) В ответ, получает 5 регистров в формате:
      0-6 дата/время
        reg0: 0-секунд
              1-минут
        reg1: 2-часов.
              3-***.
        reg2: 4-число
              5-месяц
        reg3: 6-год
              х-***.
      7-8 флаги и другие признаки
        reg4: 0-тип события
                  bit0: 'ОТКЛ';    //выключение двигателя
                  bit1: 'ABAР';    //сработала авария
                  bit2: 'CPU ON';  //включение питания контроллера
                  bit3: 'CPU OFF'; //выключение питания контроллера
                  bit4: 'ВКЛ ';    //включение двигателя
              1-аварии
                  bit0: 'МАКС. ТОК';
                  bit1: 'АСИНХРОННЫЙ ХОД';
                  bit2: 'ПРОБОЙ ИЗОЛЯЦИИ';
                  bit3: 'ОБРЫВ ПОЛЯ';
                  bit4: 'НЕИСПРАВНОСТЬ БК ВЫКЛЮЧАТЕЛЯ';
                  bit5: 'ГЕНЕРАТОРНЫЙ РЕЖИМ';
                  bit6: 'Аварийный стоп (кнопка)';
                  bit7: 'Технологическая авария';
3) Записи читаются о 0 до 1F0, при этом юзеру показывать тольк с флагами "ВКЛ"
   "ОТКЛ" и содержащие аварии

Считывать сектора с осцилограммами
Формула простая:
Адрес блока данных:=1024+SectorNumber*1024+RecNumber*64;
SectorNumber - 0..31 кол-во секторов по 1024б с данными регистратора событий
RecNumber кол-во 
Вычиляю адрес начала блока данных, передаю его МБВ он возвращает 128 байт данных
соответсвующих этому номеру


*/
#include "lnkmngr.h"
#include "crc16.h"
#include "bastypes.h"
#include "mbtypes.h"
#include "uart.h"
#include "memutil.h"
//#include "qrc.h"
//#include "uart2rs485.h" //приходится нарушать стиль
char _lnk_slot_idx = 0;//текущий слот

__no_init TLnkSlot lnks_RAM_READ   @ "EMAC_DMA_RAM";
__no_init TLnkSlot lnks_RTR_WRITE  @ "EMAC_DMA_RAM";//ретранслятор
__no_init TLnkSlot lnks_FLASH_READ @ "EMAC_DMA_RAM";
//TLnkSlot lnks_FLASH_WRITE;
__no_init TLnkSlot lnks_USER       @ "EMAC_DMA_RAM";
__no_init TLnkSlot lnks_ALB        @ "EMAC_DMA_RAM";//чтение буфера событий F020

TLnkSlot *pLnkSlots[lnk_slots_usage];

void LnkInit (void);//инициализация структур сервера
void LnkStart (void);//запуск сервера
/////////////////////////////////////////////
//собственно сервер работает распределённо в прерывании... таймера и уарта
void lnks_RAM_READ_END (void);//получены данные RAM от F020
void lnks_RAM_WRITE_END (void);//записаны данные в RAM от F020
void lnks_FLASH_READ_END (void);//получены данные FLASH от F020
void lnks_USER_WRITE_END (void);//данные Юзера переданы регулятору
////////////////////////////////////////////////////////////////////////////////
//функции аппаратного обеспечения сервера
//void TIMER3_ISR(void);
void u3RXuser(Int8U uData);
Boolean u3TXuser(pInt8U pData);
////////////////////////////////////////////////////////////////////////////////
void LnkStart (void){//запуск сервера
  //после того как всё инициализировано, начинает отправку данных первого слота
  T3TCR = 1;//TIMER0_START;
  //собственно всё начинается с запуска таймера 3. он сработает через 3байта
  //и вызовет обработчик TIMER3_ISR... вот тут то всё и зашевелится
}

void LnkSetTimer (unsigned int Delay){
  T3TCR = 0;//TIMER3 STOP;
  T3IR = 0xff;
  T3TC = 0;
  T3MR0 = Delay ;
  T3TCR = 1;//TIMER3 START;
}

void u3RXuser(Int8U uData){
  TLnkSlot *pSlot = pLnkSlots[_lnk_slot_idx];//при входе получаю ссылку на действующий слот
  //поступил какой-то байт от девайса, нужно положить его в InBuf, ++ счётчик
  //остановить таймер тайм аута
  //загрузить в таймер уставку ожидания конца пакета FrameEnd ,взвести флаг WaitFrameEnd
  pSlot->InBuf[pSlot->InptCnt] = uData;//кладу принятый байт в буфер приёмника
  pSlot->InptCnt++;
  pSlot->Status.bits.WaitTimeOut = 0;//больше не жду тайм аута
  pSlot->Status.bits.FrameEnd = 1;//а жду конца пакета
  LnkSetTimer(pSlot->FrameEnd);//уставка Фрейм Енда загружается в таймер (тем самым таймер ресетится после каждого байта)
}

Boolean u3TXuser(pInt8U pData){
  TLnkSlot *pSlot = pLnkSlots[_lnk_slot_idx];//при входе получаю ссылку на действующий слот
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
      FIO2SET |= LED3; //LED3 пакет передан, надо погасить светодиод;
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
        LnkSetTimer(pSlot->DIRTougleTime);//уставка переключения загружается в таймер (на ожидание ответа)
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
        pSlot->Status.bits.change = 1;//если у слота состояние 0 - значит его нужно переключить
        FIO1SET |= UART3DIR;//включу драйвер на передачу //10.12.10
        LnkSetTimer(pSlot->FrameEnd);//через заданное время сработает
        return(false); //передавать больше нечего
      }
    }
}

//Timer3 interrupt handler

__irq __nested __arm  void TIMER3_ISR(void) {
  T3IR = 0xff;//сброс всех флагов прерывания таймера
  T3TCR = 0; //и его остановка
  T3TC  = 0;

  TLnkSlot *pSlot;
  pSlot  = pLnkSlots[_lnk_slot_idx];//при входе получаю ссылку на действующий слот

  if (pSlot->Status.bits.change) {
    pSlot->Status.bits.change = 0;
    //нужно сделать активным следующий слот
    _lnk_slot_idx++;
    if (_lnk_slot_idx == lnk_slots_usage) {_lnk_slot_idx = 0;}
    pSlot = pLnkSlots[_lnk_slot_idx];//установлен следующий слот
    //чищу его флаги
    pSlot->Status.bits.WaitTimeOut = 0;
    pSlot->Status.bits.FrameEnd    = 0;
    pSlot->Status.bits.DIR_Tougle  = 0;
    //проверить вдруг он скипуется
    if (pSlot->Status.bits.Skip){
      //пропустить этот слот
      pSlot->Status.bits.change = 1;//если у слота состояние change=1 - значит его нужно переключить
      //LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
      LnkSetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
      goto Exit;
    }
    //не скипуется
    //и передать ему управление, но перед этим нужно присвоить ему флаг состояния с которым он должен
    //войти в обработчик слота, как правило это SendBytes, для него нужно обнулить pSlot->_Cnt
    //и передать первый байт
    pSlot->_Cnt = 0;
    FIO1SET |= UART3DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    U3IER;//почистить отложенные прерывания
    U3IER = 0x02;// разрешить прерывания передатчикa, блокировать приёмник
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
    FIO1CLR |= UART3DIR;//включу драйвер на приём //07.12.10
    pSlot->Status.bits.DIR_Tougle = 0;//
    pSlot->Status.bits.WaitTimeOut = 1;//теперь жду тайм аута
    pSlot->InptCnt = 0;
    U3IER;//почистить отложенные прерывания
    U3IER = 0;//запретить всё.
    U3RBR;//хрен знает ещё и прочитаю рег данных
    U3IER = 1;//разрешить приём. заблокировать передатчик.
    LnkSetTimer(pSlot->TimeOut); //уставка тайм аута загружается в таймер (на ожидание ответа)
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
    FIO2SET |= LED3; //LED3 надо погасить светодиод;
    FIO1SET |= UART3DIR;//DIR = 1 включу драйвер на передачу //07.12.10 ???
    __enable_interrupt(); // handles nested interrupt
    if (pSlot->OnRead){ pSlot->OnRead();}
    pSlot->Status.bits.change = 1;//езначит его нужно переключить слот
    LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
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

    FIO2CLR |= LED3; //LED3 принят пакет, надо зажечь светодиод
    FIO1SET |= UART3DIR;//DIR = 1 включу драйвер на передачу //10.12.10 ???
    __enable_interrupt(); // handles nested interrupt
    if (pSlot->OnRead){ pSlot->OnRead();}
    pSlot->Status.bits.change = 1;//переключить слот
    //LnkSetTimer(pSlot->FrameEnd); //через заданное время сработает и будет в работе новый слот
    LnkSetTimer(pSlot->DIRTougleTime); //через заданное время сработает и будет в работе новый слот
    goto Exit;
  }
  Exit:
    VICADDRESS = 0;//значит было неопределённое состояние!!!
}

void lnks_RAM_READ_END (void){//получены данные RAM от F020
  static u8 ALB_READ_START = 32;//каждые 32 чтения RAM будет вызывться чтение ALB
  RAM_DATA.i[0] = lnks_RAM_READ.InptCnt;
  RAM_DATA.i[1] = lnks_RAM_READ.Status.bits.ErrTimeOut;
  RAM_DATA.i[2] = lnks_RAM_READ.Status.bits.CRC_Error;
  lnks_RAM_READ.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks_RAM_READ.InBuf[0], lnks_RAM_READ.InptCnt) != 0);
  if ((lnks_RAM_READ.Status.bits.ErrTimeOut) || (lnks_RAM_READ.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тайм аут то не обрабатываюю данные
    return;
  }
  RAM_DATA.w2++;
  //сваплю и копирую в образ памяти RAM 020
  swp_copy_u16( &lnks_RAM_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &RAM_DATA.F020[0],
                     lnks_RAM_READ.InBuf[_u_byte_cnt] >> 1);
  //RAM_DATA.i[3]++;
  //проверю, установлен ли FLASH_WRITED //для внешней системы даю знать что флэш прочитан
  //FLASH_WRITED bit     reg_0001L.2;
  if (RAM_DATA.F020[2] & 0x04){
    //да, есть признак того что, данные во флэшке изменены, и требуется обновить данные в буфере LPC
    //для этого запускаю слот чтения флэша (вернее разрешаю переключится на него)
    lnks_FLASH_READ._Cnt =    0;//кол-во переданных символов
    lnks_FLASH_READ.Status.bits.Skip = 0;
  }
  //проверка активации слота чтения ALB
  if (ALB_READ_START-- == 0) {
    ALB_READ_START = 32;
    //запускаю слот чтения ALB (вернее разрешаю переключится на него)
    lnks_ALB._Cnt =    0;//кол-во переданных символов
    lnks_ALB.Status.bits.Skip = 0;
  }
}

void lnks_RTR_WRITE_END (void){//ретрансляция пакета завершена, и на него даже получен ответ!
  bauint w;
  static u8 TryCnt = 0;
  //проверка поступивших данных
  lnks_RTR_WRITE.Status.bits.CRC_Error = (GetCRC16_WaitZero(&lnks_RTR_WRITE.InBuf[0], lnks_RTR_WRITE.InptCnt) != 0);
  //от 020 должно прийти подтверждение записи данных
  if ((lnks_RTR_WRITE.Status.bits.ErrTimeOut) || (lnks_RTR_WRITE.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тай аут то
    //перезапускаю слот на повторно выполнение перевыполнение, но не более 3-х попыток
    if (TryCnt == 0) {
      TryCnt = 3;
      lnks_RTR_WRITE.Status.bits.Skip = 1;
    }
    TryCnt--;
    lnks_RTR_WRITE._Cnt = 0;//чтобы начать передачу сначала
    return;
  }

  //если подтверждение получено, то останавливаю слот до следующей записи
  lnks_RTR_WRITE.Status.bits.Skip = 1;
  TryCnt = 3;
  //ретранслировать обратно ответ устройства, если требуется
  if (lnks_RTR_WRITE.Status.bits.WaitRespond) {
    lnks_RTR_WRITE.Status.bits.WaitRespond = 0;//отправляю один раз
    //в ответ нужно "подмешать" адрес интерфейса от которого пришёл запрос (у меня F020 -  всегда "1",
    //а интрефейсы могут быть от 1 до 255)
    lnks_RTR_WRITE.InBuf[0] = lnks_RTR_WRITE.pC->DevAddr;//Buffer[0];
    //теперь вычисляю новую контрольную сумму
    w.i = GetCRC16(&lnks_RTR_WRITE.InBuf[0], (lnks_RTR_WRITE.InptCnt - 2));//подсчет CRC буфера кроме уже имеющейся CRC
    //подставляю новую CRC
    lnks_RTR_WRITE.InBuf[lnks_RTR_WRITE.InptCnt - 2] = w.b[1];//high byte CRC
    lnks_RTR_WRITE.InBuf[lnks_RTR_WRITE.InptCnt - 1] = w.b[0];//low byte CRC
    //копирую полученный слотом данные в буфер передатчика интерфейса
    u8_mem_cpy(&lnks_RTR_WRITE.InBuf[0], &lnks_RTR_WRITE.pC->Buffer[0], lnks_RTR_WRITE.InptCnt);

    //RAM_DATA.i[2] = lnks_RTR_WRITE.InptCnt;
    //u8_mem_cpy(&lnks_RTR_WRITE.InBuf[0], (u8 *)&RAM_DATA.i[3], (u8) (lnks_RTR_WRITE.InptCnt & 0x0000007F));

    //сообщаю сколько байт нужно передать (в общем готовлю к передаче)
    lnks_RTR_WRITE.pC->TXCount = lnks_RTR_WRITE.InptCnt;
    lnks_RTR_WRITE.pC->Idx = 0;
    lnks_RTR_WRITE.pC->OnCallBackReceive();
    //lnks_RTR_WRITE.pC->OnCallBack();
  }
}


void lnks_FLASH_READ_END (void){//получены данные FLASH от F020
  //bauint w;

  //RAM_DATA.i[6]++;
  if (lnks_FLASH_READ.Status.bits.ErrTimeOut) {
    return; //если был тай аут то не обновляю не проверяю данные
  }
  if (GetCRC16_WaitZero(&lnks_FLASH_READ.InBuf[0], lnks_FLASH_READ.InptCnt)) {
    //если пакет принят сбойный - сделать повтор чтения
    return;
  }
    swp_copy_u16( &lnks_FLASH_READ.InBuf[_u_data_section_cm03],
                  (u16 *) &F020_FLASH_DATA.F020[0],
                     lnks_FLASH_READ.InBuf[_u_byte_cnt] >> 1);
  //тут я должен сравнить настройки связи порта (скорость и адрес)
  //если они отличаются в LPC и 020, то сделать реинициализацию RS485_1 интерфейса (uart2rs485)
  //uart2rs485_Reinit (F020_FLASH_DATA[29],//BPS
  //                   F020_FLASH_DATA[28] //DevAddr
  //                   );
  //mem_cpy (&RAM_DATA.i[0], &F020_FLASH_DATA[0], 32);
  //останавливаю слот до следующей записи
  lnks_FLASH_READ.Status.bits.Skip = 1;
}

void lnks_ALB_READ_END (void){//получены данные ALB от F020
  if (lnks_ALB.Status.bits.ErrTimeOut) {
    return; //если был тайм аут то не обновляю не проверяю данные
  }
  if (GetCRC16_WaitZero(&lnks_ALB.InBuf[0], lnks_ALB.InptCnt)) {
    //если пакет принят сбойный - сделать повтор чтения
    return;
  }
   swp_copy_u16( &lnks_ALB.InBuf[_u_data_section_cm03],
                  (u16 *) &ALB_DATA,
                     lnks_ALB.InBuf[_u_byte_cnt] >> 1);
  //u8_mem_cpy((u8 *)&ALB, (u8 *)&RAM_DATA.i[0], 32);
  lnks_ALB.Status.bits.Skip = 1;//жду следующей активации
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
  _VIC_SetVectoredIRQ((u32)&TIMER3_ISR,2,VIC_TIMER3);
  VICINTENABLE |= 1<< VIC_TIMER3;
  //первым сработает тайме и запустит всю конструкцию в работу
}

void uart3_init (void){
  //сет хардваре
  UartInit(UART_3,0,NORM);
  UartSetLineCoding(UART_3,UartLineCoding_115200);
  U3IER = 0x01;//заблокировать передатчик. разрешить приём.
  //PINSEL0  &= 0xFFFFFFF0;
  //PINSEL0  |= (0x0A << 0) ; //TX3->P0.0, RX3->P0.1
  PINSEL9 |= (0x0F << 24);//TX3->P4.28, RX3->P4.29
  //сет фирмваре
  u3RXproc = u3RXuser;
  u3TXproc = u3TXuser;
   //DIR на приём
  FIO1DIR |= UART3DIR;//P1.17 - выход
  FIO1CLR |= UART3DIR;//направление "приём" DIR3 P1.17
   //DIR на передачу. Задача слота передавать пакеты!
  FIO1SET |= UART3DIR;//DIR = 1 включу драйвер на передачу //07.12.10
  FIO2SET |= LED3; //LED3 погасить светодиод;
}

void onRAM_READ(void){};
void onRAM_WRITE(void){};
void onFLASH_READ(void){};

#define __CTS_Delay 1

void LnkInit (void){//инициализация структур сервера
  bauint crc;
    _lnk_slot_idx = 0;//текущий слот lnks_RAM_READ
  //заполннение слота чтения RAM f020
    lnks_RAM_READ.OutBuf[0] = 0x01;//device_address;
    lnks_RAM_READ.OutBuf[1] = 0x03;//command_code;
    lnks_RAM_READ.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks_RAM_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks_RAM_READ.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_RAM_READ.OutBuf[5] = 0x27;//0x3F;//num_reg_Lo;  0x3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_RAM_READ.OutBuf[0], 6);
    lnks_RAM_READ.OutBuf[6]=crc.b[1];//high byte CRC
    lnks_RAM_READ.OutBuf[7]=crc.b[0];//low byte CRC
    lnks_RAM_READ.OutCnt =  8;//кол-во символов в пакете
    lnks_RAM_READ.InptCnt = 0;//пока ничего не приняли
    lnks_RAM_READ._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks_RAM_READ.Status.Stat = 0;
    lnks_RAM_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks_RAM_READ.Status.bits.change = 1;//!!! зайдя в него - меняет слот, только для пуска
    lnks_RAM_READ.FrameEnd = 120;//_15bits_Delay_115200bps / 40;//48;// / Timer3_PR; 32!
    //почему 120? это 1,8мс (1800мкс) = быстрее F020 отвечать ну просто не может!
    lnks_RAM_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks_RAM_READ.DIRTougleTime = __CTS_Delay * 2;
    //вешаю обработчик
    lnks_RAM_READ.OnRead = &lnks_RAM_READ_END;
    ///////////////////////////////////////////////////////

    //заполннение слота Записи RAM f020
    lnks_RTR_WRITE.OutBuf[0] = 0x01;//device_address;
    lnks_RTR_WRITE.OutBuf[1] = 0x03;//command_code;
    lnks_RTR_WRITE.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks_RTR_WRITE.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks_RTR_WRITE.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_RTR_WRITE.OutBuf[5] = 0x08;//num_reg_Lo; 3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_RTR_WRITE.OutBuf[0], 6);
    lnks_RTR_WRITE.OutBuf[6]=crc.b[1];//high byte CRC
    lnks_RTR_WRITE.OutBuf[7]=crc.b[0];//low byte CRC
    lnks_RTR_WRITE.OutCnt =  8;//кол-во символов в пакете
    lnks_RTR_WRITE.InptCnt = 0;//пока ничего не приняли
    lnks_RTR_WRITE._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks_RTR_WRITE.Status.Stat = 0;
    lnks_RTR_WRITE.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks_RTR_WRITE.Status.bits.Skip = 1;//слот выполняется тогда когда нужно
    lnks_RTR_WRITE.FrameEnd = 120;//_15bits_Delay_115200bps / 40;// / Timer3_PR;
    lnks_RTR_WRITE.TimeOut  = DevNotResp_TimeOut_1000ms;//DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks_RTR_WRITE.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks_RTR_WRITE.OnRead = &lnks_RTR_WRITE_END;
    ///////////////////////////////////////////////////////

    //заполннение слота чтения FLASH f020
    lnks_FLASH_READ.OutBuf[0] = 0x01;//device_address;
    lnks_FLASH_READ.OutBuf[1] = 0x03;//command_code;
    lnks_FLASH_READ.OutBuf[2] = 0x20;//reg_addr_Hi;
    lnks_FLASH_READ.OutBuf[3] = 0x00;//reg_addr_Lo;
    lnks_FLASH_READ.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_FLASH_READ.OutBuf[5] = 0x70;//num_reg_Lo;
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_FLASH_READ.OutBuf[0], 6);
    lnks_FLASH_READ.OutBuf[6]=crc.b[1];//high byte CRC
    lnks_FLASH_READ.OutBuf[7]=crc.b[0];//low byte CRC
    lnks_FLASH_READ.OutCnt =  8;//кол-во символов в пакете
    lnks_FLASH_READ.InptCnt = 0;//пока ничего не приняли
    lnks_FLASH_READ._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks_FLASH_READ.Status.Stat = 0;
    lnks_FLASH_READ.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks_FLASH_READ.Status.bits.Skip = 0;//слот выполняется, потом себя закроет
    lnks_FLASH_READ.FrameEnd = 120;//_15bits_Delay_115200bps / Timer3_PR;
    lnks_FLASH_READ.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks_FLASH_READ.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks_FLASH_READ.OnRead = &lnks_FLASH_READ_END;
    ///////////////////////////////////////////////////////

    //слот юзера будет закидывать в AINs данные на регулировку от QRC
    //заполннение слота Записи параметров пользователя в RAM
    //p3=Uz/Задание напряжения/TWORD/x002E/r0007/B/UrScale/2/
    //для начала закидываю один регистр - напряжение
    lnks_USER.OutBuf[0] = 0x01;//device_address;
    lnks_USER.OutBuf[1] = 0x10;//command_code;
    lnks_USER.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks_USER.OutBuf[3] = 0x0B;//reg_addr_Lo;
    lnks_USER.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_USER.OutBuf[5] = 0x01;//num_reg_Lo; 3F
    lnks_USER.OutBuf[6] = 0x02;//кол-во передаваемых байт
    lnks_USER.OutBuf[7] = 0x00;//данные H
    lnks_USER.OutBuf[8] = 0x00;//данные L

    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_USER.OutBuf[0], 9);
    lnks_USER.OutBuf[ 9]=crc.b[1];//high byte CRC
    lnks_USER.OutBuf[10]=crc.b[0];//low byte CRC
    lnks_USER.OutCnt =  11;//кол-во символов в пакете
    lnks_USER.InptCnt = 0;//пока ничего не приняли
    lnks_USER._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks_USER.Status.Stat = 0;
    lnks_USER.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks_USER.Status.bits.Skip = 0;//слот выполняется постоянно
    lnks_USER.FrameEnd = 120;//_15bits_Delay_115200bps / 40;// / Timer3_PR;
    lnks_USER.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks_USER.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks_USER.OnRead = &lnks_USER_WRITE_END;
    ///////////////////////////////////////////////////////
    //Чтение 1 раз в сек. данных часов и буфера статистики (ALB) из F020
    //заполннение слота чтения RAM f020
    //читаю 0x27 регистров от 0x409F до 0x40С6
    lnks_ALB.OutBuf[0] = 0x01;//device_address;
    lnks_ALB.OutBuf[1] = 0x03;//command_code;
    lnks_ALB.OutBuf[2] = 0x40;//reg_addr_Hi;
    lnks_ALB.OutBuf[3] = 0x9D;//9F;//reg_addr_Lo;
    lnks_ALB.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_ALB.OutBuf[5] = 0x29;//num_reg_Lo;  0x3F
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_ALB.OutBuf[0], 6);
    lnks_ALB.OutBuf[6]=crc.b[1];//high byte CRC
    lnks_ALB.OutBuf[7]=crc.b[0];//low byte CRC
    lnks_ALB.OutCnt =  8;//кол-во символов в пакете
    lnks_ALB.InptCnt = 0;//пока ничего не приняли
    lnks_ALB._Cnt =    0;//кол-во переданных символов
    //свойства слота
    lnks_ALB.Status.Stat = 0;
    lnks_ALB.Status.bits.NoRespond = 0;//0-ожидает ответа
    lnks_ALB.Status.bits.change = 1;//!!! зайдя в него - меняет слот, только для пуска
    lnks_ALB.Status.bits.Skip = 0;//слот выполняется, потом себя закроет
    lnks_ALB.FrameEnd = 120;//_15bits_Delay_115200bps / 48;//40;//48;// / Timer3_PR; 32!
    lnks_ALB.TimeOut  = DevNotResp_TimeOut_500ms;//(86мкс на каждый символ) нет байтов через 0,5 сек и решаем что связи нет
    lnks_ALB.DIRTougleTime = __CTS_Delay;
    //вешаю обработчик
    lnks_ALB.OnRead = &lnks_ALB_READ_END;
    ///////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////
    //pLnkSlots[0] = &lnks_RAM_READ;  //постоянно чтение RAM
    //pLnkSlots[1] = &lnks_RTR_WRITE; //ретрансоятор
    //pLnkSlots[2] = &lnks_FLASH_READ;//чтение FLASH по необходимости
    //pLnkSlots[3] = &lnks_USER;      //закидывает задание в регулятор
    //pLnkSlots[4] = &lnks_ALB;       //чтение ALB из F020
    /////////////////////////////////////////////////
    pLnkSlots[0] = &lnks_RAM_READ;  //постоянно чтение RAM
    pLnkSlots[1] = &lnks_RTR_WRITE; //ретрансоятор
    pLnkSlots[2] = &lnks_FLASH_READ;//чтение FLASH по необходимости
    //pLnkSlots[3] = &lnks_USER;      //закидывает задание в регулятор //30.03.2010 НЕ ИСПОЛЬЗУЕТСЯ
    pLnkSlots[3] = &lnks_ALB;       //чтение ALB из F020


    ////////////////////////////////////////////////////////////////
    uart3_init();//инициализация уарта 3 (подключен к F020)
    Timer3_Init();//инициализация таймера
}

void lnks_USER_WRITE_Fill (void){
  bauint crc;
    lnks_USER.OutBuf[0] = 0x01;//device_address;
    lnks_USER.OutBuf[1] = 0x10;//command_code;
    lnks_USER.OutBuf[2] = 0x00;//reg_addr_Hi;
    lnks_USER.OutBuf[3] = 0x11;//reg_addr_Lo;
    lnks_USER.OutBuf[4] = 0x00;//num_reg_Hi;
    lnks_USER.OutBuf[5] = 0x01;//num_reg_Lo; 3F
    lnks_USER.OutBuf[6] = 0x02;//кол-во передаваемых байт
    //формирую данные которые нужно передавать, только в регистр F020.AIN5
    //p37=AIN5/---/TWORD/x0042/r0011/none/1/2/
    crc.i =  RAM_DATA.QRCData.Comp;
    lnks_USER.OutBuf[7] = crc.b[1];//данные H
    lnks_USER.OutBuf[8] = crc.b[0];//данные L
    //теперь считаем КС отправляемого пакета
    crc.i = GetCRC16(&lnks_USER.OutBuf[0], 9);
    lnks_USER.OutBuf[9]=crc.b[1];//high byte CRC
    lnks_USER.OutBuf[10]=crc.b[0];//low byte CRC
}

void lnks_USER_WRITE_END (void){//записаны данные в нужное место регулятора от F020
  lnks_USER.Status.bits.CRC_Error = (GetCRC16(&lnks_USER.InBuf[0], lnks_USER.InptCnt) != 0);
  if ((lnks_USER.Status.bits.ErrTimeOut) || (lnks_USER.Status.bits.CRC_Error)) {
    //если пакет принят сбойный, или был тай аут то не обрабатываюю данные
  }
  else {
    //всё ок
    //qrc_chek_err();//если нет ответа от модуля QRC, то в МБВ передаётся код запрета регулирования 0х0800
  }
  lnks_USER_WRITE_Fill();
  lnks_USER._Cnt = 0;//чтобы начать передачу сначала
  lnks_USER.Status.bits.Skip = 0;
}
