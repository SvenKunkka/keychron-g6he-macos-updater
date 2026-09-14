/*
 * test_main.c - runs every clean-room unit test.
 */
#include "g6he_test.h"

int g6he_test_checks = 0;
int g6he_test_failures = 0;

int main(void)
{
    printf("G6 HE clean-room behavioural test suite\n");
    test_crc();
    test_sha512();
    test_sha256();
    test_version();
    test_hall();
    test_defaults();
    test_identity();
    test_mcuboot();
    test_protocol();
    test_integration();
    test_target_arch();

    printf("\n%d checks, %d failures\n", g6he_test_checks, g6he_test_failures);
    if (g6he_test_failures != 0) {
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("RESULT: PASS\n");
    return 0;
}
