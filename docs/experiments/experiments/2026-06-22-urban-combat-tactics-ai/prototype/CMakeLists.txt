cmake_minimum_required(VERSION 3.20)
project(urban_combat_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict warnings per ProjectV convention (5-10% threshold + AGENTS.md §8 comment management).
add_compile_options(-Wall -Wextra -Wpedantic)

# Two build modes:
#   Release:  -O3 -march=native -DNDEBUG (default; runs benchmark)
#   Debug:    -O0 -g (with -fsanitize=address optional)
#
# Usage:
#   cmake -S prototype -B prototype/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build prototype/build
#   ./prototype/build/urban_combat_bench

add_executable(urban_combat_bench prototype/urban_combat_bench.cpp)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(urban_combat_bench PRIVATE -O3 -march=native -DNDEBUG)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(urban_combat_bench PRIVATE -O0 -g)
endif()

if(ENABLE_ASAN)
    target_compile_options(urban_combat_bench PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    target_link_options(urban_combat_bench PRIVATE -fsanitize=address)
endif()

# Output: results.csv + summary_means.csv written to CWD (build/ by default per cmake)