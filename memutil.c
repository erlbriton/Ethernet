#include "memutil.h"
#include "ramdata.h"//debug

void u8_mem_cpy( u8 *s, u8 *d, u8 c);
void swp_copy_u16( u8 *s, u16 *d, u8 c);
void FlashSectorWrite(u32 FlashSectorAddr, u32 Buffer);
s32 FlashAddr2SecNum(u32 Addr);//адрес памяти в номер сектора (если нет такого сектора то "-1")
void CopyFlashToTmpBuffer(u32 Addr);

#define IAP_LOCATION 0x7ffffff1
unsigned long command[5];
unsigned long result[2];
//Define pointer to function type, which takes two parameters and returns void.
//Note the IAP returns the result with the base address of the table residing
//in R1.

__no_init a32a16a8 FlashTmpBuffer;//для IAP должен располагаться в "обычной" памяти

typedef void (*IAP)(unsigned long [],unsigned long[]);
IAP iap_entry = (IAP)(IAP_LOCATION);

#define _SysCLK 66355200 //MHz
/*
Sector  Sector      Address                     LPC23xx
Number  Size [kB]   Range                       128kB
0       4           0X0000 0000 - 0X0000 0FFF   x
1       4           0X0000 1000 - 0X0000 1FFF   x
2       4           0X0000 2000 - 0X0000 2FFF   x
3       4           0X0000 3000 - 0X0000 3FFF   x
4       4           0X0000 4000 - 0X0000 4FFF   x
5       4           0X0000 5000 - 0X0000 5FFF   x
6       4           0X0000 6000 - 0X0000 6FFF   x
7       4           0X0000 7000 - 0X0000 7FFF   x
8       32          0x0000 8000 - 0X0000 FFFF   x
9       32          0x0001 0000 - 0X0001 7FFF   x
10(0x0A)32          0x0001 8000 - 0X0001 FFFF   x
*/
//преобразует адрес флэша в номер сектора, в зависимости от карты памяти девайса
typedef struct TFlashSpace {
  u8  SecNum;//номер слота
  u32 AdrRng;//начальный адрес сектора. Обход идёт от большего адреса к меньшему
             //как только "пробиваемый" адрес больше или равен AdrRng, принимаю
             //решение что номер сегмента найден
} TFlashSpace;

#define FlashSectorsCount 11 //количество секторов флэша в процессоре
const struct TFlashSpace FlashSpace [FlashSectorsCount] = {
                   //Sector  Sector      Address                     LPC23xx
                   //Number  Size [kB]   Range                       128kB
  {0 , 0x00000FFF},//0       4           0X0000 0000 - 0X0000 0FFF   x
  {1 , 0x00001FFF},//1       4           0X0000 1000 - 0X0000 1FFF   x
  {2 , 0x00002FFF},//2       4           0X0000 2000 - 0X0000 2FFF   x
  {3 , 0x00003FFF},//3       4           0X0000 3000 - 0X0000 3FFF   x
  {4 , 0x00004FFF},//4       4           0X0000 4000 - 0X0000 4FFF   x
  {5 , 0x00005FFF},//5       4           0X0000 5000 - 0X0000 5FFF   x
  {6 , 0x00006FFF},//6       4           0X0000 6000 - 0X0000 6FFF   x
  {7 , 0x00007FFF},//7       4           0X0000 7000 - 0X0000 7FFF   x
  {8 , 0x0000FFFF},//8       32          0x0000 8000 - 0X0000 FFFF   x
  {9 , 0x00017FFF},//9       32          0x0001 0000 - 0X0001 7FFF   x
  {10, 0x0001FFFF} //10(0x0A)32          0x0001 8000 - 0X0001 FFFF   x
};

s32 FlashAddr2SecNum(u32 Addr){
  u32 i = 0;
  do {
    if (Addr <= FlashSpace[i].AdrRng) {
      //адрес меньше или равен сверяемому диапазону, значит это наш сектор
      return (i);//
    }
    i++;
  } while (i != FlashSectorsCount);
  //не нашёл в списке подходящий сектор
  return (-1);
}

void FlashSectorWrite(u32 Addr, u32 Buffer)
{
  s32 SN = FlashAddr2SecNum(Addr);//адрес во флэш в номер сектора
  if (SN == -1) return;//если такого сектора нет, то на выход
  u32 CCLK_Hz	   = _SysCLK / 1000; //CCLK / 1000;//System Clock (CCLK) in KHz
     //IAP Prepare sector(s) for write operation
     command[0] = 50;//IAP Prepare sector(s) for write operation
     command[1] = SN;//(u32)&_nv_array;//SectorNumber;// 0x2000;//Start Sector Number
     command[2] = SN;//(u32)&_nv_array;//SectorNumber;// 0x2000;//End Sector Number
     iap_entry (command, result);
     //Erase Sector
     command[0] = 52;//IAP Erase Sector
     command[1] = SN;//(u32)&_nv_array;//SectorNumber;// 0x2000;//Start Sector Number
     command[2] = SN;//(u32)&_nv_array;//SectorNumber;// 0x2000;//End Sector Number
     command[3] = CCLK_Hz;
     iap_entry (command, result);
     //IAP Prepare sector(s) for write operation
     command[0] = 50;//IAP Prepare sector(s) for write operation
     command[1] = SN;// 0x2000;//Start Sector Number
     command[2] = SN;// 0x2000;//End Sector Number
     iap_entry (command, result);
     //Write
     command[0] = 51;//IAP Copy RAM to Flash
     command[1] = Addr;//Destination Flash address
     command[2] = Buffer;//Source RAM address
     command[3] = FlashTmpBufferSize;//Number of bytes to be written.
     command[4] = CCLK_Hz;
     iap_entry (command, result);
}

//копирует содержимое заданного сектора флэш памяти во временный буфер (двойными словами)
void CopyFlashToTmpBuffer(u32 Addr){
  u32 *source = (u32 *) Addr;
  u32 *dest   = (u32 *) FlashTmpBuffer.dwFlashTmpBuffer;
  u32  count  = FlashTmpBufferSize_dw;
  while (count != 0) {//21 команды
    *dest++ = *source++;
    count--;
  };
}

void u8_mem_cpy( u8 *s, u8 *d, u8 c){
  while (c) {
    c--;
    *d++ = *s++;
  }
}

//swp_copy_u16( u8 *s, u16 *d, u8 c)
//копировать с перестановкой байт
//u8  *s адрес буфера - откуда нужно забирать принятые данные
//u16 *d адрес буфера - куда нужно ложить обработанные данные (пишу по 2 байта)
//u8   c количество слов! которые необходимо перевернуть
void swp_copy_u16( u8 *s, u16 *d, u8 c){
  bauint w;
  while (c) {
    c--;
    //сваплю и копирую
    w.b[1] = *s++;
    w.b[0] = *s++;
    *d++ = w.i;
  }
}

//swp_copy_u16( u8 *s, u16 *d, u8 c)
//копировать с перестановкой байт
//u8  *s адрес буфера - откуда нужно забирать принятые данные
//u16 *d адрес буфера - куда нужно ложить обработанные данные (пишу по 2 байта)
//u8   c количество слов! которые необходимо перевернуть
void swp_copy_u8( u8 *s, u8 *d, u8 c){
  bauint w;
  while (c) {
    c--;
    //сваплю и копирую
    w.b[1] = *s++;
    w.b[0] = *s++;
    *d++ = w.b[0];
    *d++ = w.b[1];
  }
}