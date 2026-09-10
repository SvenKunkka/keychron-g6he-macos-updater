from __future__ import annotations

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from g6he_mac_tool import (
    FirmwareError,
    _read_response,
    build_frame,
    inspect_firmware,
    updater_crc32,
)


def make_test_image() -> bytes:
    header_size = 32
    payload = struct.pack("<II", 0x20010000, 0x20000101) + b"test-payload"
    header = struct.pack(
        "<IIHHIIBBHII",
        0x96F3B83D,
        0x20000000,
        header_size,
        0,
        len(payload),
        0x20,
        1,
        2,
        3,
        4,
        0,
    )
    digest = hashlib.sha512(header + payload).digest()
    tlvs = struct.pack("<BBH", 0x12, 0, len(digest)) + digest
    tlv_info = struct.pack("<HH", 0x6907, 4 + len(tlvs))
    return header + payload + tlv_info + tlvs


class FirmwareParserTests(unittest.TestCase):
    def write_image(self, data: bytes) -> Path:
        handle = tempfile.NamedTemporaryFile(suffix=".signed.bin", delete=False)
        handle.write(data)
        handle.close()
        self.addCleanup(Path(handle.name).unlink, missing_ok=True)
        return Path(handle.name)

    def test_valid_mcuboot_sha512_image(self) -> None:
        report = inspect_firmware(self.write_image(make_test_image()))
        self.assertEqual(report["container"], "MCUboot signed image")
        self.assertEqual(report["header"]["version"], "1.2.3+4")
        self.assertTrue(report["embedded_sha512_verified"])
        self.assertEqual(report["payload_vector"]["reset_vector"], "0x20000101")

    def test_rejects_tampered_payload(self) -> None:
        image = bytearray(make_test_image())
        image[33] ^= 0x01
        with self.assertRaises(FirmwareError):
            inspect_firmware(self.write_image(bytes(image)))

    def test_rejects_wrong_magic(self) -> None:
        image = bytearray(make_test_image())
        image[0:4] = b"BAD!"
        with self.assertRaises(FirmwareError):
            inspect_firmware(self.write_image(bytes(image)))


class UpdaterProtocolTests(unittest.TestCase):
    def test_builds_captured_version_query(self) -> None:
        frame = build_frame(1, b"\x60")
        self.assertEqual(frame[:9].hex(), "b2aa5503fc01606000")
        self.assertEqual(len(frame), 33)
        self.assertEqual(frame[9:], bytes(24))

    def test_builds_update_frame_marker(self) -> None:
        frame = build_frame(4, b"\x63", update_frame=True)
        self.assertEqual(frame[:9].hex(), "b2aa5603fc04636300")

    def test_decodes_captured_two_report_version_response(self) -> None:
        packets = [
            bytes.fromhex(
                "b1aa5528d701a301600035344c4d4736484500000503312e302e302b3832000035"
            ),
            bytes.fromhex("b1344c4d76312e3000000100a206") + bytes(19),
        ]

        class FakeDevice:
            def read(self, _size: int, timeout: int) -> bytes:
                self.assert_timeout = timeout
                return packets.pop(0)

        payload = _read_response(FakeDevice())
        self.assertEqual(
            payload,
            bytes.fromhex(
                "a301600035344c4d4736484500000503312e302e302b3832000035344c4d76312e3000000100"
            ),
        )

    def test_crc_matches_updater_variant(self) -> None:
        self.assertEqual(updater_crc32(b"123456789"), 0x340BC6D9)


if __name__ == "__main__":
    unittest.main()
