# Clean-room traceability

Requirement → evidence → implementation → test. "Vector" names the concrete
input that makes the test meaningful; captured vectors come from
`tests/test_firmware_parser.py` and from the upstream Windows updater
decompilation (E-004/E-005).

| Requirement | Evidence IDs | Source | Test | Vector / assertion |
|---|---|---|---|---|
| BR-001 MCUboot header | E-002, C-001 | `src/g6he_mcuboot.c` | `test_mcuboot` | +84 header: load 0x20000800, hdr 0x800, img 348692, flags 0x20; synthetic 1.2.3+4 image; wrong magic |
| BR-002 SHA-512 digest | E-002, C-002 | `src/g6he_sha512.c`, `src/g6he_mcuboot.c` | `test_sha512`, `test_mcuboot` | FIPS "abc"/"" vectors; +84 embedded digest recomputed in C; tampered payload rejected |
| BR-003 TLV cardinality | E-002, F-001 | `src/g6he_mcuboot.c` | `test_mcuboot` | +84 = 4 TLVs; synthetic missing-signature rejected |
| BR-004 Vector table | V-001 | `src/g6he_mcuboot.c` | `test_mcuboot` | +84 SP 0x20063950 / reset 0x20021fc9; zero reset vector rejected |
| BR-005 RAM load range | V-002, C-001 | `src/g6he_mcuboot.c` | `test_mcuboot` | `g6he_mcuboot_ram_load_ok` true for +84 and synthetic |
| BR-006 Identity record | E-006, captured | `src/g6he_identity.c` | `test_identity` | captured 34-byte record → 54LMG6HE / 0503 / 1.0.0+82 / 54LMv1.0 / 1.0; encode round trip |
| BR-007 KCFWID blob | T-002 | `src/g6he_identity.c` | `test_identity` | observed blob → model 54LMG6HE, version 1.0.0+87 |
| BR-008 Request frame | E-004, captured | `src/g6he_protocol.c` | `test_protocol` | `b2aa5503fc01606000`; `b2aa5603fc04636300`; checksum/report-ID tamper rejected |
| BR-009 Response reassembly | E-004, captured | `src/g6he_protocol.c` | `test_protocol` | captured two-report 0x60 reply; encode/decode round trip |
| BR-010 Payload checksum | E-004 | `src/g6he_crc.c` | `test_crc` | {0x60}→0x0060; {0xFF,0x01}→0x0100 |
| BR-011 Streaming CRC-32 | E-005, E-007 | `src/g6he_crc.c` | `test_crc`, `test_mcuboot` | "123456789"→0x340BC6D9; +84 whole file→0x482F47D5 |
| BR-012 Command model | E-005, E-006 | `src/g6he_protocol.c` | `test_protocol` | 0x60/0x61 data; transfer-before-start rejected; start→updating; matching/mismatching CRC; reset; 0x67 and unknown rejected; A1 vs A3 |
| BR-013 Version relation | F-005 | `src/g6he_mcuboot.c` | `test_version` | v-prefix, upgrade/downgrade/same, unparseable→unknown, format "1.0.0+84" |
| BR-014 Hall interpolation | V-003, `FUN_2001ac20`, `FUN_2001ada4` | `src/g6he_hall.c` | `test_hall` | identity ramp; clamp at 65; half-up rounding at x=4/5; monotonic sweep; derived 0/599/600 |
| BR-015 Factory defaults | `FUN_2003fd64`, `FUN_2001ba14` | `src/g6he_defaults.c` | `test_defaults` | 2100/700/1600/1300, 30/25, mode 2, flag 1 |
| BR-016 Settings keys | T-001, N-004 | `src/g6he_settings.c` | `test_target_arch` | v1 store with `hall/ax0`/`hall/ax1` migrates to `ax0`/`ax1`, unrelated keys preserved, idempotent |
| BR-017 Capabilities | E-006 | `src/g6he_identity.c` | `test_identity`, `test_protocol` | {1,0,0x01,0} round trip |
| BR-018 Unknown stubs | U-001…U-003, R-001…R-004, G-5…G-12 | — | — | `unknowns.md` |
| BR-019 SHA-256 identity | prior manifest | `src/g6he_sha256.c` | `test_sha256`, `test_integration` | FIPS vectors; +84 `38f5bbb0…`, +87 `76e3afce…` recomputed in C |
| BR-020 Event model | architecture | `src/g6he_event.c` | `test_target_arch` | fill-to-capacity push fails without overwrite; typed constructors |
| BR-021 Optomagnetic chain | vendor switch §7.1/§7.5 | `src/g6he_optomagnetic.c` | `test_target_arch` | disabled config rejected; no-hysteresis rejected; IR debounce; Hall hysteresis; fusion holds on Hall |
| BR-022 Sensor transport | 3950/3955 datasheets | `src/g6he_sensor.c` | `test_target_arch` | unset variant rejected; incomplete transport rejected; max cpi 30000/40000; mock SPI read/write |
| BR-023 nPM1300 interface | power_tree.md | `src/g6he_power.c` | `test_target_arch` | unknown config rejected; bind/read; ship mode unsupported; 3.4/3.2 V classification |
| BR-024 Settings migration | +84/+87 tables | `src/g6he_settings.c` | `test_target_arch` | rename pair, conflict keeps newer key, version → 2 |
| BR-025 HID report | +84/+87 descriptor | `src/g6he_hid.c` | `test_target_arch` | button bits, motion clamp 127, wheel, boot encode, idle, 0xB2/0xB1/0x008c |
| BR-026 Wireless mocks | SPEC pairing/sleep | `src/g6he_wireless.c` | `test_target_arch` | send without backend rejected; send while disconnected rejected; 3 min pairing timeout → off |
| BR-027 Board blockers | board package absence | `include/g6he_board.h` | public snapshot + reproducible build | `-DG6HE_ENABLE_HARDWARE=1` yields 2 compile errors (pin table + electricals) |
| BR-028 DPI role correction | SPEC 回报率/DPI rows | — | — | documented; BR-015 role withdrawn |

## What the tests do and do not establish

- The tests establish that the implementation reproduces the named **static**
  facts and captured frame vectors, and that the real `+84` **and** `+87`
  artifacts parse and hash identically to independently recorded values
  (`test_integration`).
- The tests do **not** establish runtime equivalence. No device was driven, no
  flash was attempted, and no timing, radio, sensor, power or debounce behaviour
  was observed.
- The Hall interpolation tests use a caller-supplied identity ramp/scaled curve,
  because the compiled `y` tables are UNKNOWN. They verify the recovered
  algorithm structure and rounding, not the vendor's exact pointing curve.
- The target-architecture tests exercise mocks only. They establish that the
  fail-closed gates reject unknown configuration; they make no hardware claim.

## Hardware evidence traceability (this run)

| Evidence | Artifact | Destination |
|---|---|---|
| RAR1/RAR2/RAR3 archives | production packages | `hardware_evidence/archive_manifest.json` |
| HE BOM 3950/3955 | RAR1 | `bom_normalized.csv`, `component_variants.md` |
| HE placement | RAR1 | `placement_map.csv` |
| Gerber + drill | RAR1 | `gerber_layer_map.md`, `board_connectivity.csv` |
| Battery/charge spec | SPEC, CFG | `power_tree.md`, `optomagnetic_input_path.md` |
| Sensor pinout | 3950/3955 datasheets | `sensor_interface.md` |
| B1612 Hall | B1612 datasheet | `optomagnetic_input_path.md` |
| Unresolved mapping | all of the above | `hardware_unknowns.md` |

The derived summaries and proprietary inputs (Gerber, BOM, placement, PDFs and
extracted archives) remain in the private evidence workspace and are **not** copied
into this Git repository. This table preserves only the evidence classes and
traceability needed to explain why the public code remains fail-closed.
