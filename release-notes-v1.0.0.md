# G6 HE Firmware Updater for macOS v1.0.0

First public Apple Silicon release.

## Included

- Native macOS graphical interface
- Self-contained HID upgrade engine; no Python installation required
- Bundled signed firmware `G6HE_v1.0.0+84_202609101503.signed.bin`
- Device/model, MCUboot structure, SHA-512, protocol and CRC safety checks
- Firmware selection, dry-run validation, explicit confirmation and progress UI

## Verified hardware result

- Device: Keychron G6 HE 8K, model `54LMG6HE`
- Upgrade: `1.0.0+82` to `1.0.0+84`
- Post-restart device query: `1.0.0+84`

## macOS signing status

The app is ad-hoc signed and has not been Apple-notarized because no Developer
ID certificate was available on the build host. On first launch, macOS may
require Control-click/right-click → Open. The release includes SHA-256 checksums.
