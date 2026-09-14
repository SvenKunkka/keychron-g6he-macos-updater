/*
 * g6he_hall.h - Hall-sensor calibration interpolation.
 *
 * BR-014. Recovered from FUN_2001ac20 and FUN_2001ada4 as decompiled in
 * ghidra_hall_math.txt / ghidra_custom_functions.txt.
 *
 * Recovered facts (FACT, from the decompilation):
 *   - The interpolation is piecewise linear over six ascending x breakpoints.
 *   - The breakpoint sequence is 0, 10, 20, 30, 50, 70 (bug_risk_audit V-003).
 *   - The divisor is the delta between adjacent breakpoints, so the compiled
 *     defaults never divide by zero (V-003).
 *   - Rounding adds (delta >> 1) before the integer division.
 *   - x == 0 returns 0; x >= 0x41 (65) returns the last y value (clamp).
 *   - FUN_2001ada4 derives a second value: 0 -> 0, 1..64 -> 600 - interp(65-x),
 *     >= 65 -> 600.
 *
 * UNKNOWN: the per-axis y tables, their physical units and axis assignment.
 * This module therefore takes the curve as a parameter; the compiled defaults
 * are not embedded because only their dimensions and a 12-value prefix were
 * recovered (see docs/unknowns.md, U-101).
 */
#ifndef G6HE_HALL_H
#define G6HE_HALL_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_HALL_BREAKPOINTS 6U
#define G6HE_HALL_INTERP_LIMIT 65U /* 0x41 */
#define G6HE_HALL_DERIVED_MAX 600U

extern const uint16_t g6he_hall_breakpoints[G6HE_HALL_BREAKPOINTS];

typedef struct {
    uint16_t y[G6HE_HALL_BREAKPOINTS];
} g6he_hall_curve_t;

/* Piecewise-linear interpolation with the recovered integer rounding. */
uint16_t g6he_hall_interpolate(const g6he_hall_curve_t *curve, uint8_t x);

/* Second value derived by FUN_2001ada4 from a stored offset byte. */
uint16_t g6he_hall_derived_600(const g6he_hall_curve_t *curve, uint8_t offset);

/*
 * Compiled default calibration tables in +84 and +87 (N-001/FACT dimensions):
 * two ascending float32 runs of 114 and 120 elements next to the hall/
 * settings keys. Only the dimensions and the first 12 values were transcribed;
 * the full bodies are deliberately not embedded (UNKNOWN).
 */
#define G6HE_HALL_TABLE0_ELEMENTS 114U
#define G6HE_HALL_TABLE1_ELEMENTS 120U

#ifdef __cplusplus
}
#endif

#endif /* G6HE_HALL_H */
