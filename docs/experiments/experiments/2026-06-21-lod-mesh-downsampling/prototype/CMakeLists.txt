cmake_minimum_required(VERSION 3.25)
project(lod_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(WARNING "Recommended compiler: clang++ for full C++26 support")
endif()

add_compile_options(
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
)

add_executable(lod_bench lod_bench.cpp)
