"""Host checks for board selection/isolation; actual Kconfig resolution runs in CI."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
FIRMWARE = ROOT / "firmware"


def defaults(board: str) -> dict[str, str]:
    lines = (FIRMWARE / "boards" / board / "sdkconfig.defaults").read_text().splitlines()
    return dict(line.split("=", 1) for line in lines if line.startswith("CONFIG_"))


class BoardProfileTests(unittest.TestCase):
    def test_rgb_diagnostics_requires_devkitc_and_dedicated_mode_defaults(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                ["cmake", "-S", str(FIRMWARE), "-B", directory,
                 "-DTESLA_HEADUP_BOARD_PROFILE=h619", "-DTESLA_HEADUP_FIRMWARE_MODE=rgb_diagnostics"],
                capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("rgb_diagnostics requires TESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v",
                          result.stdout + result.stderr)
        diagnostic = (FIRMWARE / "modes/rgb_diagnostics.sdkconfig.defaults").read_text()
        self.assertIn("CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS=y", diagnostic)
        self.assertIn("CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST=n", diagnostic)
        for mode in ("normal", "display_self_test"):
            self.assertIn("CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS=n",
                          (FIRMWARE / f"modes/{mode}.sdkconfig.defaults").read_text())

    def select(self, *settings: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "select.cmake"
            script.write_text(
                "cmake_minimum_required(VERSION 3.24)\n" + "\n".join(settings) + "\n"
                f'include("{FIRMWARE.as_posix()}/cmake/board_profile.cmake")\n'
                'message("profile=${TESLA_HEADUP_BOARD_PROFILE}")\n'
                'message("defaults=${TESLA_HEADUP_BOARD_DEFAULTS}")\n'
                'message("sdkconfig=${SDKCONFIG}")\n'
                'message("mode=${TESLA_HEADUP_DEFAULT_FIRMWARE_MODE}")\n'
                'message("target=${IDF_TARGET}")\n'
            )
            return subprocess.run(
                ["cmake", "-P", str(script)], capture_output=True, text=True, check=False
            )

    def test_default_remains_h619_without_overriding_sdkconfig(self) -> None:
        result = self.select()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("profile=h619", result.stderr)
        self.assertIn("boards/h619/sdkconfig.defaults", result.stderr)
        self.assertIn("sdkconfig=\n", result.stderr)
        self.assertIn("mode=normal", result.stderr)

    def test_devkitc_uses_isolated_config_and_memory_bring_up(self) -> None:
        result = self.select('set(TESLA_HEADUP_BOARD_PROFILE "devkitc_n32r16v")')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("boards/devkitc_n32r16v/sdkconfig.defaults", result.stderr)
        self.assertIn("sdkconfig.devkitc_n32r16v", result.stderr)
        self.assertIn("mode=board_bring_up", result.stderr)
        self.assertIn("target=esp32s3", result.stderr)

    def test_devkitc_rejects_wrong_chip_target(self) -> None:
        result = self.select('set(TESLA_HEADUP_BOARD_PROFILE "devkitc_n32r16v")',
                             'set(IDF_TARGET "esp32")')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("requires IDF_TARGET=esp32s3", result.stderr)

    def test_explicit_sdkconfig_is_preserved(self) -> None:
        result = self.select(
            'set(TESLA_HEADUP_BOARD_PROFILE "devkitc_n32r16v")',
            'set(SDKCONFIG "sdkconfig.custom")',
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("sdkconfig=sdkconfig.custom", result.stderr)

    def test_unknown_and_path_like_profiles_are_rejected(self) -> None:
        for profile in ("unknown", "../h619", "h619/.."):
            with self.subTest(profile=profile):
                result = self.select(f'set(TESLA_HEADUP_BOARD_PROFILE "{profile}")')
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("Unknown Tesla Head-Up Display board profile", result.stderr)

    def test_devkitc_defaults_select_documented_memory_without_overclocking(self) -> None:
        config = defaults("devkitc_n32r16v")
        for key in (
            "ESPTOOLPY_OCT_FLASH", "ESPTOOLPY_FLASHMODE_OPI",
            "ESPTOOLPY_FLASH_SAMPLE_MODE_DTR", "ESPTOOLPY_FLASHFREQ_80M",
            "ESPTOOLPY_FLASHSIZE_32MB", "SPIRAM", "SPIRAM_MODE_OCT",
            "SPIRAM_TYPE_AUTO", "SPIRAM_SPEED_80M", "SPIRAM_BOOT_INIT",
            "SPIRAM_MEMTEST", "SPIRAM_USE_MALLOC", "ESP_CONSOLE_UART_DEFAULT",
            "SPIRAM_FETCH_INSTRUCTIONS", "SPIRAM_RODATA", "TESLA_HEADUP_DISPLAY_RGB",
        ):
            self.assertEqual(config[f"CONFIG_{key}"], "y", key)
        self.assertEqual(config["CONFIG_SPIRAM_ECC_ENABLE"], "n")
        self.assertEqual(config["CONFIG_SPIRAM_IGNORE_NOTFOUND"], "n")
        self.assertFalse(any("TESLA_HEADUP_DISPLAY_PIN" in key for key in config))

    def test_h619_retains_qio_16mb_and_original_qspi_pins(self) -> None:
        config = defaults("h619")
        self.assertEqual(config["CONFIG_ESPTOOLPY_FLASHMODE_QIO"], "y")
        self.assertEqual(config["CONFIG_ESPTOOLPY_FLASHSIZE_16MB"], "y")
        self.assertEqual(config["CONFIG_SPIRAM_MODE_OCT"], "y")
        for name, pin in {"SCK": 47, "DATA0": 18, "DATA1": 7, "DATA2": 48,
                          "DATA3": 5, "CS": 6, "RESET": 17, "POWER": 38}.items():
            self.assertEqual(config[f"CONFIG_TESLA_HEADUP_DISPLAY_PIN_{name}"], str(pin))

    def test_h619_cannot_select_devkitc_memory_bring_up(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                ["cmake", "-S", str(FIRMWARE), "-B", directory,
                 "-DTESLA_HEADUP_BOARD_PROFILE=h619", "-DTESLA_HEADUP_FIRMWARE_MODE=board_bring_up"],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("board_bring_up requires TESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v",
                          result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
