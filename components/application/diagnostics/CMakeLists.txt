set(DIAGNOSTICS_SOURCES
    "src/bounded_can_frame_capture.cpp"
)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS ${DIAGNOSTICS_SOURCES}
        INCLUDE_DIRS "include"
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
else()
    add_library(tesla_headup_diagnostics ${DIAGNOSTICS_SOURCES})
    target_include_directories(tesla_headup_diagnostics PUBLIC include)
    target_compile_features(tesla_headup_diagnostics PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_diagnostics PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
