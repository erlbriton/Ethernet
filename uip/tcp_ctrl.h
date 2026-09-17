//настройки и работа TCP/IP
#ifndef TCP_CTRL_H
#define TCP_CTRL_H
#include "includes.h"
#include "ramdata.h"
#include "flash_data.h"
//UIP
#include "uip.h"
#include "rtc.h"
#include "LPC23xx_enet.h"
#include "timer.h"
#include "clock.h"
#include <stdio.h>
//////////////////////////////////
extern void     tcp_reinit(void);//перезапуск например при изменении IP адреса или 
extern Boolean  tcp_ctrl_init (void);//фоновая инициализация
extern void     tcp_ctrl_update (void);//работа стека TCP/IP 

#endif