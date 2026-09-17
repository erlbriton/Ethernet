/*
 * websocket_server.h
 * Минимальный WebSocket-сервер (RFC 6455) для LPC2364 + uIP.
 * Поддерживает только бинарные кадры, без расширений, без фрагментации
 * (одно сообщение = один кадр), без сжатия.
 * Соглашение: одно WS-сообщение = один полный кадр Modbus RTU (с CRC).
 */
#ifndef __WEBSOCKET_SERVER_H__
#define __WEBSOCKET_SERVER_H__

#include "bastypes.h"

/* Опкоды кадров (RFC 6455) */
#define WS_OP_CONTINUATION 0x0
#define WS_OP_TEXT         0x1
#define WS_OP_BINARY       0x2
#define WS_OP_CLOSE        0x8
#define WS_OP_PING         0x9
#define WS_OP_PONG         0xA

/* Максимальный полезный груз, который соглашаемся принять.
 * Кадр Modbus RTU (256 байт максимум) помещается с запасом. */
#define WS_MAX_PAYLOAD     256

/* Результат разбора HTTP-рукопожатия */
typedef enum {
  WS_HS_INCOMPLETE = 0, /* заголовок пришёл не целиком - ждём ещё данных */
  WS_HS_OK,             /* корректный Upgrade-запрос, ответ собран в resp */
  WS_HS_REJECT          /* это не WebSocket-запрос - соединение закрыть */
} ws_handshake_result_t;

/* Результат разбора WS-кадра */
typedef enum {
  WS_FRAME_INCOMPLETE = 0, /* кадр пришёл не целиком - ждём ещё данных */
  WS_FRAME_BINARY,         /* бинарное сообщение: payload/payload_len валидны */
  WS_FRAME_TEXT,           /* текстовое сообщение: обрабатываем как бинарное */
  WS_FRAME_CLOSE,          /* клиент закрывает соединение */
  WS_FRAME_PING,           /* нужно ответить PONG-ом */
  WS_FRAME_PONG,           /* ответ на наш пинг - игнорируем */
  WS_FRAME_ERROR           /* нарушение протокола - закрыть соединение */
} ws_frame_type_t;

/*
 * Разбор входящего HTTP-рукопожатия WebSocket.
 * req/req_len   - принятые от клиента байты.
 * resp/resp_len - выход: готовый HTTP-ответ 101 для отправки клиенту.
 * Возвращает WS_HS_INCOMPLETE, если конец заголовков (\r\n\r\n) ещё не виден.
 */
ws_handshake_result_t ws_build_handshake_response(const u8 *req, u16 req_len,
                                                  u8 *resp, u16 *resp_len);

/*
 * Разбор ОДНОГО WS-кадра из потока.
 * buf/buf_len  - принятые байты (кадр может быть неполным или их может быть несколько).
 * payload      - выход: указатель на полезную нагрузку внутри buf
 *                (маска клиента снимается прямо в буфере, in place).
 * payload_len  - выход: длина полезной нагрузки.
 * consumed     - выход: сколько байт buf занял этот кадр
 *                (остаток можно передать этой функции снова).
 */
ws_frame_type_t ws_parse_frame(u8 *buf, u16 buf_len,
                               u8 **payload, u16 *payload_len,
                               u16 *consumed);

/*
 * Сборка кадра сервер->клиент (сервер отправляет БЕЗ маски).
 * out/out_size - буфер назначения и его размер.
 * Возвращает полную длину кадра или 0, если кадр не влезает в буфер.
 */
u16 ws_build_frame(u8 *out, u16 out_size,
                   u8 opcode, const u8 *payload, u16 payload_len);

#endif /* __WEBSOCKET_SERVER_H__ */