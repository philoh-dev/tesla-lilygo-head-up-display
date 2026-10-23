#include "tesla_headup/adapters/lvgl_rm67162/lvgl_pixel_orbit.hpp"

#include <array>

namespace tesla_headup::platform {
namespace {

struct Offset {
    std::int32_t x;
    std::int32_t y;
};

// Begin centered at boot, then walk the perimeter in single-pixel steps.
constexpr std::array positions{
    Offset{1, 1}, Offset{0, 2}, Offset{-1, 1}, Offset{-2, 0},
    Offset{-1, -1}, Offset{0, -2}, Offset{1, -1}, Offset{2, 0},
};

void prepare_container(lv_obj_t& object) {
    lv_obj_remove_style_all(&object);
    lv_obj_remove_flag(&object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(&object, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(&object, LV_OPA_COVER, LV_PART_MAIN);
}

}  // namespace

LvglPixelOrbit::~LvglPixelOrbit() {
    shutdown();
}

lv_obj_t* LvglPixelOrbit::initialize(lv_display_t& display) noexcept {
    if (content_ != nullptr) {
        return content_;
    }
    lv_obj_t* screen = lv_display_get_screen_active(&display);
    if (screen == nullptr) {
        return nullptr;
    }
    prepare_container(*screen);
    content_ = lv_obj_create(screen);
    if (content_ == nullptr) {
        return nullptr;
    }
    prepare_container(*content_);
    lv_obj_set_size(content_, lv_display_get_horizontal_resolution(&display),
                    lv_display_get_vertical_resolution(&display));
    lv_obj_set_pos(content_, 0, 0);
    next_position_ = 0;
    timer_ = lv_timer_create(advance, interval_ms, this);
    if (timer_ == nullptr) {
        shutdown();
        return nullptr;
    }
    return content_;
}

void LvglPixelOrbit::shutdown() noexcept {
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (content_ != nullptr) {
        lv_obj_delete(content_);
        content_ = nullptr;
    }
    next_position_ = 0;
}

void LvglPixelOrbit::advance(lv_timer_t* timer) {
    auto* orbit = static_cast<LvglPixelOrbit*>(lv_timer_get_user_data(timer));
    if (orbit == nullptr || orbit->content_ == nullptr) {
        return;
    }
    const auto position = positions[orbit->next_position_];
    orbit->next_position_ = (orbit->next_position_ + 1U) % positions.size();
    // LVGL invalidates both old and new bounds, repainting exposed edges black.
    lv_obj_set_pos(orbit->content_, position.x, position.y);
}

}  // namespace tesla_headup::platform
