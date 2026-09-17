#ifndef LNKMNGRU3_H
#define LNKMNGRU3_H

#include "includes.h"
#include "ramdata.h"
#include "mbtypes.h"
#include "lnkmngr.h"
////////////////////

#define lnk3_slots_usage 4 //кол-во используемых (в проекте) слотов
//extern TLnkSlot lnks3_RAM_READ;
extern TLnkSlot lnks3_RTR_DEXS;//ретранслятор
//extern TLnkSlot lnks3_FLASH_READ;
//extern TLnkSlot lnks_FLASH_WRITE;
//extern TLnkSlot lnks3_ALB;

extern TLnkSlot *pLnk3Slots[lnk3_slots_usage];

extern void Lnk3Init (void);//инициализация структур сервера
extern void Lnk3Start (void);//запуск сервера
extern char _lnk3_slot_idx;

#endif
