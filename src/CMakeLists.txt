add_executable(ProjectV)

# --- БЛОК АВТОКОМПИЛЯЦИИ ШЕЙДЕРОВ ---
find_program(PROJECTV_GLSLC_EXECUTABLE glslc)
find_program(PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if (PROJECTV_GLSLC_EXECUTABLE)
    set(PROJECTV_SHADER_COMPILER "${PROJECTV_GLSLC_EXECUTABLE}")
    set(PROJECTV_SHADER_COMPILER_ARGS)
elseif (PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE)
    set(PROJECTV_SHADER_COMPILER "${PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE}")
    set(PROJECTV_SHADER_COMPILER_ARGS -V)
else ()
    message(FATAL_ERROR "ProjectV requires either glslc or glslangValidator from the Vulkan SDK in PATH")
endif ()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADERS
        ${SHADER_DIR}/voxel.vert
        ${SHADER_DIR}/voxel.frag
        ${SHADER_DIR}/voxel_shadow.vert
        ${SHADER_DIR}/voxel_shadow.frag
        ${SHADER_DIR}/voxel_mesh.comp
        ${SHADER_DIR}/taa_resolve.vert
        ${SHADER_DIR}/taa_resolve.frag
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
            COMMAND ${PROJECTV_SHADER_COMPILER} ${PROJECTV_SHADER_COMPILER_ARGS} ${SHADER} -o ${SPV}
            DEPENDS ${SHADER}
            COMMAND_EXPAND_LISTS
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
        app/InputReplay.cpp
        app/LookDevCaptureAutomation.cpp
        app/main.cpp
        asset/AssetStub.cpp
        asset/AssetLoader.cpp
        asset/AssetManifest.cpp
        asset/AssetRegistry.cpp
        ecs/EcsWorld.cpp
        physics/PhysicsWorld.cpp
        core/RuntimeDiagnostics.cpp
        core/RuntimeProbe.cpp
        core/ShaderIO.cpp
        core/Types.cpp
        debug/DebugHud.cpp
        debug/DebugOverlays.cpp
        platform/PlatformEvents.cpp
        render/Renderer.cpp
        render/ScreenshotCapture.cpp
        render/SceneResources.cpp
        render/ShadowProjection.cpp
        render/Taa.cpp
        render/TaaRenderTargets.cpp
        render/vulkan/TaaResolvePipeline.cpp
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

target_link_libraries(ProjectV PRIVATE projectv_build_options)

target_link_libraries(ProjectV PRIVATE
        # Vulkan не добавляем, потому что volk его инклюдит в себе
        volk
        fmt::fmt
        flecs::flecs_static
        Jolt::Jolt
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        # Polygon-model import pipeline (M0–M5). `meshoptimizer` propagates its
        # `src/` include dir through the INTERFACE property, so the rest of
        # the project picks up `<meshoptimizer.h>` automatically.
        meshoptimizer
        glm
        fastgltf::fastgltf
        draco::draco
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif ()

add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_SHADERS}
        $<TARGET_FILE_DIR:ProjectV>
)

if (WIN32)
    find_program(PROJECTV_POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)

    add_custom_target(ProjectVRuntimeSmoke
            COMMAND
            "${PROJECTV_POWERSHELL_EXECUTABLE}"
            -ExecutionPolicy
            Bypass
            -File
            "${CMAKE_SOURCE_DIR}/tools/windows/Invoke-ProjectVRuntimeSmoke.ps1"
            -ExePath
            "$<TARGET_FILE:ProjectV>"
            DEPENDS ProjectV
            USES_TERMINAL
            VERBATIM
    )
endif ()
