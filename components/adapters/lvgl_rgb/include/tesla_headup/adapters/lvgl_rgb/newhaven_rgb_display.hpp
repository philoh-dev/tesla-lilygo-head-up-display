#pragma once

#include "sdkconfig.h"
#if CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
#include "tesla_headup/adapters/lvgl_rgb/rgb_diagnostics.hpp"
#endif

#include "tesla_headup/adapters/lvgl_rgb/rgb_frame.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "esp_err.h"
#include "lvgl.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include <cstdint>

namespace tesla_headup::platform {

struct RgbCompletionCounter {
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    std::uint32_t value{0};
    [[nodiscard]] std::uint32_t load() noexcept;
};

class NewhavenRgbDisplay final {
public:
    NewhavenRgbDisplay() noexcept;
    ~NewhavenRgbDisplay();
    NewhavenRgbDisplay(const NewhavenRgbDisplay&) = delete;
    NewhavenRgbDisplay& operator=(const NewhavenRgbDisplay&) = delete;

    [[nodiscard]] esp_err_t initialize() noexcept;
    void shutdown() noexcept;
    void process() noexcept;
    void set_projection_mirrored(bool mirrored) noexcept;
    [[nodiscard]] bool projection_mirrored() const noexcept { return mirrored_; }
    [[nodiscard]] esp_err_t set_dashboard_position(int x, int y) noexcept;
    [[nodiscard]] RgbPlacement dashboard_position() const noexcept { return origin_; }
    [[nodiscard]] esp_err_t set_brightness_percent(std::uint8_t percent) noexcept;
    [[nodiscard]] std::uint8_t brightness_percent() const noexcept { return brightness_; }
    [[nodiscard]] bool ready() const noexcept { return initialized_ && !failed_; }
    [[nodiscard]] LvglDashboardCanvas& canvas() noexcept { return canvas_; }
#if CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
    void set_diagnostic_pattern(RgbDiagnosticPattern pattern) noexcept {
        if (RgbDiagnostics::valid(pattern)) { diagnostic_pattern_ = pattern; dirty_ = true; }
    }
#endif

private:
#if CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    static constexpr RgbSurface surface_{800, 480};
#else
    static constexpr RgbSurface surface_{};
#endif
    static void flush(lv_display_t*, const lv_area_t*, std::uint8_t*);
    [[nodiscard]] esp_err_t initialize_resources() noexcept;
    [[nodiscard]] esp_err_t apply_brightness(std::uint8_t percent) noexcept;
    void fail(esp_err_t error) noexcept;

    void* panel_{nullptr};
    lv_display_t* display_{nullptr};
    std::uint16_t* framebuffers_[2]{};
    std::uint16_t* dashboard_{nullptr};
    void* draw_buffer_{nullptr};
    LvglDashboardCanvas canvas_{};
    RgbFrameExchange exchange_{};
    RgbCompletionCounter completed_frames_{};
    RgbPlacement origin_{};
#if CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS
    RgbDiagnosticPattern diagnostic_pattern_{RgbDiagnosticPattern::Red};
#endif
    std::uint8_t brightness_{100};
    bool mirrored_{true};
    bool initialized_{false};
    bool lvgl_initialized_{false};
    bool pwm_initialized_{false};
    bool lit_{false};
    bool dirty_{false};
    bool failed_{false};
    std::int64_t submitted_us_{0};
    std::int64_t report_us_{0};
    std::uint32_t presented_{0};
    std::uint32_t last_completed_{0};
};

}  // namespace tesla_headup::platform
