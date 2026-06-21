cmake_minimum_required(VERSION 3.28)
project(dof_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra -DNDEBUG")

add_executable(dof_bench dof_bench.cpp)
