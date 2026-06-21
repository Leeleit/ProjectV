cmake_minimum_required(VERSION 3.20)
project(RadarSimBench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(radar_sim_bench radar_sim_bench.cpp)

target_compile_options(radar_sim_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

target_link_libraries(radar_sim_bench PRIVATE)
