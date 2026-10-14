/*
 * websocket_server.c
 * Реализация минимального WebSocket-сервера (RFC 6455):
 *  - HTTP-рукопожатие (101 Switching Protocols, Sec-WebSocket-Accept);
 *  - разбор кадров клиента со снятием маски;
 *  - сборка кадров сервера без маски.
 * Состояния соединений и работа с UART сюда НЕ входят -
 * это задача ws_uart_bridge (шаг A4).
 */
#include "websocket_server.h"
#include "sha1.h"
#include "base64.h"
#include <string.h>

/* Магический GUID из RFC 6455, раздел 1.3 */
static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

/*---------------------------------------------------------------------------*/
/* Вспомогательные                                                           */
/*---------------------------------------------------------------------------*/

/* Поиск подстроки без учёта регистра. Возвращает указатель или 0. */
static const u8 *ci_memmem(const u8 *hay, u16 haylen, const char *needle)
{
  u16 nlen = 0;
  u16 i, j;
  while (needle[nlen]) nlen++;
  if (nlen == 0 || haylen < nlen) return 0;
  for (i = 0; i + nlen <= haylen; i++) {
    for (j = 0; j < nlen; j++) {
      u8 a = hay[i+j];
      u8 b = (u8)needle[j];
      if (a >= 'A' && a <= 'Z') a |= 0x20;
      if (b >= 'A' && b <= 'Z') b |= 0x20;
      if (a != b) break;
    }
    if (j == nlen) return &hay[i];
  }
  return 0;
}

/* Положить строку в буфер, вернуть число записанных байт */
static u16 put_str(u8 *dst, const char *s)
{
  u16 n = 0;
  while (s[n]) { dst[n] = (u8)s[n]; n++; }
  return n;
}

/*---------------------------------------------------------------------------*/
/* Рукопожатие                                                               */
/*---------------------------------------------------------------------------*/
ws_handshake_result_t ws_build_handshake_response(const u8 *req, u16 req_len,
                                                  u8 *resp, u16 *resp_len)
{
  const u8 *keyp;
  const u8 *reqend;
  u8  key[64];
  u16 keylen;
  u8  digest[20];
  char b64[32];
  u16 b64len;
  u16 o;
  sha1_ctx_t ctx;

  *resp_len = 0;

  /* заголовки ещё не пришли целиком - ждём пустую строку \r\n\r\n */
  if (!ci_memmem(req, req_len, "\r\n\r\n")) return WS_HS_INCOMPLETE;

  /* Ищем GET. Разрешаем смещение, если в начале буфера остался мусор от прошлого сокета */
  if (!ci_memmem(req, req_len, "GET ")) return WS_HS_REJECT;

  /* Ищем Upgrade и websocket раздельно, чтобы не зависеть от количества пробелов */
  if (!ci_memmem(req, req_len, "upgrade:")) return WS_HS_REJECT;
  if (!ci_memmem(req, req_len, "websocket")) return WS_HS_REJECT;

  /* извлекаем значение Sec-WebSocket-Key */
  keyp = ci_memmem(req, req_len, "sec-websocket-key:");
  if (!keyp) return WS_HS_REJECT;
  keyp += 18; /* длина "sec-websocket-key:" */
  reqend = req + req_len;
  while (keyp < reqend && *keyp == ' ') keyp++;
  keylen = 0;
  while (keyp < reqend && *keyp != '\r' && *keyp != '\n' && keylen < sizeof(key) - 1)
    key[keylen++] = *keyp++;
  while (keylen && key[keylen-1] == ' ') keylen--;
  if (keylen == 0) return WS_HS_REJECT;

  /* accept = base64(sha1(key + GUID)) */
  sha1_init(&ctx);
  sha1_update(&ctx, key, keylen);
  sha1_update(&ctx, (const u8 *)WS_GUID, sizeof(WS_GUID) - 1);
  sha1_final(&ctx, digest);
  b64len = base64_encode(digest, 20, b64, sizeof(b64));
  if (b64len == 0) return WS_HS_REJECT;

  /* собираем ответ 101. Буфер resp должен быть >= 160 байт. */
  o  = put_str(resp, "HTTP/1.1 101 Switching Protocols\r\n");
  o += put_str(resp + o, "Upgrade: websocket\r\n");
  o += put_str(resp + o, "Connection: Upgrade\r\n");
  o += put_str(resp + o, "Sec-WebSocket-Accept: ");
  memcpy(resp + o, b64, b64len);
  o += b64len;
  o += put_str(resp + o, "\r\n\r\n");

  *resp_len = o;
  return WS_HS_OK;
}

/*---------------------------------------------------------------------------*/
/* Разбор кадра клиента                                                      */
/*---------------------------------------------------------------------------*/
ws_frame_type_t ws_parse_frame(u8 *buf, u16 buf_len,
                               u8 **payload, u16 *payload_len,
                               u16 *consumed)
{
  u8  opcode, masked;
  u8  mask[4];
  u32 plen;
  u16 hdr = 2;
  u16 i;

  *payload = 0;
  *payload_len = 0;
  *consumed = 0;

  if (buf_len < 2) return WS_FRAME_INCOMPLETE;

  /* фрагментацию не поддерживаем: бит FIN обязан быть установлен */
  if ((buf[0] & 0x80) == 0) return WS_FRAME_ERROR;
  opcode = buf[0] & 0x0F;
  masked = (buf[1] & 0x80) ? 1 : 0;
  plen   = buf[1] & 0x7F;

  if (plen == 126) {
    if (buf_len < 4) return WS_FRAME_INCOMPLETE;
    plen = ((u32)buf[2] << 8) | (u32)buf[3];
    hdr = 4;
  } else if (plen == 127) {
    u32 hi;
    if (buf_len < 10) return WS_FRAME_INCOMPLETE;
    hi = ((u32)buf[2] << 24) | ((u32)buf[3] << 16) | ((u32)buf[4] << 8) | (u32)buf[5];
    if (hi != 0) return WS_FRAME_ERROR; /* длина > 4 Гбайт - ошибка */
    plen = ((u32)buf[6] << 24) | ((u32)buf[7] << 16) | ((u32)buf[8] << 8) | (u32)buf[9];
    hdr = 10;
  }
  if (plen > WS_MAX_PAYLOAD) return WS_FRAME_ERROR;

  if (masked) {
    if (buf_len < hdr + 4) return WS_FRAME_INCOMPLETE;
    mask[0] = buf[hdr]; mask[1] = buf[hdr+1];
    mask[2] = buf[hdr+2]; mask[3] = buf[hdr+3];
    hdr += 4;
  }
  /* NOTE: по RFC сервер обязан закрыть немаскированное соединение.
   * Мы принимаем и немаскированные кадры - это упрощает проверку
   * самодельными тест-клиентами. Браузеры всегда шлют с маской. */

  if (buf_len < hdr + plen) return WS_FRAME_INCOMPLETE;

  *payload     = &buf[hdr];
  *payload_len = (u16)plen;
  *consumed    = (u16)(hdr + plen);

  if (masked) {
    for (i = 0; i < plen; i++) buf[hdr + i] ^= mask[i & 3];
  }

  switch (opcode) {
    case WS_OP_BINARY: return WS_FRAME_BINARY;
    case WS_OP_TEXT:   return WS_FRAME_TEXT;
    case WS_OP_CLOSE:  return WS_FRAME_CLOSE;
    case WS_OP_PING:   return WS_FRAME_PING;
    case WS_OP_PONG:   return WS_FRAME_PONG;
    default:           return WS_FRAME_ERROR;
  }
}

/*---------------------------------------------------------------------------*/
/* Сборка кадра сервера (без маски)                                          */
/*---------------------------------------------------------------------------*/
u16 ws_build_frame(u8 *out, u16 out_size,
                   u8 opcode, const u8 *payload, u16 payload_len)
{
  u16 hdr = 2;
  u16 i;

  if (payload_len > 125) hdr = 4;
  if ((u32)out_size < (u32)hdr + (u32)payload_len) return 0; /* не влезает */

  out[0] = 0x80 | (opcode & 0x0F); /* FIN + opcode */
  if (payload_len <= 125) {
    out[1] = (u8)payload_len;
  } else {
    out[1] = 126;
    out[2] = (u8)(payload_len >> 8);
    out[3] = (u8)(payload_len & 0xFF);
  }
  for (i = 0; i < payload_len; i++) out[hdr + i] = payload[i];
  return (u16)(hdr + payload_len);
}