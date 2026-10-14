#include "mbtypes.h"
#include "bastypes.h"
#include "modbus.h"
#include "crc16.h"
#include "ramdata.h"
#include "lnkmngr.h"
#include "lnkmngr_u3.h"
#include "memutil.h"
#include "flash_data.h"
#include "DExS_mem_struct.h"
#include <stdio.h>
//const char DeviceID[]  ="xxxxTHXK iCM v0.07 14.02.2013 Communication Module 4xRS485 6DIO www.intmash.ru";
const char DeviceID[]  ="DExS.CPU.E100.RS485 v0.01 04.03.2020 Communication Module 1xRS485 1xEthernet www.intmash.ru";

void ModbusInit(void);
void ModbusRamRead(u32 DATA_BASE, TClient *pC);
void ModbusRamWrite(u32 DATA_BASE, TClient *pC);
void ModbusFlashWrite(u32 DATA_BASE, TClient *pC);
bool ModbusDataRetranslate(TLnkSlot *pLnkSlot, TClient *pC, bool NeedRespond);
void frame_end(TClient *pC);

///////////////////////////////////////////////////////////////////////////////
void ModbusInit(void){
  //RAM_DATA.FSERVICE = 0;
}

void frame_end(TClient *pC)
{
  bauint crc;
  crc.i = GetCRC16(&pC->Buffer[0], pC->TXCount);//подсчет CRC буфера
  //запись CRC в конец буфера
  pC->Buffer[pC->TXCount++] = crc.b[1];//crc >> 8;//crc;//crc >> 8     ;//CRCHi;
  pC->Buffer[pC->TXCount++] = crc.b[0];//crc;//crc >> 8;//crc  ;//CRCLo;
}

u8 GetDeviceIDLength(void){
u8 i=0;
 while (DeviceID[i++]!=0);
 return i;
}

void GetLPCDeviceID(TClient *pC){
u8 i=0;
u8 j;
   j = pC->Buffer[_u_byte_cnt] = pC->TXCount = GetDeviceIDLength();
   do
    pC->Buffer[_u_data_section_cm03+i]=DeviceID[i];
   while ((i++)!=j);
   pC->TXCount+=2;//прибавить длину заголовка
   frame_end(pC);
}

bool ModbusMemRead(TClient *pC){
  
  //забили на все. просто ретранслятор
  bauint w;
  //static u8 i0,i1,i2,i3;
  //1) по номерам регистров проверить какую область памяти пытается прочесть юзер,

  //a)RAM_DATA - копия ОЗУ DEXS в ОЗУ LPC
  //   [0..255]    r0000-r007F (256 байт)
  //b)FLASH_DATA - копия уставок DEXS в ОЗУ LPC
  //   [0..255]    rF000-rF07F (256 байт)  
  //2) вызываю соответствующую процедуру чтения
  
  
  //получаю номер регистра в w.i
  w.b[0] = pC->Buffer[_u_start_addr_lo];
  w.b[1] = pC->Buffer[_u_start_addr_hi];


  //выбор программы чтения памяти
  // DEXS RAM_DATA
  if ((w.i >= r_min_F020_RAM_DATA) && (w.i <= r_max_F020_RAM_DATA)) {
    
    printf("RamRead - ok \r\n");
    /*
    т.к. данные гребет постоянно слот LinkMngr, 
    просто отдаем то что было получено от него
    */
    ModbusRamRead((u32)&DEXS_RAM_DATA, pC);
    return (TRUE);
  }
  
  //DEXS FLASH_DATA
  if ((w.i >= r_min_F020_FLASH_DATA) && (w.i <= r_max_F020_FLASH_DATA)) {
    
    //запустили  на выполнение 
    printf("FlashRead - ok\r\n");     
    ModbusRamRead((u32)&DEXS_FLASH_DATA, pC);      
    return (TRUE);
  }

  //не нашёл подходящей области //пробую ретранслировать, вдруг 020 знает
  ModbusDataRetranslate(&lnks3_RTR_DEXS, pC, true); 
  return (FALSE);
}

//ModbusDataRetranslate ретрансляция принятого пакета в F020, без его разбора со стороны LPC
//  при необходимости контроля доставки пакета, может передать во внешнюю сеть ответ 020
//pLnkSlot - слот через который осуществляется ретрансляция
//pC - интерфейс от которого пришёл пакет, и которому, в случае необходимости,
//     потребуется передать пакет контроля доставки
//NeedRespond - "TRUE" если требуется подтверждение доставки пакета
bool ModbusDataRetranslate(TLnkSlot *pLnkSlot, TClient *pC, bool NeedRespond){
   bauint w;
  //ретрансляция - это не тяпы-ляпы! запрос может прийти с любого адреса! а F020 у меня всегда "1" намбер уан
  //по этой причине нужно перед ретрансляцией изменить адрес устройства на "1"  пересчитать контрольную сумму пакета
  //а если требуется ответ от F020, то в ответе от 020 также изменить адрес на тот что был в первоначальном запросе
  //нужно просто ретранслировать пакет через слот pLnkSlot, для этого
  //1) записать счётчик передаваемых байт pLnkSlot.OutCnt
  pLnkSlot->OutCnt = pC->Idx;//сколько байт в юарте
  //2) скопировать принятый пакет в pLnkSlot.OutBuf (от куда потом данные будут передаваться
  u8_mem_cpy(&pC->Buffer[0], &pLnkSlot->OutBuf[0], pLnkSlot->OutCnt);
    //2.1) теперь меняю адрес на F020
    pLnkSlot->OutBuf[0] = 0x01;//адрес F020
    //2.2) расчитываю CRC изменённого пакетa
    w.i = GetCRC16(&pLnkSlot->OutBuf[0], (pLnkSlot->OutCnt - 2));//подсчет CRC буфера кроме уже имеющейся CRC
    //2.3) подставляю новую CRC
    pLnkSlot->OutBuf[pLnkSlot->OutCnt - 2] = w.b[1];//high byte CRC
    pLnkSlot->OutBuf[pLnkSlot->OutCnt - 1] = w.b[0];//low byte CRC
  //3) разрешить слот к выполнению Skip = 0;
  pLnkSlot->InptCnt = 0;
  pLnkSlot->_Cnt = 0;//чтобы начать передачу сначала
  pLnkSlot->Status.bits.Skip = 0;//разрешили выполнение слота ретранслятора
  pLnkSlot->pC = pC;//сообщу какой интерфейс активировал слот
  pLnkSlot->Status.bits.WaitRespond = NeedRespond;//требуется ли ответ на ретранслированную команду
  pC->Status.bits.PacketTransmited = NeedRespond;
  printf("RTR: called, WaitRespond=%d, OutCnt=%d\r\n", NeedRespond, pLnkSlot->OutCnt);
  printf("RTR: _lnk3_slot_idx=%d, RTR.Skip=%d, RTR.change=%d\r\n",
       _lnk3_slot_idx,
       lnks3_RTR_DEXS.Status.bits.Skip,
       lnks3_RTR_DEXS.Status.bits.change);
  return (FALSE);//ответа не требуется
}

///////////////////////////////////////////////////////////////////////////////
bool ModbusMemWrite(TClient *pC){
    //забили на все. просто ретранслятор
  
  
  bauint w;
  //1) по номерам регистров проверить какую область памяти пытается прочесть юзер,
  //так как у меня есть следующие области так или иначе относящиеся ко флэшке
  //a) F020_FLASH_DATA - копия уставок F020 в ОЗУ LPC
  //   [0..255]    rF000-rF07F (256 байт)
  //b) LPC_FLASH_DATA - данные FLASH LPC (тут может быть всё что угодно,
  //   но все настройки связи, IP адреса и т.п. хранятся здесь)
  //   [256...511] rF080-rF0FF (256 байт)
  //c) LPC_ADD_FLASH_DATA - расширенные данные FLASH LPC (тут может быть всё что угодно)
  //   [512...767] rF100-rF17F (256 байт)
  //d) CLB_DATA данные калибровки хаба 128байт
  //   (скорее всего калибровка хода часов и работа с SD-картой)
  //   [768..895]  rF180-rF1BF (128 байт)
  //2) вызываю соответствующую процедуру чтения

  //получаю номер регистра в w.i
  w.b[0] = pC->Buffer[_u_start_addr_lo];
  w.b[1] = pC->Buffer[_u_start_addr_hi];
 //   printf("Modbus MemWrite start reg addr = %d",w.i);
  
  //выбор программы записи памяти
  //F020_RAM_DATA
  if ((w.i >= r_min_F020_RAM_DATA) && (w.i <= r_max_F020_RAM_DATA)) {
      //ретранслировать команду записи 
    printf("RamWrite - ok \n");
    ModbusDataRetranslate(&lnks3_RTR_DEXS, pC, TRUE );
    return (FALSE);
  }
  
    //F020_FLASH_DATA
  if ((w.i >= r_min_F020_FLASH_DATA) && (w.i <= r_max_F020_FLASH_DATA)) {
    //ретранслировать команду записи 020-му
    ModbusDataRetranslate(&lnks3_RTR_DEXS, pC, true);//TRUE);
    return (FALSE);
  }

  ModbusDataRetranslate(&lnks3_RTR_DEXS, pC, true);
  return (FALSE);
}

void ModbusRamRead(u32 DATA_BASE, TClient *pC){
  bauint w;
  //RAM_DATA_BASE начало адресов ОЗУ, для получения абсолютного адреса
  //к значению полученному из номера регистра прибавить RAM_DATA_BASE
  //1) получить из буфера номер регистра
  //1.1) преобразовать его в адрес в памяти
  w.b[0] = pC->Buffer[_u_start_addr_lo];
  w.b[1] = pC->Buffer[_u_start_addr_hi] & 0x0F;
//  u8_mem_cpy((u8 *)&w.i, (u8 *)&ALB_DATA.ALB[2], 2);
  u16 *ModbusAddrSet = (u16 *)(DATA_BASE + (u32)(w.i << 1));
  //2)получим кол-во передаваемых регистров
  u8 ModbusAddrCount = pC->Buffer[_u_word_count_lo];
  //4) заполнение буфера
  //4.1) записать в буфер кол-во передаваемых байт
  pC->Buffer[_u_byte_cnt]=pC->TXCount=(u16)(ModbusAddrCount << 1);
  pC->TXCount+=3;
  //4.2) записать в буфер содержимое регистров (старшими байтами вперёд)
  u8 j = _u_data_section_cm03;
  do {
    w.i = *ModbusAddrSet++;
    pC->Buffer[j++] = w.b[1];// >> 8;
    pC->Buffer[j++] = w.b[0];
  } while (--ModbusAddrCount != 0);
  frame_end(pC);
  //asdas
}


void ModbusRamWrite(u32 DATA_BASE, TClient *pC){
  bauint w;
  u32 i;
  //1) получить из буфера номер регистра
  //1.1) преобразовать его в адрес в памяти
  w.b[0] = pC->Buffer[_u_start_addr_lo];
  w.b[1] = pC->Buffer[_u_start_addr_hi] & 0x0F;
  i = DATA_BASE + (u32)(w.i << 1);
  u16 *ModbusAddrSet = (u16*)i;
  //2)получим кол-во переданных регистров
  u8 ModbusAddrCount = pC->Buffer[_u_word_count_lo];
  //3)чтение буфера в память
  u8 j = _u_data_section_cm10;
  do {
    w.b[1] = pC->Buffer[j++];
    w.b[0] = pC->Buffer[j++];
    *ModbusAddrSet++ = w.i;
  } while (--ModbusAddrCount != 0);
  pC->TXCount = 6;
  frame_end(pC);
}

void ModbusFlashWrite(u32 DATA_BASE, TClient *pC){
  bauint w;
  u32 i;
  bauint crc, _crc;
  //FLASH_DATA_BASE начало основного сектора, для получения абсолютного адреса
  //к значению полученному из номера регистра прибавить FLASH_DATA_BASE
  //0)копировать основной сектор флэша во временный буфер
  CopyFlashToTmpBuffer(DATA_BASE);
  //1) получить из буфера номер регистра
  //1.1) преобразовать его в адрес в памяти
  w.b[0] = pC->Buffer[_u_start_addr_lo];
  w.b[1] = pC->Buffer[_u_start_addr_hi] & 0x0F;
  i = (u32)&FlashTmpBuffer + (u32)(w.i << 1);//адрес во временном буфере
  u16 *ModbusAddrSet = (u16*)i;
  //2)получим кол-во переданных регистров
  u8 ModbusAddrCount = pC->Buffer[_u_word_count_lo];
  //3)внести изменения во временный буфер
  u8 j = _u_data_section_cm10;
  do {
    w.b[1] = pC->Buffer[j++];
    w.b[0] = pC->Buffer[j++];
    *ModbusAddrSet++ = w.i;
  } while (--ModbusAddrCount != 0);
  //4)данные находятся во временном буфере, теперь:
  //4.1)Подсчитать контрольную сумму временного буфера
    crc.i = GetCRC16((u8*)&FlashTmpBuffer, FlashTmpBufferSize-2);//подсчет CRC буфера
  //4.2)добавить контрольную сумму в конец временного буфера
    FlashTmpBuffer.wFlashTmpBuffer[FlashTmpBufferSize_w-1] = crc.i;
    _crc.b[0] = crc.b[1];
    _crc.b[1] = crc.b[0];
    FlashTmpBuffer.wFlashTmpBuffer[FlashTmpBufferSize_w-1] = _crc.i;
    //5)стереть резервный сектор флэша
  //6)записать туда данные из временного буфера
   __disable_interrupt(); // handles nested interrupt
   FlashSectorWrite((u32)&_bk_array, (u32) &FlashTmpBuffer);
  //6.1)Проверить CRC буфера, если не испортился писать дальше
     if (GetCRC16((u8*)&FlashTmpBuffer, FlashTmpBufferSize)==0){
       //7)стереть основной сектор флэша
       //8)записать туда данные из временного буфера
       FlashSectorWrite((u32)&FLASH_DATA, (u32) &FlashTmpBuffer);
     }
     else {
        //Блок оказался кривым, резервный сектор флеша испорчен.
        //Попытаться Восстановить из основного, если не получится
        //не пытаться переписать основной сектор
        FlashSectorWrite((u32)&_bk_array, (u32)&FLASH_DATA);
        //тут по идее какую нибудь ошибку бы выдать
     };
  __enable_interrupt(); // handles nested interrupt
  pC->TXCount = 6;
  frame_end(pC);
}

bool command_decode(TClient *pC){
  u8 cmd;
  //проверка контрольной суммы пакета
  //Если адрес устройства "0" - то пакет широковещательный, отвечать на него нельзя!
  if (GetCRC16(&pC->Buffer[0], pC->Idx) == 0){
    //проверка адреса устройствa
    if (pC->Buffer[_u_dev_addr] == pC->DevAddr){
      //адрес совпал, контрольная сумма пакета правильная
      //Расшифровка поля комад
      cmd = pC->Buffer[_u_cmd_code];
      //printf("\n CMD is %d\n",cmd);
      //printf("modbus _ command num = %d\n\n",cmd);
      switch (cmd){
        case 0x03: if (ModbusMemRead(pC)) {return (TRUE);}//multiplay registers read
                   return (FALSE);
	case 0x10: if (ModbusMemWrite(pC)) {return (TRUE);}//multiplay registers write
                   return (FALSE);
	case 0x11: return (ModbusDataRetranslate(&lnks3_RTR_DEXS, pC, TRUE ));//get device ID string
	case 0x12: GetLPCDeviceID(pC);//0112NS NS-номер стороки.
                   return (TRUE); //get strings
	default:  return (FALSE);
      };
    };
  };
  return (FALSE);
}

