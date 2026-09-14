/*
 * g6he_wireless.c - BLE HID / proprietary 2.4G mock state machines.
 */
#include "g6he_wireless.h"

#include <string.h>

g6he_status_t g6he_wireless_init(g6he_wireless_t *wl,
                                 g6he_wireless_link_t link)
{
    if (wl == NULL) {
        return G6HE_ERR_ARG;
    }
    if (link != G6HE_WL_LINK_BLE && link != G6HE_WL_LINK_PROPRIETARY_24G) {
        return G6HE_ERR_UNSUPPORTED;
    }
    memset(wl, 0, sizeof(*wl));
    wl->link = link;
    wl->state = G6HE_WL_OFF;
    return G6HE_OK;
}

g6he_status_t g6he_wireless_bind(g6he_wireless_t *wl,
                                 const g6he_wireless_backend_t *backend)
{
    if (wl == NULL || backend == NULL || backend->send_hid_report == NULL) {
        return G6HE_ERR_ARG;
    }
    wl->backend = *backend;
    wl->backend_bound = true;
    return G6HE_OK;
}

void g6he_wireless_on_pair_request(g6he_wireless_t *wl)
{
    if (wl == NULL) {
        return;
    }
    wl->state = G6HE_WL_PAIRING;
    wl->elapsed_ms = 0u;
    if (wl->backend_bound && wl->backend.start_pairing != NULL) {
        (void)wl->backend.start_pairing(wl->backend.ctx);
    }
}

void g6he_wireless_on_connected(g6he_wireless_t *wl)
{
    if (wl == NULL) {
        return;
    }
    wl->state = G6HE_WL_CONNECTED;
    wl->elapsed_ms = 0u;
}

void g6he_wireless_on_disconnected(g6he_wireless_t *wl)
{
    if (wl == NULL) {
        return;
    }
    wl->state = G6HE_WL_RECONNECTING;
    wl->elapsed_ms = 0u;
}

g6he_wireless_state_t g6he_wireless_tick(g6he_wireless_t *wl,
                                         uint32_t elapsed_ms)
{
    if (wl == NULL) {
        return G6HE_WL_OFF;
    }
    wl->elapsed_ms += elapsed_ms;
    if (wl->state == G6HE_WL_PAIRING &&
        wl->elapsed_ms >= G6HE_PAIRING_TIMEOUT_MS) {
        /* Documented: pairing failure enters sleep (all LEDs off). */
        wl->state = G6HE_WL_OFF;
        wl->elapsed_ms = 0u;
    }
    return wl->state;
}

g6he_status_t g6he_wireless_send_hid(g6he_wireless_t *wl,
                                     const uint8_t *report, size_t len)
{
    if (wl == NULL || report == NULL || len == 0u) {
        return G6HE_ERR_ARG;
    }
    if (!wl->backend_bound) {
        return G6HE_ERR_UNSUPPORTED;
    }
    if (wl->state != G6HE_WL_CONNECTED) {
        return G6HE_ERR_STATE;
    }
    return (wl->backend.send_hid_report(wl->backend.ctx, report, len) == 0)
               ? G6HE_OK
               : G6HE_ERR_STATE;
}
