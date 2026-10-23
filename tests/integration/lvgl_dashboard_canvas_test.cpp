#include "test.hpp"

#include "tesla_headup/adapters/lvgl_rm67162/indicator_images.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"

#include "lvgl.h"
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/hud_runtime/power_presenter.hpp"
#endif

#include <algorithm>
#include <array>
#include <vector>
#include <cstdint>
#include <string_view>

namespace {

class LvglSession final {
public:
    explicit LvglSession(std::int32_t width = 536, std::int32_t height = 240) {
        lv_init();
        display_ = lv_display_create(width, height);
    }

    ~LvglSession() {
        if (display_ != nullptr) {
            lv_display_delete(display_);
        }
        lv_deinit();
    }

    LvglSession(const LvglSession&) = delete;
    LvglSession& operator=(const LvglSession&) = delete;

    [[nodiscard]] lv_display_t* display() const noexcept {
        return display_;
    }

private:
    lv_display_t* display_{nullptr};
};

struct ExpectedImage {
    tesla_headup::application::Symbol symbol;
    const lv_image_dsc_t* descriptor;
    std::int16_t horizontal_offset;
};

constexpr std::array expected_images{
    ExpectedImage{tesla_headup::application::Symbol::LeftArrow,
                  &tesla_headup::platform::assets::turn_left_image, -175},
    ExpectedImage{tesla_headup::application::Symbol::RightArrow,
                  &tesla_headup::platform::assets::turn_right_image, 175},
    ExpectedImage{tesla_headup::application::Symbol::PositionLights,
                  &tesla_headup::platform::assets::position_lights_image, -84},
    ExpectedImage{tesla_headup::application::Symbol::LowBeam,
                  &tesla_headup::platform::assets::low_beam_image, -42},
    ExpectedImage{tesla_headup::application::Symbol::HighBeam,
                  &tesla_headup::platform::assets::high_beam_image, 0},
    ExpectedImage{tesla_headup::application::Symbol::AdaptiveHighBeam,
                  &tesla_headup::platform::assets::adaptive_high_beam_image, 0},
    ExpectedImage{tesla_headup::application::Symbol::FrontFogLight,
                  &tesla_headup::platform::assets::front_fog_image, 42},
    ExpectedImage{tesla_headup::application::Symbol::RearFogLight,
                  &tesla_headup::platform::assets::rear_fog_image, 74},
    ExpectedImage{tesla_headup::application::Symbol::Autopilot,
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
                  &tesla_headup::platform::assets::autopilot_road_scene_image, -175},
#else
                  &tesla_headup::platform::assets::autopilot_image, -175},
#endif
};

lv_obj_t* dashboard_composition(lv_display_t* display) {
    return lv_obj_get_child(lv_display_get_screen_active(display), 0);
}

lv_obj_t* dashboard_content(lv_display_t* display) {
    auto* composition = dashboard_composition(display);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    return lv_obj_get_child(composition, 0);
#else
    return composition;
#endif
}

lv_obj_t* dashboard_label(lv_obj_t* driving, std::int32_t slot) {
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    if (slot == 2 || slot == 3) {
        auto* footer = lv_obj_get_child(lv_obj_get_parent(driving), 1);
        return lv_obj_get_child_by_type(footer, slot - 2, &lv_label_class);
    }
    if (slot == 4) slot = 2;
#endif
    return lv_obj_get_child_by_type(driving, slot, &lv_label_class);
}

template <std::int32_t Width, std::int32_t Height>
struct PixelCapture {
    static constexpr std::int32_t width = Width;
    static constexpr std::int32_t height = Height;
    std::array<std::uint16_t, width * height> pixels{};
    alignas(4) std::array<std::uint16_t, width * 4> draw_buffer{};

    static void flush(lv_display_t* display, const lv_area_t* area,
                      std::uint8_t* data) {
        auto& capture = *static_cast<PixelCapture*>(
            lv_display_get_user_data(display));
        const auto* source = reinterpret_cast<const std::uint16_t*>(data);
        const auto stride = area->x2 - area->x1 + 1;
        for (auto y = area->y1; y <= area->y2; ++y) {
            for (auto x = area->x1; x <= area->x2; ++x) {
                capture.pixels[static_cast<std::size_t>(y * width + x)] =
                    source[(y - area->y1) * stride + x - area->x1];
            }
        }
        lv_display_flush_ready(display);
    }
};

using OrbitPixelCapture = PixelCapture<12, 8>;

}  // namespace

#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
static int painted_bottom(lv_obj_t* label) {
    const auto* font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    int bottom = 0;
    for (const unsigned char c : std::string_view{lv_label_get_text(label)}) {
        lv_font_glyph_dsc_t glyph{};
        if (lv_font_get_glyph_dsc(font, &glyph, c, 0) && glyph.box_h != 0)
            bottom = std::max(bottom, font->line_height - font->base_line - glyph.ofs_y);
    }
    return lv_obj_get_y(label) + bottom;
}

TEST_CASE("Inter digits are tabular across all dashboard fonts") {
    for (const auto* font : {&tesla_headup_inter_regular_64, &tesla_headup_inter_regular_110,
                            &tesla_headup_inter_medium_16, &tesla_headup_inter_medium_18, &tesla_headup_inter_medium_24,
                            &tesla_headup_inter_semibold_24, &tesla_headup_inter_semibold_40}) {
        lv_font_glyph_dsc_t first{};
        CHECK(lv_font_get_glyph_dsc(font,&first,'0',0));
        for (std::uint32_t digit = '1'; digit <= '9'; ++digit) {
            lv_font_glyph_dsc_t glyph{};
            CHECK(lv_font_get_glyph_dsc(font,&glyph,digit,0));
            CHECK_EQ(glyph.adv_w,first.adv_w);
        }
    }
}

#endif

TEST_CASE("dashboard pixel orbit is bounded periodic and independent of rendering") {
    LvglSession session;
    CHECK(session.display() != nullptr);
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* content = dashboard_composition(session.display());
    CHECK(content != nullptr);
    lv_obj_update_layout(content);
    CHECK_EQ(lv_obj_get_x(content), 0);
    CHECK_EQ(lv_obj_get_y(content), 0);
    const auto count = lv_obj_get_child_count(content);
    constexpr auto interval = tesla_headup::platform::LvglPixelOrbit::interval_ms;
    CHECK(interval >= 3U * 60U * 1000U);
    CHECK(interval <= 5U * 60U * 1000U);
    lv_tick_inc(interval - 1U);
    (void)lv_timer_handler();
    CHECK_EQ(lv_obj_get_x(content), 0);
    CHECK_EQ(lv_obj_get_y(content), 0);

    constexpr std::array<std::array<std::int32_t, 2>, 8> expected{{
        {1, 1}, {0, 2}, {-1, 1}, {-2, 0},
        {-1, -1}, {0, -2}, {1, -1}, {2, 0},
    }};
    for (std::size_t step = 0; step < 24; ++step) {
        lv_tick_inc(step == 0 ? 1U : interval);
        (void)lv_timer_handler();
        lv_obj_update_layout(content);
        const auto position = expected[step % expected.size()];
        CHECK_EQ(lv_obj_get_x(content), position[0]);
        CHECK_EQ(lv_obj_get_y(content), position[1]);
        CHECK(lv_obj_get_x(content) >= -2 && lv_obj_get_x(content) <= 2);
        CHECK(lv_obj_get_y(content) >= -2 && lv_obj_get_y(content) <= 2);
        // Fresh dashboard frames must not reset the persistent display offset.
        canvas.begin_frame();
        canvas.end_frame();
        CHECK_EQ(lv_obj_get_x(content), position[0]);
        CHECK_EQ(lv_obj_get_y(content), position[1]);
        CHECK_EQ(lv_obj_get_child_count(content), count);
    }
}

TEST_CASE("pixel orbit survives tick wrap and releases its timer and container") {
    LvglSession session;
    lv_obj_t* screen = lv_display_get_screen_active(session.display());
    const auto children = lv_obj_get_child_count(screen);
    lv_tick_inc(UINT32_MAX - 1000U);
    tesla_headup::platform::LvglPixelOrbit orbit;
    lv_obj_t* content = orbit.initialize(*session.display());
    CHECK(content != nullptr);
    CHECK_EQ(orbit.initialize(*session.display()), content);
    CHECK_EQ(lv_obj_get_child_count(screen), children + 1U);
    lv_tick_inc(tesla_headup::platform::LvglPixelOrbit::interval_ms);
    (void)lv_timer_handler();
    lv_obj_update_layout(content);
    CHECK_EQ(lv_obj_get_x(content), 1);
    CHECK_EQ(lv_obj_get_y(content), 1);
    orbit.shutdown();
    CHECK_EQ(lv_obj_get_child_count(screen), children);
    lv_tick_inc(tesla_headup::platform::LvglPixelOrbit::interval_ms);
    (void)lv_timer_handler();
    // Reinitialization starts centered and waits a complete interval again.
    content = orbit.initialize(*session.display());
    CHECK(content != nullptr);
    lv_obj_update_layout(content);
    CHECK_EQ(lv_obj_get_x(content), 0);
    CHECK_EQ(lv_obj_get_y(content), 0);
    lv_tick_inc(tesla_headup::platform::LvglPixelOrbit::interval_ms - 1U);
    (void)lv_timer_handler();
    CHECK_EQ(lv_obj_get_x(content), 0);
    orbit.shutdown();
    orbit.shutdown();
}

TEST_CASE("pixel orbit repaints exposed edges black without stale pixels") {
    LvglSession session{OrbitPixelCapture::width, OrbitPixelCapture::height};
    OrbitPixelCapture capture;
    lv_display_set_color_format(session.display(), LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(session.display(), &capture);
    lv_display_set_flush_cb(session.display(), OrbitPixelCapture::flush);
    lv_display_set_buffers(session.display(), capture.draw_buffer.data(), nullptr,
                           sizeof(capture.draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    tesla_headup::platform::LvglPixelOrbit orbit;
    lv_obj_t* content = orbit.initialize(*session.display());
    CHECK(content != nullptr);
    lv_obj_t* white = lv_obj_create(content);
    CHECK(white != nullptr);
    lv_obj_remove_style_all(white);
    lv_obj_set_pos(white, 0, 0);
    lv_obj_set_size(white, OrbitPixelCapture::width, OrbitPixelCapture::height);
    lv_obj_set_style_bg_color(white, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(white, LV_OPA_COVER, LV_PART_MAIN);
    for (std::size_t step = 0; step < 17; ++step) {
        if (step != 0) {
            lv_tick_inc(tesla_headup::platform::LvglPixelOrbit::interval_ms);
            (void)lv_timer_handler();
        }
        lv_refr_now(session.display());
        const auto dx = lv_obj_get_x(content);
        const auto dy = lv_obj_get_y(content);
        for (std::int32_t y = 0; y < OrbitPixelCapture::height; ++y) {
            for (std::int32_t x = 0; x < OrbitPixelCapture::width; ++x) {
                const bool covered = x >= dx && y >= dy &&
                    x < dx + OrbitPixelCapture::width &&
                    y < dy + OrbitPixelCapture::height;
                CHECK_EQ(capture.pixels[static_cast<std::size_t>(
                             y * OrbitPixelCapture::width + x)],
                         covered ? 0xFFFFU : 0U);
            }
        }
    }
}

TEST_CASE("orbital shift keeps static indicators on panel and alignment stable") {
    LvglSession session;
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* content = dashboard_content(session.display());
    std::array<lv_area_t, 5> previous{};
    for (std::size_t step = 0; step < 9; ++step) {
        if (step != 0) {
            lv_tick_inc(tesla_headup::platform::LvglPixelOrbit::interval_ms);
            (void)lv_timer_handler();
        }
        canvas.begin_frame();
        canvas.draw_text({.text = "135",
                          .role = tesla_headup::application::TextRole::PrimaryValue,
                          .vertical_offset = -24});
        canvas.draw_text({.text = "km/h",
                          .role = tesla_headup::application::TextRole::Unit,
                          .vertical_offset = 51});
        canvas.draw_text({.text = "RECONNECTING",
                          .role = tesla_headup::application::TextRole::StatusWarning,
                          .vertical_offset = -104});
        canvas.draw_text({.text = "D",
                          .role = tesla_headup::application::TextRole::Gear});
        canvas.draw_text({.text = "50",
                          .role = tesla_headup::application::TextRole::SpeedLimit,
                          .horizontal_offset = 220,
                          .vertical_offset = 45});
        canvas.end_frame();
        lv_obj_update_layout(content);
        const auto offset_x = lv_obj_get_x(dashboard_composition(session.display()));
        const auto offset_y = lv_obj_get_y(dashboard_composition(session.display()));
        for (std::int32_t index = 0; index < 5; ++index) {
            lv_obj_t* label = dashboard_label(content, index);
            CHECK(label != nullptr);
            lv_area_t bounds{};
            lv_obj_get_coords(label, &bounds);
            CHECK(bounds.x1 >= 0 && bounds.y1 >= 0);
            CHECK(bounds.x2 < 536 && bounds.y2 < 240);
            const auto local = lv_area_t{bounds.x1 - offset_x, bounds.y1 - offset_y,
                                         bounds.x2 - offset_x, bounds.y2 - offset_y};
            if (step == 0) {
                previous[static_cast<std::size_t>(index)] = local;
            } else {
                const auto expected = previous[static_cast<std::size_t>(index)];
                CHECK_EQ(local.x1, expected.x1);
                CHECK_EQ(local.y1, expected.y1);
                CHECK_EQ(local.x2, expected.x2);
                CHECK_EQ(local.y2, expected.y2);
            }
        }
    }
}

TEST_CASE("LVGL canvas reuses persistent image objects for dashboard symbols") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    constexpr std::uint32_t expected_image_count = 13U;
#else
    constexpr std::uint32_t expected_image_count = 10U;
#endif
    CHECK_EQ(
        lv_obj_get_child_count_by_type(root, &lv_image_class),
        expected_image_count);

    canvas.begin_frame();
    for (const ExpectedImage& expected : expected_images) {
        canvas.draw_symbol(tesla_headup::application::SymbolElement{
            .symbol = expected.symbol,
            .horizontal_offset = expected.horizontal_offset,
        });
    }

    for (std::size_t index = 0; index < expected_images.size(); ++index) {
        lv_obj_t* image = lv_obj_get_child_by_type(
            root, static_cast<std::int32_t>(index), &lv_image_class);
        CHECK(image != nullptr);
        CHECK(!lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN));
        CHECK_EQ(lv_image_get_src(image), expected_images[index].descriptor);
        CHECK_EQ(
            lv_image_get_src_width(image),
            static_cast<std::int32_t>(expected_images[index].descriptor->header.w));
        CHECK_EQ(
            lv_image_get_src_height(image),
            static_cast<std::int32_t>(expected_images[index].descriptor->header.h));
    }

    canvas.begin_frame();
    CHECK_EQ(
        lv_obj_get_child_count_by_type(root, &lv_image_class),
        expected_image_count);
    for (std::size_t index = 0; index < 10; ++index) {
        lv_obj_t* image = lv_obj_get_child_by_type(
            root, static_cast<std::int32_t>(index), &lv_image_class);
        CHECK(image != nullptr);
        CHECK(lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN));
    }
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    lv_obj_t* vehicle = lv_obj_get_child_by_type(root, 12, &lv_image_class);
    CHECK(vehicle != nullptr);
    CHECK(!lv_obj_has_flag(vehicle, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(
        lv_image_get_src(vehicle),
        &tesla_headup::platform::assets::road_scene_vehicle_image);
#endif
}

TEST_CASE("LVGL turn arrows alternate bright and dim without hiding") {
    LvglSession session;
    CHECK(session.display() != nullptr);
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    for (const bool illuminated : {false, true, false}) {
        canvas.begin_frame();
        for (const auto symbol : {tesla_headup::application::Symbol::LeftArrow,
                                  tesla_headup::application::Symbol::RightArrow}) {
            canvas.draw_symbol(tesla_headup::application::SymbolElement{
                .symbol = symbol,
                .role = tesla_headup::application::SymbolRole::TurnSignal,
                .illuminated = illuminated,
            });
        }
        for (std::int32_t index = 0; index < 2; ++index) {
            lv_obj_t* image =
                lv_obj_get_child_by_type(root, index, &lv_image_class);
            CHECK(image != nullptr);
            CHECK(!lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN));
            CHECK_EQ(lv_obj_get_style_image_opa(image, LV_PART_MAIN),
                     illuminated ? LV_OPA_COVER : LV_OPA_50);
        }
    }
    // Reusing a dim arrow's slot must not dim a different dashboard symbol.
    canvas.begin_frame();
    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .symbol = tesla_headup::application::Symbol::LowBeam,
        .role = tesla_headup::application::SymbolRole::ExteriorLight,
    });
    lv_obj_t* image = lv_obj_get_child_by_type(root, 0, &lv_image_class);
    CHECK_EQ(lv_obj_get_style_image_opa(image, LV_PART_MAIN), LV_OPA_COVER);
    canvas.begin_frame();
    CHECK(lv_obj_has_flag(image, LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("RGB565 turn arrow dark phase produces visible green pixels") {
    LvglSession session;
    PixelCapture<536, 240> capture;
    lv_display_t* display = session.display();
    CHECK(display != nullptr);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(display, &capture);
    lv_display_set_flush_cb(display, PixelCapture<536, 240>::flush);
    lv_display_set_buffers(display, capture.draw_buffer.data(), nullptr,
                           sizeof(capture.draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*display));
    lv_obj_t* root = dashboard_content(display);
    for (const auto symbol : {tesla_headup::application::Symbol::LeftArrow,
                              tesla_headup::application::Symbol::RightArrow}) {
        std::uint16_t bright_green = 0;
        for (const bool illuminated : {true, false}) {
            canvas.begin_frame();
            canvas.draw_symbol({
                .symbol = symbol,
                .role = tesla_headup::application::SymbolRole::TurnSignal,
                .illuminated = illuminated,
            });
            canvas.end_frame();
            lv_refr_now(display);
            lv_obj_t* image = lv_obj_get_child_by_type(root, 0, &lv_image_class);
            lv_area_t bounds;
            lv_obj_get_coords(image, &bounds);
            std::uint16_t peak_green = 0;
            for (auto y = bounds.y1; y <= bounds.y2; ++y) {
                for (auto x = bounds.x1; x <= bounds.x2; ++x) {
                    CHECK(x >= 0 && x < capture.width);
                    CHECK(y >= 0 && y < capture.height);
                    const auto pixel = capture.pixels[y * capture.width + x];
                    const auto green = static_cast<std::uint16_t>((pixel >> 5U) & 63U);
                    const auto red = ((pixel >> 11U) & 31U) * 2U;
                    const auto blue = (pixel & 31U) * 2U;
                    // Ignore white/grey lane pixels behind transparent padding.
                    if (green > red * 2U && green * 2U > blue * 3U &&
                        green > peak_green) {
                        peak_green = green;
                    }
                }
            }
            if (illuminated) {
                bright_green = peak_green;
                CHECK(bright_green > 40);
            } else {
                // Assert actual RGB565A8 alpha blending, not just widget opacity.
                CHECK(peak_green > 0);
                CHECK(peak_green * 100 >= bright_green * 45);
                CHECK(peak_green * 100 <= bright_green * 55);
            }
        }
    }
}

#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
TEST_CASE("live power packets produce visible fill above the track in both road scene sizes") {
    using namespace std::chrono_literals;
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        PixelCapture<800, 480> capture;
        auto* display = session.display();
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_user_data(display, &capture);
        lv_display_set_flush_cb(display, PixelCapture<800, 480>::flush);
        lv_display_set_buffers(display, capture.draw_buffer.data(), nullptr,
            sizeof(capture.draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*display));
        tesla_headup::application::TelemetryStore telemetry;
        tesla_headup::dashkit::DashKitTelemetryIngestor ingestor{telemetry};
        tesla_headup::application::PowerIndicator power;
        tesla_headup::application::Dashboard dashboard{canvas};
        CHECK(dashboard.add_feature(power));
        tesla_headup::application::DashboardRenderScheduler scheduler{dashboard};
        tesla_headup::application::PowerPresenter presenter{telemetry, power, scheduler};
        // Hand-authored frames: bus 1, 80/320 kW limits; 400 V, +400 A.
        // Four-byte DLC retains every signal required by the power gauge.
        std::array<std::uint8_t, 29> packet{
            2, 0,0,0,0, 1, 0x52,0x02,0,0, 4, 0x40,0x1F,0x00,0x7D,
               0,0,0,0, 1, 0x32,0x01,0,0, 4, 0x40,0x9C,0x60,0x70};
        CHECK_EQ(ingestor.ingest(packet, 0ms).decoded_power_frames, 2U);
        CHECK(presenter.refresh(0ms));
        CHECK(scheduler.tick(0ms));
        lv_refr_now(display);
        const int x = wide ? 12 : 10;
        const int center = wide ? 192 : 96;
        const int fill_height = wide ? 72 : 36;
        const auto pixel = [&](int y) { return capture.pixels[y * capture.width + x]; };
        constexpr std::uint16_t track = 0x2986, grey = 0xBE18, green = 0x5DED;
        CHECK_EQ(pixel(center - fill_height / 2), grey);
        CHECK_EQ(pixel(center + fill_height / 2), track);
        packet[27] = 0xE8; packet[28] = 0x03; // -100 A => -50% of regen limit.
        CHECK_EQ(ingestor.ingest(packet, 50ms).decoded_power_frames, 2U);
        CHECK(presenter.refresh(50ms));
        CHECK(scheduler.tick(50ms));
        lv_refr_now(display);
        CHECK_EQ(pixel(center - fill_height / 2), track);
        CHECK_EQ(pixel(center + fill_height / 2), green);
        CHECK(presenter.refresh(551ms)); // Current expiry clears the actual pixels.
        CHECK(scheduler.tick(551ms));
        lv_refr_now(display);
        CHECK_EQ(pixel(center - fill_height / 2), track);
        CHECK_EQ(pixel(center + fill_height / 2), track);
    }
}

TEST_CASE("road-scene regions and speed typography remain aligned across digit counts") {
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* composition = dashboard_composition(session.display());
        auto* driving = dashboard_content(session.display());
        auto* footer = lv_obj_get_child(composition, 1);
        lv_obj_update_layout(composition);
        CHECK_EQ(lv_obj_get_width(driving), wide ? 800 : 536);
        CHECK_EQ(lv_obj_get_height(driving), wide ? 408 : 204);
        CHECK_EQ(lv_obj_get_y(footer), lv_obj_get_height(driving));
        CHECK_EQ(lv_obj_get_width(footer), lv_obj_get_width(driving));
        CHECK_EQ(lv_obj_get_height(footer), wide ? 72 : 36);
        CHECK(!lv_obj_has_flag(driving, LV_OBJ_FLAG_OVERFLOW_VISIBLE));
        const auto children = lv_obj_get_child_count(driving);
        for (const auto* digits : {"0", "9", "30", "130"}) {
            canvas.begin_frame();
            canvas.draw_text({.text = digits, .role = tesla_headup::application::TextRole::PrimaryValue});
            canvas.draw_text({.text = "km/h", .role = tesla_headup::application::TextRole::Unit});
            canvas.end_frame();
            lv_obj_update_layout(composition);
            auto* speed = dashboard_label(driving, 0);
            auto* unit = dashboard_label(driving, 1);
            CHECK_EQ(lv_obj_get_x(speed), wide ? 34 : 24);
            CHECK_EQ(lv_obj_get_y(speed), wide ? 24 : 12);
            CHECK_EQ(lv_obj_get_x(unit), lv_obj_get_x(speed) + lv_obj_get_width(speed) + 3);
            CHECK_EQ(painted_bottom(unit), painted_bottom(speed));
            CHECK(lv_obj_get_y(unit) + lv_obj_get_height(unit) < lv_obj_get_height(driving));
            CHECK_EQ(lv_obj_get_child_count(driving), children);
        }
    }
}

TEST_CASE("road-scene power rail fills from center and clears unavailable data") {
    LvglSession session;
    CHECK(session.display() != nullptr);
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    const auto child_count = lv_obj_get_child_count(root);
    canvas.begin_frame();
    canvas.draw_power({500});
    lv_obj_update_layout(root);
    lv_font_glyph_dsc_t digit{};
    CHECK(lv_font_get_glyph_dsc(&tesla_headup_inter_regular_64, &digit, '0', 0));
    const int top = 12 + tesla_headup_inter_regular_64.line_height -
        tesla_headup_inter_regular_64.base_line - digit.ofs_y - digit.box_h;
    const int half = (168 - top) / 2;
    const int center = top + half;
    const int half_fill = (500 * half + 500) / 1000;
    lv_obj_t* fill = nullptr;
    for (std::uint32_t i = 0; i < child_count; ++i) {
        lv_obj_t* child = lv_obj_get_child(root, static_cast<std::int32_t>(i));
        if (lv_obj_get_x(child) == 8 && lv_obj_get_width(child) == 5 &&
            lv_obj_get_height(child) == half_fill) {
            fill = child;
            break;
        }
    }
    CHECK(fill != nullptr);
    CHECK_EQ(lv_obj_get_y(fill), center - half_fill);
    CHECK(!lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
    const auto discharge_color = lv_obj_get_style_bg_color(fill, LV_PART_MAIN);
    for (const std::int16_t level : {-500, -1000, 1000, -32768, 32767}) {
        canvas.begin_frame();
        CHECK(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
        canvas.draw_power({level});
        lv_obj_update_layout(root);
        const bool regen = level < 0;
        const auto height = level == -500 ? half_fill : half;
        CHECK_EQ(lv_obj_get_height(fill), height);
        CHECK_EQ(lv_obj_get_y(fill), regen ? center : center - height);
        CHECK(!lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
        CHECK(lv_color_eq(lv_obj_get_style_bg_color(fill, LV_PART_MAIN),
                         regen ? lv_color_hex(0x5CBD68) : discharge_color));
        CHECK_EQ(lv_obj_get_child_count(root), child_count);
    }
    canvas.begin_frame();
    canvas.draw_power({0});
    CHECK(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("outer lanes follow each blind spot independently and clear next frame") {
    using namespace tesla_headup::application;
    for (const bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* root = dashboard_content(session.display());
        const int rows = wide ? 6 : 4;
        for (const bool autopilot : {false, true})
        for (const bool illuminated : {false, true})
        for (const int signals : {0, 1, 2, 3, 0})
        for (const int mask : {0, 1, 2, 3, 0}) {
            canvas.begin_frame();
            for (int side = 0; side < 2; ++side)
                if ((mask & (1 << side)) != 0)
                    canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings,
                        side == 0 ? DisplayEdge::Left : DisplayEdge::Right,
                        GradientRole::BlindSpotWarning, 40});
            if (autopilot)
                canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot, SymbolRole::Autopilot});
            for (int side = 0; side < 2; ++side)
                if ((signals & (1 << side)) != 0) {
                    SymbolElement signal{FeatureId::TurnSignals,
                        side == 0 ? Symbol::LeftArrow : Symbol::RightArrow,
                        SymbolRole::TurnSignal};
                    signal.illuminated = illuminated;
                    canvas.draw_symbol(signal);
                }
            canvas.end_frame();
            for (int index = 0; index < rows * 4; ++index) {
                auto* line = lv_obj_get_child_by_type(root, index, &lv_line_class);
                const bool hidden = index >= rows * 2 &&
                    (mask & (1 << ((index - rows * 2) / rows))) == 0;
                CHECK_EQ(lv_obj_has_flag(line, LV_OBJ_FLAG_HIDDEN), hidden);
                if (index < rows * 2) {
                    const bool occupied = (mask & (1 << (index / rows))) != 0;
                    CHECK(lv_color_eq(lv_obj_get_style_line_color(line, LV_PART_MAIN),
                        lv_color_hex(occupied && (signals & (1 << (index / rows))) != 0
                            ? 0xFF0000 : autopilot ? 0x0071FF : 0xD9E0E3)));
                }
            }
        }
    }
}

TEST_CASE("road-scene canvas shows blind-spot vehicles in matching lanes") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    lv_obj_t* left_vehicle =
        lv_obj_get_child_by_type(root, 10, &lv_image_class);
    lv_obj_t* right_vehicle =
        lv_obj_get_child_by_type(root, 11, &lv_image_class);
    CHECK(left_vehicle != nullptr);
    CHECK(right_vehicle != nullptr);
    CHECK_EQ(
        lv_image_get_src(left_vehicle),
        &tesla_headup::platform::assets::blind_spot_left_grey_image);
    CHECK_EQ(
        lv_image_get_src(right_vehicle),
        &tesla_headup::platform::assets::blind_spot_right_grey_image);

    canvas.begin_frame();
    canvas.draw_edge_gradient(tesla_headup::application::EdgeGradientElement{
        .feature = tesla_headup::application::FeatureId::BlindSpotWarnings,
        .edge = tesla_headup::application::DisplayEdge::Left,
        .role = tesla_headup::application::GradientRole::BlindSpotWarning,
        .width = 76,
    });
    lv_obj_update_layout(root);
    CHECK(!lv_obj_has_flag(left_vehicle, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(right_vehicle, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(lv_obj_get_x(left_vehicle), 75);
    CHECK_EQ(lv_obj_get_y(left_vehicle), 107);
    CHECK_EQ(
        lv_image_get_src(left_vehicle),
        &tesla_headup::platform::assets::blind_spot_left_grey_image);

    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .feature = tesla_headup::application::FeatureId::TurnSignals,
        .symbol = tesla_headup::application::Symbol::LeftArrow,
        .role = tesla_headup::application::SymbolRole::TurnSignal,
        .visible = false,
    });
    CHECK_EQ(
        lv_image_get_src(left_vehicle),
        &tesla_headup::platform::assets::blind_spot_left_image);

    canvas.begin_frame();
    canvas.draw_edge_gradient(tesla_headup::application::EdgeGradientElement{
        .feature = tesla_headup::application::FeatureId::BlindSpotWarnings,
        .edge = tesla_headup::application::DisplayEdge::Right,
        .role = tesla_headup::application::GradientRole::BlindSpotWarning,
        .width = 76,
    });
    lv_obj_update_layout(root);
    CHECK(lv_obj_has_flag(left_vehicle, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(right_vehicle, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(lv_obj_get_x(right_vehicle), 334);
    CHECK_EQ(lv_obj_get_y(right_vehicle), 107);
    CHECK_EQ(
        lv_image_get_src(right_vehicle),
        &tesla_headup::platform::assets::blind_spot_right_grey_image);

    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .feature = tesla_headup::application::FeatureId::TurnSignals,
        .symbol = tesla_headup::application::Symbol::RightArrow,
        .role = tesla_headup::application::SymbolRole::TurnSignal,
        .visible = false,
    });
    CHECK_EQ(
        lv_image_get_src(right_vehicle),
        &tesla_headup::platform::assets::blind_spot_right_image);
}
#endif

TEST_CASE("LVGL canvas applies the speed and unit typography") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = "110",
        .role = tesla_headup::application::TextRole::PrimaryValue,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = "km/h",
        .role = tesla_headup::application::TextRole::Unit,
    });

    lv_obj_t* speed = lv_obj_get_child_by_type(root, 0, &lv_label_class);
    lv_obj_t* unit = lv_obj_get_child_by_type(root, 1, &lv_label_class);
    CHECK(speed != nullptr);
    CHECK(unit != nullptr);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    const lv_font_t* expected_speed_font = &tesla_headup_inter_regular_64;
#else
    const lv_font_t* expected_speed_font = &tesla_headup_speed_font_110;
#endif
    CHECK_EQ(
        lv_obj_get_style_text_font(speed, LV_PART_MAIN),
        expected_speed_font);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    CHECK_EQ(lv_obj_get_style_text_font(unit, LV_PART_MAIN), &tesla_headup_inter_medium_16);
#else
    CHECK_EQ(lv_obj_get_style_text_font(unit, LV_PART_MAIN), &lv_font_montserrat_18);
#endif
    CHECK(lv_color_eq(
        lv_obj_get_style_text_color(unit, LV_PART_MAIN),
        lv_color_hex(0xB8C1C5)));
}

TEST_CASE("LVGL canvas reserves a persistent status label with severity color") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    CHECK_EQ(lv_obj_get_child_count_by_type(root, &lv_label_class), 3U);
    CHECK_EQ(lv_obj_get_child_count_by_type(
        lv_obj_get_child(dashboard_composition(session.display()), 1), &lv_label_class), 2U);
#else
    CHECK_EQ(lv_obj_get_child_count_by_type(root, &lv_label_class), 5U);
#endif

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::SystemStatus,
        .text = "PAIRING REQUIRED",
        .role = tesla_headup::application::TextRole::StatusCritical,
        .vertical_offset = -104,
    });

    lv_obj_t* status = dashboard_label(root, 2);
    CHECK(status != nullptr);
    CHECK_EQ(std::string_view{lv_label_get_text(status)}, "PAIRING REQUIRED");
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    CHECK_EQ(lv_obj_get_style_text_font(status, LV_PART_MAIN), &tesla_headup_inter_medium_18);
#else
    CHECK_EQ(lv_obj_get_style_text_font(status, LV_PART_MAIN), &lv_font_montserrat_18);
#endif
    CHECK(lv_color_eq(
        lv_obj_get_style_text_color(status, LV_PART_MAIN),
        lv_color_hex(0xFF453A)));
}

TEST_CASE("LVGL canvas renders a persistent European speed-limit sign") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::SpeedLimit,
        .text = "50",
        .role = tesla_headup::application::TextRole::SpeedLimit,
        .horizontal_offset = 220,
        .vertical_offset = 45,
    });
    lv_obj_update_layout(root);

    lv_obj_t* label = dashboard_label(root, 4);
    CHECK(label != nullptr);
    CHECK_EQ(std::string_view{lv_label_get_text(label)}, "50");
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    CHECK_EQ(lv_obj_get_style_text_font(label, LV_PART_MAIN), &tesla_headup_inter_semibold_24);
#else
    CHECK_EQ(lv_obj_get_style_text_font(label, LV_PART_MAIN), &lv_font_montserrat_24);
#endif
    CHECK(lv_color_eq(
        lv_obj_get_style_text_color(label, LV_PART_MAIN),
        lv_color_hex(0x000000)));

    lv_obj_t* sign = nullptr;
    const auto object_count =
        lv_obj_get_child_count_by_type(root, &lv_obj_class);
    for (std::uint32_t index = 0; index < object_count; ++index) {
        lv_obj_t* candidate = lv_obj_get_child_by_type(
            root, static_cast<std::int32_t>(index), &lv_obj_class);
        if (candidate != nullptr && lv_obj_get_width(candidate) == 62 &&
            lv_obj_get_height(candidate) == 62) {
            sign = candidate;
            break;
        }
    }
    CHECK(sign != nullptr);
    CHECK(!lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(lv_obj_get_style_border_width(sign, LV_PART_MAIN), 7);
    CHECK(lv_color_eq(
        lv_obj_get_style_bg_color(sign, LV_PART_MAIN),
        lv_color_hex(0xFFFFFF)));
    CHECK(lv_color_eq(
        lv_obj_get_style_border_color(sign, LV_PART_MAIN),
        lv_color_hex(0xE02020)));

    canvas.begin_frame();
    CHECK(lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(label, LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("LVGL canvas keeps unit and gear in dedicated text slots") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::SystemStatus,
        .text = "CONNECTED",
        .role = tesla_headup::application::TextRole::StatusInfo,
        .vertical_offset = -104,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::SelectedGear,
        .text = "D",
        .role = tesla_headup::application::TextRole::Gear,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::VehicleSpeed,
        .text = "135",
        .role = tesla_headup::application::TextRole::PrimaryValue,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .feature = tesla_headup::application::FeatureId::VehicleSpeed,
        .text = "km/h",
        .role = tesla_headup::application::TextRole::Unit,
    });
    lv_obj_update_layout(root);

    lv_obj_t* unit = lv_obj_get_child_by_type(root, 1, &lv_label_class);
    lv_obj_t* status = dashboard_label(root, 2);
    lv_obj_t* gear = dashboard_label(root, 3);
    CHECK(unit != nullptr);
    CHECK(status != nullptr);
    CHECK(gear != nullptr);
    CHECK_EQ(std::string_view{lv_label_get_text(unit)}, "km/h");
    CHECK(!lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(std::string_view{lv_label_get_text(gear)}, "D");
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    const auto* gear_font = &tesla_headup_inter_medium_18;
#else
    const auto* gear_font = &lv_font_montserrat_24;
#endif
    CHECK_EQ(lv_obj_get_style_text_font(gear, LV_PART_MAIN), gear_font);
    CHECK_EQ(lv_obj_get_y(gear), lv_obj_get_y(status));
    CHECK(lv_obj_get_x(gear) + lv_obj_get_width(gear) >= 520);
#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    CHECK(lv_obj_get_x(status) + lv_obj_get_width(status) < lv_obj_get_x(gear));
#endif
}

#if defined(TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
TEST_CASE("battery occupies the left footer and yields to connection warnings independent of draw order") {
  for (bool wide : {false, true}) {
    LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    auto* composition = dashboard_composition(session.display());
    auto* footer = lv_obj_get_child(composition, 1);
    auto* battery = lv_obj_get_child(footer, 1);
    auto* body = lv_obj_get_child(battery, 0);
    auto* fill = lv_obj_get_child(body, 1);
    auto* label = lv_obj_get_child_by_type(battery, 0, &lv_label_class);
    const auto children = lv_obj_get_child_count(footer);
    using namespace tesla_headup::application;
    for (const auto percent : {0U, 1U, 51U, 100U}) {
        for (bool autopilot : {false, true}) {
            canvas.begin_frame();
            canvas.draw_battery({static_cast<std::uint8_t>(percent)});
            canvas.draw_text({FeatureId::SelectedGear, "D", TextRole::Gear});
            if (autopilot) canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot,
                SymbolRole::Autopilot, Anchor::Center, 0, 0, 32, 30, true, true});
            canvas.end_frame();
            lv_obj_update_layout(composition);
            CHECK(!lv_obj_has_flag(battery, LV_OBJ_FLAG_HIDDEN));
            CHECK_EQ(lv_obj_get_x(battery), wide ? 24 : 12);
            CHECK_EQ(lv_obj_get_y(battery), wide ? 24 : 6);
            const int footer_center = lv_obj_get_height(footer) / 2;
            CHECK_EQ(lv_obj_get_y(battery) + lv_obj_get_height(battery) / 2, footer_center);
            CHECK_EQ(lv_obj_get_y(body) + lv_obj_get_height(body) / 2, 12);
            CHECK_EQ(lv_obj_get_y(label) + lv_obj_get_height(label) / 2, 12);
            auto* gear = lv_obj_get_child_by_type(footer, 1, &lv_label_class);
            CHECK_EQ(lv_obj_get_y(gear) + lv_obj_get_height(gear) / 2, footer_center);
            if (autopilot) {
                auto* icon = lv_obj_get_child_by_type(footer, 0, &lv_image_class);
                CHECK_EQ(lv_obj_get_y(icon) + lv_obj_get_height(icon) / 2, footer_center);
            }
            CHECK_EQ(lv_obj_get_width(fill), static_cast<int>((40 * percent + 50) / 100));
            CHECK_EQ(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN), (40 * percent + 50) / 100 == 0);
            CHECK_EQ(lv_obj_get_child_count(footer), children);
        }
    }
    CHECK_EQ(std::string_view{lv_label_get_text(label)}, "100%");
    for (bool warning_first : {false, true}) {
        canvas.begin_frame();
        const TextElement warning{FeatureId::SystemStatus, "NO DATA", TextRole::StatusWarning};
        if (warning_first) canvas.draw_text(warning);
        canvas.draw_battery({51});
        if (!warning_first) canvas.draw_text(warning);
        canvas.end_frame();
        CHECK(lv_obj_has_flag(battery, LV_OBJ_FLAG_HIDDEN));
    }
    canvas.begin_frame();
    canvas.draw_battery({255});
    canvas.end_frame();
    CHECK(lv_obj_has_flag(battery, LV_OBJ_FLAG_HIDDEN));
    canvas.begin_frame();
    canvas.draw_battery({51});
    canvas.end_frame();
    CHECK(!lv_obj_has_flag(battery, LV_OBJ_FLAG_HIDDEN));
    canvas.begin_frame();
    canvas.end_frame();
    CHECK(lv_obj_has_flag(battery, LV_OBJ_FLAG_HIDDEN));
  }
}

TEST_CASE("Park swaps the retained ego image and hides lanes only while fresh P is rendered") {
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* composition = dashboard_composition(session.display());
        auto* root = lv_obj_get_child(composition, 0);
        const auto children = lv_obj_get_child_count(root);
        auto* ego = lv_obj_get_child_by_type(root, 12, &lv_image_class);
        for (const auto gear : {"P", "D", "P", "R", "P", "N", "P", ""}) {
            canvas.begin_frame();
            if (*gear != '\0')
                canvas.draw_text({tesla_headup::application::FeatureId::SelectedGear, gear,
                                  tesla_headup::application::TextRole::Gear});
            canvas.end_frame();
            const bool parked = std::string_view{gear} == "P";
            CHECK_EQ(lv_image_get_src(ego), wide
                ? (parked ? &tesla_headup::platform::assets::road_scene_park_wide_image
                          : &tesla_headup::platform::assets::road_scene_vehicle_wide_image)
                : (parked ? &tesla_headup::platform::assets::road_scene_park_image
                          : &tesla_headup::platform::assets::road_scene_vehicle_image));
            for (int index = 0; index < (wide ? 24 : 16); ++index)
                CHECK_EQ(lv_obj_has_flag(lv_obj_get_child_by_type(root, index, &lv_line_class),
                                         LV_OBJ_FLAG_HIDDEN), parked || index >= (wide ? 12 : 8));
            CHECK_EQ(lv_obj_get_child_count(root), children);
            if (parked) {
                lv_obj_update_layout(composition);
                CHECK_EQ(lv_obj_get_x(ego) + lv_obj_get_width(ego) / 2, wide ? 400 : 268);
                CHECK_EQ(lv_obj_get_y(ego) + lv_obj_get_height(ego), wide ? 408 : 204);
                CHECK_EQ(lv_obj_get_width(ego), wide ? 410 : 256);
                CHECK_EQ(lv_obj_get_height(ego), wide ? 307 : 160);
            }
        }
    }
}

TEST_CASE("native road scene enlarges vehicles retains twelve straight lane segments and edge-safe labels") {
    LvglSession session{800, 480};
    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    auto* composition = dashboard_composition(session.display());
    auto* root = lv_obj_get_child(composition, 0);
    auto* footer = lv_obj_get_child(composition, 1);
    lv_obj_update_layout(composition);
    CHECK_EQ(lv_obj_get_width(root), 800);
    CHECK_EQ(lv_obj_get_height(root), 408);
    CHECK_EQ(lv_obj_get_y(footer), 408);
    CHECK_EQ(lv_obj_get_width(footer), 800);
    CHECK_EQ(lv_obj_get_height(footer), 72);
    CHECK_EQ(lv_obj_get_style_bg_opa(root, LV_PART_MAIN), LV_OPA_TRANSP);
    CHECK_EQ(lv_obj_get_style_bg_opa(footer, LV_PART_MAIN), LV_OPA_TRANSP);
    CHECK(!lv_obj_has_flag(root, LV_OBJ_FLAG_OVERFLOW_VISIBLE));
    const auto footer_children = lv_obj_get_child_count(footer);
    const auto children = lv_obj_get_child_count(root);
    CHECK_EQ(lv_obj_get_child_count_by_type(root, &lv_line_class), 24U);
    auto* ego = lv_obj_get_child_by_type(root, 12, &lv_image_class);
    CHECK_EQ(lv_image_get_src(ego), &tesla_headup::platform::assets::road_scene_vehicle_wide_image);
    CHECK_EQ(lv_image_get_src_width(ego), 128); // Supplied Model Y, fitted without runtime scaling.
    for (int index = 0; index < 12; ++index) {
        auto* line = lv_obj_get_child_by_type(root, index, &lv_line_class);
        const auto* points = lv_line_get_points(line);
        for (int point = 0; point < 2; ++point) {
            const int delta = (static_cast<int>(points[point].y) - 64) * 96 / 344;
            CHECK_EQ(points[point].x, index < 6 ? 400 - delta : 400 + delta);
        }
    }
    using namespace tesla_headup::application;
    canvas.begin_frame();
    canvas.draw_text({FeatureId::VehicleSpeed, "135", TextRole::PrimaryValue});
    canvas.draw_text({FeatureId::VehicleSpeed, "km/h", TextRole::Unit});
    canvas.draw_text({FeatureId::SelectedGear, "D", TextRole::Gear});
    canvas.draw_text({FeatureId::SystemStatus, "RECONNECTING", TextRole::StatusWarning});
    canvas.draw_text({FeatureId::SpeedLimit, "130", TextRole::SpeedLimit});
    canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Left, GradientRole::BlindSpotWarning, 40});
    auto* left = lv_obj_get_child_by_type(root, 10, &lv_image_class);
    auto* right = lv_obj_get_child_by_type(root, 11, &lv_image_class);
    CHECK_EQ(lv_image_get_src(left), &tesla_headup::platform::assets::blind_spot_left_grey_wide_image);
    canvas.draw_symbol({FeatureId::TurnSignals, Symbol::LeftArrow, SymbolRole::TurnSignal,
        Anchor::Center, 0, 0, 32, 30, true, false});
    canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot, SymbolRole::Autopilot,
        Anchor::Center, 0, 0, 32, 30, true, true});
    auto* autopilot = lv_obj_get_child_by_type(footer, 0, &lv_image_class);
    CHECK_EQ(lv_image_get_src(autopilot), &tesla_headup::platform::assets::autopilot_road_scene_wide_image);
    CHECK_EQ(lv_image_get_src_width(autopilot), 77);
    CHECK_EQ(lv_image_get_src(left), &tesla_headup::platform::assets::blind_spot_left_wide_image);
    CHECK_EQ(lv_image_get_src(right), &tesla_headup::platform::assets::blind_spot_right_grey_wide_image);
    canvas.end_frame();
    lv_obj_update_layout(root);
    CHECK(lv_obj_get_x(left) + lv_obj_get_width(left) < lv_obj_get_x(ego));
    for (int index = 0; index < 5; ++index) {
        auto* label = index < 3
            ? lv_obj_get_child_by_type(root, index, &lv_label_class)
            : lv_obj_get_child_by_type(footer, index - 3, &lv_label_class);
        lv_area_t bounds{}; lv_obj_get_coords(label, &bounds);
        CHECK(bounds.x1 >= 2 && bounds.y1 >= 2);
        CHECK(bounds.x2 < 798 && bounds.y2 < 478);
        CHECK(index < 3 ? bounds.y2 < 408 : bounds.y1 >= 408);
    }
    auto* speed = lv_obj_get_child_by_type(root, 0, &lv_label_class);
    auto* unit = lv_obj_get_child_by_type(root, 1, &lv_label_class);
    CHECK_EQ(lv_obj_get_style_text_font(speed, LV_PART_MAIN), &tesla_headup_inter_regular_110);
    auto* limit = lv_obj_get_child_by_type(root, 2, &lv_label_class);
    CHECK_EQ(lv_obj_get_style_text_font(limit, LV_PART_MAIN), &tesla_headup_inter_semibold_40);
    CHECK_EQ(lv_obj_get_y(left), 284);
    CHECK_EQ(lv_obj_get_x(autopilot), 362);
    CHECK_EQ(lv_obj_get_y(autopilot), 0);
    auto* status = lv_obj_get_child_by_type(footer, 0, &lv_label_class);
    auto* gear = lv_obj_get_child_by_type(footer, 1, &lv_label_class);
    CHECK(lv_obj_get_x(gear) > lv_obj_get_x(autopilot) + 77);
    CHECK(lv_obj_get_x(status) < lv_obj_get_x(autopilot));
    lv_area_t limit_bounds{}; lv_obj_get_coords(limit, &limit_bounds);
    CHECK(limit_bounds.x1 >= 680 && limit_bounds.x2 < 776);
    CHECK(limit_bounds.y1 >= 24 && limit_bounds.y2 < 120);
    CHECK_EQ(lv_obj_get_y(speed), 24);
    CHECK(lv_obj_get_x(unit) > lv_obj_get_x(speed) + lv_obj_get_width(speed));
    CHECK_EQ(lv_obj_get_child_count(root), children);
    CHECK_EQ(lv_obj_get_child_count(footer), footer_children);
    canvas.begin_frame();
    CHECK(lv_obj_has_flag(autopilot, LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(lv_image_get_src(left), &tesla_headup::platform::assets::blind_spot_left_grey_wide_image);
}

TEST_CASE("three-lane road keeps neutral outer bounds symmetric cars and balanced ego clearance") {
    using namespace tesla_headup::application;
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* root = dashboard_content(session.display());
        const int rows = wide ? 6 : 4;
        const int center = wide ? 400 : 268;
        const auto* points = lv_line_get_points(
            lv_obj_get_child_by_type(root, rows - 1, &lv_line_class));
        // Body widths measured from opaque pixels just above the tire contact
        // plane in the supplied, prepared Model Y sprites (exclude alpha fringe).
        const int contact_y = wide ? 336 : 178;
        const int body_width = wide ? 90 : 70;
        const int left_x = points[0].x + (contact_y - points[0].y) *
            (points[1].x - points[0].x) / (points[1].y - points[0].y);
        const int lane_width = 2 * (center - left_x);
        CHECK(lane_width * 100 >= body_width * 160);
        CHECK(lane_width * 100 <= body_width * 170);
        canvas.begin_frame();
        canvas.draw_symbol({FeatureId::Autopilot, Symbol::Autopilot, SymbolRole::Autopilot});
        for (int index = rows * 2; index < rows * 4; ++index) {
            auto* line = lv_obj_get_child_by_type(root, index, &lv_line_class);
            CHECK(lv_color_eq(lv_obj_get_style_line_color(line, LV_PART_MAIN), lv_color_hex(0x515D63)));
        }
        canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Left, GradientRole::BlindSpotWarning, 40});
        canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Right, GradientRole::BlindSpotWarning, 40});
        canvas.end_frame();
        lv_obj_update_layout(root);
        auto* left = lv_obj_get_child_by_type(root, 10, &lv_image_class);
        auto* right = lv_obj_get_child_by_type(root, 11, &lv_image_class);
        const int left_center = lv_obj_get_x(left) + lv_obj_get_width(left) / 2;
        const int right_center = lv_obj_get_x(right) + lv_obj_get_width(right) / 2;
        CHECK_EQ((right_center - center) - (center - left_center), wide ? 2 : 1);
        CHECK_EQ(right_center - center, wide ? 184 : 130);
    }
}

TEST_CASE("all four boundaries share one VP through animation and clear adjacent tires") {
    using namespace tesla_headup::application;
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* root = dashboard_content(session.display());
        const int rows = wide ? 6 : 4;
        const int vp_x = wide ? 400 : 268, vp_y = wide ? 64 : 32;
        const int height = wide ? 408 : 204;
        const auto project = [&](bool outer, bool right, int y) {
            const int depth = (wide ? 96 : 69) * (y - vp_y) / (height - vp_y);
            return vp_x + (right ? 1 : -1) * (outer ? 3 : 1) * depth;
        };
        for (int y = vp_y; y <= height; ++y) {
            const int left_outer = project(true, false, y);
            const int left_inner = project(false, false, y);
            const int right_inner = project(false, true, y);
            const int right_outer = project(true, true, y);
            CHECK_EQ(left_inner - left_outer, right_inner - left_inner);
            CHECK_EQ(right_inner - left_inner, right_outer - right_inner);
        }
        for (int boundary = 0; boundary < 4; ++boundary) {
            const auto* foreground = lv_line_get_points(lv_obj_get_child_by_type(root, boundary * rows + rows - 1, &lv_line_class));
            CHECK(foreground[1].y < height);
        }
        for (int boundary = 0; boundary < 4; ++boundary) {
            const auto* points = lv_line_get_points(lv_obj_get_child_by_type(root, boundary * rows, &lv_line_class));
            CHECK_EQ(points[0].y, wide ? 124 : 62);
            CHECK_EQ(points[0].x, project(boundary >= 2, boundary % 2 != 0, points[0].y));
            CHECK(points[0].x != vp_x);
        }
        canvas.begin_frame();
        canvas.draw_text({FeatureId::VehicleSpeed, "36", TextRole::PrimaryValue});
        canvas.draw_text({FeatureId::SelectedGear, "D", TextRole::Gear});
        canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Left, GradientRole::BlindSpotWarning, 40});
        canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Right, GradientRole::BlindSpotWarning, 40});
        canvas.end_frame();
        lv_obj_update_layout(root);
        std::array<int, 2> footprint_clearances{};
        for (int side = 0; side < 2; ++side) {
            auto* car = lv_obj_get_child_by_type(root, 10 + side, &lv_image_class);
            const auto* image = static_cast<const lv_image_dsc_t*>(lv_image_get_src(car));
            const int w = image->header.w, h = image->header.h;
            const auto* alpha = image->data + w * h * 2;
            int tire_y = h - 1, first = w, last = -1;
            for (; tire_y >= 0; --tire_y) {
                for (int x = 0; x < w; ++x) if (alpha[tire_y * w + x] >= 128) {
                    first = std::min(first, x); last = std::max(last, x);
                }
                if (last >= 0) break;
            }
            CHECK(tire_y >= 0);
            // Match the outer rear tire on each side, rather than the nearer
            // inner tire that extends lower in the three-quarter artwork.
            int outer_tire_y = h - 1, outer_tire_x = -1;
            for (; outer_tire_y >= 0; --outer_tire_y) {
                for (int x = side == 0 ? 0 : w / 2; x < (side == 0 ? w / 2 : w); ++x)
                    if (alpha[outer_tire_y * w + x] >= 128) {
                        if (outer_tire_x < 0 || side != 0) outer_tire_x = x;
                    }
                if (outer_tire_x >= 0) break;
            }
            CHECK(outer_tire_x >= 0);
            const int tire_edge = lv_obj_get_x(car) + outer_tire_x;
            const int boundary_x = project(true, side != 0, lv_obj_get_y(car) + outer_tire_y);
            footprint_clearances[side] = side == 0 ? tire_edge - boundary_x : boundary_x - tire_edge;
            CHECK(footprint_clearances[side] > 4);
            const int y = lv_obj_get_y(car) + tire_y;
            const int outer_x = project(true, side != 0, y);
            if (side == 0) CHECK(outer_x + 4 < lv_obj_get_x(car) + first);
            else CHECK(outer_x - 4 > lv_obj_get_x(car) + last);
            const int corridor_center = (outer_x + project(false, side != 0, y)) / 2;
            const int car_center = lv_obj_get_x(car) + w / 2;
            CHECK(std::abs(corridor_center - car_center) <= (wide ? 20 : 12));
        }
        CHECK(std::abs(footprint_clearances[0] - footprint_clearances[1]) <= 1);
        for (auto elapsed : {0U, 100U, 250U, 1000U, 4300U}) {
            lv_tick_inc(elapsed); (void)lv_timer_handler();
            for (int index = 0; index < rows * 4; ++index) {
                const auto* points = lv_line_get_points(lv_obj_get_child_by_type(root, index, &lv_line_class));
                for (int endpoint = 0; endpoint < 2; ++endpoint)
                    CHECK_EQ(points[endpoint].x, project(index >= rows * 2, (index / rows) % 2 != 0, points[endpoint].y));
                if (index >= rows * 2) {
                    auto* outer = lv_obj_get_child_by_type(root, index, &lv_line_class);
                    auto* inner = lv_obj_get_child_by_type(root, index - rows * 2, &lv_line_class);
                    const auto* inner_points = lv_line_get_points(inner);
                    for (int endpoint = 0; endpoint < 2; ++endpoint)
                        CHECK_EQ(points[endpoint].y, inner_points[endpoint].y);
                    CHECK_EQ(lv_obj_get_style_line_width(outer, LV_PART_MAIN), lv_obj_get_style_line_width(inner, LV_PART_MAIN));
                    CHECK_EQ(lv_obj_get_style_line_opa(outer, LV_PART_MAIN), lv_obj_get_style_line_opa(inner, LV_PART_MAIN));
                }
            }
        }
    }
}

TEST_CASE("perspective dashes grow toward the car without joining through full animation cycles") {
    using namespace tesla_headup::application;
    for (const bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*session.display()));
        auto* root = dashboard_content(session.display());
        const int rows = wide ? 6 : 4;
        for (const auto gear : {"D", "R"}) {
            canvas.begin_frame();
            canvas.draw_text({FeatureId::VehicleSpeed, "120", TextRole::PrimaryValue});
            canvas.draw_text({FeatureId::SelectedGear, gear, TextRole::Gear});
            canvas.end_frame();
            for (int frame = 0; frame < 600; ++frame) {
                for (int boundary = 0; boundary < 4; ++boundary) {
                    std::array<std::array<int, 2>, 6> windows{};
                    for (int row = 0; row < rows; ++row) {
                        const auto* p = lv_line_get_points(lv_obj_get_child_by_type(
                            root, boundary * rows + row, &lv_line_class));
                        windows[row] = {static_cast<int>(p[0].y), static_cast<int>(p[1].y)};
                    }
                    std::sort(windows.begin(), windows.begin() + rows);
                    for (int row = 0; row < rows; ++row) {
                        CHECK(windows[row][1] > windows[row][0]);
                        if (row == 0) continue;
                        CHECK(windows[row][0] - windows[row - 1][1] >= 2);
                        // Independent endpoint rounding can differ by one pixel.
                        CHECK(windows[row][1] - windows[row][0] + 1 >=
                              windows[row - 1][1] - windows[row - 1][0]);
                    }
                    CHECK(windows[rows - 1][1] - windows[rows - 1][0] >
                          windows[0][1] - windows[0][0]);
                }
                lv_tick_inc(16);
                (void)lv_timer_handler();
            }
        }
    }
}

TEST_CASE("low-speed dash motion leaves overlapping road pixels stationary") {
    using namespace tesla_headup::application;
    for (bool wide : {false, true}) {
        LvglSession session{wide ? 800 : 536, wide ? 480 : 240};
        PixelCapture<800, 480> capture;
        auto* display = session.display();
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_user_data(display, &capture);
        lv_display_set_flush_cb(display, PixelCapture<800, 480>::flush);
        lv_display_set_buffers(display, capture.draw_buffer.data(), nullptr,
            sizeof(capture.draw_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        tesla_headup::platform::LvglDashboardCanvas canvas;
        CHECK(canvas.initialize(*display));
        auto* root = dashboard_content(display);
        canvas.begin_frame();
        canvas.draw_text({FeatureId::VehicleSpeed,"5",TextRole::PrimaryValue});
        canvas.draw_text({FeatureId::SelectedGear,"D",TextRole::Gear});
        canvas.draw_symbol({FeatureId::Autopilot,Symbol::Autopilot,SymbolRole::Autopilot});
        canvas.draw_edge_gradient({FeatureId::BlindSpotWarnings, DisplayEdge::Right, GradientRole::BlindSpotWarning, 40});
        canvas.end_frame();
        // Isolate lane pixels from the adjacent vehicle's painted silhouette.
        // Occupancy has already selected the outer lane in end_frame().
        lv_obj_add_flag(lv_obj_get_child_by_type(root, 11, &lv_image_class), LV_OBJ_FLAG_HIDDEN);

        lv_refr_now(display);
        // A width-band boundary must not create a sudden coverage jump.
        const auto* near_dash = lv_line_get_points(lv_obj_get_child_by_type(
            root, wide ? 23 : 15, &lv_line_class));
        int previous_coverage = -1;
        for (int y = static_cast<int>(near_dash[0].y) + 3;
             y < static_cast<int>(near_dash[1].y) - 3; ++y) {
            const int x_center = (wide ? 400 : 268) +
                3 * (wide ? 96 : 69) * (y - (wide ? 64 : 32)) / (wide ? 344 : 172);
            int coverage = 0;
            for (int x = x_center - 12; x <= x_center + 12; ++x)
                coverage += (capture.pixels[y * capture.width + x] >> 5) & 63;
            CHECK(coverage > 0);
            if (previous_coverage >= 0) {
                CHECK(coverage - previous_coverage <= 10);
                CHECK(previous_coverage - coverage <= 10);
            }
            previous_coverage = coverage;
        }
        const int count = wide ? 24 : 16;
        std::array<std::array<int, 2>,24> prior_windows{};
        std::vector<std::uint16_t> prior(capture.pixels.begin(),capture.pixels.end());
        unsigned visible_comparisons = 0;
        for (int frame = 0; frame < 100; ++frame) {
            for (int index = 0; index < count; ++index) {
                const auto* p = lv_line_get_points(lv_obj_get_child_by_type(root,index,&lv_line_class));
                prior_windows[index] = {static_cast<int>(p[0].y),static_cast<int>(p[1].y)};
            }
            prior.assign(capture.pixels.begin(),capture.pixels.end());
            lv_tick_inc(16); (void)lv_timer_handler(); lv_refr_now(display);
            for (int index = 0; index < count; ++index) {
                const auto* p = lv_line_get_points(lv_obj_get_child_by_type(root,index,&lv_line_class));
                const int first = std::max(prior_windows[index][0],static_cast<int>(p[0].y)) + 10;
                const int last = std::min(prior_windows[index][1],static_cast<int>(p[1].y)) - 10;
                for (int y = first; y < last; ++y) {
                    const int row_count = wide ? 6 : 4;
                    const bool right = (index / row_count) % 2 != 0;
                    const bool outer = index >= row_count * 2;
                    const int offset = (wide ? 96 : 69) * (y - (wide ? 64 : 32)) / (wide ? 344 : 172);
                    const int x_center = (wide ? 400 : 268) + (right ? 1 : -1) * (outer ? 3 : 1) * offset;
                    for (int x = x_center - 12; x <= x_center + 12; ++x) {
                        if (x < 0 || x >= (wide ? 800 : 536)) continue;
                        const auto i = static_cast<std::size_t>(y * capture.width + x);
                        CHECK_EQ(capture.pixels[i],prior[i]);
                        if (prior[i] != 0) ++visible_comparisons;
                    }
                }
            }
        }
        CHECK(visible_comparisons > 1000U);
    }
}

TEST_CASE("road-scene canvas keeps its lane geometry between frames") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);
    CHECK_EQ(lv_obj_get_child_count_by_type(root, &lv_line_class), 16U);
    lv_obj_t* left_top_lane =
        lv_obj_get_child_by_type(root, 0, &lv_line_class);
    lv_obj_t* right_top_lane =
        lv_obj_get_child_by_type(root, 4, &lv_line_class);
    CHECK(left_top_lane != nullptr);
    CHECK(right_top_lane != nullptr);
    CHECK_EQ(lv_line_get_points(left_top_lane)[0].y, 62);
    CHECK_EQ(lv_line_get_points(right_top_lane)[0].y, 62);
    lv_obj_t* left_second_lane =
        lv_obj_get_child_by_type(root, 1, &lv_line_class);
    CHECK(left_second_lane != nullptr);
    CHECK_EQ(
        lv_line_get_points(left_second_lane)[0].y -
            lv_line_get_points(left_top_lane)[1].y,
        8);
    for (std::int32_t index = 0; index < 8; ++index) {
        lv_obj_t* lane =
            lv_obj_get_child_by_type(root, index, &lv_line_class);
        CHECK(lane != nullptr);
        const auto* points = lv_line_get_points(lane);
        for (std::size_t point = 0; point < 2; ++point) {
            const auto perspective_offset = (points[point].y - 32) * 69 / 172;
            const std::int32_t expected_x = index < 4
                ? 268 - perspective_offset : 268 + perspective_offset;
            CHECK_EQ(points[point].x, expected_x);
        }
    }
    constexpr std::array expected_widths{2, 2, 3, 5, 2, 2, 3, 5};
    for (std::int32_t index = 0; index < 8; ++index) {
        lv_obj_t* lane =
            lv_obj_get_child_by_type(root, index, &lv_line_class);
        CHECK(lane != nullptr);
        CHECK_EQ(
            lv_obj_get_style_line_width(lane, LV_PART_MAIN),
            expected_widths[static_cast<std::size_t>(index)]);
    }

    canvas.begin_frame();
    canvas.end_frame();
    CHECK_EQ(lv_obj_get_child_count_by_type(root, &lv_line_class), 16U);
}

TEST_CASE("road-scene canvas positions the speed cluster and right indicators") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = "135",
        .role = tesla_headup::application::TextRole::PrimaryValue,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = "km/h",
        .role = tesla_headup::application::TextRole::Unit,
    });
    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .feature = tesla_headup::application::FeatureId::Autopilot,
        .symbol = tesla_headup::application::Symbol::Autopilot,
        .role = tesla_headup::application::SymbolRole::Autopilot,
        .width = 32,
        .height = 30,
    });
    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .feature = tesla_headup::application::FeatureId::ExteriorLights,
        .symbol = tesla_headup::application::Symbol::RearFogLight,
        .role = tesla_headup::application::SymbolRole::ExteriorLight,
        .horizontal_offset = 74,
        .width = 43,
        .height = 22,
    });
    lv_obj_update_layout(root);

    lv_obj_t* speed = lv_obj_get_child_by_type(root, 0, &lv_label_class);
    lv_obj_t* unit = lv_obj_get_child_by_type(root, 1, &lv_label_class);
    auto* footer = lv_obj_get_child(dashboard_composition(session.display()), 1);
    lv_obj_t* autopilot = lv_obj_get_child_by_type(footer, 0, &lv_image_class);
    lv_obj_t* rear_fog = lv_obj_get_child_by_type(root, 0, &lv_image_class);
    CHECK(speed != nullptr);
    CHECK(unit != nullptr);
    CHECK(autopilot != nullptr);
    CHECK(rear_fog != nullptr);
    CHECK_EQ(
        lv_obj_get_x(unit),
        lv_obj_get_x(speed) + lv_obj_get_width(speed) + 3);
    CHECK_EQ(painted_bottom(unit), painted_bottom(speed));
    CHECK_EQ(lv_obj_get_y(speed), 12);
    CHECK_EQ(lv_obj_get_y(footer), 204);
    CHECK_EQ(lv_obj_get_height(footer), 36);
    CHECK(lv_obj_get_y(footer) + lv_obj_get_y(autopilot) >
          lv_obj_get_y(speed) + lv_obj_get_height(speed));
    const auto autopilot_center =
        lv_obj_get_x(autopilot) + (lv_obj_get_width(autopilot) / 2);
    CHECK_EQ(lv_obj_get_width(autopilot), 38);
    CHECK_EQ(lv_obj_get_height(autopilot), 36);
    CHECK_EQ(autopilot_center, 268);
    CHECK(lv_obj_get_x(rear_fog) + lv_obj_get_width(rear_fog) >= 520);
    CHECK_EQ(
        lv_obj_get_y(rear_fog) + (lv_obj_get_height(rear_fog) / 2),
        172);
}

TEST_CASE("road-scene canvas stacks exterior lights on the right edge") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));

    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);

    constexpr std::array offsets{-84, -42, 0, 42, 74};
    constexpr std::array vertical_positions{36, 70, 104, 138, 172};
    constexpr std::array symbols{
        tesla_headup::application::Symbol::PositionLights,
        tesla_headup::application::Symbol::LowBeam,
        tesla_headup::application::Symbol::AdaptiveHighBeam,
        tesla_headup::application::Symbol::FrontFogLight,
        tesla_headup::application::Symbol::RearFogLight,
    };
    canvas.begin_frame();
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        canvas.draw_symbol(tesla_headup::application::SymbolElement{
            .feature = tesla_headup::application::FeatureId::ExteriorLights,
            .symbol = symbols[index],
            .role = tesla_headup::application::SymbolRole::ExteriorLight,
            .horizontal_offset = static_cast<std::int16_t>(offsets[index]),
            .width = 43,
            .height = 22,
        });
    }
    lv_obj_update_layout(root);

    for (std::size_t index = 0; index < symbols.size(); ++index) {
        lv_obj_t* image = lv_obj_get_child_by_type(
            root, static_cast<std::int32_t>(index), &lv_image_class);
        CHECK(image != nullptr);
        CHECK(lv_obj_get_x(image) + lv_obj_get_width(image) >= 520);
        CHECK_EQ(
            lv_obj_get_y(image) + (lv_obj_get_height(image) / 2),
            vertical_positions[index]);
    }
}

TEST_CASE("road-scene lanes follow active Autopilot color") {
    LvglSession session;
    CHECK(session.display() != nullptr);

    tesla_headup::platform::LvglDashboardCanvas canvas;
    CHECK(canvas.initialize(*session.display()));
    lv_obj_t* root = dashboard_content(session.display());
    CHECK(root != nullptr);
    lv_obj_t* lane = lv_obj_get_child_by_type(root, 0, &lv_line_class);
    CHECK(lane != nullptr);

    canvas.begin_frame();
    canvas.draw_symbol(tesla_headup::application::SymbolElement{
        .feature = tesla_headup::application::FeatureId::Autopilot,
        .symbol = tesla_headup::application::Symbol::Autopilot,
        .role = tesla_headup::application::SymbolRole::Autopilot,
    });
    CHECK(lv_color_eq(
        lv_obj_get_style_line_color(lane, LV_PART_MAIN),
        lv_color_hex(0x0071FF)));

    canvas.begin_frame();
    CHECK(lv_color_eq(
        lv_obj_get_style_line_color(lane, LV_PART_MAIN),
        lv_color_hex(0xD9E0E3)));
}

std::int32_t lane_displacement_after_100_ms(
    std::string_view speed,
    std::string_view gear = "D",
    std::int32_t line_index = 0, bool full_panel = false) {
    LvglSession session{full_panel ? 800 : 536, full_panel ? 480 : 240};
    if (session.display() == nullptr) {
        return -1;
    }

    tesla_headup::platform::LvglDashboardCanvas canvas;
    if (!canvas.initialize(*session.display())) {
        return -1;
    }

    lv_obj_t* root = dashboard_content(session.display());
    lv_obj_t* line = lv_obj_get_child_by_type(
        root, line_index, &lv_line_class);
    if (line == nullptr) {
        return -1;
    }

    canvas.begin_frame();
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = speed,
        .role = tesla_headup::application::TextRole::PrimaryValue,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = "km/h",
        .role = tesla_headup::application::TextRole::Unit,
    });
    canvas.draw_text(tesla_headup::application::TextElement{
        .text = gear,
        .role = tesla_headup::application::TextRole::Gear,
    });
    const auto before = lv_line_get_points(line)[0].y;
    lv_tick_inc(100);
    (void)lv_timer_handler();
    const auto after = lv_line_get_points(line)[0].y;
    return static_cast<std::int32_t>(after - before);
}

TEST_CASE("road-scene lane motion reverses with selected gear") {
    const auto drive_displacement =
        lane_displacement_after_100_ms("36", "D", 1);
    const auto reverse_displacement =
        lane_displacement_after_100_ms("36", "R", 1);
    const auto park_displacement =
        lane_displacement_after_100_ms("36", "P", 1);
    const auto neutral_displacement =
        lane_displacement_after_100_ms("36", "N", 1);

    CHECK(drive_displacement > 0);
    CHECK(reverse_displacement < 0);
    CHECK_EQ(park_displacement, 0);
    CHECK_EQ(neutral_displacement, 0);
}

TEST_CASE("road-scene lane motion follows physical vehicle speed") {
    const auto stopped_displacement = lane_displacement_after_100_ms("0");
    const auto city_displacement = lane_displacement_after_100_ms("36");
    const auto highway_displacement = lane_displacement_after_100_ms("72");

    CHECK_EQ(stopped_displacement, 0);
    CHECK(city_displacement > 0);
    CHECK(highway_displacement > city_displacement);
}

TEST_CASE("native road scene retains speed-proportional reverse park and neutral motion") {
    CHECK(lane_displacement_after_100_ms("36", "D", 1, true) > 0);
    CHECK(lane_displacement_after_100_ms("36", "R", 1, true) < 0);
    CHECK_EQ(lane_displacement_after_100_ms("36", "P", 1, true), 0);
    CHECK_EQ(lane_displacement_after_100_ms("36", "N", 1, true), 0);
    CHECK_EQ(lane_displacement_after_100_ms("0", "D", 1, true), 0);
    CHECK(lane_displacement_after_100_ms("72", "D", 1, true) >
          lane_displacement_after_100_ms("36", "D", 1, true));
}
#endif
