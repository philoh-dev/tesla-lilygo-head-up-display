#pragma once

#include "tesla_headup/adapters/lvgl_rgb/rgb_frame.hpp"

#include <array>
#include <cstdio>
#include <optional>
#include <string_view>

namespace tesla_headup::platform {

enum class RgbDiagnosticPattern : std::uint8_t {
    Red, Green, Blue, White, Black, Checkerboard, Corners, Viewport, Count
};

class RgbDiagnostics final {
public:
    static constexpr std::array<std::string_view, 8> names{
        "red", "green", "blue", "white", "black", "checkerboard", "corners", "viewport"};

    [[nodiscard]] static constexpr bool valid(RgbDiagnosticPattern pattern) noexcept {
        return static_cast<unsigned>(pattern) < names.size();
    }
    [[nodiscard]] static constexpr std::string_view name(RgbDiagnosticPattern pattern) noexcept {
        return valid(pattern) ? names[static_cast<unsigned>(pattern)] : "invalid";
    }
    [[nodiscard]] static constexpr std::optional<RgbDiagnosticPattern> parse(std::string_view name) noexcept {
        for (unsigned i = 0; i < names.size(); ++i) {
            if (names[i] == name) return static_cast<RgbDiagnosticPattern>(i);
        }
        return std::nullopt;
    }
    [[nodiscard]] static constexpr RgbDiagnosticPattern next(RgbDiagnosticPattern pattern) noexcept {
        return valid(pattern) ? static_cast<RgbDiagnosticPattern>((static_cast<unsigned>(pattern) + 1U) % names.size())
                              : RgbDiagnosticPattern::Red;
    }
    [[nodiscard]] static constexpr RgbDiagnosticPattern previous(RgbDiagnosticPattern pattern) noexcept {
        return valid(pattern) ? static_cast<RgbDiagnosticPattern>((static_cast<unsigned>(pattern) + names.size() - 1U) % names.size())
                              : RgbDiagnosticPattern::Red;
    }

    // Complete native RGB565 scanout frame; no LVGL, allocations or assets.
    // Solids/checkerboard have no overlays so every output pixel is diagnostic.
    [[nodiscard]] static bool render(std::span<std::uint16_t> output,
        RgbDiagnosticPattern pattern, RgbPlacement origin, bool mirrored) noexcept {
        if (output.size() != RgbFrame::pixels || !valid(pattern) || !RgbFrame::valid(origin)) return false;
        constexpr std::array<std::uint16_t, 5> solids{0xf800, 0x07e0, 0x001f, 0xffff, 0x0000};
        std::fill(output.begin(), output.end(), static_cast<unsigned>(pattern) < solids.size()
            ? solids[static_cast<unsigned>(pattern)] : std::uint16_t{0});
        Painter p{output};
        if (pattern == RgbDiagnosticPattern::Checkerboard) {
            for (int y = 0; y < 480; ++y)
                for (int x = 0; x < 800; ++x)
                    p.pixel(x, y, ((x / 20 + y / 20) & 1) ? 0xffff : 0);
        } else if (pattern == RgbDiagnosticPattern::Corners) {
            p.border(0, 0, 800, 480, 1, 0xffff);
            p.text(8, 8, "TL 0,0", 3);
            p.text(624, 8, "TR 799,0", 3);
            p.text(8, 448, "BL 0,479", 3);
            p.text(588, 448, "BR 799,479", 3);
            p.text(373, 60, "TOP", 3);
            p.text(346, 400, "BOTTOM", 3);
            p.text(38, 220, "LEFT", 3);
            p.text(650, 220, "RIGHT", 3);
            p.text(274, 220, "800X480 RGB", 3);
            p.rect(399, 140, 3, 50, 0xffff);
            for (int i = 0; i < 15; ++i) {
                p.rect(400 - i, 140 + i, 2, 2, 0xffff);
                p.rect(400 + i, 140 + i, 2, 2, 0xffff);
            }
            p.rect(280, 275, 80, 40, 0xf800);
            p.rect(360, 275, 80, 40, 0x07e0);
            p.rect(440, 275, 80, 40, 0x001f);
            p.text(310, 325, "R", 3);
            p.text(390, 325, "G", 3);
            p.text(470, 325, "B", 3);
        } else if (pattern == RgbDiagnosticPattern::Viewport) {
            p.border(0, 0, 800, 480, 1, 0xffff);
            p.border(origin.x, origin.y, 536, 240, 2, 0x07ff);
            p.text(origin.x + 8, origin.y + 8, "TL", 2);
            p.text(origin.x + 502, origin.y + 8, "TR", 2);
            p.text(origin.x + 8, origin.y + 218, "BL", 2);
            p.text(origin.x + 502, origin.y + 218, "BR", 2);
            p.text(origin.x + 178, origin.y + 70, "536X240 VIEWPORT", 2);
            p.text(origin.x + 184, origin.y + 100, "800X480 PANEL", 2);
            char position[32]{};
            (void)std::snprintf(position, sizeof(position), "X=%d Y=%d", origin.x, origin.y);
            p.text(origin.x + 196, origin.y + 130, position, 2);
            p.text(origin.x + 178, origin.y + 160, "CYAN = VIEWPORT", 2);
        }
        // Exactly one final horizontal mirror, including text and placement.
        if (mirrored)
            for (int y = 0; y < 480; ++y)
                std::reverse(output.begin() + y * 800, output.begin() + (y + 1) * 800);
        return true;
    }

private:
    // Small code-native 5x7 calibration font, independent of dashboard fonts.
    [[nodiscard]] static std::array<std::uint8_t, 7> glyph(char c) noexcept {
        switch (c) {
        case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
        case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
        case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
        case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
        case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
        case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
        case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
        case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
        case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
        case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
        case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
        case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
        case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
        case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
        case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
        case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
        case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
        case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
        case '=': return {0,0,31,0,31,0,0}; case ',': return {0,0,0,0,0,4,8};
        default: return {};
        }
    }
    struct Painter {
        std::span<std::uint16_t> pixels;
        void pixel(int x, int y, std::uint16_t color) noexcept {
            if (x >= 0 && x < 800 && y >= 0 && y < 480) pixels[y * 800 + x] = color;
        }
        void rect(int x, int y, int w, int h, std::uint16_t color) noexcept {
            for (int row = std::max(y, 0); row < std::min(y + h, 480); ++row)
                for (int col = std::max(x, 0); col < std::min(x + w, 800); ++col)
                    pixel(col, row, color);
        }
        void border(int x, int y, int w, int h, int thickness, std::uint16_t color) noexcept {
            rect(x, y, w, thickness, color); rect(x, y + h - thickness, w, thickness, color);
            rect(x, y, thickness, h, color); rect(x + w - thickness, y, thickness, h, color);
        }
        void text(int x, int y, std::string_view value, int scale) noexcept {
            for (const char c : value) {
                const auto rows = glyph(c);
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (rows[row] & (1U << (4 - col)))
                            rect(x + col * scale, y + row * scale, scale, scale, 0xffff);
                x += 6 * scale;
            }
        }
    };
};

class RgbDiagnosticSequence final {
public:
    static constexpr std::uint32_t interval_ms = 3000;
    [[nodiscard]] RgbDiagnosticPattern pattern() const noexcept { return pattern_; }
    [[nodiscard]] bool automatic() const noexcept { return automatic_; }
    void select(RgbDiagnosticPattern pattern, std::uint32_t now) noexcept {
        if (RgbDiagnostics::valid(pattern)) pattern_ = pattern;
        last_ = now; started_ = true;
    }
    void set_automatic(bool value, std::uint32_t now) noexcept {
        automatic_ = value; last_ = now; started_ = true;
    }
    [[nodiscard]] bool tick(std::uint32_t now) noexcept {
        if (!started_) { last_ = now; started_ = true; return true; }
        if (!automatic_) return false;
        const auto steps = static_cast<std::uint32_t>(now - last_) / interval_ms;
        if (steps == 0) return false;
        last_ += steps * interval_ms;
        const auto old = pattern_;
        pattern_ = static_cast<RgbDiagnosticPattern>((static_cast<unsigned>(pattern_) + steps % 8U) % 8U);
        return old != pattern_;
    }
private:
    RgbDiagnosticPattern pattern_{RgbDiagnosticPattern::Red};
    std::uint32_t last_{0};
    bool automatic_{true};
    bool started_{false};
};

}  // namespace tesla_headup::platform
