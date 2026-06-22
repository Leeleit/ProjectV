cmake_minimum_required(VERSION 3.20)
project(soldier_role_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(soldier_role_bench soldier_role_bench.cpp)

target_compile_options(soldier_role_bench PRIVATE
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)
