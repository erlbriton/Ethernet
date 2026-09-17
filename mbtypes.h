#ifndef __MBTYPES_H
#define __MBTYPES_H
#include "includes.h"

// MODBUS commans structure (common block)
#define _u_dev_addr      0
#define _u_cmd_code      1
#define _u_start_addr_hi 2
#define _u_start_addr_lo 3
#define _u_word_count_hi 4
#define _u_word_count_lo 5
// MODBUS commans structure ( additional data field for 10h command)
#define _u_byte_cnt_cm10     6
#define _u_data_section_cm10 7
// MODBUS commans structure ( additional data field for 03h command)
#define _u_data_section_cm03 3

// MODBUS commans structure (byte count, as rule for transmiting response from device)
#define _u_byte_cnt 2

#define illegal_data_address       0x02
#define error_in_data_size         0x03
#define error_in_request_data_size 0x04
#define error_message_lenght       0x03

typedef
  union TClntStat { // Byte-addressable UINT
     u8 Stat;
	 struct {
           unsigned ReceiveValidPacket : 1; //когда примет валидный пакет, то взведёт этот флаг как сигнал северу
           unsigned PacketTransmited   : 1; //когда передаст пакет, подаст серверу сигнал, что можно переключаться на других клиентов
           unsigned WaitForReceive     : 1;
           unsigned WaitForTransmit    : 1;
	 }bits;
  } TClntStat;

typedef struct _TClient_
{
  TClntStat Status;//состояние интерфейса
  u16  ClntTimeOut;//FE для данных поступающих от внешнего интерфейса (скорость вариэйбл)
  u8   DevAddr;    //Адрес устройства на который нужно реагировать (его получаю при обмене по запросу от сервера,
                   //и на 0-адрес(групповой)тоже нужно реагировать. При проверке валидности пакета, поле адреса
                   //сравнивается с DevAddr и "0" если там они, то пакет передаётся серверу)
  void (*OnCallBack)(void);//чтобы не ждать ответа от 020, продолжаем выполнение.
  void (*OnCallBackReceive)(void);//для длительных операций чтения и записи, подготовка данных переносится в основной
                                  //цикл, после того как данные подготовлены, через эту функцю данные отправляются на передачу
  u8  TXCount;    //кол-во байт которое нужно передать
  u8  Idx;        //индекс для перемещения по буферу
  u8  Buffer[256];//тут складируются принятые данные, а так же готовятся пакеты для передачи
  u8  ID; //номер интерфейса 0-UART0, 1-UART1, 2-UART2, 3-UART3
  //для операций с файлами через интерфейс
  u8 BPS; //cкорость сввязи
} TClient;

#endif
