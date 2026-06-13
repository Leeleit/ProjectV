cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

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

# Vulkan Memory Allocator
add_subdirectory(external/VulkanMemoryAllocator)
# VMA is a header-only INTERFACE target. The upstream headers use
# raw `T*` pointers in hundreds of public API signatures and
# (correctly) omit Clang's `_Nonnull` / `_Nullable` annotations
# — they're not C-only code, and Apple-clang / upstream Clang's
# `-Wnullability-completeness` (which catches pointers crossing
# the annotated/non-annotated boundary in *Obj-C* method families)
# treats every such pointer as a diagnostic. VMA's contract
# (Vulkan physical-device memory, alloc callbacks, etc.) is
# fundamentally `void*`/nullable-by-vulkan-spec, so the warning
# is a third-party-API false positive. Scope the suppression to
# VMA's own INTERFACE so our own code in `src/` and `tests/` is
# NOT affected (per AGENTS.md §7.2.7 "не глушить в ProjectV таргете").
target_compile_options(VulkanMemoryAllocator INTERFACE
    -Wno-nullability-completeness
)

# fmt
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

# --- Asset / model import libs (ProjectV voxel renderer stays unchanged;
#     these are wired in for the new polygon-model pipeline, see
#     agent/active-sessions.md session 2026-06-11-asset-pipeline-m0-m5) ---

# glm (header-only math types, used by the new model pipeline and by
# fastgltf's glm element traits). The submodule is at `external/glm/`
# but is not yet wired in; we add it as a subdirectory and disable
# the optional static-library / test / install targets so it stays a
# pure include-path dependency.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Use GLM as header-only" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "Disable GLM tests" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "Disable GLM install" FORCE)
add_subdirectory(external/glm)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/glm")

# meshoptimizer (header-only API; we link the static lib to keep the include
# path attached through the INTERFACE property and to avoid pulling in
# the demo/gltfpack tools).
set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "Disable meshopt demo" FORCE)
set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "Disable meshopt gltfpack" FORCE)
set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "Build static meshopt" FORCE)
set(MESHOPT_INSTALL OFF CACHE BOOL "Disable meshopt install" FORCE)
set(MESHOPT_WERROR OFF CACHE BOOL "Disable meshopt -Werror" FORCE)
add_subdirectory(external/meshoptimizer)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/meshoptimizer")

# fastgltf (simdjson is bundled in external/fastgltf/deps/simdjson).
# We don't need the deprecated-extension opt-in, the test/example/docs
# targets, or the experimental C++20 module target for the renderer
# integration (M0 only needs a TU to compile and link).
set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "Disable fastgltf tests" FORCE)
set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "Disable fastgltf examples" FORCE)
set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "Disable fastgltf docs" FORCE)
set(FASTGLTF_ENABLE_DEPRECATED_EXT OFF CACHE BOOL "Disable fastgltf deprecated-ext" FORCE)
set(FASTGLTF_ENABLE_CPP_MODULES OFF CACHE BOOL "Disable fastgltf C++20 modules" FORCE)
add_subdirectory(external/fastgltf)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/fastgltf")

# draco (decode-only, no transcoder / CLI / Maya / Unity plugins).
# `DRACO_DEBUG_COMPILER_WARNINGS=OFF` keeps draco from re-enabling
# `-Werror`-grade warnings that would break ProjectV's `/WX` MSVC build.
# `DRACO_TRANSCODER_SUPPORTED=OFF` drops the glTF material/scene/texture
# objects we never decode; the KHR_draco_mesh_compression path still
# works because it lives in the always-built mesh-decoder object set.
set(DRACO_TRANSCODER_SUPPORTED OFF CACHE BOOL "Disable draco glTF transcoder" FORCE)
set(DRACO_DEBUG_COMPILER_WARNINGS OFF CACHE BOOL "Disable draco debug warnings" FORCE)
set(DRACO_BUILD_EXECUTABLES OFF CACHE BOOL "Disable draco CLI tools" FORCE)
set(DRACO_MAYA_PLUGIN OFF CACHE BOOL "Disable draco Maya plugin" FORCE)
set(DRACO_UNITY_PLUGIN OFF CACHE BOOL "Disable draco Unity plugin" FORCE)
# draco's CMakeLists.txt:40 calls `include(FindPythonInterp)`. That
# module is scheduled for removal under CMP0148 (CMake 4.0+). Without
# the policy being explicitly set, every configure re-emits a
# `(dev) Policy CMP0148 is not set` warning at the include() call.
# Set the policy as the default for every subdirectory's scope via
# `CMAKE_POLICY_DEFAULT_CMP<NNNN>`, so the setting follows
# `add_subdirectory(external/draco)` into draco's own scope (policies
# are stack-based and a subdir starts a fresh stack — only the
# CMAKE_POLICY_DEFAULT_CMP<NNNN> cache vars are carried across the
# boundary by default). We do NOT call `cmake_policy(SET CMP0148 OLD)`
# in this scope, because that triggers a separate deprecation warning
# ("OLD behavior will be removed in a future version of CMake") that's
# strictly worse than the dev warning we're trying to suppress.
set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
add_subdirectory(external/draco)
projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/draco")

# nlohmann/json (defense r0, 2026-06-13). Header-only JSON parser
# pulled in by `FetchContent` rather than vendored as a submodule, so
# the project does not need yet another pinned external directory for
# a single 1.1 MB header. Used by `src/voxel/SceneConfig.{hpp,cpp}`
# to load runtime scene presets from `runtime/scene.json` (per
# ТЗ 4.5.1 "Использование структурированных форматов для сохранения
# сцен").
include(FetchContent)
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(projectv_build_options INTERFACE)
if (MSVC)
    target_compile_options(projectv_build_options INTERFACE
            /W4
            /WX
            /permissive-
            /utf-8
    )
    target_compile_definitions(projectv_build_options INTERFACE
            NOMINMAX
    )
else ()
    # libstdc++ 16+ marks std::is_trivial deprecated; flecs 2.2.0 still uses it.
    # Suppress that one deprecation at the build-options layer so external
    # targets (flecs, ...) and our own code stay warning-clean until we either
    # move to libc++ or upgrade flecs.
    target_compile_options(projectv_build_options INTERFACE
            -Wno-deprecated-declarations
    )
    # Force-include <cstring> because legacy project code uses std::memcpy /
    # std::memset / std::strcmp without an explicit <cstring> include; on
    # MSVC these come in transitively via <cstdint>/<cstdlib>, but on
    # libstdc++ they do not, so the namespace-qualified calls break.
    #
    # **Tier 2.A — module support (`2026-06-13`).** Removed the
    # global `-include cstring` flag here. C++20 modules require
    # `export module X;` to be the first non-preprocessor token in
    # a `.ixx` TU; a force-include injects preprocessor output
    # *before* the file content, which would land in neither the
    # global module fragment nor the module purview. The
    # remaining caller that relied on the implicit include
    # (`src/render/vulkan/VulkanBootstrap.cpp`'s `std::strcmp`
    # per `agent/memory.md §6`) now has an explicit `<cstring>`
    # include added in the same commit.
endif ()
target_compile_definitions(projectv_build_options INTERFACE
        VK_NO_PROTOTYPES
)
# draco's CMakeLists exposes include paths only through the
# `INCLUDES` parameter of `draco_add_library`, which the helper
# forwards to `target_include_directories(... PRIVATE ...)` on the
# OBJECT-library leaves, not on the public umbrella `draco::draco`
# target. To use `#include <draco/compression/decode.h>` from
# ProjectV TUs we attach the source-tree include dir at the build
# options layer so it propagates through the same interface target
# as the rest of the project.
target_include_directories(projectv_build_options INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/external/draco"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/draco/src"
        "${CMAKE_BINARY_DIR}"
)

if (PROJECTV_ENABLE_VALIDATION)
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_VALIDATION=1)
else ()
    target_compile_definitions(projectv_build_options INTERFACE PROJECTV_ENABLE_VALIDATION=0)
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

# --- Vulkan Stack End ---


#------------------------Submodules-End----------------------

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()
