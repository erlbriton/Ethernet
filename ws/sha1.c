/*
 * sha1.c
 * Компактная реализация SHA1 (W-массив на 16 слов, экономим стек).
 */
#include "sha1.h"
#include <string.h>

#define ROL(v,b) (((v) << (b)) | ((v) >> (32 - (b))))

static void sha1_transform(u32 state[5], const u8 block[64])
{
  u32 a, b, c, d, e;
  u32 w[16];
  int i;

  for (i = 0; i < 16; i++) {
    w[i]  = ((u32)block[i*4+0]) << 24;
    w[i] |= ((u32)block[i*4+1]) << 16;
    w[i] |= ((u32)block[i*4+2]) << 8;
    w[i] |= ((u32)block[i*4+3]);
  }

  a = state[0]; b = state[1]; c = state[2]; d = state[3]; e = state[4];

  for (i = 0; i < 80; i++) {
    u32 f, k, t;
    if (i < 20)      { f = (b & c) | ((~b) & d);        k = 0x5A827999; }
    else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1; }
    else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
    else             { f = b ^ c ^ d;                   k = 0xCA62C1D6; }
    if (i >= 16) {
      u32 idx = (u32)i & 15;
      w[idx] = ROL(w[(idx+13)&15] ^ w[(idx+8)&15] ^ w[(idx+2)&15] ^ w[idx], 1);
    }
    t = ROL(a,5) + f + e + k + w[i & 15];
    e = d; d = c; c = ROL(b,30); b = a; a = t;
  }

  state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

void sha1_init(sha1_ctx_t *ctx)
{
  ctx->state[0] = 0x67452301;
  ctx->state[1] = 0xEFCDAB89;
  ctx->state[2] = 0x98BADCFE;
  ctx->state[3] = 0x10325476;
  ctx->state[4] = 0xC3D2E1F0;
  ctx->count[0] = 0;
  ctx->count[1] = 0;
}

void sha1_update(sha1_ctx_t *ctx, const u8 *data, u32 len)
{
  u32 i, j, part;

  j = (ctx->count[0] >> 3) & 63;  /* сколько байт уже лежит в buffer */

  /* прибавляем биты к счётчику */
  if ((ctx->count[0] += (len << 3)) < (len << 3)) ctx->count[1]++;
  ctx->count[1] += (len >> 29);

  i = 0;
  if (j) {  /* буфер был не пуст - донаполняем до 64 */
    part = 64 - j;
    if (len < part) part = len;
    memcpy(&ctx->buffer[j], &data[0], part);
    if (j + part == 64) sha1_transform(ctx->state, ctx->buffer);
    i = part;
    if (i == len) return;  /* всё ушло в буфер */
  }

  for (; i + 64 <= len; i += 64) {
    sha1_transform(ctx->state, &data[i]);
  }

  if (i < len) {
    memcpy(ctx->buffer, &data[i], len - i);
  }
}

void sha1_final(sha1_ctx_t *ctx, u8 digest[20])
{
  u8 bits[8];
  u8 zeros[56];
  u8 tail = 0x80;
  u32 i, j, padlen;

  bits[0] = (u8)(ctx->count[1] >> 24);
  bits[1] = (u8)(ctx->count[1] >> 16);
  bits[2] = (u8)(ctx->count[1] >> 8);
  bits[3] = (u8)(ctx->count[1]);
  bits[4] = (u8)(ctx->count[0] >> 24);
  bits[5] = (u8)(ctx->count[0] >> 16);
  bits[6] = (u8)(ctx->count[0] >> 8);
  bits[7] = (u8)(ctx->count[0]);

  j = (ctx->count[0] >> 3) & 63;
  padlen = (j < 56) ? (56 - j) : (120 - j);

  sha1_update(ctx, &tail, 1);
  padlen--;
  if (padlen > 0) {
    memset(zeros, 0, sizeof(zeros));
    sha1_update(ctx, zeros, padlen);
  }
  sha1_update(ctx, bits, 8);

  for (i = 0; i < 5; i++) {
    digest[i*4+0] = (u8)(ctx->state[i] >> 24);
    digest[i*4+1] = (u8)(ctx->state[i] >> 16);
    digest[i*4+2] = (u8)(ctx->state[i] >> 8);
    digest[i*4+3] = (u8)(ctx->state[i]);
  }
}

void sha1(const u8 *data, u32 len, u8 digest[20])
{
  sha1_ctx_t ctx;
  sha1_init(&ctx);
  sha1_update(&ctx, data, len);
  sha1_final(&ctx, digest);
}