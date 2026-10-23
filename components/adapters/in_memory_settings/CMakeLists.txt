if(COMMAND idf_component_register)
    idf_component_register(
        INCLUDE_DIRS "include"
        REQUIRES settings
    )
    target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
else()
    add_library(tesla_headup_in_memory_settings INTERFACE)
    target_include_directories(tesla_headup_in_memory_settings INTERFACE include)
    target_link_libraries(tesla_headup_in_memory_settings INTERFACE tesla_headup_settings)
    target_compile_features(tesla_headup_in_memory_settings INTERFACE cxx_std_20)
endif()
