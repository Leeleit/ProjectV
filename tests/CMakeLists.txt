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

# Draco decoder test executable (M3). Like the asset-loader test,
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

# Frustum-cull test executable (M5). CPU-only: the helper under
# test (`IsAabbVisibleAgainstCameraFrustum`) is an inline camera-
# basis math function in `src/render/SceneResources.hpp` and does
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

# Box-UV fixture test executable (M6 prep). Validates the GLB
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

# Math types smoke test (Tier 0.A, 2026-06-13). Header-only
# `projectv::math` (`src/core/Math.hpp`) — Vec3/Vec4/Mat4 with
# 16-byte alignment for SIMD. The TU only includes Math.hpp
# and uses the types, so it needs no other library links.
# No Vulkan init, no Jolt, no asset pipeline.
add_executable(ProjectVMathTests
        MathTest.cpp
)

target_compile_definitions(ProjectVMathTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVMathTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVMathTests PRIVATE projectv_build_options)

add_test(NAME ProjectVMathTests COMMAND ProjectVMathTests)
set_tests_properties(ProjectVMathTests PROPERTIES LABELS "unit")



