cmake_minimum_required(VERSION 3.20)

project(MyCppContainer LANGUAGES CXX)

add_library(mycppcontainer INTERFACE)

target_include_directories(mycppcontainer INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(mycppcontainer INTERFACE cxx_std_20)

add_subdirectory(examples)

include(CTest)

if(BUILD_TESTING)
    find_package(Catch2 3 REQUIRED)
    add_subdirectory(tests)
endif()