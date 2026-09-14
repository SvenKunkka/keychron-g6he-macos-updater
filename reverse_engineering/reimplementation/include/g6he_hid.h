/*
 * g6he_hid.h - USB HID report model.
 *
 * BR-025. Two report families are modelled:
 *
 *  1. The standard boot-mouse report (4 bytes: buttons, dx, dy, wheel) used for
 *     HID input.
 *  2. The Keychron firmware-update reports already modelled in g6he_protocol.h
 *     (output report ID 0xB2, input 0xB1, 33-byte reports, usage page 0x008c).
 *     Those constants are FACT from the update-protocol descriptor present in
 *     both +84 and +87 (verified by test_integration).
 */
#ifndef G6HE_HID_H
#define G6HE_HID_H

#include "g6he_common.h"
#include "g6he_event.h"
#include "g6he_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* USB HID usage page of the vendor update interface (FACT). */
#define G6HE_HID_UPDATE_USAGE_PAGE 0x008CU
#define G6HE_HID_REPORT_ID_OUT G6HE_REPORT_ID_OUT /* 0xB2 */
#define G6HE_HID_REPORT_ID_IN G6HE_REPORT_ID_IN   /* 0xB1 */

#define G6HE_HID_BOOT_REPORT_SIZE 4U

#define G6HE_HID_BUTTON_LEFT 0x01U
#define G6HE_HID_BUTTON_RIGHT 0x02U
#define G6HE_HID_BUTTON_MIDDLE 0x04U
#define G6HE_HID_BUTTON_BACK 0x08U
#define G6HE_HID_BUTTON_FORWARD 0x10U
#define G6HE_HID_BUTTON_DPI 0x20U

typedef struct {
    uint8_t buttons;
    int8_t dx;
    int8_t dy;
    int8_t wheel;
} g6he_hid_mouse_report_t;

/* Map a button id to its HID bit; 0 for unknown. */
uint8_t g6he_hid_button_bit(g6he_button_id_t id);

/*
 * Accumulate one input event into a report. Motion and wheel are clamped to the
 * int8 range; a button event sets or clears its bit. Returns true when the
 * report changed. Battery/mode events are not part of the mouse report.
 */
bool g6he_hid_accumulate(g6he_hid_mouse_report_t *report,
                         const g6he_event_t *event);

/* Serialise to the 4-byte boot report. */
g6he_status_t g6he_hid_encode_boot(const g6he_hid_mouse_report_t *report,
                                   uint8_t out[G6HE_HID_BOOT_REPORT_SIZE]);

/* True when the report carries no state change at all. */
bool g6he_hid_report_is_idle(const g6he_hid_mouse_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_HID_H */
