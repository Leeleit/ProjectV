cmake_minimum_required(VERSION 4.0)
project(decoder_pipeline_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

add_compile_options(
        -O3
        -march=native
        -DNDEBUG
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(decoder_pipeline_bench decoder_pipeline_bench.cpp)