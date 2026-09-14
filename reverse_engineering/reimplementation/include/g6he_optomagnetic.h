/*
 * g6he_optomagnetic.h - optomagnetic button chain.
 *
 * BR-021. Stages are separated exactly as the hardware does:
 *
 *   raw Hall  ->  IR/contact  ->  fusion  ->  debounce  ->  HID output
 *
 * Evidence (vendor optomagnetic switch specification + B1612Hall datasheet):
 * the switch contains BOTH a reed-grating phototransistor contact and a moving
 * magnet sensed by an analog linear Hall. The IR/PT path is digital, the Hall
 * path is analog. See hardware_evidence/optomagnetic_input_path.md.
 *
 * Fail-closed: the electrical thresholds and the IR drive are UNKNOWN. A button
 * is disabled unless the caller supplies a calibration; there is no guessed
 * default threshold.
 */
#ifndef G6HE_HALL_OPTO_H
#define G6HE_HALL_OPTO_H

#include "g6he_common.h"
#include "g6he_event.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t hall_press_threshold;   /* raw sample >= this is "pressed" */
    uint16_t hall_release_threshold; /* raw sample <= this is "released" */
    uint16_t debounce_ms;
    bool enable_ir;                  /* use the phototransistor contact */
    bool enable_hall;                /* use the analog Hall channel */
    bool ir_active_high;             /* true: contact closed reads high */
} g6he_om_config_t;

/* Disabled, threshold-free configuration. Applying it yields G6HE_ERR_UNSUPPORTED. */
g6he_om_config_t g6he_optomagnetic_config_disabled(void);

typedef struct {
    g6he_om_config_t cfg;
    g6he_button_id_t id;
    uint16_t hall_raw;
    bool ir_closed;         /* logical: switch contact closed */
    bool hall_pressed;
    bool fused;             /* debounced output state */
    bool candidate;         /* pending debounced state */
    uint16_t candidate_ms;
    bool event_pending;
    g6he_event_t pending;
} g6he_om_button_t;

g6he_status_t g6he_optomagnetic_init(g6he_om_button_t *button,
                                     g6he_button_id_t id,
                                     const g6he_om_config_t *config);

g6he_status_t g6he_optomagnetic_feed_hall_raw(g6he_om_button_t *button,
                                              uint16_t raw);
/* contact: true when the phototransistor path reports the switch closed. */
g6he_status_t g6he_optomagnetic_feed_ir(g6he_om_button_t *button,
                                        bool contact_closed);

/* Advance the debounce timer. dt_ms is the elapsed time since the last poll. */
g6he_status_t g6he_optomagnetic_poll(g6he_om_button_t *button, uint16_t dt_ms,
                                     uint32_t now_ms);

bool g6he_optomagnetic_take_event(g6he_om_button_t *button, g6he_event_t *out);

/* Raw Hall reading last supplied (0 when the Hall channel is disabled). */
uint16_t g6he_optomagnetic_hall_raw(const g6he_om_button_t *button);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_HALL_OPTO_H */
