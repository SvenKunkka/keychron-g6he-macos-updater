/*
 * g6he_mcuboot.h - MCUboot container validation and version parsing.
 *
 * BR-001..BR-005, BR-013.
 *
 * Evidence:
 *   - E-002/FACT: header magic 0x96F3B83D, header 0x800, TLV magic 0x6907,
 *     digest over header+payload, one key-hash and one signature TLV.
 *   - C-001/C-002/FACT (+84/+87): load 0x20000800, flags 0x00000020
 *     (IMAGE_F_RAM_LOAD), image sizes 348692/349964, four TLVs
 *     SHA512/SIG_PURE/KEYHASH/ED25519.
 *   - V-001/V-002: initial SP and reset vector must be thumb-coded and inside
 *     the nRF54LM20A RAM window 0x20000000..0x20080000.
 *
 * The layout is the public MCUboot image header; TLV type numbers match
 * upstream mcu-tools/mcuboot boot/bootutil/include/bootutil/image.h.
 */
#ifndef G6HE_MCUBOOT_H
#define G6HE_MCUBOOT_H

#include "g6he_common.h"
#include "g6he_sha512.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_MCUBOOT_MAGIC 0x96F3B83DU
#define G6HE_MCUBOOT_TLV_MAGIC 0x6907U
#define G6HE_MCUBOOT_RAM_LOAD_FLAG 0x00000020U
#define G6HE_MCUBOOT_HEADER_MIN_SIZE 32U
#define G6HE_MCUBOOT_HEADER_MAX_SIZE 0x10000U

#define G6HE_RAM_START 0x20000000U
#define G6HE_RAM_END_EXCLUSIVE 0x20080000U

#define G6HE_TLV_KEYHASH 0x01U
#define G6HE_TLV_SHA256 0x10U
#define G6HE_TLV_SHA384 0x11U
#define G6HE_TLV_SHA512 0x12U
#define G6HE_TLV_RSA2048_PSS 0x20U
#define G6HE_TLV_ECDSA224 0x21U
#define G6HE_TLV_ECDSA256 0x22U
#define G6HE_TLV_RSA3072_PSS 0x23U
#define G6HE_TLV_ED25519 0x24U
#define G6HE_TLV_SIG_PURE 0x25U

#define G6HE_MAX_TLVS 12U
#define G6HE_VERSION_STRING_MAX 24U

typedef struct {
    uint32_t magic;
    uint32_t load_address;
    uint32_t image_size;
    uint32_t flags;
    uint16_t header_size;
    uint16_t protected_tlv_size;
    uint8_t major;
    uint8_t minor;
    uint16_t revision;
    uint32_t build_number;
} g6he_image_header_t;

typedef struct {
    uint32_t offset; /* absolute file offset of the TLV record */
    uint8_t type;
    uint8_t pad;
    uint16_t length;
} g6he_tlv_t;

typedef struct {
    g6he_image_header_t header;
    uint32_t tlv_offset;
    uint16_t tlv_total;
    g6he_tlv_t tlvs[G6HE_MAX_TLVS];
    size_t tlv_count;
    uint32_t initial_sp;
    uint32_t reset_vector;
    bool digest_verified;
    bool key_hash_present;
    bool signature_present;
    bool sig_pure;
} g6he_image_info_t;

/*
 * Parse and validate an MCUboot image.
 *
 * Enforces: magic, header-size bounds/alignment, non-zero load address,
 * unsupported protected-TLV region, TLV area ending exactly at end of buffer,
 * exactly one supported digest TLV (SHA-512) that recomputes, exactly one
 * key-hash and one signature TLV, and a sane vector table.
 *
 * FACT/F-001: the host parser must fail closed on malformed or unsigned
 * images. Unlike the v1.0.0 updater audited as F-001, this implementation
 * requires exactly one digest, key-hash and signature TLV.
 */
g6he_status_t g6he_mcuboot_parse(const uint8_t *data, size_t len,
                                 g6he_image_info_t *out);

/* Format "major.minor.revision+build" into out (needs G6HE_VERSION_STRING_MAX). */
void g6he_mcuboot_format_version(const g6he_image_header_t *header,
                                 char out[G6HE_VERSION_STRING_MAX]);

/* True when the image load range lies inside the nRF54LM20A RAM window. */
bool g6he_mcuboot_ram_load_ok(const g6he_image_header_t *header);

/* Search the payload region (header_size .. tlv_offset) for a NUL-terminated
 * ASCII string. Returns a pointer into data or NULL. */
const uint8_t *g6he_mcuboot_find_string(const uint8_t *data, size_t len,
                                        const g6he_image_info_t *info,
                                        const char *needle);
bool g6he_mcuboot_has_model(const uint8_t *data, size_t len,
                            const g6he_image_info_t *info, const char *model);

/*
 * Parse and compare "v?X.Y.Z+B" versions.
 * Returns -1/0/1, or 2 when either side cannot be parsed.
 */
int g6he_version_compare(const char *current, const char *target);
/* "upgrade" | "same" | "downgrade" | "unknown" (BR-013/F-005). */
const char *g6he_version_relation(const char *current, const char *target);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_MCUBOOT_H */
