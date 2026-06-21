cmake_minimum_required(VERSION 3.28)
project(aerial_perspective_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_executable(aerial_perspective_bench aerial_perspective_bench.cpp)

target_compile_options(aerial_perspective_bench PRIVATE
    -O3 -march=native -DNDEBUG
    -Wall -Wextra -Wpedantic -Werror
)
