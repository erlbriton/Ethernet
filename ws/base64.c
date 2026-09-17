/*
 * base64.c
 * Только кодирование (декодирование серверу не нужно).
 */
#include "base64.h"

static const char b64tab[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

u16 base64_encode(const u8 *src, u16 srclen, char *out, u16 outsize)
{
  u16 i, o;
  u16 need;

  need = (u16)(((u32)srclen + 2) / 3 * 4);
  if ((u16)(need + 1) > outsize) return 0;  /* буфер мал */

  o = 0;
  for (i = 0; i + 2 < srclen; i += 3) {
    u32 v = ((u32)src[i] << 16) | ((u32)src[i+1] << 8) | (u32)src[i+2];
    out[o++] = b64tab[(v >> 18) & 63];
    out[o++] = b64tab[(v >> 12) & 63];
    out[o++] = b64tab[(v >> 6) & 63];
    out[o++] = b64tab[v & 63];
  }
  if (i < srclen) {  /* хвост из 1 или 2 байт */
    u32 v = (u32)src[i] << 16;
    if (i + 1 < srclen) v |= (u32)src[i+1] << 8;
    out[o++] = b64tab[(v >> 18) & 63];
    out[o++] = b64tab[(v >> 12) & 63];
    out[o++] = (i + 1 < srclen) ? b64tab[(v >> 6) & 63] : '=';
    out[o++] = '=';
  }
  out[o] = '\0';
  return o;
}