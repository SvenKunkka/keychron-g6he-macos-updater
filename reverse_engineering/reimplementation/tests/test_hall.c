/*
 * test_hall.c - BR-014 Hall interpolation.
 *
 * With the recovered breakpoints the interpolation should reproduce the
 * identity ramp exactly, clamp above 65, and round half up.
 */
#include "g6he_test.h"

#include "g6he_hall.h"

void test_hall(void)
{
    const g6he_hall_curve_t identity = {
        {0u, 10u, 20u, 30u, 50u, 70u}};
    const g6he_hall_curve_t scaled = {
        {0u, 100u, 200u, 300u, 500u, 600u}};
    const g6he_hall_curve_t half = {{0u, 1u, 1u, 1u, 1u, 1u}};
    unsigned x;

    printf("[hall]\n");
    CHECK_EQ_INT(g6he_hall_breakpoints[0], 0);
    CHECK_EQ_INT(g6he_hall_breakpoints[1], 10);
    CHECK_EQ_INT(g6he_hall_breakpoints[2], 20);
    CHECK_EQ_INT(g6he_hall_breakpoints[3], 30);
    CHECK_EQ_INT(g6he_hall_breakpoints[4], 50);
    CHECK_EQ_INT(g6he_hall_breakpoints[5], 70);

    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 0u), 0);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 5u), 5);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 15u), 15);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 25u), 25);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 40u), 40);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 60u), 60);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 65u), 70);
    CHECK_EQ_INT(g6he_hall_interpolate(&identity, 200u), 70);

    /* Half-up rounding: (4*1 + 5)/10 == 0, (5*1 + 5)/10 == 1. */
    CHECK_EQ_INT(g6he_hall_interpolate(&half, 4u), 0);
    CHECK_EQ_INT(g6he_hall_interpolate(&half, 5u), 1);

    /* Monotonic and within range for every legal x. */
    {
        uint16_t previous = 0;
        int ok = 1;
        for (x = 0; x <= 70u; ++x) {
            const uint16_t value = g6he_hall_interpolate(&scaled, (uint8_t)x);
            if (value < previous) {
                ok = 0;
            }
            previous = value;
        }
        CHECK(ok);
        CHECK_EQ_INT(g6he_hall_interpolate(&scaled, 70u), 600);
    }

    /* Derived value from FUN_2001ada4. */
    CHECK_EQ_INT(g6he_hall_derived_600(&identity, 0u), 0);
    CHECK_EQ_INT(g6he_hall_derived_600(&identity, 65u), 600);
    CHECK_EQ_INT(g6he_hall_derived_600(&identity, 100u), 600);
    CHECK_EQ_INT(g6he_hall_derived_600(&identity, 64u), 599);

    CHECK_EQ_INT(g6he_hall_interpolate(NULL, 10u), 0);
}
