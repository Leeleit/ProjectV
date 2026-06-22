cmake_minimum_required(VERSION 3.20)
project(evac_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(evac_bench evac_bench.cpp)

target_compile_options(evac_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
