# Keychron Mouse Firmware Updater for macOS v1.3.0

## Changed

- Version comparison no longer asserts intent. `_version_relation` returned
  `upgrade` / `downgrade`; that value was shown to users as a conclusion about what
  the write meant. It is replaced by `_version_order`, which reports only the
  numeric ordering as a fact: `newer`, `older`, `same`, or `unknown`. The updater
  does not decide whether a write is an upgrade, because what a build counter means
  is a vendor release convention.
- `version_relation` is renamed to `version_order` in the CLI JSON output, and
  `requires_downgrade_confirmation` to `requires_version_change_confirmation`.
- `--allow-downgrade` is renamed to `--allow-version-change`, and now applies to any
  target that does not sort above the running version: an older build, a same-version
  reflash, or a parallel release line.
- A same-version target is no longer skipped unconditionally. The write is treated as
  `already_current` only when the image hash recorded as running for that model and
  version matches the selected file; otherwise the result carries a
  `version_order_note` and enters the confirmation flow.
- The GUI reports the version relationship as a fact and asks for confirmation when
  the target does not sort higher, instead of labelling the operation.

## Why

Vendors ship parallel release lines with independent build counters. Observed side by
side on this device family: public-line builds `1.0.0+66`, `+67`, `+84`, `+87`, alongside
an internal-test (`_nc_`) build `1.0.0+1` whose package timestamp is *later* than the
`+84` package. A numerically lower target is therefore not necessarily an older image,
and a numerically higher one is not necessarily an upgrade.

A generic updater has no reliable way to know a vendor's release-line semantics, so the
previous `upgrade` / `downgrade` labels could state the opposite of the truth when
crossing lines. Reporting the ordering and requiring confirmation keeps the safety
property (nothing unexpected is written silently) without asserting something the tool
cannot know.

Filenames are also not a trustworthy version source: a package named
`..._v1.0.0+86_...` was observed to contain an image whose embedded version is `1.0.0+87`.
Use the `inspect` output, which reads the image header.

## Validation

- `python3 -m unittest discover -s tests -v` — 25 tests pass (7 new), covering
  version-order classification, the parallel-release-line cases, same-version
  reflash detection, and refusal of an older-sorting target without consent.
- Live device (`54LMG6HE`, PID `0xd09d`, running `1.0.0+1`): `devices` reports
  `compatible: true`; `upgrade --dry-run` against `1.0.0+84` reports
  `version_order: newer` and `write_attempted: false`.

## Not done

- No device write was performed as part of this change. The `0xd09d` unit was
  already updated to the internal `1.0.0+1` build by the user before this review.
- Ad-hoc signed; still not Apple-notarized.
