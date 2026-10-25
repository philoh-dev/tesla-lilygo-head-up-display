#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/indicator_images.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace tesla_headup::platform {
namespace {
// Reserve the orbit's two pixels as well as the requested clear border.
constexpr int inset = 35 + LvglPixelOrbit::maximum_offset;
constexpr int limit_diameter = 54;
constexpr int battery_icon_height = 18;
constexpr int power_bar_height = 6;
constexpr auto foreground = 0xFFFFFFU;
constexpr auto secondary = 0xB8C1C5U;

void hide(lv_obj_t* object) {
    if (object != nullptr) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}
void show(lv_obj_t* object) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}
std::size_t slot(application::TextRole role) {
    switch (role) {
    case application::TextRole::PrimaryValue: return 0;
    case application::TextRole::Unit: return 1;
    case application::TextRole::Gear: return 3;
    case application::TextRole::SpeedLimit: return 4;
    default: return 2;
    }
}
struct InkBounds { int top; int bottom; };
InkBounds ink_bounds(const lv_font_t* font, std::string_view text) {
    InkBounds bounds{font->line_height, 0};
    for (const unsigned char character : text) {
        lv_font_glyph_dsc_t glyph{};
        if (lv_font_get_glyph_dsc(font, &glyph, character, 0) && glyph.box_h != 0) {
            const int bottom = font->line_height - font->base_line - glyph.ofs_y;
            bounds.top = std::min(bounds.top, bottom - glyph.box_h);
            bounds.bottom = std::max(bounds.bottom, bottom);
        }
    }
    return bounds;
}
InkBounds ink_bounds(lv_obj_t* label) {
    return ink_bounds(lv_obj_get_style_text_font(label, LV_PART_MAIN),
                      lv_label_get_text(label));
}
void footer(lv_obj_t* label, lv_obj_t* root, bool right) {
    lv_obj_update_layout(label);
    const auto ink = ink_bounds(label);
    const int center_y = lv_obj_get_height(root) - inset - battery_icon_height / 2;
    lv_obj_set_pos(label, right ? lv_obj_get_width(root) - inset - lv_obj_get_width(label) : inset,
                   center_y - (ink.top + ink.bottom) / 2);
}
lv_obj_t* box(lv_obj_t* parent, int x, int y, int width, int height,
              std::uint32_t color, int radius) {
    auto* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0);
    return object;
}
} // namespace

LvglDashboardCanvas::~LvglDashboardCanvas() { shutdown(); }
void LvglDashboardCanvas::shutdown() noexcept {
    pixel_orbit_.shutdown();
    root_ = nullptr;
    speed_limit_sign_ = nullptr;
    battery_label_ = nullptr;
    battery_group_ = nullptr;
    battery_fill_ = nullptr;
    power_track_ = nullptr;
    power_fill_ = nullptr;
    power_permille_ = 0;
    text_slots_.fill(nullptr);
    image_slots_.fill(nullptr);
    blind_spot_vehicle_slots_.fill(nullptr);
}
bool LvglDashboardCanvas::initialize(lv_display_t& display) noexcept {
    if (ready()) return true;
    root_ = pixel_orbit_.initialize(display);
    if (root_ == nullptr) return false;
    speed_limit_sign_ = lv_obj_create(root_);
    lv_obj_remove_style_all(speed_limit_sign_);
    lv_obj_remove_flag(speed_limit_sign_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(speed_limit_sign_, limit_diameter, limit_diameter);
    lv_obj_set_style_radius(speed_limit_sign_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(speed_limit_sign_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(speed_limit_sign_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(speed_limit_sign_, lv_color_hex(0xE02020), 0);
    lv_obj_set_style_border_width(speed_limit_sign_, 5, 0);
    for (auto*& label : text_slots_) {
        label = lv_label_create(root_);
        lv_obj_set_style_text_color(label, lv_color_hex(foreground), 0);
    }
    battery_label_ = lv_label_create(root_);
    lv_obj_set_style_text_font(battery_label_, &tesla_headup_inter_medium_18, 0);
    lv_obj_set_style_text_color(battery_label_, lv_color_hex(foreground), 0);
    // Retain the road_scene outline and colors with a taller 18 px body.
    battery_group_ = box(root_, 0, 0, 47, battery_icon_height, 0, 0);
    lv_obj_set_style_bg_opa(battery_group_, LV_OPA_TRANSP, 0);
    auto* body = box(battery_group_, 0, 0, 44, battery_icon_height, 0x7C8589, 2);
    box(body, 2, 2, 40, battery_icon_height - 4, 0x252A2D, 1);
    box(battery_group_, 44, 5, 3, 8, 0x7C8589, 1);
    battery_fill_ = box(body, 2, 2, 40, battery_icon_height - 4, 0x5CBD68, 1);
    power_track_ = box(root_, 0, 0, 1, power_bar_height, 0x2B3236, 3);
    power_fill_ = box(power_track_, 0, 0, 1, power_bar_height, secondary, 0);
    // All images are small project-created indicators, prepared at native size.
    for (std::size_t i = 0; i < 2; ++i) image_slots_[i] = lv_image_create(root_);
    for (std::size_t side = 0; side < 2; ++side) {
        auto*& icon = blind_spot_vehicle_slots_[side];
        icon = lv_image_create(root_);
        lv_image_set_src(icon, side == 0 ? &assets::hud_blind_spot_left_image :
                                          &assets::hud_blind_spot_right_image);
        lv_obj_align(icon, side == 0 ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID,
                     side == 0 ? inset : -inset, 6);
    }
    begin_frame();
    return true;
}
bool LvglDashboardCanvas::ready() const noexcept { return root_ != nullptr; }
void LvglDashboardCanvas::begin_frame() {
    for (auto* label : text_slots_) hide(label);
    for (auto* image : image_slots_) hide(image);
    for (auto* icon : blind_spot_vehicle_slots_) hide(icon);
    turn_signal_active_.fill(false);
    hide(speed_limit_sign_);
    hide(battery_label_);
    hide(battery_group_);
    hide(power_fill_);
    power_permille_ = 0;
}
void LvglDashboardCanvas::draw_text(const application::TextElement& element) {
    if (!ready()) return;
    auto* label = text_slots_[slot(element.role)];
    std::array<char, 32> text{};
    std::copy_n(element.text.begin(), std::min(element.text.size(), text.size() - 1), text.begin());
    lv_label_set_text(label, text.data());
    const lv_font_t* font = &tesla_headup_inter_medium_18;
    auto color = foreground;
    switch (element.role) {
    case application::TextRole::PrimaryValue: font = &tesla_headup_inter_regular_100; break;
    case application::TextRole::Unit: color = secondary; break;
    case application::TextRole::Gear: break;
    case application::TextRole::SpeedLimit:
        font = &tesla_headup_inter_semibold_18;
        color = 0x000000;
        show(speed_limit_sign_);
        break;
    case application::TextRole::StatusInfo: color = secondary; break;
    case application::TextRole::StatusWarning: color = 0xFFB000; break;
    case application::TextRole::StatusCritical: color = 0xFF453A; break;
    }
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    show(label);
}
void LvglDashboardCanvas::draw_symbol(const application::SymbolElement& element) {
    if (!ready() || !element.visible) return;
    const bool left = element.symbol == application::Symbol::LeftArrow;
    if (!left && element.symbol != application::Symbol::RightArrow) return;
    turn_signal_active_[left ? 0 : 1] = true;
    auto* image = image_slots_[left ? 0 : 1];
    lv_image_set_src(image, left ? &assets::turn_left_image : &assets::turn_right_image);
    lv_obj_set_style_image_opa(image, element.illuminated ? LV_OPA_COVER : LV_OPA_50, 0);
    lv_obj_align(image, left ? LV_ALIGN_TOP_LEFT : LV_ALIGN_TOP_RIGHT,
                 left ? inset : -inset, inset);
    show(image);
}
void LvglDashboardCanvas::draw_battery(const application::BatteryChargeElement& element) {
    if (!ready() || element.percent > 100) return;
    std::array<char, 8> text{};
    std::snprintf(text.data(), text.size(), "%u%%", static_cast<unsigned>(element.percent));
    lv_label_set_text(battery_label_, text.data());
    const int width = (40 * element.percent + 50) / 100;
    lv_obj_set_width(battery_fill_, width);
    if (width == 0) hide(battery_fill_);
    else show(battery_fill_);
    show(battery_group_);
    show(battery_label_);
}
void LvglDashboardCanvas::draw_power(const application::PowerBarElement& element) {
    if (!ready()) return;
    power_permille_ = static_cast<std::int16_t>(
        std::clamp<std::int32_t>(element.permille, -1000, 1000));
}
void LvglDashboardCanvas::draw_edge_gradient(const application::EdgeGradientElement& element) {
    if (!ready()) return;
    show(blind_spot_vehicle_slots_[element.edge == application::DisplayEdge::Left ? 0 : 1]);
}
void LvglDashboardCanvas::end_frame() {
    if (!ready()) return;
    // Resolve after all features so draw order and the blink's dark phase do
    // not affect the occupied-side warning color.
    for (std::size_t side = 0; side < 2; ++side) {
        auto* icon = blind_spot_vehicle_slots_[side];
        lv_obj_set_style_image_recolor(icon, lv_color_hex(0xFF453A), 0);
        lv_obj_set_style_image_recolor_opa(icon,
            turn_signal_active_[side] ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    lv_obj_update_layout(root_);
    auto* speed = text_slots_[0];
    auto* unit = text_slots_[1];
    // A fixed digit envelope keeps the baseline stable as the value changes.
    const auto speed_ink = ink_bounds(lv_obj_get_style_text_font(speed, LV_PART_MAIN),
                                      "0123456789");
    const int speed_y = inset - speed_ink.top;
    const bool has_speed = !lv_obj_has_flag(speed, LV_OBJ_FLAG_HIDDEN);
    const bool has_unit = !lv_obj_has_flag(unit, LV_OBJ_FLAG_HIDDEN);
    const int speed_width = has_speed ? lv_obj_get_width(speed) : 0;
    const int gap = has_speed && has_unit ? 4 : 0;
    const int group_width = speed_width + gap + (has_unit ? lv_obj_get_width(unit) : 0);
    const int group_x = (lv_obj_get_width(root_) - group_width) / 2;
    lv_obj_set_pos(speed, group_x, speed_y);
    // Center both labels together and align their visible glyph bottoms.
    const int unit_x = group_x + speed_width + gap;
    const auto unit_ink = ink_bounds(unit);
    const int unit_y = speed_y + speed_ink.bottom - unit_ink.bottom;
    lv_obj_set_pos(unit, unit_x, unit_y);
    if (has_speed && has_unit) {
        // Startup/no-speed frames use RIGHT_MID below. LVGL retains that
        // alignment across set_pos(), so restore screen-coordinate positioning
        // before placing the sign above the unit when speed becomes available.
        lv_obj_set_align(speed_limit_sign_, LV_ALIGN_TOP_LEFT);
        lv_obj_set_pos(speed_limit_sign_,
            unit_x + (lv_obj_get_width(unit) - limit_diameter) / 2,
            std::max(inset, unit_y + unit_ink.top - 6 - limit_diameter));
    } else {
        lv_obj_align(speed_limit_sign_, LV_ALIGN_RIGHT_MID, -inset, 0);
    }
    lv_obj_align_to(text_slots_[4], speed_limit_sign_, LV_ALIGN_CENTER, 0, 0);
    const auto limit_ink = ink_bounds(text_slots_[4]);
    lv_obj_set_y(text_slots_[4], lv_obj_get_y(speed_limit_sign_) +
        (limit_diameter - limit_ink.bottom + limit_ink.top) / 2 - limit_ink.top);
    footer(battery_label_, root_, false);
    lv_obj_set_x(battery_label_, inset + 58);
    lv_obj_set_pos(battery_group_, inset,
        lv_obj_get_height(root_) - inset - battery_icon_height);
    footer(text_slots_[2], root_, true);
    footer(text_slots_[3], root_, true);
    // A connection warning takes precedence regardless of feature draw order.
    if (!lv_obj_has_flag(text_slots_[2], LV_OBJ_FLAG_HIDDEN)) hide(text_slots_[3]);
    lv_obj_update_layout(root_);
    // Reserve enough room for 100% so charge changes cannot shift the gauge.
    // A longer connection message shortens it rather than covering it.
    const int bar_left = inset + 58 + 50 + 16;
    auto* right_label = !lv_obj_has_flag(text_slots_[2], LV_OBJ_FLAG_HIDDEN)
                            ? text_slots_[2] : text_slots_[3];
    const int bar_right = (lv_obj_has_flag(right_label, LV_OBJ_FLAG_HIDDEN)
                              ? lv_obj_get_width(root_) - inset
                              : lv_obj_get_x(right_label)) - 16;
    const int half = std::max(0, (bar_right - bar_left) / 2);
    const int bar_y = lv_obj_get_height(root_) - inset -
                      battery_icon_height / 2 - power_bar_height / 2;
    lv_obj_set_pos(power_track_, bar_left, bar_y);
    lv_obj_set_width(power_track_, half * 2);
    if (half < 8) hide(power_track_);
    else show(power_track_);
    const int fill = (half * std::abs(static_cast<int>(power_permille_)) + 500) / 1000;
    if (fill == 0 || half < 8) hide(power_fill_);
    else {
        lv_obj_set_pos(power_fill_, power_permille_ > 0 ? half - fill : half, 0);
        lv_obj_set_width(power_fill_, fill);
        lv_obj_set_style_bg_color(power_fill_,
            lv_color_hex(power_permille_ > 0 ? secondary : 0x5CBD68), 0);
        show(power_fill_);
    }
    lv_obj_update_layout(root_);
    lv_obj_invalidate(root_);
}
} // namespace tesla_headup::platform
