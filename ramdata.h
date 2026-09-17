#ifndef RAMDATA_H
#define RAMDATA_H
#include "includes.h"
#include "rtc.h"

struct TRAM_DATA{
  //адреса iCM начинаются с 0х0040
  u32 IP_ADDR;    //r0040 текущий IP-адрес
  u16 uart1SET;  //r0042 настройки UART2
  u16 RAM_SIZE; //размер для чтения РАМКИ
  u16 FLASH_SIZE; //размер для чтения РАМКИ
 /* u16 PERFCNT;   //r0043 счётчик производительности основного цикла
  u16 USERS;     //r0044 количество юзеров на Ethernet-e
  u32 mIP_ADDR;  //r0040 модифицированный IP-адрес*/
};


extern volatile struct TRAM_DATA RAM_DATA;

#endif
