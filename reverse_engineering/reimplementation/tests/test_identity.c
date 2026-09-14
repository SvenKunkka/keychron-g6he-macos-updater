/*
 * test_identity.c - BR-006, BR-007, BR-017.
 *
 * Vectors: the 34-byte identity field captured from a real 0x60 response
 * (tests/test_firmware_parser.py) and the KCFWID blob recorded in T-002.
 */
#include "g6he_test.h"

#include "g6he_identity.h"

static const uint8_t captured_identity[34] = {
    0x35, 0x34, 0x4c, 0x4d, 0x47, 0x36, 0x48, 0x45, 0x00, 0x00, /* 54LMG6HE */
    0x05, 0x03,                                                 /* hw 0503 */
    0x31, 0x2e, 0x30, 0x2e, 0x30, 0x2b, 0x38, 0x32, 0x00, 0x00, /* 1.0.0+82 */
    0x35, 0x34, 0x4c, 0x4d, 0x76, 0x31, 0x2e, 0x30, 0x00, 0x00, /* 54LMv1.0 */
    0x01, 0x00};                                                /* 1.0 */

static const char kcfwid_blob[] =
    "KCFWID"
    "\x01\x08" "54LMG6HE"
    "\x02\x08" "1.0.0+87"
    "\x00" "54LMG6HE" "\x00" "54LMv1.0" "\x00" "Sep 14 2026" "\x00"
    "16:13:11" "\x00";

void test_identity(void)
{
    g6he_identity_t identity;
    uint8_t encoded[G6HE_IDENTITY_DATA_SIZE];
    char model[G6HE_IDENTITY_FIELD_MAX];
    char version[G6HE_VERSION_FIELD_MAX];
    g6he_capabilities_t caps;
    uint8_t cap_bytes[G6HE_CAPABILITY_DATA_SIZE];

    printf("[identity]\n");
    CHECK_EQ_INT(g6he_identity_decode(captured_identity,
                                      sizeof(captured_identity), &identity),
                 G6HE_OK);
    CHECK_EQ_STR(identity.model, "54LMG6HE");
    CHECK_EQ_INT(identity.hardware_revision[0], 0x05);
    CHECK_EQ_INT(identity.hardware_revision[1], 0x03);
    CHECK_EQ_STR(identity.firmware_version, "1.0.0+82");
    CHECK_EQ_STR(identity.bootloader_model, "54LMv1.0");
    CHECK_EQ_INT(identity.bootloader_version_major, 1);
    CHECK_EQ_INT(identity.bootloader_version_minor, 0);

    CHECK_EQ_INT(g6he_identity_encode(&identity, encoded), G6HE_OK);
    CHECK_MEM_EQ(encoded, captured_identity, sizeof(encoded));

    CHECK_EQ_INT(g6he_identity_decode(captured_identity, 10u, &identity),
                 G6HE_ERR_TRUNCATED);

    CHECK_EQ_INT(g6he_capabilities_decode(
                     (const uint8_t[]){1u, 0u, 0x01u, 0u}, 4u, &caps),
                 G6HE_OK);
    CHECK_EQ_INT(caps.protocol_version, 1);
    CHECK_EQ_INT(caps.dfu_version, 0);
    CHECK_EQ_INT(caps.supported_update_modes, 0x01);
    CHECK_EQ_INT(caps.bootloader_required, 0);
    CHECK_EQ_INT(g6he_capabilities_encode(&caps, cap_bytes), G6HE_OK);
    CHECK_EQ_INT(cap_bytes[2], 0x01);

    CHECK_EQ_INT(g6he_kcfwid_extract((const uint8_t *)kcfwid_blob,
                                     sizeof(kcfwid_blob) - 1u, model, version),
                 G6HE_OK);
    CHECK_EQ_STR(model, "54LMG6HE");
    CHECK_EQ_STR(version, "1.0.0+87");

    /* Regression for the dead-store fix in g6he_kcfwid_extract(): tag 0x02 is
     * the last field read, so an arbitrary trailing tail must not change the
     * extracted model/version. This locks in the fixed (unchanged) behaviour. */
    {
        static const char with_tail[] =
            "KCFWID"
            "\x01\x08" "54LMG6HE"
            "\x02\x08" "1.0.0+84"
            "\xaa\xbb\xcc trailing tail that is not a TLV";
        CHECK_EQ_INT(g6he_kcfwid_extract((const uint8_t *)with_tail,
                                         sizeof(with_tail) - 1u, model,
                                         version),
                     G6HE_OK);
        CHECK_EQ_STR(model, "54LMG6HE");
        CHECK_EQ_STR(version, "1.0.0+84");
    }

    CHECK_EQ_INT(g6he_kcfwid_extract((const uint8_t *)"NOPEQD", 6u, model,
                                     version),
                 G6HE_ERR_MAGIC);
}
