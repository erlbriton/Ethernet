#ifndef LNKMNGRU2_H
#define LNKMNGRU2_H

#include "includes.h"
#include "ramdata.h"
#include "mbtypes.h"
#include "lnkmngr.h"
////////////////////

#define lnk2_slots_usage 4 //кол-во используемых (в проекте) слотов
extern TLnkSlot lnks2_RAM_READ;
extern TLnkSlot lnks2_RTR_WRITE;//ретранслятор
extern TLnkSlot lnks2_FLASH_READ;
//extern TLnkSlot lnks_FLASH_WRITE;
extern TLnkSlot lnks2_ALB;

extern TLnkSlot *pLnk2Slots[lnk2_slots_usage];

extern void Lnk2Init (void);//инициализация структур сервера
extern void Lnk2Start (void);//запуск сервера
extern char _lnk2_slot_idx;

#endif
