"""Exercise the Windows flash launcher's plans without SDK or hardware access."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
POWERSHELL = shutil.which('pwsh') or shutil.which('powershell')


@unittest.skipUnless(POWERSHELL, 'PowerShell is required for Windows launcher checks')
class FlashScriptTests(unittest.TestCase):
    def plan(self, *arguments):
        # The script must resolve the repository from its own path, not the cwd.
        with tempfile.TemporaryDirectory() as directory:
            return subprocess.run(
                [POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                 str(ROOT / 'scripts/flash.ps1'), '-DryRun', *arguments],
                cwd=directory, capture_output=True, text=True, check=False)

    def test_default_builds_checks_size_then_flashes_and_monitors(self):
        result = self.plan('COM7')
        self.assertEqual(result.returncode, 0, result.stderr)
        commands = [line for line in result.stdout.splitlines() if line.startswith('& ')]
        self.assertEqual(len(commands), 3)
        self.assertIn('-DTESLA_HEADUP_BOARD_PROFILE=h619', commands[0])
        self.assertIn('-DIDF_TARGET=esp32s3', commands[0])
        self.assertIn('-DTESLA_HEADUP_DASHBOARD_LAYOUT=true_hud', commands[0])
        self.assertIn('build-flash-h619-true_hud-normal', commands[0])
        self.assertIn('sdkconfig', commands[0])
        self.assertIn('check_firmware_size.py', commands[1])
        self.assertIn('1179648', commands[1])
        self.assertIn("'-p' 'COM7'", commands[2])
        self.assertIn("'flash' 'monitor'", commands[2])

    def test_layouts_and_modes_have_isolated_builds(self):
        for layout in ('true_hud', 'road_scene'):
            for mode in ('normal', 'display_self_test'):
                with self.subTest(layout=layout, mode=mode):
                    result = self.plan('-Layout', layout, '-Mode', mode)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn(f'build-flash-h619-{layout}-{mode}', result.stdout)
                    self.assertIn(f'-DTESLA_HEADUP_FIRMWARE_MODE={mode}', result.stdout)

    def test_port_autodetection_and_custom_baud_without_monitor(self):
        result = self.plan('-NoMonitor', '-Baud', '115200')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('port: auto', result.stdout)
        self.assertNotIn("'-p'", result.stdout)
        self.assertNotIn("'monitor'", result.stdout)
        self.assertIn("'-b' '115200' 'flash'", result.stdout)

    def test_build_only_never_plans_a_flash(self):
        result = self.plan('-BuildOnly')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertNotIn("'flash'", result.stdout)
        self.assertNotIn("'monitor'", result.stdout)
        self.assertIn('check_firmware_size.py', result.stdout)

    def test_invalid_layout_mode_and_baud_are_rejected(self):
        for arguments in (('-Layout', '../other'), ('-Layout', 'minimal'), ('-Mode', 'board_bring_up'),
                          ('-Baud', '0')):
            with self.subTest(arguments=arguments):
                self.assertNotEqual(self.plan(*arguments).returncode, 0)

    def test_checkout_paths_with_spaces_are_kept_as_single_arguments(self):
        with tempfile.TemporaryDirectory() as directory:
            checkout = Path(directory) / 'project with spaces'
            script_directory = checkout / 'scripts'
            script_directory.mkdir(parents=True)
            launcher = script_directory / 'flash.ps1'
            shutil.copyfile(ROOT / 'scripts/flash.ps1', launcher)
            result = subprocess.run(
                [POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                 str(launcher), 'COM7', '-DryRun'],
                capture_output=True, text=True, check=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(f"'{checkout / 'firmware'}'", result.stdout)
            self.assertIn(f"'-DSDKCONFIG={checkout / 'build-flash-h619-true_hud-normal' / 'sdkconfig'}'",
                          result.stdout)

    def test_missing_sdk_stops_before_build_or_flash(self):
        with tempfile.TemporaryDirectory() as directory:
            result = subprocess.run(
                [POWERSHELL, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                 str(ROOT / 'scripts/flash.ps1'), '-BuildOnly', '-IdfPath', directory],
                capture_output=True, text=True, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('ESP-IDF was not found', result.stderr)
            self.assertNotIn("'build'", result.stdout)
            self.assertNotIn("'flash'", result.stdout)
