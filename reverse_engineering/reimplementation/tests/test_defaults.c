/*
 * test_defaults.c - BR-015 recovered factory defaults.
 */
#include "g6he_test.h"

#include "g6he_defaults.h"

void test_defaults(void)
{
    const g6he_defaults_t *d = g6he_defaults_factory();

    printf("[defaults]\n");
    CHECK(d != NULL);
    if (d == NULL) {
        return;
    }
    CHECK_EQ_INT(d->dpi_stages[0], 2100);
    CHECK_EQ_INT(d->dpi_stages[1], 700);
    CHECK_EQ_INT(d->dpi_stages[2], 1600);
    CHECK_EQ_INT(d->dpi_alt, 1300);
    CHECK_EQ_INT(d->timer_a, 30);
    CHECK_EQ_INT(d->timer_b, 25);
    CHECK_EQ_INT(d->mode, 2);
    CHECK_EQ_INT(d->flag_a, 1);
    CHECK_EQ_INT(G6HE_DEFAULT_DPI_STAGE0, 0x0834);
    CHECK_EQ_INT(G6HE_PERSIST_SENTINEL, 0xFFFF);
}
