/*
 * uip_app_dispatch.c
 * Единственная точка входа UIP_APPCALL.
 * 502  -> старый мост TCP<->UART (Modbus),
 * 8080 -> новый мост WebSocket<->UART.
 */
#include "uip.h"
#include "tcpip_uart_bridge.h"
#include "ws_uart_bridge.h"

void uip_app_dispatch(void)
{
  if (uip_conn->lport == HTONS(502)) {
    tcpip_uart_bridge_appcall();
    return;
  }
  if (uip_conn->lport == HTONS(8080)) {
    ws_uart_bridge_appcall();
    return;
  }
  /* прочих локальных портов у нас нет - событий не ждём */
}