cmake_minimum_required(VERSION 4.3)
project(depth_quant_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
    -Wno-deprecated-declarations
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -DNDEBUG -ffp-contract=off)
endif()

# Analytical benchmark - no Vulkan deps required
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(depth_quant_bench
    main.cpp
    voxel_scene.cpp
    depth_quant_bench.cpp
)
target_link_libraries(depth_quant_bench PRIVATE pthread)
