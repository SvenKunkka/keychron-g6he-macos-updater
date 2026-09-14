/*
 * g6he_hid.c - USB HID report model.
 */
#include "g6he_hid.h"

#include <string.h>

static int8_t clamp_i8(int32_t value)
{
    if (value > 127) {
        return 127;
    }
    if (value < -127) {
        return -127;
    }
    return (int8_t)value;
}

uint8_t g6he_hid_button_bit(g6he_button_id_t id)
{
    switch (id) {
    case G6HE_BTN_LEFT:
        return G6HE_HID_BUTTON_LEFT;
    case G6HE_BTN_RIGHT:
        return G6HE_HID_BUTTON_RIGHT;
    case G6HE_BTN_MIDDLE:
        return G6HE_HID_BUTTON_MIDDLE;
    case G6HE_BTN_SIDE_BACK:
        return G6HE_HID_BUTTON_BACK;
    case G6HE_BTN_SIDE_FORWARD:
        return G6HE_HID_BUTTON_FORWARD;
    case G6HE_BTN_DPI:
        return G6HE_HID_BUTTON_DPI;
    default:
        return 0u;
    }
}

bool g6he_hid_accumulate(g6he_hid_mouse_report_t *report,
                         const g6he_event_t *event)
{
    if (report == NULL || event == NULL) {
        return false;
    }
    switch (event->type) {
    case G6HE_EV_MOTION:
        report->dx = clamp_i8((int32_t)report->dx + event->u.motion.dx);
        report->dy = clamp_i8((int32_t)report->dy + event->u.motion.dy);
        return true;
    case G6HE_EV_WHEEL:
        report->wheel = clamp_i8((int32_t)report->wheel + event->u.wheel.delta);
        return true;
    case G6HE_EV_BUTTON: {
        const uint8_t bit = g6he_hid_button_bit((g6he_button_id_t)
                                                    event->u.button.id);
        if (bit == 0u) {
            return false;
        }
        if (event->u.button.pressed) {
            report->buttons |= bit;
        } else {
            report->buttons = (uint8_t)(report->buttons & (uint8_t)~bit);
        }
        return true;
    }
    default:
        return false;
    }
}

g6he_status_t g6he_hid_encode_boot(const g6he_hid_mouse_report_t *report,
                                   uint8_t out[G6HE_HID_BOOT_REPORT_SIZE])
{
    if (report == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    memset(out, 0, G6HE_HID_BOOT_REPORT_SIZE);
    out[0] = (uint8_t)(report->buttons & 0x07u); /* boot protocol: 3 buttons */
    out[1] = (uint8_t)report->dx;
    out[2] = (uint8_t)report->dy;
    out[3] = (uint8_t)report->wheel;
    return G6HE_OK;
}

bool g6he_hid_report_is_idle(const g6he_hid_mouse_report_t *report)
{
    return report != NULL && report->buttons == 0u && report->dx == 0 &&
           report->dy == 0 && report->wheel == 0;
}
