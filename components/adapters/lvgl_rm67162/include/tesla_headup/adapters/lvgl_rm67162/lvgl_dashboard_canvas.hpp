#pragma once

#include "tesla_headup/ports/dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/lvgl_pixel_orbit.hpp"

#include "lvgl.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tesla_headup::platform {

class LvglDashboardCanvas final : public application::DashboardCanvas {
public:
    LvglDashboardCanvas() = default;
    ~LvglDashboardCanvas() override;

    [[nodiscard]] bool initialize(lv_display_t& display) noexcept;
    [[nodiscard]] bool ready() const noexcept;
    void shutdown() noexcept;

    void begin_frame() override;
    void draw_text(const application::TextElement& element) override;
    void draw_symbol(const application::SymbolElement& element) override;
    void draw_edge_gradient(
        const application::EdgeGradientElement& element) override;
    void end_frame() override;
    void draw_power(const application::PowerBarElement& element) override;
    void draw_battery(const application::BatteryChargeElement& element) override;

private:
    static constexpr std::size_t text_slot_count = 5;
    static constexpr std::size_t image_slot_count = 10;
    static constexpr std::size_t blind_spot_vehicle_count = 2;
    static constexpr std::size_t road_line_count = 12;

    static void road_animation_tick(lv_timer_t* timer);
    void update_road_geometry() noexcept;

    lv_obj_t* root_{nullptr};
    lv_obj_t* status_bar_{nullptr};
    lv_obj_t* status_autopilot_{nullptr};
    LvglPixelOrbit pixel_orbit_{};
    std::array<lv_obj_t*, text_slot_count> text_slots_{};
    std::array<lv_obj_t*, image_slot_count> image_slots_{};
    std::array<lv_obj_t*, blind_spot_vehicle_count>
        blind_spot_vehicle_slots_{};
    lv_obj_t* speed_limit_sign_{nullptr};
    lv_obj_t* power_fill_{nullptr};
    lv_obj_t* power_track_{nullptr};
    std::int16_t power_permille_{0};
    lv_obj_t* battery_group_{nullptr};
    lv_obj_t* battery_fill_{nullptr};
    lv_obj_t* battery_label_{nullptr};
    std::array<lv_obj_t*, road_line_count> road_line_slots_{};
    std::array<std::array<lv_point_precise_t, 2>, road_line_count>
        road_line_points_{};
    std::array<lv_obj_t*, road_line_count> outer_line_slots_{};
    std::array<std::array<lv_point_precise_t, 2>, road_line_count> outer_line_points_{};
    lv_obj_t* road_vehicle_{nullptr};
    lv_timer_t* road_animation_timer_{nullptr};
    std::uint32_t last_animation_tick_{0};
    std::uint32_t road_phase_units_{0};
    std::uint16_t displayed_speed_{0};
    std::uint16_t road_speed_kmh_{0};
    std::uint8_t road_cycle_{0};
    std::int8_t road_direction_{1};
    bool speed_drawn_this_frame_{false};
    std::array<bool, blind_spot_vehicle_count> turn_signal_active_{};
    bool wide_road_scene_{false};
    bool vehicle_assets_retained_{false};
    std::size_t next_image_slot_{0};
};

}  // namespace tesla_headup::platform
