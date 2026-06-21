# Standalone Vulkan 1.4 prototype for frame-flight-allocator-budget experiment.
# NOT ProjectV's mainline build — pure isolated research.
# Vendors VMA 3.4.0 + volk from ProjectV/external/. Self-contained.

cmake_minimum_required(VERSION 3.25)
project(frame-flight-allocator-budget-prototype C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-nullability-extension       # VMA 3.4.0 uses _Nullable/_Nonnull from ObjC
    -Wno-unused-parameter
    -O3 -march=native -DNDEBUG
)

# Vendored dependencies from ProjectV/external/
# (prototype/ -> 2026-06-21-.../ -> experiments/ -> experiments/ -> docs/ -> ProjectV/)
set(PROJECTV_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." CACHE STRING "ProjectV root")
set(VMA_INCLUDE_DIR "${PROJECTV_ROOT}/external/VulkanMemoryAllocator/include")
set(VOLK_INCLUDE_DIR "${PROJECTV_ROOT}/external/volk")
set(VMA_SRC "${VMA_INCLUDE_DIR}/../src/VulkanSample.cpp") # Not needed; VMA is header-only.

# Validate paths exist.
if(NOT EXISTS "${VMA_INCLUDE_DIR}/vk_mem_alloc.h")
    message(FATAL_ERROR "VMA not found at ${VMA_INCLUDE_DIR}; check PROJECTV_ROOT")
endif()
if(NOT EXISTS "${VOLK_INCLUDE_DIR}/volk.h")
    message(FATAL_ERROR "volk not found at ${VOLK_INCLUDE_DIR}; check PROJECTV_ROOT")
endif()

# Vulkan headers + loader (system).
find_package(Vulkan REQUIRED)

add_executable(frame-flight-prototype
    main.cpp
    # VMA implementation lives in single header via VMA_IMPLEMENTATION macro (main.cpp)
    # volk implementation in volk.c (vendored).
    ${VOLK_INCLUDE_DIR}/volk.c
)

target_include_directories(frame-flight-prototype PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VMA_INCLUDE_DIR}
    ${VOLK_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(frame-flight-prototype PRIVATE Vulkan::Vulkan)

# Define VMA_USE_VOLK to route vk* calls through volk loader.
target_compile_definitions(frame-flight-prototype PRIVATE
    VMA_USE_VOLK=1
    VMA_VULKAN_VERSION=1004000  # Vulkan 1.4
)

# Tracy instrumentation is OUT-OF-SCOPE here (cross-cutting, see
# `tracy-gpu-vs-manual` backlog). Pure CPU-side timing via std::chrono.
