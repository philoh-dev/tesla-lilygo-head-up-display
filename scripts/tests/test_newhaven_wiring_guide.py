"""Pin-map consistency checks, not verification of actual hardware or wiring."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
GUIDE = (ROOT / "docs/newhaven-wiring.md").read_text()
BOARD = (ROOT / "firmware/boards/devkitc_n32r16v/board.hpp").read_text()


def gpio_rows() -> list[tuple[str, int, str, int, str]]:
    return [(signal, int(gpio), header, int(breakout), fpc)
            for signal, gpio, header, breakout, fpc in re.findall(
                r"^\| ([RGB][0-7]|PCLK|HSYNC|VSYNC|DE|DISP|PWM) \| (\d+) \| "
                r"(J[13]\.\d+) \| (\d+) \| (\d+|driver) \|$", GUIDE, re.MULTILINE)]


class NewhavenWiringGuideTests(unittest.TestCase):
    def test_gpio_mapping_matches_firmware_without_reordering_color_bits(self) -> None:
        rows = gpio_rows()
        self.assertEqual(len(rows), 22)
        pins = re.search(r"rgb_data_pins\s*\{([^}]+)\}", BOARD)
        self.assertIsNotNone(pins)
        self.assertEqual([row[1] for row in rows[:16]],
                         [int(pin) for pin in re.findall(r"\d+", pins.group(1))])
        controls = {"PCLK": "pixel_clock_pin", "HSYNC": "hsync_pin",
                    "VSYNC": "vsync_pin", "DE": "data_enable_pin",
                    "DISP": "display_enable_pin", "PWM": "backlight_pwm_pin"}
        for signal, gpio, _, _, _ in rows[16:]:
            self.assertRegex(BOARD, rf"{controls[signal]}\s*=\s*{gpio};")

    def test_panel_and_breakout_numbers_are_separate_and_match_color_ranges(self) -> None:
        for signal, _, _, header, fpc in gpio_rows()[:16]:
            color, bit = signal[0], int(signal[1])
            self.assertEqual(header, {"R": 21, "G": 7, "B": 29}[color] + bit)
            self.assertEqual(int(fpc), {"R": 5, "G": 13, "B": 21}[color] + bit)
        controls = {row[0]: (row[3], row[4]) for row in gpio_rows()[16:]}
        self.assertEqual(controls, {"PCLK": (15, "30"), "HSYNC": (17, "32"),
                                   "VSYNC": (18, "33"), "DE": (19, "34"),
                                   "DISP": (16, "31"), "PWM": (5, "driver")})

    def test_devkit_header_positions_match_the_documented_gpio_labels(self) -> None:
        headers = {4: "J1.4", 5: "J1.5", 6: "J1.6", 7: "J1.7", 8: "J1.12",
                   9: "J1.15", 10: "J1.16", 11: "J1.17", 12: "J1.18",
                   13: "J1.19", 14: "J1.20", 15: "J1.8", 16: "J1.9",
                   17: "J1.10", 18: "J1.11", 21: "J3.18", 39: "J3.9",
                   40: "J3.8", 41: "J3.7", 42: "J3.6", 1: "J3.4", 2: "J3.5"}
        self.assertEqual({row[1]: row[2] for row in gpio_rows()}, headers)

    def test_grounded_unused_inputs_and_all_forty_header_positions_are_accounted_for(self) -> None:
        grounded = {(signal, int(header), int(fpc)) for signal, header, fpc in re.findall(
            r"^\| ([RGB][0-7]|BIST) \| (\d+) \| (\d+) \| GND[^|]*\|$", GUIDE, re.MULTILINE)}
        self.assertEqual(grounded, {("R0", 21, 5), ("R1", 22, 6), ("R2", 23, 7),
                                   ("G0", 7, 13), ("G1", 8, 14), ("B0", 29, 21),
                                   ("B1", 30, 22), ("B2", 31, 23), ("BIST", 20, 35)})
        active = {row[3] for row in gpio_rows()}
        self.assertTrue(active.isdisjoint({row[1] for row in grounded}))
        self.assertEqual(active | {row[1] for row in grounded} | {1, 2, 3, 4, 6, 37, 38, 39, 40},
                         set(range(1, 41)))

    def test_unresolved_power_and_orientation_checks_are_not_presented_as_cleared(self) -> None:
        for required in ("Power-on is blocked", "MIC5225", "150 mA", "240 mA",
                         "Do not connect GPIO2 directly", "level shifter",
                         "without a display connected can damage", "SJ1, SJ2 and SJ3 all open",
                         "physical insertion orientation still needs", "Do not independently feed H6"):
            self.assertIn(required, GUIDE)


if __name__ == "__main__":
    unittest.main()
