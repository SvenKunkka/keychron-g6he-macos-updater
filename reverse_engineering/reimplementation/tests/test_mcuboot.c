/*
 * test_mcuboot.c - BR-001..BR-005 container validation.
 *
 * Includes an integration check against the real +84 artifact when it can be
 * located. The artifact is only ever opened read-only; it is never modified.
 */
#include "g6he_test.h"

#include "g6he_crc.h"
#include "g6he_mcuboot.h"
#include "g6he_sha512.h"

#include <stdio.h>
#include <stdlib.h>

static void put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static size_t build_synthetic(uint8_t *buf)
{
    const size_t header_size = 32u;
    uint8_t payload[32];
    size_t payload_len = 0;
    uint8_t digest[G6HE_SHA512_DIGEST_SIZE];
    uint8_t tlvs[256];
    size_t tlv_len = 0;
    size_t tlv_offset;
    unsigned i;

    put32(payload + payload_len, 0x20010000u);
    payload_len += 4;
    put32(payload + payload_len, 0x20000101u);
    payload_len += 4;
    memcpy(payload + payload_len, "test-payload", 12u);
    payload_len += 12u;

    put32(buf + 0, G6HE_MCUBOOT_MAGIC);
    put32(buf + 4, 0x20000000u);
    put16(buf + 8, (uint16_t)header_size);
    put16(buf + 10, 0u);
    put32(buf + 12, (uint32_t)payload_len);
    put32(buf + 16, G6HE_MCUBOOT_RAM_LOAD_FLAG);
    buf[20] = 1u;
    buf[21] = 2u;
    put16(buf + 22, 3u);
    put32(buf + 24, 4u);
    put32(buf + 28, 0u);
    memcpy(buf + header_size, payload, payload_len);

    tlv_offset = header_size + payload_len;
    g6he_sha512(buf, tlv_offset, digest);

    tlvs[tlv_len++] = G6HE_TLV_SHA512;
    tlvs[tlv_len++] = 0u;
    put16(tlvs + tlv_len, G6HE_SHA512_DIGEST_SIZE);
    tlv_len += 2;
    memcpy(tlvs + tlv_len, digest, G6HE_SHA512_DIGEST_SIZE);
    tlv_len += G6HE_SHA512_DIGEST_SIZE;

    tlvs[tlv_len++] = G6HE_TLV_KEYHASH;
    tlvs[tlv_len++] = 0u;
    put16(tlvs + tlv_len, 64u);
    tlv_len += 2;
    for (i = 0; i < 64u; ++i) {
        tlvs[tlv_len++] = (uint8_t)i;
    }

    tlvs[tlv_len++] = G6HE_TLV_ED25519;
    tlvs[tlv_len++] = 0u;
    put16(tlvs + tlv_len, 64u);
    tlv_len += 2;
    for (i = 0; i < 64u; ++i) {
        tlvs[tlv_len++] = (uint8_t)(63u - i);
    }

    put16(buf + tlv_offset, G6HE_MCUBOOT_TLV_MAGIC);
    put16(buf + tlv_offset + 2, (uint16_t)(4u + tlv_len));
    memcpy(buf + tlv_offset + 4, tlvs, tlv_len);
    return tlv_offset + 4u + tlv_len;
}

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

static void test_real_artifact(void)
{
    const char *path = getenv("G6HE_REAL_IMAGE");
    uint8_t *data = NULL;
    size_t len = 0;
    g6he_image_info_t info;

    if (path == NULL) {
        path = "../../firmware/G6HE_v1.0.0+84_202609101503.signed.bin";
    }
    if (!read_file(path, &data, &len)) {
        printf("  SKIP real artifact not readable at %s\n", path);
        return;
    }
    printf("  real artifact: %s (%lu bytes, SHA-512 digest recomputed in C)\n",
           path, (unsigned long)len);
    CHECK_EQ_INT(len, 350953u);
    CHECK_EQ_INT(g6he_mcuboot_parse(data, len, &info), G6HE_OK);
    CHECK_EQ_INT(info.header.magic, G6HE_MCUBOOT_MAGIC);
    CHECK_EQ_INT(info.header.load_address, 0x20000800u);
    CHECK_EQ_INT(info.header.header_size, 0x800u);
    CHECK_EQ_INT(info.header.image_size, 348692u);
    CHECK_EQ_INT(info.header.flags, G6HE_MCUBOOT_RAM_LOAD_FLAG);
    CHECK_EQ_INT(info.tlv_offset, 0x55A14u);
    CHECK_EQ_INT(info.tlv_total, 213u);
    CHECK_EQ_INT(info.tlv_count, 4u);
    CHECK(info.digest_verified);
    CHECK(info.key_hash_present);
    CHECK(info.signature_present);
    CHECK(info.sig_pure);
    CHECK_EQ_INT(info.initial_sp, 0x20063950u);
    CHECK_EQ_INT(info.reset_vector, 0x20021FC9u);
    CHECK(g6he_mcuboot_ram_load_ok(&info.header));
    CHECK(g6he_mcuboot_has_model(data, len, &info, "54LMG6HE"));
    CHECK(!g6he_mcuboot_has_model(data, len, &info, "54LMG9HE"));
    {
        char text[G6HE_VERSION_STRING_MAX];
        g6he_mcuboot_format_version(&info.header, text);
        CHECK_EQ_STR(text, "1.0.0+84");
    }
    CHECK_EQ_INT(g6he_crc32(data, len), 0x482F47D5u);
    free(data);
}

void test_mcuboot(void)
{
    uint8_t buffer[1024];
    size_t length;
    g6he_image_info_t info;

    printf("[mcuboot]\n");
    length = build_synthetic(buffer);
    CHECK_EQ_INT(g6he_mcuboot_parse(buffer, length, &info), G6HE_OK);
    CHECK_EQ_INT(info.header.image_size, 20u);
    CHECK(info.digest_verified);
    CHECK(info.sig_pure == false);
    CHECK_EQ_STR(info.tlvs[0].type == G6HE_TLV_SHA512 ? "sha512" : "?", "sha512");
    {
        char text[G6HE_VERSION_STRING_MAX];
        g6he_mcuboot_format_version(&info.header, text);
        CHECK_EQ_STR(text, "1.2.3+4");
    }

    /* Tampered payload fails the digest. */
    buffer[33] ^= 0x01u;
    CHECK_EQ_INT(g6he_mcuboot_parse(buffer, length, &info), G6HE_ERR_DIGEST);
    buffer[33] ^= 0x01u;

    /* Wrong magic. */
    buffer[0] = 'B';
    CHECK_EQ_INT(g6he_mcuboot_parse(buffer, length, &info), G6HE_ERR_MAGIC);
    buffer[0] = (uint8_t)(G6HE_MCUBOOT_MAGIC & 0xFFu);

    /* Truncated input. */
    CHECK_EQ_INT(g6he_mcuboot_parse(buffer, 8u, &info), G6HE_ERR_TRUNCATED);

    /* Protected-TLV containers are rejected, not guessed at. */
    put16(buffer + 10, 1u);
    CHECK_EQ_INT(g6he_mcuboot_parse(buffer, length, &info),
                 G6HE_ERR_UNSUPPORTED);
    put16(buffer + 10, 0u);

    /* Missing signature TLV. */
    {
        uint8_t copy[1024];
        size_t copy_len;
        uint32_t tlv_offset;
        memcpy(copy, buffer, length);
        copy_len = length - 68u;
        tlv_offset = (uint32_t)copy[8] | ((uint32_t)copy[9] << 8);
        tlv_offset += (uint32_t)copy[12] | ((uint32_t)copy[13] << 8) |
                      ((uint32_t)copy[14] << 16) | ((uint32_t)copy[15] << 24);
        put16(copy + tlv_offset + 2, (uint16_t)(copy_len - tlv_offset));
        CHECK_EQ_INT(g6he_mcuboot_parse(copy, copy_len, &info),
                     G6HE_ERR_SIGNATURE);
    }

    /* Zero reset vector with a recomputed digest. */
    {
        uint8_t copy[1024];
        uint8_t digest[G6HE_SHA512_DIGEST_SIZE];
        uint32_t tlv_offset;
        memcpy(copy, buffer, length);
        tlv_offset = (uint32_t)copy[8] | ((uint32_t)copy[9] << 8);
        tlv_offset += (uint32_t)copy[12] | ((uint32_t)copy[13] << 8) |
                      ((uint32_t)copy[14] << 16) | ((uint32_t)copy[15] << 24);
        put32(copy + 32u + 4u, 0u);
        g6he_sha512(copy, tlv_offset, digest);
        memcpy(copy + tlv_offset + 8, digest, G6HE_SHA512_DIGEST_SIZE);
        CHECK_EQ_INT(g6he_mcuboot_parse(copy, length, &info), G6HE_ERR_RANGE);
    }

    test_real_artifact();
}
