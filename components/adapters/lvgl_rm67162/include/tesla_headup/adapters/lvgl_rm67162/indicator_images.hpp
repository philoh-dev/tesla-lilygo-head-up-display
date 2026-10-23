#pragma once

#include "lvgl.h"

namespace tesla_headup::platform::assets {

extern const lv_image_dsc_t turn_left_image;
extern const lv_image_dsc_t turn_right_image;
extern const lv_image_dsc_t position_lights_image;
extern const lv_image_dsc_t low_beam_image;
extern const lv_image_dsc_t high_beam_image;
extern const lv_image_dsc_t adaptive_high_beam_image;
extern const lv_image_dsc_t front_fog_image;
extern const lv_image_dsc_t rear_fog_image;
extern const lv_image_dsc_t autopilot_image;
extern const lv_image_dsc_t blind_spot_left_image;
extern const lv_image_dsc_t blind_spot_right_image;
extern const lv_image_dsc_t blind_spot_left_grey_image;
extern const lv_image_dsc_t blind_spot_right_grey_image;
extern const lv_image_dsc_t autopilot_road_scene_image;
extern lv_image_dsc_t autopilot_road_scene_wide_image;
extern const lv_image_dsc_t road_scene_vehicle_image;
extern lv_image_dsc_t road_scene_vehicle_wide_image;
extern lv_image_dsc_t road_scene_park_image;
extern lv_image_dsc_t road_scene_park_wide_image;
extern lv_image_dsc_t blind_spot_left_wide_image;
extern lv_image_dsc_t blind_spot_right_wide_image;
extern lv_image_dsc_t blind_spot_left_grey_wide_image;
extern lv_image_dsc_t blind_spot_right_grey_wide_image;
[[nodiscard]] bool retain_road_scene_wide_images() noexcept;
void release_road_scene_wide_images() noexcept;
[[nodiscard]] bool retain_road_scene_compact_images() noexcept;
void release_road_scene_compact_images() noexcept;

}  // namespace tesla_headup::platform::assets
