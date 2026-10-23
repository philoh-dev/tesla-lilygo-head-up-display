set(HUD_RUNTIME_SOURCES
    "src/autopilot_presenter.cpp"
    "src/blind_spot_presenter.cpp"
    "src/dashboard_render_scheduler.cpp"
    "src/dashboard_settings_applier.cpp"
    "src/display_self_test_source.cpp"
    "src/exterior_light_presenter.cpp"
    "src/selected_gear_presenter.cpp"
    "src/speed_limit_presenter.cpp"
    "src/system_status_presenter.cpp"
    "src/turn_signal_presenter.cpp"
    "src/vehicle_speed_presenter.cpp"
)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS ${HUD_RUNTIME_SOURCES}
        INCLUDE_DIRS "include"
        REQUIRES dashboard telemetry
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
else()
    add_library(tesla_headup_hud_runtime ${HUD_RUNTIME_SOURCES})
    target_include_directories(tesla_headup_hud_runtime PUBLIC include)
    target_link_libraries(tesla_headup_hud_runtime
        PUBLIC
            tesla_headup_dashboard
            tesla_headup_telemetry
    )
    target_compile_features(tesla_headup_hud_runtime PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_hud_runtime PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
