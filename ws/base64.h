/*
 * base64.h
 * Кодирование Base64 (нужно только для Sec-WebSocket-Accept).
 */
#ifndef __BASE64_H__
#define __BASE64_H__

#include "bastypes.h"

/* Кодирует src в out. В out должно быть место под base64_enc_len(len)+1 байт.
 * Возвращает длину строки без '\0' или 0, если буфер мал. */
u16 base64_encode(const u8 *src, u16 srclen, char *out, u16 outsize);

#endif /* __BASE64_H__ */