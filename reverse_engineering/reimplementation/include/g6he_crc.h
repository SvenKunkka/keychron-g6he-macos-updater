/*
 * g6he_crc.h - checksums used by the G6 HE update path.
 *
 * BR-010 (payload checksum) and BR-011 (streaming CRC-32).
 *
 * Evidence:
 *   - E-004/FACT: payload checksum is the little-endian 16-bit sum of payload
 *     bytes and is appended after the payload inside the HID frame.
 *   - E-005/FACT + E-007/FACT: the file/streaming CRC is the reflected CRC-32
 *     polynomial 0xEDB88320, initial value 0xFFFFFFFF, no final XOR.
 */
#ifndef G6HE_CRC_H
#define G6HE_CRC_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 16-bit little-endian sum of bytes, truncated to 16 bits. */
uint16_t g6he_payload_checksum(const uint8_t *data, size_t len);

/* Reflected CRC-32, polynomial 0xEDB88320, no final XOR.
 *
 * g6he_crc32_update() continues a running CRC. g6he_crc32() starts from the
 * updater's initial value 0xFFFFFFFF.
 */
uint32_t g6he_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t g6he_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_CRC_H */
