cmake_minimum_required(VERSION 3.28)
project(light_propagation_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")

add_executable(light_propagation_bench light_propagation_bench.cpp)

target_compile_options(light_propagation_bench PRIVATE
    -O3 -march=native -DNDEBUG
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)
