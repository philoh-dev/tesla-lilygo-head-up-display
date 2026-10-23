from pathlib import Path
import tempfile
import unittest

from scripts.check_firmware_size import check_firmware_size


class FirmwareSizeCheckTests(unittest.TestCase):
    def test_accepts_binary_at_budget(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "firmware.bin"
            binary.write_bytes(b"1234")

            self.assertEqual(check_firmware_size(binary, 4), 4)

    def test_rejects_binary_over_budget(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "firmware.bin"
            binary.write_bytes(b"12345")

            with self.assertRaisesRegex(ValueError, "exceeds size budget"):
                check_firmware_size(binary, 4)

    def test_rejects_missing_binary(self) -> None:
        with self.assertRaises(FileNotFoundError):
            check_firmware_size(Path("missing-firmware.bin"), 4)

    def test_rejects_non_positive_budget(self) -> None:
        with self.assertRaises(ValueError):
            check_firmware_size(Path("unused.bin"), 0)


if __name__ == "__main__":
    unittest.main()
