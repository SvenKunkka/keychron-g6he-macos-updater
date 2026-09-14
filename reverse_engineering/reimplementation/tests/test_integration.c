/*
 * test_integration.c - real-artifact integration vectors for +84 and +87.
 *
 * BR-018. Both vendor artifacts are opened READ-ONLY and are never modified.
 * Every check here is a fact about the shipped image; nothing is copied from
 * the images into this source tree.
 *
 *   +84  G6HE_v1.0.0+84_202609101503.signed.bin   (350953 B)
 *   +87  G6HE_v1.0.0+86_202609141615.signed.bin   (352225 B, filename says +86)
 *
 * The second artifact's filename build number (+86) disagrees with the
 * embedded MCUboot version (+87); the test asserts the embedded value and the
 * mismatch, so the release-traceability defect cannot regress silently.
 *
 * Paths are supplied by the environment so no vendor path is baked into the
 * source tree: G6HE_REAL_IMAGE (+84, with a tree-relative fallback used by the
 * existing mcuboot test) and G6HE_REAL_IMAGE_87 (no fallback; skipped unless
 * the environment or the Makefile provides it).
 */
#include "g6he_test.h"

#include "g6he_crc.h"
#include "g6he_identity.h"
#include "g6he_mcuboot.h"
#include "g6he_sha256.h"
#include "g6he_sha512.h"

#include <stdio.h>
#include <stdlib.h>

static bool read_file(const char *path, uint8_t **out, size_t *out_len)
{
    FILE *file;
    long size;
    uint8_t *buffer;

    file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    buffer = (uint8_t *)malloc((size_t)size);
    if (buffer == NULL) {
        fclose(file);
        return false;
    }
    if (fread(buffer, 1u, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return false;
    }
    fclose(file);
    *out = buffer;
    *out_len = (size_t)size;
    return true;
}

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

static bool contains_bytes(const uint8_t *hay, size_t hay_len,
                           const uint8_t *needle, size_t needle_len)
{
    size_t i;

    if (needle_len == 0u || hay_len < needle_len) {
        return false;
    }
    for (i = 0; i + needle_len <= hay_len; ++i) {
        if (hay[i] == needle[0] &&
            memcmp(hay + i, needle, needle_len) == 0) {
            return true;
        }
    }
    return false;
}

typedef struct {
    const char *env_name;
    const char *fallback;
    size_t size;
    const char *sha256;
    const char *version;
    uint32_t image_size;
    uint32_t tlv_offset;
    uint16_t tlv_total;
    uint32_t initial_sp;
    uint32_t reset_vector;
    uint32_t crc32;
    bool g9_absent;
    const char *expect_model;
    const char *expect_version;
} vector_t;

static const vector_t k_vectors[] = {
    {"G6HE_REAL_IMAGE",
     "../../firmware/G6HE_v1.0.0+84_202609101503.signed.bin",
     350953u,
     "38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a",
     "1.0.0+84",
     348692u,
     0x55A14u,
     213u,
     0x20063950u,
     0x20021FC9u,
     0x482F47D5u,
     true,
     "54LMG6HE",
     "1.0.0+84"},
    {"G6HE_REAL_IMAGE_87",
     NULL,
     352225u,
     "76e3afced4a60d85c5113ed41fe56a8138e653b3fa4499f4c861062addd6c62a",
     "1.0.0+87",
     349964u,
     0x55F0Cu,
     213u,
     0x20063E38u,
     0x2002238Du,
     0x1B58CDE4u,
     true,
     "54LMG6HE",
     "1.0.0+87"},
};

static void check_vector(const vector_t *v)
{
    const char *path = getenv(v->env_name);
    uint8_t *data = NULL;
    size_t len = 0;
    g6he_image_info_t info;
    char hex[G6HE_SHA256_DIGEST_SIZE * 2u + 1u];
    uint8_t digest256[G6HE_SHA256_DIGEST_SIZE];
    uint8_t digest512[G6HE_SHA512_DIGEST_SIZE];
    char text[G6HE_VERSION_STRING_MAX];

    if (path == NULL || path[0] == '\0') {
        path = v->fallback;
    }
    if (path == NULL || !read_file(path, &data, &len)) {
        printf("  SKIP %s not readable at %s\n", v->version,
               path != NULL ? path : "(unset)");
        return;
    }
    printf("  %s: %s (%lu bytes)\n", v->version, path, (unsigned long)len);

    CHECK_EQ_INT(len, v->size);

    /* Whole-file SHA-256 (independent of the container). */
    g6he_sha256(data, len, digest256);
    hex_of(digest256, sizeof(digest256), hex);
    CHECK_EQ_STR(hex, v->sha256);

    /* Whole-file updater CRC-32 (reflected 0xEDB88320, init 0xFFFFFFFF). */
    CHECK_EQ_INT(g6he_crc32(data, len), v->crc32);

    /* Container, digest TLV and vector table. */
    CHECK_EQ_INT(g6he_mcuboot_parse(data, len, &info), G6HE_OK);
    CHECK_EQ_INT(info.header.magic, G6HE_MCUBOOT_MAGIC);
    CHECK_EQ_INT(info.header.load_address, 0x20000800u);
    CHECK_EQ_INT(info.header.header_size, 0x800u);
    CHECK_EQ_INT(info.header.image_size, v->image_size);
    CHECK_EQ_INT(info.header.flags, G6HE_MCUBOOT_RAM_LOAD_FLAG);
    CHECK_EQ_INT(info.header.protected_tlv_size, 0u);
    CHECK_EQ_INT(info.tlv_offset, v->tlv_offset);
    CHECK_EQ_INT(info.tlv_total, v->tlv_total);
    CHECK_EQ_INT(info.tlv_count, 4u);
    CHECK(info.digest_verified);
    CHECK(info.key_hash_present);
    CHECK(info.signature_present);
    CHECK(info.sig_pure);
    CHECK_EQ_INT(info.initial_sp, v->initial_sp);
    CHECK_EQ_INT(info.reset_vector, v->reset_vector);
    CHECK(g6he_mcuboot_ram_load_ok(&info.header));

    /* Container arithmetic closes exactly: no trailing bytes. */
    CHECK_EQ_INT((size_t)v->tlv_offset + (size_t)v->tlv_total, len);
    CHECK_EQ_INT((size_t)0x800u + (size_t)v->image_size, v->tlv_offset);

    /* Embedded SHA-512 recomputed over header + payload. */
    g6he_sha512(data, (size_t)v->tlv_offset, digest512);
    {
        size_t i;
        bool digest_tlv_found = false;
        for (i = 0; i < info.tlv_count; ++i) {
            if (info.tlvs[i].type == G6HE_TLV_SHA512) {
                digest_tlv_found = true;
                CHECK_EQ_INT(info.tlvs[i].length, G6HE_SHA512_DIGEST_SIZE);
                CHECK_MEM_EQ(digest512,
                             data + info.tlvs[i].offset + 4u,
                             G6HE_SHA512_DIGEST_SIZE);
            }
        }
        CHECK(digest_tlv_found);
    }

    /* Vector-table bounds: inside the nRF54LM20A RAM window, Thumb reset. */
    CHECK_EQ_INT(info.initial_sp & 3u, 0u);
    CHECK(info.initial_sp >= G6HE_RAM_START);
    CHECK(info.initial_sp < G6HE_RAM_END_EXCLUSIVE);
    CHECK(info.reset_vector >= G6HE_RAM_START);
    CHECK(info.reset_vector < G6HE_RAM_END_EXCLUSIVE);
    CHECK_EQ_INT(info.reset_vector & 1u, 1u);

    /* Version string and the filename/build-number mismatch. */
    g6he_mcuboot_format_version(&info.header, text);
    CHECK_EQ_STR(text, v->version);
    if (strcmp(v->version, "1.0.0+87") == 0) {
        CHECK(strstr(path, "+86") != NULL); /* filename says +86 ... */
        CHECK(strstr(path, "+87") == NULL); /* ... embedded says +87 */
    }

    /* Identity: G6 model present, G9 identity absent from the payload. */
    CHECK(g6he_mcuboot_has_model(data, len, &info, v->expect_model));
    if (v->g9_absent) {
        CHECK(!g6he_mcuboot_has_model(data, len, &info, "54LMG9HE"));
        CHECK(!g6he_mcuboot_has_model(data, len, &info, "0x3434:0xd092"));
    }

    /* KCFWID identity blob: model + embedded version. */
    {
        const uint8_t *blob = g6he_mcuboot_find_string(data, len, &info,
                                                       "KCFWID");
        char model[G6HE_IDENTITY_FIELD_MAX];
        char version[G6HE_VERSION_FIELD_MAX];
        CHECK(blob != NULL);
        if (blob != NULL) {
            const size_t remaining = (size_t)(data + info.tlv_offset - blob);
            CHECK_EQ_INT(g6he_kcfwid_extract(blob, remaining, model, version),
                         G6HE_OK);
            CHECK_EQ_STR(model, v->expect_model);
            CHECK_EQ_STR(version, v->expect_version);
        }
    }

    /* Update-protocol HID descriptor signature: usage page 0x008c followed by
     * report IDs 0xB1 and 0xB2 and 32-count reports. */
    {
        static const uint8_t usage_page[] = {0x05u, 0x8Cu};
        static const uint8_t report_b1[] = {0x85u, 0xB1u};
        static const uint8_t report_b2[] = {0x85u, 0xB2u};
        static const uint8_t count32[] = {0x95u, 0x20u};
        CHECK(contains_bytes(data, len, usage_page, sizeof(usage_page)));
        CHECK(contains_bytes(data, len, report_b1, sizeof(report_b1)));
        CHECK(contains_bytes(data, len, report_b2, sizeof(report_b2)));
        CHECK(contains_bytes(data, len, count32, sizeof(count32)));
    }
    free(data);
}

void test_integration(void)
{
    size_t i;

    printf("[integration]\n");
    for (i = 0; i < sizeof(k_vectors) / sizeof(k_vectors[0]); ++i) {
        check_vector(&k_vectors[i]);
    }
}
