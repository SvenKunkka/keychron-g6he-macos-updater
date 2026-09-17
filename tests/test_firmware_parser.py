from __future__ import annotations

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from g6he_mac_tool import (
    STATUS_HARDWARE_VERIFIED,
    STATUS_INCOMPATIBLE,
    STATUS_UNVERIFIED_MODEL,
    STATUS_UNVERIFIED_PRODUCT_ID,
    FirmwareError,
    _device_compatibility,
    _query_after_restart,
    _read_response,
    _version_relation,
    build_frame,
    inspect_firmware,
    updater_crc32,
)


G6_MODEL = "54LMG6HE"
G6_PROTOCOL_INFO = {
    "model": G6_MODEL,
    "protocol_version": 1,
    "dfu_version": 0,
    "supported_update_modes": 1,
    "bootloader_required": False,
}


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
    tlvs = (
        struct.pack("<BBH", 0x12, 0, len(digest))
        + digest
        + struct.pack("<BBH", 0x01, 0, 64)
        + bytes(range(64))
        + struct.pack("<BBH", 0x24, 0, 64)
        + bytes(reversed(range(64)))
    )
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
        self.assertEqual(report["authentication"]["signature_tlv_count"], 1)

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

    def test_rejects_missing_signature(self) -> None:
        image = bytearray(make_test_image())
        header_size = struct.unpack_from("<H", image, 8)[0]
        image_size = struct.unpack_from("<I", image, 12)[0]
        tlv_offset = header_size + image_size
        image = image[:-68]
        struct.pack_into("<H", image, tlv_offset + 2, len(image) - tlv_offset)
        with self.assertRaisesRegex(FirmwareError, "signature TLV"):
            inspect_firmware(self.write_image(bytes(image)))

    def test_rejects_zero_reset_vector_even_with_valid_digest(self) -> None:
        image = bytearray(make_test_image())
        header_size = struct.unpack_from("<H", image, 8)[0]
        image_size = struct.unpack_from("<I", image, 12)[0]
        tlv_offset = header_size + image_size
        struct.pack_into("<I", image, header_size + 4, 0)
        digest_offset = tlv_offset + 8
        image[digest_offset : digest_offset + 64] = hashlib.sha512(
            image[:tlv_offset]
        ).digest()
        with self.assertRaisesRegex(FirmwareError, "reset vector"):
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

    def test_version_relation_accepts_optional_v_prefix(self) -> None:
        self.assertEqual(_version_relation("v1.2.3+7", "1.2.3+8"), "upgrade")
        self.assertEqual(_version_relation("1.2.3+7", "v1.2.3+6"), "downgrade")
        self.assertEqual(_version_relation("v1.2.3+7", "1.2.3+7"), "same")

    def test_unknown_keychron_model_is_protocol_compatible(self) -> None:
        result = _device_compatibility(
            {"product_id": 0xD05A, "product_string": "Keychron TurboLink 8K"},
            {
                "model": "54L2DNGD",
                "protocol_version": 1,
                "dfu_version": 0,
                "supported_update_modes": 1,
                "bootloader_required": False,
            },
        )
        self.assertTrue(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_UNVERIFIED_MODEL)
        self.assertEqual(result["compatibility_warnings"], [])


class DeviceCompatibilityTests(unittest.TestCase):
    """The USB product ID is advisory metadata, not a hardware whitelist.

    Regression coverage for the reported failure where a genuine Keychron G6 HE 8K
    (model ``54LMG6HE``) was rejected because it enumerated with product ID
    ``0xd09d`` instead of the single product ID recorded during the original
    acceptance run (``0xd086``).
    """

    def test_acceptance_verified_pair_remains_hardware_verified(self) -> None:
        result = _device_compatibility({"product_id": 0xD086}, dict(G6_PROTOCOL_INFO))
        self.assertTrue(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_HARDWARE_VERIFIED)
        self.assertEqual(result["compatibility_warnings"], [])
        self.assertTrue(result["product_id_recognized"])
        self.assertEqual(result["known_product_ids"], ["0xd086", "0xd09d"])

    def test_recorded_second_product_id_is_compatible_without_warning(self) -> None:
        result = _device_compatibility({"product_id": 0xD09D}, dict(G6_PROTOCOL_INFO))
        self.assertTrue(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_UNVERIFIED_PRODUCT_ID)
        self.assertEqual(result["compatibility_reasons"], [])
        self.assertEqual(result["compatibility_warnings"], [])
        self.assertTrue(result["product_id_recognized"])

    def test_unrecorded_product_id_warns_but_stays_compatible(self) -> None:
        result = _device_compatibility({"product_id": 0x1234}, dict(G6_PROTOCOL_INFO))
        self.assertTrue(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_UNVERIFIED_PRODUCT_ID)
        self.assertEqual(result["compatibility_reasons"], [])
        self.assertFalse(result["product_id_recognized"])
        self.assertEqual(len(result["compatibility_warnings"]), 1)
        warning = result["compatibility_warnings"][0]
        self.assertIn("0x1234", warning)
        self.assertIn(G6_MODEL, warning)
        self.assertIn("0xd086", warning)

    def test_strict_product_id_promotes_warning_to_blocker(self) -> None:
        result = _device_compatibility(
            {"product_id": 0x1234}, dict(G6_PROTOCOL_INFO), strict_product_id=True
        )
        self.assertFalse(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_INCOMPATIBLE)
        self.assertEqual(len(result["compatibility_reasons"]), 1)
        self.assertEqual(result["compatibility_warnings"], [])

    def test_protocol_failure_still_blocks_known_model(self) -> None:
        info = dict(G6_PROTOCOL_INFO, protocol_version=2)
        result = _device_compatibility({"product_id": 0xD09D}, info)
        self.assertFalse(result["compatible"])
        self.assertEqual(result["compatibility_status"], STATUS_INCOMPATIBLE)
        self.assertIn("unsupported protocol 2", result["compatibility_reasons"])

    def test_product_id_change_across_restart_still_verifies(self) -> None:
        """Post-restart verification must not require an unchanged product ID."""
        after = {
            "device_id": "after",
            "product_id": "0xd086",
            "model": G6_MODEL,
            "firmware_version": "1.0.0+84",
        }
        with mock.patch("g6he_mac_tool.query_device", side_effect=OSError("gone")), \
                mock.patch("g6he_mac_tool.discover_devices", return_value=[after]):
            verified = _query_after_restart(G6_MODEL, "before")
        self.assertEqual(verified["product_id"], "0xd086")
        self.assertEqual(verified["firmware_version"], "1.0.0+84")

    def test_restart_verification_rejects_ambiguous_matches(self) -> None:
        first = {"model": G6_MODEL, "product_id": "0xd086"}
        second = {"model": G6_MODEL, "product_id": "0xd09d"}
        with mock.patch("g6he_mac_tool.query_device", side_effect=OSError("gone")), \
                mock.patch(
                    "g6he_mac_tool.discover_devices", return_value=[first, second]
                ):
            with self.assertRaisesRegex(Exception, "ambiguous"):
                _query_after_restart(G6_MODEL, None)


if __name__ == "__main__":
    unittest.main()
