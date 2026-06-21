cmake_minimum_required(VERSION 3.20)
project(flight_model_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(flight_model_bench flight_model_bench.cpp)

target_compile_options(flight_model_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
