cmake_minimum_required(VERSION 3.28)
project(resource_harvest_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic")

add_executable(resource_harvest_bench resource_harvest_bench.cpp)
