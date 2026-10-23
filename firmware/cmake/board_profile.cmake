# Keep selection usable without ESP-IDF, so profile isolation can be host-tested.
set(TESLA_HEADUP_BOARD_PROFILE "h619" CACHE STRING "Tesla Head-Up Display board profile")
set_property(CACHE TESLA_HEADUP_BOARD_PROFILE PROPERTY STRINGS h619 devkitc_n32r16v)
if(NOT TESLA_HEADUP_BOARD_PROFILE STREQUAL "h619" AND
   NOT TESLA_HEADUP_BOARD_PROFILE STREQUAL "devkitc_n32r16v")
    message(FATAL_ERROR "Unknown Tesla Head-Up Display board profile: ${TESLA_HEADUP_BOARD_PROFILE}")
endif()

get_filename_component(TESLA_HEADUP_FIRMWARE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TESLA_HEADUP_BOARD_DEFAULTS
    "${TESLA_HEADUP_FIRMWARE_DIR}/boards/${TESLA_HEADUP_BOARD_PROFILE}/sdkconfig.defaults")

# Defaults cannot override an already generated sdkconfig. Do not let a new
# board silently reuse the LilyGO configuration, even on a clean configure.
if(TESLA_HEADUP_BOARD_PROFILE STREQUAL "devkitc_n32r16v" AND NOT DEFINED SDKCONFIG)
    set(SDKCONFIG "${TESLA_HEADUP_FIRMWARE_DIR}/sdkconfig.devkitc_n32r16v")
endif()

if(TESLA_HEADUP_BOARD_PROFILE STREQUAL "devkitc_n32r16v")
    if(DEFINED IDF_TARGET AND NOT IDF_TARGET STREQUAL "esp32s3")
        message(FATAL_ERROR "devkitc_n32r16v requires IDF_TARGET=esp32s3")
    endif()
    set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF chip target")
    set(TESLA_HEADUP_DEFAULT_FIRMWARE_MODE "board_bring_up")
else()
    set(TESLA_HEADUP_DEFAULT_FIRMWARE_MODE "normal")
endif()
