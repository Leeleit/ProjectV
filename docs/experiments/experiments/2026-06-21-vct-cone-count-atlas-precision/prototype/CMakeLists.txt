# prototype/CMakeLists.txt — minimal standalone Vulkan 1.4 compute benchmark
# (Not intended to be added to ProjectV mainline. This is research-only.)

cmake_minimum_required(VERSION 4.0)
project(vct_cone_atlas_bench CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

add_executable(vct_cone_atlas_bench vct_main.cpp)
target_include_directories(vct_cone_atlas_bench PRIVATE ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vct_cone_atlas_bench PRIVATE ${Vulkan_LIBRARIES})
target_compile_options(vct_cone_atlas_bench PRIVATE
    -O3 -march=native -DNDEBUG -Wall -Wextra -Wno-unused-parameter)

find_program(GLSLC glslc REQUIRED)

# 4 cone count variants (6 / 12 / 24 / 1024 reference) compiled as separate SPIR-V
foreach(c CONE_6 CONE_12 CONE_24 CONE_1024)
    set(spv "${CMAKE_CURRENT_BINARY_DIR}/cone_march_${c}.spv")
    add_custom_command(
        OUTPUT ${spv}
        COMMAND ${GLSLC} -D${c} ${CMAKE_CURRENT_SOURCE_DIR}/cone_march.comp -o ${spv}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/cone_march.comp
        COMMENT "Compiling cone_march ${c} -> ${spv}"
        VERBATIM
    )
    list(APPEND cone_spvs ${spv})
endforeach()

# Single atlas init (sets up synthetic content)
# (atlas content is filled on CPU; no atlas_init.comp needed for v1)

add_custom_target(vct_spirv ALL DEPENDS ${cone_spvs})
add_dependencies(vct_cone_atlas_bench vct_spirv)
