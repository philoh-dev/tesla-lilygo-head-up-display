if(COMMAND idf_component_register)
    idf_component_register(
        SRCS "src/dashkit_connection.cpp"
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
    add_library(tesla_headup_dashkit_ble
        src/dashkit_connection.cpp
    )

    target_include_directories(tesla_headup_dashkit_ble
        PUBLIC
            include
    )

    target_compile_features(tesla_headup_dashkit_ble PUBLIC cxx_std_20)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(tesla_headup_dashkit_ble PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
        )
    endif()
endif()
