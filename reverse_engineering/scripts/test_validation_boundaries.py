#!/usr/bin/env python3
"""Non-device tests for firmware validation boundaries in the macOS updater."""

from __future__ import annotations

import hashlib
import json
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from g6he_mac_tool import FirmwareError, inspect_firmware  # noqa: E402


def inspect_bytes(data: bytes) -> tuple[bool, str]:
    with tempfile.NamedTemporaryFile(suffix=".signed.bin") as handle:
        handle.write(data)
        handle.flush()
        try:
            inspect_firmware(Path(handle.name))
            return True, "accepted"
        except FirmwareError as exc:
            return False, str(exc)


def offsets(data: bytes) -> tuple[int, int, int]:
    header_size = struct.unpack_from("<H", data, 8)[0]
    image_size = struct.unpack_from("<I", data, 12)[0]
    return header_size, image_size, header_size + image_size


def rewrite_digest(data: bytearray) -> None:
    _, _, tlv_start = offsets(data)
    cursor = tlv_start + 4
    while cursor < len(data):
        tlv_type, _pad, length = struct.unpack_from("<BBH", data, cursor)
        if tlv_type == 0x12 and length == 64:
            data[cursor + 4 : cursor + 68] = hashlib.sha512(data[:tlv_start]).digest()
            return
        cursor += 4 + length
    raise RuntimeError("SHA-512 TLV not found")


def remove_authentication_tlvs(data: bytes) -> bytes:
    mutable = bytearray(data)
    _, _, tlv_start = offsets(mutable)
    cursor = tlv_start + 4
    kept = bytearray()
    while cursor < len(mutable):
        tlv_type, _pad, length = struct.unpack_from("<BBH", mutable, cursor)
        entry = mutable[cursor : cursor + 4 + length]
        if tlv_type == 0x12:
            kept.extend(entry)
        cursor += 4 + length
    struct.pack_into("<H", mutable, tlv_start + 2, 4 + len(kept))
    return bytes(mutable[: tlv_start + 4] + kept)


def main() -> int:
    parser_path = ROOT / "firmware" / "G6HE_v1.0.0+84_202609101503.signed.bin"
    original = parser_path.read_bytes()
    _, _, tlv_start = offsets(original)
    tests: list[dict[str, object]] = []

    signature_flip = bytearray(original)
    cursor = tlv_start + 4
    while cursor < len(signature_flip):
        tlv_type, _pad, length = struct.unpack_from("<BBH", signature_flip, cursor)
        if tlv_type == 0x24:
            signature_flip[cursor + 4] ^= 1
            break
        cursor += 4 + length
    accepted, detail = inspect_bytes(bytes(signature_flip))
    tests.append({"case": "corrupted_ed25519_signature", "accepted": accepted, "detail": detail})

    unsigned = remove_authentication_tlvs(original)
    accepted, detail = inspect_bytes(unsigned)
    tests.append({"case": "signature_and_key_hash_removed", "accepted": accepted, "detail": detail})

    bad_load = bytearray(original)
    struct.pack_into("<I", bad_load, 4, 0x10000000)
    rewrite_digest(bad_load)
    accepted, detail = inspect_bytes(bytes(bad_load))
    tests.append({"case": "load_address_outside_nrf54lm20a_ram", "accepted": accepted, "detail": detail})

    bad_vector = bytearray(original)
    header_size, _, _ = offsets(bad_vector)
    struct.pack_into("<II", bad_vector, header_size, 0, 0)
    rewrite_digest(bad_vector)
    accepted, detail = inspect_bytes(bytes(bad_vector))
    tests.append({"case": "zero_stack_and_reset_vectors", "accepted": accepted, "detail": detail})

    rollback = bytearray(original)
    struct.pack_into("<BBHI", rollback, 20, 0, 9, 0, 1)
    rewrite_digest(rollback)
    accepted, detail = inspect_bytes(bytes(rollback))
    tests.append({"case": "older_header_version_without_valid_signature", "accepted": accepted, "detail": detail})

    tampered_payload = bytearray(original)
    tampered_payload[header_size + 100] ^= 1
    accepted, detail = inspect_bytes(bytes(tampered_payload))
    tests.append({"case": "payload_changed_without_digest_update", "accepted": accepted, "detail": detail})

    report = {
        "schema_version": 1,
        "input_sha256": hashlib.sha256(original).hexdigest(),
        "method": "Targeted offline mutation; no HID device commands were sent.",
        "tests": tests,
        "interpretation": (
            "Acceptance means the host-side parser considers the container structurally valid. "
            "The device bootloader may still reject it during signature authentication."
        ),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
