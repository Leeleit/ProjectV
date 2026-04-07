add_executable(ProjectVTests
        VoxelWorldTests.cpp
        ../src/app/AppUpdate.cpp
        ../src/app/Camera.cpp
        ../src/app/InputActions.cpp
        ../src/ecs/EcsWorld.cpp
        ../src/debug/DebugHud.cpp
        ../src/platform/PlatformEvents.cpp
        ../src/core/RuntimeDiagnostics.cpp
        ../src/core/RuntimeProbe.cpp
        ../src/render/vulkan/VulkanResult.cpp
        ../src/voxel/VoxelInteraction.cpp
        ../src/voxel/VoxelMaterials.cpp
        ../src/voxel/VoxelRaycast.cpp
        ../src/voxel/VoxelWorld.cpp
)

target_include_directories(ProjectVTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(ProjectVTests PRIVATE
        NOMINMAX
        VK_NO_PROTOTYPES
)

target_compile_options(ProjectVTests PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
)

target_link_libraries(ProjectVTests PRIVATE
        volk
        fmt::fmt
        flecs::flecs_static
        SDL3::SDL3
        GPUOpen::VulkanMemoryAllocator
)

add_test(NAME ProjectVTests COMMAND ProjectVTests)
