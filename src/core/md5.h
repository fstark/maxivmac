/*
 * Public domain MD5 implementation.
 * Based on the reference implementation in RFC 1321.
 * Simplified to a single header for embedding.
 */

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace md5_impl {

struct MD5Context {
    uint32_t state[4];
    uint64_t count;
    uint8_t  buffer[64];
};

static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }
static inline uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static inline void md5_transform(uint32_t state[4], const uint8_t block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
        M[i] = (uint32_t)block[i*4]
             | ((uint32_t)block[i*4+1] << 8)
             | ((uint32_t)block[i*4+2] << 16)
             | ((uint32_t)block[i*4+3] << 24);
    }

    static const uint32_t T[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
        0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
        0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
        0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
        0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
        0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
        0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
        0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
        0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const int s[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16)      { f = F(b,c,d); g = i; }
        else if (i < 32) { f = G(b,c,d); g = (5*i+1) % 16; }
        else if (i < 48) { f = H(b,c,d); g = (3*i+5) % 16; }
        else              { f = I(b,c,d); g = (7*i) % 16; }
        uint32_t temp = d;
        d = c; c = b;
        b = b + rotl(a + f + T[i] + M[g], s[i]);
        a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static inline void md5_init(MD5Context* ctx)
{
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count = 0;
    std::memset(ctx->buffer, 0, 64);
}

static inline void md5_update(MD5Context* ctx, const uint8_t* data, size_t len)
{
    size_t index = (size_t)(ctx->count & 0x3F);
    ctx->count += len;

    size_t i = 0;
    if (index) {
        size_t part = 64 - index;
        if (len >= part) {
            std::memcpy(ctx->buffer + index, data, part);
            md5_transform(ctx->state, ctx->buffer);
            i = part;
        } else {
            std::memcpy(ctx->buffer + index, data, len);
            return;
        }
    }
    for (; i + 64 <= len; i += 64) {
        md5_transform(ctx->state, data + i);
    }
    if (i < len) {
        std::memcpy(ctx->buffer, data + i, len - i);
    }
}

static inline void md5_final(MD5Context* ctx, uint8_t digest[16])
{
    uint8_t pad[64];
    std::memset(pad, 0, 64);
    pad[0] = 0x80;

    uint64_t bits = ctx->count * 8;
    size_t index = (size_t)(ctx->count & 0x3F);
    size_t padLen = (index < 56) ? (56 - index) : (120 - index);
    md5_update(ctx, pad, padLen);

    uint8_t bitsLE[8];
    for (int i = 0; i < 8; i++) bitsLE[i] = (uint8_t)(bits >> (i * 8));
    md5_update(ctx, bitsLE, 8);

    for (int i = 0; i < 4; i++) {
        digest[i*4+0] = (uint8_t)(ctx->state[i]);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

} // namespace md5_impl


// ── Public API ──

inline void md5_buf(const uint8_t* data, size_t len, uint8_t out[16])
{
    md5_impl::MD5Context ctx;
    md5_impl::md5_init(&ctx);
    md5_impl::md5_update(&ctx, data, len);
    md5_impl::md5_final(&ctx, out);
}

inline bool md5_file(const char* path, uint8_t out[16])
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;

    md5_impl::MD5Context ctx;
    md5_impl::md5_init(&ctx);

    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        md5_impl::md5_update(&ctx, buf, n);
    }
    std::fclose(f);
    md5_impl::md5_final(&ctx, out);
    return true;
}
