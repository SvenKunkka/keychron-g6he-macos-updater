# Public research snapshot — 2026-09-14

This snapshot archives the current owner-authorized, clean-room G6 HE behavioural
reimplementation. It is published for interoperability, review, testing and continued
development. It does not claim to recover the vendor's original source code.

## VERIFIED

- A clean Apple Silicon host build completed with **489 checks and 0 failures**.
- UndefinedBehaviorSanitizer completed with no runtime errors.
- Clang static analysis completed without findings after one behaviour-preserving
  dead-store cleanup and regression test.
- The architecture rejects hardware enablement while the board pin table and
  safety-critical electrical values remain unknown.
- No device command, firmware write, signing action or protection bypass was performed.

## PARTIAL

- The portable event, optomagnetic input, PAW3950/3955 sensor, nPM1300 power, settings,
  HID and wireless interfaces are implemented and host-tested with mocks.
- Two owner-authorized G6 HE firmware releases were used read-only as local integration
  vectors. The newer file's name reports `+86` while its embedded version is `+87`.
- PCB production evidence confirms the main MCU and major component families, but does
  not provide a source schematic or complete signal connectivity.

## BLOCKED

- Exact nRF54LM20A GPIO, ADC, SPI and TWI assignments are unknown.
- Charger termination, current, rail and NTC configuration are not verified.
- The local target toolchain and an official signing pipeline are absent.
- G9 pin mapping and the additional DPI-button circuit/behaviour are unsupported by
  current evidence.

## NEXT

Obtain the G6 HE board pin map (schematic, netlist or vendor devicetree) and the battery
datasheet with the vendor nPM1300 charger configuration. Until then, keep target hardware
disabled and treat every build from this tree as engineering-only and not flashable.

## Public/private boundary

Published here: clean-room source, tests, interface documentation and evidence-labelled
conclusions.

Not published here: PCB production packages, BOM/placement exports, datasheets, newer
vendor firmware, decompiler output, private engineering logs, signing material or any
device-flashing workflow.
