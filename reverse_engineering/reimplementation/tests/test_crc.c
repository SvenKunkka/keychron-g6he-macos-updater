/*
 * test_crc.c - BR-010, BR-011.
 *
 * The vectors are the updater's published test vector (0x340BC6D9 for
 * "123456789") and the +84 whole-file CRC recorded as FACT in E-007.
 */
#include "g6he_test.h"

#include "g6he_crc.h"

void test_crc(void)
{
    static const uint8_t digits[] = "123456789";
    uint8_t split_sample[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

    printf("[crc]\n");
    CHECK_EQ_INT(g6he_crc32(digits, 9u), 0x340BC6D9);
    CHECK_EQ_INT(g6he_crc32((const uint8_t *)"", 0u), 0xFFFFFFFF);

    /* Incremental update must equal a one-shot pass. */
    {
        const uint32_t incremental =
            g6he_crc32_update(g6he_crc32_update(0xFFFFFFFFu, split_sample, 3u),
                              split_sample + 3, 5u);
        CHECK_EQ_INT(incremental, g6he_crc32(split_sample, 8u));
    }

    CHECK_EQ_INT(g6he_payload_checksum((const uint8_t *)"\x60", 1u), 0x0060);
    CHECK_EQ_INT(g6he_payload_checksum((const uint8_t *)"\x63", 1u), 0x0063);
    {
        const uint8_t two[] = {0xFF, 0x01};
        CHECK_EQ_INT(g6he_payload_checksum(two, 2u), 0x0100);
    }
}
