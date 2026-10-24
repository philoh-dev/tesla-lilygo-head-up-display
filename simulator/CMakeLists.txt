find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(TESLA_HEADUP_SDL_TARGET SDL2::SDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(TESLA_HEADUP_SDL_TARGET PkgConfig::SDL2)
endif()
if(WIN32)
    add_compile_definitions(SDL_MAIN_HANDLED)
endif()

include(FetchContent)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(
    LV_BUILD_CONF_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h"
    CACHE FILEPATH "" FORCE
)
FetchContent_Declare(
    lvgl
    URL https://github.com/lvgl/lvgl/archive/c033a98afddd65aaafeebea625382a94020fe4a7.tar.gz
    URL_HASH SHA256=9c6f8230d0b0e11141aad8f047af699322dbd3d37b2c41170b1526c12d5dfbb3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lvgl)

if(TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "road_scene")
    set(
        TESLA_HEADUP_DASHBOARD_CANVAS_SOURCE
        ../components/adapters/lvgl_rm67162/src/lvgl_road_scene_dashboard_canvas.cpp
    )
    list(APPEND TESLA_HEADUP_DASHBOARD_TEXT_FONT_SOURCE
        "../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_medium_16.c"
        "../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_medium_18.c"
        "../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_medium_24.c"
        "../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_semibold_24.c"
    )
    set(
        TESLA_HEADUP_DASHBOARD_SPEED_FONT_SOURCE
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_regular_64.c
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_regular_110.c
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_semibold_40.c
    )
    set(
        TESLA_HEADUP_DASHBOARD_LAYOUT_ASSET_SOURCE
        ../components/adapters/lvgl_rm67162/assets/road_scene_indicator_images.cpp
        ../components/adapters/lvgl_rm67162/assets/road_scene_wide_images.cpp
        ../components/adapters/lvgl_rm67162/src/prepared_image_storage.cpp
    )
    set(TESLA_HEADUP_DASHBOARD_LAYOUT_DEFINITION TESLA_HEADUP_DASHBOARD_LAYOUT_ROAD_SCENE=1)
elseif(TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "true_hud")
    set(TESLA_HEADUP_DASHBOARD_LAYOUT_ASSET_SOURCE
        ../components/adapters/lvgl_rm67162/assets/true_hud_indicator_images.cpp)
    set(TESLA_HEADUP_DASHBOARD_CANVAS_SOURCE
        ../components/adapters/lvgl_rm67162/src/lvgl_true_hud_dashboard_canvas.cpp)
    set(TESLA_HEADUP_DASHBOARD_SPEED_FONT_SOURCE
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_regular_100.c)
    set(TESLA_HEADUP_DASHBOARD_TEXT_FONT_SOURCE
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_medium_18.c
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_medium_24.c
        ../components/adapters/lvgl_rm67162/assets/tesla_headup_inter_semibold_18.c)
    set(TESLA_HEADUP_DASHBOARD_LAYOUT_DEFINITION TESLA_HEADUP_DASHBOARD_LAYOUT_TRUE_HUD=1)
else()
    message(FATAL_ERROR "Unsupported dashboard layout: ${TESLA_HEADUP_DASHBOARD_LAYOUT}")
endif()

add_library(tesla_headup_lvgl_dashboard_canvas
    ../components/adapters/lvgl_rm67162/src/lvgl_pixel_orbit.cpp
    ../components/adapters/lvgl_rm67162/assets/indicator_images.cpp
    ${TESLA_HEADUP_DASHBOARD_TEXT_FONT_SOURCE}
    ${TESLA_HEADUP_DASHBOARD_LAYOUT_ASSET_SOURCE}
    ${TESLA_HEADUP_DASHBOARD_SPEED_FONT_SOURCE}
    ${TESLA_HEADUP_DASHBOARD_CANVAS_SOURCE}
)
target_include_directories(tesla_headup_lvgl_dashboard_canvas PUBLIC
    ../components/adapters/lvgl_rm67162/include
)
target_link_libraries(tesla_headup_lvgl_dashboard_canvas
    PUBLIC
        tesla_headup_dashboard
        lvgl::lvgl
)
target_compile_features(tesla_headup_lvgl_dashboard_canvas PUBLIC cxx_std_20)
target_compile_definitions(
    tesla_headup_lvgl_dashboard_canvas
    PUBLIC ${TESLA_HEADUP_DASHBOARD_LAYOUT_DEFINITION}
)

add_executable(tesla_headup_simulator
    lvgl_sdl_display.cpp
    main.cpp
)
target_include_directories(tesla_headup_simulator PRIVATE ../components/adapters/lvgl_rgb/include)

target_link_libraries(tesla_headup_simulator
    PRIVATE
        tesla_headup_hud_runtime
        tesla_headup_lvgl_dashboard_canvas
        ${TESLA_HEADUP_SDL_TARGET}
)
target_compile_features(tesla_headup_simulator PRIVATE cxx_std_20)

if(WIN32 AND TARGET SDL2::SDL2)
    add_custom_command(TARGET tesla_headup_simulator POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:tesla_headup_simulator>")
endif()

if(TESLA_HEADUP_BUILD_TESTS)
    set(TESLA_HEADUP_CANVAS_TEST_SOURCE ../tests/integration/lvgl_dashboard_canvas_test.cpp)
    if(TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "true_hud")
        set(TESLA_HEADUP_CANVAS_TEST_SOURCE ../tests/integration/lvgl_true_hud_dashboard_canvas_test.cpp)
    endif()
    add_executable(tesla_headup_lvgl_dashboard_canvas_tests
        ${TESLA_HEADUP_CANVAS_TEST_SOURCE}
        ../tests/test_main.cpp
    )
    if(TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "road_scene")
        target_sources(tesla_headup_lvgl_dashboard_canvas_tests PRIVATE
            ../tests/integration/prepared_image_storage_test.cpp)
    endif()
    target_include_directories(tesla_headup_lvgl_dashboard_canvas_tests PRIVATE
        ../tests/support
    )
    target_link_libraries(tesla_headup_lvgl_dashboard_canvas_tests PRIVATE
        tesla_headup_lvgl_dashboard_canvas
    )
    if(TESLA_HEADUP_DASHBOARD_LAYOUT STREQUAL "road_scene")
        target_link_libraries(tesla_headup_lvgl_dashboard_canvas_tests PRIVATE
            tesla_headup_hud_runtime tesla_headup_dashkit_tesla)
    endif()
    target_compile_features(tesla_headup_lvgl_dashboard_canvas_tests PRIVATE cxx_std_20)
    add_test(
        NAME tesla_headup_lvgl_dashboard_canvas_tests
        COMMAND tesla_headup_lvgl_dashboard_canvas_tests
    )
    add_executable(tesla_headup_rgb_dashboard_tests
        ../tests/integration/rgb_dashboard_test.cpp
        ../tests/test_main.cpp
    )
    target_include_directories(tesla_headup_rgb_dashboard_tests PRIVATE
        ../tests/support ../components/adapters/lvgl_rgb/include)
    target_link_libraries(tesla_headup_rgb_dashboard_tests PRIVATE tesla_headup_lvgl_dashboard_canvas)
    target_compile_features(tesla_headup_rgb_dashboard_tests PRIVATE cxx_std_20)
    add_test(NAME tesla_headup_rgb_dashboard_tests COMMAND tesla_headup_rgb_dashboard_tests)

    add_executable(tesla_headup_sdl_display_tests
        lvgl_sdl_display.cpp
        ../tests/integration/sdl_display_test.cpp
        ../tests/test_main.cpp)
    target_include_directories(tesla_headup_sdl_display_tests PRIVATE
        . ../tests/support ../components/adapters/lvgl_rgb/include)
    target_link_libraries(tesla_headup_sdl_display_tests PRIVATE
        tesla_headup_lvgl_dashboard_canvas ${TESLA_HEADUP_SDL_TARGET})
    if(WIN32 AND TARGET SDL2::SDL2)
        add_custom_command(TARGET tesla_headup_sdl_display_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL2::SDL2>" "$<TARGET_FILE_DIR:tesla_headup_sdl_display_tests>")
    endif()
    target_compile_features(tesla_headup_sdl_display_tests PRIVATE cxx_std_20)
    add_test(NAME tesla_headup_sdl_display_tests COMMAND tesla_headup_sdl_display_tests)
    set_tests_properties(tesla_headup_sdl_display_tests PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 60)
    add_test(NAME tesla_headup_rgb_simulator_smoke COMMAND tesla_headup_simulator
        --rgb --direct-view --smoke-test)
    set_tests_properties(tesla_headup_rgb_simulator_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    foreach(pattern red green blue white black checkerboard corners viewport)
        add_test(NAME tesla_headup_rgb_diagnostic_${pattern}_smoke COMMAND tesla_headup_simulator
            --rgb-diagnostics --diagnostic-pattern ${pattern} --origin-x 17 --origin-y 33
            --direct-view --smoke-test)
        set_tests_properties(tesla_headup_rgb_diagnostic_${pattern}_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    endforeach()
    add_test(NAME tesla_headup_rgb_diagnostic_invalid_smoke COMMAND tesla_headup_simulator
        --rgb-diagnostics --diagnostic-pattern invalid --smoke-test)
    set_tests_properties(tesla_headup_rgb_diagnostic_invalid_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy" WILL_FAIL TRUE)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(tesla_headup_simulator_targets tesla_headup_lvgl_dashboard_canvas tesla_headup_simulator)
    if(TESLA_HEADUP_BUILD_TESTS)
        list(APPEND tesla_headup_simulator_targets tesla_headup_lvgl_dashboard_canvas_tests tesla_headup_rgb_dashboard_tests tesla_headup_sdl_display_tests)
    endif()
    foreach(target IN LISTS tesla_headup_simulator_targets)
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endforeach()
endif()
