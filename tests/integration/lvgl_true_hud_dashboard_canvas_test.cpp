#include "test.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"
#include "tesla_headup/adapters/dashkit_tesla/dashkit_telemetry_ingestor.hpp"
#include "tesla_headup/application/dashboard/centered_vehicle_speed.hpp"
#include "tesla_headup/application/dashboard/speed_limit_indicator.hpp"
#include "tesla_headup/application/hud_runtime/speed_limit_presenter.hpp"
#include "tesla_headup/application/hud_runtime/power_presenter.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <string_view>

namespace {
using namespace tesla_headup;
struct Session {
    lv_display_t* display;
    platform::LvglDashboardCanvas canvas;
    std::array<std::uint16_t, 536 * 240> pixels{};
    alignas(4) std::array<std::uint16_t, 536 * 8> buffer{};
    Session() {
        lv_init();
        display = lv_display_create(536, 240);
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_user_data(display, this);
        lv_display_set_buffers(display, buffer.data(), nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, [](lv_display_t* display, const lv_area_t* area, std::uint8_t* bytes) {
            auto& session = *static_cast<Session*>(lv_display_get_user_data(display));
            auto* source = reinterpret_cast<const std::uint16_t*>(bytes);
            for (int y = area->y1; y <= area->y2; ++y)
                for (int x = area->x1; x <= area->x2; ++x)
                    session.pixels[y * 536 + x] = *source++;
            lv_display_flush_ready(display);
        });
        CHECK(canvas.initialize(*display));
    }
    ~Session() { canvas.shutdown(); lv_display_delete(display); lv_deinit(); }
    lv_obj_t* root() { return lv_obj_get_child(lv_display_get_screen_active(display), 0); }
    lv_obj_t* label(int slot) { return lv_obj_get_child_by_type(root(), slot, &lv_label_class); }
    lv_obj_t* power_track() {
        for (std::uint32_t i = 0; i < lv_obj_get_child_count(root()); ++i) {
            auto* child = lv_obj_get_child(root(), i);
            if (lv_color_eq(lv_obj_get_style_bg_color(child, 0), lv_color_hex(0x2B3236)))
                return child;
        }
        return nullptr;
    }
    void frame(std::string_view speed) {
        canvas.begin_frame();
        canvas.draw_text({.text=speed, .role=application::TextRole::PrimaryValue});
        canvas.draw_text({.text="km/h", .role=application::TextRole::Unit});
        canvas.draw_text({.text="130", .role=application::TextRole::SpeedLimit});
        canvas.draw_text({.text="D", .role=application::TextRole::Gear});
        canvas.draw_battery({.percent=100});
        canvas.draw_edge_gradient({.edge=application::DisplayEdge::Left});
        canvas.draw_edge_gradient({.edge=application::DisplayEdge::Right});
        canvas.draw_symbol({.symbol=application::Symbol::LeftArrow});
        canvas.draw_symbol({.symbol=application::Symbol::RightArrow});
        canvas.end_frame();
    }
};
}

TEST_CASE("True HUD renders a live DashKit speed limit through the firmware presenter") {
    using namespace std::chrono_literals;
    Session session;
    application::TelemetryStore telemetry;
    dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    application::CenteredVehicleSpeed speed;
    application::SpeedLimitIndicator limit;
    application::Dashboard dashboard{session.canvas};
    CHECK(dashboard.add_feature(speed));
    CHECK(dashboard.add_feature(limit, false));
    application::DashboardRenderScheduler scheduler{dashboard};
    application::SpeedLimitPresenter presenter{telemetry, limit, dashboard, scheduler};
    std::array<std::uint8_t, 19> packet{
        1, 1, 0, 0, 0, 0, 0x99, 0x03, 0, 0, 8,
        0, 10, 0, 0, 0, 0, 0, 0};
    auto* sign = lv_obj_get_child(session.root(), 0);
    CHECK(lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
    auto now = 0ms;
    for (const auto raw : {10U, 26U, 6U}) {
        packet[12] = static_cast<std::uint8_t>(raw);
        now += 500ms;
        CHECK_EQ(ingestor.ingest(packet, now).decoded_speed_limit_frames, 1U);
        CHECK(presenter.refresh(now));
        dashboard.render();
        lv_refr_now(session.display);
        CHECK(!lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
        CHECK(!lv_obj_has_flag(session.label(4), LV_OBJ_FLAG_HIDDEN));
        CHECK_EQ(std::string_view{lv_label_get_text(session.label(4))},
                 raw == 10U ? "50" : raw == 26U ? "130" : "30");
        unsigned red_pixels = 0;
        unsigned white_pixels = 0;
        for (int y = lv_obj_get_y(sign); y < lv_obj_get_y(sign) + 54; ++y)
            for (int x = lv_obj_get_x(sign); x < lv_obj_get_x(sign) + 54; ++x) {
                const auto pixel = session.pixels[y * 536 + x];
                if (pixel == lv_color_to_u16(lv_color_hex(0xE02020))) ++red_pixels;
                if (pixel == 0xFFFF) ++white_pixels;
            }
        CHECK(red_pixels > 400);
        CHECK(white_pixels > 500);
    }
    CHECK(presenter.refresh(10s));
    dashboard.render();
    CHECK(lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(session.label(4), LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("True HUD speed limit stays on screen when speed arrives after startup") {
    Session session;
    // Firmware renders connection status before the first vehicle-speed packet.
    session.canvas.begin_frame();
    session.canvas.draw_text({.text="CONNECTING", .role=application::TextRole::StatusInfo});
    session.canvas.end_frame();
    for (int transition = 0; transition < 3; ++transition) {
        session.frame("0");
        lv_refr_now(session.display);
        auto* sign = lv_obj_get_child(session.root(), 0);
        lv_area_t bounds{};
        lv_obj_get_coords(sign, &bounds);
        CHECK(bounds.x1 >= 35);
        CHECK(bounds.x2 < 501);
        CHECK(bounds.y1 >= 35);
        CHECK(bounds.y2 < 205);
        CHECK(!lv_obj_has_flag(sign, LV_OBJ_FLAG_HIDDEN));
        auto* digits = session.label(4);
        lv_area_t digit_bounds{};
        lv_obj_get_coords(digits, &digit_bounds);
        CHECK(digit_bounds.x1 >= bounds.x1);
        CHECK(digit_bounds.x2 <= bounds.x2);
        CHECK(digit_bounds.y1 >= bounds.y1);
        CHECK(digit_bounds.y2 <= bounds.y2);
        unsigned ring_pixels = 0;
        for (int y = bounds.y1; y <= bounds.y2; ++y)
            for (int x = bounds.x1; x <= bounds.x2; ++x)
                if (session.pixels[y * 536 + x] ==
                    lv_color_to_u16(lv_color_hex(0xE02020))) ++ring_pixels;
        CHECK(ring_pixels > 400);
        auto* unit = session.label(1);
        CHECK(std::abs(bounds.x1 * 2 + lv_obj_get_width(sign) -
                       lv_obj_get_x(unit) * 2 - lv_obj_get_width(unit)) <= 1);
        session.canvas.begin_frame();
        session.canvas.draw_text({.text="50", .role=application::TextRole::SpeedLimit});
        session.canvas.end_frame();
    }
}

TEST_CASE("True HUD keeps all artwork inside the 35 pixel border through the pixel orbit") {
    Session session;
    for (const auto speed : {"0", "9", "88", "130", "400"}) {
        session.frame(speed);
        auto* speed_label = session.label(0);
        auto* unit = session.label(1);
        CHECK(!lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN));
        const auto group_left = lv_obj_get_x(speed_label);
        const auto group_right = lv_obj_get_x(unit) + lv_obj_get_width(unit);
        CHECK(std::abs(group_left + group_right - 536) <= 1);
        CHECK_EQ(lv_obj_get_x(unit) - lv_obj_get_x(speed_label) - lv_obj_get_width(speed_label), 4);
        auto* limit = lv_obj_get_child(session.root(), 0);
        CHECK(std::abs(lv_obj_get_x(limit) * 2 + lv_obj_get_width(limit) -
                       lv_obj_get_x(unit) * 2 - lv_obj_get_width(unit)) <= 1);
        CHECK(lv_obj_get_y(limit) + lv_obj_get_height(limit) <= lv_obj_get_y(unit));
        CHECK_EQ(lv_obj_get_style_text_font(session.label(3), 0), &tesla_headup_inter_medium_18);
        CHECK_EQ(lv_obj_get_style_text_font(session.label(5), 0), &tesla_headup_inter_medium_18);
        auto* left = lv_obj_get_child_by_type(session.root(), 0, &lv_image_class);
        auto* right = lv_obj_get_child_by_type(session.root(), 1, &lv_image_class);
        CHECK_EQ(lv_obj_get_x(left), 37);
        CHECK_EQ(lv_obj_get_x(right) + lv_obj_get_width(right), 499);
        CHECK_EQ(lv_obj_get_y(left), 37);
        CHECK_EQ(lv_obj_get_y(right), 37);
        for (int step = 0; step < 9; ++step) {
            if (step != 0) {
                lv_tick_inc(platform::LvglPixelOrbit::interval_ms);
                lv_timer_handler();
            }
            lv_refr_now(session.display);
            unsigned lit = 0;
            for (int y = 0; y < 240; ++y) {
                for (int x = 0; x < 536; ++x) {
                    if (session.pixels[y * 536 + x] != 0) {
                        CHECK(x >= 35 && x < 501 && y >= 35 && y < 205);
                        ++lit;
                    }
                }
            }
            CHECK(lit > 2000);
        }
    }
}

TEST_CASE("True HUD gives status precedence over gear and clears stale content") {
    Session session;
    session.frame("130");
    const auto objects = lv_obj_get_child_count(session.root());
    session.canvas.draw_text({.text="RECONNECTING", .role=application::TextRole::StatusWarning});
    session.canvas.draw_text({.text="D", .role=application::TextRole::Gear});
    session.canvas.end_frame();
    CHECK(!lv_obj_has_flag(session.label(2), LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(session.label(3), LV_OBJ_FLAG_HIDDEN));
    session.frame("1");
    CHECK(lv_obj_has_flag(session.label(2), LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(session.label(3), LV_OBJ_FLAG_HIDDEN));
    session.canvas.begin_frame();
    session.canvas.draw_text({.text="DISCONNECTED", .role=application::TextRole::StatusCritical});
    session.canvas.draw_symbol({.symbol=application::Symbol::Autopilot});
    session.canvas.draw_power({.permille=900});
    session.canvas.draw_edge_gradient({});
    session.canvas.end_frame();
    CHECK(lv_obj_has_flag(session.label(0), LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_has_flag(session.label(5), LV_OBJ_FLAG_HIDDEN));
    CHECK_EQ(lv_obj_get_child_count(session.root()), objects);
}

TEST_CASE("True HUD horizontal power fills left and recuperation fills right without footer overlap") {
    Session session;
    session.frame("130");
    auto* track = session.power_track();
    CHECK(track != nullptr);
    auto* fill = lv_obj_get_child(track, 0);
    for (const auto level : {500, -500, 2000, -2000, 0}) {
        session.canvas.draw_power({static_cast<std::int16_t>(level)});
        session.canvas.end_frame();
        lv_refr_now(session.display);
        const int half = lv_obj_get_width(track) / 2;
        const int width = (half * std::min(std::abs(level), 1000) + 500) / 1000;
        CHECK(lv_obj_get_x(track) > lv_obj_get_x(session.label(5)) + lv_obj_get_width(session.label(5)));
        CHECK(lv_obj_get_x(track) + lv_obj_get_width(track) < lv_obj_get_x(session.label(3)));
        CHECK_EQ(lv_obj_get_y(track) * 2 + lv_obj_get_height(track), 388);
        CHECK_EQ(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN), level == 0);
        if (level == 0) continue;
        CHECK_EQ(lv_obj_get_width(fill), width);
        CHECK_EQ(lv_obj_get_x(fill), level > 0 ? half - width : half);
        const int x = lv_obj_get_x(track) + half + (level > 0 ? -1 : 1);
        const int y = lv_obj_get_y(track) + 3;
        CHECK_EQ(session.pixels[y * 536 + x],
                 lv_color_to_u16(lv_color_hex(level > 0 ? 0xB8C1C5 : 0x5CBD68)));
    }
    session.canvas.draw_power({-1000});
    session.canvas.draw_text({.text="PAIRING REQUIRED", .role=application::TextRole::StatusCritical});
    session.canvas.end_frame();
    CHECK(lv_obj_get_x(track) + lv_obj_get_width(track) <= lv_obj_get_x(session.label(2)) - 16);
    session.canvas.begin_frame();
    session.canvas.end_frame();
    CHECK(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("True HUD power bar follows BMS utilization and clears stale telemetry") {
    using namespace std::chrono_literals;
    Session session;
    application::TelemetryStore telemetry;
    dashkit::DashKitTelemetryIngestor ingestor{telemetry};
    application::PowerIndicator feature;
    application::Dashboard dashboard{session.canvas};
    CHECK(dashboard.add_feature(feature));
    application::DashboardRenderScheduler scheduler{dashboard};
    application::PowerPresenter presenter{telemetry, feature, scheduler};
    std::array<std::uint8_t, 29> packet{
        2, 0,0,0,0, 1, 0x52,0x02,0,0, 4, 0x40,0x1F,0x00,0x7D,
           0,0,0,0, 1, 0x32,0x01,0,0, 4, 0x40,0x9C,0x60,0x70};
    CHECK_EQ(ingestor.ingest(packet, 0ms).decoded_power_frames, 2U);
    CHECK(presenter.refresh(0ms));
    CHECK(scheduler.tick(0ms));
    auto* track = session.power_track();
    CHECK(track != nullptr);
    auto* fill = lv_obj_get_child(track, 0);
    CHECK(!lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
    CHECK(lv_obj_get_x(fill) < lv_obj_get_width(track) / 2);
    packet[27] = 0xE8; packet[28] = 0x03; // -100 A, half the available regen limit.
    CHECK_EQ(ingestor.ingest(packet, 50ms).decoded_power_frames, 2U);
    CHECK(presenter.refresh(50ms));
    CHECK(scheduler.tick(50ms));
    CHECK_EQ(lv_obj_get_x(fill), lv_obj_get_width(track) / 2);
    CHECK(lv_color_eq(lv_obj_get_style_bg_color(fill, 0), lv_color_hex(0x5CBD68)));
    CHECK(presenter.refresh(551ms));
    CHECK(scheduler.tick(551ms));
    CHECK(lv_obj_has_flag(fill, LV_OBJ_FLAG_HIDDEN));
    CHECK(!lv_obj_has_flag(track, LV_OBJ_FLAG_HIDDEN));
}

TEST_CASE("True HUD uses tabular Inter figures") {
    for (const auto* font : {&tesla_headup_inter_regular_100, &tesla_headup_inter_medium_18,
                            &tesla_headup_inter_medium_24, &tesla_headup_inter_semibold_18}) {
        lv_font_glyph_dsc_t zero{};
        CHECK(lv_font_get_glyph_dsc(font, &zero, '0', 0));
        for (unsigned c = '1'; c <= '9'; ++c) {
            lv_font_glyph_dsc_t digit{};
            CHECK(lv_font_get_glyph_dsc(font, &digit, c, 0));
            CHECK_EQ(digit.adv_w, zero.adv_w);
        }
    }
}

TEST_CASE("True HUD speed and metric keep a fixed vertical position as speed changes") {
    Session session;
    session.frame("0");
    const int speed_y = lv_obj_get_y(session.label(0));
    const int unit_y = lv_obj_get_y(session.label(1));
    for (const auto speed : {"1", "2", "3", "4", "5", "6", "7", "8", "9",
                             "10", "11", "17", "70", "99", "100", "111", "130", "199", "400"}) {
        session.frame(speed);
        CHECK_EQ(lv_obj_get_y(session.label(0)), speed_y);
        CHECK_EQ(lv_obj_get_y(session.label(1)), unit_y);
        auto* unit = session.label(1);
        CHECK(std::abs(lv_obj_get_x(session.label(0)) + lv_obj_get_x(unit) +
                       lv_obj_get_width(unit) - 536) <= 1);
    }
}

TEST_CASE("True HUD limit digits retain three pixels of white radial padding") {
    Session session;
    session.frame("130");
    auto* sign = lv_obj_get_child(session.root(), 0);
    const double center_x = lv_obj_get_x(sign) + 26.5;
    const double center_y = lv_obj_get_y(sign) + 26.5;
    for (unsigned limit = 1; limit <= 150; ++limit) {
        std::array<char, 4> text{};
        std::snprintf(text.data(), text.size(), "%u", limit);
        session.canvas.draw_text({.text=text.data(), .role=application::TextRole::SpeedLimit});
        session.canvas.end_frame();
        lv_refr_now(session.display);
        unsigned digit_pixels = 0;
        for (int y = lv_obj_get_y(sign); y < lv_obj_get_y(sign) + 54; ++y) {
            for (int x = lv_obj_get_x(sign); x < lv_obj_get_x(sign) + 54; ++x) {
                const auto pixel = session.pixels[y * 536 + x];
                const int red = (pixel >> 11) & 31;
                const int green = (pixel >> 5) & 63;
                const int blue = pixel & 31;
                const double radius_squared = (x - center_x) * (x - center_x) +
                                              (y - center_y) * (y - center_y);
                if (pixel != 0xFFFF && red == blue && std::abs(green - 2 * red) <= 1 &&
                    radius_squared < 21.5 * 21.5) {
                    CHECK(radius_squared <= 19.0 * 19.0);
                    ++digit_pixels;
                }
            }
        }
        CHECK(digit_pixels > 5);
    }
}

TEST_CASE("True HUD battery retains its outline at zero and clears unavailable charge") {
    Session session;
    const auto battery_pixels = [&session](bool green_only) {
        lv_refr_now(session.display);
        unsigned count = 0;
        for (int y = 180; y < 205; ++y)
            for (int x = 35; x < 85; ++x) {
                const auto pixel = session.pixels[y * 536 + x];
                if (green_only ? pixel == 0x5DEDU : pixel != 0) ++count;
            }
        return count;
    };
    session.frame("130");
    const auto full = battery_pixels(true);
    CHECK(full > 300);
    session.canvas.draw_battery({.percent=50});
    session.canvas.end_frame();
    const auto half = battery_pixels(true);
    CHECK(half > full / 3 && half < full * 2 / 3);
    for (const auto percent : {19U, 20U, 21U, 20U, 51U}) {
        session.canvas.draw_battery({static_cast<std::uint8_t>(percent)});
        session.canvas.end_frame();
        lv_refr_now(session.display);
        CHECK_EQ(session.pixels[190 * 536 + 40],
                 lv_color_to_u16(lv_color_hex(percent <= 20 ? 0xFFB000 : 0x5CBD68)));
    }
    session.canvas.draw_battery({.percent=0});
    session.canvas.end_frame();
    CHECK_EQ(battery_pixels(true), 0U);
    CHECK(battery_pixels(false) > 100);
    session.canvas.begin_frame();
    session.canvas.draw_battery({.percent=255});
    session.canvas.end_frame();
    CHECK_EQ(battery_pixels(false), 0U);
}

TEST_CASE("True HUD blind-spot icons track occupancy and matching turn intent independent of draw order") {
    Session session;
    const auto objects = lv_obj_get_child_count(session.root());
    for (unsigned occupied = 0; occupied < 4; ++occupied) {
        for (unsigned signals = 0; signals < 4; ++signals) {
            for (const bool signals_first : {false, true}) {
                session.canvas.begin_frame();
                const auto draw_signals = [&] {
                    for (unsigned side = 0; side < 2; ++side)
                        if ((signals & (1U << side)) != 0)
                            session.canvas.draw_symbol({
                                .symbol=side == 0 ? application::Symbol::LeftArrow : application::Symbol::RightArrow,
                                .illuminated=false});
                };
                if (signals_first) draw_signals();
                for (unsigned side = 0; side < 2; ++side)
                    if ((occupied & (1U << side)) != 0)
                        session.canvas.draw_edge_gradient({
                            .edge=side == 0 ? application::DisplayEdge::Left : application::DisplayEdge::Right});
                if (!signals_first) draw_signals();
                session.canvas.end_frame();
                for (unsigned side = 0; side < 2; ++side) {
                    auto* icon = lv_obj_get_child_by_type(session.root(), 2 + side, &lv_image_class);
                    CHECK_EQ(lv_obj_has_flag(icon, LV_OBJ_FLAG_HIDDEN), (occupied & (1U << side)) == 0);
                    CHECK_EQ(lv_obj_get_style_image_recolor_opa(icon, 0),
                             (signals & (1U << side)) != 0 ? LV_OPA_COVER : LV_OPA_TRANSP);
                    CHECK_EQ(lv_obj_get_x(icon), side == 0 ? 37 : 459);
                    CHECK_EQ(lv_obj_get_width(icon), 40);
                    CHECK_EQ(lv_obj_get_height(icon), 40);
                    CHECK(lv_obj_get_y(icon) > 67);
                    CHECK(lv_obj_get_y(icon) + lv_obj_get_height(icon) < 185);
                }
                CHECK_EQ(lv_obj_get_child_count(session.root()), objects);
            }
        }
    }
}
