if(COMMAND idf_component_register)
    idf_component_register(
        SRCS
            "src/tesla_battery_charge_decoder.cpp"
            "src/tesla_driver_assistance_decoder.cpp"
            "src/tesla_gear_decoder.cpp"
            "src/tesla_power_decoder.cpp"
            "src/tesla_speed_decoder.cpp"
            "src/tesla_turn_signal_decoder.cpp"
        INCLUDE_DIRS "include"
        REQUIRES domain dashkit_protocol
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
else()
    add_library(tesla_headup_tesla_decoder
        src/tesla_battery_charge_decoder.cpp
        src/tesla_driver_assistance_decoder.cpp
        src/tesla_gear_decoder.cpp
        src/tesla_power_decoder.cpp
        src/tesla_speed_decoder.cpp
        src/tesla_turn_signal_decoder.cpp
    )

    target_include_directories(tesla_headup_tesla_decoder PUBLIC include)
    target_link_libraries(tesla_headup_tesla_decoder
        PUBLIC
            tesla_headup_dashkit_protocol
            tesla_headup_domain
    )
    target_compile_features(tesla_headup_tesla_decoder PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_tesla_decoder PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
