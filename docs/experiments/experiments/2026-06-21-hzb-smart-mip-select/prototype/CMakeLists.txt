cmake_minimum_required(VERSION 3.25)
project(hzb_smart_mip_bench LANGUAGES CXX)

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
)

add_executable(hzb_smart_mip_bench
    hzb_smart_mip_bench.cpp
    cull_simulator.cpp
    ground_truth_raycaster.cpp
)
