/*
 * g6he_event.c - common event model.
 */
#include "g6he_event.h"

#include <string.h>

void g6he_event_queue_init(g6he_event_queue_t *queue)
{
    if (queue == NULL) {
        return;
    }
    memset(queue, 0, sizeof(*queue));
}

bool g6he_event_queue_push(g6he_event_queue_t *queue, const g6he_event_t *event)
{
    size_t slot;

    if (queue == NULL || event == NULL) {
        return false;
    }
    if (queue->count >= G6HE_EVENT_QUEUE_CAPACITY) {
        return false;
    }
    slot = (queue->head + queue->count) % G6HE_EVENT_QUEUE_CAPACITY;
    queue->items[slot] = *event;
    queue->count++;
    return true;
}

bool g6he_event_queue_pop(g6he_event_queue_t *queue, g6he_event_t *out)
{
    if (queue == NULL || out == NULL || queue->count == 0u) {
        return false;
    }
    *out = queue->items[queue->head];
    queue->head = (queue->head + 1u) % G6HE_EVENT_QUEUE_CAPACITY;
    queue->count--;
    return true;
}

size_t g6he_event_queue_count(const g6he_event_queue_t *queue)
{
    return (queue == NULL) ? 0u : queue->count;
}

g6he_event_t g6he_event_motion(uint32_t timestamp_ms, int16_t dx, int16_t dy)
{
    g6he_event_t e;

    memset(&e, 0, sizeof(e));
    e.type = G6HE_EV_MOTION;
    e.timestamp_ms = timestamp_ms;
    e.u.motion.dx = dx;
    e.u.motion.dy = dy;
    return e;
}

g6he_event_t g6he_event_button(uint32_t timestamp_ms, g6he_button_id_t id,
                               bool pressed, uint16_t depth, bool depth_valid)
{
    g6he_event_t e;

    memset(&e, 0, sizeof(e));
    e.type = G6HE_EV_BUTTON;
    e.timestamp_ms = timestamp_ms;
    e.u.button.id = (uint8_t)id;
    e.u.button.pressed = pressed;
    e.u.button.depth = depth;
    e.u.button.depth_valid = depth_valid;
    return e;
}

g6he_event_t g6he_event_wheel(uint32_t timestamp_ms, int8_t delta)
{
    g6he_event_t e;

    memset(&e, 0, sizeof(e));
    e.type = G6HE_EV_WHEEL;
    e.timestamp_ms = timestamp_ms;
    e.u.wheel.delta = delta;
    return e;
}

g6he_event_t g6he_event_mode(uint32_t timestamp_ms, g6he_mode_t mode)
{
    g6he_event_t e;

    memset(&e, 0, sizeof(e));
    e.type = G6HE_EV_MODE;
    e.timestamp_ms = timestamp_ms;
    e.u.mode.mode = (uint8_t)mode;
    return e;
}

g6he_event_t g6he_event_battery(uint32_t timestamp_ms, uint16_t millivolts,
                                uint8_t percent, bool charging)
{
    g6he_event_t e;

    memset(&e, 0, sizeof(e));
    e.type = G6HE_EV_BATTERY;
    e.timestamp_ms = timestamp_ms;
    e.u.battery.millivolts = millivolts;
    e.u.battery.percent = percent;
    e.u.battery.charging = charging;
    return e;
}
