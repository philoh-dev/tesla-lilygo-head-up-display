if(NOT COMMAND idf_component_register)
    message(FATAL_ERROR "lvgl_rm67162 is an ESP-IDF-only component")
endif()

if(CONFIG_TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE)
    set(DASHBOARD_LAYOUT_SOURCE "src/lvgl_road_scene_dashboard_canvas.cpp")
    set(DASHBOARD_LAYOUT_ASSET_SOURCE "assets/road_scene_indicator_images.cpp" "src/prepared_image_storage.cpp")
    if(CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE)
        list(APPEND DASHBOARD_LAYOUT_ASSET_SOURCE "assets/road_scene_wide_images.cpp")
    endif()
    list(APPEND DASHBOARD_LAYOUT_ASSET_SOURCE
        "assets/tesla_headup_inter_medium_16.c"
        "assets/tesla_headup_inter_medium_18.c"
        "assets/tesla_headup_inter_medium_24.c"
        "assets/tesla_headup_inter_semibold_24.c"
    )
    set(DASHBOARD_SPEED_FONT_SOURCE "assets/tesla_headup_inter_regular_64.c")
    if(CONFIG_TESLA_HEADUP_RGB_FULL_ROAD_SCENE)
        set(DASHBOARD_SPEED_FONT_SOURCE "assets/tesla_headup_inter_regular_110.c" "assets/tesla_headup_inter_semibold_40.c")
    endif()
else()
    # ESP-IDF's early component-requirements pass runs before Kconfig is loaded.
    # Both canvases have the same dependencies; the configured pass selects
    # road_scene above or the default true_hud sources here.
    set(DASHBOARD_LAYOUT_SOURCE "src/lvgl_true_hud_dashboard_canvas.cpp")
    set(DASHBOARD_LAYOUT_ASSET_SOURCE
        "assets/true_hud_indicator_images.cpp"
        "assets/tesla_headup_inter_medium_18.c"
        "assets/tesla_headup_inter_medium_24.c"
        "assets/tesla_headup_inter_semibold_18.c")
    set(DASHBOARD_SPEED_FONT_SOURCE "assets/tesla_headup_inter_regular_100.c")
endif()

set(DISPLAY_TRANSPORT_SOURCE "src/rm67162_display.cpp")
set(DISPLAY_TRANSPORT_REQUIRES driver esp-lcd-panel-rm67162 esp_lcd)
if(CONFIG_TESLA_HEADUP_DISPLAY_RGB)
    # Retain the established canvas/assets component, but do not compile the
    # RM67162 QSPI transport into Newhaven firmware.
    set(DISPLAY_TRANSPORT_SOURCE "")
    set(DISPLAY_TRANSPORT_REQUIRES "")
endif()

idf_component_register(
    SRCS
        "assets/indicator_images.cpp"
        ${DASHBOARD_LAYOUT_ASSET_SOURCE}
        ${DASHBOARD_SPEED_FONT_SOURCE}
        ${DASHBOARD_LAYOUT_SOURCE}
        ${DISPLAY_TRANSPORT_SOURCE}
        "src/lvgl_pixel_orbit.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        dashboard
        ${DISPLAY_TRANSPORT_REQUIRES}
        esp_timer
        lvgl
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
)
