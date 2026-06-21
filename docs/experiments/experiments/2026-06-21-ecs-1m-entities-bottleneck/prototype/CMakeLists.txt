cmake_minimum_required(VERSION 3.30)
project(ecs_bench LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../external/flecs
    ${CMAKE_CURRENT_BINARY_DIR}/flecs EXCLUDE_FROM_ALL)

add_executable(ecs_bench ecs_bench.cpp)
target_link_libraries(ecs_bench PRIVATE flecs_static)
target_include_directories(ecs_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../external/flecs/include)
