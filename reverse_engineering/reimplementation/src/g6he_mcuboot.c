/*
 * g6he_mcuboot.c - MCUboot container validation (BR-001..BR-005, BR-013).
 */
#include "g6he_mcuboot.h"

#include <string.h>

static uint16_t rd_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static bool is_sig_type(uint8_t type)
{
    return type >= G6HE_TLV_RSA2048_PSS && type <= G6HE_TLV_ED25519;
}

g6he_status_t g6he_mcuboot_parse(const uint8_t *data, size_t len,
                                 g6he_image_info_t *out)
{
    uint32_t tlv_offset;
    uint16_t tlv_magic;
    uint16_t tlv_total;
    size_t cursor;
    size_t end;
    unsigned digest_count = 0;
    unsigned key_hash_count = 0;
    unsigned signature_count = 0;

    if (data == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (len < G6HE_MCUBOOT_HEADER_MIN_SIZE) {
        return G6HE_ERR_TRUNCATED;
    }
    memset(out, 0, sizeof(*out));

    out->header.magic = rd_u32(data + 0);
    out->header.load_address = rd_u32(data + 4);
    out->header.header_size = rd_u16(data + 8);
    out->header.protected_tlv_size = rd_u16(data + 10);
    out->header.image_size = rd_u32(data + 12);
    out->header.flags = rd_u32(data + 16);
    out->header.major = data[20];
    out->header.minor = data[21];
    out->header.revision = rd_u16(data + 22);
    out->header.build_number = rd_u32(data + 24);

    if (out->header.magic != G6HE_MCUBOOT_MAGIC) {
        return G6HE_ERR_MAGIC;
    }
    /* header_size is uint16, so it is inherently <= 0xFFFF; the documented
     * 0x10000 ceiling cannot be exceeded and is enforced by the type. */
    if (out->header.header_size < G6HE_MCUBOOT_HEADER_MIN_SIZE ||
        (size_t)out->header.header_size > len ||
        (out->header.header_size % 4u) != 0u) {
        return G6HE_ERR_TLV;
    }
    if (out->header.image_size < 8u) {
        return G6HE_ERR_TRUNCATED;
    }
    if (out->header.load_address == 0u ||
        out->header.load_address == 0xFFFFFFFFu) {
        return G6HE_ERR_RANGE;
    }
    if (out->header.protected_tlv_size != 0u) {
        /* v1.1.0 explicitly rejects protected-TLV containers (PARTIAL). */
        return G6HE_ERR_UNSUPPORTED;
    }

    tlv_offset = out->header.header_size + out->header.image_size;
    if (tlv_offset < out->header.header_size) { /* overflow */
        return G6HE_ERR_TLV;
    }
    if ((size_t)tlv_offset + 4u > len) {
        return G6HE_ERR_TRUNCATED;
    }
    tlv_magic = rd_u16(data + tlv_offset);
    tlv_total = rd_u16(data + tlv_offset + 2);
    if (tlv_magic != G6HE_MCUBOOT_TLV_MAGIC) {
        return G6HE_ERR_MAGIC;
    }
    if (tlv_total < 4u) {
        return G6HE_ERR_TLV;
    }
    if ((size_t)tlv_offset + (size_t)tlv_total != len) {
        return G6HE_ERR_TLV;
    }

    out->tlv_offset = tlv_offset;
    out->tlv_total = tlv_total;
    end = (size_t)tlv_offset + (size_t)tlv_total;
    cursor = (size_t)tlv_offset + 4u;

    while (cursor < end) {
        uint8_t type;
        uint8_t pad;
        uint16_t length;
        size_t value_start;
        size_t value_end;

        if (cursor + 4u > end) {
            return G6HE_ERR_TRUNCATED;
        }
        type = data[cursor];
        pad = data[cursor + 1];
        length = rd_u16(data + cursor + 2);
        if (pad != 0u) {
            return G6HE_ERR_TLV;
        }
        value_start = cursor + 4u;
        value_end = value_start + length;
        if (value_end > end) {
            return G6HE_ERR_TLV;
        }

        if (out->tlv_count < G6HE_MAX_TLVS) {
            out->tlvs[out->tlv_count].offset = (uint32_t)cursor;
            out->tlvs[out->tlv_count].type = type;
            out->tlvs[out->tlv_count].pad = pad;
            out->tlvs[out->tlv_count].length = length;
        }
        out->tlv_count++;

        if (type == G6HE_TLV_SHA512) {
            uint8_t digest[G6HE_SHA512_DIGEST_SIZE];
            digest_count++;
            if (length != G6HE_SHA512_DIGEST_SIZE) {
                return G6HE_ERR_DIGEST;
            }
            g6he_sha512(data, (size_t)tlv_offset, digest);
            if (memcmp(digest, data + value_start, G6HE_SHA512_DIGEST_SIZE) ==
                0) {
                out->digest_verified = true;
            }
        } else if (type == G6HE_TLV_SHA256 || type == G6HE_TLV_SHA384) {
            /* Valid MCUboot digest types, not implemented in this tree. */
            return G6HE_ERR_UNSUPPORTED;
        } else if (type == G6HE_TLV_KEYHASH) {
            key_hash_count++;
            if (length != 32u && length != 48u && length != 64u) {
                return G6HE_ERR_TLV;
            }
            out->key_hash_present = true;
        } else if (is_sig_type(type)) {
            signature_count++;
            if (length == 0u) {
                return G6HE_ERR_SIGNATURE;
            }
            out->signature_present = true;
        } else if (type == G6HE_TLV_SIG_PURE && length == 1u) {
            out->sig_pure = (data[value_start] == 1u);
        }

        cursor = value_end;
    }

    if (digest_count != 1u) {
        return G6HE_ERR_DIGEST;
    }
    if (!out->digest_verified) {
        return G6HE_ERR_DIGEST;
    }
    if (key_hash_count != 1u) {
        return G6HE_ERR_SIGNATURE;
    }
    if (signature_count != 1u) {
        return G6HE_ERR_SIGNATURE;
    }

    if ((size_t)out->header.header_size + 8u <= (size_t)tlv_offset) {
        out->initial_sp = rd_u32(data + out->header.header_size);
        out->reset_vector = rd_u32(data + out->header.header_size + 4);
    }
    if (out->initial_sp == 0u || out->initial_sp == 0xFFFFFFFFu) {
        return G6HE_ERR_RANGE;
    }
    if (out->reset_vector == 0u || out->reset_vector == 0xFFFFFFFFu ||
        (out->reset_vector & 1u) == 0u) {
        return G6HE_ERR_RANGE;
    }
    return G6HE_OK;
}

void g6he_mcuboot_format_version(const g6he_image_header_t *header,
                                 char out[G6HE_VERSION_STRING_MAX])
{
    static const char digits[] = "0123456789";
    char tmp[G6HE_VERSION_STRING_MAX];
    size_t n = 0;
    size_t i;
    uint32_t values[4];

    if (header == NULL || out == NULL) {
        return;
    }
    values[0] = header->major;
    values[1] = header->minor;
    values[2] = header->revision;
    values[3] = header->build_number;

    for (i = 0; i < 4u; ++i) {
        char rev[10];
        size_t r = 0;
        uint32_t value = values[i];
        if (i != 0u) {
            tmp[n++] = (i == 3u) ? '+' : '.';
        }
        do {
            rev[r++] = digits[value % 10u];
            value /= 10u;
        } while (value != 0u && r < sizeof(rev));
        while (r > 0u && n + 1u < sizeof(tmp)) {
            tmp[n++] = rev[--r];
        }
    }
    tmp[n] = '\0';
    memcpy(out, tmp, n + 1u);
}

bool g6he_mcuboot_ram_load_ok(const g6he_image_header_t *header)
{
    uint64_t load;
    uint64_t image_end;

    if (header == NULL) {
        return false;
    }
    load = header->load_address;
    image_end = load + header->header_size + header->image_size;
    return load >= G6HE_RAM_START && image_end <= G6HE_RAM_END_EXCLUSIVE;
}

static const uint8_t *bounded_memmem(const uint8_t *haystack, size_t hay_len,
                                     const uint8_t *needle, size_t needle_len)
{
    size_t i;

    if (needle_len == 0u || hay_len < needle_len) {
        return NULL;
    }
    for (i = 0; i + needle_len <= hay_len; ++i) {
        if (haystack[i] == needle[0] &&
            memcmp(haystack + i, needle, needle_len) == 0) {
            return haystack + i;
        }
    }
    return NULL;
}

const uint8_t *g6he_mcuboot_find_string(const uint8_t *data, size_t len,
                                        const g6he_image_info_t *info,
                                        const char *needle)
{
    size_t payload_start;
    size_t payload_end;

    if (data == NULL || info == NULL || needle == NULL) {
        return NULL;
    }
    payload_start = info->header.header_size;
    payload_end = info->tlv_offset;
    if (payload_start > payload_end || payload_end > len) {
        return NULL;
    }
    return bounded_memmem(data + payload_start, payload_end - payload_start,
                          (const uint8_t *)needle, strlen(needle));
}

bool g6he_mcuboot_has_model(const uint8_t *data, size_t len,
                            const g6he_image_info_t *info, const char *model)
{
    return g6he_mcuboot_find_string(data, len, info, model) != NULL;
}

static bool parse_u32_dec(const char **cursor, uint32_t *out)
{
    const char *p = *cursor;
    uint32_t value = 0;
    bool any = false;

    while (*p >= '0' && *p <= '9') {
        value = (value * 10u) + (uint32_t)(*p - '0');
        any = true;
        ++p;
    }
    if (!any) {
        return false;
    }
    *cursor = p;
    *out = value;
    return true;
}

static bool parse_version(const char *text, uint32_t out[4])
{
    const char *p;

    if (text == NULL) {
        return false;
    }
    p = text;
    if (*p == 'v' || *p == 'V') {
        ++p;
    }
    if (!parse_u32_dec(&p, &out[0]) || *p != '.') {
        return false;
    }
    ++p;
    if (!parse_u32_dec(&p, &out[1]) || *p != '.') {
        return false;
    }
    ++p;
    if (!parse_u32_dec(&p, &out[2]) || *p != '+') {
        return false;
    }
    ++p;
    if (!parse_u32_dec(&p, &out[3]) || *p != '\0') {
        return false;
    }
    return true;
}

int g6he_version_compare(const char *current, const char *target)
{
    uint32_t a[4];
    uint32_t b[4];
    size_t i;

    if (!parse_version(current, a) || !parse_version(target, b)) {
        return 2; /* not comparable */
    }
    for (i = 0; i < 4u; ++i) {
        if (a[i] < b[i]) {
            return -1;
        }
        if (a[i] > b[i]) {
            return 1;
        }
    }
    return 0;
}

const char *g6he_version_relation(const char *current, const char *target)
{
    const int cmp = g6he_version_compare(current, target);

    if (cmp == 2) {
        return "unknown";
    }
    if (cmp == 0) {
        return "same";
    }
    return (cmp > 0) ? "downgrade" : "upgrade";
}
