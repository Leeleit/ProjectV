cmake_minimum_required(VERSION 3.20)
project(vtol_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the ProjectV build flags per benchmarks/methodology.md
add_compile_options(
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

# Use steady_clock + chrono (header-only, no extra deps)
add_executable(vtol_bench vtol_bench.cpp)
