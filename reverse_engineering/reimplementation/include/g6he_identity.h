/*
 * g6he_identity.h - device identity and capability records.
 *
 * BR-006 (identity record), BR-007 (KCFWID blob), BR-017 (capabilities).
 *
 * Evidence:
 *   - E-006/FACT live read: model 54LMG6HE, hw release 0x0503, fw 1.0.0+82,
 *     bootloader model 54LMv1.0, bootloader version 1.0, protocol 1, DFU 0,
 *     update modes 0x01, no bootloader switch.
 *   - tests/test_firmware_parser.py captured a two-report 0x60 response whose
 *     34-byte identity field decodes to exactly that record.
 *   - T-002/FACT: the 0x60 response payload embeds the KCFWID identity blob.
 *
 * The KCFWID tag numbering (0x01 model, 0x02 version) is INFERENCE, 80: it is
 * read from the byte layout of the observed blob, and only those two tags are
 * decoded. Remaining bytes are left to the caller / UNKNOWN.
 */
#ifndef G6HE_IDENTITY_H
#define G6HE_IDENTITY_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_IDENTITY_DATA_SIZE 34U
#define G6HE_CAPABILITY_DATA_SIZE 4U
#define G6HE_IDENTITY_FIELD_MAX 11U /* 10 chars + NUL */
#define G6HE_VERSION_FIELD_MAX 16U

typedef struct {
    char model[G6HE_IDENTITY_FIELD_MAX];
    uint8_t hardware_revision[2];
    char firmware_version[G6HE_IDENTITY_FIELD_MAX];
    char bootloader_model[G6HE_IDENTITY_FIELD_MAX];
    uint8_t bootloader_version_major;
    uint8_t bootloader_version_minor;
} g6he_identity_t;

typedef struct {
    uint8_t protocol_version;
    uint8_t dfu_version;
    uint8_t supported_update_modes;
    uint8_t bootloader_required;
} g6he_capabilities_t;

g6he_status_t g6he_identity_decode(const uint8_t *data, size_t len,
                                   g6he_identity_t *out);
g6he_status_t g6he_identity_encode(const g6he_identity_t *identity,
                                   uint8_t out[G6HE_IDENTITY_DATA_SIZE]);

g6he_status_t g6he_capabilities_decode(const uint8_t *data, size_t len,
                                       g6he_capabilities_t *out);
g6he_status_t g6he_capabilities_encode(const g6he_capabilities_t *caps,
                                       uint8_t out[G6HE_CAPABILITY_DATA_SIZE]);

/*
 * Extract model and version from a "KCFWID" identity blob.
 * model must hold 11 bytes and version G6HE_VERSION_FIELD_MAX bytes.
 */
g6he_status_t g6he_kcfwid_extract(const uint8_t *data, size_t len,
                                  char model[G6HE_IDENTITY_FIELD_MAX],
                                  char version[G6HE_VERSION_FIELD_MAX]);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_IDENTITY_H */
