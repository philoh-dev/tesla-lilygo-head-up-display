if(COMMAND idf_component_register)
    idf_component_register(
        SRCS
            "src/dashkit_status_mapper.cpp"
            "src/dashkit_telemetry_ingestor.cpp"
        INCLUDE_DIRS "include"
        REQUIRES
            dashkit_ble
            dashkit_protocol
            diagnostics
            hud_runtime
            telemetry
            tesla_decoder
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
else()
    add_library(tesla_headup_dashkit_tesla
        src/dashkit_status_mapper.cpp
        src/dashkit_telemetry_ingestor.cpp
    )
    target_include_directories(tesla_headup_dashkit_tesla PUBLIC include)
    target_link_libraries(tesla_headup_dashkit_tesla
        PUBLIC
            tesla_headup_dashkit_protocol
            tesla_headup_dashkit_ble
            tesla_headup_diagnostics
            tesla_headup_hud_runtime
            tesla_headup_telemetry
            tesla_headup_tesla_decoder
    )
    target_compile_features(tesla_headup_dashkit_tesla PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_dashkit_tesla PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
