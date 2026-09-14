/*
 * g6he_hall.c - Hall calibration interpolation (BR-014).
 */
#include "g6he_hall.h"

const uint16_t g6he_hall_breakpoints[G6HE_HALL_BREAKPOINTS] = {0u, 10u, 20u,
                                                               30u, 50u, 70u};

uint16_t g6he_hall_interpolate(const g6he_hall_curve_t *curve, uint8_t x)
{
    unsigned i;

    if (curve == NULL) {
        return 0;
    }
    if (x == 0u) {
        return 0;
    }
    if (x >= G6HE_HALL_INTERP_LIMIT) {
        return curve->y[G6HE_HALL_BREAKPOINTS - 1u];
    }
    for (i = 1; i < G6HE_HALL_BREAKPOINTS; ++i) {
        const uint16_t x_lo = g6he_hall_breakpoints[i - 1u];
        const uint16_t x_hi = g6he_hall_breakpoints[i];
        if (x <= x_hi) {
            const uint32_t delta = (uint32_t)x_hi - (uint32_t)x_lo;
            const uint16_t y_lo = curve->y[i - 1u];
            const uint16_t y_hi = curve->y[i];
            uint32_t numerator;
            uint32_t result;

            if (delta == 0u) {
                return y_hi;
            }
            numerator = (uint32_t)((uint32_t)x - (uint32_t)x_lo) *
                        (uint32_t)(uint16_t)(y_hi - y_lo);
            result = (uint32_t)y_lo + ((numerator + (delta >> 1)) / delta);
            return (uint16_t)(result & 0xFFFFu);
        }
    }
    return curve->y[G6HE_HALL_BREAKPOINTS - 1u];
}

uint16_t g6he_hall_derived_600(const g6he_hall_curve_t *curve, uint8_t offset)
{
    if (offset == 0u) {
        return 0u;
    }
    if (offset < G6HE_HALL_INTERP_LIMIT) {
        const int32_t s = (int32_t)g6he_hall_interpolate(
            curve, (uint8_t)(G6HE_HALL_INTERP_LIMIT - offset));
        return (uint16_t)((600 - s) & 0xFFFF);
    }
    return (uint16_t)G6HE_HALL_DERIVED_MAX;
}
