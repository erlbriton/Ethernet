/*
 * ws_uart_bridge.c
 * Мост WebSocket (порт 8080) <-> UART (Modbus RTU) для LPC2364 + uIP.
 *
 * Поток данных:
 *   браузер -> WS binary frame -> снятие маски -> command_decode()
 *           -> ответ RTU в tcpbrdata.Buffer -> WS binary frame -> браузер.
 *
 * Особенности:
 *  - рукопожатие и кадры могут прийти в одном TCP-сегменте: приёмник rx
 *    накапливает поток, остатки заголовков/кадров не теряются;
 *  - если UART занят (кадр 502-моста в работе), кадр ждёт освобождения
 *    через uip_poll(), а не теряется;
 *  - ВНИМАНИЕ: tcpbrdata и UartState общие с мостом 502 (как и сейчас для
 *    двух клиентов 502). Одновременная работа 502 и 8080 не рекомендуется.
 */
#include "ws_uart_bridge.h"
#include "websocket_server.h"
#include "tcpip_uart_bridge.h"  /* UARTIDLE / UARTBUSY */
#include "crc16.h"
#include "modbus.h"
#include "includes.h"
#include "mbtypes.h"
#include "ramdata.h"
#include "lnkmngr.h"
#include "lnkmngr_u3.h"
#include "uip.h"
#include <string.h>

extern Int32U UartState;    /* определён в tcpip_uart_bridge.c */
extern TClient tcpbrdata;   /* определён в tcpip_uart_bridge.c */

#define WS_RX_SIZE  1200                    /* накопитель потока: рукопожатие + кадры */
#define WS_TX_SIZE  300//(WS_MAX_PAYLOAD + 16)  /* запас под заголовок кадра WS */

#define WS_ST_FREE       0
#define WS_ST_HANDSHAKE  1
#define WS_ST_CONNECTED  2

typedef struct {
  u8  state;
  u8  pending;      /* кадр ждёт освобождения UART */
  u16 pend_off;     /* смещение полезной нагрузки ожидающего кадра в rx */
  u16 pend_plen;    /* длина полезной нагрузки ожидающего кадра */
  u16 pend_consumed;/* полная длина ожидающего кадра в rx */
  u8  rx[WS_RX_SIZE];
  u16 rx_len;
  u8  tx[WS_TX_SIZE];
  u16 tx_len;       /* длина последнего отправленного кадра (для rexmit) */
} ws_sess_t;

static ws_sess_t ws;

/*---------------------------------------------------------------------------*/
/* Вспомогательные                                                           */
/*---------------------------------------------------------------------------*/
static void ws_release(void)
{
  ws.state = WS_ST_FREE;
  ws.pending = 0;
  ws.rx_len = 0;
  ws.tx_len = 0;
}

/* сбросить n байт из начала приёмника */
static void ws_consume(u16 n)
{
  if (n >= ws.rx_len) { ws.rx_len = 0; return; }
  memmove(ws.rx, ws.rx + n, ws.rx_len - n);
  ws.rx_len = (u16)(ws.rx_len - n);
}

/* найти конец HTTP-заголовков (\r\n\r\n), вернуть смещение ПОСЛЕ них или 0 */
static u16 ws_header_end(const u8 *buf, u16 len)
{
  u16 i;
  for (i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
      return (u16)(i + 4);
  }
  return 0;
}

/* отправить кадр клиенту, запомнив его для возможной повторной отправки */
static void ws_send_opcode(u8 opcode, const u8 *payload, u16 plen)
{
  ws.tx_len = ws_build_frame(ws.tx, sizeof(ws.tx), opcode, payload, plen);
  if (ws.tx_len) uip_send(ws.tx, ws.tx_len);
}

/* выполнить Modbus-кадр СЕЙЧАС (UART считается свободным) */
static void ws_modbus_exec(const u8 *payload, u16 len)
{
  UartState = UARTBUSY;
  tcpbrdata.Idx = len;
  memcpy(&tcpbrdata.Buffer[0], payload, len);

  tcpbrdata.TXCount = 0; // сброс перед новой командой: 0 = «ответа ещё нет»

  if (command_decode(&tcpbrdata)) {
    /* ответ RTU (с CRC) уезжает клиенту бинарным кадром */
    ws_send_opcode(WS_OP_BINARY, &tcpbrdata.Buffer[0], tcpbrdata.TXCount);
  }
  else {
    /* Возможно, запущена ретрансляция (команда 0x11):
       ModbusDataRetranslate выставил WaitRespond=1 и активировал слот.
       Ждём ответа от DExS, удерживая WebSocket-сессию открытой. */
    volatile TLnkSlotStat *vs = &lnks3_RTR_DEXS.Status;
    volatile u8 *vtx = &tcpbrdata.TXCount;

    if (vs->bits.WaitRespond) {
      u32 wait = 30000000;
      while (vs->bits.WaitRespond && wait > 0) {
        wait--;
      }
      if (*vtx > 0) {
        /* Ответ от DExS получен — отправляем клиенту бинарным кадром */
        ws_send_opcode(WS_OP_BINARY, &tcpbrdata.Buffer[0], tcpbrdata.TXCount);
      } else {
        /* Таймаут — сбрасываем флаг и отпускаем UART */
        vs->bits.WaitRespond = 0;
      }
    }
    /* Если WaitRespond не был выставлен — команда просто не опознана,
       ничего не отправляем (как и раньше). */
  }

  UartState = UARTIDLE;
}

/*---------------------------------------------------------------------------*/
/* Разбор накопленного потока                                                */
/*---------------------------------------------------------------------------*/
static void ws_process_rx(void)
{
  u8 *payload;
  u16 plen, consumed;
  ws_frame_type_t t;

  while (ws.rx_len > 0) {

    if (ws.state == WS_ST_HANDSHAKE) {
      u8  resp[160];
      u16 resp_len;
      u16 hlen;
      ws_handshake_result_t r;

      r = ws_build_handshake_response(ws.rx, ws.rx_len, resp, &resp_len);
      if (r == WS_HS_INCOMPLETE) {
        if (ws.rx_len >= WS_RX_SIZE) { uip_close(); ws_release(); } /* заголовок слишком длинный */
        return;
      }
      if (r == WS_HS_REJECT) { uip_close(); ws_release(); return; }

      uip_send(resp, resp_len);          /* ответ 101 */
      hlen = ws_header_end(ws.rx, ws.rx_len);
      if (hlen == 0) hlen = ws.rx_len;   /* защита, не должно случиться */
      ws_consume(hlen);                  /* за хвостом могут быть уже кадры WS */
      ws.state = WS_ST_CONNECTED;
      continue;
    }

    /* --- WS_ST_CONNECTED --- */

    if (ws.pending) {                    /* дожидается освобождения UART */
      if ((UartState & 0x01) != UARTIDLE) return;
      ws_modbus_exec(ws.rx + ws.pend_off, ws.pend_plen);
      ws.pending = 0;
      ws_consume(ws.pend_consumed);
      continue;
    }

    t = ws_parse_frame(ws.rx, ws.rx_len, &payload, &plen, &consumed);
    switch (t) {
      case WS_FRAME_INCOMPLETE:
        if (ws.rx_len >= WS_RX_SIZE) { uip_close(); ws_release(); } /* кадр-гигант */
        return;

      case WS_FRAME_BINARY:
      case WS_FRAME_TEXT:
        if ((UartState & 0x01) != UARTIDLE) {
          /* UART занят: кадр остаётся в начале rx, придём по uip_poll() */
          ws.pending      = 1;
          ws.pend_off     = (u16)(payload - ws.rx);
          ws.pend_plen    = plen;
          ws.pend_consumed = consumed;
          return;
        }
        ws_modbus_exec(payload, plen);
        break;

      case WS_FRAME_PING:
        ws_send_opcode(WS_OP_PONG, payload, plen);
        break;

      case WS_FRAME_PONG:
        break;

      case WS_FRAME_CLOSE:
        ws_send_opcode(WS_OP_CLOSE, 0, 0);
        uip_close();
        ws_release();
        return;

      default: /* WS_FRAME_ERROR */
        uip_abort();
        ws_release();
        return;
    }
    ws_consume(consumed);
  }
}

/*---------------------------------------------------------------------------*/
/* Обработчик событий uIP для порта 8080                                     */
/*---------------------------------------------------------------------------*/
void ws_uart_bridge_appcall(void)
{
  if (uip_conn->lport != HTONS(8080)) return; /* защита от чужих соединений */

  if (uip_closed() || uip_aborted() || uip_timedout()) {
    ws_release();
    return;
  }

  if (uip_connected()) {
    if (ws.state != WS_ST_FREE) { uip_abort(); return; } /* второе подключение */
    ws_release();
    ws.state = WS_ST_HANDSHAKE;
  }

  if (uip_newdata()) {
    u16 n = uip_datalen();
    if (ws.rx_len + n > WS_RX_SIZE) n = (u16)(WS_RX_SIZE - ws.rx_len);
    if (n) {
      memcpy(ws.rx + ws.rx_len, uip_appdata, n);
      ws.rx_len = (u16)(ws.rx_len + n);
    }
    ws_process_rx();
  }

  if (uip_rexmit()) {
    if (ws.tx_len) uip_send(ws.tx, ws.tx_len);
  }

  if (uip_poll()) {
    if (ws.state == WS_ST_CONNECTED && ws.pending) ws_process_rx();
  }
}

/*---------------------------------------------------------------------------*/
/* Инициализация                                                             */
/*---------------------------------------------------------------------------*/
void ws_uart_bridge_init(void)
{
  ws_release();
  uip_listen(HTONS(8080));
}