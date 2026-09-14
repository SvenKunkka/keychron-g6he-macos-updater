/*
 * g6he_test.h - minimal dependency-free test harness.
 */
#ifndef G6HE_TEST_H
#define G6HE_TEST_H

#include <stdio.h>
#include <string.h>

extern int g6he_test_checks;
extern int g6he_test_failures;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g6he_test_checks++;                                                \
        if (!(cond)) {                                                     \
            g6he_test_failures++;                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                 \
    do {                                                                   \
        long long chk_a = (long long)(a);                                  \
        long long chk_b = (long long)(b);                                  \
        g6he_test_checks++;                                                \
        if (chk_a != chk_b) {                                              \
            g6he_test_failures++;                                          \
            printf("  FAIL %s:%d: %s=%lld != %s=%lld\n", __FILE__,         \
                   __LINE__, #a, chk_a, #b, chk_b);                        \
        }                                                                  \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                 \
    do {                                                                   \
        const char *chk_a = (a);                                           \
        const char *chk_b = (b);                                           \
        g6he_test_checks++;                                                \
        if (chk_a == NULL || chk_b == NULL || strcmp(chk_a, chk_b) != 0) { \
            g6he_test_failures++;                                          \
            printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, \
                   chk_a ? chk_a : "(null)", chk_b ? chk_b : "(null)");   \
        }                                                                  \
    } while (0)

#define CHECK_MEM_EQ(a, b, n)                                              \
    do {                                                                   \
        g6he_test_checks++;                                                \
        if (memcmp((a), (b), (n)) != 0) {                                  \
            g6he_test_failures++;                                          \
            printf("  FAIL %s:%d: memory differs over %u bytes\n",         \
                   __FILE__, __LINE__, (unsigned)(n));                     \
        }                                                                  \
    } while (0)

void test_crc(void);
void test_sha512(void);
void test_sha256(void);
void test_version(void);
void test_hall(void);
void test_defaults(void);
void test_identity(void);
void test_mcuboot(void);
void test_protocol(void);
void test_integration(void);
void test_target_arch(void);

#endif /* G6HE_TEST_H */
