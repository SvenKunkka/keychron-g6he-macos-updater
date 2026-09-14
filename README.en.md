# Keychron Mouse Firmware Updater for macOS

[简体中文](README.md) | [English](README.en.md)

A native graphical firmware updater for Keychron mice on Apple Silicon Macs.
The workflow is straightforward:

1. Connect and select a Keychron mouse over USB.
2. Select the matching `.signed.bin` firmware image.
3. Validate the device, firmware identity, and version direction before updating.

The application includes a standalone update engine and does not require Python,
Wine, or Windows. To reduce the risk of flashing an incompatible image, generalized
device support is limited to the observed Keychron HID update protocol and never
bypasses the device bootloader's signature verification.

> This is a clean-room interoperability implementation. The current build is ad-hoc
> signed and has not been notarized by Apple.

## Compatibility and validation

The updater discovers Keychron USB devices using vendor ID `0x3434` on HID usage page
`0x008c`, usage `0x0001`. It then reads the device model, installed firmware version,
and protocol capabilities directly from the selected device.

A firmware image can proceed to the confirmation step only when all of the following
conditions are met:

- The MCUboot container, digest, key-hash TLV, and signature TLV are structurally valid.
- The device reports protocol version 1, DFU version 0, and standard update support.
- The firmware contains an exact match for the selected device model.
- Known device profiles pass USB PID, RAM address, and vector-range validation.
- Firmware downgrades receive a separate warning and confirmation.

| Device | USB PID | Validation status |
|---|---:|---|
| Keychron G6 HE 8K / `54LMG6HE` | `0xd086` | Hardware-verified update from `1.0.0+82` to `1.0.0+84` |
| Other Keychron mice reporting the same protocol | Detected dynamically | Protocol-compatible; an exact device/firmware pair still requires hardware acceptance testing |

Detecting a device and recognizing its protocol does not mean that its complete firmware
update flow has been hardware-verified.

## Installation and use

1. Download and open `Keychron-Mouse-Firmware-Updater-v1.1.0-macOS-arm64.dmg`.
2. Drag **Keychron Mouse Firmware Updater** into the Applications folder.
3. If macOS blocks the first launch, right-click the app and choose **Open**.
4. Switch the mouse to wired mode and connect it directly to the Mac over USB.
5. Select the mouse in the app, then select its matching signed firmware image.
6. Review the model, version, and trust information before confirming the update.

The updater prevents automatic Mac sleep during transfer. If a transport error occurs,
it attempts to return the device to a recoverable state. Do not disconnect USB or close
the application while an update is in progress.

## Building from source

The build machine requires Apple Command Line Tools, Homebrew, and `hidapi`:

```bash
brew install hidapi
./scripts/build_release.sh
```

Build outputs are written to `dist/` and include a macOS DMG, an app ZIP archive, and a
versioned SHA-256 checksum file.

## Command-line interface

```bash
python3 keychron_mouse_updater.py devices
python3 keychron_mouse_updater.py probe --device DEVICE_ID
python3 keychron_mouse_updater.py inspect firmware.signed.bin
python3 keychron_mouse_updater.py upgrade firmware.signed.bin --device DEVICE_ID --dry-run
python3 keychron_mouse_updater.py upgrade firmware.signed.bin --device DEVICE_ID --confirm DEVICE_MODEL
```

An intentional downgrade also requires `--allow-downgrade`.

## Firmware and signature trust boundary

- The app does not bundle or automatically select firmware. Users must choose a
  vendor-signed image that matches the selected device model.
- A recognized release hash is reported as `known_release_hash`; other structurally
  valid images are reported as `bootloader_signature_only`.
- Without a configured Keychron release public key, the host can validate the presence
  and structure of signature metadata but cannot independently authenticate it. The
  device bootloader remains the final signature authority.
- The MIT license applies only to the application source code. Firmware remains subject
  to its original rights and licensing terms.

## License

Application source code: [MIT](LICENSE). Third-party components:
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
