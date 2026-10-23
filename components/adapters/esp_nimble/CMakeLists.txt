if(NOT COMMAND idf_component_register)
    message(FATAL_ERROR "esp_nimble is an ESP-IDF-only component")
endif()

idf_component_register(
    SRCS "src/esp_nimble_central.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_timer freertos dashkit_ble
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

