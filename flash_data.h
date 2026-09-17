#ifndef FLASH_DATA_H
#define FLASH_DATA_H
#include "includes.h"


typedef struct{// TFLASH_DATA{
  //МАС-адрес Ethernet карты
  u16 MAC0;
  u16 MAC1;
  u16 MAC2;  
//  u16 RS485_Settings;
//  u32 Ethernet_IPaddr;
} TFLASH_DATA;

extern const TFLASH_DATA FLASH_DATA;
//extern const struct TFLASH_DATA FLASH_DATA;
extern const u8 _bk_array[];
#endif
