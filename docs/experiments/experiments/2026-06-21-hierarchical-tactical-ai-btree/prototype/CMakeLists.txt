cmake_minimum_required(VERSION 3.20)
project(btree_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Force clang++ (per hardware-profile.md §6: clang 22.1.6 is dev host compiler)
if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Overriding compiler to clang++")
    set(CMAKE_CXX_COMPILER "clang++")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(btree_bench btree_bench.cpp)
