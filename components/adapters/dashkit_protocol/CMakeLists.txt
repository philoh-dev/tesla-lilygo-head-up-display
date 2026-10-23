if(COMMAND idf_component_register)
    idf_component_register(
        SRCS "src/dashkit_packet_parser.cpp"
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
    add_library(tesla_headup_dashkit_protocol
        src/dashkit_packet_parser.cpp
    )

    target_include_directories(tesla_headup_dashkit_protocol PUBLIC include)
    target_compile_features(tesla_headup_dashkit_protocol PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_dashkit_protocol PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
