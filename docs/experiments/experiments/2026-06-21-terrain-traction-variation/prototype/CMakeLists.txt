cmake_minimum_required(VERSION 3.20)
project(terrain_traction_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(terrain_traction_bench terrain_traction_bench.cpp)

# Enable OpenMP if available
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(terrain_traction_bench PUBLIC OpenMP::OpenMP_CXX)
endif()
