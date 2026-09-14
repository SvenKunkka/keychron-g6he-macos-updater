/*
 * g6he_sha256.h - SHA-256 from FIPS 180-4.
 *
 * Used by the integration test to verify the whole-file SHA-256 identity of the
 * real vendor artifacts (the prior task's manifest records SHA-256 digests).
 * It is NOT used by the MCUboot container parser: the analysed G6 HE images
 * carry an IMAGE_TLV_SHA512 digest, and the parser continues to reject other
 * digest TLV types as UNSUPPORTED rather than guessing.
 *
 * Clean-room implementation from the published FIPS 180-4 constants.
 */
#ifndef G6HE_SHA256_H
#define G6HE_SHA256_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_SHA256_DIGEST_SIZE 32
#define G6HE_SHA256_BLOCK_SIZE 64

typedef struct {
    uint32_t state[8];
    uint64_t total_len;
    uint8_t buffer[G6HE_SHA256_BLOCK_SIZE];
    size_t buffer_len;
} g6he_sha256_ctx_t;

void g6he_sha256_init(g6he_sha256_ctx_t *ctx);
void g6he_sha256_update(g6he_sha256_ctx_t *ctx, const uint8_t *data, size_t len);
void g6he_sha256_final(g6he_sha256_ctx_t *ctx,
                       uint8_t out[G6HE_SHA256_DIGEST_SIZE]);
void g6he_sha256(const uint8_t *data, size_t len,
                 uint8_t out[G6HE_SHA256_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_SHA256_H */
