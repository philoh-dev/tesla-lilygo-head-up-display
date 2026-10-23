if(TESLA_HEADUP_FIRMWARE_MODE STREQUAL "board_bring_up")
    idf_component_register(
        SRCS "board_bring_up.cpp"
        INCLUDE_DIRS "../boards/devkitc_n32r16v"
        REQUIRES esp_psram spi_flash heap
    )
elseif(TESLA_HEADUP_FIRMWARE_MODE STREQUAL "rgb_diagnostics")
    idf_component_register(
        SRCS "rgb_diagnostics.cpp"
        REQUIRES lvgl_rgb esp_timer freertos
    )
else()
    # During ESP-IDF's early requirement scan, custom project variables may
    # not be available. Include the bring-up dependencies here as well so the
    # dependency graph is valid in either pass; this does not initialize them.
    idf_component_register(
        SRCS "main.cpp"
        REQUIRES
            dashboard
            dashkit_ble
            dashkit_tesla
            diagnostics
            esp_diagnostics
            esp_nimble
            esp_nvs_settings
            esp_timer
            esp_psram
            spi_flash
            heap
            hud_runtime
            lvgl_rm67162
            lvgl_rgb
            nvs_flash
            settings
            telemetry
    )
endif()

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)
