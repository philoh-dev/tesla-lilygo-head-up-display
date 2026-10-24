#pragma once

#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"

#include "esp_err.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace tesla_headup::platform {

class Rm67162Display final {
public:
    static constexpr int logical_width = 536;
    static constexpr int logical_height = 240;

    [[nodiscard]] esp_err_t initialize() noexcept;
    void process() noexcept;

    void set_projection_mirrored(bool mirrored) noexcept;
    [[nodiscard]] bool projection_mirrored() const noexcept;
    [[nodiscard]] esp_err_t set_brightness_percent(
        std::uint8_t percent) noexcept;
    [[nodiscard]] std::uint8_t brightness_percent() const noexcept;
    [[nodiscard]] bool ready() const noexcept;

    [[nodiscard]] LvglDashboardCanvas& canvas() noexcept;

private:
    static void flush(
        lv_display_t* display,
        const lv_area_t* area,
        std::uint8_t* pixels);
    static void performance_event(lv_event_t* event);
    [[nodiscard]] esp_err_t write_command(
        std::uint16_t address,
        const void* parameters = nullptr,
        std::size_t parameter_size = 0) noexcept;
    [[nodiscard]] esp_err_t write_pixels(
        const lv_area_t& area,
        const std::uint8_t* pixels) noexcept;
    void report_performance() noexcept;

    // Keep ESP-IDF's C-only SPI declarations out of this public C++ header.
    // The concrete handle is converted back in the implementation.
    void* spi_device_{nullptr};
    lv_display_t* lvgl_display_{nullptr};
    void* draw_buffer_a_{nullptr};
    LvglDashboardCanvas canvas_{};
    bool projection_mirrored_{false};
    bool projection_configured_{false};
    std::uint8_t brightness_percent_{100};
    std::atomic<std::uint32_t> refresh_started_at_us_{0};
    std::atomic<std::uint32_t> flush_started_at_us_{0};
    std::atomic<std::uint32_t> refresh_count_{0};
    std::atomic<std::uint32_t> refresh_total_us_{0};
    std::atomic<std::uint32_t> refresh_max_us_{0};
    std::atomic<std::uint32_t> flush_count_{0};
    std::atomic<std::uint32_t> flush_total_us_{0};
    std::atomic<std::uint32_t> flush_max_us_{0};
    std::uint64_t last_performance_report_us_{0};
};

}  // namespace tesla_headup::platform
