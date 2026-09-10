# Evidence register

## E-001 — Input identity

- Class: FACT
- Confidence: 100 (Confirmed)
- Firmware: 350,953 bytes, SHA-256
  `38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a`
- Windows updater: 2,228,224 bytes, SHA-256
  `c49533765b4f00db78c75585f43482b1bebd95df30549405ac49895565ac4b14`

## E-002 — Signed-image container

- Class: FACT
- Confidence: 100 (Confirmed)
- Header magic `0x96f3b83d`; TLV magic `0x6907`; header `0x800` bytes;
  image `0x55214` bytes; TLV starts at `0x55a14` and ends at EOF `0x55ae9`.
- Embedded SHA-512 matches the complete header plus payload.
- An Ed25519 signature and key hash are present. Authenticity is not independently
  host-verified because the public key is not included; the device bootloader is the
  final signature authority.

## E-003 — Product and build identity

- Class: FACT
- Confidence: 100 (Confirmed)
- Header version is `1.0.0+84`.
- Payload strings include `Keychron G6 HE 8K`, `54LMG6HE`, `54LMv1.0`,
  `nrf54lm20a`, `Sep 10 2026`, and `15:10:32`.

## E-004 — Windows updater transport

- Class: FACT
- Confidence: 100 (Confirmed by decompilation)
- The PE32 MFC application uses SetupAPI, HID.DLL, `CreateFileW`, `ReadFile`, and
  `WriteFile`.
- Its generic transport writes and reads fixed 33-byte reports. Report ID B2 is
  used for output and B1 for input on this device type.
- Frame header is `B2 AA (55|56) length ~length sequence`; payload checksum is the
  little-endian 16-bit sum of payload bytes. Responses echo sequence and command.
- The application flushes the HID input queue before each command. The macOS port
  reproduces this behavior to exclude asynchronous mouse reports.

## E-005 — Update state machine

- Class: FACT
- Confidence: 100 (Confirmed by decompilation and live responses)
- `0x60` queries identity/version; `0x61` capabilities; `0x62` selects mode;
  `0x63` starts; `0x64` transfers 16-byte chunks; `0x65` checks CRC; `0x66`
  resets; `0x67` is only for devices that declare a required bootloader switch.
- Normal responses use A3. Update-frame responses (`0x63`/`0x64`) use A1 on the
  connected G6 HE and still echo sequence/command/status.
- CRC is reflected polynomial `0xedb88320`, initial `0xffffffff`, no final XOR.

## E-006 — Connected device identity and compatibility

- Class: FACT
- Confidence: 100 (Confirmed live)
- `Keychron G6 HE 8K`: `VID 0x3434 / PID 0xd086`, release `0x0100`.
- Upgrade interface: interface 3, usage page `0x008c`, usage `0x0001`, output B2,
  input B1, 33 bytes including report ID.
- Device response: model `54LMG6HE`, firmware `1.0.0+82`, bootloader model
  `54LMv1.0`, protocol 1, DFU 0, update modes `0x01`, no bootloader switch needed.

## E-007 — End-to-end upgrade acceptance

- Class: FACT
- Confidence: 100 (Confirmed live)
- Firmware SHA-256:
  `38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a`.
- Transfer CRC: `0x482f47d5`; 350,953 bytes in 21,935 acknowledged chunks.
- Device accepted final CRC command and reset command.
- After USB re-enumeration, the device returned model `54LMG6HE` and firmware
  version `1.0.0+84`. This is the acceptance criterion for successful upgrade.

## E-008 — Local regression tests

- Class: FACT
- Confidence: 100 (Confirmed)
- Seven tests pass: MCUboot success/tamper/magic checks, captured query frame,
  update-frame marker, captured two-report response assembly, and updater CRC vector.
