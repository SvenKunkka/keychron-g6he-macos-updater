/*
 * g6he_board.h - board configuration and compile-time blockers.
 *
 * This is the ONLY place where a board pin or electrical constant may be
 * declared. Every value carries an evidence id and a confidence class. Until a
 * value is supported by a named artifact it is G6HE_PIN_UNKNOWN /
 * G6HE_ELECTRICAL_UNKNOWN, and enabling hardware in a target build then fails
 * at compile time instead of guessing.
 *
 * Evidence for the *existence* of each part is the G6_HE_V1.3 production
 * package (RAR1). Evidence for the pin mapping is NOT present: there is no
 * schematic, netlist, ball map or devicetree in any supplied package. See
 * hardware_evidence/hardware_unknowns.md (HW-U-010 .. HW-U-021).
 */
#ifndef G6HE_BOARD_H
#define G6HE_BOARD_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- fail-closed sentinels ------------------------------------------------ */

#define G6HE_PIN_UNKNOWN 0xFFFFFFFFu
#define G6HE_ELECTRICAL_UNKNOWN (-2147483647)

/*
 * Set to 1 only when every pin used by an enabled peripheral has a named
 * evidence id. It is 0 today: the production packages contain no pin evidence.
 */
#define G6HE_BOARD_PIN_TABLE_COMPLETE 0
#define G6HE_BOARD_ELECTRICAL_COMPLETE 0

/* ---- parts whose existence is evidenced (RAR1) ---------------------------- */

#define G6HE_BOARD_MCU "nRF54LM20A"        /* U1, CSP98 PAAA, HW-EV-U1 */
#define G6HE_BOARD_PMIC "nPM1300-CAAA"     /* U7, QFN-32,      HW-EV-U7 */
#define G6HE_BOARD_HALL "B1612Hall-10"     /* U8/U10,          HW-EV-U8 */
#define G6HE_BOARD_IRPT "IRPT-B01020001"   /* S1/S2,           HW-EV-S1 */
#define G6HE_BOARD_SENSOR_3950 "PAW3950"   /* U2 variant A,    HW-EV-U2A */
#define G6HE_BOARD_SENSOR_3955 "PAW3955"   /* U2 variant B,    HW-EV-U2B */

/* ---- pin assignments (all UNKNOWN) --------------------------------------- */

/* Sensor 4-wire SPI + motion + reset (sensor side is FACT; MCU side UNKNOWN). */
#define G6HE_PIN_SENSOR_SCLK G6HE_PIN_UNKNOWN   /* HW-U-011 */
#define G6HE_PIN_SENSOR_MOSI G6HE_PIN_UNKNOWN   /* HW-U-011 */
#define G6HE_PIN_SENSOR_MISO G6HE_PIN_UNKNOWN   /* HW-U-011 */
#define G6HE_PIN_SENSOR_NCS G6HE_PIN_UNKNOWN    /* HW-U-011 */
#define G6HE_PIN_SENSOR_MOTION G6HE_PIN_UNKNOWN /* HW-U-011 */
#define G6HE_PIN_SENSOR_NRESET G6HE_PIN_UNKNOWN /* HW-U-011 */

/* nPM1300 control (TWI + IRQ + ship/host control). */
#define G6HE_PIN_PMIC_SCL G6HE_PIN_UNKNOWN      /* HW-U-012 */
#define G6HE_PIN_PMIC_SDA G6HE_PIN_UNKNOWN      /* HW-U-012 */
#define G6HE_PIN_PMIC_IRQ G6HE_PIN_UNKNOWN      /* HW-U-012 */

/* Optomagnetic buttons: PT digital input and Hall analog input. */
#define G6HE_PIN_BTN_LEFT_IR G6HE_PIN_UNKNOWN   /* HW-U-013 */
#define G6HE_PIN_BTN_RIGHT_IR G6HE_PIN_UNKNOWN  /* HW-U-013 */
#define G6HE_ADC_BTN_LEFT_HALL G6HE_PIN_UNKNOWN /* HW-U-013 */
#define G6HE_ADC_BTN_RIGHT_HALL G6HE_PIN_UNKNOWN/* HW-U-013 */

/* Wheel encoder, mode switch and LEDs. */
#define G6HE_PIN_WHEEL_A G6HE_PIN_UNKNOWN       /* HW-U-016 */
#define G6HE_PIN_WHEEL_B G6HE_PIN_UNKNOWN       /* HW-U-016 */
#define G6HE_PIN_MODE_BT G6HE_PIN_UNKNOWN       /* HW-U-015 */
#define G6HE_PIN_MODE_CABLE G6HE_PIN_UNKNOWN    /* HW-U-015 */
#define G6HE_PIN_MODE_24G G6HE_PIN_UNKNOWN      /* HW-U-015 */
#define G6HE_PIN_LED_RGB G6HE_PIN_UNKNOWN       /* HW-U-017 */

/* ---- electrical constants (all UNKNOWN) ---------------------------------- */

#define G6HE_CHARGE_CURRENT_MA G6HE_ELECTRICAL_UNKNOWN   /* HW-U-030; SPEC ~250 mA, not a register value */
#define G6HE_TERM_VOLTAGE_MV G6HE_ELECTRICAL_UNKNOWN     /* HW-U-031 */
#define G6HE_BUCK1_MV G6HE_ELECTRICAL_UNKNOWN            /* HW-U-032 */
#define G6HE_BUCK2_MV G6HE_ELECTRICAL_UNKNOWN            /* HW-U-032 */
#define G6HE_LDO1_MV G6HE_ELECTRICAL_UNKNOWN             /* HW-U-032 */
#define G6HE_LDO2_MV G6HE_ELECTRICAL_UNKNOWN             /* HW-U-032 */
#define G6HE_NTC_BETA G6HE_ELECTRICAL_UNKNOWN            /* HW-U-033 */

/* Evidenced product requirements (not register values): SPEC/CFG. */
#define G6HE_BATTERY_CAPACITY_MAH 350                     /* SPEC row 22, CFG row 5 */
#define G6HE_LOW_BATTERY_MV 3400                          /* SPEC: red LED below 3.4 V */
#define G6HE_SHUTDOWN_BATTERY_MV 3200                     /* SPEC: auto off below 3.2 V */

/* ---- compile-time blockers ------------------------------------------------ */

#if defined(G6HE_ENABLE_HARDWARE) && (G6HE_ENABLE_HARDWARE == 1)
#if !G6HE_BOARD_PIN_TABLE_COMPLETE
#error "G6HE: hardware enabled but the board pin table is incomplete. \
Every GPIO/ADC assignment is UNKNOWN (no schematic/ball map). Refusing to guess. \
See hardware_evidence/hardware_unknowns.md HW-U-010..HW-U-021."
#endif
#if !G6HE_BOARD_ELECTRICAL_COMPLETE
#error "G6HE: hardware enabled but charge current / termination voltage / rail \
voltages are UNKNOWN. Refusing to program unsafe electrical constants. \
See hardware_evidence/power_tree.md HW-U-030..HW-U-034."
#endif
#endif /* G6HE_ENABLE_HARDWARE */

#ifdef __cplusplus
}
#endif

#endif /* G6HE_BOARD_H */
