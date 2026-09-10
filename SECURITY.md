# Security and device-safety policy

The updater enumerates Keychron USB devices only on the observed firmware-upgrade HID
usage page and usage. It then requires a successful protocol/capability query and an
exact match between the selected device model and an identifier embedded in the chosen
firmware.

Before writing, the host validates MCUboot boundaries, one supported image digest, one
key-hash TLV, one signature TLV, vector sanity, known-device memory ranges, version
direction and transfer CRC. It prevents macOS sleep during the critical section,
retries hidapi transport failures and attempts a reset if transfer is interrupted.

The host does not currently contain Keychron firmware-signing public keys. It therefore
cannot independently prove signature authenticity for arbitrary packages; the device
bootloader remains the final signature authority. Unknown models that return the known
protocol are labeled protocol-compatible but not hardware-verified.

Do not disconnect USB during an update. Perform intentional interruption/recovery tests
only on a recoverable engineering unit.

Please report security or device-safety issues through GitHub Issues without posting
private keys, credentials, personal data or proprietary firmware.
