cmake_minimum_required(VERSION 3.20)
project(frame_pacing_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(frame_pacing_bench
    main.cpp
    triangle.vert.spv.h
    triangle.frag.spv.h)
target_include_directories(frame_pacing_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(frame_pacing_bench PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-but-set-variable
    -Wno-deprecated-declarations -Wno-missing-field-initializers)
target_link_libraries(frame_pacing_bench PRIVATE SDL3::SDL3 Vulkan::Vulkan)
