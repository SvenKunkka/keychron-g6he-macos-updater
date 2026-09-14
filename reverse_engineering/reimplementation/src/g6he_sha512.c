/*
 * g6he_sha512.c - SHA-512 from FIPS 180-4 (BR-002).
 *
 * Clean-room implementation. Standard constants are the published FIPS 180-4
 * round constants and initial hash values, not vendor data.
 */
#include "g6he_sha512.h"

static const uint64_t K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static uint64_t rotr(uint64_t value, unsigned bits)
{
    return (value >> bits) | (value << (64u - bits));
}

static uint64_t load_be64(const uint8_t *p)
{
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8) | ((uint64_t)p[7]);
}

static void store_be64(uint8_t *p, uint64_t value)
{
    p[0] = (uint8_t)(value >> 56);
    p[1] = (uint8_t)(value >> 48);
    p[2] = (uint8_t)(value >> 40);
    p[3] = (uint8_t)(value >> 32);
    p[4] = (uint8_t)(value >> 24);
    p[5] = (uint8_t)(value >> 16);
    p[6] = (uint8_t)(value >> 8);
    p[7] = (uint8_t)(value);
}

static void process_block(g6he_sha512_ctx_t *ctx, const uint8_t block[128])
{
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h;
    unsigned i;

    for (i = 0; i < 16; ++i) {
        w[i] = load_be64(block + (i * 8u));
    }
    for (i = 16; i < 80; ++i) {
        const uint64_t s0 = rotr(w[i - 15], 1) ^ rotr(w[i - 15], 8) ^
                            (w[i - 15] >> 7);
        const uint64_t s1 = rotr(w[i - 2], 19) ^ rotr(w[i - 2], 61) ^
                            (w[i - 2] >> 6);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 80; ++i) {
        const uint64_t s1 = rotr(e, 14) ^ rotr(e, 18) ^ rotr(e, 41);
        const uint64_t ch = (e & f) ^ ((~e) & g);
        const uint64_t temp1 = h + s1 + ch + K[i] + w[i];
        const uint64_t s0 = rotr(a, 28) ^ rotr(a, 34) ^ rotr(a, 39);
        const uint64_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint64_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

void g6he_sha512_init(g6he_sha512_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }
    ctx->state[0] = 0x6a09e667f3bcc908ULL;
    ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL;
    ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL;
    ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL;
    ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
}

void g6he_sha512_update(g6he_sha512_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i = 0;

    if (ctx == NULL || (data == NULL && len != 0)) {
        return;
    }
    ctx->total_len += (uint64_t)len;

    if (ctx->buffer_len != 0) {
        const size_t need = G6HE_SHA512_BLOCK_SIZE - ctx->buffer_len;
        const size_t take = (len < need) ? len : need;
        for (i = 0; i < take; ++i) {
            ctx->buffer[ctx->buffer_len + i] = data[i];
        }
        ctx->buffer_len += take;
        i = take;
        if (ctx->buffer_len == G6HE_SHA512_BLOCK_SIZE) {
            process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }

    while ((len - i) >= G6HE_SHA512_BLOCK_SIZE) {
        process_block(ctx, data + i);
        i += G6HE_SHA512_BLOCK_SIZE;
    }

    while (i < len) {
        ctx->buffer[ctx->buffer_len++] = data[i++];
    }
}

void g6he_sha512_final(g6he_sha512_ctx_t *ctx,
                       uint8_t out[G6HE_SHA512_DIGEST_SIZE])
{
    uint64_t bit_len;
    size_t i;

    if (ctx == NULL || out == NULL) {
        return;
    }
    bit_len = ctx->total_len * 8u;

    /* Append 0x80, pad with zeros until 112 mod 128, then the 128-bit length.
     * The high 64 bits of the bit length are always zero for our inputs. */
    ctx->buffer[ctx->buffer_len++] = 0x80;
    if (ctx->buffer_len > 112) {
        while (ctx->buffer_len < G6HE_SHA512_BLOCK_SIZE) {
            ctx->buffer[ctx->buffer_len++] = 0;
        }
        process_block(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while (ctx->buffer_len < 112) {
        ctx->buffer[ctx->buffer_len++] = 0;
    }
    store_be64(ctx->buffer + 112, 0);
    store_be64(ctx->buffer + 120, bit_len);
    process_block(ctx, ctx->buffer);
    ctx->buffer_len = 0;

    for (i = 0; i < 8; ++i) {
        store_be64(out + (i * 8u), ctx->state[i]);
    }
}

void g6he_sha512(const uint8_t *data, size_t len,
                 uint8_t out[G6HE_SHA512_DIGEST_SIZE])
{
    g6he_sha512_ctx_t ctx;
    g6he_sha512_init(&ctx);
    g6he_sha512_update(&ctx, data, len);
    g6he_sha512_final(&ctx, out);
}
