# Security and device-safety policy

The application accepts only a structurally valid MCUboot image whose embedded
SHA-512 matches its header and payload, and only writes to the verified
Keychron G6 HE USB identity `3434:d086` with device model `54LMG6HE`.

The host package does not contain the vendor's Ed25519 public key. Signature
authentication therefore remains the responsibility of the device bootloader.
Do not disconnect USB or allow the Mac to sleep during an update.

Please report security or device-safety issues through GitHub Issues without
posting private keys, credentials, personal data, or proprietary firmware.
