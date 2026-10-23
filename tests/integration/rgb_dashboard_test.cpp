#include "test.hpp"
#include "tesla_headup/adapters/lvgl_rgb/rgb_frame.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "lvgl.h"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
using tesla_headup::platform::RgbFrame;

struct Capture {
    tesla_headup::platform::RgbSurface surface{};
    std::vector<std::uint16_t> pixels;
    explicit Capture(tesla_headup::platform::RgbSurface size = {}) : surface(size), pixels(size.pixels()) {}
    bool valid{true};
    unsigned flushes{0};
    static void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* data) {
        auto& self = *static_cast<Capture*>(lv_display_get_user_data(display));
        const auto bytes = static_cast<std::size_t>(area->x2 - area->x1 + 1) *
                           (area->y2 - area->y1 + 1) * 2U;
        self.valid = self.valid && RgbFrame::update(self.pixels,
            area->x1, area->y1, area->x2, area->y2, {data, bytes}, self.surface);
        ++self.flushes;
        lv_display_flush_ready(display);
    }
};

std::vector<std::uint16_t> render(lv_color_format_t format, tesla_headup::platform::RgbSurface size = {},
                                bool autopilot = true, std::uint32_t elapsed_ms = 0) {
    lv_init();
    lv_display_t* display = lv_display_create(size.width, size.height);
    CHECK(display);
    Capture capture{size};
    alignas(4) std::array<std::uint16_t, 800 * 40> buffer{};
    lv_display_set_color_format(display, format);
    lv_display_set_user_data(display, &capture);
    lv_display_set_flush_cb(display, Capture::flush);
    lv_display_set_buffers(display, buffer.data(), nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*display));
    using namespace tesla_headup::application;
    canvas.begin_frame();
    canvas.draw_text({FeatureId::VehicleSpeed, "135", TextRole::PrimaryValue});
    canvas.draw_text({FeatureId::VehicleSpeed, "km/h", TextRole::Unit});
    canvas.draw_text({FeatureId::SelectedGear, "D", TextRole::Gear});
    canvas.draw_text({.feature = FeatureId::SpeedLimit, .text = "50",
                      .role = TextRole::SpeedLimit,
                      .horizontal_offset = 220, .vertical_offset = 45});
    canvas.draw_symbol({FeatureId::TurnSignals, Symbol::LeftArrow, SymbolRole::TurnSignal,
                       Anchor::Center, -175, -58, 32, 30, true, true});
    if (autopilot)
        canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot, SymbolRole::Autopilot,
                           Anchor::Center, -175, 60, 32, 30, true, true});
    canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Left,
                              GradientRole::BlindSpotWarning, 40});
    canvas.draw_power({250});
    canvas.end_frame();
    if (elapsed_ms != 0) {
        lv_tick_inc(elapsed_ms);
        (void)lv_timer_handler();
    }
    lv_refr_now(display);
    CHECK(capture.valid);
    CHECK(capture.flushes > 1); // Exercise real LVGL partial refresh assembly.
    CHECK(std::count_if(capture.pixels.begin(), capture.pixels.end(),
                        [](auto pixel) { return pixel != 0; }) > 1000);
    canvas.shutdown();
    lv_display_delete(display);
    lv_deinit();
    return capture.pixels;
}

void preview(const std::vector<std::uint16_t>& pixels, const char* name) {
    const char* directory = std::getenv("TESLA_HEADUP_RGB_PREVIEW_DIR");
    if (!directory) {
        return;
    }
    std::filesystem::create_directories(directory);
    std::ofstream image(std::filesystem::path{directory} / name, std::ios::binary);
    image << "P6\n800 480\n255\n";
    for (auto p : pixels) {
        const std::array<unsigned char, 3> rgb{
            static_cast<unsigned char>(((p >> 11) & 31U) * 255U / 31U),
            static_cast<unsigned char>(((p >> 5) & 63U) * 255U / 63U),
            static_cast<unsigned char>((p & 31U) * 255U / 31U)};
        image.write(reinterpret_cast<const char*>(rgb.data()), rgb.size());
    }
    CHECK(image.good());
}

TEST_CASE("Newhaven native RGB565 matches the existing swapped renderer colors") {
    const auto native = render(LV_COLOR_FORMAT_RGB565);
    auto swapped = render(LV_COLOR_FORMAT_RGB565_SWAPPED);
    for (auto& pixel : swapped) {
        pixel = static_cast<std::uint16_t>((pixel >> 8) | (pixel << 8));
    }
    CHECK(native == swapped);
}

TEST_CASE("real dashboard fits RGB output and is mirrored exactly once") {
    const auto logical = render(LV_COLOR_FORMAT_RGB565);
    std::vector<std::uint16_t> direct(RgbFrame::pixels);
    std::vector<std::uint16_t> mirrored(RgbFrame::pixels);
    CHECK(RgbFrame::compose(logical, direct, {132, 0}, false));
    CHECK(RgbFrame::compose(logical, mirrored, {132, 0}, true));
    for (int y = 0; y < 480; ++y) {
        for (int x = 0; x < 800; ++x) {
            CHECK_EQ(direct[y * 800 + x], mirrored[y * 800 + 799 - x]);
            if (x < 132 || x >= 668 || y >= 240) {
                CHECK_EQ(direct[y * 800 + x], 0);
            }
        }
    }
    preview(direct, "rgb-direct.ppm");
    preview(mirrored, "rgb-mirrored.ppm");
}

#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
TEST_CASE("native road scene renders the full 800x480 surface and mirrors only at output") {
    const tesla_headup::platform::RgbSurface surface{800, 480};
    const auto logical = render(LV_COLOR_FORMAT_RGB565, surface);
    auto swapped = render(LV_COLOR_FORMAT_RGB565_SWAPPED, surface);
    for (auto& pixel : swapped) pixel = static_cast<std::uint16_t>((pixel >> 8) | (pixel << 8));
    CHECK(logical == swapped);
    std::vector<std::uint16_t> direct(RgbFrame::pixels), mirrored(RgbFrame::pixels);
    CHECK(RgbFrame::compose(logical, direct, {0, 0}, false, surface));
    CHECK(RgbFrame::compose(logical, mirrored, {0, 0}, true, surface));
    CHECK(direct == logical);
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 800; ++x)
            CHECK_EQ(direct[y * 800 + x], mirrored[y * 800 + 799 - x]);
    CHECK(direct[30 * 800 + 728] != 0); // Top-right speed-limit border.
    int speed_top = 384, speed_bottom = 0;
    for (int y = 24; y < 150; ++y)
        for (int x = 34; x < 140; ++x)
            if (direct[y * 800 + x] != 0) {
                speed_top = std::min(speed_top, y);
                speed_bottom = y;
            }
    CHECK(speed_top >= 24 && speed_top <= 26);
    CHECK(speed_bottom >= 24 + tesla_headup_inter_regular_110.line_height - 4);
    CHECK(speed_bottom < 24 + tesla_headup_inter_regular_110.line_height);
    for (int y = 188; y < 196; ++y)
        for (int x = 15; x < 30; ++x)
            CHECK_EQ(direct[y * 800 + x], 0); // No horizontal rail marker.
    int badge_left = 800, badge_right = -1, badge_top = 480, badge_bottom = -1;
    for (int y = 408; y < 480; ++y)
        for (int x = 352; x < 448; ++x)
            if (direct[y * 800 + x] != 0) {
                badge_left = std::min(badge_left, x); badge_right = std::max(badge_right, x);
                badge_top = std::min(badge_top, y); badge_bottom = std::max(badge_bottom, y);
            }
    CHECK_EQ(badge_right - badge_left + 1, 67);
    CHECK_EQ(badge_bottom - badge_top + 1, 66);
    // No road marking may cross into the transparent footer.
    for (int y = 408; y < 480; ++y)
        for (int x = 280; x < 520; ++x)
            if (x < 352 || x >= 448 || y < 408 || y >= 480)
                CHECK_EQ(direct[y * 800 + x], 0);
}

TEST_CASE("native road animation stays above the status bar without Autopilot telemetry") {
    for (const auto elapsed : {0U, 100U, 500U, 1000U}) {
        const auto pixels = render(LV_COLOR_FORMAT_RGB565, {800, 480}, false, elapsed);
        for (int y = 408; y < 480; ++y)
            for (int x = 280; x < 520; ++x)
                CHECK_EQ(pixels[y * 800 + x], 0);
    }
}
#endif
}  // namespace
