cmake_minimum_required(VERSION 3.24)

project(tesla_headup LANGUAGES C CXX)

option(TESLA_HEADUP_BUILD_TESTS "Build host-side tests" ON)
option(TESLA_HEADUP_BUILD_SIMULATOR "Build the SDL dashboard simulator" OFF)
set(
    TESLA_HEADUP_DASHBOARD_LAYOUT
    "true_hud"
    CACHE STRING
    "Dashboard layout compiled into the simulator (true_hud or road_scene)"
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

add_subdirectory(components/domain)
add_subdirectory(components/application/dashboard)
add_subdirectory(components/application/diagnostics)
add_subdirectory(components/application/settings)
add_subdirectory(components/application/telemetry)
add_subdirectory(components/application/hud_runtime)
add_subdirectory(components/adapters/dashkit_ble)
add_subdirectory(components/adapters/dashkit_protocol)
add_subdirectory(components/adapters/tesla_decoder)
add_subdirectory(components/adapters/dashkit_tesla)
add_subdirectory(components/adapters/esp_nvs_settings)
add_subdirectory(components/adapters/in_memory_settings)

if(TESLA_HEADUP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TESLA_HEADUP_BUILD_SIMULATOR)
    add_subdirectory(simulator)
endif()
