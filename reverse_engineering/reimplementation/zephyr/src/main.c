/*
 * G6 HE clean-room target entry point.
 *
 * ENGINEERING-UNSIGNED-NOT-FOR-FLASHING.
 *
 * This is not a firmware release. With hardware disabled (the default) it
 * exercises the portable modules against mock backends and reports the
 * fail-closed state. It sends no radio traffic, drives no GPIO, programs no
 * PMIC register and defines no flash path.
 */
#include "g6he_board.h"
#include "g6he_event.h"
#include "g6he_hid.h"
#include "g6he_optomagnetic.h"
#include "g6he_power.h"
#include "g6he_sensor.h"
#include "g6he_settings.h"
#include "g6he_wireless.h"

#ifdef __ZEPHYR__
#include <zephyr/kernel.h>
#define G6HE_LOG(fmt, ...) printk(fmt "\n", ##__VA_ARGS__)
#else
#include <stdio.h>
#define G6HE_LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#endif

static int mock_sensor_read(void *ctx, uint8_t reg, uint8_t *buf, size_t len)
{
    (void)ctx;
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(reg + i);
    }
    return 0;
}

static int mock_sensor_write(void *ctx, uint8_t reg, const uint8_t *buf,
                             size_t len)
{
    (void)ctx;
    (void)reg;
    (void)buf;
    return (len == 0u) ? -1 : 0;
}

static int mock_sensor_reset(void *ctx)
{
    (void)ctx;
    return 0;
}

static int mock_sensor_motion(void *ctx)
{
    (void)ctx;
    return 0;
}

static int mock_wl_send(void *ctx, const uint8_t *report, size_t len)
{
    (void)ctx;
    (void)report;
    return (len == 0u) ? -1 : 0;
}

int main(void)
{
    g6he_event_queue_t queue;
    g6he_sensor_t sensor;
    g6he_sensor_transport_t transport = {
        .read = mock_sensor_read,
        .write = mock_sensor_write,
        .reset = mock_sensor_reset,
        .motion_asserted = mock_sensor_motion,
        .ctx = NULL,
    };
    g6he_om_config_t om_cfg = g6he_optomagnetic_config_disabled();
    g6he_om_button_t left;
    g6he_power_config_t power_cfg = g6he_power_config_unknown();
    g6he_settings_t settings;
    g6he_wireless_t wl;
    g6he_wireless_backend_t wl_backend = {
        .send_hid_report = mock_wl_send,
        .start_pairing = NULL,
        .ctx = NULL,
    };
    g6he_hid_mouse_report_t report = {0, 0, 0, 0};
    uint8_t boot[G6HE_HID_BOOT_REPORT_SIZE];

    G6HE_LOG("G6 HE clean-room target — ENGINEERING-UNSIGNED-NOT-FOR-FLASHING");
    G6HE_LOG("MCU=%s PMIC=%s HALL=%s SENSOR(variants)=%s/%s",
             G6HE_BOARD_MCU, G6HE_BOARD_PMIC, G6HE_BOARD_HALL,
             G6HE_BOARD_SENSOR_3950, G6HE_BOARD_SENSOR_3955);
    G6HE_LOG("pin table complete=%d electrical complete=%d (both must be 1 "
             "before hardware may be enabled)",
             G6HE_BOARD_PIN_TABLE_COMPLETE, G6HE_BOARD_ELECTRICAL_COMPLETE);

    g6he_event_queue_init(&queue);
    {
        const g6he_event_t motion = g6he_event_motion(0u, 1, -1);
        (void)g6he_event_queue_push(&queue, &motion);
    }

    (void)g6he_sensor_bind(&sensor, G6HE_SENSOR_PAW3955, &transport);
    G6HE_LOG("sensor max cpi (datasheet) = %u", g6he_sensor_max_cpi(&sensor));

    /* Optomagnetic: no calibration is evidenced, so the channel stays disabled. */
    if (g6he_optomagnetic_init(&left, G6HE_BTN_LEFT, &om_cfg) != G6HE_OK) {
        G6HE_LOG("optomagnetic left: DISABLED (no evidenced calibration)");
    }

    G6HE_LOG("power config valid = %d (expected 0: UNKNOWN electricals)",
             g6he_power_validate_config(&power_cfg) == G6HE_OK);

    g6he_settings_init(&settings, 1u);
    (void)g6he_settings_migrate(&settings);
    G6HE_LOG("settings schema migrated to v%u (hall/ax0 -> ax0)",
             settings.version);

    (void)g6he_wireless_init(&wl, G6HE_WL_LINK_BLE);
    (void)g6he_wireless_bind(&wl, &wl_backend);
    G6HE_LOG("wireless: no RF configuration is evidenced; radio disabled");

    (void)g6he_hid_encode_boot(&report, boot);
    G6HE_LOG("hid boot report = %02x %02x %02x %02x", boot[0], boot[1], boot[2],
             boot[3]);

    G6HE_LOG("no device command sent, no image signed, no flash target.");
    return 0;
}
