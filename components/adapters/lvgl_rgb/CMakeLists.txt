if(NOT COMMAND idf_component_register)
    message(FATAL_ERROR "lvgl_rgb hardware transport is ESP-IDF-only")
endif()

set(rgb_sources "")
if(CONFIG_TESLA_HEADUP_DISPLAY_RGB)
    set(rgb_sources "src/newhaven_rgb_display.cpp")
endif()
idf_component_register(
    SRCS ${rgb_sources}
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "../../../firmware/boards/devkitc_n32r16v"
    REQUIRES lvgl_rm67162 esp_lcd esp_timer esp_psram esp_driver_gpio esp_driver_ledc
)
if(CONFIG_TESLA_HEADUP_DISPLAY_RGB)
    target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
endif()
