set(DASHBOARD_SOURCES
    "src/blind_spot_warnings.cpp"
    "src/centered_vehicle_speed.cpp"
    "src/dashboard.cpp"
    "src/exterior_light_indicators.cpp"
    "src/selected_gear_indicator.cpp"
    "src/speed_limit_indicator.cpp"
    "src/system_status_indicator.cpp"
    "src/turn_signal_indicators.cpp"
    "src/autopilot_indicator.cpp"
)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS ${DASHBOARD_SOURCES}
        INCLUDE_DIRS "include"
        REQUIRES domain
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
else()
    add_library(tesla_headup_dashboard ${DASHBOARD_SOURCES})
    target_include_directories(tesla_headup_dashboard PUBLIC include)
    target_link_libraries(tesla_headup_dashboard PUBLIC tesla_headup_domain)
    target_compile_features(tesla_headup_dashboard PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_dashboard PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
