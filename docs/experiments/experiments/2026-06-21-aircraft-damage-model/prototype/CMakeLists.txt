cmake_minimum_required(VERSION 3.20)
project(aircraft_damage_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(aircraft_damage_bench aircraft_damage_bench.cpp)

# Enable OpenMP if available (optional, but good for Zen 3 benchmarking)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(aircraft_damage_bench PUBLIC OpenMP::OpenMP_CXX)
endif()
