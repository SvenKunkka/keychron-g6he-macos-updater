/*
 * g6he_event.h - common event model for motion, wheel, buttons and mode.
 *
 * BR-020. All input paths normalise to one timestamped event type so the HID
 * and wireless layers do not depend on any particular driver.
 */
#ifndef G6HE_EVENT_H
#define G6HE_EVENT_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_EVENT_QUEUE_CAPACITY 32U

typedef enum {
    G6HE_EV_NONE = 0,
    G6HE_EV_MOTION = 1,
    G6HE_EV_BUTTON = 2,
    G6HE_EV_WHEEL = 3,
    G6HE_EV_MODE = 4,
    G6HE_EV_BATTERY = 5
} g6he_event_type_t;

typedef enum {
    G6HE_BTN_LEFT = 0,
    G6HE_BTN_RIGHT = 1,
    G6HE_BTN_MIDDLE = 2,
    G6HE_BTN_SIDE_FORWARD = 3,
    G6HE_BTN_SIDE_BACK = 4,
    G6HE_BTN_DPI = 5,
    G6HE_BTN_COUNT = 6
} g6he_button_id_t;

typedef enum {
    G6HE_MODE_BT = 0,
    G6HE_MODE_CABLE = 1,
    G6HE_MODE_24G = 2,
    G6HE_MODE_COUNT = 3
} g6he_mode_t;

typedef struct {
    uint32_t timestamp_ms;
    g6he_event_type_t type;
    union {
        struct {
            int16_t dx;
            int16_t dy;
        } motion;
        struct {
            uint8_t id;       /* g6he_button_id_t */
            bool pressed;
            uint16_t depth;   /* analog depth, 0 when only a contact is known */
            bool depth_valid; /* false when the source has no analog channel */
        } button;
        struct {
            int8_t delta;
        } wheel;
        struct {
            uint8_t mode;     /* g6he_mode_t */
        } mode;
        struct {
            uint16_t millivolts;
            uint8_t percent;
            bool charging;
        } battery;
    } u;
} g6he_event_t;

typedef struct {
    g6he_event_t items[G6HE_EVENT_QUEUE_CAPACITY];
    size_t head;
    size_t count;
} g6he_event_queue_t;

void g6he_event_queue_init(g6he_event_queue_t *queue);

/* Push overwrites nothing: returns false when the queue is full. */
bool g6he_event_queue_push(g6he_event_queue_t *queue, const g6he_event_t *event);
bool g6he_event_queue_pop(g6he_event_queue_t *queue, g6he_event_t *out);
size_t g6he_event_queue_count(const g6he_event_queue_t *queue);

/* Convenience constructors (zero the event, set type, copy fields). */
g6he_event_t g6he_event_motion(uint32_t timestamp_ms, int16_t dx, int16_t dy);
g6he_event_t g6he_event_button(uint32_t timestamp_ms, g6he_button_id_t id,
                               bool pressed, uint16_t depth, bool depth_valid);
g6he_event_t g6he_event_wheel(uint32_t timestamp_ms, int8_t delta);
g6he_event_t g6he_event_mode(uint32_t timestamp_ms, g6he_mode_t mode);
g6he_event_t g6he_event_battery(uint32_t timestamp_ms, uint16_t millivolts,
                                uint8_t percent, bool charging);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_EVENT_H */
