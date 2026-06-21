cmake_minimum_required(VERSION 3.28)
# Plain CXX project — we use the standard Vulkan API linked from libvulkan.so.
# No volk needed (no extensions beyond Vulkan 1.4 core are used by this prototype).
project(gpu-fluid-ca-atomic-strategy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-O3 -march=native -DNDEBUG -Wall -Wextra -Wno-unused-parameter)

# ProjectV vendored external libs (5 levels up: prototype/ → experiments/2026-06-21-.../ → experiments/ → docs/ → ProjectV/)
set(PROJECTV_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)
set(VMA_INCLUDE_DIR ${PROJECTV_ROOT}/external/VulkanMemoryAllocator/include)

if(NOT EXISTS ${VMA_INCLUDE_DIR}/vk_mem_alloc.h)
    message(FATAL_ERROR "VMA not found at ${VMA_INCLUDE_DIR}; expected from ProjectV vendored external/")
endif()

include_directories(
    ${VMA_INCLUDE_DIR}
    .
)

find_package(Vulkan REQUIRED)

# VMA + Vulkan API (regular function prototypes from vulkan.h, linked from libvulkan.so)
add_executable(atomic_bench
    main.cpp
)
target_link_libraries(atomic_bench PRIVATE Vulkan::Vulkan m)

# GLSL → SPIR-V via glslc (Vulkan 1.4 target).
# 7 SPIR-V files: 5 single-build (A/B/D/E/F via strategies.comp + -DSTRATEGY_ID=N),
# 2 separate files for Strategy C 2-stage (collect + writeback).
set(STRATEGY_SPV_FILES
    S0.spv  S1.spv  S3.spv  S4.spv  S5.spv
    C_collect.spv  C_writeback.spv
)

# Single-build strategies (A=0, B=1, D=3, E=4, F=5)
foreach(ID 0 1 3 4 5)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/S${ID}.spv
        COMMAND glslc
                -fshader-stage=comp
                -O
                --target-env=vulkan1.4
                -DSTRATEGY_ID=${ID}
                ${CMAKE_CURRENT_SOURCE_DIR}/strategies.comp
                -o ${CMAKE_CURRENT_BINARY_DIR}/S${ID}.spv
        DEPENDS strategies.comp
        COMMENT "Compiling strategies.comp -DSTRATEGY_ID=${ID} → S${ID}.spv"
        VERBATIM
    )
    list(APPEND STRATEGY_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/S${ID}.spv)
endforeach()

# Strategy C 2-stage (separate files)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/C_collect.spv
    COMMAND glslc
            -fshader-stage=comp
            -O
            --target-env=vulkan1.4
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies_C_collect.comp
            -o ${CMAKE_CURRENT_BINARY_DIR}/C_collect.spv
    DEPENDS strategies_C_collect.comp
    COMMENT "Compiling strategies_C_collect.comp → C_collect.spv"
    VERBATIM
)
list(APPEND STRATEGY_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/C_collect.spv)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/C_writeback.spv
    COMMAND glslc
            -fshader-stage=comp
            -O
            --target-env=vulkan1.4
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies_C_writeback.comp
            -o ${CMAKE_CURRENT_BINARY_DIR}/C_writeback.spv
    DEPENDS strategies_C_writeback.comp
    COMMENT "Compiling strategies_C_writeback.comp → C_writeback.spv"
    VERBATIM
)
list(APPEND STRATEGY_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/C_writeback.spv)

add_custom_target(atomic_bench_shaders ALL
    DEPENDS ${STRATEGY_OUTPUTS}
)
add_dependencies(atomic_bench atomic_bench_shaders)

# Copy SPIR-V next to binary for easy execution
foreach(SPV ${STRATEGY_SPV_FILES})
    add_custom_command(TARGET atomic_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/${SPV}
            $<TARGET_FILE_DIR:atomic_bench>/${SPV}
        VERBATIM
    )
endforeach()
