# Behavioural requirements

Each requirement carries an evidence class and confidence band, and a status
that is one of:

- **implemented** — present in `src/`, exercised by a host test;
- **partial** — implemented for the evidenced part, UNKNOWN parts remain;
- **documented** — recorded but deliberately not implemented.

Evidence IDs (`E-`, `C-`, `T-`, `N-`, `V-`, `F-`, `R-`, `U-`) refer to the
documents in `../`.

| ID | Requirement | Evidence | Class / confidence | Status |
|---|---|---|---|---|
| BR-001 | Parse the MCUboot image header (magic 0x96F3B83D, load address, header size, protected-TLV size, image size, flags, version) and reject malformed values | E-002, C-001 | FACT / 100 | implemented |
| BR-002 | Verify the embedded SHA-512 digest over `header + payload` | E-002, C-002 | FACT / 100 | implemented |
| BR-003 | Require exactly one digest, one key-hash and one signature TLV, with valid lengths and reserved byte | E-002, F-001 | FACT / 100 | implemented |
| BR-004 | Validate the payload vector table: non-zero initial SP, Thumb reset vector, both inside RAM | V-001 | FACT / 100 | implemented |
| BR-005 | Accept the RAM-load layout only inside the nRF54LM20A window 0x20000000..0x20080000 | V-002, C-001 | FACT / 100 | implemented |
| BR-006 | Encode/decode the 34-byte identity record (model, hw revision, fw version, bootloader model/version) | E-006, captured frame | FACT / 100 | implemented |
| BR-007 | Extract model and version from the `KCFWID` blob | T-002 | FACT layout / INFERENCE tag meaning (80) | implemented |
| BR-008 | Build and parse the 33-byte HID request frame: report ID B2, `AA`, kind `55`/`56`, length complement, sequence, payload, 16-bit LE byte sum | E-004, captured frames | FACT / 100 | implemented |
| BR-009 | Reassemble a response from one or more 33-byte reports, validate ID/length/checksum | E-004, captured two-report response | FACT / 100 | implemented |
| BR-010 | 16-bit little-endian payload checksum | E-004 | FACT / 100 | implemented |
| BR-011 | Streaming CRC-32, reflected 0xEDB88320, init 0xFFFFFFFF, no final XOR | E-005, E-007 | FACT / 100 | implemented |
| BR-012 | Command model 0x60 identity, 0x61 capabilities, 0x62 select mode, 0x63 start, 0x64 16-byte chunks, 0x65 CRC check, 0x66 reset, 0x67 bootloader switch; A3 normal / A1 update responses | E-005, E-006 | FACT command set / INFERENCE device-side actions (60–80) | partial |
| BR-013 | Compare `v?X.Y.Z+B` versions and classify upgrade/same/downgrade/unknown | F-005, updater | FACT / 100 | implemented |
| BR-014 | Piecewise-linear Hall interpolation over breakpoints {0,10,20,30,50,70} with `(delta>>1)` rounding, x==0 identity and x>=65 clamp; second value `600-interp(65-offset)` | V-003, `FUN_2001ac20`, `FUN_2001ada4` | FACT structure / INFERENCE index reconstruction (70–85) | partial |
| BR-015 | Recovered factory defaults: DPI stages 2100/700/1600, alt 1300, timers 30/25, mode 2, flag 1 | `FUN_2003fd64`, `FUN_2001ba14` | FACT values / INFERENCE roles (75) | partial |
| BR-016 | Settings key registry and the `+84`→`+87` `hall/ax0`→`ax0` rename | T-001, N-004 | FACT strings / UNKNOWN effect | documented |
| BR-017 | Capability record: protocol, DFU, update modes, bootloader-required | E-006 | FACT fields / INFERENCE 4th byte (70) | implemented |
| BR-018 | Keep unknown firmware behaviour (radio, USB, sensor, power, DPI button, debounce, persist) as explicit stubs rather than guesses | U-001…U-003, R-001…R-004, G-5…G-12 | UNKNOWN | documented |
| BR-019 | Verify whole-file SHA-256 of the real artifacts against the recorded manifest digests | RAR-independent manifest | FACT / 100 | implemented |
| BR-020 | Normalise motion, wheel, button, mode and battery inputs into one timestamped event model with a bounded queue | architecture | INFERENCE (interface design) | implemented |
| BR-021 | Separate the optomagnetic chain into raw Hall, IR/contact, fusion, debounce and HID output, with no guessed threshold | vendor switch spec §7.1/§7.5, B1612 datasheet | FACT device structure / INFERENCE mapping (85) | partial |
| BR-022 | Model the PAW3950/3955 4-wire SPI transport and select the variant at bind time (max cpi 30 k / 40 k) | 3950/3955 datasheets Table 1 | FACT / 100 | partial |
| BR-023 | Expose an nPM1300 power/charger interface that refuses unknown charge current, termination voltage, rails and NTC | power_tree.md, NCS driver boundary | FACT limits / UNKNOWN values | implemented |
| BR-024 | Implement the evidenced `hall/ax0`→`ax0`, `hall/ax1`→`ax1` migration as an explicit, idempotent, versioned rename | +84/+87 string tables | FACT strings / UNKNOWN runtime effect | implemented |
| BR-025 | Model the USB HID report path: boot-mouse report plus the verified vendor update report IDs 0xB2/0xB1 (usage page 0x008c) | +84/+87 descriptor | FACT / 100 | implemented |
| BR-026 | Provide BLE HID and proprietary-2.4G interfaces as stubs/mocks; implement only the documented product timings | SPEC pairing/sleep rows | FACT timings / UNKNOWN RF | partial |
| BR-027 | Board/devicetree template with compile-time blockers for unknown pins and unsafe electrical constants | board package absence | FACT absence / UNKNOWN mapping | implemented |
| BR-028 | Correct the BR-015 DPI-stage role: the specification documents 400/800/1600/3200/5000, which does not match the recovered literals | SPEC 回报率/DPI rows | FACT spec vs FACT literals → role INFERENCE withdrawn | documented |

## Notes on the partial requirements

- **BR-012** reproduces the wire contract exactly (frames, checksums, command
  bytes, response types, cardinality) and models the device actions that the
  host contract implies. It is not evidence of the vendor's internal state
  machine; no G6 HE was driven during this work.
- **BR-014** reproduces the recovered *algorithm shape and rounding*. The
  per-axis `y` tables, their units and axis assignment are UNKNOWN and are
  caller-supplied (`g6he_hall_curve_t`). Only the two compiled tables'
  dimensions (114 and 120 float32) and a 12-value prefix were recovered.
- **BR-015** embeds the exact literals the decompiled initialisers store. Their
  semantic roles are reconstructed labels, not vendor names.
- **BR-016** is recorded because it is a release-traceability-relevant fact; the
  runtime effect of the key rename is UNKNOWN. BR-024 now implements the rename
  itself as an explicit, idempotent migration; the *runtime effect* remains
  UNKNOWN.
- **BR-021/BR-022/BR-026** implement the evidenced structure and refuse the rest.
  The optomagnetic thresholds, the sensor register map and all RF parameters are
  not implemented; target builds that enable them fail to compile (BR-027).
- **BR-028** is a correction discovered in this run. The electronic
  specification documents DPI stages 400/800/1600/3200/5000 (and the
  configuration table lists the same family), which does **not** match the
  previously recovered literal group 2100/700/1600 plus 1300. The earlier
  "DPI stage table" role for those literals is therefore **withdrawn**; their
  role is UNKNOWN again until a settings dump or vendor config table is read.
