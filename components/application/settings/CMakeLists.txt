set(SETTINGS_SOURCES
    "src/settings_service.cpp"
)

if(COMMAND idf_component_register)
    idf_component_register(
        SRCS ${SETTINGS_SOURCES}
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
    add_library(tesla_headup_settings ${SETTINGS_SOURCES})
    target_include_directories(tesla_headup_settings PUBLIC include)
    target_link_libraries(tesla_headup_settings PUBLIC tesla_headup_domain)
    target_compile_features(tesla_headup_settings PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_settings PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
