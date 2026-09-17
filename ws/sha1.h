/*
 * sha1.h
 * Минимальная реализация SHA1 для рукопожатия WebSocket.
 */
#ifndef __SHA1_H__
#define __SHA1_H__

#include "bastypes.h"

typedef struct {
  u32 state[5];
  u32 count[2];      /* счётчик бит: младшее и старшее слово */
  u8  buffer[64];    /* недообработанный блок */
} sha1_ctx_t;

void sha1_init(sha1_ctx_t *ctx);
void sha1_update(sha1_ctx_t *ctx, const u8 *data, u32 len);
void sha1_final(sha1_ctx_t *ctx, u8 digest[20]);

/* разово: посчитать дайджест целиком */
void sha1(const u8 *data, u32 len, u8 digest[20]);

#endif /* __SHA1_H__ */