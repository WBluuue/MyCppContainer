cmake_minimum_required(VERSION 3.20)

project(MyCppContainer LANGUAGES CXX)

add_executable(container_lab_demo src/main.cpp)

target_compile_features(container_lab_demo PRIVATE cxx_std_20)