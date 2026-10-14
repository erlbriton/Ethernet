/*
 * TCP/IP to UART bridge header file
 */

#ifndef __TCPIP_UART_BRIDGE_H__
#define __TCPIP_UART_BRIDGE_H__

//#include "multiapplication.h"
//#include "uip.h"
//#include "uipopt.h"
//#include "modbus.h"
//#include "includes.h"
#include "uip-conf.h"

#define UARTIDLE  0  // uart не зан€т - можно отправл€ть пакет
#define UARTBUSY  1  // uart зан€т!!!

#define WAIT_RESPOND  40

/* ќжидание ответа от DExS в циклах while().
   —лот ретрансл€ции использует таймаут DevNotResp_TimeOut_1000ms (~1000 мс).
   ќдин проход пустого цикла на LPC2364 (72 ћ√ц) Ч примерно 5Ц10 циклов CPU,
   итого 30 000 000 итераций ? 2Ц4 секунды, с запасом x2Цx4 к таймауту слота.
   ≈сли частота CPU или таймаут слота измен€тс€ Ч пересчитать. */
#define WAIT_FOR_REPLY_LOOPS  30000000

//extern u16_t uip_len;
//extern u8_t uip_buf[UIP_BUFSIZE+2];

struct tcpipuart_state {
  enum {IDLE_STATE, WAITSND_STATE, RESPONDED_STATE} state;
  u8_t buffer[256];
  int len;
  int rcnt;
};

typedef struct tcpipuart_state uip_tcp_appstate_t;

#include "uip_app_dispatch.h"

#ifndef UIP_APPCALL
#define UIP_APPCALL uip_app_dispatch
#endif

void tcpip_uart_bridge_init(void);
void tcpip_uart_bridge_appcall(void);

#endif /* __TCPIP_UART_BRIDGE_H__ */

