cmake_minimum_required(VERSION 3.25)
project(logistics_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Per docs/experiments/benchmarks/methodology.md §2: Clang, -O3, -march=native, -DNDEBUG, -std=c++26
add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(logistics_bench logistics_bench.cpp)
