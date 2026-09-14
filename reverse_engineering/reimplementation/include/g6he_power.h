/*
 * g6he_power.h - nPM1300 power/charger interface.
 *
 * BR-023. On a target build the backend is expected to wrap the official
 * Zephyr/NCS nPM1300 drivers (`npm1300_charger`, `npm1300_regulator`,
 * `npm1300_fuel_gauge`) rather than a private register table. On the host the
 * backend is a mock.
 *
 * Fail-closed: charge current, termination voltage, rail voltages and NTC
 * parameters are UNKNOWN (see hardware_evidence/power_tree.md). They default to
 * G6HE_ELECTRICAL_UNKNOWN and a configuration containing any unknown required
 * value is rejected.
 */
#ifndef G6HE_POWER_H
#define G6HE_POWER_H

#include "g6he_common.h"
#include "g6he_board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    G6HE_BATT_OK = 0,
    G6HE_BATT_LOW = 1,      /* below G6HE_LOW_BATTERY_MV (3.4 V, SPEC) */
    G6HE_BATT_CRITICAL = 2  /* below G6HE_SHUTDOWN_BATTERY_MV (3.2 V, SPEC) */
} g6he_battery_class_t;

typedef struct {
    int32_t charge_current_ma;
    int32_t termination_voltage_mv;
    int32_t buck1_mv;
    int32_t buck2_mv;
    int32_t ldo1_mv;
    int32_t ldo2_mv;
    int32_t ntc_beta;
} g6he_power_config_t;

typedef struct {
    uint16_t battery_mv;
    uint8_t percent;
    bool charging;
    bool vbus_present;
    bool ntc_ok;
} g6he_power_state_t;

/* Backend returns 0 on success. */
typedef struct {
    int (*read_state)(void *ctx, g6he_power_state_t *out);
    int (*set_ship_mode)(void *ctx, bool enable);
    void *ctx;
} g6he_power_backend_t;

g6he_power_config_t g6he_power_config_unknown(void);

/* Rejects any required field that is still G6HE_ELECTRICAL_UNKNOWN. */
g6he_status_t g6he_power_validate_config(const g6he_power_config_t *config);

g6he_status_t g6he_power_bind(const g6he_power_backend_t *backend);
g6he_status_t g6he_power_read_state(g6he_power_state_t *out);
g6he_status_t g6he_power_enter_ship_mode(void);

/* Evidenced thresholds only; no electrical guess. */
g6he_battery_class_t g6he_power_classify(uint16_t battery_mv);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_POWER_H */
