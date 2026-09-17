# Keychron Mouse Firmware Updater for macOS v1.2.0

## Fixed

- A genuine Keychron G6 HE 8K could be reported as `协议不兼容` / `incompatible` with
  `known model 54LMG6HE has unexpected USB product ID 0xd09d`, leaving the upgrade
  button permanently disabled.
- Post-restart verification no longer requires the USB product ID to be unchanged.
  Matching only on the model prevents a successful write from being reported as a
  verification failure when newly written firmware declares a different product ID.

## Root cause

`KNOWN_DEVICE_PROFILES` recorded a single USB product ID per model (`0xd086` for
`54LMG6HE`) and treated any other product ID as a hard incompatibility.

The USB product ID is a descriptor supplied by the device firmware and board
configuration, so it can differ between production batches, hardware revisions and
factory firmware builds of the same model. The second unit used to reproduce this
report enumerates as `0xd09d` with `hardware_revision 0503` and factory firmware
`1.0.0+0`, while the original acceptance unit enumerated as `0xd086` on `1.0.0+82`.

## Changed

- Product ID is now advisory metadata for a known model. An unrecorded product ID
  produces `compatibility_warnings`, downgrades the reported status to
  `protocol_compatible_unverified_product_id`, and is displayed in the device panel
  and in the upgrade confirmation dialog — it no longer blocks the update.
- `compatible` now reflects hard blockers only: protocol version, DFU version,
  update-mode advertisement, and a required bootloader transition.
- Model identity continues to be enforced by the protocol-reported model string, the
  exact model string embedded in the signed image, and the profile RAM/vector layout
  check, which is still mandatory for `54LMG6HE`.
- A recorded-but-not-yet-acceptance-verified product ID (currently `0xd09d`) is
  reported distinctly from an acceptance-verified one (`0xd086`). The project keeps
  the distinction rather than claiming `hardware_verified` for a unit that has not
  completed an end-to-end upgrade run.
- Upgrade results now include `compatibility_warnings`, `previous_product_id`, and
  `product_id_changed`; warnings are also written to stderr before a write begins.
- `devices`, `probe` and `upgrade` accept `--strict-product-id` to restore the
  previous blocking behaviour for users who want product ID to remain a hard gate.

## Verified

- `python3 -m unittest discover -s tests -v` — 18 tests pass (7 new).
- Live `0xd09d` unit: `devices` reports `compatible: true` with
  `protocol_compatible_unverified_product_id`, and `upgrade --dry-run` returns
  `status: ready`, `version_relation: upgrade`,
  `firmware_trust_status: known_release_hash`, `write_attempted: false`.

## Not verified

- No write was attempted on the `0xd09d` unit, so that exact device/firmware pair has
  not completed an end-to-end upgrade acceptance run. Only `0xd086` remains
  `hardware_verified`.
- Ad-hoc signed; still not Apple-notarized.
