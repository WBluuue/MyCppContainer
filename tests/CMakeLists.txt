add_executable(container_lab_tests 
    test_smoke.cpp
    test_vector.cpp
)

target_link_libraries(container_lab_tests
    PRIVATE 
        mycppcontainer
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(container_lab_tests)

if(MYCPPCONTAINER_ENABLE_SANITIZERS)
    target_compile_options(container_lab_tests PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )

    target_link_options(container_lab_tests PRIVATE
        -fsanitize=address,undefined
    )
endif()