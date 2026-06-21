# prototype/CMakeLists.txt — minimal standalone Vulkan 1.4 compute benchmark
# (Not intended to be added to ProjectV mainline. This is research-only.)

cmake_minimum_required(VERSION 4.0)
project(gpu_noise_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find Vulkan 1.4
find_package(Vulkan REQUIRED)

add_executable(gpu_noise_bench main.cpp)
target_include_directories(gpu_noise_bench PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(gpu_noise_bench PRIVATE ${Vulkan_LIBRARIES})
target_compile_options(gpu_noise_bench PRIVATE -O3 -march=native -DNDEBUG -Wall -Wextra)

# Custom command: compile 5 noise kernel SPIR-V variants.
# glslc must be in PATH or specify absolute path.
find_program(GLSLC glslc REQUIRED)

foreach(v VARIANT_VALUE VARIANT_PERLIN VARIANT_SIMPLEX VARIANT_OPENSIMPLEX2 VARIANT_WORLEY)
    string(TOLOWER ${v} variant_lc)
    set(spv "${CMAKE_CURRENT_BINARY_DIR}/noise_${variant_lc}.spv")
    add_custom_command(
        OUTPUT ${spv}
        COMMAND ${GLSLC} -D${v} noise_kernels.comp -o ${spv}
        DEPENDS noise_kernels.comp
        COMMENT "Compiling ${v} -> noise_${variant_lc}.spv"
        VERBATIM
    )
    list(APPEND spv_outputs ${spv})
endforeach()

add_custom_target(noise_spirv ALL DEPENDS ${spv_outputs})
add_dependencies(gpu_noise_bench noise_spirv)
