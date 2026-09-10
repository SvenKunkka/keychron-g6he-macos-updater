# G6 HE `1.0.0+84` firmware and macOS updater bug-risk audit

Date: 2026-09-10  
Firmware SHA-256: `38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a`

> Version note: findings F-001 through F-005 below describe the v1.0.0 updater.
> v1.1.0 now requires the digest/key-hash/signature TLVs, validates vectors and
> known-profile ranges, retries HID exceptions, attempts reset after interruption,
> prevents sleep and separately confirms downgrades. The host still lacks the Keychron
> release public key, so the device bootloader remains the final signature authority.
> F-006 is a firmware release-traceability issue and is not fixed by updater code.

## Executive result

The supplied firmware file is internally consistent and matches the previously
installed `1.0.0+84` artifact. No confirmed crash or data-corruption defect was
proven inside the firmware binary by static analysis alone.

The inspected v1.0.0 updater had five confirmed functional failure-handling/validation gaps and one release-traceability
issue exist in the current macOS updater/package. The two highest-priority items are
incomplete host-side firmware validation and the absence of a recovery/reset path after
transfer errors. Four firmware behaviors need targeted hardware validation before this
build should be considered broadly release ready.

## VERIFIED

### V-001 — Container and identity are internally consistent

- Evidence class: FACT
- Confidence: 100 (Confirmed)
- The file length, MCUboot header, payload boundary, TLV boundary, embedded SHA-512,
  target model, version and vector table all agree.
- Header and payload both identify version `1.0.0+84` and target `54LMG6HE`.
- Initial SP `0x20063950` and all populated vector targets are in the nRF54LM20A RAM
  address range. Every populated exception vector has its Thumb bit set.
- No common plaintext private-key/token pattern was found.

### V-002 — RAM-load layout fits the MCU address space

- Evidence class: FACT
- Confidence: 100 (Confirmed for address layout only)
- MCUboot flag `0x20` denotes `IMAGE_F_RAM_LOAD`.
- Mapped file end is `0x200562e9`; initial SP is `0x20063950`; the 512 KiB RAM limit
  used for this check is `0x20080000`.
- Address-space fit does not prove runtime heap/stack headroom.

### V-003 — Hall interpolation constants are structurally safe

- Evidence class: FACT
- Confidence: 95 (High)
- Decompiled interpolation code divides by the delta between adjacent calibration
  breakpoints.
- The embedded breakpoint sequence is `0, 10, 20, 30, 50, 70`, so the divisor is
  non-zero for the compiled defaults. The output curves are monotonic and capped at
  `600` for the inspected paths.

## CONFIRMED updater defects

### F-001 — Host validation accepts unauthenticated or impossible images

- Severity: High
- Evidence class: FACT
- Confidence: 100 (Confirmed by offline mutation tests)
- The parser rejected a payload changed without updating its SHA-512, but accepted
  each of the following:
  - one-bit corruption of the Ed25519 signature;
  - removal of both the Ed25519 signature and key-hash TLVs;
  - a load address outside nRF54LM20A RAM;
  - zero initial-SP and reset vectors;
  - an older header version after recomputing only the unkeyed SHA-512.
- Consequence: the app may start a long transfer for a malformed, unsigned or
  incompatible image and only discover failure at the device bootloader. Bootloader
  signature enforcement remains the final safety boundary, but the host should fail
  closed before changing device state.
- Recommended fix: require exactly one supported digest, key-hash and Ed25519
  signature TLV; validate lengths, header flags, load range, vector range, target
  identifier and a trusted release SHA-256. Verify Ed25519 on the host once the vendor
  public key is available.

### F-002 — Transient hidapi failures bypass the five-attempt retry

- Severity: High
- Evidence class: FACT
- Confidence: 100 (Confirmed by source and runtime type inspection)
- The chunk loop retries only `OSError` and `ProtocolError`.
- `hid.HIDException` inherits directly from `Exception`, not `OSError`, so a normal
  hidapi transport exception exits immediately instead of retrying.
- Recommended fix: include `hid.HIDException` in the retry policy, add short bounded
  backoff, and distinguish timeout, disconnect and permanent protocol rejection.

### F-003 — No recovery/reset is attempted after entering update mode

- Severity: High
- Evidence class: FACT
- Confidence: 100 (Confirmed by control-flow review)
- After commands `0x62` and `0x63`, any exception during the 21,935 chunk transfers or
  CRC check leaves the function without attempting `0x66` reset or a reconnect-based
  recovery flow.
- Consequence: the mouse may remain in an update state until physical reconnect, and
  the UI cannot tell whether it is safely recoverable.
- Recommended fix: track the updater phase and use a bounded `finally` recovery path;
  after reconnect, query the running version and present exact recovery instructions.

### F-004 — The app warns against sleep but does not prevent it

- Severity: Medium
- Evidence class: FACT
- Confidence: 100 (Confirmed by source review)
- The transfer spans 21,935 acknowledged HID chunks. The UI warns the user not to let
  the Mac sleep, but no macOS power assertion is acquired for the critical section.
- Recommended fix: hold an `IOPMAssertion` (or equivalent process-scoped assertion)
  from immediately before `0x62` until reset/recovery completes.

### F-005 — Downgrades are not blocked or explicitly confirmed

- Severity: Medium
- Evidence class: FACT plus conditional impact
- Confidence: 100 that the host check is absent; impact depends on bootloader policy
- The updater checks only equality with the current version. Any different version,
  including an older signed image, proceeds after the same model confirmation.
- Recommended fix: compare semantic version plus build number and require a distinct
  `--allow-downgrade`/GUI confirmation. Preserve any device-side security-counter rule.

### F-006 — Package filename time predates embedded build time

- Severity: Low (release traceability)
- Evidence class: FACT
- Confidence: 100 (Confirmed)
- Filename suffix: `202609101503` (15:03).
- Embedded build strings: `15:10:22` and `15:10:32` on `Sep 10 2026`.
- Recommended fix: generate the filename and release manifest from signed-image build
  metadata after signing, not from a pre-build job timestamp.

## PARTIAL — firmware behaviors that still need hardware tests

### R-001 — BLE privacy and bond-state lifecycle

- Evidence class: FACT plus hypothesis
- Confidence: 70 (Probable test risk, not a confirmed defect)
- The binary contains and references `mouse_ble/rpa`, `ppt_ptx/bond` and `bt/keys`
  settings namespaces.
- Test risk: a persisted RPA may fail to rotate, or bond/RPA cleanup may diverge after
  firmware update, factory reset or switching among hosts.
- Required test: capture advertising addresses over time, across power cycles and
  after bond deletion; verify old hosts cannot reconnect after reset.

### R-002 — Invalid TX-power value silently falls back to 0 dBm

- Evidence class: FACT plus hypothesis
- Confidence: 65 (Possible)
- The binary contains the diagnostic text `TX power to enumerator conversion failed,
  defaulting to 0 dBm`; a direct code reference was not recovered in this stripped
  binary.
- Test risk: corrupt or out-of-range configuration could silently increase/decrease
  range and power consumption rather than reporting a recoverable configuration error.
- Required test: exercise every supported power level and invalid persisted values,
  while measuring current and RF output.

### R-003 — Runtime RAM high-water under simultaneous 8K USB/BLE activity

- Evidence class: FACT plus hypothesis
- Confidence: 60 (Possible)
- The RAM-loaded image ends 55,100 bytes below the initial stack pointer; 116,400 bytes
  remain from the initial SP to the top of the 512 KiB RAM range.
- These are layout margins, not free-memory measurements; `.bss`, heaps, stacks and DMA
  buffers cannot be reconstructed reliably without the linker map.
- Required test: enable stack/heap watermarking and run 8K motion, BLE reconnect,
  configuration writes, charging and sleep/wake together for at least eight hours.

### R-004 — Persistent Hall settings recovery

- Evidence class: FACT plus hypothesis
- Confidence: 55 (Possible)
- Dual settings keys `hall/mem0`, `hall/mem1`, `hall/ax0` and `hall/ax1` exist, and
  reset/default paths are present.
- Required test: use a factory test build or debugger to inject truncated, stale and
  out-of-range records; verify both axes return to safe defaults without boot loops,
  divide faults or asymmetric behavior.

## BLOCKED

- The G6 HE USB upgrade interface was not present during this audit. A planned
  100-iteration read-only version-query stress test could not start; zero live device
  commands were sent in that attempt.
- Power-loss/unplug recovery remains intentionally untested on the user's mouse.
- Ed25519 authenticity cannot be independently verified without the matching public
  key or a vendor-published trust anchor.
- Runtime stack/heap use, polling stability, RF behavior and battery impact cannot be
  proven from this binary alone.

## NEXT

1. Fix F-001 through F-005, rebuild the app, and rerun unit plus offline mutation tests.
2. Reconnect one G6 HE in wired mode and run 100 read-only identity/version queries,
   USB descriptor checks, sleep/wake, cable reconnect and 8K polling-loss tests.
3. Use a sacrificial/recoverable unit for controlled interruption at 10%, 50% and 90%
   transfer progress; verify bootloader re-entry and successful reflashing each time.
4. Run BLE address rotation/bond cleanup, low-battery, charging, rapid host switching
   and eight-hour combined-load tests with logs and current measurements.

## Generated evidence

- `firmware_static_report.json`: deterministic MCUboot, vector, memory, string, entropy
  and plaintext-secret checks.
- `ghidra_findings.txt`, `ghidra_custom_functions.txt`, `ghidra_hall_math.txt` and
  `ghidra_callers.txt`: Ghidra 12.1.3 analysis/decompilation excerpts.
- `scripts/test_validation_boundaries.py`: reproducible, offline validation-boundary
  cases; it never opens a HID device.

## Authoritative references

- Nordic nRF54LM20A product specification, key features:
  https://docs.nordicsemi.com/r/bundle/ps_nrf54lm20a/page/keyfeatures_html5.html
- MCUboot image design and RAM-load flag:
  https://github.com/mcu-tools/mcuboot/blob/main/docs/design.md
