#include "flash_data.h"
 /*
#pragma location=0xFFFF0400
__no_init volatile char PORT1; // PORT1 is located at address 0xFFFF0400

#pragma location="foo"
char PORT1; //PORT1 is located in segment foo

// A better way is to use a corresponding mechanism
#define FLASH _Pragma("location=\"FLASH\"")
...
FLASH int i; // i is placed in the FLASH segment
*/


#pragma section = "NVDATA"
const TFLASH_DATA FLASH_DATA @ "NVDATA" =
{
  // _____INT
  //| ____MASH
  //|| ___Network
  //||| __Card
  //IMNC
  //UART0 (СОМ4) связь с АСУ ТП (прошивка HMI)
  0x494D, //IN (IntMash)
  0x4E43, //NC (Network Card)
  0x0001, //номер сетевой карты
//  0x0401,   //  u16 RS485_Settings;
//  0xC0A80064,//192.168.0.100 IP_ADDR;//IP адрес устройства
};

#pragma location="BKDATA"
__root const u8 _bk_array[];
