# Interfaces

All types are in `include/`. Functions never allocate, never block and never
touch a device. Status codes are the `g6he_status_t` enum in
`g6he_common.h`; `G6HE_OK` is 0.

## g6he_crc.h — BR-010, BR-011

```c
uint16_t g6he_payload_checksum(const uint8_t *data, size_t len);
uint32_t g6he_crc32_update(uint32_t crc, const uint8_t *data, size_t len);
uint32_t g6he_crc32(const uint8_t *data, size_t len);
```

`g6he_crc32()` starts from 0xFFFFFFFF with no final XOR. `data == NULL` is
treated as an empty buffer.

## g6he_sha512.h — BR-002

```c
void g6he_sha512_init(g6he_sha512_ctx_t *ctx);
void g6he_sha512_update(g6he_sha512_ctx_t *ctx, const uint8_t *data, size_t len);
void g6he_sha512_final(g6he_sha512_ctx_t *ctx, uint8_t out[64]);
void g6he_sha512(const uint8_t *data, size_t len, uint8_t out[64]);
```

FIPS 180-4 SHA-512. Only SHA-512 is implemented; MCUboot SHA-256/SHA-384
digest types return `G6HE_ERR_UNSUPPORTED`.

## g6he_mcuboot.h — BR-001…BR-005, BR-013

```c
g6he_status_t g6he_mcuboot_parse(const uint8_t *data, size_t len,
                                 g6he_image_info_t *out);
void g6he_mcuboot_format_version(const g6he_image_header_t *header,
                                 char out[G6HE_VERSION_STRING_MAX]);
bool g6he_mcuboot_ram_load_ok(const g6he_image_header_t *header);
const uint8_t *g6he_mcuboot_find_string(const uint8_t *data, size_t len,
                                        const g6he_image_info_t *info,
                                        const char *needle);
bool g6he_mcuboot_has_model(const uint8_t *data, size_t len,
                            const g6he_image_info_t *info, const char *model);
int g6he_version_compare(const char *current, const char *target);
const char *g6he_version_relation(const char *current, const char *target);
```

`g6he_mcuboot_parse()` returns non-zero (and fails closed) for any malformed,
unsigned, protected-TLV, out-of-range or digest-mismatching image.
`g6he_version_compare()` returns -1/0/1, or 2 when a version cannot be parsed;
`g6he_version_relation()` maps that to `"upgrade"`, `"same"`, `"downgrade"` or
`"unknown"`.

## g6he_identity.h — BR-006, BR-007, BR-017

```c
g6he_status_t g6he_identity_decode(const uint8_t *data, size_t len,
                                   g6he_identity_t *out);
g6he_status_t g6he_identity_encode(const g6he_identity_t *identity,
                                   uint8_t out[34]);
g6he_status_t g6he_capabilities_decode(const uint8_t *data, size_t len,
                                       g6he_capabilities_t *out);
g6he_status_t g6he_capabilities_encode(const g6he_capabilities_t *caps,
                                       uint8_t out[4]);
g6he_status_t g6he_kcfwid_extract(const uint8_t *data, size_t len,
                                  char model[11], char version[16]);
```

Field widths are fixed by the protocol: 10-byte NUL-padded model, 2-byte
hardware revision, 10-byte firmware version, 10-byte bootloader model, 2-byte
bootloader version.

## g6he_hall.h — BR-014

```c
extern const uint16_t g6he_hall_breakpoints[6]; /* {0,10,20,30,50,70} */
uint16_t g6he_hall_interpolate(const g6he_hall_curve_t *curve, uint8_t x);
uint16_t g6he_hall_derived_600(const g6he_hall_curve_t *curve, uint8_t offset);
```

The per-axis `y` values are caller-supplied because the compiled tables are
UNKNOWN. `x >= 65` clamps to the last `y`.

## g6he_defaults.h — BR-015

```c
const g6he_defaults_t *g6he_defaults_factory(void);
```

Returns a pointer to static storage holding the recovered literals. Roles are
reconstructed labels.

## g6he_protocol.h — BR-008…BR-012

```c
size_t g6he_frame_build(uint8_t out[33], uint8_t sequence,
                        const uint8_t *payload, size_t payload_len,
                        bool update_frame);
g6he_status_t g6he_frame_parse(const uint8_t report[33], g6he_request_t *out);
g6he_status_t g6he_response_encode(const g6he_response_t *response,
                                   bool update_frame, uint8_t *out,
                                   size_t out_cap, size_t *out_bytes);
g6he_status_t g6he_response_decode(const uint8_t *reports, size_t reports_len,
                                   g6he_response_t *out);
void g6he_updater_init(g6he_updater_t *updater,
                       const g6he_identity_t *identity,
                       const g6he_capabilities_t *capabilities);
g6he_status_t g6he_updater_handle(g6he_updater_t *updater,
                                  const g6he_request_t *request,
                                  g6he_response_t *response);
```

`g6he_updater_handle()` always produces a response for a well-formed request.
Protocol-level rejection is reported through `response->status`; the return
value is `G6HE_OK` in that case. Malformed input (null pointers, empty payload)
returns an error and still fills the response with `status = 8`.

The `sequence` field must be in 1..255; 0 is rejected, matching the host tool's
sequence policy.

## Target-ready interfaces (BR-020…BR-027)

Header-by-header contract; see `docs/target_architecture.md` for the module
graph and the fail-closed rules.

### g6he_board.h — compile-time gate

The only place a board pin or electrical constant may be declared. All values
are `G6HE_PIN_UNKNOWN` / `G6HE_ELECTRICAL_UNKNOWN` today. Defining
`G6HE_ENABLE_HARDWARE=1` triggers `#error` for each incomplete table.

### g6he_event.h — BR-020

```c
void g6he_event_queue_init(g6he_event_queue_t *queue);
bool g6he_event_queue_push(g6he_event_queue_t *queue, const g6he_event_t *event);
bool g6he_event_queue_pop(g6he_event_queue_t *queue, g6he_event_t *out);
g6he_event_t g6he_event_motion(uint32_t ts, int16_t dx, int16_t dy);
g6he_event_t g6he_event_button(uint32_t ts, g6he_button_id_t id, bool pressed,
                               uint16_t depth, bool depth_valid);
```

Fixed 32-slot ring; a full queue rejects the push (never overwrites).

### g6he_optomagnetic.h — BR-021

```c
g6he_status_t g6he_optomagnetic_init(g6he_om_button_t *b, g6he_button_id_t id,
                                     const g6he_om_config_t *cfg);
g6he_status_t g6he_optomagnetic_feed_hall_raw(g6he_om_button_t *b, uint16_t raw);
g6he_status_t g6he_optomagnetic_feed_ir(g6he_om_button_t *b, bool closed);
g6he_status_t g6he_optomagnetic_poll(g6he_om_button_t *b, uint16_t dt_ms,
                                     uint32_t now_ms);
bool g6he_optomagnetic_take_event(g6he_om_button_t *b, g6he_event_t *out);
```

`init` rejects a disabled configuration (`UNSUPPORTED`) and a Hall pair without
hysteresis (`RANGE`); there is no default threshold.

### g6he_sensor.h — BR-022

```c
g6he_status_t g6he_sensor_bind(g6he_sensor_t *s, g6he_sensor_variant_t variant,
                               const g6he_sensor_transport_t *transport);
uint16_t g6he_sensor_max_cpi(const g6he_sensor_t *s); /* 30000 / 40000 / 0 */
g6he_status_t g6he_sensor_read_reg(...), g6he_sensor_write_reg(...);
g6he_status_t g6he_sensor_reset(...), g6he_sensor_motion_asserted(...);
```

`G6HE_SENSOR_UNSET` (the default) is rejected; an incomplete transport is
rejected.

### g6he_power.h — BR-023

```c
g6he_status_t g6he_power_validate_config(const g6he_power_config_t *cfg);
g6he_status_t g6he_power_bind(const g6he_power_backend_t *backend);
g6he_status_t g6he_power_read_state(g6he_power_state_t *out);
g6he_status_t g6he_power_enter_ship_mode(void);
g6he_battery_class_t g6he_power_classify(uint16_t mv); /* 3.4 V / 3.2 V */
```

Any `G6HE_ELECTRICAL_UNKNOWN` required field makes `validate_config` return
`UNSUPPORTED`.

### g6he_settings.h — BR-024

Versioned store with `g6he_settings_migrate()` applying the evidenced
`hall/ax0`→`ax0`, `hall/ax1`→`ax1` renames. Idempotent; unrelated keys are
preserved.

### g6he_hid.h — BR-025

`g6he_hid_accumulate()` folds events into `g6he_hid_mouse_report_t`;
`g6he_hid_encode_boot()` emits the 4-byte boot report. Update report IDs
`0xB2`/`0xB1` and usage page `0x008c` are shared with `g6he_protocol.h`.

### g6he_wireless.h — BR-026

`g6he_wireless_tick()` implements only the documented product timings
(3 min pairing timeout, 0.5 s reconnect blink, 10 min idle sleep).
`g6he_wireless_send_hid()` returns `UNSUPPORTED` without a bound backend and
`STATE` when not connected. No RF parameter exists in this interface.
