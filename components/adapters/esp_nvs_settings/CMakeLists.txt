if(COMMAND idf_component_register)
    idf_component_register(
        SRCS
            "src/esp_nvs_settings_store.cpp"
            "src/settings_record_codec.cpp"
        INCLUDE_DIRS "include"
        REQUIRES nvs_flash settings
    )
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wall
        -Wextra
        -Werror
    )
else()
    add_library(tesla_headup_esp_nvs_settings_codec
        src/settings_record_codec.cpp
    )
    target_include_directories(tesla_headup_esp_nvs_settings_codec PUBLIC include)
    target_link_libraries(tesla_headup_esp_nvs_settings_codec PUBLIC tesla_headup_settings)
    target_compile_features(tesla_headup_esp_nvs_settings_codec PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_esp_nvs_settings_codec PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
