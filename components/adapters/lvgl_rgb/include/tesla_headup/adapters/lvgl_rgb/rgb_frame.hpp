#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace tesla_headup::platform {

// CPU-side RGB565 composition, independent of ESP-IDF and LVGL. Coordinates
// refer to the unmirrored panel; projection flips the entire physical output.
struct RgbPlacement {
    int x{132};
    int y{0};
};

struct RgbSurface {
    int width{536};
    int height{240};
    [[nodiscard]] constexpr std::size_t pixels() const noexcept {
        return static_cast<std::size_t>(width) * height;
    }
};

class RgbFrame final {
public:
    static constexpr int width = 800;
    static constexpr int height = 480;
    static constexpr int dashboard_width = 536;
    static constexpr int dashboard_height = 240;
    static constexpr std::size_t pixels = width * height;
    static constexpr std::size_t dashboard_pixels = dashboard_width * dashboard_height;

    [[nodiscard]] static constexpr bool valid(RgbPlacement origin, RgbSurface surface = {}) noexcept {
        return surface.width > 0 && surface.width <= width &&
               surface.height > 0 && surface.height <= height &&
               origin.x >= 0 && origin.x <= width - surface.width &&
               origin.y >= 0 && origin.y <= height - surface.height;
    }

    // Inclusive LVGL rectangle. Reject malformed/out-of-range areas atomically;
    // never trust a callback's dimensions to index a buffer unchecked.
    [[nodiscard]] static bool update(
        std::span<std::uint16_t> dashboard,
        int x1, int y1, int x2, int y2,
        std::span<const std::uint8_t> rgb565, RgbSurface surface = {}) noexcept {
        if (!valid({0, 0}, surface) || dashboard.size() != surface.pixels() || x1 < 0 || y1 < 0 ||
            x2 < x1 || y2 < y1 || x2 >= surface.width || y2 >= surface.height) {
            return false;
        }
        const auto row_bytes = static_cast<std::size_t>(x2 - x1 + 1) * 2U;
        if (rgb565.size() != row_bytes * static_cast<std::size_t>(y2 - y1 + 1)) {
            return false;
        }
        for (int y = y1; y <= y2; ++y) {
            std::memcpy(dashboard.data() + y * surface.width + x1,
                        rgb565.data() + (y - y1) * row_bytes, row_bytes);
        }
        return true;
    }

    [[nodiscard]] static bool compose(
        std::span<const std::uint16_t> dashboard,
        std::span<std::uint16_t> output,
        RgbPlacement origin, bool mirrored, RgbSurface surface = {}) noexcept {
        if (!valid(origin, surface) || dashboard.size() != surface.pixels() ||
            output.size() != pixels || dashboard.data() == output.data()) {
            return false;
        }
        std::fill(output.begin(), output.end(), std::uint16_t{0});
        const int start_x = mirrored ? width - origin.x - surface.width : origin.x;
        for (int y = 0; y < surface.height; ++y) {
            const auto* source = dashboard.data() + y * surface.width;
            auto* destination = output.data() + (origin.y + y) * width + start_x;
            if (mirrored) {
                std::reverse_copy(source, source + surface.width, destination);
            } else {
                std::copy_n(source, surface.width, destination);
            }
        }
        return true;
    }
};

// The IDF bounce-buffer callback occurs after the driver latches cur_fb_index
// for its next frame. Snapshot the counter AFTER draw_bitmap returns, then
// wait for TWO later callbacks before recycling the previous framebuffer.
// A callback already in flight on the other core might have latched the old
// index before submission but increment its counter after our snapshot.
class RgbFrameExchange final {
public:
    [[nodiscard]] bool pending() const noexcept { return pending_; }
    [[nodiscard]] unsigned writable_index() const noexcept { return 1U - front_; }
    void submitted(std::uint32_t completed_frames) noexcept {
        submitted_at_ = completed_frames;
        pending_ = true;
    }
    [[nodiscard]] bool retire(std::uint32_t completed_frames) noexcept {
        if (!pending_ || static_cast<std::uint32_t>(completed_frames - submitted_at_) < 2U) {
            return false;
        }
        front_ = 1U - front_;
        pending_ = false;
        return true;
    }

private:
    unsigned front_{0};
    std::uint32_t submitted_at_{0};
    bool pending_{false};
};

}  // namespace tesla_headup::platform
