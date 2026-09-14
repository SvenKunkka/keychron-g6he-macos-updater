/*
 * g6he_defaults.h - recovered factory default parameters.
 *
 * BR-015. Constants recovered from FUN_2001ba14, FUN_2001ada4 and FUN_2003fd64
 * (ghidra_custom_functions.txt / ghidra_callers.txt).
 *
 * FACT (values): the decompiled initialisers store exactly these literals.
 * INFERENCE (role): the 0x834/700/0x640 group is read as a three-stage DPI
 * (CPI) table because it repeats in two per-axis blocks and is written next to
 * the per-axis DPI structure; 0x514 (1300) appears as a separate DPI value.
 * The labels below are reconstructed, not original vendor names.
 *
 * UNKNOWN: units, which stage is active at boot, per-axis differences, and the
 * meaning of the timer/flag fields.
 */
#ifndef G6HE_DEFAULTS_H
#define G6HE_DEFAULTS_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_DEFAULT_DPI_STAGE0 0x0834u /* 2100 */
#define G6HE_DEFAULT_DPI_STAGE1 700u
#define G6HE_DEFAULT_DPI_STAGE2 0x0640u /* 1600 */
#define G6HE_DEFAULT_DPI_ALT 0x0514u    /* 1300 */
#define G6HE_DEFAULT_TIMER_A 0x001Eu    /* 30 */
#define G6HE_DEFAULT_TIMER_B 0x0019u    /* 25 */
#define G6HE_DEFAULT_MODE 2u
#define G6HE_DEFAULT_FLAG_A 1u
#define G6HE_PERSIST_SENTINEL 0xFFFFu

#define G6HE_DPI_STAGE_COUNT 3U

typedef struct {
    uint16_t dpi_stages[G6HE_DPI_STAGE_COUNT];
    uint16_t dpi_alt;
    uint16_t timer_a;
    uint16_t timer_b;
    uint16_t mode;
    uint16_t flag_a;
} g6he_defaults_t;

/* Returns a pointer to the recovered factory default set (static storage). */
const g6he_defaults_t *g6he_defaults_factory(void);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_DEFAULTS_H */
