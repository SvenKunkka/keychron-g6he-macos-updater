#!/usr/bin/env python3
"""Deterministic, read-only static checks for the G6 HE MCUboot image."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from g6he_mac_tool import FirmwareError, inspect_firmware  # noqa: E402


RAM_START = 0x20000000
RAM_END = 0x20080000  # nRF54LM20A: 512 KiB RAM
EXPECTED_HEADER_SIZE = 0x800
EXPECTED_LOAD_ADDRESS = 0x20000800
EXPECTED_FLAGS = 0x20


def entropy(block: bytes) -> float:
    if not block:
        return 0.0
    counts = Counter(block)
    size = len(block)
    return -sum((count / size) * math.log2(count / size) for count in counts.values())


def strings_with_offsets(data: bytes, minimum: int = 5) -> list[tuple[int, str]]:
    found: list[tuple[int, str]] = []
    start: int | None = None
    for offset, value in enumerate(data + b"\x00"):
        if 0x20 <= value <= 0x7E:
            start = offset if start is None else start
        elif start is not None:
            if offset - start >= minimum:
                found.append((start, data[start:offset].decode("ascii")))
            start = None
    return found


def parse_vector_table(data: bytes, offset: int, count: int = 64) -> dict[str, object]:
    available = max(0, min(count, (len(data) - offset) // 4))
    words = list(struct.unpack_from(f"<{available}I", data, offset)) if available else []
    initial_sp = words[0] if words else 0
    handlers = words[1:]
    unique_handlers = sorted(set(value & ~1 for value in handlers if value not in (0, 0xFFFFFFFF)))
    invalid_handlers = [
        {"index": index + 1, "value": f"0x{value:08x}"}
        for index, value in enumerate(handlers)
        if value not in (0, 0xFFFFFFFF)
        and not (RAM_START <= (value & ~1) < RAM_END)
    ]
    even_handlers = [
        {"index": index + 1, "value": f"0x{value:08x}"}
        for index, value in enumerate(handlers)
        if value not in (0, 0xFFFFFFFF) and value & 1 == 0
    ]
    return {
        "offset": f"0x{offset:x}",
        "initial_sp": f"0x{initial_sp:08x}",
        "initial_sp_in_ram": RAM_START <= initial_sp <= RAM_END,
        "reset_vector": f"0x{words[1]:08x}" if len(words) > 1 else None,
        "entry_count": available,
        "unique_handler_addresses": [f"0x{value:08x}" for value in unique_handlers],
        "invalid_handler_addresses": invalid_handlers,
        "non_thumb_handler_addresses": even_handlers,
        "default_handler_repetition": Counter(
            f"0x{value & ~1:08x}" for value in handlers if value not in (0, 0xFFFFFFFF)
        ).most_common(5),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    data = args.firmware.read_bytes()
    report = inspect_firmware(args.firmware)
    header = report["header"]
    header_size = int(header["header_size"])
    image_size = int(header["image_size"])
    load_address = int(header["load_address"], 16)
    flags = int(header["flags"], 16)
    image_end = load_address + header_size + image_size
    file_end_mapped = load_address + len(data)
    initial_sp = struct.unpack_from("<I", data, header_size)[0]

    strings = strings_with_offsets(data)
    selected_patterns = re.compile(
        r"Keychron|G6 HE|54LM|nrf54|Sep \d|\d{2}:\d{2}:\d{2}|"
        r"mouse_ble|ppt_ptx|bt/keys|hall/|TX power|watchdog|fault|assert|fatal|"
        r"WEST_TOPDIR|CMAKE_SOURCE_DIR",
        re.IGNORECASE,
    )
    selected_strings = [
        {"file_offset": f"0x{offset:x}", "value": value}
        for offset, value in strings
        if selected_patterns.search(value)
    ]

    secret_patterns = {
        "private_key_pem": re.compile(r"-----BEGIN (?:EC |RSA |OPENSSH )?PRIVATE KEY-----"),
        "aws_access_key": re.compile(r"AKIA[0-9A-Z]{16}"),
        "github_token": re.compile(r"gh[pousr]_[A-Za-z0-9_]{20,}"),
        "generic_bearer": re.compile(r"Bearer [A-Za-z0-9._~+/=-]{16,}", re.IGNORECASE),
    }
    possible_secrets: list[dict[str, str]] = []
    for offset, value in strings:
        for kind, pattern in secret_patterns.items():
            if pattern.search(value):
                possible_secrets.append({"kind": kind, "file_offset": f"0x{offset:x}"})

    header_version = header["version"]
    version_strings = sorted(
        {
            value
            for _, value in strings
            if re.fullmatch(r"\d+\.\d+\.\d+\+\d+", value)
        }
    )
    timestamp_strings = sorted(
        {
            value
            for _, value in strings
            if re.fullmatch(r"(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec) +\d{1,2} \d{4}", value)
            or re.fullmatch(r"\d{2}:\d{2}:\d{2}", value)
        }
    )

    windows = []
    window_size = 4096
    for offset in range(0, len(data), window_size):
        block = data[offset : offset + window_size]
        windows.append(
            {
                "offset": f"0x{offset:x}",
                "size": len(block),
                "entropy": round(entropy(block), 4),
                "zero_ratio": round(block.count(0) / len(block), 4),
                "ff_ratio": round(block.count(0xFF) / len(block), 4),
            }
        )

    checks = {
        "sha256_matches_known_release": report["sha256"]
        == "38f5bbb0ff3eec06600c766094a01c5da987c75d17192b35acc89dd0aed0695a",
        "embedded_sha512_valid": report["embedded_sha512_verified"],
        "expected_header_size": header_size == EXPECTED_HEADER_SIZE,
        "expected_ram_load_address": load_address == EXPECTED_LOAD_ADDRESS,
        "expected_ram_load_flag": flags == EXPECTED_FLAGS,
        "mapped_file_fits_ram": RAM_START <= load_address < file_end_mapped <= RAM_END,
        "image_body_fits_ram": RAM_START <= load_address < image_end <= RAM_END,
        "initial_stack_pointer_in_ram": RAM_START <= initial_sp <= RAM_END,
        "header_version_embedded_in_payload": header_version in version_strings,
        "no_common_plaintext_secret_pattern": not possible_secrets,
    }

    output = {
        "schema_version": 1,
        "input": {
            "path": str(args.firmware.resolve()),
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
        },
        "mcuboot": report,
        "memory_map": {
            "ram_start": f"0x{RAM_START:08x}",
            "ram_end_exclusive": f"0x{RAM_END:08x}",
            "load_address": f"0x{load_address:08x}",
            "vector_address": f"0x{load_address + header_size:08x}",
            "image_end": f"0x{image_end:08x}",
            "mapped_file_end": f"0x{file_end_mapped:08x}",
            "initial_sp": f"0x{initial_sp:08x}",
            "bytes_image_end_to_initial_sp": initial_sp - image_end,
            "bytes_initial_sp_to_ram_end": RAM_END - initial_sp,
            "bytes_mapped_file_end_to_ram_end": RAM_END - file_end_mapped,
        },
        "vector_table": parse_vector_table(data, header_size),
        "version_and_build_strings": {
            "header_version": header_version,
            "payload_versions": version_strings,
            "timestamps": timestamp_strings,
        },
        "selected_strings": selected_strings,
        "possible_plaintext_secrets": possible_secrets,
        "entropy_windows": windows,
        "checks": checks,
        "limitations": [
            "Ed25519 authenticity is not independently verified without the bootloader public key.",
            "Static analysis cannot prove runtime behavior, timing safety, or power-loss recovery.",
            "RAM margins are address-space observations, not measured runtime stack/heap high-water marks.",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2, ensure_ascii=False) + "\n")
    if not all(checks.values()):
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
