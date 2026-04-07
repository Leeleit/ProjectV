add_executable(ProjectV)

# --- БЛОК АВТОКОМПИЛЯЦИИ ШЕЙДЕРОВ ---
find_program(GLSLC glslc REQUIRED)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADERS
        ${SHADER_DIR}/voxel.vert
        ${SHADER_DIR}/voxel.frag
        ${SHADER_DIR}/voxel_mesh.comp
        ${SHADER_DIR}/debug_overlay.vert
        ${SHADER_DIR}/debug_overlay.frag
        ${SHADER_DIR}/debug_hud.vert
        ${SHADER_DIR}/debug_hud.frag
)

foreach (SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${GLSLC} ${SHADER} -o ${SPV}
            DEPENDS ${SHADER}
            VERBATIM
    )
    list(APPEND SPV_SHADERS ${SPV})
endforeach ()

add_custom_target(Shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(ProjectV Shaders)
# --- КОНЕЦ БЛОКА ШЕЙДЕРОВ ---

target_sources(ProjectV PRIVATE
        app/AppUpdate.cpp
        app/Camera.cpp
        app/FramePreparation.cpp
        app/InputActions.cpp
        app/main.cpp
        core/RuntimeDiagnostics.cpp
        core/RuntimeProbe.cpp
        core/ShaderIO.cpp
        core/Types.cpp
        debug/DebugHud.cpp
        platform/PlatformEvents.cpp
        render/Renderer.cpp
        render/SceneResources.cpp
        render/vulkan/VulkanBootstrap.cpp
        render/vulkan/VulkanDebug.cpp
        render/vulkan/VulkanGraphicsPipeline.cpp
        render/vulkan/VulkanInit.cpp
        render/vulkan/VulkanResult.cpp
        render/vulkan/VulkanSwapchain.cpp
        render/vulkan/VulkanVoxelMeshingPipeline.cpp
        voxel/VoxelInteraction.cpp
        voxel/VoxelMaterials.cpp
        voxel/VoxelRaycast.cpp
        voxel/VoxelWorld.cpp
        # Add new files here
)

target_include_directories(ProjectV PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(ProjectV PRIVATE
        NOMINMAX
        VK_NO_PROTOTYPES
)

if (PROJECTV_ENABLE_TRACY)
    target_compile_definitions(ProjectV PRIVATE PROJECTV_ENABLE_TRACY=1)
endif ()

target_compile_options(ProjectV PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
)

target_link_libraries(ProjectV PRIVATE
        # Vulkan не добавляем, потому что volk его инклюдит в себе
        volk
        fmt::fmt
        # GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif ()

add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_SHADERS}
        $<TARGET_FILE_DIR:ProjectV>
)
