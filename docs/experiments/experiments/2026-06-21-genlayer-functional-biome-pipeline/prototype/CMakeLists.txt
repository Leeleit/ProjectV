cmake_minimum_required(VERSION 3.28)
project(genlayer_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic")

add_executable(genlayer_bench genlayer_bench.cpp)

# Auto-detect TBB for parallel execution policies
find_package(TBB QUIET)
if(TBB_FOUND)
    target_link_libraries(genlayer_bench PRIVATE TBB::tbb)
    target_compile_definitions(genlayer_bench PRIVATE TBB_AVAILABLE)
    message(STATUS "TBB found - parallel execution policy enabled")
else()
    message(STATUS "TBB not found - parallel execution policy may fall back to sequential")
endif()
