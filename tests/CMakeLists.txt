add_executable(ProjectVTests
        VoxelWorldTests.cpp
        ../src/app/AppUpdate.cpp
        ../src/app/Camera.cpp
        ../src/app/InputActions.cpp
        ../src/app/InputReplay.cpp
        ../src/app/LookDevCaptureAutomation.cpp
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



