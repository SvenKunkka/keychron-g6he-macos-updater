# Unknowns and deliberate stubs

These are behaviours the evidence does not support implementing. They are kept
out of the code rather than guessed, per the project rule that an unverified
function must not be described as complete. Each entry names the safest
non-destructive way to close it.

## Algorithm / calibration

| ID | Unknown | Current evidence | Impact if wrong | Next safe check |
|---|---|---|---|---|
| U-101 | Hall compiled `y` tables: full values, physical units, axis assignment | N-001/FACT dimensions (114 + 120 float32, ranges and 12-value prefix); role INFERENCE 80 | Pointing/calibration curve incorrect at runtime | Read-only settings dump of a connected unit, or vendor config table |
| U-118 | Exact boundaries of the two compiled float tables | Re-derived this run: strict-monotonic predicate gives 114/120, non-decreasing gives 122/123; no count word or pointer to the tables exists in the payload | A transcribed table could be off by 2+ elements at either end | Same as U-101. **The tables were therefore NOT embedded**; see final report |
| U-102 | Exact `FUN_2001ac20` index/table layout (the reconstructed segment lookup) | Decompilation structure + V-003 | Off-by-one segment at some x values | Trace the table pointer once in an authorized debugger session |
| U-103 | `FUN_2001ada4` second-value formula beyond the structural reading | Decompilation (`600 - interp(65-offset)`) | Derived value wrong near boundaries | Same debugger trace as U-102 |
| U-104 | Meaning of the recovered scalar defaults `0.36618003 → 0.36465910` and three neighbours | N-002/FACT values only | Wrong config default after any port | Vendor configuration table / read-only settings diff |

## Firmware runtime

| ID | Unknown | Current evidence | Impact | Next safe check |
|---|---|---|---|---|
| U-105 | Device-side update command semantics, target size and flash partition | E-005/FACT host contract; device actions INFERENCE | A reimplemented updater could accept/reject differently | Read-only HID capture of an official update on a sacrificial unit |
| U-106 | Update-protocol reject status codes (only 0 is evidenced) | Decompiled config protocol uses 8/9; reused here as labels | Host error categorisation differs | Same capture as U-105 |
| U-107 | Reset/post-restart timing and re-enumeration behaviour | E-007/FACT single successful run; U-003 open | Recovery assumptions wrong | Non-destructive repeated read-only version queries |
| U-108 | Sensor transport (SPI/I²C), ADC channels, GPIO map, DPI button | Strings/adc driver names only | A port cannot drive the hardware | Board schematic / devicetree |
| U-109 | BLE stack configuration, RPA rotation and bond lifecycle | R-001/PARTIAL | Privacy/bond regressions | Passive BLE address capture across power cycles |
| U-110 | Power states, sleep timers and the meaning of the recovered 30/25 values | `FUN_2001ba14` values only | Battery/runtime regressions | Bench current measurement |
| U-111 | Debounce/long-press/multi-click algorithm and DPI-cycle order | Not evidenced in this artifact | Input behaviour wrong | Logic-analyzer/HID capture from normal use |
| U-112 | Persisted settings layout and the runtime effect of the `hall/ax0`→`ax0` rename | N-004/FACT strings, UNKNOWN effect | Silent default fallback after update | Read-only settings dump before/after an official update |
| U-113 | `KCFWID` tag semantics beyond tags 0x01 (model) and 0x02 (version) | T-002 layout only | Identity parse incomplete | More captured 0x60 responses from other models |

## Release / build

| ID | Unknown | Current evidence | Impact | Next safe check |
|---|---|---|---|---|
| U-114 | Ed25519 public key / trust anchor | U-001/FACT no public key shipped | Signature cannot be host-verified | Vendor public key publication |
| U-115 | Linker map, `.bss`/`.data`/heap/stack high-water | S-001 address-space only | No RAM-safety claim possible | Vendor `.map`/`.elf` or instrumented engineering build |
| U-116 | Code-level delta between `+84` and `+87` | D-001/FACT build not bit-reproducible | A code-only fix could be missed | Official build environment or linker map |
| U-117 | Signing command, key identity and G9-specific hardware | U-001, G-5…G-12 | No signed/flashable artifact may be produced | Vendor build+signing pipeline, G9 baseline and board docs |

## Hardware mapping (G6_HE_V1.3 production package, this run)

The production packages contain no source schematic or netlist, so every pin
mapping remains UNKNOWN. The detailed private evidence register uses IDs HW-U-001 …
HW-U-043; the most consequential public findings are:

| ID | Unknown | Impact |
|---|---|---|
| HW-U-001 | no schematic/netlist in any package | no complete netlist claim is possible |
| HW-U-002 | blind/buried via layer span (BOM: `四层盲埋孔`; 28 × 0.1 mm holes) | inner-layer paths stay UNKNOWN |
| HW-U-010/011/012/013 | nRF54LM20A ball map; sensor SPI; PMIC TWI; Hall/IR pins | no target build |
| HW-U-030…034 | charge current, termination voltage, rails, NTC, `350mah4v4`→`4v35` meaning | **safety** |
| HW-U-035 | U3/U4 AW86214 and U5 SGM66051 are in placement but absent from both BOMs | population risk |

## Battery profile string (resolved to an identifier, not a voltage)

`350mah4v4` (+84) → `350mah4v35` (+87) is a **battery fuel-gauge profile
identifier** inside a `float32` parameter block; the configured cell in the
production documentation is **350 mAh / 4.4 V** and the specification states
charge current ≈250 mA. Whether the identifier also selects nPM1300 charger
register values is **UNKNOWN**, and no safe charge voltage may be inferred from
the string. The underlying production documents remain in the private evidence
workspace.

## Process boundary

This tree performs **no** device access and **no** signing. Any closure of the
unknowns above that involves a device must be re-authorized, must be read-only
where stated, and must never bypass secure boot, signature checks, readout
protection or debug locks.
