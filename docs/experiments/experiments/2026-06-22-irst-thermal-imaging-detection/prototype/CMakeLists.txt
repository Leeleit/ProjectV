cmake_minimum_required(VERSION 3.28)
project(irst_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_executable(irst_bench irst_bench.cpp)
target_compile_options(irst_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
    -fconstexpr-steps=1000000000
)
