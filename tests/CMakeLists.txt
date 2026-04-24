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
