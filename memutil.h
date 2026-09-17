#ifndef __MEMUTIL_H
#define __MEMUTIL_H
#include "includes.h"
#include "bastypes.h"

//буфер временного хранения данных сектора FALSH
//для ускорения копирования данные хранятся двойными словами
//в данном случае размер буфера (100h)256 байт = (40h)64 двойных слов
#define FlashTmpBufferSize	  512 //IAP требует 512/1024/4096/8192/
#define FlashTmpBufferSize_dw FlashTmpBufferSize / 4 //размер буфера в двойных словах
#define FlashTmpBufferSize_w  FlashTmpBufferSize / 2 //размер буфера в словах

typedef union {//массив DWORD, WORD, BYTE
  u32 dwFlashTmpBuffer [FlashTmpBufferSize_dw];
  u16 wFlashTmpBuffer  [FlashTmpBufferSize_w];
  u8  bFlashTmpBuffer  [FlashTmpBufferSize];
} a32a16a8;

extern a32a16a8 FlashTmpBuffer;
extern void u8_mem_cpy( u8 *s, u8 *d, u8 c);
extern void swp_copy_u16( u8 *s, u16 *d, u8 c);
extern void swp_copy_u8( u8 *s, u8 *d, u8 c);
extern void FlashSectorWrite(u32 FlashSectorAddr, u32 Buffer);
extern s32 FlashAddr2SecNum(u32 Addr);
extern void CopyFlashToTmpBuffer(u32 Addr);

#endif
