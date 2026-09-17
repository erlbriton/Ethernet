#ifndef MODBUS_H
#define MODBUS_H
#include "mbtypes.h"

//диапазон адресов оперативных данных F020
#define r_min_F020_RAM_DATA 0x0000
#define r_max_F020_RAM_DATA 0x00FD
//#define w_max_F020_RAM_DATA 0x0026

////диапазон адресов данных ALB
//#define r_min_ALB_DATA 0x409D
//#define r_max_ALB_DATA 0x40C6

//адреса оперативки LPC
//#define r_min_LPC_RAM_DATA 0x0040
//#define r_max_LPC_RAM_DATA 0x007F

//#define r_min_LPC_RAM_DATA 0x0040
//#define r_max_LPC_RAM_DATA 0x007F

//на них мапятся часы (используется для установки времени)
//#define r_min_LPC_RTC_DATA 0x0078
//#define r_max_LPC_RTC_DATA 0x007F

//диапазон адресов копии FLASH (уставок) F020
#define r_min_F020_FLASH_DATA 0x2000
#define r_max_F020_FLASH_DATA 0x20FF

//диапазон адресов уставок LPC
//#define r_min_LPC_FLASH_DATA 0xF000
//#define r_max_LPC_FLASH_DATA 0xF0FF

//диапазон адресов дополнительных уставок LPC
#define r_min_LPC_ADD_FLASH_DATA 0xF100
#define r_max_LPC_ADD_FLASH_DATA 0xF17F

//диапазон адресов данные калибровки LPC
#define r_min_CLB_DATA 0xF180
#define r_max_CLB_DATA 0xF1BF

extern u8 LedState;
extern void ModbusInit(void);
extern bool ModbusMemRead(TClient *pC);
extern bool ModbusMemWrite(TClient *pC);
extern bool command_decode(TClient *pC);
extern void frame_end(TClient *pC);

#endif
