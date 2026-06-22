cmake_minimum_required(VERSION 3.20)
project(missile_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(missile_bench missile_bench.cpp)

target_compile_options(missile_bench PRIVATE -O3 -march=native -Wall -Wextra -Wpedantic)
