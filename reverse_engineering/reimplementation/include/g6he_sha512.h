/*
 * g6he_sha512.h - SHA-512 for MCUboot digest verification (BR-002).
 *
 * Clean-room implementation from FIPS 180-4. The G6 HE image carries an
 * IMAGE_TLV_SHA512 (type 0x12) digest over header+payload (E-002/FACT).
 *
 * INFERENCE, high confidence: IMAGE_TLV_SHA256 (0x10) and IMAGE_TLV_SHA384
 * (0x11) are valid MCUboot digest types but are not present in the analysed
 * G6 HE artifact, so only SHA-512 is implemented. A caller that meets another
 * digest type receives G6HE_ERR_UNSUPPORTED rather than a guessed result.
 */
#ifndef G6HE_SHA512_H
#define G6HE_SHA512_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_SHA512_DIGEST_SIZE 64
#define G6HE_SHA512_BLOCK_SIZE 128

typedef struct {
    uint64_t state[8];
    uint64_t total_len;
    uint8_t buffer[G6HE_SHA512_BLOCK_SIZE];
    size_t buffer_len;
} g6he_sha512_ctx_t;

void g6he_sha512_init(g6he_sha512_ctx_t *ctx);
void g6he_sha512_update(g6he_sha512_ctx_t *ctx, const uint8_t *data, size_t len);
void g6he_sha512_final(g6he_sha512_ctx_t *ctx,
                       uint8_t out[G6HE_SHA512_DIGEST_SIZE]);
void g6he_sha512(const uint8_t *data, size_t len,
                 uint8_t out[G6HE_SHA512_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_SHA512_H */
