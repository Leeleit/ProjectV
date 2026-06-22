cmake_minimum_required(VERSION 3.20)
project(fuel_ammo_maint_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

add_executable(fuel_ammo_maint_bench fuel_ammo_maint_bench.cpp)
