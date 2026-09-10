# Remaining boundaries

## U-001 — Signature public key

- Status: Open, non-blocking for the verified device flow.
- The package includes an Ed25519 signature and key hash but not the public key.
- Host-side SHA-512 integrity is verified; signature authenticity remains the device
  bootloader's responsibility.

## U-002 — Alternate-device bootloader identity

- Status: Open, out of scope for the connected G6 HE revision.
- The connected device declares that no separate bootloader transition is required.
- The tool rejects any future device that requests `0x67`, rather than guessing its
  bootloader VID/PID or recovery path.

## U-003 — Recovery from physical interruption

- Status: Not destructively tested.
- Power-loss/unplug recovery was not induced on the user's mouse. This is intentionally
  not claimed by the end-to-end success result.
