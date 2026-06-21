cmake_minimum_required(VERSION 3.20)
project(aoi_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  message(WARNING "Expected Clang/GCC; got ${CMAKE_CXX_COMPILER_ID}")
endif()

add_compile_options(
  -O3
  -march=native
  -DNDEBUG
  -Wall
  -Wextra
  -Wpedantic
)

add_executable(aoi_bench aoi_bench.cpp)
target_include_directories(aoi_bench PRIVATE ${CMAKE_SOURCE_DIR})
