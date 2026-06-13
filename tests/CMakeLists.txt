# **libc++ migration (Phase 2, `2026-06-13`).** Helper
# macro that adds the project's `.ixx` modules to a test
# target's `FILE_SET CXX_MODULES` with the project's
# `src/` as `BASE_DIRS`. Every test that includes
# `core/Types.hpp` (transitively) needs the modules
# available so the `import projectv.math;` /
# `import projectv.string_id;` shims in `core/Types.hpp`
# and `core/Math.hpp` / `core/StringId.hpp` resolve at
# compile time. Without the FILE_SET, the `import`
# statement in the shim fails with "module not found".
macro(projectv_test_add_modules TARGET)
    target_sources(${TARGET} PRIVATE
        FILE_SET CXX_MODULES FILES
        ${CMAKE_SOURCE_DIR}/src/core/Math.ixx
        ${CMAKE_SOURCE_DIR}/src/core/StringId.ixx
        BASE_DIRS
        ${CMAKE_SOURCE_DIR}/src
    )
endmacro()

add_executable(ProjectVTests
        VoxelWorldTests.cpp
        ../src/app/AppUpdate.cpp
        ../src/app/Camera.cpp
        ../src/app/InputActions.cpp
        ../src/app/InputReplay.cpp
        ../src/app/LookDevCaptureAutomation.cpp
        ../src/audio/AudioEngine.cpp
        ../src/ecs/EcsWorld.cpp
        ../src/physics/PhysicsWorld.cpp
        ../src/debug/DebugHud.cpp
        ../src/debug/DebugOverlays.cpp
        ../src/platform/PlatformEvents.cpp
        ../src/core/RuntimeDiagnostics.cpp
        ../src/core/RuntimeProbe.cpp
        ../src/render/ScreenshotCapture.cpp
        ../src/render/ShadowProjection.cpp
        ../src/render/vulkan/VulkanResult.cpp
        ../src/voxel/VoxelInteraction.cpp
        ../src/voxel/VoxelMaterials.cpp
        ../src/voxel/VoxelRaycast.cpp
        ../src/voxel/VoxelWorld.cpp
)

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" PROJECTV_TESTS_SOURCE_DIR_CMAKE)
target_compile_definitions(ProjectVTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVTests PRIVATE projectv_build_options)

target_link_libraries(ProjectVTests PRIVATE
        volk
        fmt::fmt
        flecs::flecs_static
        Jolt::Jolt
        SDL3::SDL3
        GPUOpen::VulkanMemoryAllocator
        # Pulled in transitively by `core/Types.hpp` after the asset
        # pipeline landed `MeshGpuResources.hpp`; the `glm` target's
        # INTERFACE include directory (`external/glm/`) has to be on
        # the test TU's include path or the build dies on the very
        # first TU that opens `AssetLoader.hpp`.
        glm
        # Audio engine (2026-06-12). `miniaudio` is needed
        # because `AppUpdate.cpp` now `#include`s
        # `audio/AudioEngine.hpp`, which includes the
        # library header. Linking the library is the
        # cheapest way to inherit `external/miniaudio/`
        # on the test TU's include path. The audio
        # engine itself is not used in any test (the
        # test path passes `nullptr` for the engine),
        # but the include is needed to compile
        # `AppUpdate.cpp`.
        miniaudio
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVTests PRIVATE Tracy::TracyClient)
endif ()

# **Tier 2 + Phase 2 libc++ migration (`2026-06-13`).**
# `ProjectVTests` includes `core/Types.hpp` transitively
# (via `VoxelWorld.cpp`, `Camera.cpp`, `PhysicsWorld.cpp`,
# `DebugHud.cpp`, `DebugOverlays.cpp`, `ScreenshotCapture.cpp`),
# so the `projectv.math` and `projectv.string_id` module
# `import` statements at the top of `core/Types.hpp`
# need the modules in this target's `FILE_SET CXX_MODULES`
# to resolve. Without this call, the build fails with
# "fatal error: module 'projectv.math' not found" on every
# TU that includes `core/Types.hpp`. **The same
# regression appeared in the Tier 1.B phase 2 commit
# (`92c4380`); the per-test `projectv_test_add_modules()`
# call was added to the new M-pipeline tests but never to
# the legacy `ProjectVTests` target.** Wiring it up here
# is the build-system side of the same fix the operator
# applied by hand to `src/core/Types.hpp` to migrate to
# libc++.
projectv_test_add_modules(ProjectVTests)

add_test(NAME ProjectVTests COMMAND ProjectVTests)
set_tests_properties(ProjectVTests PROPERTIES LABELS "unit")

# Asset-pipeline test executable (M1). Separate from `ProjectVTests`
# so its `main()` does not collide with the existing harness, and so
# it can link `fastgltf` and `glm` without dragging the full
# voxel-test link line in. The test fixture `tests/fixtures/box.glb`
# is referenced through `PROJECTV_TESTS_SOURCE_DIR`.
add_executable(ProjectVAssetTests
        AssetLoaderTests.cpp
        ../src/asset/AssetLoader.cpp
        ../src/asset/AssetManifest.cpp
        ../src/asset/AssetRegistry.cpp
        ../src/asset/DracoMeshDecoder.cpp
)

target_compile_definitions(ProjectVAssetTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVAssetTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVAssetTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVAssetTests PRIVATE
        fastgltf::fastgltf
        draco::draco
        glm
)

add_test(NAME ProjectVAssetTests COMMAND ProjectVAssetTests)
set_tests_properties(ProjectVAssetTests PROPERTIES LABELS "unit")

# AssetTests pulls in `core/Types.hpp` (transitively via
# `asset/AssetLoader.cpp` → `asset/MeshGpuResources.hpp`),
# so it needs the projectv.math and projectv.string_id
# modules in its FILE_SET so the shim's `import`s resolve.
projectv_test_add_modules(ProjectVAssetTests)

# Mesh-baker test executable (M2). CPU-only, no Vulkan init needed
# because `MeshBaker` is the unit; GPU upload (`MeshGpuResources`)
# is exercised at runtime in M4 when the model is actually drawn.
add_executable(ProjectVMeshBakerTests
        MeshBakerTests.cpp
        ../src/asset/AssetLoader.cpp
        ../src/asset/DracoMeshDecoder.cpp
        ../src/asset/MeshBaker.cpp
)

target_compile_definitions(ProjectVMeshBakerTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVMeshBakerTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVMeshBakerTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVMeshBakerTests PRIVATE
        fastgltf::fastgltf
        draco::draco
        meshoptimizer
        glm
)

add_test(NAME ProjectVMeshBakerTests COMMAND ProjectVMeshBakerTests)
set_tests_properties(ProjectVMeshBakerTests PROPERTIES LABELS "unit")

# MeshBakerTests pulls in `core/Types.hpp` (transitively
# via `asset/MeshBaker.cpp` → `core/Types.hpp` for the
# `MeshGpuResources` field), so it needs the modules.
projectv_test_add_modules(ProjectVMeshBakerTests)

# MeshBakerTests pulls in `core/Types.hpp` (transitively via
# `asset/MeshBaker.cpp` → `core/Types.hpp` for the
# `MeshGpuResources` field), so it needs the modules.

# it's a separate executable to keep its own `main()` and its
# own draco / fastgltf link line. The draco-compressed fixture
# `tests/fixtures/box_draco.glb` is the smallest self-contained
# draco file we have; the non-draco Box.glb is also loaded
# through the same code path to verify the M3 change did not
# regress M1.
add_executable(ProjectVDracoTests
        DracoDecoderTests.cpp
        ../src/asset/AssetLoader.cpp
        ../src/asset/DracoMeshDecoder.cpp
        ../src/asset/MeshBaker.cpp
)

target_compile_definitions(ProjectVDracoTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVDracoTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVDracoTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVDracoTests PRIVATE
        fastgltf::fastgltf
        draco::draco
        meshoptimizer
        glm
)

add_test(NAME ProjectVDracoTests COMMAND ProjectVDracoTests)
set_tests_properties(ProjectVDracoTests PROPERTIES LABELS "unit")

# DracoTests pulls in `core/Types.hpp` (transitively via
# `asset/DracoMeshDecoder.cpp` → `asset/MeshGpuResources.hpp`),
# so it needs the modules.
projectv_test_add_modules(ProjectVDracoTests)

# DracoTests pulls in `core/Types.hpp` (transitively via
# `asset/DracoMeshDecoder.cpp` → `asset/MeshGpuResources.hpp`),
# so it needs the modules.
# Frustum-cull test executable (M5). CPU-only: the helper under
# test (`IsAabbVisibleAgainstCameraFrustum`) is an inline camera-
# not touch Vulkan. The executable links `projectv_build_options`
# for warning flags but does not need any of the asset pipeline
# libraries — keeping it lean keeps the test runtime well under
# 100 ms so the per-test budget in
# `decisions.md §4` stays comfortable.
add_executable(ProjectVFrustumCullingTests
        FrustumCullingTests.cpp
)

target_compile_definitions(ProjectVFrustumCullingTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVFrustumCullingTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVFrustumCullingTests PRIVATE projectv_build_options)
# `core/Types.hpp` (transitively included by
# `render/SceneResources.hpp`) needs `volk.h` on the include path;
# linking `volk` is the cheapest way to inherit that since the
# library exports `external/volk/` as a PUBLIC include directory.
# `glm` is pulled in the same way by the asset-pipeline headers
# (`AssetLoader.hpp` → `MeshBaker.hpp`); its INTERFACE include
# path (`external/glm/`) is also needed. `vk_mem_alloc.h` is
# pulled in transitively by `MeshGpuResources.hpp` →
# `core/Types.hpp` and is exported by the VMA target.
target_link_libraries(ProjectVFrustumCullingTests PRIVATE volk glm GPUOpen::VulkanMemoryAllocator)

add_test(NAME ProjectVFrustumCullingTests COMMAND ProjectVFrustumCullingTests)
set_tests_properties(ProjectVFrustumCullingTests PROPERTIES LABELS "unit")

# FrustumCullingTests is a header-only test (no
# `core/Types.hpp` include), but the helper is cheap
# and keeps the macro use uniform across tests.
projectv_test_add_modules(ProjectVFrustumCullingTests)

# FrustumCullingTests is a header-only test (no
# `core/Types.hpp` include), but the helper is cheap
# and keeps the macro use uniform across tests.
# header byte order (`magic, version, length`) by reading the
# fixture back from disk. The original M6 generator wrote the
# header in the wrong order and fastgltf reported
# `Error::UnsupportedVersion`, which is a misleading symptom for
# a header-byte-order bug. This test catches the regression at
# the byte level so the next person doesn't have to debug a
# fastgltf error message that points to the wrong place.
add_executable(ProjectVBoxUvFixtureTests
        BoxUvFixtureTests.cpp
)

target_compile_definitions(ProjectVBoxUvFixtureTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVBoxUvFixtureTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVBoxUvFixtureTests PRIVATE projectv_build_options)

add_test(NAME ProjectVBoxUvFixtureTests COMMAND ProjectVBoxUvFixtureTests)
set_tests_properties(ProjectVBoxUvFixtureTests PROPERTIES LABELS "unit")

# BoxUvFixtureTests is a header-only test (no
# `core/Types.hpp` include), but the helper is cheap
# and keeps the macro use uniform across tests.
projectv_test_add_modules(ProjectVBoxUvFixtureTests)

# No Vulkan init, no Jolt, no asset pipeline.
add_executable(ProjectVMathTests
        MathTest.cpp
)

# MathTest needs the modules so the `core/Math.hpp` shim
# can `import projectv.math;` (and the `string_id` import
# via `core/Types.hpp` if any test-only header pulls it in).
projectv_test_add_modules(ProjectVMathTests)

target_compile_definitions(ProjectVMathTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVMathTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVMathTests PRIVATE projectv_build_options)

add_test(NAME ProjectVMathTests COMMAND ProjectVMathTests)
set_tests_properties(ProjectVMathTests PROPERTIES LABELS "unit")

# StringID smoke test (Tier 1.D, 2026-06-13). Header-only
# `projectv::core::StringID` (`src/core/StringId.hpp`) — 16 B
# FNV-1a hash + length tuple with constexpr literal ctor.
# Same target shape as `ProjectVMathTests`: pure C++ standard
# library, no third-party deps, no Vulkan, no asset pipeline.
add_executable(ProjectVStringIdTests
        StringIdTest.cpp
)

target_sources(ProjectVStringIdTests PRIVATE
        FILE_SET CXX_MODULES FILES
        ${CMAKE_SOURCE_DIR}/src/core/StringId.ixx
        BASE_DIRS
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(ProjectVStringIdTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVStringIdTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVStringIdTests PRIVATE projectv_build_options)

add_test(NAME ProjectVStringIdTests COMMAND ProjectVStringIdTests)
set_tests_properties(ProjectVStringIdTests PROPERTIES LABELS "unit")

# StringIdTests is a header-only test (no `core/Types.hpp`
# include), but the shim's `import projectv.string_id;`
# statement needs the module in scope. The macro is the
# uniform way to wire that.
projectv_test_add_modules(ProjectVStringIdTests)

# C++20 modules smoke test (Tier 2.A, 2026-06-13). Tiny
# executable that `import projectv.probe;` and prints the
# value. Validates the CMake `FILE_SET CXX_MODULES` +
# `CMAKE_CXX_SCAN_FOR_MODULES=ON` pipeline end-to-end
# before we commit to converting the real `Math.ixx`.
#
# The probe module is duplicated into `tests/` because
# CMake's `FILE_SET CXX_MODULES` base directory is the
# target's source dir, and the test target's source dir
# is `tests/`. A future refactor (Tier 2.D) can move
# shared modules into a common location and use
# `BASE_DIRS` on the FILE_SET to widen the scope.
add_executable(ProjectVModuleSmoke
        ModuleSmokeTest.cpp
)

target_sources(ProjectVModuleSmoke PRIVATE
        FILE_SET CXX_MODULES FILES
        ${CMAKE_SOURCE_DIR}/src/core/Probe.ixx
        BASE_DIRS
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(ProjectVModuleSmoke PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVModuleSmoke PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVModuleSmoke PRIVATE projectv_build_options)

add_test(NAME ProjectVModuleSmoke COMMAND ProjectVModuleSmoke)
set_tests_properties(ProjectVModuleSmoke PROPERTIES LABELS "unit")

# C++23+ `import std;` probe (Tier 2.C, 2026-06-13).
# Per the libc++ docs (https://libcxx.llvm.org/docs/Modules.html)
# and the research in commit `e0029dc`-era, **`import std;` is
# ONLY available with libc++**, not with libstdc++ (libstdc++
# 16.1.1 ships `bits/std.cc` in GCC's legacy `-fmodules` format
# which Clang 22 cannot consume).
#
# **Phase 2 (`2026-06-13`).** The main `ProjectV` build
# has now switched to libc++ (via the root
# `CMakeLists.txt`'s `CMAKE_CXX_STDLIB=libc++` + the
# `projectv_build_options` INTERFACE library). This
# probe inherits libc++ from there; the per-target
# `-stdlib=libc++` override that was here in Phase 1
# is gone.
#
# The probe precompiles `std.pcm` from
# `/usr/share/libc++/v1/std.cppm` as a CMake custom
# command (output goes into the binary dir), then links
# the consumer TU with `-fmodule-file=std=<...>`. If
# the precompile or the consumer link fails, the failure
# message names the exact missing piece (the libc++
# `.cppm`, the `libc++.modules.json`, etc.).
add_executable(ProjectVStdModuleProbe
        StdModuleProbe.cpp
)

target_link_directories(ProjectVStdModuleProbe PRIVATE
        /usr/lib
)
target_link_options(ProjectVStdModuleProbe PRIVATE
        -stdlib=libc++
)
target_link_libraries(ProjectVStdModuleProbe PRIVATE
        c++
        c++abi
        "-l:libstdc++.so.6"
)

# **Precompile `std.pcm` as a custom target.** Output goes
# into the build's binary dir so the consumer TU can
# `-fmodule-file=std=...` find it. The precompile command
# is the canonical libc++ + Clang invocation from
# https://0xstubs.org/using-the-c23-std-module-with-clang-18/
# — same flags, just modernised for `-std=c++26`.
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/std.pcm
        COMMAND /usr/sbin/clang++
                -std=c++26
                -stdlib=libc++
                -Wno-reserved-identifier
                -Wno-reserved-module-identifier
                --precompile
                -o ${CMAKE_CURRENT_BINARY_DIR}/std.pcm
                /usr/share/libc++/v1/std.cppm
        DEPENDS /usr/share/libc++/v1/std.cppm
        COMMENT "Precompiling libc++ std.pcm"
        VERBATIM
)
add_custom_target(std_pcm_build DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/std.pcm)
add_dependencies(ProjectVStdModuleProbe std_pcm_build)

target_compile_options(ProjectVStdModuleProbe PRIVATE
        -fmodule-file=std=${CMAKE_CURRENT_BINARY_DIR}/std.pcm
)
target_sources(ProjectVStdModuleProbe PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/std.pcm
)

# Re-enable the test. The probe takes ~30s on a cold
# cache because of the precompile step; on warm sccache
# it's <1s.
add_test(NAME ProjectVStdModuleProbe COMMAND ProjectVStdModuleProbe)
set_tests_properties(ProjectVStdModuleProbe PROPERTIES LABELS "unit")



