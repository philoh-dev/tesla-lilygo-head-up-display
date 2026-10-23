if(NOT COMMAND idf_component_register)
    return()
endif()

idf_component_register(
    SRCS "src/esp_log_can_frame_sink.cpp"
    INCLUDE_DIRS "include"
    REQUIRES diagnostics log
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror
)
