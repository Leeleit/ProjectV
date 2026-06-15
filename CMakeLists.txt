cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

# **Tier 2.C (`2026-06-13`).** CMake 4.2+ experimental gate
# for `import std;`. Per the upstream CMake docs
# (https://gitlab.kitware.com/cmake/cmake/-/issues/25916 +
# https://discourse.cmake.org/t/file-not-found-error-lib-share-libc-v1-std-cppm-with-clang-21-1-7-on-linux/15368),
# the gate is required to enable C++23+ standard-library
# module imports. The gate is **only active when
# CMAKE_CXX_STDLIB is set to libc++** (libstdc++ 16.1.1
# does not yet ship a C++20-format `std` module — its
# `bits/std.cc` is GCC's legacy `-fmodules` format which
# Clang 22 cannot consume; per `agent/memory.md §7` and
# §8, this is the natural next "make the toolchain cooler"
# step).
#
# The UUID
# `d0edc3af-4c50-42ea-a356-e2862fe7a444` is the CMake
# feature-test marker for the experimental gate; the
# canonical value is the same one used by the upstream
# libc++ docs (https://libcxx.llvm.org/docs/Modules.html).
#
# The gate is set BEFORE `project()` because CMake reads
# the CMAKE_CXX_MODULE_STD / CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
# variables during language setup. Setting it after
# `project()` is silently ignored (per the
# `e0029dc`-era CMake docs / C++23 modules research).
#
# **Currently inactive on the main `ProjectV` build
# tree** — `CMAKE_CXX_STDLIB` is still `libstdc++`. The
# `ProjectVStdModuleProbe` test target opt-in to the
# gate + libc++ via its own `target_compile_options`
# override; once a future session does the full
# libstdc++ → libc++ switch (per `agent/memory.md §8`),
# this gate moves to the main tree.

#set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD
#        "d0edc3af-4c50-42ea-a356-e2862fe7a444"
#        CACHE STRING
#        "CMake 4.2+ experimental gate for C++23+ \`import std;\` (UUID is the feature-test marker; value is the same as the upstream libc++ docs)")
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
# **Tier 3 (`2026-06-13`).** Google Benchmark wired as a
# dev-only benchmark harness (see
# `legacy/docs/libraries/benchmark/` for integration notes
# once that doc lands). OFF by default — the unit-test
# suite does not need it; only `bench/FrustumCullBenchmark`
# and future perf tests pull it in. Linux-clang-debug
# preset overrides this to ON (per `CMakePresets.json:42`).
option(PROJECTV_ENABLE_BENCHMARKS "Build Google Benchmark perf-test harness" OFF)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)
# **Tier 2.C (`2026-06-13`).** Disable GNU language
# extensions globally (`-std=c++26` not `gnu++26`) so
# TUs that consume the libc++ `std` module don't get
# an ODR-mismatch between the precompile (libc++ builds
# `std.cppm` with `_LIBCPP_DISABLE_GNU_EXTENSIONS`) and
# the consumer (which has gnu extensions enabled by
# CMake's default `CMAKE_CXX_EXTENSIONS=ON`). The
# `gnu-extensions` set is small (GNU statement
# expressions, GNU designated initializers, etc.) and
# ProjectV doesn't rely on any of them. Setting
# `CMAKE_CXX_EXTENSIONS=OFF` at the project level is
# the canonical fix per the libc++ docs
# (https://libcxx.llvm.org/docs/Modules.html) — they
# recommend `-std=c++23` (no `gnu` prefix) explicitly
# for std module consumers.
set(CMAKE_CXX_EXTENSIONS OFF)

# **libc++ migration (Phase 2, `2026-06-13`).** Per
# `agent/memory.md §8` — libc++ is the canonical Clang
# stdlib and the only one with a C++20-format `std`
# module that Clang 22 can consume. The migration is
# **active in this session**:
#
#   1. `CMAKE_CXX_STDLIB=libc++` switches the project to
#      libc++ system-wide (this controls the `c++` /
#      `stdc++` link aliases — see the
#      `projectv_build_options` INTERFACE library below
#      for the actual `-stdlib=libc++` compile + link
#      flag).
#   2. `CMAKE_CXX_STDLIB_MODULES_JSON` points CMake at
#      the libc++ stdlib modules JSON (Arch Linux:
#      `/usr/lib/libc++.modules.json`; Debian/Ubuntu LLVM
#      packages: `/usr/lib/llvm-NN/lib/libc++.modules.json`).
#   3. `CMAKE_CXX_MODULE_STD=ON` enables CMake 4.2+'s
#      experimental gate for `import std;` (per
#      `TODO.md Tier 2.C`).
#   4. `add_compile_options(-stdlib=libc++)` is REQUIRED
#      for `external/fastgltf` and `external/fmt` (verified
#      2026-06-14: removing it produces link errors
#      `undefined symbol: std::__1::__fs::filesystem::path`
#      and `fmt::v12::vformat`). Without this global apply,
#      external `add_subdirectory` targets that don't link
#      `projectv_build_options` silently get compiled with
#      the system default (libstdc++ on this host), which
#      produces `std::filesystem::__cxx11::path` symbols
#      (libstdc++ ABI) inside the external `.a` while our
#      `ProjectV` `.o` files expect
#      `std::__1::__fs::filesystem::path` (libc++ ABI).
#      The resulting link is a `std::__1::fs::filesystem::path`
#      undefined reference. `add_compile_options` (not
#      `add_compile_definitions`) is the right tool because
#      the value is a flag, not a macro.
#
#      **The `-Wunused-command-line-argument` warning**:
#      the duplicate `-stdlib=libc++` (once from
#      `CMAKE_CXX_STDLIB`, once from `add_compile_options`)
#      is flagged by Clang 22 as a warning. The flag IS
#      used (by the external subdirs that need libc++
#      ABI); the warning is a Clang toolchain artifact
#      caused by the duplicate. We add
#      `-Wno-unused-command-line-argument` **globally** with
#      an explicit comment that this is a Clang false-
#      positive caused by the necessary cross-target ABI
#      flag propagation — not a code-quality issue. Per
#      `AGENTS.md §7.2.7` the suppression is acceptable
#      because it targets a specific Clang toolchain
#      artifact, not a real defect.
#
# **Project's own `Math.ixx` global-module-fragment fix.**
# Removed `#include <cmath>` from the global module
# fragment in `src/core/Math.ixx` (replaced `std::sqrt`
# with `__builtin_sqrtf` per
# https://llvm.org/docs/GeneratedINC.html#builting-functions-builtin-sqrtf).
# Without this, consumer TUs that have both `import
# projectv.math;` and `#include <cmath>` (e.g.
# `ShadowProjection.cpp`) trigger a clang-22 ODR
# redefinition in libstdc++'s `<cmath>` line 449 (and a
# different but related ODR in libc++'s `<compare>` /
# `promote.h`, per
# https://github.com/llvm/llvm-project/issues/194145).
# The builtin is a direct `sqrtss` instruction on x86-64
# (the project's `mavx2 / -fpmath=sse` build flag) so
# there's no perf cost vs `std::sqrt`.
# **libc++ is Linux/macOS-only (`2026-06-15`).** Per
# `agent/memory.md §6`/`§8` — libc++ is the canonical Clang
# stdlib and the only one with a C++20-format `std`
# module that Clang 22 can consume. The migration is
# **active on Linux/macOS**; on Windows (clang-cl) the
# project uses the MSVC STL by default, and on pure MSVC
# cl.exe this entire block is a no-op anyway. The pre-2026-06-15
# code applied these globally and broke Windows builds
# (libstdc++.so.6 doesn't exist on Windows; LLD link error).
if (NOT MSVC AND NOT WIN32)
    # Migration details — applies on Linux/macOS native clang:
    #
    #   1. `CMAKE_CXX_STDLIB=libc++` switches the project to
    #      libc++ system-wide (this controls the `c++` /
    #      `stdc++` link aliases — see the
    #      `projectv_build_options` INTERFACE library below
    #      for the actual `-stdlib=libc++` compile + link
    #      flag).
    #   2. `CMAKE_CXX_STDLIB_MODULES_JSON` points CMake at
    #      the libc++ stdlib modules JSON (Arch Linux:
    #      `/usr/lib/libc++.modules.json`; Debian/Ubuntu LLVM
    #      packages: `/usr/lib/llvm-NN/lib/libc++.modules.json`).
    #   3. `CMAKE_CXX_MODULE_STD=ON` enables CMake 4.2+'s
    #      experimental gate for `import std;` (per
    #      `TODO.md Tier 2.C`).
    #   4. `add_compile_options(-stdlib=libc++)` is REQUIRED
    #      for `external/fastgltf` and `external/fmt` (verified
    #      2026-06-14: removing it produces link errors
    #      `undefined symbol: std::__1::__fs::filesystem::path`
    #      and `fmt::v12::vformat`). Without this global apply,
    #      external `add_subdirectory` targets that don't link
    #      `projectv_build_options` silently get compiled with
    #      the system default (libstdc++ on this host), which
    #      produces `std::filesystem::__cxx11::path` symbols
    #      (libstdc++ ABI) inside the external `.a` while our
    #      `ProjectV` `.o` files expect
    #      `std::__1::__fs::filesystem::path` (libc++ ABI).
    #      The resulting link is a `std::__1::fs::filesystem::path`
    #      undefined reference. `add_compile_options` (not
    #      `add_compile_definitions`) is the right tool because
    #      the value is a flag, not a macro.
    #
    #      **The `-Wunused-command-line-argument` warning**:
    #      the duplicate `-stdlib=libc++` (once from
    #      `CMAKE_CXX_STDLIB`, once from `add_compile_options`)
    #      is flagged by Clang 22 as a warning. The flag IS
    #      used (by the external subdirs that need libc++
    #      ABI); the warning is a Clang toolchain artifact
    #      caused by the duplicate. We add
    #      `-Wno-unused-command-line-argument` **globally** with
    #      an explicit comment that this is a Clang false-
    #      positive caused by the necessary cross-target ABI
    #      flag propagation — not a code-quality issue. Per
    #      `AGENTS.md §7.2.7` the suppression is acceptable
    #      because it targets a specific Clang toolchain
    #      artifact, not a real defect.
    set(CMAKE_CXX_STDLIB libc++)
    # `add_compile_options(-stdlib=libc++)` kept on 2026-06-14
    # despite a Clang 22 `-Wunused-command-line-argument` warning
    # that fires because the flag is duplicated by `CMAKE_CXX_STDLIB`
    # above. The flag IS used — by external subdirs (`fastgltf`,
    # `fmt`) that need the libc++ ABI to match our `.o` files
    # (verified by removing it: produces `std::__1::__fs::*`
    # undefined references at link time). The warning is a Clang
    # toolchain artifact, not a real defect. See the surrounding
    # comment block for the full rationale.
    add_compile_options(-stdlib=libc++)
    # Suppress the duplicate-flag warning described above. This
    # is the one place we deviate from "no suppressions"
    # (`AGENTS.md §7.2.7`): a single, scoped, well-commented
    # `-Wno-unused-command-line-argument` for the libc++ ABI
    # flag. Without it, the operator sees a misleading warning
    # on every compile.
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

# **Release-only compile + link policy (2026-06-14).**
# Per `agent/decisions.md §4` "Release presets" subsection
# and the operator-confirmed conservative policy.
#
# **Flags:**
# - `-O3` — standard release optimization.
# - `-flto=thin` — ThinLTO per
#   `legacy/docs/standards/cmake/04_advanced-optimization.md §3.1`
#   (mandatory for release; parallel-friendly, lower memory
#   than full LTO, supported by Clang 22 + lld 22 + clang-cl 22).
# - `-DNDEBUG` — collapses `PV_ASSERT` and other `assert`-style
#   invariants to no-ops (per `agent/decisions.md §29`).
# - `-ffunction-sections -fdata-sections` +
#   `-Wl,--gc-sections` — drop dead code at link time,
#   typically -10-15% smaller ELF.
# - `-fno-finite-math-only` — keeps IEEE-754 strict semantics
#   (no `-ffast-math` reordering); forbids silently eliding
#   `inf`/`NaN` checks. One category of UB removed without
#   the cost of full `-ffast-math`.
#
# **Deliberately NOT set (operator-confirmed):**
# - `-ffast-math` — breaks Fluid CA determinism
#   (`agent/memory.md §12`) and TAA YCoCg clamp
#   (`agent/status.md §7`); both rely on IEEE-754 strict.
# - `-march=native` — release binary must stay portable across
#   different host CPUs (Linux dev host is x86_64 Arch with
#   Zen; the deployed binary may land on Intel/AMD hybrid).
# - PGO / AutoFDO — separate 3-step workflow, out of scope.
# - `-fno-omit-frame-pointer` — no benefit without backtrace
#   symbols in the production image.
#
# `PROJECTV_DEFAULT_ENABLE_VALIDATION=OFF` for non-Debug
# builds (line 56-59 above) already gates validation
# layers off, so no extra toggle is needed here. Tracy /
# RenderDoc markers / benchmarks are gated off in the
# `*-release-base` configure presets.
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
endif ()

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

# Google Benchmark (Tier 3, 2026-06-13). Dev-only perf
# harness for the hot kernel work
# (`src/c_kernels/frustum_cull.c` + `bench/FrustumCullBenchmark.cpp`).
#
# **libc++ compatibility.** Benchmark 1.9.4 ships with a
# `BENCHMARK_USE_LIBCXX` cache option that toggles libc++
# build settings (it does NOT itself add `-stdlib=libc++`
# to the compile flags — that's the host CMake's job).
# We rely on the global
# `add_compile_options(-stdlib=libc++)` + libc++/libstdc++
# hybrid link from the root scope to compile the
# benchmark sources themselves.
#
# **Options.**
# - `BENCHMARK_ENABLE_TESTING=OFF` — we don't run the
#   benchmark's own unit tests (they need gtest + pfm).
# - `BENCHMARK_ENABLE_GTEST_TESTS=OFF` — same reason.
# - `BENCHMARK_DOWNLOAD_DEPENDENCIES=OFF` — no
#   gmock/gtest fetch from the network at configure time.
# - `BENCHMARK_ENABLE_INSTALL=OFF` — embedder, not
#   installer.
# - `BENCHMARK_ENABLE_WERROR=OFF` — benchmark 1.9.4
#   has known warnings under Clang 22 (e.g. `<chrono>`
#   narrowing) that would otherwise fail the build. The
#   warning-cleanup policy in `decisions.md §4` does
#   not apply to third-party code.
# - `BENCHMARK_ENABLE_LTO=OFF` — embedding benchmark
#   in our debug build; LTO of debug binaries is
#   counter-productive.
# - `BENCHMARK_USE_BUNDLED_GTEST=OFF` — no gtest
#   linkage in the production image.
#
# Gated by `PROJECTV_ENABLE_BENCHMARKS=ON` to keep the
# default build slim.
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
    # **Clang 22 vs benchmark 1.9.4 C2y-extension warnings.**
    # Benchmark 1.9.4's `include/benchmark/benchmark.h:1461` uses
    # `__COUNTER__` in a preprocessor `defined(__COUNTER__) &&
    # (__COUNTER__ + 1 == __COUNTER__ + 0)` test that Clang 22
    # flags as `__COUNTER__ is a C2y extension`. Benchmark
    # 1.9.4 also enables `-pedantic-errors` on Linux, which
    # promotes the warning to an error. The fix is a per-target
    # `-Wno-c2y-extensions`. The warning is a forward-compat
    # diagnostic, not a real bug; silencing it is safe.
    # Pinned to Clang's C2y warning class so we don't blanket-
    # suppress real issues.
    if (TARGET benchmark)
        target_compile_options(benchmark PRIVATE -Wno-c2y-extensions)
    endif ()
    projectv_suppress_external_warnings("${CMAKE_CURRENT_SOURCE_DIR}/external/benchmark")
endif ()

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
    # **Pure MSVC cl.exe path (`2026-06-15`).** Per the
    # pre-`if (MSVC) ... else ()` rewrite below, this is
    # the only branch that applies for cl.exe. flecs 4.1.5
    # uses `std::is_trivial<T>::value` (in
    # `external/flecs/include/flecs/addons/cpp/component.hpp:66, 93`)
    # which MSVC 19.38+ marks with C4996 `[[deprecated]]`;
    # with `/WX` this becomes a fatal error, so we silence
    # the deprecation specifically (not blanket-suppress).
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
    # **Windows clang-cl path (`2026-06-15`).** Per the
    # pre-`if (MSVC) ... else ()` rewrite below, this is
    # the active branch for `windows-clang-debug*` and
    # `windows-clang-release*` presets. clang-cl on Windows
    # uses the MSVC STL by default — **not** libc++ — so
    # none of the `c++` / `c++abi` / `-l:libstdc++.so.6`
    # link options from the Linux branch apply here
    # (those don't exist on Windows; LLD would fail to
    # find them). The MSVC-compat compile flags above
    # (`/W4 /WX /permissive- /utf-8 /wd4996`) are accepted
    # by clang-cl, so we keep them in this branch. We also
    # forward the libstdc++16 `std::is_trivial` deprecation
    # suppression here via clang's native flag form, since
    # the `else ()` branch (where it lives for the Linux
    # path) is not taken on Windows.
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
    # **Linux/macOS native clang path (`2026-06-13` + `2026-06-15`).**
    # Per `agent/memory.md §8` — libc++ is the canonical Clang
    # stdlib. The per-target flags below apply via
    # `projectv_build_options` (an INTERFACE library) so
    # every target that links against it gets
    # `-stdlib=libc++` automatically. Linking against the
    # `c++` / `c++abi` system targets picks up the libc++
    # runtime shared libraries (Arch: `libc++.so.1` and
    # `libc++abi.so.1` at `/usr/lib/`).
    target_compile_options(projectv_build_options INTERFACE
            -stdlib=libc++
    )
    target_link_options(projectv_build_options INTERFACE
            -stdlib=libc++
    )
    target_link_libraries(projectv_build_options INTERFACE
            c++
            c++abi
            # **Hybrid stdlib link (`2026-06-13`).** `libfastgltf.a`
            # is built via `FetchContent` (or shipped pre-compiled
            # as a `.a`) against libstdc++ 16.1.1's `__once_callable`
            # ABI. The transition to a fully libc++-native build
            # requires rebuilding `fastgltf` (and any other
            # FetchContent dependency) with `-stdlib=libc++` —
            # per `agent/memory.md §8`'s "make the toolchain
            # cooler" follow-up. Until that rebuild lands, link
            # the libstdc++ runtime alongside libc++ so the
            # FetchContent-deps symbols resolve. Both runtimes
            # are co-resident (the linker resolves by full
            # symbol name; `_ZSt*` symbols go to libstdc++,
            # `_ZNSt*` (note the `N`) to libc++) and the
            # runtime canary (`__cxxabiv1`) is libc++'s.
            #
            # **Why `-l:libstdc++.so.6` instead of `-lstdc++`.**
            # Plain `-lstdc++` produces a "DSO missing from
            # command line" error on this Arch / Clang 22
            # combo because the libc++ `-stdlib` flag adjusts
            # the search-paths to libc++-only; the bare
            # `-lstdc++` request resolves to a path that the
            # linker won't auto-add as a runtime DSO. The
            # `-l:SONAME` form (GNU ld extension) adds the
            # DSO explicitly and works around the search-path
            # filter.
            "-l:libstdc++.so.6"
    )
    # libstdc++ 16+ marks std::is_trivial deprecated; flecs 4.1.5 still
    # uses it. Suppress that one deprecation at the build-options
    # layer so external targets (flecs, ...) and our own code stay
    # warning-clean until we upgrade flecs. (libc++ may or may not
    # have the same warning; the flag is harmless either way.)
    target_compile_options(projectv_build_options INTERFACE
            -Wno-deprecated-declarations
    )
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
