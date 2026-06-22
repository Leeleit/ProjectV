cmake_minimum_required(VERSION 3.25)
project(aging_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic")

add_executable(aging_bench aging_bench.cpp)
