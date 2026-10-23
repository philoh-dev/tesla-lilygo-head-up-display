#include "tesla_headup/adapters/lvgl_rm67162/lvgl_dashboard_canvas.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/indicator_images.hpp"
#include "tesla_headup/adapters/lvgl_rm67162/speed_font.hpp"

#include <algorithm>
#include <array>

namespace tesla_headup::platform {
namespace {

constexpr auto background = 0x02070AU;
constexpr auto foreground = 0xEEF7FAU;
constexpr auto secondary_foreground = 0xB8C1C5U;
constexpr auto status_warning = 0xFFB000U;
constexpr auto status_critical = 0xFF453AU;
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

const lv_image_dsc_t* symbol_image(application::Symbol symbol) {
    switch (symbol) {
    case application::Symbol::LeftArrow:
        return &assets::turn_left_image;
    case application::Symbol::RightArrow:
        return &assets::turn_right_image;
    case application::Symbol::PositionLights:
    case application::Symbol::LowBeam:
    case application::Symbol::HighBeam:
    case application::Symbol::AdaptiveHighBeam:
    case application::Symbol::FrontFogLight:
    case application::Symbol::RearFogLight:
        return nullptr;
    case application::Symbol::Autopilot:
        return &assets::autopilot_image;
    }
    return nullptr;
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
    root_ = nullptr;
    speed_limit_sign_ = nullptr;
}

bool LvglDashboardCanvas::initialize(lv_display_t& display) noexcept {
    root_ = pixel_orbit_.initialize(display);
    if (root_ == nullptr) {
        return false;
    }

    prepare_object(*root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);

    speed_limit_sign_ = lv_obj_create(root_);
    if (speed_limit_sign_ == nullptr) {
        return false;
    }
    prepare_object(*speed_limit_sign_);
    lv_obj_set_size(
        speed_limit_sign_, speed_limit_diameter, speed_limit_diameter);
    lv_obj_set_style_radius(
        speed_limit_sign_, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        speed_limit_sign_, lv_color_hex(speed_limit_white), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(speed_limit_sign_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        speed_limit_sign_, lv_color_hex(speed_limit_red), LV_PART_MAIN);
    lv_obj_set_style_border_width(
        speed_limit_sign_, speed_limit_border_width, LV_PART_MAIN);
    hide(speed_limit_sign_);

    for (lv_obj_t*& text : text_slots_) {
        text = lv_label_create(root_);
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
    return true;
}

bool LvglDashboardCanvas::ready() const noexcept {
    return root_ != nullptr;
}

void LvglDashboardCanvas::begin_frame() {
    next_image_slot_ = 0;
    for (lv_obj_t* object : text_slots_) {
        hide(object);
    }
    for (lv_obj_t* object : image_slots_) {
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
    const lv_font_t* font =
        primary ? &tesla_headup_speed_font_110
                : (element.role == application::TextRole::Gear ||
                   element.role == application::TextRole::SpeedLimit)
                      ? &lv_font_montserrat_24
                      : &lv_font_montserrat_18;
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
    lv_obj_set_style_text_color(
        &label,
        lv_color_hex(color),
        LV_PART_MAIN);
    if (element.role == application::TextRole::Gear) {
        lv_obj_align(&label, LV_ALIGN_TOP_RIGHT, -15, 6);
    } else if (element.role == application::TextRole::SpeedLimit) {
        lv_obj_align(
            speed_limit_sign_,
            LV_ALIGN_CENTER,
            element.horizontal_offset,
            element.vertical_offset);
        lv_obj_align_to(&label, speed_limit_sign_, LV_ALIGN_CENTER, 0, 0);
        show(*speed_limit_sign_);
    } else {
        lv_obj_align(
            &label,
            LV_ALIGN_CENTER,
            element.horizontal_offset,
            element.vertical_offset);
    }
    show(label);
}

void LvglDashboardCanvas::draw_symbol(
    const application::SymbolElement& element) {
    const auto* descriptor = symbol_image(element.symbol);
    if (!element.visible || descriptor == nullptr) {
        return;
    }
    if (next_image_slot_ >= image_slots_.size()) {
        return;
    }

    lv_obj_t& image = *image_slots_[next_image_slot_++];
    lv_image_set_src(&image, descriptor);
    lv_obj_set_style_image_opa(
        &image,
        element.role == application::SymbolRole::TurnSignal &&
                !element.illuminated
            ? LV_OPA_50
            : LV_OPA_COVER,
        LV_PART_MAIN);
    lv_obj_align(
        &image,
        LV_ALIGN_CENTER,
        element.horizontal_offset,
        element.vertical_offset);
    show(image);
}

void LvglDashboardCanvas::draw_edge_gradient(
    const application::EdgeGradientElement& element) {
    (void)element;
}

void LvglDashboardCanvas::end_frame() {
    if (root_ != nullptr) {
        lv_obj_invalidate(root_);
    }
}

void LvglDashboardCanvas::draw_power(const application::PowerBarElement&) {}

void LvglDashboardCanvas::draw_battery(const application::BatteryChargeElement&) {}

}  // namespace tesla_headup::platform
