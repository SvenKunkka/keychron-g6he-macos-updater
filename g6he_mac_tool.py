#!/usr/bin/env python3
"""macOS firmware inspection and HID protocol tooling for Keychron G6 HE."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


MCUBOOT_MAGIC = 0x96F3B83D
MCUBOOT_TLV_MAGIC = 0x6907
KEYCHRON_VID = 0x3434
G6_HE_PID = 0xD086
UPGRADE_USAGE_PAGE = 0x008C
UPGRADE_USAGE = 0x0001
OUTPUT_REPORT_ID = 0xB2
INPUT_REPORT_ID = 0xB1
REPORT_SIZE = 33
CHUNK_SIZE = 16
TARGET_MODEL = "54LMG6HE"
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
    if header_size < 32 or header_size > len(data):
        raise FirmwareError(f"invalid MCUboot header size: {header_size}")

    tlv_offset = header_size + image_size
    if tlv_offset + 4 > len(data):
        raise FirmwareError("declared image size extends beyond the file")
    tlv_magic, tlv_total = struct.unpack_from("<HH", data, tlv_offset)
    if tlv_magic != MCUBOOT_TLV_MAGIC:
        raise FirmwareError(
            f"unexpected TLV magic 0x{tlv_magic:04x} at 0x{tlv_offset:x}"
        )
    if tlv_offset + tlv_total != len(data):
        raise FirmwareError(
            "TLV total does not end at EOF: "
            f"0x{tlv_offset + tlv_total:x} != 0x{len(data):x}"
        )

    tlvs: list[dict[str, Any]] = []
    cursor = tlv_offset + 4
    expected_end = tlv_offset + tlv_total
    sha512_verified = False
    while cursor < expected_end:
        if cursor + 4 > expected_end:
            raise FirmwareError("truncated TLV header")
        tlv_type, pad, length = struct.unpack_from("<BBH", data, cursor)
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
        if tlv_type == 0x12 and length == 64:
            sha512_verified = hashlib.sha512(data[:tlv_offset]).digest() == value
            item["matches_header_and_payload"] = sha512_verified
        elif tlv_type == 0x25 and length == 1:
            item["value"] = value[0]
        tlvs.append(item)
        cursor = value_end

    if not sha512_verified:
        raise FirmwareError("embedded SHA-512 does not match the header and payload")

    initial_sp = reset_vector = None
    if header_size + 8 <= tlv_offset:
        initial_sp, reset_vector = struct.unpack_from("<II", data, header_size)

    strings = _printable_strings(data[header_size:tlv_offset])
    identifiers = [
        value
        for value in strings
        if value in {
            "Keychron G6 HE 8K",
            "Keychron G6 HE",
            "G6 HE 8K",
            "nrf54lm20a",
            "KCFWID",
            "54LMG6HE",
            "54LMv1.0",
            "1.0.0+84",
            "Sep 10 2026",
            "15:10:32",
        }
    ]

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
        "embedded_sha512_verified": sha512_verified,
        "tlvs": tlvs,
        "embedded_identifiers": sorted(set(identifiers)),
        "signature_note": (
            "The image contains an Ed25519 signature and key hash, but the public "
            "key is not present in this package; only the embedded SHA-512 can be "
            "verified locally. The target bootloader must authenticate the signature."
        ),
    }


def list_hid_devices() -> list[dict[str, Any]]:
    try:
        import hid  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "Python HID support is missing. Install the 'hidapi' package first."
        ) from exc

    devices: list[dict[str, Any]] = []
    for item in hid.enumerate():
        manufacturer = item.get("manufacturer_string") or ""
        product = item.get("product_string") or ""
        haystack = f"{manufacturer} {product}".lower()
        if "keychron" not in haystack and "g6" not in haystack:
            continue
        path = item.get("path")
        if isinstance(path, bytes):
            path = path.decode("utf-8", errors="backslashreplace")
        devices.append(
            {
                "vendor_id": f"0x{int(item.get('vendor_id', 0)):04x}",
                "product_id": f"0x{int(item.get('product_id', 0)):04x}",
                "manufacturer": manufacturer,
                "product": product,
                "serial_number": item.get("serial_number") or "",
                "usage_page": f"0x{int(item.get('usage_page', 0)):04x}",
                "usage": f"0x{int(item.get('usage', 0)):04x}",
                "interface_number": item.get("interface_number"),
                "release_number": item.get("release_number"),
                "path": path,
            }
        )
    return devices


def _import_hid() -> Any:
    try:
        import hid  # type: ignore
    except ImportError as exc:
        raise RuntimeError(
            "Python HID support is missing. Install the 'hidapi' package first."
        ) from exc
    return hid


def _find_upgrade_interface() -> dict[str, Any]:
    hid = _import_hid()
    matches = [
        item
        for item in hid.enumerate(KEYCHRON_VID, G6_HE_PID)
        if int(item.get("usage_page", 0)) == UPGRADE_USAGE_PAGE
        and int(item.get("usage", 0)) == UPGRADE_USAGE
    ]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ProtocolError(
            "G6 HE USB upgrade interface not found; switch the mouse to wired mode "
            "and connect it directly by USB cable"
        )
    raise ProtocolError(
        f"refusing an ambiguous device selection ({len(matches)} upgrade interfaces)"
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


def query_device() -> dict[str, Any]:
    hid = _import_hid()
    interface = _find_upgrade_interface()
    with hid.Device(path=interface["path"]) as device:
        device_info, _ = _query_open_device(device)
    path = interface.get("path")
    if isinstance(path, bytes):
        path = path.decode("utf-8", errors="backslashreplace")
    return {
        "vendor_id": f"0x{int(interface.get('vendor_id', 0)):04x}",
        "product_id": f"0x{int(interface.get('product_id', 0)):04x}",
        "product": interface.get("product_string") or "",
        "path": path,
        **device_info,
        "write_attempted": False,
    }


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


def _validate_upgrade_target(
    firmware_report: dict[str, Any], device_info: dict[str, Any]
) -> None:
    if TARGET_MODEL not in firmware_report["embedded_identifiers"]:
        raise FirmwareError(
            f"firmware does not contain the required model identifier {TARGET_MODEL}"
        )
    if device_info["model"] != TARGET_MODEL:
        raise ProtocolError(
            f"connected model {device_info['model']!r} does not match {TARGET_MODEL}"
        )
    if device_info["protocol_version"] != 1:
        raise ProtocolError(
            f"unsupported updater protocol {device_info['protocol_version']}"
        )
    if device_info["dfu_version"] != 0:
        raise ProtocolError(f"unsupported DFU version {device_info['dfu_version']}")
    if not device_info["supported_update_modes"] & 0x01:
        raise ProtocolError("device does not advertise standard firmware update mode")
    if device_info["bootloader_required"]:
        raise ProtocolError(
            "this device requires a bootloader transition that is not enabled by this tool"
        )


def upgrade_firmware(
    path: Path, *, dry_run: bool, confirmation: str | None
) -> dict[str, Any]:
    firmware_report = inspect_firmware(path)
    data = path.read_bytes()
    file_crc = updater_crc32(data)
    hid = _import_hid()
    interface = _find_upgrade_interface()

    with hid.Device(path=interface["path"]) as device:
        device_info, sequence = _query_open_device(device)
        _validate_upgrade_target(firmware_report, device_info)
        target_version = firmware_report["header"]["version"]
        if device_info["firmware_version"] == target_version:
            return {
                "status": "already_current",
                "device": device_info,
                "target_version": target_version,
                "firmware_sha256": firmware_report["sha256"],
                "firmware_crc32": f"0x{file_crc:08x}",
                "write_attempted": False,
                "verified_after_restart": True,
            }
        if dry_run:
            return {
                "status": "ready",
                "device": device_info,
                "target_version": target_version,
                "firmware_size": len(data),
                "firmware_sha256": firmware_report["sha256"],
                "firmware_crc32": f"0x{file_crc:08x}",
                "chunks": math.ceil(len(data) / CHUNK_SIZE),
                "write_attempted": False,
            }
        if confirmation != TARGET_MODEL:
            raise ProtocolError(
                f"actual upgrade requires --confirm {TARGET_MODEL}"
            )

        print(
            f"Starting verified upgrade {device_info['firmware_version']} -> "
            f"{target_version}; keep the USB cable connected.",
            file=sys.stderr,
            flush=True,
        )
        exchange(device, sequence, b"\x62\x00")
        sequence = _next_sequence(sequence)
        exchange(device, sequence, b"\x63", update_frame=True)
        sequence = _next_sequence(sequence)

        running_crc = 0xFFFFFFFF
        total_chunks = math.ceil(len(data) / CHUNK_SIZE)
        last_percent = -1
        for chunk_index, offset in enumerate(range(0, len(data), CHUNK_SIZE), 1):
            chunk = data[offset : offset + CHUNK_SIZE]
            last_error: Exception | None = None
            for _attempt in range(5):
                try:
                    exchange(
                        device,
                        sequence,
                        b"\x64" + chunk,
                        update_frame=True,
                    )
                    last_error = None
                    break
                except (OSError, ProtocolError) as exc:
                    last_error = exc
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
                f"local streaming CRC mismatch: 0x{running_crc:08x} != 0x{file_crc:08x}"
            )
        exchange(
            device,
            sequence,
            b"\x65" + struct.pack("<II", file_crc, running_crc),
        )
        sequence = _next_sequence(sequence)
        exchange(device, sequence, b"\x66", expect_response=False)

    verified: dict[str, Any] | None = None
    last_verification_error: Exception | None = None
    for _ in range(30):
        time.sleep(0.5)
        try:
            candidate = query_device()
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
        "previous_version": device_info["firmware_version"],
        "target_version": target_version,
        "device": verified,
        "firmware_sha256": firmware_report["sha256"],
        "firmware_crc32": f"0x{file_crc:08x}",
        "bytes_written": len(data),
        "chunks_written": total_chunks,
        "write_attempted": True,
        "verified_after_restart": True,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Safe G6 HE firmware inspection and HID discovery for macOS"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)
    inspect_parser = subparsers.add_parser(
        "inspect", help="validate a firmware package without touching a device"
    )
    inspect_parser.add_argument("firmware", type=Path)
    subparsers.add_parser("list", help="list matching HID interfaces without writing")
    subparsers.add_parser(
        "probe", help="query the connected G6 HE model and firmware version"
    )
    upgrade_parser = subparsers.add_parser(
        "upgrade", help="validate or install a G6 HE signed firmware image"
    )
    upgrade_parser.add_argument("firmware", type=Path)
    upgrade_parser.add_argument(
        "--dry-run", action="store_true", help="validate without sending update commands"
    )
    upgrade_parser.add_argument(
        "--confirm", help=f"required for writes; must be exactly {TARGET_MODEL}"
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "inspect":
            result: Any = inspect_firmware(args.firmware)
        elif args.command == "list":
            result = {"matching_devices": list_hid_devices(), "write_attempted": False}
        elif args.command == "probe":
            result = query_device()
        elif args.command == "upgrade":
            result = upgrade_firmware(
                args.firmware, dry_run=args.dry_run, confirmation=args.confirm
            )
        else:  # pragma: no cover - argparse enforces the choices.
            raise AssertionError(args.command)
    except (FirmwareError, OSError, RuntimeError, ValueError) as exc:
        print(json.dumps({"ok": False, "error": str(exc)}, indent=2), file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "result": result}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
