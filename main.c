#include "includes.h"
#include "bastypes.h"
#include "lnkmngr.h"
#include "lnkmngr_u3.h"
#include "ramdata.h"
#include "uart1rs485.h"
#include "mbtypes.h"
#include "modbus.h"
#include "init.h"
#include "uip.h"
#include "rtc.h"
#include "LPC23xx_enet.h"
#include "timer.h"
#include "clock.h"
//#include "DExS_mem_struct.h"
#include <stdio.h>
//UIP
#include "tcp_ctrl.h"
/*
две задачи этой платы:
1 - основная
служить в виде тупого ретранслятора.
т.е. если приходит запрос по внешним интефейсам(RS485,Ethernet)
просто пересылать этот запрос по внутреннему интерфейсу(UART3) DExS-у
дожидаться ответа и возвращать его обратно

2 - команда EC
Выгребать с какой то периодичностью уставки для внешних интерфейсов связи
*/


static Int32U j=0xffffff;
void LED_Blink(void){
  if (j-- == 0) {
      FIO0PIN ^= WRK_LED;
      j=0xffff;
  }
}


int sendchar(int ch);
//struct __FILE {int handle;};
//FILE __stdout;

#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)

PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART */
  //USART_SendData(EVAL_COM1, (uint8_t) ch);

  /* Loop until the end of transmission */
  //while (USART_GetFlagStatus(EVAL_COM1, USART_FLAG_TC) == RESET)
  //{}
  while(U0LSR_bit.THRE != 1);
    U0THR = ch;
  return ch;
}

int sendchar(int ch)
{
 //while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
 //USART_SendData(USART2, ch);
 return 0;
}



/*************************************************************************
 * Function Name: main
 * Parameters: none
 *
 * Return: none
 *
 * Description: main
 *
 *************************************************************************/
int main(void)
{
  Init(); //железо
  
  RAM_DATA.uart1SET = 0x0601;//настройки по дефолту, 1 адрес, скорость 6 (115200)
  RAM_DATA.IP_ADDR =  0xC0A80064;//192.168.0.100
  RAM_DATA.RAM_SIZE = 50;
  RAM_DATA.FLASH_SIZE = 50;
//  0xC0A8584D;//88.77
  /*  интерфейс отладки
  работает через порт прошиватора
  можем кидать сырые данные printf на серийный порт  */
  UartInit(UART_0,0,NORM);
  UartSetLineCoding(UART_0,UartLineCoding_115200);
  printf("Process starting");
  //RS485
  uart1rs485_init();// RS485
  
  printf("\r\ndefault settings:\r\nUART = 0x0%x\r\n", RAM_DATA.uart1SET);
  baulong ip_temp;
  ip_temp.i= RAM_DATA.IP_ADDR; 
  printf("IP_ADDR %i.%i.%i.%i\r\n",ip_temp.b[3], ip_temp.b[2], ip_temp.b[1], ip_temp.b[0]);
  printf("RAM size = %d\r\n",RAM_DATA.RAM_SIZE);//,RAM_DATA.RAM_SIZE);
  printf("FLASH size = %d \r\n",RAM_DATA.FLASH_SIZE);

  
  
  //LinkManager  UART3
  Lnk3Init();//инициализация LNK-сервера 
  Lnk3Start();//запуск сервера  
  
  //Modbus
  ModbusInit();
  
  //разрешили прерывания
  __enable_interrupt();
  
  /*   инциализация TCP/IP заложена в tcp_ctrl_update  */
  //когда настроен Modbus и UARTs надо прочитать IP адрес
  while (1)  {
    LED_Blink();
    tcp_ctrl_update ();    
    //FIO0CLR  =  ETHERNET_LED; // 
    //FIO0SET  =  ETHERNET_LED;  //
  }
}

