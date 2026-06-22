cmake_minimum_required(VERSION 3.20)
project(stealth_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(stealth_bench stealth_bench.cpp)

# Enable optimization
target_compile_options(stealth_bench PRIVATE -O3 -march=native -Wall -Wextra -Wpedantic)
