/*
 * g6he_power.c - nPM1300 power/charger interface (fail-closed).
 */
#include "g6he_power.h"

#include <string.h>

static g6he_power_backend_t g_backend;
static bool g_bound;

g6he_power_config_t g6he_power_config_unknown(void)
{
    g6he_power_config_t cfg;

    cfg.charge_current_ma = G6HE_ELECTRICAL_UNKNOWN;
    cfg.termination_voltage_mv = G6HE_ELECTRICAL_UNKNOWN;
    cfg.buck1_mv = G6HE_ELECTRICAL_UNKNOWN;
    cfg.buck2_mv = G6HE_ELECTRICAL_UNKNOWN;
    cfg.ldo1_mv = G6HE_ELECTRICAL_UNKNOWN;
    cfg.ldo2_mv = G6HE_ELECTRICAL_UNKNOWN;
    cfg.ntc_beta = G6HE_ELECTRICAL_UNKNOWN;
    return cfg;
}

g6he_status_t g6he_power_validate_config(const g6he_power_config_t *config)
{
    if (config == NULL) {
        return G6HE_ERR_ARG;
    }
    if (config->charge_current_ma == G6HE_ELECTRICAL_UNKNOWN ||
        config->termination_voltage_mv == G6HE_ELECTRICAL_UNKNOWN ||
        config->buck1_mv == G6HE_ELECTRICAL_UNKNOWN ||
        config->buck2_mv == G6HE_ELECTRICAL_UNKNOWN ||
        config->ldo1_mv == G6HE_ELECTRICAL_UNKNOWN ||
        config->ldo2_mv == G6HE_ELECTRICAL_UNKNOWN ||
        config->ntc_beta == G6HE_ELECTRICAL_UNKNOWN) {
        return G6HE_ERR_UNSUPPORTED;
    }
    return G6HE_OK;
}

g6he_status_t g6he_power_bind(const g6he_power_backend_t *backend)
{
    if (backend == NULL || backend->read_state == NULL) {
        return G6HE_ERR_ARG;
    }
    g_backend = *backend;
    g_bound = true;
    return G6HE_OK;
}

g6he_status_t g6he_power_read_state(g6he_power_state_t *out)
{
    if (out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (!g_bound) {
        return G6HE_ERR_UNSUPPORTED; /* no PMIC backend on this build */
    }
    memset(out, 0, sizeof(*out));
    return (g_backend.read_state(g_backend.ctx, out) == 0) ? G6HE_OK
                                                           : G6HE_ERR_STATE;
}

g6he_status_t g6he_power_enter_ship_mode(void)
{
    if (!g_bound || g_backend.set_ship_mode == NULL) {
        return G6HE_ERR_UNSUPPORTED;
    }
    return (g_backend.set_ship_mode(g_backend.ctx, true) == 0)
               ? G6HE_OK
               : G6HE_ERR_STATE;
}

g6he_battery_class_t g6he_power_classify(uint16_t battery_mv)
{
    if (battery_mv < G6HE_SHUTDOWN_BATTERY_MV) {
        return G6HE_BATT_CRITICAL;
    }
    if (battery_mv < G6HE_LOW_BATTERY_MV) {
        return G6HE_BATT_LOW;
    }
    return G6HE_BATT_OK;
}
