# CMakeLists.txt — 2026-06-22-procedural-weapon-fire-vfx-particle-system prototype
#
# Standalone C++26 CPU prototype (analytical GPU cost model).
# Build inside prototype/ — do NOT link against mainline ProjectV per AGENTS.md.

cmake_minimum_required(VERSION 4.0)
project(projectv_vfx_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Per hardware-profile.md §6 — Clang 22.1.6, LLD, libstdc++ 16
add_compile_options(
    -O3
    -march=native
    -DNDEBUG
    -Wall
    -Wextra
    -Wpedantic
    -Wno-unused-parameter
    -fno-math-errno
)

add_executable(vfx_bench vfx_bench.cpp)

# Allow include of standalone headers
target_include_directories(vfx_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
