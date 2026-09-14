/*
 * g6he_wireless.h - BLE HID and proprietary 2.4G interfaces (mock/stub).
 *
 * BR-026. The radio behaviour is NOT at evidence level L4: the update protocol
 * exposes no BLE parameters, no 2.4G channel/pairing registers and no RF
 * configuration. Therefore:
 *
 *  - the BLE HID path is a stub that refuses to operate without a bound backend;
 *  - the proprietary 2.4G pairing/reconnect state machine implements only the
 *    documented *product* timing (SPEC: pairing slow-blink for 3 min, reconnect
 *    fast-blink at 0.5 s, sleep after 10 min idle) as a host-testable mock.
 *
 * No RF parameter, address, channel or TX power is defined here.
 */
#ifndef G6HE_WIRELESS_H
#define G6HE_WIRELESS_H

#include "g6he_common.h"
#include "g6he_hid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Documented product timings (SPEC), milliseconds. */
#define G6HE_PAIRING_TIMEOUT_MS 180000U /* 3 min */
#define G6HE_RECONNECT_BLINK_MS 500U
#define G6HE_IDLE_SLEEP_MS 600000U /* 10 min, wireless modes only */

typedef enum {
    G6HE_WL_OFF = 0,
    G6HE_WL_PAIRING = 1,
    G6HE_WL_RECONNECTING = 2,
    G6HE_WL_CONNECTED = 3
} g6he_wireless_state_t;

typedef enum {
    G6HE_WL_LINK_BLE = 0,
    G6HE_WL_LINK_PROPRIETARY_24G = 1
} g6he_wireless_link_t;

/* Backend returns 0 on success. A NULL backend means "no radio in this build". */
typedef struct {
    int (*send_hid_report)(void *ctx, const uint8_t *report, size_t len);
    int (*start_pairing)(void *ctx);
    void *ctx;
} g6he_wireless_backend_t;

typedef struct {
    g6he_wireless_link_t link;
    g6he_wireless_state_t state;
    uint32_t elapsed_ms;
    bool backend_bound;
    g6he_wireless_backend_t backend;
} g6he_wireless_t;

g6he_status_t g6he_wireless_init(g6he_wireless_t *wl,
                                 g6he_wireless_link_t link);
g6he_status_t g6he_wireless_bind(g6he_wireless_t *wl,
                                 const g6he_wireless_backend_t *backend);

/* Advance the pairing/reconnect mock. Returns the new state. */
g6he_wireless_state_t g6he_wireless_tick(g6he_wireless_t *wl,
                                         uint32_t elapsed_ms);

/* Event-driven transitions used by the mock and by tests. */
void g6he_wireless_on_pair_request(g6he_wireless_t *wl);
void g6he_wireless_on_connected(g6he_wireless_t *wl);
void g6he_wireless_on_disconnected(g6he_wireless_t *wl);

/* Refuses unless a backend is bound. */
g6he_status_t g6he_wireless_send_hid(g6he_wireless_t *wl,
                                     const uint8_t *report, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_WIRELESS_H */
