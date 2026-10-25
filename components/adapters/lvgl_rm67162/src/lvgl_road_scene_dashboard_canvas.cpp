#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/indicator_images.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"
#include "src/draw/lv_draw_private.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <span>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif

namespace tesla_headup::platform {
namespace {

constexpr auto background = 0x000000U;
constexpr auto foreground = 0xF4F7F8U;
constexpr auto secondary_foreground = 0xB8C1C5U;
constexpr auto lane_foreground = 0xD9E0E3U;
// Dominant blue in the RGB565A8 Autopilot asset (RGB565 0x039F).
constexpr auto autopilot_blue = 0x0071FFU;
constexpr auto status_warning = 0xFFB000U;
constexpr auto status_critical = 0xFF453AU;
constexpr auto blind_spot_red = 0xFF0000U;
constexpr auto speed_limit_red = 0xE02020U;
constexpr auto speed_limit_white = 0xFFFFFFU;
constexpr auto speed_limit_black = 0x000000U;
constexpr std::int32_t speed_limit_diameter = 62;
constexpr std::int32_t speed_limit_border_width = 7;

std::size_t text_slot(application::TextRole role) noexcept {
    switch (role) {
    case application::TextRole::PrimaryValue:
        return 0;
    case application::TextRole::Unit:
        return 1;
    case application::TextRole::StatusInfo:
    case application::TextRole::StatusWarning:
    case application::TextRole::StatusCritical:
        return 2;
    case application::TextRole::Gear:
        return 3;
    case application::TextRole::SpeedLimit:
        return 4;
    }
    return 0;
}
constexpr auto speed_rail_marker = 0x2B3236U;
constexpr std::int32_t driving_height = 408; // 85% of the native 480 px panel.
constexpr std::int32_t status_height = 72;
constexpr std::int32_t compact_driving_height = 204;
constexpr std::int32_t compact_status_height = 36;
// Match the 30 Hz LVGL refresh budget instead of rebuilding geometry at 60 Hz.
// Motion remains distance/time based, including when frames run late.
constexpr std::uint32_t animation_period_ms = 33;
constexpr std::uint32_t lane_cycle_units = 43'200;
constexpr std::size_t lane_rows_per_side = 4;
constexpr std::uint32_t complete_road_cycle_units =
    lane_cycle_units * lane_rows_per_side;

struct LineSpec {
    std::array<lv_point_precise_t, 2> points;
    std::int32_t width;
    std::uint32_t color;
};

constexpr std::array legacy_road_lines{
    LineSpec{{lv_point_precise_t{240, 0}, lv_point_precise_t{236, 19}},
             2,
             lane_foreground},
    LineSpec{{lv_point_precise_t{234, 34}, lv_point_precise_t{227, 70}},
             3,
             lane_foreground},
    LineSpec{{lv_point_precise_t{222, 94}, lv_point_precise_t{213, 139}},
             4,
             lane_foreground},
    LineSpec{{lv_point_precise_t{208, 166}, lv_point_precise_t{198, 220}},
             6,
             lane_foreground},
    LineSpec{{lv_point_precise_t{296, 0}, lv_point_precise_t{300, 19}},
             2,
             lane_foreground},
    LineSpec{{lv_point_precise_t{302, 34}, lv_point_precise_t{309, 70}},
             3,
             lane_foreground},
    LineSpec{{lv_point_precise_t{314, 94}, lv_point_precise_t{323, 139}},
             4,
             lane_foreground},
    LineSpec{{lv_point_precise_t{328, 166}, lv_point_precise_t{338, 220}},
             6,
             lane_foreground},
};

// Every boundary is a ray from the same VP to its own bottom anchor.
constexpr int project_lane_x(bool wide, bool outer, bool right, int y) {
    const int vp_x = wide ? 400 : 268;
    // Keep the shared apex above the visible road to separate all four tips.
    const int vp_y = wide ? 64 : 32;
    const int height = wide ? driving_height : compact_driving_height;
    const int inner_half = wide ? 96 : 69;
    // Quantize the common depth offset once: boundaries at -3d, -d, +d,
    // +3d have identical 2d lane widths even at integer pixel coordinates.
    const int depth_offset = static_cast<int>(static_cast<std::int64_t>(inner_half) *
        (y - vp_y) / (height - vp_y));
    return vp_x + (right ? 1 : -1) * (outer ? 3 : 1) * depth_offset;
}

// One depth treatment for blue and gray markings, also during animation.
void style_road_line(lv_obj_t* line, bool wide, int near_y) {
    const int vp_y = wide ? 64 : 32;
    const int height = wide ? driving_height : compact_driving_height;
    const int depth = std::clamp(near_y - vp_y, 0, height - vp_y);
    const int width = 1 + depth * (wide ? 7 : 5) / (height - vp_y);
    const auto opacity = static_cast<lv_opa_t>(64 + depth * 191 / (height - vp_y));
    lv_obj_set_style_line_width(line, width, LV_PART_MAIN);
    lv_obj_set_style_line_opa(line, opacity, LV_PART_MAIN);
}

// Rasterize a stationary full ray, revealing only the moving dash window.
// Moving rounded endpoints made LVGL choose a slightly different raster slope
// every time either endpoint crossed a pixel, especially on the outer lanes.
struct RoadRay { bool wide; bool outer; bool right; };
std::array<RoadRay, 8> road_rays{{
    {false,false,false}, {false,false,true}, {false,true,false}, {false,true,true},
    {true,false,false}, {true,false,true}, {true,true,false}, {true,true,true}}};

void draw_stationary_road_ray(lv_event_t* event) {
    auto* task = static_cast<lv_draw_task_t*>(lv_event_get_param(event));
    auto* original = lv_draw_task_get_line_dsc(task);
    if (original == nullptr) return;
    const auto& ray = *static_cast<const RoadRay*>(lv_event_get_user_data(event));
    auto* object = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
    lv_area_t bounds;
    lv_obj_get_coords(object, &bounds);
    const int vp_x = ray.wide ? 400 : 268, vp_y = ray.wide ? 64 : 32;
    const int height = ray.wide ? driving_height : compact_driving_height;
    const int first_y = static_cast<int>(original->p1.y);
    const int last_y = static_cast<int>(original->p2.y);
    const auto color = original->color;
    original->opa = LV_OPA_TRANSP;
    auto* layer = original->base.layer;
    const auto saved_clip = layer->_clip_area;
    lv_area_t clip = task->clip_area;
    clip.y1 = std::max<std::int32_t>(clip.y1, first_y);
    clip.y2 = std::min<std::int32_t>(clip.y2, last_y);
    if (clip.y1 > clip.y2) return;
    layer->_clip_area = clip;

    // A single full-depth triangle gives each edge one continuous AA mask.
    // Clip only the dash window: geometry and gradient stay fixed as it moves.
    // Separate constant-width bands created visible shoulders along the ray.
    const int bottom_x = bounds.x1 + project_lane_x(ray.wide, ray.outer, ray.right, height);
    const int half_width = ray.wide ? 4 : 3;
    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.p[0] = {static_cast<lv_value_precise_t>(bounds.x1 + vp_x),
                     static_cast<lv_value_precise_t>(bounds.y1 + vp_y)};
    triangle.p[1] = {static_cast<lv_value_precise_t>(bottom_x - half_width),
                     static_cast<lv_value_precise_t>(bounds.y1 + height)};
    triangle.p[2] = {static_cast<lv_value_precise_t>(bottom_x + half_width),
                     static_cast<lv_value_precise_t>(bounds.y1 + height)};
    triangle.color = color;
    triangle.opa = LV_OPA_COVER;
    triangle.grad.dir = LV_GRAD_DIR_VER;
    triangle.grad.stops_count = 2;
    triangle.grad.stops[0] = {color, 64, 0};
    triangle.grad.stops[1] = {color, LV_OPA_COVER, 255};
    lv_draw_triangle(layer, &triangle);
    layer->_clip_area = saved_clip;
}

void attach_stationary_ray(lv_obj_t* line, bool wide, bool outer, bool right) {
    lv_obj_add_flag(line, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(line, draw_stationary_road_ray, LV_EVENT_DRAW_TASK_ADDED,
        &road_rays[(wide ? 4 : 0) + (outer ? 2 : 0) + (right ? 1 : 0)]);
}

constexpr auto road_lines = [] {
    auto lines = legacy_road_lines;
    for (std::size_t index = 0; index < lines.size(); ++index)
        for (auto& point : lines[index].points) {
            point.y = 62 + point.y * 126 / 240;
            point.x = project_lane_x(false, false, index >= 4, point.y);
        }
    return lines;
}();

constexpr std::array legacy_road_exit_lines{
    LineSpec{{lv_point_precise_t{193, 246}, lv_point_precise_t{184, 316}},
             8,
             lane_foreground},
    LineSpec{{lv_point_precise_t{343, 246}, lv_point_precise_t{352, 316}},
             8,
             lane_foreground},
};

constexpr auto road_exit_lines = [] {
    auto lines = legacy_road_exit_lines;
    for (std::size_t index = 0; index < lines.size(); ++index)
        for (auto& point : lines[index].points) {
            point.y = 62 + point.y * 126 / 240;
            point.x = project_lane_x(false, false, index != 0, point.y);
        }
    return lines;
}();

// Lower chase-camera horizon leaves the upper field clear for driving data.
// Six markings per side. Both endpoints use the same straight perspective
// boundary; interpolation therefore cannot kink the lane as markings move.
constexpr LineSpec wide_line(int side, int y, int length, int width) {
    const auto bottom = 124 + (y + length) * 252 / 480;
    y = 124 + y * 252 / 480;
    length = bottom - y;
    const auto x = [side](int row) { return project_lane_x(true, false, side != 0, row); };
    return {{{lv_point_precise_t{static_cast<lv_value_precise_t>(x(y)), static_cast<lv_value_precise_t>(y)},
              lv_point_precise_t{static_cast<lv_value_precise_t>(x(y + length)), static_cast<lv_value_precise_t>(y + length)}}},
            width, lane_foreground};
}
constexpr auto wide_road_lines = [] {
    std::array<LineSpec, 12> result{};
    constexpr std::array y{0, 44, 105, 181, 276, 394};
    constexpr std::array length{17, 24, 33, 44, 58, 74};
    for (int side = 0; side < 2; ++side)
        for (int row = 0; row < 6; ++row)
            result[side * 6 + row] = wide_line(side, y[row], length[row], row + 2);
    return result;
}();
constexpr std::array wide_exit_lines{wide_line(0, 535, 90, 9), wide_line(1, 535, 90, 9)};

const lv_image_dsc_t& vehicle_image(bool wide, int side, bool red, bool parked = false) {
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    (void)wide;
#else
#ifndef ESP_PLATFORM
    if (!wide)
#endif
    {
        (void)wide;
        if (side < 0) return parked ? assets::road_scene_park_image : assets::road_scene_vehicle_image;
        return side == 0 ? (red ? assets::blind_spot_left_image : assets::blind_spot_left_grey_image)
                         : (red ? assets::blind_spot_right_image : assets::blind_spot_right_grey_image);
    }
#endif
#if !defined(ESP_PLATFORM) || CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    if (side < 0) return parked ? assets::road_scene_park_wide_image : assets::road_scene_vehicle_wide_image;
    return side == 0 ? (red ? assets::blind_spot_left_wide_image : assets::blind_spot_left_grey_wide_image)
                     : (red ? assets::blind_spot_right_wide_image : assets::blind_spot_right_grey_wide_image);
#endif
}

bool retain_vehicle_images(bool wide) {
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    (void)wide;
    return assets::retain_road_scene_wide_images();
#elif defined(ESP_PLATFORM)
    (void)wide;
    return assets::retain_road_scene_compact_images();
#else
    return wide ? assets::retain_road_scene_wide_images() : assets::retain_road_scene_compact_images();
#endif
}

void release_vehicle_images(bool wide) {
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    (void)wide;
    assets::release_road_scene_wide_images();
#elif defined(ESP_PLATFORM)
    (void)wide;
    assets::release_road_scene_compact_images();
#else
    if (wide) assets::release_road_scene_wide_images();
    else assets::release_road_scene_compact_images();
#endif
}

const lv_font_t* value_font(bool wide, bool limit) {
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    (void)wide;
    return limit ? &tesla_headup_inter_semibold_40 : &tesla_headup_inter_regular_110;
#else
#ifndef ESP_PLATFORM
    if (wide) return limit ? &tesla_headup_inter_semibold_40 : &tesla_headup_inter_regular_110;
#endif
    (void)wide;
    return limit ? &tesla_headup_inter_semibold_24 : &tesla_headup_inter_regular_64;
#endif
}
// Use painted glyph bounds rather than the font's invisible line padding.
std::array<int, 2> text_ink_bounds(const lv_font_t* font, std::string_view text) {
    int top = font->line_height, bottom = 0;
    for (const unsigned char character : text) {
        lv_font_glyph_dsc_t glyph{};
        if (!lv_font_get_glyph_dsc(font, &glyph, character, 0) || glyph.box_h == 0) continue;
        const int end = font->line_height - font->base_line - glyph.ofs_y;
        top = std::min(top, end - glyph.box_h);
        bottom = std::max(bottom, end);
    }
    return {top, bottom};
}

int power_rail_top(bool wide) {
    return (wide ? 24 : 12) + text_ink_bounds(value_font(wide, false), "0")[0];
}
int power_rail_half(bool wide) {
    return ((wide ? 336 : 168) - power_rail_top(wide)) / 2;
}


std::int32_t interpolate(
    std::int32_t from,
    std::int32_t to,
    std::uint32_t phase) noexcept {
    const auto difference = static_cast<std::int64_t>(to) - from;
    return static_cast<std::int32_t>(
        from + ((difference * phase) / lane_cycle_units));
}

std::uint16_t parse_speed(std::string_view text) noexcept {
    std::uint16_t result = 0;
    for (const char character : text) {
        if (character < '0' || character > '9') {
            break;
        }
        result = static_cast<std::uint16_t>(
            result * 10U + static_cast<unsigned>(character - '0'));
    }
    return result;
}

std::uint16_t miles_per_hour_to_kilometres_per_hour(
    std::uint16_t miles_per_hour) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(miles_per_hour) * 1'609'344U +
         500'000U) /
        1'000'000U);
}

void prepare_object(lv_obj_t& object) {
    lv_obj_remove_flag(&object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(&object, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(&object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(&object, 0, LV_PART_MAIN);
}

void hide(lv_obj_t* object) {
    if (object != nullptr) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

void show(lv_obj_t& object) {
    lv_obj_remove_flag(&object, LV_OBJ_FLAG_HIDDEN);
}

void set_image_source(lv_obj_t* image, const lv_image_dsc_t* source) {
    if (lv_image_get_src(image) != source) lv_image_set_src(image, source);
}

lv_obj_t* create_box(
    lv_obj_t& parent,
    std::int32_t x,
    std::int32_t y,
    std::int32_t width,
    std::int32_t height,
    std::uint32_t color,
    std::int32_t radius = 0) {
    lv_obj_t* box = lv_obj_create(&parent);
    if (box == nullptr) {
        return nullptr;
    }
    prepare_object(*box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(box, radius, LV_PART_MAIN);
    return box;
}

lv_obj_t* create_region(lv_obj_t& parent, int y, int height, int width) {
    auto* region = lv_obj_create(&parent);
    if (region == nullptr) return nullptr;
    lv_obj_remove_style_all(region);
    prepare_object(*region);
    lv_obj_remove_flag(region, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(region, 0, y);
    lv_obj_set_size(region, width, height);
    lv_obj_set_style_bg_opa(region, LV_OPA_TRANSP, LV_PART_MAIN);
    return region;
}

// The outer boundaries remain neutral even when Autopilot colors the ego lane.
// Adjacent lanes have the same projected width as the ego lane at every depth.
void outer_geometry(const std::array<lv_point_precise_t, 2>& inner,
                    std::array<lv_point_precise_t, 2>& outer, bool wide, bool right) {
    for (std::size_t i = 0; i < outer.size(); ++i) {
        outer[i] = inner[i];
        outer[i].x = project_lane_x(wide, true, right, outer[i].y);
    }
}

bool create_road_scene(
    lv_obj_t& root,
    std::array<lv_obj_t*, 12>& line_slots,
    std::array<std::array<lv_point_precise_t, 2>, 12>& line_points,
    std::array<lv_obj_t*, 12>& outer_slots,
    std::array<std::array<lv_point_precise_t, 2>, 12>& outer_points, bool wide) {
    const auto lines = wide ? std::span<const LineSpec>{wide_road_lines} : std::span<const LineSpec>{road_lines};
    for (std::size_t index = 0; index < lines.size(); ++index) {
        const LineSpec& spec = lines[index];
        lv_obj_t* line = lv_line_create(&root);
        if (line == nullptr) {
            return false;
        }
        line_slots[index] = line;
        attach_stationary_ray(line, wide, false, index >= lines.size() / 2);
        line_points[index] = spec.points;
        lv_line_set_points(
            line,
            line_points[index].data(),
            line_points[index].size());
        lv_obj_set_style_line_color(
            line, lv_color_hex(spec.color), LV_PART_MAIN);
        style_road_line(line, wide, line_points[index][1].y);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    }

    for (std::size_t index = 0; index < lines.size(); ++index) {
        auto* line = lv_line_create(&root);
        if (line == nullptr) return false;
        outer_slots[index] = line;
        attach_stationary_ray(line, wide, true, index >= lines.size() / 2);
        outer_geometry(line_points[index], outer_points[index], wide, index >= lines.size() / 2);
        lv_line_set_points(line, outer_points[index].data(), 2);
        lv_obj_set_style_line_color(line, lv_color_hex(0x515D63), LV_PART_MAIN);
        style_road_line(line, wide, outer_points[index][1].y);
        lv_obj_set_style_line_rounded(line, true, LV_PART_MAIN);
    }

    // Neutral power track; the retained fill grows from its center.
    if (create_box(root, wide ? 10 : 8, power_rail_top(wide), 5,
                   power_rail_half(wide) * 2, speed_rail_marker, 3) == nullptr) {
        return false;
    }
    return true;
}

const lv_image_dsc_t* symbol_image(application::Symbol symbol, bool wide) {
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
    (void)wide;
    if (symbol == application::Symbol::Autopilot) return &assets::autopilot_road_scene_wide_image;
#elif !defined(ESP_PLATFORM)
    if (wide && symbol == application::Symbol::Autopilot) return &assets::autopilot_road_scene_wide_image;
#else
    (void)wide;
#endif
    switch (symbol) {
    case application::Symbol::LeftArrow:
        return &assets::turn_left_image;
    case application::Symbol::RightArrow:
        return &assets::turn_right_image;
    case application::Symbol::Autopilot:
#if defined(ESP_PLATFORM) && CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE
        return &assets::autopilot_road_scene_wide_image;
#else
        return &assets::autopilot_road_scene_image;
#endif
    }
    return nullptr;
}

std::array<std::int16_t, 2> symbol_position(
    const application::SymbolElement& element) {
    switch (element.role) {
    case application::SymbolRole::TurnSignal:
        return {
            static_cast<std::int16_t>(
                element.symbol == application::Symbol::LeftArrow ? -45 : 45),
            -92};
    case application::SymbolRole::Autopilot:
        return {-200, 35};
    }
    return {element.horizontal_offset, element.vertical_offset};
}

}  // namespace

LvglDashboardCanvas::~LvglDashboardCanvas() {
    shutdown();
}

void LvglDashboardCanvas::shutdown() noexcept {
    if (road_animation_timer_ != nullptr) {
        lv_timer_delete(road_animation_timer_);
        road_animation_timer_ = nullptr;
    }
    pixel_orbit_.shutdown();
    if (vehicle_assets_retained_) release_vehicle_images(wide_road_scene_);
    vehicle_assets_retained_ = false;
    road_line_slots_.fill(nullptr);
    outer_line_slots_.fill(nullptr);
    road_phase_units_ = 0;
    road_cycle_ = 0;
    root_ = nullptr;
    status_bar_ = nullptr;
    status_autopilot_ = nullptr;
    speed_limit_sign_ = nullptr;
    power_fill_ = nullptr;
    battery_group_ = nullptr;
    battery_fill_ = nullptr;
    battery_label_ = nullptr;
}

void LvglDashboardCanvas::road_animation_tick(lv_timer_t* timer) {
    auto* canvas = static_cast<LvglDashboardCanvas*>(
        lv_timer_get_user_data(timer));
    if (canvas == nullptr) {
        return;
    }

    const std::uint32_t now = lv_tick_get();
    const std::uint32_t elapsed = now - canvas->last_animation_tick_;
    canvas->last_animation_tick_ = now;
    if (canvas->road_speed_kmh_ == 0 || canvas->road_direction_ == 0 ||
        elapsed == 0) {
        return;
    }

    // speed[km/h] * time[ms] / 3600 is travelled distance in metres.
    // Keeping the numerator avoids floating point: 43,200 units are the
    // 12 metres occupied by one 6 m marking and its 6 m gap.
    const std::uint64_t travelled_units =
        static_cast<std::uint64_t>(canvas->road_speed_kmh_) * elapsed;
    const std::uint64_t current_units =
        static_cast<std::uint64_t>(canvas->road_cycle_) * lane_cycle_units +
        canvas->road_phase_units_;
    const auto total_cycle = canvas->wide_road_scene_ ? lane_cycle_units * 6U : complete_road_cycle_units;
    const std::uint64_t delta = travelled_units % total_cycle;
    const std::uint64_t next_units = canvas->road_direction_ > 0
                                         ? (current_units + delta) %
                                               total_cycle
                                         : (current_units +
                                            total_cycle - delta) % total_cycle;
    canvas->road_cycle_ = static_cast<std::uint8_t>(
        next_units / lane_cycle_units);
    canvas->road_phase_units_ = static_cast<std::uint32_t>(
        next_units % lane_cycle_units);
    canvas->update_road_geometry();
}

void LvglDashboardCanvas::update_road_geometry() noexcept {
    const std::size_t rows = wide_road_scene_ ? 6 : lane_rows_per_side;
    for (std::size_t side = 0; side < 2; ++side) {
        for (std::size_t identity = 0;
             identity < rows;
             ++identity) {
            const std::size_t row =
                (identity + road_cycle_) % rows;
            const std::size_t source_index =
                side * rows + row;
            const auto lines = wide_road_scene_ ? std::span<const LineSpec>{wide_road_lines} : std::span<const LineSpec>{road_lines};
            const LineSpec& source = lines[source_index];
            const LineSpec& destination =
                row + 1 < rows
                    ? lines[source_index + 1]
                    : (wide_road_scene_ ? wide_exit_lines[side] : road_exit_lines[side]);
            const std::size_t slot =
                side * rows + identity;
            auto& points = road_line_points_[slot];
            for (std::size_t point = 0; point < points.size(); ++point) {
                points[point].x = static_cast<lv_value_precise_t>(interpolate(
                    source.points[point].x,
                    destination.points[point].x,
                    road_phase_units_));
                points[point].y = static_cast<lv_value_precise_t>(interpolate(
                    source.points[point].y,
                    destination.points[point].y,
                    road_phase_units_));
            }

            for (auto& point : points)
                point.x = project_lane_x(wide_road_scene_, false, side != 0, point.y);

            lv_obj_t* line = road_line_slots_[slot];
            if (line == nullptr) {
                continue;
            }
            lv_line_set_points(line, points.data(), points.size());
            outer_geometry(points, outer_line_points_[slot], wide_road_scene_, side != 0);
            lv_line_set_points(outer_line_slots_[slot], outer_line_points_[slot].data(), 2);
            style_road_line(line, wide_road_scene_, points[1].y);
            style_road_line(outer_line_slots_[slot], wide_road_scene_, points[1].y);
        }
    }
}

bool LvglDashboardCanvas::initialize(lv_display_t& display) noexcept {
    shutdown();
    wide_road_scene_ = lv_display_get_horizontal_resolution(&display) == 800 &&
                       lv_display_get_vertical_resolution(&display) == 480;
    if (!retain_vehicle_images(wide_road_scene_)) return false;
    vehicle_assets_retained_ = true;
    root_ = pixel_orbit_.initialize(display);
    if (root_ == nullptr) {
        return false;
    }

    {
        auto* composition = root_;
        const auto main_height = wide_road_scene_ ? driving_height : compact_driving_height;
        const auto footer_height = wide_road_scene_ ? status_height : compact_status_height;
        const auto width = wide_road_scene_ ? 800 : 536;
        root_ = create_region(*composition, 0, main_height, width);
        status_bar_ = create_region(*composition, main_height, footer_height, width);
        if (root_ == nullptr || status_bar_ == nullptr) return false;
        status_autopilot_ = lv_image_create(status_bar_);
        if (status_autopilot_ == nullptr) return false;
        hide(status_autopilot_);
    }

    prepare_object(*root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_TRANSP, LV_PART_MAIN);
    if (!create_road_scene(
            *root_, road_line_slots_, road_line_points_,
            outer_line_slots_, outer_line_points_, wide_road_scene_)) {
        return false;
    }

    power_fill_ = create_box(*root_, 18, 120, 5, 1, secondary_foreground, 2);
    if (power_fill_ == nullptr) {
        return false;
    }
    hide(power_fill_);

    {
        battery_group_ = create_box(*status_bar_, wide_road_scene_ ? 24 : 12,
            wide_road_scene_ ? 24 : 6, 110, 24, background);
        if (battery_group_ == nullptr) return false;
        lv_obj_set_style_bg_opa(battery_group_, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_align(battery_group_, LV_ALIGN_LEFT_MID, wide_road_scene_ ? 24 : 12, 0);
        auto* body = create_box(*battery_group_, 0, 5, 44, 14, 0x7C8589, 2);
        if (body == nullptr ||
            create_box(*body, 2, 2, 40, 10, 0x252A2D, 1) == nullptr ||
            create_box(*battery_group_, 44, 9, 3, 6, 0x7C8589, 1) == nullptr) return false;
        battery_fill_ = create_box(*body, 2, 2, 40, 10, 0x5CBD68, 1);
        battery_label_ = lv_label_create(battery_group_);
        if (battery_fill_ == nullptr || battery_label_ == nullptr) return false;
        lv_obj_set_style_text_font(battery_label_, &tesla_headup_inter_medium_18, LV_PART_MAIN);
        lv_obj_set_style_text_color(battery_label_, lv_color_hex(secondary_foreground), LV_PART_MAIN);
        lv_obj_align(battery_label_, LV_ALIGN_LEFT_MID, 58, 0);
        hide(battery_group_);
    }

    speed_limit_sign_ = lv_obj_create(root_);
    if (speed_limit_sign_ == nullptr) {
        return false;
    }
    prepare_object(*speed_limit_sign_);
    lv_obj_set_size(
        speed_limit_sign_, wide_road_scene_ ? 96 : speed_limit_diameter,
        wide_road_scene_ ? 96 : speed_limit_diameter);
    lv_obj_set_style_radius(
        speed_limit_sign_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        speed_limit_sign_, lv_color_hex(speed_limit_white), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(speed_limit_sign_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        speed_limit_sign_, lv_color_hex(speed_limit_red), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        speed_limit_sign_, wide_road_scene_ ? 10 : speed_limit_border_width, LV_PART_MAIN);
    hide(speed_limit_sign_);

    for (std::size_t index = 0; index < text_slots_.size(); ++index) {
        auto*& text = text_slots_[index];
        auto* parent = (index == 2 || index == 3) ? status_bar_ : root_;
        text = lv_label_create(parent);
        if (text == nullptr) {
            return false;
        }
        lv_obj_set_style_text_color(
            text, lv_color_hex(foreground), LV_PART_MAIN);
        hide(text);
    }

    for (lv_obj_t*& image : image_slots_) {
        image = lv_image_create(root_);
        if (image == nullptr) {
            return false;
        }
        hide(image);
    }
    for (std::size_t index = 0;
         index < blind_spot_vehicle_slots_.size();
         ++index) {
        lv_obj_t*& vehicle = blind_spot_vehicle_slots_[index];
        vehicle = lv_image_create(root_);
        if (vehicle == nullptr) {
            return false;
        }
        lv_image_set_src(
            vehicle,
            &vehicle_image(wide_road_scene_, static_cast<int>(index), false));
        hide(vehicle);
    }
    road_vehicle_ = lv_image_create(root_);
    if (road_vehicle_ == nullptr) {
        return false;
    }
    lv_image_set_src(road_vehicle_, &vehicle_image(wide_road_scene_, -1, false));
    lv_obj_align(road_vehicle_, LV_ALIGN_CENTER, 0, wide_road_scene_ ? 92 : 46);
    last_animation_tick_ = lv_tick_get();
    road_animation_timer_ = lv_timer_create(
        road_animation_tick,
        animation_period_ms,
        this);
    if (road_animation_timer_ == nullptr) {
        return false;
    }
    return true;
}

bool LvglDashboardCanvas::ready() const noexcept {
    return root_ != nullptr;
}

void LvglDashboardCanvas::begin_frame() {
    turn_signal_active_.fill(false);
    hide(battery_group_);
    hide(power_fill_);
    hide(status_autopilot_);
    next_image_slot_ = 0;
    speed_drawn_this_frame_ = false;
    // Preserve the original forward-motion fallback if gear presentation is
    // disabled. A rendered P/N/R/D observation overrides it below.
    road_direction_ = 1;
    for (lv_obj_t* line : road_line_slots_) {
        if (line != nullptr) {
            lv_obj_set_style_line_color(
                line, lv_color_hex(lane_foreground), LV_PART_MAIN);
        }
    }
    for (lv_obj_t* object : text_slots_) {
        hide(object);
    }
    for (lv_obj_t* object : image_slots_) {
        hide(object);
    }
    for (lv_obj_t* object : blind_spot_vehicle_slots_) {
        hide(object);
    }
    hide(speed_limit_sign_);
}

void LvglDashboardCanvas::draw_text(
    const application::TextElement& element) {
    lv_obj_t& label = *text_slots_[text_slot(element.role)];
    const std::array<char, 24> storage = [&element] {
        std::array<char, 24> result{};
        const auto count = std::min(element.text.size(), result.size() - 1);
        std::copy_n(element.text.begin(), count, result.begin());
        return result;
    }();
    lv_label_set_text(&label, storage.data());
    const bool primary = element.role == application::TextRole::PrimaryValue;
    if (primary) {
        speed_drawn_this_frame_ = true;
        displayed_speed_ = parse_speed(element.text);
        road_speed_kmh_ = displayed_speed_;
    } else if (element.role == application::TextRole::Unit) {
        road_speed_kmh_ =
            element.text == "mph"
                ? miles_per_hour_to_kilometres_per_hour(displayed_speed_)
                : displayed_speed_;
    } else if (element.role == application::TextRole::Gear) {
        if (element.text == "D") {
            road_direction_ = 1;
        } else if (element.text == "R") {
            road_direction_ = -1;
        } else {
            road_direction_ = 0;
        }
    }
    const lv_font_t* font =
        primary ? value_font(wide_road_scene_, false)
                : element.role == application::TextRole::Unit ? &tesla_headup_inter_medium_16
                : element.role == application::TextRole::SpeedLimit ? value_font(wide_road_scene_, true)
                : (element.role == application::TextRole::Gear ||
                   element.role == application::TextRole::SpeedLimit)
                      ? (wide_road_scene_ ? &tesla_headup_inter_medium_24 : &tesla_headup_inter_medium_18)
                      : &tesla_headup_inter_medium_18;
    const auto color = [role = element.role] {
        switch (role) {
        case application::TextRole::PrimaryValue:
            return foreground;
        case application::TextRole::Unit:
        case application::TextRole::StatusInfo:
            return secondary_foreground;
        case application::TextRole::StatusWarning:
            return status_warning;
        case application::TextRole::StatusCritical:
            return status_critical;
        case application::TextRole::Gear:
            return foreground;
        case application::TextRole::SpeedLimit:
            return speed_limit_black;
        }
        return secondary_foreground;
    }();
    lv_obj_set_style_text_font(&label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(&label, lv_color_hex(color), LV_PART_MAIN);

    switch (element.role) {
    case application::TextRole::PrimaryValue:
        lv_obj_align(&label, LV_ALIGN_TOP_LEFT, wide_road_scene_ ? 34 : 24,
                     wide_road_scene_ ? 24 : 12);
        break;
    case application::TextRole::Unit: {
        lv_obj_update_layout(root_);
        const lv_obj_t& speed = *text_slots_.front();
        constexpr std::int32_t unit_gap = 3;
        lv_obj_set_pos(
            &label,
            lv_obj_get_x(&speed) + lv_obj_get_width(&speed) + unit_gap,
            lv_obj_get_y(&speed) +
                text_ink_bounds(value_font(wide_road_scene_, false), lv_label_get_text(&speed))[1] -
                text_ink_bounds(font, element.text)[1]);
        break;
    }
    case application::TextRole::StatusInfo:
    case application::TextRole::StatusWarning:
    case application::TextRole::StatusCritical:
        lv_obj_align(&label, LV_ALIGN_LEFT_MID, wide_road_scene_ ? 24 : 12, 0);
        break;
    case application::TextRole::Gear:
        lv_obj_align(&label, LV_ALIGN_RIGHT_MID, wide_road_scene_ ? -24 : -12, 0);
        break;
    case application::TextRole::SpeedLimit:
        lv_obj_align(speed_limit_sign_, LV_ALIGN_TOP_RIGHT,
            wide_road_scene_ ? -24 : -12, wide_road_scene_ ? 24 : 12);
        lv_obj_align_to(&label, speed_limit_sign_, LV_ALIGN_CENTER, 0, 0);
        show(*speed_limit_sign_);
        break;
    }
    show(label);
}

void LvglDashboardCanvas::draw_power(const application::PowerBarElement& element) {
    const auto level = std::clamp<std::int32_t>(element.permille, -1000, 1000);
    if (level == 0 || power_fill_ == nullptr) {
        return;
    }
    const auto magnitude = level < 0 ? -level : level;
    const auto half = power_rail_half(wide_road_scene_);
    const auto center = power_rail_top(wide_road_scene_) + half;
    const auto height = std::max<std::int32_t>(1, (magnitude * half + 500) / 1000);
    lv_obj_set_pos(power_fill_, wide_road_scene_ ? 10 : 8, level > 0 ? center - height : center);
    lv_obj_set_size(power_fill_, 5, height);
    lv_obj_set_style_bg_color(
        power_fill_, lv_color_hex(level > 0 ? secondary_foreground : 0x5CBD68U),
        LV_PART_MAIN);
    show(*power_fill_);
}

void LvglDashboardCanvas::draw_symbol(
    const application::SymbolElement& element) {
    if (element.role == application::SymbolRole::TurnSignal) {
        if (element.symbol == application::Symbol::LeftArrow) {
            turn_signal_active_[0] = true;
            set_image_source(
                blind_spot_vehicle_slots_[0],
                &vehicle_image(wide_road_scene_, 0, true));
        } else if (element.symbol == application::Symbol::RightArrow) {
            turn_signal_active_[1] = true;
            set_image_source(
                blind_spot_vehicle_slots_[1],
                &vehicle_image(wide_road_scene_, 1, true));
        }
    }

    const auto* descriptor = symbol_image(element.symbol, wide_road_scene_);
    if (!element.visible || descriptor == nullptr) {
        return;
    }
    const bool footer_icon = element.role == application::SymbolRole::Autopilot;
    if (!footer_icon && next_image_slot_ >= image_slots_.size()) {
        return;
    }

    lv_obj_t& image = footer_icon ? *status_autopilot_ : *image_slots_[next_image_slot_++];
    set_image_source(&image, descriptor);
    lv_obj_set_style_image_opa(
        &image,
        element.role == application::SymbolRole::TurnSignal &&
                !element.illuminated
            ? LV_OPA_50
            : LV_OPA_COVER,
        LV_PART_MAIN);

    if (element.role == application::SymbolRole::Autopilot &&
        element.symbol == application::Symbol::Autopilot) {
        for (lv_obj_t* line : road_line_slots_) {
            if (line != nullptr) {
                lv_obj_set_style_line_color(
                    line, lv_color_hex(autopilot_blue), LV_PART_MAIN);
            }
        }
    }

    auto position = symbol_position(element);
    {
        if (element.role == application::SymbolRole::TurnSignal)
            position = {static_cast<std::int16_t>((element.symbol == application::Symbol::LeftArrow ? -1 : 1) *
                           (wide_road_scene_ ? 100 : 66)),
                        static_cast<std::int16_t>(wide_road_scene_ ? -150 : -75)};
        else if (element.role == application::SymbolRole::Autopilot) position = {0, 0};
        else position[0] = wide_road_scene_ ? 370 : 234;
    }
    lv_obj_align(
        &image,
        LV_ALIGN_CENTER,
        position[0],
        position[1]);


    show(image);
}

void LvglDashboardCanvas::draw_edge_gradient(
    const application::EdgeGradientElement& element) {
    const auto vehicle_index =
        element.edge == application::DisplayEdge::Left ? 0U : 1U;
    lv_obj_t& vehicle = *blind_spot_vehicle_slots_[vehicle_index];
    lv_obj_align(
        &vehicle,
        LV_ALIGN_CENTER,
        static_cast<std::int16_t>((element.edge == application::DisplayEdge::Left ? -1 : 1) *
                                 (wide_road_scene_ ? 184 : 130) +
                                 (element.edge == application::DisplayEdge::Left ? (wide_road_scene_ ? 2 : 1) : 0)),
        wide_road_scene_ ? 150 : 61);
    show(vehicle);
}

void LvglDashboardCanvas::draw_battery(const application::BatteryChargeElement& element) {
    if (battery_group_ == nullptr || element.percent > 100) return;
    const auto percent = element.percent;
    const auto width = (40 * percent + 50) / 100;
    lv_obj_set_width(battery_fill_, width);
    lv_obj_set_style_bg_color(battery_fill_,
        lv_color_hex(percent <= 20 ? 0xFFB000 : 0x5CBD68), LV_PART_MAIN);
    if (width == 0) hide(battery_fill_);
    else show(*battery_fill_);
    // Small bounded stack buffer: no heap formatting or per-frame widget allocation.
    char text[5]{};
    unsigned index = 0;
    if (percent == 100) text[index++] = '1';
    if (percent >= 10) text[index++] = static_cast<char>('0' + (percent / 10) % 10);
    text[index++] = static_cast<char>('0' + percent % 10);
    text[index++] = '%';
    lv_label_set_text(battery_label_, text);
    show(*battery_group_);
}

void LvglDashboardCanvas::end_frame() {
    // Keep warning sources stable across frames. Resetting red to gray in
    // begin_frame and back to red during rendering repeatedly reopened images.
    for (std::size_t side = 0; side < blind_spot_vehicle_count; ++side) {
        set_image_source(blind_spot_vehicle_slots_[side],
            &vehicle_image(wide_road_scene_, static_cast<int>(side),
                           turn_signal_active_[side]));
    }
    // Connection/data warnings take precedence over battery in the left footer.
    // Healthy Ready + fresh traffic emits no status text; only then is the
    // fresh battery widget shown. Resolve after every feature, independent of order.
    if (!lv_obj_has_flag(text_slots_[text_slot(application::TextRole::StatusInfo)], LV_OBJ_FLAG_HIDDEN))
        hide(battery_group_);
    // Only a currently rendered, fresh P observation selects the parked view.
    // Missing/disabled gear must not leave a previous Park scene latched.
    const auto* gear = text_slots_[text_slot(application::TextRole::Gear)];
    const bool parked = !lv_obj_has_flag(gear, LV_OBJ_FLAG_HIDDEN) &&
                        std::string_view{lv_label_get_text(gear)} == "P";
    for (auto* line : road_line_slots_) {
        if (line != nullptr) {
            if (parked) hide(line);
            else show(*line);
        }
    }
    const std::size_t rows = wide_road_scene_ ? 6 : lane_rows_per_side;
    // A matching active turn signal and occupied blind spot override lane color.
    // Keep the warning stable through the signal's dim blink phase.
    for (std::size_t index = 0; index < rows * 2; ++index) {
        if (turn_signal_active_[index / rows] &&
            !lv_obj_has_flag(blind_spot_vehicle_slots_[index / rows], LV_OBJ_FLAG_HIDDEN))
            lv_obj_set_style_line_color(road_line_slots_[index],
                lv_color_hex(blind_spot_red), LV_PART_MAIN);
    }
    for (std::size_t index = 0; index < outer_line_slots_.size(); ++index) {
        auto* line = outer_line_slots_[index];
        if (line == nullptr) continue;
        const auto* vehicle = blind_spot_vehicle_slots_[index / rows];
        if (parked || lv_obj_has_flag(vehicle, LV_OBJ_FLAG_HIDDEN)) hide(line);
        else show(*line);
    }
    set_image_source(road_vehicle_, &vehicle_image(wide_road_scene_, -1, false, parked));
    {
        if (parked) lv_obj_align(road_vehicle_, LV_ALIGN_BOTTOM_MID, 0, 0);
        else lv_obj_align(road_vehicle_, LV_ALIGN_CENTER, 0, wide_road_scene_ ? 92 : 46);
    }
    if (!speed_drawn_this_frame_) {
        road_speed_kmh_ = 0;
    }
    // LVGL invalidates changed widgets and line bounds itself. Invalidating
    // both regions here repainted every sprite and label for a power update.
}

}  // namespace tesla_headup::platform
