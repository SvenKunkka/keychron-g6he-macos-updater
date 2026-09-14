/*
 * test_sha256.c - FIPS 180-4 SHA-256 known-answer tests.
 *
 * Vectors are the published FIPS 180-4 / NIST examples:
 *   ""      -> e3b0c442...
 *   "abc"   -> ba7816bf...
 *   the 448-bit "abcdbcde..." message
 * plus a split-update case to exercise the streaming path across blocks.
 */
#include "g6he_test.h"

#include "g6he_sha256.h"

static void hex_of(const uint8_t *bytes, size_t len, char *out)
{
    static const char digits[] = "0123456789abcdef";
    size_t i;

    for (i = 0; i < len; ++i) {
        out[i * 2u] = digits[bytes[i] >> 4];
        out[i * 2u + 1u] = digits[bytes[i] & 0x0Fu];
    }
    out[len * 2u] = '\0';
}

void test_sha256(void)
{
    uint8_t digest[G6HE_SHA256_DIGEST_SIZE];
    char hex[G6HE_SHA256_DIGEST_SIZE * 2u + 1u];
    g6he_sha256_ctx_t ctx;
    size_t i;

    printf("[sha256]\n");

    g6he_sha256((const uint8_t *)"", 0u, digest);
    hex_of(digest, sizeof(digest), hex);
    CHECK_EQ_STR(hex,
                 "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    g6he_sha256((const uint8_t *)"abc", 3u, digest);
    hex_of(digest, sizeof(digest), hex);
    CHECK_EQ_STR(hex,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    {
        static const char msg[] =
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        g6he_sha256((const uint8_t *)msg, sizeof(msg) - 1u, digest);
        hex_of(digest, sizeof(digest), hex);
        CHECK_EQ_STR(
            hex,
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    }

    /* Streaming across a block boundary: 1000 x 'a'. */
    g6he_sha256_init(&ctx);
    for (i = 0; i < 10u; ++i) {
        static const char chunk[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        g6he_sha256_update(&ctx, (const uint8_t *)chunk, 100u);
    }
    g6he_sha256_final(&ctx, digest);
    hex_of(digest, sizeof(digest), hex);
    CHECK_EQ_STR(hex,
                 "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
}
