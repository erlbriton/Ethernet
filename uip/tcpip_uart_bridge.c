/*
 * TCP/IP to UART bridge
 */

#include "tcpip_uart_bridge.h"
#include "crc16.h"
#include "modbus.h"
#include "includes.h"
#include "mbtypes.h"
#include "ramdata.h"
#include "lnkmngr.h"
#include "lnkmngr_u3.h"
#include "uip.h"//!!!
#include <string.h>
#include <stdio.h>

TClient tcpbrdata;
void tcpbrCallBack(void);


Int32U UartState = 0; // флаг, указывающий состояние uart

/*---------------------------------------------------------------------------*/
static void
connected (struct tcpipuart_state *s) {

  s->rcnt = 0; // счетчик заресетили
  s->state = IDLE_STATE;  // поехали!

}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
newdata (struct tcpipuart_state *s) {
Int32U index;
printf("new data detected! state = %d\r\n",s->state);
//u32 TimeOut;
//  Если пришли новые данные для текущего соединения:
//  во-первых, соединение должно быть свободно
//  во-вторых, надо выяснить состояние интерфейса:
//  если интерфейс свободен - сразу заносим туда пакет, запускаем работу хаба
//  и, заполучив ответ, отправляем его по ethernet
//  если же интерфейс занят, надо встать в очередь на отправку
  if (s->state == IDLE_STATE){ // приложение не занято - можно записывать данные
    s->rcnt = 0; // счетчик заресетили
    index = s->len = uip_datalen();
    memmove(&s->buffer[0], uip_appdata, index);
    // Смотрим состояние uart
    if ((UartState & 0x01) == UARTIDLE) {  // uart не занят
      UartState = UARTBUSY; // заняли uart
      // положили пакет в буфер интерфейса (длину пакета кладем в поле Idx)
      index = tcpbrdata.Idx = s->len;
      memmove(&tcpbrdata.Buffer[0], &s->buffer[0], index);

      tcpbrdata.TXCount = 0; // сброс перед новой командой: 0 = «ответа ещё нет»

      // запускаем работу хаба
      //08.11.13
      //tcpbrdata.Status.bits.Retranslate = 0;
      //debug
      if (command_decode(&tcpbrdata)) { // значит хаб отработал успешно
        // в буфере интерфейса теперь лежит ответ (длина пакета в поле TXCount)
        // - перекладываем его в буфер соединения
//      _retranslate_:
        index = s->len = tcpbrdata.TXCount;
        memmove(&s->buffer[0], &tcpbrdata.Buffer[0], index);
       // s->state = RESPONDED_STATE;       // ответ получен
        UartState = UARTIDLE;             // освободили uart
        uip_send(&s->buffer[0], s->len);  // это должно заложить ответ в нужное место
      }
      else {  // command_decode вернул FALSE
        // Возможно, запущена ретрансляция (команда 0x11):
        // ModbusDataRetranslate выставил WaitRespond=1 и активировал слот.
        // Ждём ответа от DExS, удерживая соединение открытым.
        volatile TLnkSlotStat *vs = &lnks3_RTR_DEXS.Status;
        volatile u8 *vtx = &tcpbrdata.TXCount;
        if (vs->bits.WaitRespond) {
          u32 wait = 30000000;
          while (vs->bits.WaitRespond && wait > 0) {
            wait--;
          }
          if (*vtx > 0) {
            // Ответ от DExS получен — отправляем клиенту
            index = s->len = tcpbrdata.TXCount;
            memmove(&s->buffer[0], &tcpbrdata.Buffer[0], index);
            UartState = UARTIDLE;
            uip_send(&s->buffer[0], s->len);
          } else {
            // Таймаут — сбрасываем и отпускаем соединение
            vs->bits.WaitRespond = 0;
            s->state = IDLE_STATE;
            UartState = UARTIDLE;
          }
        } else {
          // Не ретрансляция — команда не опознана
          s->state = IDLE_STATE;
          UartState = UARTIDLE;
        }
      }
    }
    else {  // uart занят
      s->state = WAITSND_STATE; // ставим соединение в очередь uart
    }
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
handle_connect (struct tcpipuart_state *s) {
Int32U index;

  switch (s->state)
  {
    case WAITSND_STATE:  // ожидаем отправки по uart
      // Смотрим состояние uart
      if ((UartState & 0x01) == UARTIDLE) {  // uart не занят
        UartState = UARTBUSY; // заняли uart
        // положили пакет в буфер интерфейса (длину пакета кладем в поле Idx)
        index = tcpbrdata.Idx = s->len;
        memmove(&tcpbrdata.Buffer[0], &s->buffer[0], index);

        tcpbrdata.TXCount = 0; // сброс перед новой командой: 0 = «ответа ещё нет»

        // запускаем работу хаба
        if (command_decode(&tcpbrdata)) { // значит хаб отработал успешно
          // в буфере интерфейса теперь лежит ответ (длина пакета в поле TXCount)
          // - перекладываем его в буфер соединения
          index = s->len = tcpbrdata.TXCount;
          memmove(&s->buffer[0], &tcpbrdata.Buffer[0], index);
          s->state = RESPONDED_STATE;       // ответ получен
          UartState = UARTIDLE;             // освободили uart
          uip_send(&s->buffer[0], s->len);  // это должно заложить ответ в нужное место
        }
        else {  // command_decode вернул FALSE
          // Возможно, запущена ретрансляция (команда 0x11):
          // ModbusDataRetranslate выставил WaitRespond=1 и активировал слот.
          // Ждём ответа от DExS, удерживая соединение открытым.
          volatile TLnkSlotStat *vs = &lnks3_RTR_DEXS.Status;
          volatile u8 *vtx = &tcpbrdata.TXCount;

          if (vs->bits.WaitRespond) {
            u32 wait = 30000000;
            while (vs->bits.WaitRespond && wait > 0) {
              wait--;
            }
            if (*vtx > 0) {
              // Ответ от DExS получен — отправляем клиенту
              index = s->len = tcpbrdata.TXCount;
              memmove(&s->buffer[0], &tcpbrdata.Buffer[0], index);
              s->state = RESPONDED_STATE;
              UartState = UARTIDLE;
              uip_send(&s->buffer[0], s->len);
            } else {
              // Таймаут — сбрасываем и отпускаем соединение
              vs->bits.WaitRespond = 0;
              s->state = IDLE_STATE;
              UartState = UARTIDLE;
            }
          } else {
            // Не ретрансляция — команда не опознана
            s->state = IDLE_STATE;
            UartState = UARTIDLE;
          }
        }
      }
      break;

    case RESPONDED_STATE:  // ничего не ожидаем
         printf("RESPOND %d",s->rcnt);
        s->rcnt++;
       
        if (s->rcnt == WAIT_RESPOND) {
          s->rcnt = 0; // счетчик заресетили
          UartState = UARTIDLE;
          s->state = IDLE_STATE;
        } 
      break;

    case IDLE_STATE:  // ничего не ожидаем

      break;
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
acked (struct tcpipuart_state *s) {

  /* если получен ACK, значит можно освобождать приложение  */
  if (s->state == RESPONDED_STATE){ // ответ получен
    s->state = IDLE_STATE;
  }
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static void
rexmit (struct tcpipuart_state *s) {

  /* если надо переслать - перешлем!  */

  uip_send(&s->buffer[0], s->len); // это должно заложить ответ в нужное место
}
/*---------------------------------------------------------------------------*/
/******************************************************************************
 *      The tcp to uart bridge application function
 *
 *****************************************************************************/
void
tcpip_uart_bridge_appcall(void) {

  struct tcpipuart_state *s = (struct tcpipuart_state *)&(uip_conn->appstate);
  // это, собсна, поле appstate структуры uip_conn

  if(uip_closed() || uip_aborted() || uip_timedout()) {
    /* НИЧО НЕ ДЕЛАЕМ */
  }

  if(uip_connected()) {
    connected(s);
  }

  if(uip_acked()) {
    acked(s);
  }

  if(uip_newdata()) {
    newdata(s);
  }

  if(uip_rexmit()) {
    rexmit(s);
  }

  if(uip_poll()) {
    handle_connect(s);
  }

}

/******************************************************************************
 *      Initialize the tcp to uart bridge
 *
 *      This function initializes the tcp to uart bridge and should be
 *      called at system boot-up.
 *****************************************************************************/
void
tcpip_uart_bridge_init(void)
{
  tcpbrdata.DevAddr = 1;  // адрес устройства в сети модбас
  tcpbrdata.Idx = 0;      // буфер начать с начала
  tcpbrdata.TXCount = 0;
  tcpbrdata.ClntTimeOut = 1;  // этот таймаут нам не нужен
  tcpbrdata.OnCallBack = tcpbrCallBack; // это пустая процедура
  tcpbrdata.OnCallBackReceive  = tcpbrCallBack; // это пустая процедура
  uip_listen(HTONS(502));
}

void
tcpbrCallBack(void)
{

}