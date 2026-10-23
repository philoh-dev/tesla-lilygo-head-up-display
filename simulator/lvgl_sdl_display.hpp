#pragma once

#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "preview_options.hpp"

#include <SDL.h>
#include <lvgl.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace tesla_headup::simulator {

class LvglSdlDisplay final {
public:
    static constexpr int width = 536;
    static constexpr int height = 240;

    explicit LvglSdlDisplay(SDL_Renderer& renderer, PreviewOptions options = {});
    ~LvglSdlDisplay();

    LvglSdlDisplay(const LvglSdlDisplay&) = delete;
    LvglSdlDisplay& operator=(const LvglSdlDisplay&) = delete;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] platform::LvglDashboardCanvas& canvas() noexcept;

    void set_projection_mirrored(bool mirrored) noexcept;
    [[nodiscard]] bool projection_mirrored() const noexcept;
    [[nodiscard]] bool rgb_output() const noexcept { return options_.rgb; }
    [[nodiscard]] platform::RgbPlacement dashboard_position() const noexcept { return options_.origin; }
    [[nodiscard]] bool set_dashboard_position(int x, int y) noexcept;
    void move_dashboard(int dx, int dy) noexcept;
    [[nodiscard]] bool save_screenshot(const char* path) noexcept;
    [[nodiscard]] bool set_diagnostic_pattern(platform::RgbDiagnosticPattern pattern) noexcept;

    void refresh() noexcept;
    void process() noexcept;

private:
    static constexpr int native_width = 240;
    static constexpr int native_height = 536;
    static constexpr int buffer_lines = 40;
    static constexpr std::size_t bytes_per_pixel = 2;
    static constexpr std::size_t draw_buffer_size =
        static_cast<std::size_t>(width) * buffer_lines * bytes_per_pixel;

    static void flush(
        lv_display_t* display,
        const lv_area_t* area,
        std::uint8_t* pixels) noexcept;

    void receive(
        const lv_area_t& area,
        const std::uint8_t* pixels) noexcept;
    void present() noexcept;

    SDL_Renderer& renderer_;
    PreviewOptions options_;
    SDL_Texture* texture_{nullptr};
    lv_display_t* display_{nullptr};
    platform::LvglDashboardCanvas canvas_{};
    std::array<std::uint8_t, 800 * buffer_lines * bytes_per_pixel> draw_buffer_a_{};
    std::array<std::uint8_t, draw_buffer_size> draw_buffer_b_{};
    std::vector<std::uint16_t> panel_ram_;
    std::vector<std::uint16_t> presented_frame_;
    bool projection_mirrored_{true};
    bool frame_dirty_{false};
};

}  // namespace tesla_headup::simulator
