//структура данных регулятора на F020
#ifndef DEXS_MEM_STRUCT_H
#define DEXS_MEM_STRUCT_H
#include "includes.h"
#include "ramdata.h"




// RAM
#define DEXS_RAM_SIZE (RAM_DATA.RAM_SIZE/2)//240
typedef __packed struct TDEXS_RAM_DATA{
  u16 DEXS_RAM[250];  
  //u16 DEXS_RAM[DEXS_RAM_SIZE];  
}TDEXS_RAM_DATA;
extern struct TDEXS_RAM_DATA DEXS_RAM_DATA;//данные RAM 


//FLASH
#define DEXS_FLASH_SIZE (RAM_DATA.FLASH_SIZE/2)//50
struct TDEXS_FLASH_DATA{
  u16 DEXS_FLASH[250];
//  u16 DEXS_FLASH[DEXS_FLASH_SIZE];
};
extern struct TDEXS_FLASH_DATA DEXS_FLASH_DATA;//данные FLASH 

#endif