cmake_minimum_required(VERSION 3.20)
project(infantry_soldier_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(infantry_soldier_bench infantry_soldier_bench.cpp)

# Enable OpenMP if available
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(infantry_soldier_bench PUBLIC OpenMP::OpenMP_CXX)
endif()
