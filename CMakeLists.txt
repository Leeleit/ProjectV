cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(ProjectV
        VERSION 0.0.1
        LANGUAGES C CXX
        DESCRIPTION "ProjectV – Voxel Engine"
)

# =============================================================================
# 1. Global Settings
# =============================================================================

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# 2. Dependencies
# =============================================================================

set(DRACO_WERROR OFF CACHE BOOL "Disable Draco WError" FORCE)
set(SDL_WERROR OFF CACHE BOOL "Disable SDL WError" FORCE)
set(RMLUI_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable RmlUi WError" FORCE)

find_package(Vulkan REQUIRED)

# --- Volk ---
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(external/volk)

add_subdirectory(external/VulkanMemoryAllocator)

# --- Draco ---
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_subdirectory(external/draco)

add_subdirectory(external/fastgltf)
add_subdirectory(external/flecs)

# --- Freetype ---
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
add_subdirectory(external/freetype)
add_library(Freetype::Freetype ALIAS freetype)

add_subdirectory(external/glaze)
add_subdirectory(external/glm)

# --- ImGui (Manual Build) ---
set(IMGUI_DIR "external/imgui")
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
        ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Vulkan Freetype::Freetype)

# --- Jolt Physics ---
set(JPH_DEBUG_RENDERER ON CACHE BOOL "" FORCE)
set(JPH_PROFILE_ENABLED ON CACHE BOOL "" FORCE)
add_subdirectory(external/JoltPhysics/Build)

add_subdirectory(external/meshoptimizer)
add_subdirectory(external/miniaudio)

# --- RmlUi ---
add_subdirectory(external/RmlUi)

# --- SDL3 ---
set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)
add_subdirectory(external/SDL)

# --- Tracy ---
option(TRACY_ENABLE "Enable Tracy profiling" ON)
add_subdirectory(external/tracy)

add_subdirectory(external/zstd)


set(THIRD_PARTY_TARGETS
        fastgltf
        Tracy TracyClient
        Jolt
        SDL3 SDL3-static
        rmlui_core rmlui_debugger
        meshoptimizer
        miniaudio
        draco_api draco_attributes draco_core draco_dec draco_enc draco_io
        draco_mesh draco_metadata draco_point_cloud
        draco_compression_attributes_dec draco_compression_attributes_enc
        draco_compression_attributes_pred_schemes_dec draco_compression_attributes_pred_schemes_enc
        draco_compression_bit_coders draco_compression_encode draco_compression_expert_encode
        draco_compression_mesh_dec draco_compression_mesh_enc draco_compression_mesh_traverser
        draco_compression_point_cloud_dec draco_compression_point_cloud_enc
        draco_metadata_dec draco_metadata_enc
        draco_animation draco_animation_dec draco_animation_enc
        draco_points_dec draco_points_enc
)

foreach (LIB_TARGET ${THIRD_PARTY_TARGETS})
    if (TARGET ${LIB_TARGET})
        target_compile_options(${LIB_TARGET} PRIVATE "/WX-")
        target_compile_options(${LIB_TARGET} PRIVATE
                "-Wno-error"
                "-Wno-unused-command-line-argument"
                "-Wno-unknown-argument"
                "-Wno-unknown-attributes"
                "-Wno-padded"
                "-Wno-implicit-const-int-float-conversion"
                "-Wno-range-loop-construct"
                "-Wno-unused-parameter"
                "-Wno-deprecated-declarations"
                "-Wno-ignored-qualifiers"
                "-Wno-cast-align"
                "-Wno-sign-compare"
                "-w"
                "/w"
        )
    endif ()
endforeach ()

# =============================================================================
# 3. Add The Source Code
# =============================================================================

add_subdirectory(src)
