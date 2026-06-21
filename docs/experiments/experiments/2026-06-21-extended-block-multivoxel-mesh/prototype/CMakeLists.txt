cmake_minimum_required(VERSION 3.28)
project(multivoxel_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(multivoxel_bench multivoxel_bench.cpp)
target_compile_options(multivoxel_bench PRIVATE
  -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic)
