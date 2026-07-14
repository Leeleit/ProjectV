add_executable(ProjectV)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
else ()
    target_sources(ProjectV PRIVATE
            FILE_SET CXX_MODULES FILES
            core/Math.ixx
            core/StringId.ixx
            core/Probe.ixx
            core/Types.ixx
            ecs/EcsWorld.ixx
    )
endif ()

find_program(PROJECTV_GLSLC_EXECUTABLE glslc)
find_program(PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if (PROJECTV_GLSLC_EXECUTABLE)
    set(PROJECTV_SHADER_COMPILER "${PROJECTV_GLSLC_EXECUTABLE}")
    set(PROJECTV_SHADER_COMPILER_ARGS --target-env=vulkan1.3 -I${CMAKE_CURRENT_SOURCE_DIR}/shaders)
elseif (PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE)
    set(PROJECTV_SHADER_COMPILER "${PROJECTV_GLSLANG_VALIDATOR_EXECUTABLE}")
    set(PROJECTV_SHADER_COMPILER_ARGS -V --target-env vulkan1.3 -I${CMAKE_CURRENT_SOURCE_DIR}/shaders)
else ()
    message(FATAL_ERROR "ProjectV requires either glslc or glslangValidator from the Vulkan SDK in PATH")
endif ()

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADERS
        ${SHADER_DIR}/voxel.vert
        ${SHADER_DIR}/voxel.frag
        ${SHADER_DIR}/voxel_mesh.comp
        ${SHADER_DIR}/voxel_face_cluster.comp
        ${SHADER_DIR}/voxel_mesh_pre.comp
        ${SHADER_DIR}/voxel_mesh.mesh
        ${SHADER_DIR}/hzb_cull.comp
        ${SHADER_DIR}/hiz_minify.comp
        ${SHADER_DIR}/fluid_ca.comp
        ${SHADER_DIR}/world_gen.comp
        ${SHADER_DIR}/voxelize.comp
        ${SHADER_DIR}/debug_overlay.vert
        ${SHADER_DIR}/debug_overlay.frag
        ${SHADER_DIR}/model.vert
        ${SHADER_DIR}/model.frag
        ${SHADER_DIR}/sky_atmosphere.vert
        ${SHADER_DIR}/sky_atmosphere.frag
        ${SHADER_DIR}/volumetric_fog.comp
        ${SHADER_DIR}/cloudscape.vert
        ${SHADER_DIR}/cloudscape.frag
        ${SHADER_DIR}/voxel_rtx_shadow.rgen
        ${SHADER_DIR}/voxel_rtx_shadow.rint
        ${SHADER_DIR}/voxel_rtx_shadow.rchit
        ${SHADER_DIR}/voxel_rtx_shadow.rmiss
        ${SHADER_DIR}/probe_update.comp
        ${SHADER_DIR}/bloom_threshold.comp
        ${SHADER_DIR}/bloom_downsample.comp
        ${SHADER_DIR}/bloom_upsample.comp
        ${SHADER_DIR}/post_composite.comp
        ${SHADER_DIR}/post_composite_bindless.comp
        ${SHADER_DIR}/tonemap_resolve.comp
        ${SHADER_DIR}/progressive_accum.comp
        ${SHADER_DIR}/smaa_edge.comp
        ${SHADER_DIR}/smaa_blend.comp
        ${SHADER_DIR}/smaa_neighborhood.comp
)

foreach (SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
            OUTPUT ${SPV}
            COMMAND ${PROJECTV_SHADER_COMPILER} ${PROJECTV_SHADER_COMPILER_ARGS} ${SHADER} -o ${SPV}
            DEPENDS ${SHADER} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common/common_constants.glsl
            COMMAND_EXPAND_LISTS
            VERBATIM
    )
    list(APPEND SPV_SHADERS ${SPV})
endforeach ()

set(VOXEL_FRAG_RTX_SPV ${CMAKE_CURRENT_BINARY_DIR}/voxel.frag.rtx.spv)
add_custom_command(
        OUTPUT ${VOXEL_FRAG_RTX_SPV}
        COMMAND ${PROJECTV_SHADER_COMPILER} ${PROJECTV_SHADER_COMPILER_ARGS}
        -DVOXEL_RTX_ENABLED ${SHADER_DIR}/voxel.frag -o ${VOXEL_FRAG_RTX_SPV}
        DEPENDS ${SHADER_DIR}/voxel.frag
        COMMAND_EXPAND_LISTS
        VERBATIM
)
list(APPEND SPV_SHADERS ${VOXEL_FRAG_RTX_SPV})

add_custom_target(Shaders ALL DEPENDS ${SPV_SHADERS})

target_sources(ProjectV PRIVATE
        app/AppUpdate.cpp
        app/AppUpdateHelpers.cpp
        app/BenchmarkAutomation.cpp
        app/Camera.cpp
        app/FramePreparation.cpp
        c_kernels/FrustumCulling.cpp
        app/InputActions.cpp
        app/ModelGravigun.cpp
        app/InputReplay.cpp
        app/LookDevCaptureAutomation.cpp
        app/main.cpp
        asset/AssetStub.cpp
        asset/AssetLoader.cpp
        asset/AssetManifest.cpp
        asset/AssetRegistry.cpp
        asset/DracoMeshDecoder.cpp
        asset/MeshBaker.cpp
        asset/MeshGpuResources.cpp
        asset/ModelPass.cpp
        asset/ModelManifestLoader.cpp
        audio/AudioEngine.cpp
        audio/MusicDirectoryPath.cpp
        ecs/EcsWorld.cpp
        physics/GreedyPhysicsMerger.cpp
        physics/PhysicsWorld.cpp
        physics/PhysicsWorld_Walk.cpp
        physics/walk/WalkInternals.cpp
        core/RuntimeDiagnostics.cpp
        core/RuntimeProbe.cpp
        core/RepoRoot.cpp
        core/ShaderIO.cpp
        core/Types.cpp
        debug/DebugOverlays.cpp
        ui/ImGuiLayer.cpp
        ui/HudStyle.cpp
        ui/HudPanels.cpp
        platform/PlatformEvents.cpp
        render/Renderer.cpp
        render/RendererOverlay.cpp
        render/RendererScreenshot.cpp
        render/RendererRecordCommands.cpp
        render/AaPass.cpp
        render/RendererDrawFrame.cpp
        render/ScreenshotCapture.cpp
        render/SceneResources.cpp
        render/SceneResourcesUtilities.cpp
        render/SceneResourcesVisibility.cpp
        render/SceneResourcesDestroy.cpp
        render/SceneResourcesUpdate.cpp
        render/SceneResourcesFrame.cpp
        render/SceneResourcesFrameCompute.cpp
        render/HizCullingSetup.cpp
        render/HizCullingBuild.cpp
        render/HizCullingDispatch.cpp
        render/LodDownsampleGpuConsume.cpp
        render/RayTracedShadows.cpp
        render/RayTracedShadowsBlas.cpp
        render/RayTracedShadowsTlas.cpp
        render/RayTracedShadowsPass.cpp
        render/RayTracedShadowsMask.cpp
        render/RtxShadowPipeline.cpp
        render/RtxShadowSBT.cpp
        render/RtxGiProbes.cpp
        render/RtxGiProbesPipeline.cpp
        render/RtxGiProbesUpdate.cpp
        render/vulkan/VulkanBootstrapInit.cpp
        render/vulkan/VulkanBootstrapFeatures.cpp
        render/vulkan/VulkanBootstrapDevice.cpp
        render/vulkan/VulkanDebug.cpp
        render/vulkan/VulkanGraphicsPipeline.cpp
        render/vulkan/VulkanGraphicsPipelineBindings.cpp
        render/vulkan/VulkanGraphicsPipelineCreate.cpp
        render/vulkan/VulkanGraphicsPipelineOverlay.cpp
        render/vulkan/HardwareRayTracingProbe.cpp
        render/vulkan/VulkanInit.cpp
        render/vulkan/VulkanResult.cpp
        render/vulkan/VulkanSwapchain.cpp
        render/vulkan/VulkanSyncPrimitives.cpp
        render/vulkan/VulkanVoxelMeshingPipeline.cpp
        render/vulkan/VulkanMeshShaderSetup.cpp
        render/vulkan/VulkanMeshShaderBindings.cpp
        render/vulkan/VulkanMeshShaderCull.cpp
        render/vulkan/VulkanMeshShaderDraw.cpp
        render/vulkan/VulkanFluidCaSetup.cpp
        render/vulkan/VulkanFluidCaDispatch.cpp
        render/vulkan/VulkanVoxelizeSetup.cpp
        render/vulkan/VulkanVoxelizeMipChain.cpp
        render/vulkan/VulkanWorldGenPipeline.cpp
        render/vulkan/VulkanAsyncCompute.cpp
        render/SkyAtmosphereLut.cpp
        render/SkyAtmosphereSetup.cpp
        render/SkyAtmospherePass.cpp
        render/VolumetricFog.cpp
        render/Cloudscape.cpp
        render/CloudscapeResources.cpp
        render/PostFxSetup.cpp
        render/PostFxDestroy.cpp
        render/PostFxPass.cpp
        render/BindlessHeap.cpp
        voxel/VoxelInteraction.cpp
        voxel/VoxelMaterials.cpp
        voxel/VoxelRaycast.cpp
        voxel/VoxelLodDownsample.cpp
        voxel/NanoVdb.cpp
        voxel/VoxelWorld.cpp
        voxel/VoxelWorldPreset.cpp
        voxel/VoxelWorldSnapshot.cpp
        voxel/VoxelWorldFluid.cpp
        voxel/VoxelWorldLod.cpp
        voxel/VoxelWorldStatic.cpp
        voxel/ChunkStreamer.cpp
        voxel/SceneConfig.cpp
)

target_include_directories(ProjectV PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(${PROJECT_SOURCE_DIR}/external/miniaudio
        ${CMAKE_BINARY_DIR}/external/miniaudio EXCLUDE_FROM_ALL)
projectv_suppress_external_warnings("${PROJECT_SOURCE_DIR}/external/miniaudio")
target_link_libraries(ProjectV PRIVATE miniaudio)

if (LINUX)
    target_link_libraries(ProjectV PRIVATE pthread dl m)
endif ()

target_link_libraries(ProjectV PRIVATE projectv_build_options)

target_compile_definitions(ProjectV PRIVATE
        PROJECTV_CMAKE_BUILD_DIR="${CMAKE_BINARY_DIR}")

target_link_libraries(ProjectV PRIVATE
        volk
        fmt::fmt
        flecs::flecs_static
        Jolt::Jolt
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        meshoptimizer
        glm
        fastgltf::fastgltf
        draco::draco
        nlohmann_json::nlohmann_json
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif ()

add_custom_target(CopyShaders ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_SHADERS}
        $<TARGET_FILE_DIR:ProjectV>
)
add_dependencies(CopyShaders Shaders)
add_dependencies(ProjectV CopyShaders)

add_library(projectv_c_kernels STATIC
        c_kernels/frustum_cull.cpp
)
target_include_directories(projectv_c_kernels PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(projectv_c_kernels PUBLIC cxx_std_26)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(projectv_c_kernels PRIVATE -mavx2)
endif ()
target_link_libraries(ProjectV PRIVATE projectv_c_kernels)

if (WIN32)
    find_program(PROJECTV_POWERSHELL_EXECUTABLE
            NAMES pwsh powershell
            HINTS "C:/Program Files/PowerShell/7"
            "C:/Windows/System32/WindowsPowerShell/v1.0"
            REQUIRED)

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
