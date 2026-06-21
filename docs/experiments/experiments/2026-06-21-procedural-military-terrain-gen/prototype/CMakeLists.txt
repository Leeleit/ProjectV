# CMakeLists.txt for 2026-06-21-procedural-military-terrain-gen prototype.
#
# Per agent/knowledge.md §17 Linux baseline: clang++ 22.1.6, Ninja, Release.

cmake_minimum_required(VERSION 3.20)
project(military_terrain_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(military_terrain_bench
    military_terrain_bench.cpp
)

target_link_libraries(military_terrain_bench PRIVATE stdc++ stdc++fs)
