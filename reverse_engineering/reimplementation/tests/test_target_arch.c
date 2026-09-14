/*
 * test_target_arch.c - tests for the target-ready architecture modules.
 *
 * Covers the common event model, optomagnetic chain, sensor transport,
 * nPM1300 power interface, settings migration, USB HID model and the
 * wireless mocks. No hardware is touched; every transport is a mock.
 */
#include "g6he_test.h"

#include "g6he_event.h"
#include "g6he_hid.h"
#include "g6he_optomagnetic.h"
#include "g6he_power.h"
#include "g6he_sensor.h"
#include "g6he_settings.h"
#include "g6he_wireless.h"

/* ---------- event model ---------- */

static void test_events(void)
{
    g6he_event_queue_t queue;
    g6he_event_t e;
    g6he_event_t out;
    size_t i;

    g6he_event_queue_init(&queue);
    CHECK_EQ_INT(g6he_event_queue_count(&queue), 0u);

    e = g6he_event_motion(10u, 5, -7);
    CHECK_EQ_INT(e.type, G6HE_EV_MOTION);
    CHECK_EQ_INT(e.u.motion.dx, 5);
    CHECK_EQ_INT(e.u.motion.dy, -7);
    CHECK(g6he_event_queue_push(&queue, &e));

    e = g6he_event_wheel(11u, -3);
    CHECK_EQ_INT(e.type, G6HE_EV_WHEEL);
    CHECK(g6he_event_queue_push(&queue, &e));

    CHECK_EQ_INT(g6he_event_queue_count(&queue), 2u);
    CHECK(g6he_event_queue_pop(&queue, &out));
    CHECK_EQ_INT(out.type, G6HE_EV_MOTION);
    CHECK_EQ_INT(out.u.motion.dx, 5);
    CHECK(g6he_event_queue_pop(&queue, &out));
    CHECK_EQ_INT(out.type, G6HE_EV_WHEEL);
    CHECK(!g6he_event_queue_pop(&queue, &out));

    /* Fill to capacity; one extra push must fail, not overwrite. */
    for (i = 0; i < G6HE_EVENT_QUEUE_CAPACITY; ++i) {
        e = g6he_event_motion((uint32_t)i, 1, 1);
        CHECK(g6he_event_queue_push(&queue, &e));
    }
    e = g6he_event_motion(999u, 1, 1);
    CHECK(!g6he_event_queue_push(&queue, &e));
    CHECK_EQ_INT(g6he_event_queue_count(&queue), G6HE_EVENT_QUEUE_CAPACITY);
}

/* ---------- optomagnetic chain ---------- */

static void test_optomagnetic(void)
{
    g6he_om_button_t btn;
    g6he_event_t out;
    g6he_om_config_t cfg;

    /* Disabled configuration must not silently enable a button. */
    cfg = g6he_optomagnetic_config_disabled();
    CHECK_EQ_INT(g6he_optomagnetic_init(&btn, G6HE_BTN_LEFT, &cfg),
                 G6HE_ERR_UNSUPPORTED);

    /* Hall without hysteresis is rejected. */
    cfg = g6he_optomagnetic_config_disabled();
    cfg.enable_hall = true;
    cfg.hall_press_threshold = 100u;
    cfg.hall_release_threshold = 100u;
    CHECK_EQ_INT(g6he_optomagnetic_init(&btn, G6HE_BTN_LEFT, &cfg),
                 G6HE_ERR_RANGE);

    /* IR-only path with 5 ms debounce. */
    cfg = g6he_optomagnetic_config_disabled();
    cfg.enable_ir = true;
    cfg.ir_active_high = true;
    cfg.debounce_ms = 5u;
    CHECK_EQ_INT(g6he_optomagnetic_init(&btn, G6HE_BTN_LEFT, &cfg), G6HE_OK);

    CHECK_EQ_INT(g6he_optomagnetic_feed_ir(&btn, true), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 2u, 2u), G6HE_OK);
    CHECK(!g6he_optomagnetic_take_event(&btn, &out)); /* not yet debounced */
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 3u, 5u), G6HE_OK);
    CHECK(g6he_optomagnetic_take_event(&btn, &out));
    CHECK_EQ_INT(out.type, G6HE_EV_BUTTON);
    CHECK_EQ_INT(out.u.button.id, G6HE_BTN_LEFT);
    CHECK(out.u.button.pressed);
    CHECK(!out.u.button.depth_valid); /* IR-only: no analog depth */
    CHECK(!g6he_optomagnetic_take_event(&btn, &out)); /* consumed once */

    /* Release needs its own debounce. */
    CHECK_EQ_INT(g6he_optomagnetic_feed_ir(&btn, false), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 5u, 10u), G6HE_OK);
    CHECK(g6he_optomagnetic_take_event(&btn, &out));
    CHECK(!out.u.button.pressed);

    /* Hall-only path with hysteresis. */
    cfg = g6he_optomagnetic_config_disabled();
    cfg.enable_hall = true;
    cfg.hall_press_threshold = 2000u;
    cfg.hall_release_threshold = 1500u;
    cfg.debounce_ms = 1u;
    CHECK_EQ_INT(g6he_optomagnetic_init(&btn, G6HE_BTN_RIGHT, &cfg), G6HE_OK);

    CHECK_EQ_INT(g6he_optomagnetic_feed_hall_raw(&btn, 1800u), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 5u, 1u), G6HE_OK);
    CHECK(!g6he_optomagnetic_take_event(&btn, &out)); /* inside hysteresis band */

    CHECK_EQ_INT(g6he_optomagnetic_feed_hall_raw(&btn, 2200u), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_hall_raw(&btn), 2200u);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 1u, 2u), G6HE_OK);
    CHECK(g6he_optomagnetic_take_event(&btn, &out));
    CHECK(out.u.button.pressed);
    CHECK(out.u.button.depth_valid);
    CHECK_EQ_INT(out.u.button.depth, 2200u);

    /* Fusion: IR releases while Hall still pressed keeps the button down. */
    cfg = g6he_optomagnetic_config_disabled();
    cfg.enable_ir = true;
    cfg.enable_hall = true;
    cfg.ir_active_high = true;
    cfg.hall_press_threshold = 2000u;
    cfg.hall_release_threshold = 1500u;
    cfg.debounce_ms = 1u;
    CHECK_EQ_INT(g6he_optomagnetic_init(&btn, G6HE_BTN_LEFT, &cfg), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_feed_hall_raw(&btn, 2500u), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_feed_ir(&btn, true), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 1u, 1u), G6HE_OK);
    CHECK(g6he_optomagnetic_take_event(&btn, &out));
    CHECK(out.u.button.pressed);
    CHECK_EQ_INT(g6he_optomagnetic_feed_ir(&btn, false), G6HE_OK);
    CHECK_EQ_INT(g6he_optomagnetic_poll(&btn, 10u, 20u), G6HE_OK);
    CHECK(!g6he_optomagnetic_take_event(&btn, &out)); /* Hall holds it down */
}

/* ---------- sensor transport ---------- */

static int mock_read(void *ctx, uint8_t reg, uint8_t *buf, size_t len)
{
    (void)ctx;
    for (size_t i = 0; i < len; ++i) {
        buf[i] = (uint8_t)(reg + i);
    }
    return 0;
}

static int mock_write(void *ctx, uint8_t reg, const uint8_t *buf, size_t len)
{
    (void)ctx;
    (void)reg;
    (void)buf;
    return (len == 0u) ? -1 : 0;
}

static int mock_reset(void *ctx)
{
    (void)ctx;
    return 0;
}

static int mock_motion(void *ctx)
{
    (void)ctx;
    return 1;
}

static void test_sensor(void)
{
    g6he_sensor_t sensor;
    g6he_sensor_transport_t t;
    uint8_t buf[2];
    bool asserted = false;

    memset(&sensor, 0, sizeof(sensor));
    t.read = mock_read;
    t.write = mock_write;
    t.reset = mock_reset;
    t.motion_asserted = mock_motion;
    t.ctx = NULL;

    CHECK_EQ_INT(g6he_sensor_bind(&sensor, G6HE_SENSOR_UNSET, &t),
                 G6HE_ERR_UNSUPPORTED);
    CHECK_EQ_INT(g6he_sensor_max_cpi(&sensor), 0u);

    t.read = NULL;
    CHECK_EQ_INT(g6he_sensor_bind(&sensor, G6HE_SENSOR_PAW3955, &t),
                 G6HE_ERR_ARG);
    t.read = mock_read;

    CHECK_EQ_INT(g6he_sensor_bind(&sensor, G6HE_SENSOR_PAW3950, &t), G6HE_OK);
    CHECK_EQ_INT(g6he_sensor_max_cpi(&sensor), 30000u);
    CHECK_EQ_INT(g6he_sensor_bind(&sensor, G6HE_SENSOR_PAW3955, &t), G6HE_OK);
    CHECK_EQ_INT(g6he_sensor_max_cpi(&sensor), 40000u);

    CHECK_EQ_INT(g6he_sensor_read_reg(&sensor, 0x10u, buf, 2u), G6HE_OK);
    CHECK_EQ_INT(buf[0], 0x10u);
    CHECK_EQ_INT(buf[1], 0x11u);
    CHECK_EQ_INT(g6he_sensor_write_reg(&sensor, 0x10u, buf, 2u), G6HE_OK);
    CHECK_EQ_INT(g6he_sensor_reset(&sensor), G6HE_OK);
    CHECK_EQ_INT(g6he_sensor_motion_asserted(&sensor, &asserted), G6HE_OK);
    CHECK(asserted);
}

/* ---------- power ---------- */

static int mock_power_state(void *ctx, g6he_power_state_t *out)
{
    (void)ctx;
    out->battery_mv = 3900u;
    out->percent = 80u;
    out->charging = true;
    out->vbus_present = true;
    out->ntc_ok = true;
    return 0;
}

static void test_power(void)
{
    g6he_power_config_t cfg = g6he_power_config_unknown();
    g6he_power_backend_t backend;
    g6he_power_state_t state;

    CHECK_EQ_INT(g6he_power_validate_config(&cfg), G6HE_ERR_UNSUPPORTED);
    CHECK_EQ_INT(g6he_power_read_state(&state), G6HE_ERR_UNSUPPORTED);

    cfg.charge_current_ma = 250;
    cfg.termination_voltage_mv = 4350;
    cfg.buck1_mv = 3300;
    cfg.buck2_mv = 1800;
    cfg.ldo1_mv = 1800;
    cfg.ldo2_mv = 3300;
    cfg.ntc_beta = 3380;
    CHECK_EQ_INT(g6he_power_validate_config(&cfg), G6HE_OK);

    backend.read_state = mock_power_state;
    backend.set_ship_mode = NULL;
    backend.ctx = NULL;
    CHECK_EQ_INT(g6he_power_bind(&backend), G6HE_OK);
    CHECK_EQ_INT(g6he_power_read_state(&state), G6HE_OK);
    CHECK_EQ_INT(state.battery_mv, 3900u);
    CHECK(state.charging);
    CHECK_EQ_INT(g6he_power_enter_ship_mode(), G6HE_ERR_UNSUPPORTED);

    /* Evidenced thresholds (SPEC): low < 3.4 V, critical < 3.2 V. */
    CHECK_EQ_INT(g6he_power_classify(4000u), G6HE_BATT_OK);
    CHECK_EQ_INT(g6he_power_classify(3400u), G6HE_BATT_OK);
    CHECK_EQ_INT(g6he_power_classify(3399u), G6HE_BATT_LOW);
    CHECK_EQ_INT(g6he_power_classify(3200u), G6HE_BATT_LOW);
    CHECK_EQ_INT(g6he_power_classify(3199u), G6HE_BATT_CRITICAL);
}

/* ---------- settings migration ---------- */

static void test_settings(void)
{
    g6he_settings_t s;
    char value[G6HE_SETTINGS_VALUE_MAX];

    g6he_settings_init(&s, 1u);
    CHECK_EQ_INT(g6he_settings_set(&s, "hall/ax0", "curve-a"), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_set(&s, "hall/ax1", "curve-b"), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_set(&s, "hall/mem0", "keep"), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_set(&s, "vendor/custom", "v"), G6HE_OK);
    CHECK(g6he_settings_has(&s, "hall/ax0"));

    CHECK_EQ_INT(g6he_settings_migrate(&s), G6HE_OK);
    CHECK_EQ_INT(s.version, G6HE_SETTINGS_SCHEMA_VERSION);
    CHECK(!g6he_settings_has(&s, "hall/ax0"));
    CHECK(!g6he_settings_has(&s, "hall/ax1"));
    CHECK(g6he_settings_has(&s, "ax0"));
    CHECK(g6he_settings_has(&s, "ax1"));
    CHECK_EQ_INT(g6he_settings_get(&s, "ax0", value, sizeof(value)), G6HE_OK);
    CHECK_EQ_STR(value, "curve-a");
    CHECK_EQ_INT(g6he_settings_get(&s, "ax1", value, sizeof(value)), G6HE_OK);
    CHECK_EQ_STR(value, "curve-b");
    /* Unrelated keys are preserved, not dropped. */
    CHECK_EQ_INT(g6he_settings_get(&s, "hall/mem0", value, sizeof(value)),
                 G6HE_OK);
    CHECK_EQ_STR(value, "keep");
    CHECK(g6he_settings_has(&s, "vendor/custom"));

    /* Idempotent. */
    CHECK_EQ_INT(g6he_settings_migrate(&s), G6HE_OK);
    CHECK(g6he_settings_has(&s, "ax0"));
    CHECK_EQ_INT(s.count, 4u);

    /* A v2 store with both keys keeps the newer destination. */
    g6he_settings_init(&s, 1u);
    CHECK_EQ_INT(g6he_settings_set(&s, "ax0", "new"), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_set(&s, "hall/ax0", "old"), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_migrate(&s), G6HE_OK);
    CHECK_EQ_INT(g6he_settings_get(&s, "ax0", value, sizeof(value)), G6HE_OK);
    CHECK_EQ_STR(value, "new");

    /* Rename table is exactly the evidenced pair. */
    CHECK_EQ_INT(g6he_settings_rename_count(), 2u);
    CHECK_EQ_STR(g6he_settings_renames()[0].from, "hall/ax0");
    CHECK_EQ_STR(g6he_settings_renames()[0].to, "ax0");
    CHECK_EQ_STR(g6he_settings_renames()[1].from, "hall/ax1");
    CHECK_EQ_STR(g6he_settings_renames()[1].to, "ax1");
}

/* ---------- HID ---------- */

static void test_hid(void)
{
    g6he_hid_mouse_report_t report;
    g6he_event_t e;
    uint8_t bytes[G6HE_HID_BOOT_REPORT_SIZE];

    report.buttons = 0;
    report.dx = report.dy = report.wheel = 0;
    CHECK(g6he_hid_report_is_idle(&report));

    e = g6he_event_button(1u, G6HE_BTN_LEFT, true, 0u, false);
    CHECK(g6he_hid_accumulate(&report, &e));
    CHECK_EQ_INT(report.buttons, G6HE_HID_BUTTON_LEFT);

    e = g6he_event_motion(2u, 40, -20);
    CHECK(g6he_hid_accumulate(&report, &e));
    CHECK_EQ_INT(report.dx, 40);
    CHECK_EQ_INT(report.dy, -20);

    e = g6he_event_motion(3u, 100, 0);
    CHECK(g6he_hid_accumulate(&report, &e));
    CHECK_EQ_INT(report.dx, 127); /* clamped, not wrapped */

    e = g6he_event_wheel(4u, -2);
    CHECK(g6he_hid_accumulate(&report, &e));
    CHECK_EQ_INT(report.wheel, -2);

    CHECK_EQ_INT(g6he_hid_encode_boot(&report, bytes), G6HE_OK);
    CHECK_EQ_INT(bytes[0], G6HE_HID_BUTTON_LEFT);
    CHECK_EQ_INT(bytes[1], 127);
    CHECK_EQ_INT(bytes[2], (uint8_t)-20);
    CHECK_EQ_INT(bytes[3], (uint8_t)-2);

    e = g6he_event_button(5u, G6HE_BTN_LEFT, false, 0u, false);
    CHECK(g6he_hid_accumulate(&report, &e));
    CHECK_EQ_INT(report.buttons, 0u);

    /* Non-HID events do not touch the report. */
    e = g6he_event_battery(6u, 3900u, 80u, false);
    CHECK(!g6he_hid_accumulate(&report, &e));

    CHECK_EQ_INT(g6he_hid_button_bit(G6HE_BTN_SIDE_BACK), G6HE_HID_BUTTON_BACK);
    CHECK_EQ_INT(g6he_hid_button_bit(G6HE_BTN_COUNT), 0u);
    CHECK_EQ_INT(G6HE_HID_UPDATE_USAGE_PAGE, 0x008Cu);
    CHECK_EQ_INT(G6HE_HID_REPORT_ID_OUT, 0xB2u);
    CHECK_EQ_INT(G6HE_HID_REPORT_ID_IN, 0xB1u);
}

/* ---------- wireless mocks ---------- */

static int mock_send(void *ctx, const uint8_t *report, size_t len)
{
    (void)ctx;
    (void)report;
    return (len == 0u) ? -1 : 0;
}

static void test_wireless(void)
{
    g6he_wireless_t wl;
    g6he_wireless_backend_t backend;
    uint8_t report[4] = {0, 1, 0, 0};

    CHECK_EQ_INT(g6he_wireless_init(&wl, G6HE_WL_LINK_BLE), G6HE_OK);
    CHECK_EQ_INT(wl.state, G6HE_WL_OFF);
    CHECK_EQ_INT(g6he_wireless_send_hid(&wl, report, sizeof(report)),
                 G6HE_ERR_UNSUPPORTED);

    backend.send_hid_report = mock_send;
    backend.start_pairing = NULL;
    backend.ctx = NULL;
    CHECK_EQ_INT(g6he_wireless_bind(&wl, &backend), G6HE_OK);
    CHECK_EQ_INT(g6he_wireless_send_hid(&wl, report, sizeof(report)),
                 G6HE_ERR_STATE); /* not connected */

    g6he_wireless_on_connected(&wl);
    CHECK_EQ_INT(g6he_wireless_send_hid(&wl, report, sizeof(report)), G6HE_OK);

    /* Documented pairing timeout: 3 min then sleep. */
    g6he_wireless_on_pair_request(&wl);
    CHECK_EQ_INT(wl.state, G6HE_WL_PAIRING);
    CHECK_EQ_INT(g6he_wireless_tick(&wl, G6HE_PAIRING_TIMEOUT_MS - 1u),
                 G6HE_WL_PAIRING);
    CHECK_EQ_INT(g6he_wireless_tick(&wl, 1u), G6HE_WL_OFF);

    g6he_wireless_on_disconnected(&wl);
    CHECK_EQ_INT(wl.state, G6HE_WL_RECONNECTING);
    CHECK_EQ_INT(g6he_wireless_tick(&wl, G6HE_RECONNECT_BLINK_MS),
                 G6HE_WL_RECONNECTING);
}

void test_target_arch(void)
{
    printf("[target_arch/events]\n");
    test_events();
    printf("[target_arch/optomagnetic]\n");
    test_optomagnetic();
    printf("[target_arch/sensor]\n");
    test_sensor();
    printf("[target_arch/power]\n");
    test_power();
    printf("[target_arch/settings]\n");
    test_settings();
    printf("[target_arch/hid]\n");
    test_hid();
    printf("[target_arch/wireless]\n");
    test_wireless();
}
