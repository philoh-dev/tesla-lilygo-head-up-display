cmake_minimum_required(VERSION 3.24)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/board_profile.cmake")

set(
    TESLA_HEADUP_DASHBOARD_LAYOUT
    "true_hud"
    CACHE STRING
    "Dashboard layout compiled into the firmware (true_hud or road_scene)"
)
set_property(
    CACHE TESLA_HEADUP_DASHBOARD_LAYOUT
    PROPERTY STRINGS true_hud road_scene
)
if(NOT TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "road_scene" AND
   NOT TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "true_hud")
    message(
        FATAL_ERROR
        "Unknown TESLA_HEADUP_DASHBOARD_LAYOUT: ${TESLA_HEADUP_DASHBOARD_LAYOUT}"
    )
endif()

set(
    TESLA_HEADUP_FIRMWARE_MODE
    "${TESLA_HEADUP_DEFAULT_FIRMWARE_MODE}"
    CACHE STRING
    "Firmware mode (normal, display_self_test, board_bring_up, or rgb_diagnostics)"
)
set_property(
    CACHE TESLA_HEADUP_FIRMWARE_MODE
    PROPERTY STRINGS normal display_self_test board_bring_up rgb_diagnostics
)
if(NOT TESLA_HEADUP_FIRMWARE_MODE STREQUAL "normal" AND
   NOT TESLA_HEADUP_FIRMWARE_MODE STREQUAL "display_self_test" AND
   NOT TESLA_HEADUP_FIRMWARE_MODE STREQUAL "board_bring_up" AND
   NOT TESLA_HEADUP_FIRMWARE_MODE STREQUAL "rgb_diagnostics")
    message(FATAL_ERROR "Unknown Tesla Head-Up Display firmware mode: ${TESLA_HEADUP_FIRMWARE_MODE}")
endif()

if(TESLA_HEADUP_BOARD_PROFILE STREQUAL "h619" AND TESLA_HEADUP_FIRMWARE_MODE STREQUAL "rgb_diagnostics")
    message(FATAL_ERROR "rgb_diagnostics requires TESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v")
endif()

if(TESLA_HEADUP_BOARD_PROFILE STREQUAL "h619" AND
       TESLA_HEADUP_FIRMWARE_MODE STREQUAL "board_bring_up")
    message(FATAL_ERROR "board_bring_up requires TESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v")
endif()

set(
    TESLA_HEADUP_LAYOUT_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/layouts/${TESLA_HEADUP_DASHBOARD_LAYOUT}.sdkconfig.defaults"
)
if(NOT EXISTS "${TESLA_HEADUP_LAYOUT_DEFAULTS}")
    message(FATAL_ERROR "Missing defaults for dashboard layout: ${TESLA_HEADUP_DASHBOARD_LAYOUT}")
endif()

set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults"
    "${TESLA_HEADUP_BOARD_DEFAULTS}"
    "${TESLA_HEADUP_LAYOUT_DEFAULTS}"
)
if(TESLA_HEADUP_FIRMWARE_MODE STREQUAL "rgb_diagnostics")
    list(APPEND SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/modes/rgb_diagnostics.sdkconfig.defaults")
elseif(TESLA_HEADUP_FIRMWARE_MODE STREQUAL "display_self_test")
    list(APPEND SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/modes/display_self_test.sdkconfig.defaults")
else()
    list(APPEND SDKCONFIG_DEFAULTS
        "${CMAKE_CURRENT_LIST_DIR}/modes/normal.sdkconfig.defaults")
endif()

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/domain"
    "${CMAKE_CURRENT_LIST_DIR}/../components/application"
    "${CMAKE_CURRENT_LIST_DIR}/../components/adapters"
)

# LVGL 9.3 implements the RGB565-swapped software renderer, but its ESP-IDF
# Kconfig does not expose the corresponding option. The RM67162 consumes
# RGB565 most-significant byte first, so enable this renderer for LVGL and all
# firmware components instead of transmitting unrendered DMA-buffer contents.
add_compile_definitions(LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=1)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tesla_headup_firmware)

if(TESLA_HEADUP_FIRMWARE_MODE STREQUAL "rgb_diagnostics")
    if(NOT CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS OR CONFIG_TESLA_HEADUP_DISPLAY_SELF_TEST)
        message(FATAL_ERROR "RGB diagnostic mode requires a fresh diagnostic SDKCONFIG")
    endif()
elseif(CONFIG_TESLA_HEADUP_RGB_DIAGNOSTICS)
    message(FATAL_ERROR "Diagnostic sdkconfig must only be used with TESLA_HEADUP_FIRMWARE_MODE=rgb_diagnostics")
endif()

if(TESLA_HEADUP_BOARD_PROFILE STREQUAL "devkitc_n32r16v")
    if(NOT CONFIG_TESLA_HEADUP_DISPLAY_RGB OR
       NOT CONFIG_IDF_TARGET_ESP32S3 OR
       NOT CONFIG_ESPTOOLPY_OCT_FLASH OR
       NOT CONFIG_ESPTOOLPY_FLASHMODE_OPI OR
       NOT CONFIG_ESPTOOLPY_FLASH_SAMPLE_MODE_DTR OR
       NOT CONFIG_ESPTOOLPY_FLASHFREQ_80M OR
       NOT CONFIG_ESPTOOLPY_FLASHSIZE_32MB OR
       NOT CONFIG_SPIRAM OR
       NOT CONFIG_SPIRAM_MODE_OCT OR
       NOT CONFIG_SPIRAM_SPEED_80M OR
       NOT CONFIG_SPIRAM_BOOT_INIT OR
       NOT CONFIG_SPIRAM_MEMTEST OR
       NOT CONFIG_SPIRAM_USE_MALLOC OR
       NOT CONFIG_SPIRAM_FETCH_INSTRUCTIONS OR
       NOT CONFIG_SPIRAM_RODATA OR
       CONFIG_SPIRAM_ECC_ENABLE OR CONFIG_SPIRAM_IGNORE_NOTFOUND)
        message(FATAL_ERROR
            "Incompatible sdkconfig for devkitc_n32r16v. Use a fresh build directory "
            "and SDKCONFIG=sdkconfig.devkitc_n32r16v; do not reuse the LilyGO sdkconfig.")
    endif()
elseif(CONFIG_TESLA_HEADUP_DISPLAY_RGB)
    message(FATAL_ERROR "The RGB transport requires TESLA_HEADUP_BOARD_PROFILE=devkitc_n32r16v")
endif()
