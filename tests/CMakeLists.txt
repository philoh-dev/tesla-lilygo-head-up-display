add_executable(tesla_headup_tests
    test_main.cpp
    integration/synthetic_dashkit_replay_test.cpp
    unit/blind_spot_warnings_test.cpp
    unit/battery_charge_pipeline_test.cpp
    unit/can_frame_diagnostics_test.cpp
    unit/devkitc_board_profile_test.cpp
    unit/rgb_frame_test.cpp
    unit/rgb_diagnostics_test.cpp
    unit/dashboard_test.cpp
    unit/dashboard_settings_applier_test.cpp
    unit/display_self_test_source_test.cpp
    unit/dashkit_connection_test.cpp
    unit/dashkit_packet_parser_test.cpp
    unit/dashkit_status_mapper_test.cpp
    unit/driver_assistance_pipeline_test.cpp
    unit/exterior_lights_test.cpp
    unit/exterior_light_pipeline_test.cpp
    unit/gear_pipeline_test.cpp
    unit/power_pipeline_test.cpp
    unit/settings_test.cpp
    unit/speed_pipeline_test.cpp
    unit/speed_limit_pipeline_test.cpp
    unit/system_status_test.cpp
    unit/tesla_driver_assistance_decoder_test.cpp
    unit/tesla_exterior_lights_decoder_test.cpp
    unit/tesla_gear_decoder_test.cpp
    unit/tesla_speed_decoder_test.cpp
    unit/tesla_turn_signal_decoder_test.cpp
    unit/telemetry_store_test.cpp
    unit/turn_signal_pipeline_test.cpp
    unit/turn_signals_test.cpp
    unit/vehicle_speed_test.cpp
)

target_link_libraries(tesla_headup_tests PRIVATE
    tesla_headup_dashkit_ble
    tesla_headup_dashkit_tesla
    tesla_headup_diagnostics
    tesla_headup_esp_nvs_settings_codec
    tesla_headup_hud_runtime
    tesla_headup_in_memory_settings
    tesla_headup_settings
)
target_compile_features(tesla_headup_tests PRIVATE cxx_std_20)
target_include_directories(tesla_headup_tests PRIVATE fixtures support
    "${CMAKE_CURRENT_LIST_DIR}/../components/adapters/lvgl_rgb/include"
    "${CMAKE_CURRENT_LIST_DIR}/../firmware/boards/devkitc_n32r16v")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(tesla_headup_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
endif()

add_test(NAME tesla_headup_tests COMMAND tesla_headup_tests)
