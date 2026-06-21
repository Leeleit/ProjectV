cmake_minimum_required(VERSION 3.28)
project(dynamic_light_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic")

add_executable(dynamic_light_bench dynamic_light_bench.cpp)
