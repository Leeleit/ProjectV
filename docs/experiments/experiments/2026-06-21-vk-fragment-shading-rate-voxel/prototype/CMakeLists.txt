cmake_minimum_required(VERSION 3.28)
project(vrs_voxel_sim CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Expected Clang 22.1.x per `agent/knowledge.md` baseline; found ${CMAKE_CXX_COMPILER_ID}")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

add_executable(vrs_voxel_sim vrs_voxel_sim.cpp)

set_target_properties(vrs_voxel_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    OUTPUT_NAME "vrs_voxel_sim"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(vrs_voxel_sim PRIVATE
        $<$<CONFIG:Release>:-O3;-march=native;-DNDEBUG>
    )
endif()
