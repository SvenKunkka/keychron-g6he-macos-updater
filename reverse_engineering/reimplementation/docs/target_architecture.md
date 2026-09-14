# Target-ready clean-room architecture

Status: **target-ready, fail-closed.** It builds and is tested on the host, but
it is **not** target firmware, **not** signed, and has **no flash target**.

## Module map

```
                       g6he_event.h/.c        (BR-020)
                    common motion/wheel/button/mode/battery events
                                |
   sensor path                  |                 button path
   -----------                  v                 -----------
 g6he_sensor.h/.c        +---------------+     g6he_optomagnetic.h/.c (BR-021)
 (BR-022)                |  event queue  |     raw Hall -> IR/contact ->
 PAW3950/3955            +-------+-------+     fusion -> debounce -> event
 4-wire SPI transport            |
 variant selection               v
                          g6he_hid.h/.c        (BR-025)
                          USB HID report model + update-report IDs
                                |
                +---------------+---------------+
                v                               v
       g6he_power.h/.c                 g6he_wireless.h/.c  (BR-026)
       (BR-023) nPM1300                BLE HID stub /
       charger+regulator               proprietary-2.4G mock FSM
       + fuel-gauge interface
                |
       g6he_settings.h/.c              (BR-016, BR-024)
       schema v2 + explicit hall/ax0 -> ax0 migration

       g6he_board.h                    compile-time gate (pins + electricals)
```

## Level-of-evidence rule

A behaviour is implemented only when it is **L4**: traceable to a named
artifact and independently checkable. Everything else is one of:

* a **stub** that returns `G6HE_ERR_UNSUPPORTED` until a backend is bound;
* a **mock** whose documented product behaviour is testable on the host;
* a **compile-time failure** when a target build tries to enable it.

| Module | L4 content | Non-L4 handling |
|---|---|---|
| `g6he_event` | queue + typed events | none (pure logic) |
| `g6he_optomagnetic` | stage separation; vendor switch has IR contact + analog Hall; B1612 electricals | thresholds/drive are caller-supplied; disabled config is rejected; no guessed default |
| `g6he_sensor` | 4-wire SPI, variant pin-compatibility, max cpi 30 k/40 k, MOTION/NRESET | register map not implemented (`G6HE_SENSOR_REGISTER_MAP_EVIDENCED 0`); MCU pins UNKNOWN |
| `g6he_power` | product thresholds 3.4 V / 3.2 V; capacity 350 mAh; official Zephyr/NCS driver boundary | charge current, termination voltage, rails, NTC β are all `G6HE_ELECTRICAL_UNKNOWN` and rejected by `g6he_power_validate_config` |
| `g6he_settings` | the evidenced `hall/ax0`→`ax0`, `hall/ax1`→`ax1` rename | migration is a pure rename; unknown keys preserved |
| `g6he_hid` | boot-mouse report; update report IDs 0xB2/0xB1 and usage page 0x008c | no vendor feature reports invented |
| `g6he_wireless` | SPEC timings (3 min pairing timeout, 0.5 s reconnect blink, 10 min idle sleep) | no RF parameters, channels, addresses or TX power; BLE needs a bound backend |
| `g6he_board` | part existence (U1/U7/U8/U10/S1/S2/U2) | every pin and safety electrical is `UNKNOWN` |

## The compile-time blockers

`include/g6he_board.h` is the only place a board constant may be declared. It
contains, in order: the sentinels (`G6HE_PIN_UNKNOWN`,
`G6HE_ELECTRICAL_UNKNOWN`), the evidenced part names, the (all-UNKNOWN) pin
assignments and electrical constants, and finally:

```c
#if defined(G6HE_ENABLE_HARDWARE) && (G6HE_ENABLE_HARDWARE == 1)
#if !G6HE_BOARD_PIN_TABLE_COMPLETE
#error "hardware enabled but the board pin table is incomplete ... refusing to guess"
#endif
#if !G6HE_BOARD_ELECTRICAL_COMPLETE
#error "hardware enabled but charge current / termination voltage / rail voltages are UNKNOWN ..."
#endif
#endif
```

Demonstrated behaviour (recorded in `docs/verification/fail_closed.log`):

```
cc -DG6HE_ENABLE_HARDWARE=1 ... -> 2 errors (pin table, electricals)
```

So a developer cannot accidentally ship guessed GPIOs or a guessed termination
voltage: the build stops.

## Host build and the unsigned engineering artifact

* `make test` — 489 checks, 0 failures (host, `G6HE_ENABLE_HARDWARE=0`).
* `make target-smoke` — builds
  `build/ENGINEERING-UNSIGNED-NOT-FOR-FLASHING`, the host smoke executable of
  the target entry point. It is deliberately named as unsigned and
  non-flashable.
* `src/g6he_board.h` plus the Zephyr project's `g6he-reject-flash` target exist
  so no release/flash path can be created by accident.

## Zephyr project

`zephyr/` contains `CMakeLists.txt`, `prj.conf` and `src/main.c`. It lists the
same portable translation units and compiles them with
`G6HE_ENABLE_HARDWARE=OFF` by default. `prj.conf` disables GPIO/SPI/I2C/ADC/
INPUT/USB/BT/SETTINGS/FLASH and MCUboot. Enabling hardware requires an evidenced
board overlay **and** `G6HE_BOARD_PIN_TABLE_COMPLETE`/`..._ELECTRICAL_COMPLETE`
to be set to 1 — which is only valid after new evidence exists.

A devicetree overlay template lives in
`docs/nrf54lm20a_bringup_plan.md`; pin values are intentionally absent.

## What did not change

* No device command was sent, no image was signed, no binary was modified.
* No RF parameter, charge current, termination voltage or pin number was
  invented.
* The host behavioural model from the previous run still passes unchanged
  (217 → 489 checks, all green).
