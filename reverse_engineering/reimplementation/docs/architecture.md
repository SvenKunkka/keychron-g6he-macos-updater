# Architecture

## 1. Purpose

Provide an auditable, hardware-independent implementation of the G6 HE
firmware behaviours that the reverse-engineering workspace recovered well enough
to specify and test. The tree is the clean-room counterpart to the analysis
documents: the analysis says *what* the firmware does and with what confidence;
this tree turns the testable parts into running code.

## 2. Non-goals and the target-ready layer

The original behavioural model is still **not** target firmware, and the vendor
source tree, linker map and `.elf` remain missing. On top of it, this tree now
carries a **target-ready but fail-closed** layer (see
`docs/target_architecture.md` and `docs/nrf54lm20a_bringup_plan.md`):

- common event model (`g6he_event`), optomagnetic chain
  (`g6he_optomagnetic`), sensor transport and variant selection
  (`g6he_sensor`), nPM1300 power interface (`g6he_power`), settings schema +
  explicit migration (`g6he_settings`), USB HID report model (`g6he_hid`) and
  BLE/2.4G stubs (`g6he_wireless`);
- `include/g6he_board.h` is the single place board constants may live. Every pin
  and every safety-critical electrical is `UNKNOWN`, and enabling hardware in a
  target build (`-DG6HE_ENABLE_HARDWARE=1`) **fails to compile** rather than
  guessing;
- a Zephyr project skeleton under `zephyr/` that lists the same translation
  units, with hardware disabled by default.

Still out of scope / not claimed: interoperability, runtime equivalence,
signing, flashing and any RF behaviour.

## 3. Layers and dependency direction

```
        tests/ (host)                         docs/ (this directory)
            |
  +---------v--------------------------------------------------+
  | input        g6he_event  g6he_optomagnetic  g6he_sensor   |
  |              g6he_wheel*  g6he_hid  g6he_wireless         |
  | power        g6he_power  g6he_settings  g6he_board (gate) |
  | transport    g6he_protocol   frame codec + device model    |
  | records      g6he_identity   identity/caps/KCFWID          |
  | container    g6he_mcuboot    header/TLV + version compare  |
  | algorithm    g6he_hall       piecewise-linear interpolation|
  | config       g6he_defaults   recovered factory constants   |
  | primitives   g6he_crc  g6he_sha512  g6he_sha256            |
  +------------------------------------------------------------+
```

Dependencies point downward only. `g6he_protocol` depends on `g6he_identity`
and `g6he_crc`; `g6he_mcuboot` depends on `g6he_sha512`; the input/power
modules depend on `g6he_board` and `g6he_event`; nothing depends on a platform
or on device state.

*`g6he_wheel` is represented by wheel events in `g6he_event`; no separate wheel
module exists yet because the encoder channel mapping is UNKNOWN (HW-U-016).

## 4. Data flow reproduced

**Image validation (BR-001…BR-005):**

```
file bytes -> header fields -> TLV area walk -> SHA-512 over header+payload
           -> cardinality checks (1 digest / 1 key-hash / 1 signature)
           -> vector-table sanity -> g6he_image_info_t
```

**Firmware update (BR-008…BR-012), device-side model:**

```
33-byte report B2 AA (55|56) len ~len seq payload csum
   -> g6he_frame_parse -> g6he_request_t
   -> g6he_updater_handle (0x60..0x67 state machine)
   -> g6he_response_t [type seq cmd status data]
   -> g6he_response_encode -> one or more 33-byte B1 reports
```

**Hall calibration (BR-014):**

```
raw x (0..70) -> segment lookup over {0,10,20,30,50,70}
             -> y_lo + round_half_up((x-x_lo)*(y_hi-y_lo)/(x_hi-x_lo))
             -> clamp/identity at x==0 and x>=65
             -> FUN_2001ada4 second value: 0, 600-interp(65-offset), or 600
```

## 5. Hardware abstraction seams

The target-ready layer introduces explicit seams; the original modules keep
none.

- `g6he_sensor_transport_t` — `read`/`write`/`reset`/`motion_asserted` callbacks.
  A Zephyr port supplies SPI-backed callbacks; the host tests supply mocks.
- `g6he_power_backend_t` — `read_state`/`set_ship_mode`. A Zephyr port wraps the
  official NCS `npm1300_charger`/`regulator`/`fuel_gauge` drivers.
- `g6he_wireless_backend_t` — `send_hid_report`/`start_pairing`. A Zephyr port
  wraps the BLE HID stack or the proprietary 2.4G stack.
- `g6he_optomagnetic_feed_hall_raw` / `_feed_ir` — the caller reads the ADC and
  the GPIO; the module owns only fusion and debounce.
- `g6he_hall_curve_t` — per-axis calibration data is caller-supplied, so the
  unknown compiled tables do not have to be fabricated.
- `g6he_board.h` — every pin and safety electrical, with compile-time blockers.

## 6. Clean-room process

1. Behaviour was read from the English evidence documents and from the public
   MCUboot/HID/FIPS specifications.
2. Reconstructed names (`g6he_hall_interpolate`, `dpi_stages`, …) are clearly
   labels, never asserted original vendor identifiers.
3. No decompiler text is present in this tree. The decompilation lives in `../`
   and is referenced only by address and evidence ID.
4. Where evidence was insufficient, the behaviour is a documented stub or a
   caller-supplied parameter, not a guess (see `unknowns.md`).

## 7. Build

Hosted C11, `-Wall -Wextra -Werror`. Two host artifacts:

- `build/g6he_tests` — 489 checks, 0 failures;
- `build/ENGINEERING-UNSIGNED-NOT-FOR-FLASHING` — the smoke executable of the
  Zephyr target entry point, unsigned and non-flashable by design.

No dynamic allocation except the read-only integration loader in the tests.
Verification logs live in `docs/verification/`.
