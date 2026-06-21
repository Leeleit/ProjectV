cmake_minimum_required(VERSION 4.0)
project(sdf_hybrid_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ProjectV mainline toolchain per hardware-profile.md §6 + agent/knowledge.md §17.
# Clang 22.1.6 + LLD + libstdc++.
add_compile_options(
    -O3
    -march=native
    -std=c++26
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
)

# Source files.
set(SDF_HYBRID_SOURCES
    sdf_overlay.cpp
    vct_cone_march.cpp
    physics_normals.cpp
    bench.cpp
)

add_executable(sdf_hybrid_bench ${SDF_HYBRID_SOURCES})
target_include_directories(sdf_hybrid_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sdf_hybrid_bench PRIVATE stdc++)

# Static analysis helper target (optional, for manual review).
add_custom_target(sdf_hybrid_check
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++26 -O3 -march=native -DNDEBUG
            -Wall -Wextra -Wpedantic -fsyntax-only
            sdf_overlay.cpp vct_cone_march.cpp physics_normals.cpp bench.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Syntax-only check (no codegen, no link)"
    VERBATIM
)
