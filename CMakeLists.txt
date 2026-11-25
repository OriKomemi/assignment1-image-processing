cmake_minimum_required(VERSION 3.16)
project(Assignment1 LANGUAGES CXX)

find_package(PNG REQUIRED)
add_executable(assignment1 src/main.cpp src/filters.cpp)
target_compile_features(assignment1 PRIVATE cxx_std_17)
target_link_libraries(assignment1 PRIVATE PNG::PNG)
