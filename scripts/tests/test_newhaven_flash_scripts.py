"""Cross-platform contract checks; native Windows execution is a separate check."""

from pathlib import Path
import shlex
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPTS = {
    "flash-newhaven-memory.bat": ("board_bring_up", "true_hud", "memory", "memory"),
    "flash-newhaven-self-test.bat": ("display_self_test", "road_scene", "self-test", "self-test"),
    "flash-newhaven-firmware.bat": ("normal", "road_scene", "normal", "normal"),
}


class NewhavenFlashScriptTests(unittest.TestCase):
    def source(self, filename: str) -> str:
        return (ROOT / filename).read_text()

    def command(self, filename: str) -> list[str]:
        commands = [line for line in self.source(filename).splitlines()
                    if line.startswith("call idf.py ")]
        self.assertEqual(len(commands), 1, filename)
        return shlex.split(commands[0], posix=False)

    def test_each_mode_explicitly_selects_the_correct_board_target_and_layout(self) -> None:
        for filename, (mode, layout, build, config) in SCRIPTS.items():
            with self.subTest(script=filename):
                command = self.command(filename)
                self.assertIn("-DIDF_TARGET=esp32s3", command)
                self.assertIn("-DTESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v", command)
                self.assertIn(f"-DTESLA_HEADUP_FIRMWARE_MODE={mode}", command)
                self.assertIn(f"-DTESLA_HEADUP_DASHBOARD_LAYOUT={layout}", command)
                self.assertIn(f"-DSDKCONFIG=sdkconfig.devkitc_n32r16v.{config}", command)
                self.assertEqual(command[command.index("-B") + 1], f"build-newhaven-{build}")
                self.assertEqual(command[-3:], ["build", "flash", "monitor"])

    def test_modes_have_separate_generated_configs_and_build_directories(self) -> None:
        commands = [self.command(name) for name in SCRIPTS]
        self.assertEqual(len({cmd[cmd.index("-B") + 1] for cmd in commands}), 3)
        self.assertEqual(len({next(arg for arg in cmd if arg.startswith("-DSDKCONFIG="))
                              for cmd in commands}), 3)

    def test_paths_port_and_error_status_are_handled_like_a_reusable_batch_entrypoint(self) -> None:
        for filename in SCRIPTS:
            with self.subTest(script=filename):
                source = self.source(filename)
                self.assertIn('pushd "%~dp0firmware"', source)
                self.assertIn('if errorlevel 1 goto firmware_missing', source)
                self.assertIn('where idf.py >nul 2>&1\nif errorlevel 1 goto idf_missing', source)
                self.assertIn('set "PORT_OPTION="', source)
                self.assertIn('if not "%~1"=="" set PORT_OPTION=-p "%~1"', source)
                self.assertIn('%PORT_OPTION%', self.command(filename))
                self.assertIn('if not "%~2"=="" goto usage', source)
                self.assertIn('set "RESULT=%ERRORLEVEL%"\n\npopd\nexit /b %RESULT%', source)
                self.assertIn(':usage\necho Usage: %~nx0 [COM3]\nexit /b 2', source)

    def test_helpers_do_not_erase_settings_or_change_chip_efuses(self) -> None:
        for filename in SCRIPTS:
            command = self.command(filename)
            for forbidden in ("erase-flash", "erase_flash", "erase-region", "set-target", "espefuse"):
                self.assertNotIn(forbidden, command)

    def test_existing_lilygo_entrypoints_are_not_redirected_to_newhaven(self) -> None:
        for filename in ("flash-firmware.bat", "flash-self-test.bat"):
            source = self.source(filename)
            self.assertNotIn("devkitc_n32r16v", source)
            self.assertNotIn("build-newhaven-", source)


if __name__ == "__main__":
    unittest.main()
