cmake_minimum_required(VERSION 4.0)
project(greedy_physics_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_executable(greedy_physics_bench greedy_physics_bench.cpp)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(greedy_physics_bench PRIVATE
        -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic
    )
endif()

target_compile_features(greedy_physics_bench PRIVATE cxx_std_26)
