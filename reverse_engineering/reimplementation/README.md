# G6 HE clean-room behavioural reimplementation

Status: **host-verified behavioural model of the documented G6 HE subset — not a
replacement firmware and not flashable.**

Public archive: [`PUBLIC_SNAPSHOT_2026-09-14.md`](PUBLIC_SNAPSHOT_2026-09-14.md).

This tree is a clean-room, readable reimplementation of the firmware behaviours
that the existing G6 HE analysis recovered to a testable level. It was written
from the English evidence documents in `../` (`evidence.md`,
`firmware_static_report.json`, `bug_risk_audit.md`, `ghidra_hall_math.txt`,
`ghidra_callers.txt`) and from public specifications (MCUboot image format, USB
HID, FIPS 180-4, CRC-32). No vendor source, no decompiler output and no
extracted vendor data were copied into this tree.

## What it is, and what it is not

It **is**:

- A modular C11 library plus host test suite that reproduces, with passing
  tests, specific recovered behaviours:
  - MCUboot container parse and SHA-512 digest verification (BR-001…BR-005);
  - the HID firmware-update frame codec, checksums and command/state model
    (BR-008…BR-012);
  - the Hall calibration piecewise-linear interpolation with the recovered
    breakpoints, rounding and clamp (BR-014);
  - the recovered factory default parameters (BR-015);
  - the device identity, capability and `KCFWID` records (BR-006, BR-007,
    BR-017);
  - semantic version comparison (BR-013).
- A **target-ready, fail-closed architecture** on top of the behavioural model
  (BR-020…BR-027): common event model, optomagnetic chain, PAW3950/3955
  transport and variant selection, nPM1300 power interface, settings schema with
  the explicit `hall/ax0`→`ax0` migration, USB HID report model, and BLE/2.4G
  stubs. `include/g6he_board.h` refuses to compile when hardware is enabled
  without evidenced pins and safety-critical electrical constants.
- Verified against the real `1.0.0+84` **and** `1.0.0+87` artifacts read-only:
  sizes, SHA-256 and SHA-512 digests, MCUboot headers, four TLVs, vector tables,
  whole-file CRCs, model strings, KCFWID identity and the update-protocol
  descriptor all reproduce — including the `+86` filename / `+87` embedded
  version mismatch. The `+84` artifact is checked **absent** of the G9 model
  string.

It is **not**:

- A complete firmware, an RTOS port, a radio/USB/sensor driver implementation, a
  linker map or a bootable image. There is no nRF54LM20A target build here; the
  Zephyr project under `zephyr/` is target-ready but refuses to enable hardware
  (see `docs/nrf54lm20a_bringup_plan.md`).
- Flashable. Nothing here produces a `.signed.bin`; there is no signing step and
  no key material. The only executable target output is
  `build/ENGINEERING-UNSIGNED-NOT-FOR-FLASHING`. Do not attempt to flash
  anything built here.
- A claim of runtime equivalence. No device was written to or observed. Almost
  all behavioural claims are static and are labelled by evidence class.

## Build and test

Requires a C11 compiler and `make`. No third-party libraries.

```bash
cd reverse_engineering/reimplementation
make test          # 489 checks, 0 failures
make target-smoke  # builds build/ENGINEERING-UNSIGNED-NOT-FOR-FLASHING
```

Expected tail of `make test`:

```
[protocol]
[integration]
  1.0.0+84: <authorized-input>/G6HE_v1.0.0+84_202609101503.signed.bin (350953 bytes)
  1.0.0+87: <authorized-input>/G6HE_v1.0.0+86_202609141615.signed.bin (352225 bytes)
[target_arch/events]
...
[target_arch/wireless]

489 checks, 0 failures
RESULT: PASS
```

The integration section reads `+84` from `G6HE_REAL_IMAGE` (tree-relative
fallback) and `+87` from `G6HE_REAL_IMAGE_87`. If either artifact cannot be
located that vector prints `SKIP` and the remaining checks still run.

The public snapshot records the clean build, UBSan, clang static analyzer,
ASan-environment limitation, and fail-closed results. Raw local logs are excluded
because they contain machine-specific paths; rerun the documented commands to produce
fresh logs on another host.

## Evidence and confidence

Every material claim uses the project evidence classes:

| Label | Meaning |
|---|---|
| FACT | directly observed and reproducible from a named artifact |
| INFERENCE | best explanation supported by several facts |
| HYPOTHESIS | plausible but needs a named test |
| UNKNOWN | not currently supportable |

See `docs/behavioral_requirements.md` for the requirement list,
`docs/traceability.md` for requirement → evidence → test, and
`docs/unknowns.md` for what is deliberately not implemented.

## Hardware-safety boundary

This tree contains no device access code, no flash routine, no bootloader
command sender and no signature logic. It was built without contacting any
device. Any real-device validation is a separate, owner-authorized activity.
