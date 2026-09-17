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
- Twenty-five tests pass: MCUboot success/tamper/magic checks, captured query frame,
  update-frame marker, captured two-report response assembly, updater CRC vector,
  device-compatibility classification, post-restart verification behaviour, and
  version-order classification including the parallel-release-line cases.

## E-009 — USB product ID is not a stable model identity

- Class: FACT
- Confidence: 100 (Confirmed live)
- A second `Keychron G6 HE 8K` reports `VID 0x3434 / PID 0xd09d` on every one of its
  interfaces (including the upgrade interface: interface 3, usage page `0x008c`,
  usage `0x0001`), with `release_number 0x0100`, `hardware_revision 0503`, model
  `54LMG6HE`, firmware `1.0.0+0`, bootloader model `54LMv1.0`, protocol 1, DFU 0,
  update modes `0x01`, no bootloader switch required.
- The unit recorded in E-006/E-007 reported `PID 0xd086` for the same model. The
  product ID therefore varies within one model, consistent with a per-batch,
  per-board or per-firmware-build descriptor rather than a fixed model identity.
- Consequence: treating an unrecorded product ID for a known model as a hard
  incompatibility rejected a genuine device. Product ID is now advisory; identity is
  carried by the model string returned over the protocol, the model string embedded
  in the signed image, and the profile RAM/vector layout. `--strict-product-id`
  restores the strict behaviour.
- Offline validation on the `0xd09d` unit: firmware
  `38f5bbb0…95a` inspects as a valid MCUboot image with CRC `0x482f47d5` (matching
  E-007), and `upgrade --dry-run` returns `status: ready`,
  `version_relation: upgrade`, `firmware_trust_status: known_release_hash`,
  `write_attempted: false`.
- Not yet established: no write was attempted on this unit, so the `0xd09d` pair has
  not completed an end-to-end upgrade acceptance run and is reported as
  `protocol_compatible_unverified_product_id` rather than `hardware_verified`. In
  particular, whether this unit reports `0xd086` after writing `1.0.0+84` remains
  unconfirmed.

## E-010 — Version build counters are per release line, not global

- Class: FACT
- Confidence: 100 (Confirmed live and from vendor packages)
- The updater sorts version strings numerically and reports only the ordering
  (`newer` / `older` / `same` / `unknown`). It does not label a result an "upgrade"
  or a "downgrade", because the meaning of a build counter is a vendor release
  convention.
- Vendor packages observed side by side show independent counters:
  public-line builds `1.0.0+66`, `+67`, `+84`, `+87`; internal-test (`_nc_`) build
  `1.0.0+1`, whose filename timestamp `202609161520` is *later* than the `+84`
  package's `202609101503`. A lower build number therefore does not imply an older
  image.
- The `0xd09d` unit was updated to `1.0.0+1` from
  `G6HE_nc_v1.0.0+1_202609161520.signed.bin`
  (SHA-256 `9adb197d778e09b6e12a1cd7baadee4d437afcd56c35f36d23fc82d8d152533b`,
  353,481 bytes) and read back `1.0.0+1` afterwards. This is the first completed
  write acceptance run for a `0xd09d` unit. The tool did not cause this write; it
  was performed by the user before the version-ordering behaviour was reviewed.
- Filenames are not a reliable version source: `G6HE_v1.0.0+86_202609141615.signed.bin`
  contains an image whose embedded version is `1.0.0+87`. The updater's `inspect`
  output, not the filename, is authoritative.

## E-011 — Equal version strings do not prove equal content

- Class: FACT
- Confidence: 100 (Confirmed by code review and unit tests)
- The device's `0x60` identity response reports a version string only; the updater
  cannot read back the running image digest over the protocol.
- Previously `upgrade_firmware` returned `already_current` and skipped the write
  whenever the target version string equalled the running one, which silently
  refused an intentional same-version reflash of different content.
- The write is now skipped only when the image hash recorded as running for that
  model and version (`_RUNNING_IMAGE_HASHES`) matches the selected file. Otherwise
  the result carries `version_order_note` and enters the confirmation flow.
- `_RUNNING_IMAGE_HASHES` is a record of what this project has observed running, not
  a general map; for an unrecorded model/version pair the tool reports that identical
  content cannot be confirmed rather than assuming it.
