cmake_minimum_required(VERSION 3.20)
project(tracy_gpu_vs_manual LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Resolve ProjectV root and Tracy vendored path.
get_filename_component(_pv_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
set(TRACY_ROOT "${_pv_root}/external/tracy" CACHE PATH "Path to vendored Tracy")
set(VOLK_ROOT "${_pv_root}/external/volk" CACHE PATH "Path to vendored volk")

# volk — load Vulkan at runtime, no libvulkan link needed.
set(VOLK_IMPLEMENTATION ON CACHE BOOL "" FORCE)
add_subdirectory("${VOLK_ROOT}" volk_build EXCLUDE_FROM_ALL)

# Tracy client — single-file, MIT, vendored.
# We compile TracyClient.cpp as a static lib.
# Set options BEFORE add_subdirectory so Tracy's `option()` sees cache values.
set(TRACY_ENABLE          ON  CACHE BOOL "" FORCE)
# Use symbol table so Tracy can call KHR-promoted calibrated-timestamp functions
# via the instance/device procaddrs we provide (not the non-existent global
# `vkGetPhysicalDeviceCalibrateableTimeDomainsEXT` symbol on Vulkan 1.4).
add_compile_definitions(TRACY_VK_USE_SYMBOL_TABLE)
set(TRACY_NO_CALLSTACK    ON  CACHE BOOL "" FORCE)
set(TRACY_NO_BROADCAST    ON  CACHE BOOL "" FORCE)
set(TRACY_NO_CODE_TRANSFER ON CACHE BOOL "" FORCE)
set(TRACY_NO_CONTEXT_SWITCH ON CACHE BOOL "" FORCE)
set(TRACY_NO_SAMPLING     ON  CACHE BOOL "" FORCE)
set(TRACY_NO_VSYNC_CAPTURE ON CACHE BOOL "" FORCE)
set(TRACY_NO_FRAME_IMAGE  ON  CACHE BOOL "" FORCE)
set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "" FORCE)
set(TRACY_NO_CRASH_HANDLER ON CACHE BOOL "" FORCE)
set(TRACY_NO_EXIT         OFF CACHE BOOL "" FORCE)
add_subdirectory("${TRACY_ROOT}" tracy_build EXCLUDE_FROM_ALL)

# Build configs — Default (baseline) + Tracy-enabled.
add_executable(tracy_gpu_vs_manual bench.cpp)
target_include_directories(tracy_gpu_vs_manual PRIVATE "${VOLK_ROOT}/include")
target_link_libraries(tracy_gpu_vs_manual PRIVATE volk_headers)

add_executable(tracy_gpu_vs_manual_tracy bench.cpp)
target_compile_definitions(tracy_gpu_vs_manual_tracy PRIVATE PROFILE_TRACY=1)
target_include_directories(tracy_gpu_vs_manual_tracy PRIVATE
    "${VOLK_ROOT}/include"
    "${TRACY_ROOT}/public")
target_link_libraries(tracy_gpu_vs_manual_tracy PRIVATE volk_headers TracyClient)

# Wall + GPU timing via chrono + vkCmdWriteTimestamp (no external deps).
find_package(Threads REQUIRED)
target_link_libraries(tracy_gpu_vs_manual        PRIVATE Threads::Threads)
target_link_libraries(tracy_gpu_vs_manual_tracy PRIVATE Threads::Threads)

set_target_properties(tracy_gpu_vs_manual tracy_gpu_vs_manual_tracy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

message(STATUS "ProjectV root: ${_pv_root}")
message(STATUS "Tracy root:    ${TRACY_ROOT}")
message(STATUS "volk root:     ${VOLK_ROOT}")
