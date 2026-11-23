add_executable(container_lab_tests test_smoke.cpp)

target_link_libraries(container_lab_tests
    PRIVATE 
        mycppcontainer
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(container_lab_tests)