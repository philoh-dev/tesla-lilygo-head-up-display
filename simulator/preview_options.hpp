#pragma once

#include "tesla_headup/adapters/lvgl_rgb/rgb_frame.hpp"
#include "tesla_headup/adapters/lvgl_rgb/rgb_diagnostics.hpp"

#include <charconv>
#include <string_view>

namespace tesla_headup::simulator {

struct PreviewOptions {
    bool rgb{false};
    bool diagnostics{false};
    platform::RgbDiagnosticPattern diagnostic_pattern{platform::RgbDiagnosticPattern::Red};
    bool explicit_pattern{false};
    bool viewport{false};
    int preview_speed{135};
    int preview_speed_limit{50};
    platform::RgbPlacement origin{};
    std::string_view error{};

    [[nodiscard]] bool full_road_scene() const noexcept {
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
        return rgb && !diagnostics && !viewport;
#else
        return false;
#endif
    }
    [[nodiscard]] platform::RgbSurface surface() const noexcept {
        return full_road_scene() ? platform::RgbSurface{800, 480} : platform::RgbSurface{};
    }

    [[nodiscard]] int output_width() const noexcept {
        return rgb ? platform::RgbFrame::width : platform::RgbFrame::dashboard_width;
    }
    [[nodiscard]] int output_height() const noexcept {
        return rgb ? platform::RgbFrame::height : platform::RgbFrame::dashboard_height;
    }

    // Other arguments belong to the existing telemetry/screenshot preview.
    [[nodiscard]] static PreviewOptions parse(int count, const char* const* args) noexcept {
        PreviewOptions options;
        bool positioned = false;
        for (int index = 1; index < count; ++index) {
            const std::string_view argument{args[index]};
            if (argument == "--screenshot" && index + 1 < count) {
                ++index; // A filename is not a preview option.
            } else if (argument == "--preview-speed" || argument == "--preview-speed-limit") {
                if (++index >= count) {
                    options.error = "Preview speed requires an integer value";
                    return options;
                }
                const std::string_view value{args[index]};
                int parsed = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                const bool limit = argument == "--preview-speed-limit";
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                    parsed < (limit ? 1 : 0) || parsed > (limit ? 150 : 400)) {
                    options.error = "Preview speed must be 0..400; speed limit must be 1..150";
                    return options;
                }
                (limit ? options.preview_speed_limit : options.preview_speed) = parsed;
            } else if (argument == "--rgb") {
                options.rgb = true;
            } else if (argument == "--rgb-viewport") {
                options.rgb = true;
                options.viewport = true;
            } else if (argument == "--rgb-diagnostics") {
                options.rgb = true;
                options.diagnostics = true;
            } else if (argument == "--diagnostic-pattern") {
                if (++index >= count) {
                    options.error = "--diagnostic-pattern requires a pattern name";
                    return options;
                }
                const auto pattern = platform::RgbDiagnostics::parse(args[index]);
                if (!pattern) {
                    options.error = "Pattern must be red, green, blue, white, black, checkerboard, corners or viewport";
                    return options;
                }
                options.diagnostic_pattern = *pattern;
                options.explicit_pattern = true;
            } else if (argument == "--origin-x" || argument == "--origin-y") {
                if (++index >= count) {
                    options.error = "--origin-x/--origin-y requires an integer value";
                    return options;
                }
                const std::string_view value{args[index]};
                int parsed = 0;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
                    options.error = "--origin-x/--origin-y requires an integer value";
                    return options;
                }
                (argument == "--origin-x" ? options.origin.x : options.origin.y) = parsed;
                positioned = true;
            }
        }
        if (options.full_road_scene() && !positioned) options.origin = {0, 0};
        if (options.explicit_pattern && !options.diagnostics) {
            options.error = "--diagnostic-pattern requires --rgb-diagnostics";
        } else if (positioned && !options.rgb) {
            options.error = "--origin-x/--origin-y requires --rgb";
        } else if (!platform::RgbFrame::valid(options.origin, options.surface())) {
            options.error = options.full_road_scene()
                ? "Full road scene uses origin (0,0); use --rgb-viewport for movable 536x240 output"
                : "RGB origin must be X=0..264 and Y=0..240";
        }
        return options;
    }
};

}  // namespace tesla_headup::simulator
