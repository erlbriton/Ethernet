/*
 * ws_uart_bridge.h
 * Мост WebSocket (порт 8080) <-> UART (Modbus RTU).
 * Соглашение: одно WS-сообщение = один кадр Modbus RTU (с CRC).
 * Сессия одна (статическая): второе одновременное WS-подключение отклоняется.
 */
#ifndef __WS_UART_BRIDGE_H__
#define __WS_UART_BRIDGE_H__

#include "bastypes.h"

void ws_uart_bridge_init(void);      /* открывает слушатель 8080 (вызов в tcp_ctrl.c, шаг A6) */
void ws_uart_bridge_appcall(void);   /* вызывается диспетчером для соединений с lport=8080 (шаг A5) */

#endif /* __WS_UART_BRIDGE_H__ */