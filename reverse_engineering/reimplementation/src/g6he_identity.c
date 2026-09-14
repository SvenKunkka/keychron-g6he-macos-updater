/*
 * g6he_identity.c - identity/capability codecs (BR-006, BR-007, BR-017).
 */
#include "g6he_identity.h"

#include <string.h>

static void copy_fixed_string(char *dst, size_t dst_size, const uint8_t *src,
                              size_t src_len)
{
    size_t n = 0;

    while (n < src_len && src[n] != '\0') {
        ++n;
    }
    if (n >= dst_size) {
        n = dst_size - 1u;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

g6he_status_t g6he_identity_decode(const uint8_t *data, size_t len,
                                   g6he_identity_t *out)
{
    if (data == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (len < G6HE_IDENTITY_DATA_SIZE) {
        return G6HE_ERR_TRUNCATED;
    }
    memset(out, 0, sizeof(*out));
    copy_fixed_string(out->model, sizeof(out->model), data + 0, 10u);
    out->hardware_revision[0] = data[10];
    out->hardware_revision[1] = data[11];
    copy_fixed_string(out->firmware_version, sizeof(out->firmware_version),
                      data + 12, 10u);
    copy_fixed_string(out->bootloader_model, sizeof(out->bootloader_model),
                      data + 22, 10u);
    out->bootloader_version_major = data[32];
    out->bootloader_version_minor = data[33];
    return G6HE_OK;
}

g6he_status_t g6he_identity_encode(const g6he_identity_t *identity,
                                   uint8_t out[G6HE_IDENTITY_DATA_SIZE])
{
    if (identity == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    memset(out, 0, G6HE_IDENTITY_DATA_SIZE);
    memcpy(out + 0, identity->model, 10u);
    out[10] = identity->hardware_revision[0];
    out[11] = identity->hardware_revision[1];
    memcpy(out + 12, identity->firmware_version, 10u);
    memcpy(out + 22, identity->bootloader_model, 10u);
    out[32] = identity->bootloader_version_major;
    out[33] = identity->bootloader_version_minor;
    return G6HE_OK;
}

g6he_status_t g6he_capabilities_decode(const uint8_t *data, size_t len,
                                       g6he_capabilities_t *out)
{
    if (data == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (len < G6HE_CAPABILITY_DATA_SIZE) {
        return G6HE_ERR_TRUNCATED;
    }
    out->protocol_version = data[0];
    out->dfu_version = data[1];
    out->supported_update_modes = data[2];
    out->bootloader_required = data[3];
    return G6HE_OK;
}

g6he_status_t g6he_capabilities_encode(const g6he_capabilities_t *caps,
                                       uint8_t out[G6HE_CAPABILITY_DATA_SIZE])
{
    if (caps == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    out[0] = caps->protocol_version;
    out[1] = caps->dfu_version;
    out[2] = caps->supported_update_modes;
    out[3] = caps->bootloader_required;
    return G6HE_OK;
}

g6he_status_t g6he_kcfwid_extract(const uint8_t *data, size_t len,
                                  char model[G6HE_IDENTITY_FIELD_MAX],
                                  char version[G6HE_VERSION_FIELD_MAX])
{
    static const char prefix[] = "KCFWID";
    size_t cursor;

    if (data == NULL || model == NULL || version == NULL) {
        return G6HE_ERR_ARG;
    }
    if (len < sizeof(prefix) - 1u) {
        return G6HE_ERR_TRUNCATED;
    }
    if (memcmp(data, prefix, sizeof(prefix) - 1u) != 0) {
        return G6HE_ERR_MAGIC;
    }
    cursor = sizeof(prefix) - 1u;

    if (cursor + 2u > len || data[cursor] != 0x01u) {
        return G6HE_ERR_TLV;
    }
    {
        const uint8_t field_len = data[cursor + 1u];
        cursor += 2u;
        if (field_len == 0u || field_len > 10u || cursor + field_len > len) {
            return G6HE_ERR_TLV;
        }
        copy_fixed_string(model, G6HE_IDENTITY_FIELD_MAX, data + cursor,
                          field_len);
        cursor += field_len;
    }

    if (cursor + 2u > len || data[cursor] != 0x02u) {
        return G6HE_ERR_TLV;
    }
    {
        const uint8_t field_len = data[cursor + 1u];
        cursor += 2u;
        if (field_len == 0u || field_len >= G6HE_VERSION_FIELD_MAX ||
            cursor + field_len > len) {
            return G6HE_ERR_TLV;
        }
        copy_fixed_string(version, G6HE_VERSION_FIELD_MAX, data + cursor,
                          field_len);
        /* No further tag is decoded, so advancing `cursor` here was a dead
         * store (clang -analyzer deadcode.DeadStores). Tag 0x02 is the last
         * field read; trailing bytes of the blob are deliberately ignored, as
         * the captured KCFWID blob carries free-form strings after them. */
    }
    return G6HE_OK;
}
