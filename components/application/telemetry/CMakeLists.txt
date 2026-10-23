if(COMMAND idf_component_register)
    idf_component_register(
        SRCS "src/telemetry_store.cpp"
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
    add_library(tesla_headup_telemetry
        src/telemetry_store.cpp
    )

    target_include_directories(tesla_headup_telemetry PUBLIC include)
    target_link_libraries(tesla_headup_telemetry PUBLIC tesla_headup_domain)
    target_compile_features(tesla_headup_telemetry PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_telemetry PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
