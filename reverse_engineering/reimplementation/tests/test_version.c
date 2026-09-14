/*
 * test_version.c - BR-013 version comparison (mirrors updater F-005 handling).
 */
#include "g6he_test.h"

#include "g6he_mcuboot.h"

void test_version(void)
{
    g6he_image_header_t header;

    printf("[version]\n");
    CHECK_EQ_STR(g6he_version_relation("v1.2.3+7", "1.2.3+8"), "upgrade");
    CHECK_EQ_STR(g6he_version_relation("1.2.3+7", "v1.2.3+6"), "downgrade");
    CHECK_EQ_STR(g6he_version_relation("v1.2.3+7", "1.2.3+7"), "same");
    CHECK_EQ_STR(g6he_version_relation("1.0.0+84", "1.0.0+87"), "upgrade");
    CHECK_EQ_STR(g6he_version_relation("garbage", "1.0.0+1"), "unknown");
    CHECK_EQ_INT(g6he_version_compare("1.0.0+84", "1.0.0+84"), 0);
    CHECK_EQ_INT(g6he_version_compare("1.0.0+87", "1.0.0+84"), 1);
    CHECK_EQ_INT(g6he_version_compare("1.0.0+84", "1.0.0+87"), -1);
    CHECK_EQ_INT(g6he_version_compare("nope", "1.0.0+1"), 2);

    header.magic = G6HE_MCUBOOT_MAGIC;
    header.load_address = 0x20000800u;
    header.image_size = 348692u;
    header.flags = G6HE_MCUBOOT_RAM_LOAD_FLAG;
    header.header_size = 0x800u;
    header.protected_tlv_size = 0u;
    header.major = 1u;
    header.minor = 0u;
    header.revision = 0u;
    header.build_number = 84u;
    {
        char text[G6HE_VERSION_STRING_MAX];
        g6he_mcuboot_format_version(&header, text);
        CHECK_EQ_STR(text, "1.0.0+84");
    }
    header.build_number = 0u;
    header.revision = 0u;
    header.minor = 2u;
    header.major = 3u;
    {
        char text[G6HE_VERSION_STRING_MAX];
        g6he_mcuboot_format_version(&header, text);
        CHECK_EQ_STR(text, "3.2.0+0");
    }
}
