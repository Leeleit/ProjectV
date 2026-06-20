cmake_minimum_required(VERSION 3.20)
project(hzb_binding_bench CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

find_package(Vulkan REQUIRED)

# Shaders go in ${CMAKE_BINARY_DIR}/shaders/ so the runtime can find them
# relative to the working directory (= build dir).
set(SHADER_DIR ${CMAKE_BINARY_DIR}/shaders)

add_custom_command(
        OUTPUT ${SHADER_DIR}/sample_textureLod.comp.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DIR}
        COMMAND glslangValidator -V -o ${SHADER_DIR}/sample_textureLod.comp.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_textureLod.comp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_textureLod.comp
        COMMENT "Compiling sample_textureLod.comp"
        VERBATIM
)

add_custom_command(
        OUTPUT ${SHADER_DIR}/sample_texelFetch_storage.comp.spv
        COMMAND glslangValidator -V -o ${SHADER_DIR}/sample_texelFetch_storage.comp.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_texelFetch_storage.comp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_texelFetch_storage.comp
        COMMENT "Compiling sample_texelFetch_storage.comp"
        VERBATIM
)

add_custom_command(
        OUTPUT ${SHADER_DIR}/sample_texelFetch_sampled.comp.spv
        COMMAND glslangValidator -V -o ${SHADER_DIR}/sample_texelFetch_sampled.comp.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_texelFetch_sampled.comp
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/sample_texelFetch_sampled.comp
        COMMENT "Compiling sample_texelFetch_sampled.comp"
        VERBATIM
)

add_custom_target(hzb_shaders ALL
        DEPENDS
        ${SHADER_DIR}/sample_textureLod.comp.spv
        ${SHADER_DIR}/sample_texelFetch_storage.comp.spv
        ${SHADER_DIR}/sample_texelFetch_sampled.comp.spv
)

add_executable(hzb_bench src/main.cpp)
target_link_libraries(hzb_bench PRIVATE Vulkan::Vulkan)
target_include_directories(hzb_bench PRIVATE ${Vulkan_INCLUDE_DIRS})
add_dependencies(hzb_bench hzb_shaders)
