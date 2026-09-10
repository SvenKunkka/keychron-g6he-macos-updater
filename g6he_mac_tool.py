#!/usr/bin/env python3
"""macOS firmware inspection and HID upgrade tooling for Keychron mice."""

from __future__ import annotations

import argparse
import contextlib
import hashlib
import json
import math
import os
import re
import struct
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


MCUBOOT_MAGIC = 0x96F3B83D
MCUBOOT_TLV_MAGIC = 0x6907
KEYCHRON_VID = 0x3434
UPGRADE_USAGE_PAGE = 0x008C
UPGRADE_USAGE = 0x0001
OUTPUT_REPORT_ID = 0xB2
INPUT_REPORT_ID = 0xB1
REPORT_SIZE = 33
CHUNK_SIZE = 16
TLV_NAMES = {
    0x01: "key_hash",
    0x10: "sha256",
    0x11: "sha384",
    0x12: "sha512",
    0x20: "rsa2048_pss_signature",
    0x21: "ecdsa224_signature",
    0x22: "ecdsa256_signature",
    0x23: "rsa3072_pss_signature",
    0x24: "ed25519_signature",
    0x25: "signature_pure_flag",
}
DIGEST_TYPES = {0x10: hashlib.sha256, 0x11: hashlib.sha384, 0x12: hashlib.sha512}
SIGNATURE_TYPES = {0x20, 0x21, 0x22, 0x23, 0x24}
VERSION_PATTERN = re.compile(r"^v?(\d+)\.(\d+)\.(\d+)\+(\d+)$")


@dataclass(frozen=True)
class DeviceProfile:
    model: str
    display_name: str
    product_ids: tuple[int, ...]
    ram_start: int | None = None
    ram_end: int | None = None
    validation_status: str = "protocol_compatible"


KNOWN_DEVICE_PROFILES = {
    "54LMG6HE": DeviceProfile(
        model="54LMG6HE",
        display_name="Keychron G6 HE 8K",
        product_ids=(0xD086,),
        ram_start=0x20000000,
        ram_end=0x20080000,
        validation_status="hardware_verified",
    ),
}
TRUSTED_RELEASE_HASHES = {
    "54LMG6HE": {
        "38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a"
    }
}


class FirmwareError(ValueError):
    """Raised when a firmware container is malformed or inconsistent."""


class ProtocolError(RuntimeError):
    """Raised when the HID updater protocol returns an invalid response."""


@dataclass(frozen=True)
class ImageHeader:
    magic: str
    load_address: str
    header_size: int
    protected_tlv_size: int
    image_size: int
    flags: str
    version: str
    build_number: int


def _printable_strings(data: bytes, minimum: int = 5) -> list[str]:
    strings: list[str] = []
    start: int | None = None
    for index, value in enumerate(data + b"\x00"):
        if 0x20 <= value <= 0x7E:
            if start is None:
                start = index
        elif start is not None:
            if index - start >= minimum:
                strings.append(data[start:index].decode("ascii"))
            start = None
    return strings


def inspect_firmware(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    if len(data) < 32:
        raise FirmwareError("file is too small to contain an MCUboot header")

    magic, load_address, header_size, protected_tlv_size, image_size, flags = (
        struct.unpack_from("<IIHHII", data, 0)
    )
    major, minor, revision, build_number = struct.unpack_from("<BBHI", data, 20)
    if magic != MCUBOOT_MAGIC:
        raise FirmwareError(
            f"unexpected image magic 0x{magic:08x}; expected MCUboot 0x{MCUBOOT_MAGIC:08x}"
        )
    if header_size < 32 or header_size > min(len(data), 0x10000):
        raise FirmwareError(f"invalid MCUboot header size: {header_size}")
    if header_size % 4:
        raise FirmwareError("MCUboot header size is not 4-byte aligned")
    if image_size < 8:
        raise FirmwareError("firmware payload is too small to contain a vector table")
    if load_address in (0, 0xFFFFFFFF):
        raise FirmwareError("invalid MCUboot load address")
    if protected_tlv_size:
        raise FirmwareError(
            "protected MCUboot TLVs are not supported by this updater yet"
        )

    tlv_offset = header_size + image_size
    if tlv_offset + 4 > len(data):
        raise FirmwareError("declared image size extends beyond the file")
    tlv_magic, tlv_total = struct.unpack_from("<HH", data, tlv_offset)
    if tlv_magic != MCUBOOT_TLV_MAGIC:
        raise FirmwareError(
            f"unexpected TLV magic 0x{tlv_magic:04x} at 0x{tlv_offset:x}"
        )
    if tlv_total < 4:
        raise FirmwareError("invalid MCUboot TLV total size")
    if tlv_offset + tlv_total != len(data):
        raise FirmwareError(
            "TLV total does not end at EOF: "
            f"0x{tlv_offset + tlv_total:x} != 0x{len(data):x}"
        )

    tlvs: list[dict[str, Any]] = []
    cursor = tlv_offset + 4
    expected_end = tlv_offset + tlv_total
    verified_digest: str | None = None
    digest_count = 0
    key_hash_count = 0
    signature_count = 0
    while cursor < expected_end:
        if cursor + 4 > expected_end:
            raise FirmwareError("truncated TLV header")
        tlv_type, pad, length = struct.unpack_from("<BBH", data, cursor)
        if pad != 0:
            raise FirmwareError(f"non-zero TLV reserved byte at 0x{cursor:x}")
        value_start = cursor + 4
        value_end = value_start + length
        if value_end > expected_end:
            raise FirmwareError("TLV value extends beyond declared TLV area")
        value = data[value_start:value_end]
        item: dict[str, Any] = {
            "offset": f"0x{cursor:x}",
            "type": f"0x{tlv_type:02x}",
            "name": TLV_NAMES.get(tlv_type, "unknown"),
            "pad": pad,
            "length": length,
            "value_sha256": hashlib.sha256(value).hexdigest(),
        }
        if tlv_type in DIGEST_TYPES:
            digest_count += 1
            digest = DIGEST_TYPES[tlv_type]
            expected_length = digest().digest_size
            if length != expected_length:
                raise FirmwareError(
                    f"invalid {TLV_NAMES[tlv_type]} TLV length {length}"
                )
            matches = digest(data[:tlv_offset]).digest() == value
            item["matches_header_and_payload"] = matches
            if matches:
                verified_digest = TLV_NAMES[tlv_type]
        elif tlv_type == 0x01:
            key_hash_count += 1
            if length not in (32, 48, 64):
                raise FirmwareError(f"invalid key-hash TLV length {length}")
        elif tlv_type in SIGNATURE_TYPES:
            signature_count += 1
            if length == 0:
                raise FirmwareError("empty signature TLV")
        elif tlv_type == 0x25 and length == 1:
            item["value"] = value[0]
        tlvs.append(item)
        cursor = value_end

    if digest_count != 1:
        raise FirmwareError(f"expected exactly one image digest TLV, found {digest_count}")
    if verified_digest is None:
        raise FirmwareError("embedded image digest does not match the header and payload")
    if key_hash_count != 1:
        raise FirmwareError(f"expected exactly one key-hash TLV, found {key_hash_count}")
    if signature_count != 1:
        raise FirmwareError(f"expected exactly one signature TLV, found {signature_count}")

    initial_sp = reset_vector = None
    if header_size + 8 <= tlv_offset:
        initial_sp, reset_vector = struct.unpack_from("<II", data, header_size)
    if initial_sp in (0, 0xFFFFFFFF):
        raise FirmwareError("invalid initial stack pointer in payload vector table")
    if reset_vector in (0, 0xFFFFFFFF) or reset_vector & 1 == 0:
        raise FirmwareError("invalid Thumb reset vector in payload vector table")

    strings = _printable_strings(data[header_size:tlv_offset])
    model_candidates = sorted(
        {
            value
            for value in strings
            if value in KNOWN_DEVICE_PROFILES
            or re.fullmatch(r"\d{2}[A-Z0-9]{6,10}", value)
        }
    )
    identifiers = sorted(
        {
            value
            for value in strings
            if value in model_candidates
            or "Keychron" in value
            or VERSION_PATTERN.fullmatch(value)
        }
    )

    header = ImageHeader(
        magic=f"0x{magic:08x}",
        load_address=f"0x{load_address:08x}",
        header_size=header_size,
        protected_tlv_size=protected_tlv_size,
        image_size=image_size,
        flags=f"0x{flags:08x}",
        version=f"{major}.{minor}.{revision}+{build_number}",
        build_number=build_number,
    )
    return {
        "file": str(path.resolve()),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "container": "MCUboot signed image",
        "header": asdict(header),
        "payload_vector": {
            "initial_sp": f"0x{initial_sp:08x}" if initial_sp is not None else None,
            "reset_vector": f"0x{reset_vector:08x}" if reset_vector is not None else None,
        },
        "tlv_offset": f"0x{tlv_offset:x}",
        "tlv_total_size": tlv_total,
        "embedded_digest_verified": True,
        "verified_digest": verified_digest,
        "embedded_sha512_verified": verified_digest == "sha512",
        "tlvs": tlvs,
        "embedded_identifiers": identifiers,
        "firmware_model_candidates": model_candidates,
        "authentication": {
            "key_hash_tlv_count": key_hash_count,
            "signature_tlv_count": signature_count,
            "host_signature_verified": False,
        },
        "signature_note": (
            "The image contains a signature and key hash, but a matching public key "
            "is not configured on the host. The target bootloader must authenticate "
            "the signature."
        ),
    }


def _path_bytes(path: Any) -> bytes:
    if isinstance(path, bytes):
        return path
    return str(path).encode("utf-8", errors="surrogateescape")


def _device_id(item: dict[str, Any]) -> str:
    return hashlib.sha256(_path_bytes(item.get("path", b""))).hexdigest()[:16]


def _serialize_interface(item: dict[str, Any], *, include_path: bool = False) -> dict[str, Any]:
    path = item.get("path")
    if isinstance(path, bytes):
        path = path.decode("utf-8", errors="backslashreplace")
    result = {
        "device_id": _device_id(item),
        "vendor_id": f"0x{int(item.get('vendor_id', 0)):04x}",
        "product_id": f"0x{int(item.get('product_id', 0)):04x}",
        "manufacturer": item.get("manufacturer_string") or "",
        "product": item.get("product_string") or "",
        "serial_number": item.get("serial_number") or "",
        "usage_page": f"0x{int(item.get('usage_page', 0)):04x}",
        "usage": f"0x{int(item.get('usage', 0)):04x}",
        "interface_number": item.get("interface_number"),
        "release_number": item.get("release_number"),
    }
    if include_path:
        result["path"] = path
    return result


def list_hid_devices() -> list[dict[str, Any]]:
    try:
        import hid  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "Python HID support is missing. Install the 'hidapi' package first."
        ) from exc

    devices: list[dict[str, Any]] = []
    for item in hid.enumerate(KEYCHRON_VID, 0):
        if int(item.get("vendor_id", 0)) != KEYCHRON_VID:
            continue
        devices.append(_serialize_interface(item, include_path=True))
    return devices


def _import_hid() -> Any:
    try:
        import hid  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "Python HID support is missing. Install the 'hidapi' package first."
        ) from exc
    return hid


def _upgrade_interfaces() -> list[dict[str, Any]]:
    hid = _import_hid()
    return [
        item
        for item in hid.enumerate(KEYCHRON_VID, 0)
        if int(item.get("vendor_id", 0)) == KEYCHRON_VID
        if int(item.get("usage_page", 0)) == UPGRADE_USAGE_PAGE
        and int(item.get("usage", 0)) == UPGRADE_USAGE
    ]


def _find_upgrade_interface(device_id: str | None = None) -> dict[str, Any]:
    matches = _upgrade_interfaces()
    if device_id is not None:
        matches = [item for item in matches if _device_id(item) == device_id]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        suffix = f" for selection {device_id}" if device_id else ""
        raise ProtocolError(
            "Keychron mouse USB upgrade interface not found"
            f"{suffix}; switch the mouse to wired mode and connect it directly by USB"
        )
    raise ProtocolError(
        f"multiple Keychron mouse upgrade interfaces found ({len(matches)}); "
        "select one with --device"
    )


def build_frame(sequence: int, payload: bytes, update_frame: bool = False) -> bytes:
    if not 1 <= sequence <= 0xFF:
        raise ValueError("sequence must be between 1 and 255")
    if len(payload) > REPORT_SIZE - 8:
        raise ValueError("payload is too large for a 33-byte HID report")
    frame = bytearray(REPORT_SIZE)
    frame[0] = OUTPUT_REPORT_ID
    frame[1] = 0xAA
    frame[2] = 0x56 if update_frame else 0x55
    frame[3] = len(payload) + 2
    frame[4] = (~frame[3]) & 0xFF
    frame[5] = sequence
    frame[6 : 6 + len(payload)] = payload
    checksum = sum(payload) & 0xFFFF
    struct.pack_into("<H", frame, 6 + len(payload), checksum)
    return bytes(frame)


def _read_response(device: Any, timeout_ms: int = 1200) -> bytes:
    first = bytes(device.read(REPORT_SIZE, timeout=timeout_ms))
    if not first:
        raise ProtocolError("device did not return a HID response")
    if len(first) != REPORT_SIZE:
        raise ProtocolError(f"short HID response: {len(first)} bytes")
    if first[0] != INPUT_REPORT_ID or first[1] != 0xAA:
        raise ProtocolError(f"unexpected HID response prefix: {first[:2].hex()}")
    if ((first[3] + first[4]) & 0xFF) != 0xFF:
        raise ProtocolError("invalid HID response length complement")

    payload_length = first[3] - 2
    required = payload_length + 2
    collected = bytearray(first[6:])
    while len(collected) < required:
        continuation = bytes(device.read(REPORT_SIZE, timeout=timeout_ms))
        if not continuation:
            raise ProtocolError("timed out waiting for a continuation report")
        if len(continuation) != REPORT_SIZE or continuation[0] != INPUT_REPORT_ID:
            raise ProtocolError("invalid HID continuation report")
        collected.extend(continuation[1:])

    payload = bytes(collected[:payload_length])
    received_checksum = struct.unpack_from("<H", collected, payload_length)[0]
    expected_checksum = sum(payload) & 0xFFFF
    if received_checksum != expected_checksum:
        raise ProtocolError(
            "HID response checksum mismatch: "
            f"0x{received_checksum:04x} != 0x{expected_checksum:04x}"
        )
    return payload


def exchange(
    device: Any,
    sequence: int,
    payload: bytes,
    *,
    update_frame: bool = False,
    expect_response: bool = True,
) -> bytes:
    frame = build_frame(sequence, payload, update_frame)
    # The Windows implementation calls HidD_FlushQueue before every command.
    # hidapi does not expose that function directly, so drain queued reports in
    # non-blocking mode to avoid mistaking an asynchronous mouse event for the
    # command response.
    device.nonblocking = True
    try:
        while device.read(REPORT_SIZE):
            pass
    finally:
        device.nonblocking = False
    written = device.write(frame)
    if written != REPORT_SIZE:
        raise ProtocolError(f"short HID write: {written} of {REPORT_SIZE} bytes")
    if not expect_response:
        return b""
    response = _read_response(device)
    if len(response) < 4:
        raise ProtocolError("updater response is too short")
    expected_response_type = 0xA1 if update_frame else 0xA3
    if response[0] != expected_response_type:
        raise ProtocolError(
            f"unexpected response type 0x{response[0]:02x}; expected "
            f"0x{expected_response_type:02x}: {response.hex()}"
        )
    if response[1] != sequence:
        raise ProtocolError(
            f"sequence mismatch: device {response[1]}, request {sequence}"
        )
    if response[2] != payload[0]:
        raise ProtocolError(
            f"command mismatch: device 0x{response[2]:02x}, request 0x{payload[0]:02x}"
        )
    if response[3] != 0:
        raise ProtocolError(
            f"device rejected command 0x{payload[0]:02x} with status {response[3]}"
        )
    return response


def _decode_c_string(value: bytes) -> str:
    return value.split(b"\x00", 1)[0].decode("ascii", errors="replace")


def _query_open_device(device: Any, sequence: int = 1) -> tuple[dict[str, Any], int]:
    version_response = exchange(device, sequence, b"\x60")
    sequence = _next_sequence(sequence)
    capability_response = exchange(device, sequence, b"\x61")
    sequence = _next_sequence(sequence)

    version_data = version_response[4:]
    if len(version_data) < 34:
        raise ProtocolError(
            f"version response is shorter than expected ({len(version_data)} bytes)"
        )
    capabilities = capability_response[4:]
    if len(capabilities) < 3:
        raise ProtocolError("capabilities response is shorter than expected")
    bootloader_required = len(capability_response) + 2 >= 10 and (
        len(capabilities) >= 4 and capabilities[3] != 0
    )
    return (
        {
            "model": _decode_c_string(version_data[0:10]),
            "hardware_revision": version_data[10:12].hex(),
            "firmware_version": _decode_c_string(version_data[12:22]),
            "bootloader_model": _decode_c_string(version_data[22:32]),
            "bootloader_version": f"{version_data[32]}.{version_data[33]}",
            "protocol_version": capabilities[0],
            "dfu_version": capabilities[1],
            "supported_update_modes": capabilities[2],
            "bootloader_required": bootloader_required,
        },
        sequence,
    )


def _device_compatibility(
    interface: dict[str, Any], device_info: dict[str, Any]
) -> dict[str, Any]:
    reasons: list[str] = []
    if device_info["protocol_version"] != 1:
        reasons.append(f"unsupported protocol {device_info['protocol_version']}")
    if device_info["dfu_version"] != 0:
        reasons.append(f"unsupported DFU {device_info['dfu_version']}")
    if not device_info["supported_update_modes"] & 0x01:
        reasons.append("standard firmware update mode is not advertised")
    if device_info["bootloader_required"]:
        reasons.append("an unverified bootloader transition is required")

    profile = KNOWN_DEVICE_PROFILES.get(device_info["model"])
    product_id = int(interface.get("product_id", 0))
    if profile and product_id not in profile.product_ids:
        reasons.append(
            f"known model {profile.model} has unexpected USB product ID 0x{product_id:04x}"
        )
    return {
        "compatible": not reasons,
        "compatibility_status": (
            profile.validation_status if profile and not reasons else
            "protocol_compatible_unverified_model" if not reasons else
            "incompatible"
        ),
        "display_name": profile.display_name if profile else (
            interface.get("product_string") or device_info["model"] or "Keychron Mouse"
        ),
        "compatibility_reasons": reasons,
    }


def query_device(
    device_id: str | None = None, *, interface: dict[str, Any] | None = None
) -> dict[str, Any]:
    hid = _import_hid()
    interface = interface or _find_upgrade_interface(device_id)
    with hid.Device(path=interface["path"]) as device:
        device_info, _ = _query_open_device(device)
    return {
        **_serialize_interface(interface),
        **device_info,
        **_device_compatibility(interface, device_info),
        "write_attempted": False,
    }


def discover_devices() -> list[dict[str, Any]]:
    devices: list[dict[str, Any]] = []
    for interface in _upgrade_interfaces():
        try:
            devices.append(query_device(interface=interface))
        except Exception as exc:
            devices.append(
                {
                    **_serialize_interface(interface),
                    "display_name": interface.get("product_string") or "Keychron Mouse",
                    "compatible": False,
                    "compatibility_status": "probe_failed",
                    "compatibility_reasons": [str(exc)],
                    "probe_error": str(exc),
                    "write_attempted": False,
                }
            )
    return devices


def _next_sequence(sequence: int) -> int:
    sequence = (sequence + 1) & 0xFF
    return sequence or 1


def updater_crc32(data: bytes, initial: int = 0xFFFFFFFF) -> int:
    """CRC-32 used by the Windows updater (reflected, no final XOR)."""
    crc = initial
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc & 0xFFFFFFFF


def _version_key(value: str) -> tuple[int, int, int, int] | None:
    match = VERSION_PATTERN.fullmatch(value)
    if not match:
        return None
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def _version_relation(current: str, target: str) -> str:
    current_key = _version_key(current)
    target_key = _version_key(target)
    if current_key is None or target_key is None:
        return "unknown"
    if target_key == current_key:
        return "same"
    return "upgrade" if target_key > current_key else "downgrade"


@contextlib.contextmanager
def _prevent_system_sleep() -> Any:
    assertion: subprocess.Popen[bytes] | None = None
    if sys.platform == "darwin" and Path("/usr/bin/caffeinate").exists():
        assertion = subprocess.Popen(
            ["/usr/bin/caffeinate", "-dimsu", "-w", str(os.getpid())],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    try:
        yield
    finally:
        if assertion is not None and assertion.poll() is None:
            assertion.terminate()
            try:
                assertion.wait(timeout=2)
            except subprocess.TimeoutExpired:
                assertion.kill()
                assertion.wait(timeout=2)


def _validate_upgrade_target(
    firmware_report: dict[str, Any], device_info: dict[str, Any], firmware_data: bytes
) -> None:
    target_model = device_info["model"]
    header_size = int(firmware_report["header"]["header_size"])
    tlv_offset = int(firmware_report["tlv_offset"], 16)
    embedded_strings = set(
        _printable_strings(firmware_data[header_size:tlv_offset], minimum=4)
    )
    if target_model not in embedded_strings:
        raise FirmwareError(
            f"firmware does not contain the selected device model {target_model!r}"
        )
    if not device_info["compatible"]:
        raise ProtocolError("; ".join(device_info["compatibility_reasons"]))

    profile = KNOWN_DEVICE_PROFILES.get(target_model)
    if profile and profile.ram_start is not None and profile.ram_end is not None:
        load_address = int(firmware_report["header"]["load_address"], 16)
        header_size = int(firmware_report["header"]["header_size"])
        image_size = int(firmware_report["header"]["image_size"])
        initial_sp = int(firmware_report["payload_vector"]["initial_sp"], 16)
        reset_vector = int(firmware_report["payload_vector"]["reset_vector"], 16) & ~1
        image_end = load_address + header_size + image_size
        if not (
            profile.ram_start <= load_address < image_end <= profile.ram_end
            and profile.ram_start <= initial_sp <= profile.ram_end
            and profile.ram_start <= reset_vector < image_end
        ):
            raise FirmwareError(
                f"firmware memory layout is outside the verified range for {target_model}"
            )


def _query_after_restart(
    expected_model: str, preferred_device_id: str, preferred_product_id: str
) -> dict[str, Any]:
    try:
        candidate = query_device(preferred_device_id)
        if candidate["model"] == expected_model:
            return candidate
    except Exception:
        pass
    matches = [
        candidate
        for candidate in discover_devices()
        if candidate.get("model") == expected_model
        and candidate.get("product_id") == preferred_product_id
    ]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ProtocolError(f"restarted device model {expected_model} was not found")
    raise ProtocolError(
        f"multiple restarted devices match model {expected_model}; verification is ambiguous"
    )


def upgrade_firmware(
    path: Path,
    *,
    device_id: str | None,
    dry_run: bool,
    confirmation: str | None,
    allow_downgrade: bool = False,
) -> dict[str, Any]:
    firmware_report = inspect_firmware(path)
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != firmware_report["sha256"]:
        raise FirmwareError("firmware file changed while it was being validated")
    file_crc = updater_crc32(data)
    hid = _import_hid()
    interface = _find_upgrade_interface(device_id)
    selected_device_id = _device_id(interface)

    with hid.Device(path=interface["path"]) as device:
        raw_device_info, sequence = _query_open_device(device)
        device_info = {
            **_serialize_interface(interface),
            **raw_device_info,
            **_device_compatibility(interface, raw_device_info),
        }
        _validate_upgrade_target(firmware_report, device_info, data)
        target_version = firmware_report["header"]["version"]
        version_relation = _version_relation(
            device_info["firmware_version"], target_version
        )
        trusted_hashes = TRUSTED_RELEASE_HASHES.get(device_info["model"], set())
        trust_status = (
            "known_release_hash"
            if firmware_report["sha256"] in trusted_hashes
            else "bootloader_signature_only"
        )
        common_result = {
            "device": device_info,
            "target_version": target_version,
            "version_relation": version_relation,
            "firmware_sha256": firmware_report["sha256"],
            "firmware_crc32": f"0x{file_crc:08x}",
            "firmware_trust_status": trust_status,
            "host_signature_verified": False,
        }
        if version_relation == "same":
            return {
                "status": "already_current",
                **common_result,
                "write_attempted": False,
                "verified_after_restart": True,
            }
        if dry_run:
            return {
                "status": "ready",
                **common_result,
                "firmware_size": len(data),
                "chunks": math.ceil(len(data) / CHUNK_SIZE),
                "requires_downgrade_confirmation": version_relation == "downgrade",
                "write_attempted": False,
            }
        if version_relation == "downgrade" and not allow_downgrade:
            raise ProtocolError(
                f"refusing firmware downgrade {device_info['firmware_version']} -> "
                f"{target_version}; pass --allow-downgrade only when intentional"
            )
        if confirmation != device_info["model"]:
            raise ProtocolError(
                f"actual upgrade requires --confirm {device_info['model']}"
            )

        print(
            f"Starting verified upgrade {device_info['firmware_version']} -> "
            f"{target_version}; keep the USB cable connected.",
            file=sys.stderr,
            flush=True,
        )
        hid_exception = getattr(hid, "HIDException", None)
        retryable_errors: tuple[type[BaseException], ...] = (OSError, ProtocolError)
        if isinstance(hid_exception, type) and issubclass(hid_exception, BaseException):
            retryable_errors += (hid_exception,)

        running_crc = 0xFFFFFFFF
        total_chunks = math.ceil(len(data) / CHUNK_SIZE)
        update_mode_entered = False
        try:
            with _prevent_system_sleep():
                exchange(device, sequence, b"\x62\x00")
                update_mode_entered = True
                sequence = _next_sequence(sequence)
                exchange(device, sequence, b"\x63", update_frame=True)
                sequence = _next_sequence(sequence)

                last_percent = -1
                for chunk_index, offset in enumerate(range(0, len(data), CHUNK_SIZE), 1):
                    chunk = data[offset : offset + CHUNK_SIZE]
                    last_error: Exception | None = None
                    for attempt in range(5):
                        try:
                            exchange(
                                device,
                                sequence,
                                b"\x64" + chunk,
                                update_frame=True,
                            )
                            last_error = None
                            break
                        except retryable_errors as exc:
                            last_error = exc
                            if attempt < 4:
                                time.sleep(0.02 * (attempt + 1))
                    if last_error is not None:
                        raise ProtocolError(
                            f"data transfer failed at offset 0x{offset:x} after 5 attempts: "
                            f"{last_error}"
                        )
                    running_crc = updater_crc32(chunk, running_crc)
                    sequence = _next_sequence(sequence)
                    percent = chunk_index * 100 // total_chunks
                    if percent >= last_percent + 5 or chunk_index == total_chunks:
                        print(
                            f"Progress: {percent}% ({chunk_index}/{total_chunks})",
                            file=sys.stderr,
                            flush=True,
                        )
                        last_percent = percent

                if running_crc != file_crc:
                    raise ProtocolError(
                        f"local streaming CRC mismatch: 0x{running_crc:08x} != "
                        f"0x{file_crc:08x}"
                    )
                exchange(
                    device,
                    sequence,
                    b"\x65" + struct.pack("<II", file_crc, running_crc),
                )
                sequence = _next_sequence(sequence)
                exchange(device, sequence, b"\x66", expect_response=False)
                update_mode_entered = False
        except Exception as exc:
            recovery = "update mode was not entered"
            if update_mode_entered:
                try:
                    exchange(device, sequence, b"\x66", expect_response=False)
                    recovery = "a best-effort reset command was sent"
                except Exception as recovery_exc:
                    recovery = f"reset attempt failed: {recovery_exc}"
            raise ProtocolError(f"upgrade interrupted; {recovery}: {exc}") from exc

    verified: dict[str, Any] | None = None
    last_verification_error: Exception | None = None
    for _ in range(30):
        time.sleep(0.5)
        try:
            candidate = _query_after_restart(
                device_info["model"],
                selected_device_id,
                device_info["product_id"],
            )
            if candidate["firmware_version"] == target_version:
                verified = candidate
                break
            last_verification_error = ProtocolError(
                f"device returned version {candidate['firmware_version']}"
            )
        except (OSError, RuntimeError, ValueError) as exc:
            last_verification_error = exc
    if verified is None:
        raise ProtocolError(
            "firmware transfer completed but post-restart version verification failed: "
            f"{last_verification_error}"
        )
    return {
        "status": "upgraded_and_verified",
        **common_result,
        "previous_version": device_info["firmware_version"],
        "device": verified,
        "bytes_written": len(data),
        "chunks_written": total_chunks,
        "write_attempted": True,
        "verified_after_restart": True,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Safe Keychron mouse firmware inspection and HID upgrade for macOS"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)
    inspect_parser = subparsers.add_parser(
        "inspect", help="validate a firmware package without touching a device"
    )
    inspect_parser.add_argument("firmware", type=Path)
    subparsers.add_parser("list", help="list all Keychron HID interfaces without writing")
    subparsers.add_parser(
        "devices", help="discover and probe Keychron mouse upgrade interfaces"
    )
    probe_parser = subparsers.add_parser(
        "probe", help="query one Keychron mouse model and firmware version"
    )
    probe_parser.add_argument("--device", help="device_id returned by the devices command")
    upgrade_parser = subparsers.add_parser(
        "upgrade", help="validate or install a signed Keychron mouse firmware image"
    )
    upgrade_parser.add_argument("firmware", type=Path)
    upgrade_parser.add_argument(
        "--device", help="device_id returned by the devices command"
    )
    upgrade_parser.add_argument(
        "--dry-run", action="store_true", help="validate without sending update commands"
    )
    upgrade_parser.add_argument(
        "--confirm", help="required for writes; must exactly match the device model"
    )
    upgrade_parser.add_argument(
        "--allow-downgrade",
        action="store_true",
        help="allow an intentional target version lower than the running version",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "inspect":
            result: Any = inspect_firmware(args.firmware)
        elif args.command == "list":
            result = {"matching_devices": list_hid_devices(), "write_attempted": False}
        elif args.command == "devices":
            result = {"devices": discover_devices(), "write_attempted": False}
        elif args.command == "probe":
            result = query_device(args.device)
        elif args.command == "upgrade":
            result = upgrade_firmware(
                args.firmware,
                device_id=args.device,
                dry_run=args.dry_run,
                confirmation=args.confirm,
                allow_downgrade=args.allow_downgrade,
            )
        else:  # pragma: no cover - argparse enforces the choices.
            raise AssertionError(args.command)
    except Exception as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "result": result}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
