/*
 * g6he_crc.c - checksum implementations (BR-010, BR-011).
 */
#include "g6he_crc.h"

uint16_t g6he_payload_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    size_t i;

    if (data == NULL) {
        return 0;
    }
    for (i = 0; i < len; ++i) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFFu);
}

uint32_t g6he_crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    size_t i;
    unsigned bit;

    if (data == NULL) {
        return crc;
    }
    for (i = 0; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit) {
            if (crc & 1u) {
                crc = (crc >> 1) ^ 0xEDB88320u;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

uint32_t g6he_crc32(const uint8_t *data, size_t len)
{
    return g6he_crc32_update(0xFFFFFFFFu, data, len);
}
