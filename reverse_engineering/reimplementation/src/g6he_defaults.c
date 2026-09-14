/*
 * g6he_defaults.c - recovered factory defaults (BR-015).
 */
#include "g6he_defaults.h"

static const g6he_defaults_t k_factory_defaults = {
    .dpi_stages = {G6HE_DEFAULT_DPI_STAGE0, G6HE_DEFAULT_DPI_STAGE1,
                   G6HE_DEFAULT_DPI_STAGE2},
    .dpi_alt = G6HE_DEFAULT_DPI_ALT,
    .timer_a = G6HE_DEFAULT_TIMER_A,
    .timer_b = G6HE_DEFAULT_TIMER_B,
    .mode = G6HE_DEFAULT_MODE,
    .flag_a = G6HE_DEFAULT_FLAG_A,
};

const g6he_defaults_t *g6he_defaults_factory(void)
{
    return &k_factory_defaults;
}
