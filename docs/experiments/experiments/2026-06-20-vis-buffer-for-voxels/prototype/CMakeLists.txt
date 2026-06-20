cmake_minimum_required(VERSION 3.20)
project(visbuffer_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_LINKER_TYPE LLD)

find_package(Vulkan REQUIRED)

add_executable(visbuffer_bench
        src/main.cpp
        src/vulkan_setup.cpp
        src/scene.cpp
        src/pipeline_baseline.cpp
        src/benchmark.cpp
)

target_include_directories(visbuffer_bench PRIVATE src ${Vulkan_INCLUDE_DIRS})
target_link_libraries(visbuffer_bench PRIVATE ${Vulkan_LIBRARIES} m)

target_compile_options(visbuffer_bench PRIVATE
        -Wall -Wextra -Wno-unused-parameter
        -O3 -ffp-contract=off
        -march=native
)

# Shaders are compiled to .spv manually (glslc invoked in build script / setup).
# Runtime loads them via fopen. CMake just needs the .spv files to exist (developer ran glslc).
file(GLOB SHADER_SPV "shaders/*.spv")
add_custom_target(generate_shaders DEPENDS ${SHADER_SPV})
add_dependencies(visbuffer_bench generate_shaders)
