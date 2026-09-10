# Keychron Mouse Firmware Updater for macOS v1.1.0

## New

- General Keychron mouse selection instead of a fixed G6 HE target.
- Dynamic discovery of compatible Keychron firmware-upgrade HID interfaces.
- Exact selected-device model matching against the chosen firmware.
- Clear distinction between hardware-verified profiles and protocol-compatible,
  unverified models.
- Firmware is selected by the user; the app package no longer bundles one model's image.

## Safety improvements

- Requires exactly one valid image digest, key-hash and signature TLV.
- Validates vector-table sanity and known-device RAM ranges.
- Detects and separately confirms firmware downgrades.
- Retries native `hid.HIDException` transport errors with bounded backoff.
- Prevents macOS sleep during transfer.
- Attempts a best-effort device reset when transfer is interrupted.
- Reacquires the selected model after USB re-enumeration before declaring success.

## Verified profile

- Keychron G6 HE 8K, model `54LMG6HE`, USB PID `0xd086`.
- Previously observed successful update: `1.0.0+82 → 1.0.0+84`.

Other Keychron devices using the same queried protocol are selectable but remain marked
as unverified until their exact device/firmware pair completes an end-to-end acceptance
test.

## macOS signing

The application is ad-hoc signed and is not Apple-notarized because no Developer ID
certificate is available on this build host.
