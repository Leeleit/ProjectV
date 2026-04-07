cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(ProjectV
        VERSION 0.0.1
        LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectVThirdParty)
include(CTest)

option(PROJECTV_ENABLE_TRACY "Enable Tracy profiling instrumentation" ON)
option(PROJECTV_BUILD_TRACY_PROFILER "Build bundled Tracy profiler UI" OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#-------------------------Submodules-------------------------

# SDL
set(SDL_STATIC ON CACHE BOOL "Static linking for SDL" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)
add_subdirectory(external/SDL)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/SDL")

# --- Vulkan Stack ---

# Vulkan
find_package(Vulkan REQUIRED)

# Volk
set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
add_subdirectory(external/volk)

# Vulkan Memory Allocator
add_subdirectory(external/VulkanMemoryAllocator)

# fmt
set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install target" FORCE)
set(FMT_MODULE OFF CACHE BOOL "Disable fmt module target" FORCE)
add_subdirectory(external/fmt)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/fmt")

# flecs
set(FLECS_STATIC ON CACHE BOOL "Build static flecs lib" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "Disable shared flecs lib" FORCE)
set(FLECS_TESTS OFF CACHE BOOL "Disable flecs tests" FORCE)
add_subdirectory(external/flecs)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/flecs")

# JoltPhysics
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Disable Jolt warnings-as-errors" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "Keep parent compiler flags for Jolt" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "Disable Jolt IPO" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "Disable Jolt install target" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use shared MSVC runtime for Jolt" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Disable Jolt debug renderer" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "Disable Jolt profiler" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static Jolt lib" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "Disable Jolt DX12 compute" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "Disable Jolt Vulkan compute" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "Disable Jolt Metal compute" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "Disable Jolt CPU compute path" FORCE)
add_subdirectory(external/JoltPhysics/Build)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/JoltPhysics/Build")

projectv_add_tracy()

# --- Vulkan Stack End ---


#------------------------Submodules-End----------------------

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
