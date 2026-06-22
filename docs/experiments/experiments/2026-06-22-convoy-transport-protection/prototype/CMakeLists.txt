cmake_minimum_required(VERSION 3.28)
project(convoy_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(convoy_bench convoy_bench.cpp)
target_compile_options(convoy_bench PRIVATE -Wall -Wextra -O3 -march=native)
