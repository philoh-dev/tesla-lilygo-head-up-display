#include "lvgl_sdl_display.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace tesla_headup::simulator {
namespace {

std::uint32_t lvgl_milliseconds() {
    return SDL_GetTicks();
}

}  // namespace

LvglSdlDisplay::LvglSdlDisplay(SDL_Renderer& renderer, PreviewOptions options)
    : renderer_(renderer), options_(options),
      panel_ram_(options.surface().pixels()),
      presented_frame_(static_cast<std::size_t>(options.output_width()) * options.output_height()) {
    if (options_.full_road_scene() && options_.origin.x == 132 && options_.origin.y == 0)
        options_.origin = {0, 0};
    (void)SDL_RenderSetLogicalSize(&renderer_, options_.output_width(), options_.output_height());
    lv_init();
    lv_tick_set_cb(lvgl_milliseconds);
    if (!options_.error.empty() || !platform::RgbFrame::valid(options_.origin, options_.surface()) ||
        (options_.diagnostics && !options_.rgb)) {
        return;
    }

    texture_ = SDL_CreateTexture(
        &renderer_,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        options_.output_width(),
        options_.output_height());
    if (texture_ == nullptr) {
        return;
    }
    if (options_.diagnostics) {
        frame_dirty_ = true;
        return; // Raw output deliberately bypasses the LVGL dashboard.
    }

    display_ = lv_display_create(options_.rgb ? options_.surface().width : native_width,
                                 options_.rgb ? options_.surface().height : native_height);
    if (display_ == nullptr) {
        return;
    }

    lv_display_set_color_format(display_, options_.rgb ? LV_COLOR_FORMAT_RGB565
                                                     : LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, flush);
    if (!options_.rgb) {
        lv_display_set_rotation(display_, LV_DISPLAY_ROTATION_90);
    }
    lv_display_set_buffers(
        display_,
        draw_buffer_a_.data(),
        options_.rgb ? nullptr : draw_buffer_b_.data(),
        options_.rgb ? options_.surface().width * buffer_lines * bytes_per_pixel : draw_buffer_size,
        LV_DISPLAY_RENDER_MODE_PARTIAL);
    if (!canvas_.initialize(*display_)) {
        lv_display_delete(display_);
        display_ = nullptr;
    }
}

LvglSdlDisplay::~LvglSdlDisplay() {
    canvas_.shutdown();
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
    lv_deinit();
}

bool LvglSdlDisplay::ready() const noexcept {
    return texture_ != nullptr &&
        (options_.diagnostics || (display_ != nullptr && canvas_.ready()));
}

platform::LvglDashboardCanvas& LvglSdlDisplay::canvas() noexcept {
    return canvas_;
}

void LvglSdlDisplay::set_projection_mirrored(bool mirrored) noexcept {
    projection_mirrored_ = mirrored;
    if (ready()) {
        present();
    }
}

bool LvglSdlDisplay::projection_mirrored() const noexcept {
    return projection_mirrored_;
}

bool LvglSdlDisplay::set_dashboard_position(int x, int y) noexcept {
    if (!options_.rgb || !platform::RgbFrame::valid({x, y}, options_.surface())) {
        return false;
    }
    options_.origin = {x, y};
    if (ready()) {
        present();
    }
    return true;
}

void LvglSdlDisplay::move_dashboard(int dx, int dy) noexcept {
    if (options_.rgb) {
        // Clamp deltas first so even extreme caller input cannot overflow.
        (void)set_dashboard_position(
            std::clamp(options_.origin.x + std::clamp(dx, -800, 800), 0, 800 - options_.surface().width),
            std::clamp(options_.origin.y + std::clamp(dy, -480, 480), 0, 480 - options_.surface().height));
    }
}

bool LvglSdlDisplay::save_screenshot(const char* path) noexcept {
    if (!ready() || path == nullptr) {
        return false;
    }
    // Export native output pixels, independent of window size/Retina scaling.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        presented_frame_.data(), options_.output_width(), options_.output_height(),
        16, options_.output_width() * static_cast<int>(sizeof(std::uint16_t)),
        SDL_PIXELFORMAT_RGB565);
    if (surface == nullptr) {
        return false;
    }
    const bool saved = SDL_SaveBMP(surface, path) == 0;
    SDL_FreeSurface(surface);
    return saved;
}

bool LvglSdlDisplay::set_diagnostic_pattern(platform::RgbDiagnosticPattern pattern) noexcept {
    if (!options_.diagnostics || !platform::RgbDiagnostics::valid(pattern)) return false;
    options_.diagnostic_pattern = pattern;
    frame_dirty_ = true;
    if (ready()) present();
    return true;
}

void LvglSdlDisplay::refresh() noexcept {
    if (!ready()) {
        return;
    }
    if (!options_.diagnostics) lv_refr_now(display_);
    if (frame_dirty_) {
        present();
    }
}

void LvglSdlDisplay::process() noexcept {
    if (!ready()) {
        return;
    }
    if (!options_.diagnostics) (void)lv_timer_handler();
    if (frame_dirty_) {
        present();
    }
}

void LvglSdlDisplay::flush(
    lv_display_t* display,
    const lv_area_t* area,
    std::uint8_t* pixels) noexcept {
    auto* self = static_cast<LvglSdlDisplay*>(
        lv_display_get_user_data(display));
    if (self != nullptr && area != nullptr && pixels != nullptr) {
        self->receive(*area, pixels);
        if (self->options_.rgb && lv_display_flush_is_last(display)) {
            self->frame_dirty_ = true;
        }
    }
    lv_display_flush_ready(display);
}

void LvglSdlDisplay::receive(
    const lv_area_t& area,
    const std::uint8_t* pixels) noexcept {
    if (options_.rgb) {
        if (area.x1 < 0 || area.y1 < 0 || area.x2 < area.x1 || area.y2 < area.y1 ||
            area.x2 >= options_.surface().width || area.y2 >= options_.surface().height) {
            SDL_SetError("Invalid RGB dashboard flush area");
            return;
        }
        const auto size = static_cast<std::size_t>(area.x2 - area.x1 + 1) *
                          (area.y2 - area.y1 + 1) * bytes_per_pixel;
        (void)platform::RgbFrame::update(panel_ram_, area.x1, area.y1, area.x2, area.y2,
                                        {pixels, size}, options_.surface());
        return;
    }
    const int clipped_x1 = std::max(area.x1, 0);
    const int clipped_y1 = std::max(area.y1, 0);
    const int clipped_x2 = std::min(area.x2, width - 1);
    const int clipped_y2 = std::min(area.y2, height - 1);
    const int source_width = area.x2 - area.x1 + 1;

    for (int y = clipped_y1; y <= clipped_y2; ++y) {
        for (int x = clipped_x1; x <= clipped_x2; ++x) {
            const auto source_pixel = static_cast<std::size_t>(
                (y - area.y1) * source_width + (x - area.x1));
            const auto source_byte = source_pixel * bytes_per_pixel;

            // LV_COLOR_FORMAT_RGB565_SWAPPED contains the exact two-byte
            // sequence sent over QSPI: most-significant byte first.
            const auto color = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(pixels[source_byte]) << 8U) |
                pixels[source_byte + 1]);
            panel_ram_[static_cast<std::size_t>(y * width + x)] = color;
        }
    }
    frame_dirty_ = true;
}

void LvglSdlDisplay::present() noexcept {
    if (options_.diagnostics) {
        if (!platform::RgbDiagnostics::render(presented_frame_, options_.diagnostic_pattern,
                                               options_.origin, projection_mirrored_)) return;
    } else if (options_.rgb) {
        if (!platform::RgbFrame::compose(panel_ram_, presented_frame_, options_.origin,
                                         projection_mirrored_, options_.surface())) {
            return;
        }
    } else {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int logical_x = projection_mirrored_ ? width - x - 1 : x;
                presented_frame_[static_cast<std::size_t>(y * width + x)] =
                    panel_ram_[static_cast<std::size_t>(y * width + logical_x)];
            }
        }
    }

    (void)SDL_UpdateTexture(
        texture_,
        nullptr,
        presented_frame_.data(),
        options_.output_width() * static_cast<int>(sizeof(std::uint16_t)));
    (void)SDL_SetRenderDrawColor(&renderer_, 0, 0, 0, 255);
    (void)SDL_RenderClear(&renderer_);
    (void)SDL_RenderCopy(&renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(&renderer_);
    frame_dirty_ = false;
}

}  // namespace tesla_headup::simulator
