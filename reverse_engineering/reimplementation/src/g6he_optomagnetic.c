/*
 * g6he_optomagnetic.c - raw Hall / IR / fusion / debounce / HID output.
 */
#include "g6he_optomagnetic.h"

#include <string.h>

g6he_om_config_t g6he_optomagnetic_config_disabled(void)
{
    g6he_om_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.enable_ir = false;
    cfg.enable_hall = false;
    return cfg;
}

g6he_status_t g6he_optomagnetic_init(g6he_om_button_t *button,
                                     g6he_button_id_t id,
                                     const g6he_om_config_t *config)
{
    if (button == NULL || config == NULL) {
        return G6HE_ERR_ARG;
    }
    if (!config->enable_ir && !config->enable_hall) {
        return G6HE_ERR_UNSUPPORTED;
    }
    if (config->enable_hall &&
        config->hall_press_threshold <= config->hall_release_threshold) {
        return G6HE_ERR_RANGE; /* no hysteresis: reject rather than guess */
    }
    memset(button, 0, sizeof(*button));
    button->cfg = *config;
    button->id = id;
    button->ir_closed = false;
    button->fused = false;
    return G6HE_OK;
}

g6he_status_t g6he_optomagnetic_feed_hall_raw(g6he_om_button_t *button,
                                              uint16_t raw)
{
    if (button == NULL) {
        return G6HE_ERR_ARG;
    }
    if (!button->cfg.enable_hall) {
        return G6HE_ERR_UNSUPPORTED;
    }
    button->hall_raw = raw;
    if (raw >= button->cfg.hall_press_threshold) {
        button->hall_pressed = true;
    } else if (raw <= button->cfg.hall_release_threshold) {
        button->hall_pressed = false;
    }
    return G6HE_OK;
}

g6he_status_t g6he_optomagnetic_feed_ir(g6he_om_button_t *button,
                                        bool contact_closed)
{
    if (button == NULL) {
        return G6HE_ERR_ARG;
    }
    if (!button->cfg.enable_ir) {
        return G6HE_ERR_UNSUPPORTED;
    }
    button->ir_closed = button->cfg.ir_active_high ? contact_closed
                                                   : !contact_closed;
    return G6HE_OK;
}

g6he_status_t g6he_optomagnetic_poll(g6he_om_button_t *button, uint16_t dt_ms,
                                     uint32_t now_ms)
{
    bool target;

    if (button == NULL) {
        return G6HE_ERR_ARG;
    }
    if (!button->cfg.enable_ir && !button->cfg.enable_hall) {
        return G6HE_ERR_UNSUPPORTED;
    }

    target = false;
    if (button->cfg.enable_ir && button->ir_closed) {
        target = true;
    }
    if (button->cfg.enable_hall && button->hall_pressed) {
        target = true;
    }

    if (target == button->fused) {
        button->candidate = target;
        button->candidate_ms = 0u;
        return G6HE_OK;
    }
    if (target != button->candidate) {
        button->candidate = target;
        button->candidate_ms = 0u;
    }
    button->candidate_ms =
        (uint16_t)(button->candidate_ms + dt_ms);
    if (button->candidate_ms >= button->cfg.debounce_ms) {
        button->fused = target;
        button->candidate_ms = 0u;
        button->pending = g6he_event_button(
            now_ms, button->id, button->fused, button->hall_raw,
            button->cfg.enable_hall);
        button->event_pending = true;
    }
    return G6HE_OK;
}

bool g6he_optomagnetic_take_event(g6he_om_button_t *button, g6he_event_t *out)
{
    if (button == NULL || out == NULL || !button->event_pending) {
        return false;
    }
    *out = button->pending;
    button->event_pending = false;
    return true;
}

uint16_t g6he_optomagnetic_hall_raw(const g6he_om_button_t *button)
{
    return (button == NULL || !button->cfg.enable_hall) ? 0u
                                                        : button->hall_raw;
}
