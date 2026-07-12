cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

set(CMAKE_CXX_MODULE_STD
        OFF
        CACHE BOOL
        "Enable the CMake C++23+ standard-library module gate. Required for \`import std;\`. Currently OFF because the main ProjectV target uses libstdc++ (which does not yet ship a C++20-format std module); opt in per-target for libc++-only probe builds.")

project(ProjectV
        VERSION 0.0.1
        LANGUAGES C CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectVThirdParty)
include(CTest)

set(PROJECTV_DEFAULT_ENABLE_VALIDATION OFF)
set(PROJECTV_DEFAULT_ENABLE_RENDERDOC_MARKERS OFF)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PROJECTV_DEFAULT_ENABLE_VALIDATION ON)
    set(PROJECTV_DEFAULT_ENABLE_RENDERDOC_MARKERS ON)
endif ()

option(PROJECTV_ENABLE_VALIDATION "Enable Vulkan validation layers" ${PROJECTV_DEFAULT_ENABLE_VALIDATION})
option(PROJECTV_ENABLE_TRACY "Enable Tracy profiling instrumentation" ON)
option(PROJECTV_ENABLE_IMGUI "Build bundled Dear ImGui support library" OFF)
option(
        PROJECTV_ENABLE_RENDERDOC_MARKERS
        "Enable Vulkan debug names and markers for tools like RenderDoc"
        ${PROJECTV_DEFAULT_ENABLE_RENDERDOC_MARKERS}
)
option(PROJECTV_BUILD_TRACY_PROFILER "Build bundled Tracy profiler UI" OFF)
option(PROJECTV_ENABLE_BENCHMARKS "Build Google Benchmark perf-test harness" OFF)
option(PROJECTV_ENABLE_NATIVE_ARCH "Enable -march=native (non-portable, single-machine builds)" OFF)
option(PROJECTV_HW_RAY_TRACING "Enable hardware ray tracing probes (RTX path, feature-flagged)" OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT MSVC AND NOT WIN32)
    set(CMAKE_CXX_STDLIB libc++)
    add_compile_options(-stdlib=libc++)
    add_compile_options(-Wno-unused-command-line-argument)
    if (NOT DEFINED CMAKE_CXX_STDLIB_MODULES_JSON AND EXISTS "/usr/lib/libc++.modules.json")
        set(CMAKE_CXX_STDLIB_MODULES_JSON "/usr/lib/libc++.modules.json"
                CACHE FILEPATH
                "Path to libc++'s stdlib modules JSON")
    endif ()
    set(CMAKE_CXX_MODULE_STD ON CACHE BOOL
            "CMake 4.2+ C++23+ standard-library module gate (required for \`import std;\`)")
    mark_as_advanced(CMAKE_CXX_MODULE_STD)
endif ()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(
            -O3
            -flto=thin
            -DNDEBUG
            -ffunction-sections
            -fdata-sections
            -fno-finite-math-only
    )
    add_link_options(
            -flto=thin
            -Wl,--gc-sections
    )
    if (PROJECTV_ENABLE_NATIVE_ARCH)
        add_compile_options(-march=native)
    endif ()
endif ()

set(SDL_STATIC ON CACHE BOOL "Static linking for SDL" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)
add_subdirectory(external/SDL)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/SDL")

find_package(Vulkan REQUIRED)

if (WIN32)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
elseif (APPLE)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_MACOS_MVK)
elseif (ANDROID)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_ANDROID_KHR)
else ()
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XCB_KHR)
endif ()
add_subdirectory(external/volk)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/volk")

add_subdirectory(external/VulkanMemoryAllocator)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/VulkanMemoryAllocator")
set_target_properties(VulkanMemoryAllocator PROPERTIES SYSTEM TRUE)
target_compile_options(VulkanMemoryAllocator INTERFACE
    -Wno-nullability-completeness
)

set(FMT_DOC OFF CACHE BOOL "Disable fmt docs" FORCE)
set(FMT_TEST OFF CACHE BOOL "Disable fmt tests" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "Disable fmt install target" FORCE)
set(FMT_MODULE OFF CACHE BOOL "Disable fmt module target" FORCE)
add_subdirectory(external/fmt)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/fmt")

if (PROJECTV_ENABLE_IMGUI)
    add_library(projectv_imgui STATIC
            external/imgui/imgui.cpp
            external/imgui/imgui_draw.cpp
            external/imgui/imgui_tables.cpp
            external/imgui/imgui_widgets.cpp
    )
    target_include_directories(projectv_imgui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
endif ()

set(FLECS_STATIC ON CACHE BOOL "Build static flecs lib" FORCE)
set(FLECS_SHARED OFF CACHE BOOL "Disable shared flecs lib" FORCE)
set(FLECS_TESTS OFF CACHE BOOL "Disable flecs tests" FORCE)
add_subdirectory(external/flecs)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/flecs")

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

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use GLM as header-only" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "Disable GLM tests" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "Disable GLM install" FORCE)
add_subdirectory(external/glm)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/glm")

set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "Disable meshopt demo" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "Disable meshopt gltfpack" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static meshopt" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "Disable meshopt install" FORCE)
set(MESHOPT_WERROR OFF CACHE BOOL "Disable meshopt -Werror" FORCE)
add_subdirectory(external/meshoptimizer)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/meshoptimizer")

set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "Disable fastgltf tests" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "Disable fastgltf examples" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "Disable fastgltf docs" FORCE)
set(FASTGLTF_ENABLE_DEPRECATED_EXT OFF CACHE BOOL "Disable fastgltf deprecated-ext" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Disable fastgltf C++20 modules" FORCE)
add_subdirectory(external/fastgltf)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/fastgltf")

set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "Disable draco glTF transcoder" FORCE)
set(DRACO_DEBUG_COMPILER_WARNINGS OFF CACHE BOOL "Disable draco debug warnings" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "Disable draco CLI tools" FORCE)
set(DRACO_MAYA_PLUGIN OFF CACHE BOOL "Disable draco Maya plugin" FORCE)
set(DRACO_UNITY_PLUGIN OFF CACHE BOOL "Disable draco Unity plugin" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_subdirectory(external/draco)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/draco")

if (PROJECTV_ENABLE_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark's own unit tests" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest-backed benchmark tests" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "Disable gtest download" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "Disable benchmark install target" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "Disable -Werror for benchmark" FORCE)
    set(BENCHMARK_ENABLE_LTO OFF CACHE BOOL "Disable LTO for benchmark" FORCE)
    set(BENCHMARK_USE_BUNDLED_GTEST OFF CACHE BOOL "Disable bundled gtest in benchmark" FORCE)
    set(BENCHMARK_USE_LIBCXX ON CACHE BOOL "Use libc++ as benchmark's stdlib" FORCE)
    add_subdirectory(external/benchmark)
    if (TARGET benchmark)
        target_compile_options(benchmark PRIVATE -Wno-c2y-extensions)
    endif ()
    projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/benchmark")
endif ()

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
add_subdirectory(external/nlohmann_json EXCLUDE_FROM_ALL)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json")

add_library(projectv_build_options INTERFACE)
if (MSVC)
    target_compile_options(projectv_build_options INTERFACE
            /W4
            /WX
            /permissive-
            /utf-8
            /wd4996
    )
    target_compile_definitions(projectv_build_options INTERFACE
            NOMINMAX
    )
elseif (WIN32)
    target_compile_options(projectv_build_options INTERFACE
            /W4
            /WX
            /permissive-
            /utf-8
            /wd4996
            /clang:-Wno-deprecated-declarations
    )
    target_compile_definitions(projectv_build_options INTERFACE
            NOMINMAX
    )
else ()
    target_compile_options(projectv_build_options INTERFACE
            -stdlib=libc++
    )
    target_link_options(projectv_build_options INTERFACE
            -stdlib=libc++
    )
    target_link_libraries(projectv_build_options INTERFACE
            c++
            c++abi
            "-l:libstdc++.so.6"
    )
    target_compile_options(projectv_build_options INTERFACE
            -Wno-deprecated-declarations
    )
endif ()
target_compile_definitions(projectv_build_options INTERFACE
        VK_NO_PROTOTYPES
)
target_include_directories(projectv_build_options INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/draco"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/draco/src"
        "${CMAKE_BINARY_DIR}"
)
target_link_libraries(projectv_build_options INTERFACE SDL3::SDL3)

if (PROJECTV_ENABLE_VALIDATION)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_VALIDATION=1)
else ()
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_VALIDATION=0)
endif ()

if (PROJECTV_HW_RAY_TRACING)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_HW_RAY_TRACING=1)
else ()
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_HW_RAY_TRACING=0)
endif ()

if (PROJECTV_ENABLE_RENDERDOC_MARKERS)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_RENDERDOC_MARKERS=1)
else ()
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_RENDERDOC_MARKERS=0)
endif ()

if (PROJECTV_ENABLE_IMGUI)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_IMGUI=1)
    target_link_libraries(projectv_build_options INTERFACE projectv_imgui)
else ()
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_IMGUI=0)
endif ()

if (PROJECTV_ENABLE_TRACY)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_TRACY=1)
endif ()

message(STATUS "ProjectV options:")
message(STATUS "  validation           = ${PROJECTV_ENABLE_VALIDATION}")
message(STATUS "  tracy instrumentation = ${PROJECTV_ENABLE_TRACY}")
message(STATUS "  imgui support         = ${PROJECTV_ENABLE_IMGUI}")
message(STATUS "  renderdoc markers     = ${PROJECTV_ENABLE_RENDERDOC_MARKERS}")
message(STATUS "  tracy profiler ui     = ${PROJECTV_BUILD_TRACY_PROFILER}")


add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
