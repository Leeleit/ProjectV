cmake_minimum_required(VERSION 3.20)
project(helicopter_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(helicopter_bench helicopter_bench.cpp)

target_compile_options(helicopter_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
