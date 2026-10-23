#pragma once

#include <array>
#include <cstddef>

namespace tesla_headup::board::devkitc_n32r16v {

inline constexpr std::size_t flash_bytes = 32U * 1024U * 1024U;
inline constexpr std::size_t psram_bytes = 16U * 1024U * 1024U;

// Used by the RGB adapter; not driven by board_bring_up.
// ESP-IDF RGB565 data_gpio_nums order: B[3:7], G[2:7], R[3:7].
inline constexpr std::array<int, 16> rgb_data_pins{
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21};
inline constexpr int pixel_clock_pin = 39;
inline constexpr int hsync_pin = 40;
inline constexpr int vsync_pin = 41;
inline constexpr int data_enable_pin = 42;
inline constexpr int display_enable_pin = 1;
// MCU-side PWM: use a non-inverting level shifter. NHD-TFT40 pulls PWM to 5 V.
inline constexpr int backlight_pwm_pin = 2;

// Excludes strapping (0/3/45/46), native USB (19/20), UART0 (43/44),
// memory (26..37), LEDs (38 on v1.1, 48 on v1.0), and unexposed pins.
constexpr bool is_safe_display_pin(int pin) noexcept {
    return pin == 1 || pin == 2 || (pin >= 4 && pin <= 18) ||
           pin == 21 || (pin >= 39 && pin <= 42) || pin == 47;
}

inline constexpr std::array<int, 22> display_pins{
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21,
    pixel_clock_pin, hsync_pin, vsync_pin, data_enable_pin,
    display_enable_pin, backlight_pwm_pin};

constexpr bool valid_display_pin_assignment() noexcept {
    for (std::size_t i = 0; i < display_pins.size(); ++i) {
        if (!is_safe_display_pin(display_pins[i])) {
            return false;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (display_pins[i] == display_pins[j]) {
                return false;
            }
        }
    }
    for (std::size_t i = 0; i < rgb_data_pins.size(); ++i) {
        if (rgb_data_pins[i] != display_pins[i]) {
            return false;
        }
    }
    return true;
}

static_assert(valid_display_pin_assignment());

}  // namespace tesla_headup::board::devkitc_n32r16v
