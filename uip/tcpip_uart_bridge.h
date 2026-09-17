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

#define UARTIDLE  0  // uart не занят - можно отправлять пакет
#define UARTBUSY  1  // uart занят!!!

#define WAIT_RESPOND  40

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

