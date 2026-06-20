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
        ${SHADER_DIR}/model.vert
        ${SHADER_DIR}/model.frag
        ${SHADER_DIR}/ray_march.comp
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

set(VOXEL_FRAG_TAA_SPV ${CMAKE_CURRENT_BINARY_DIR}/voxel.frag.taa_on.spv)
add_custom_command(
        OUTPUT ${VOXEL_FRAG_TAA_SPV}
        COMMAND ${PROJECTV_SHADER_COMPILER} ${PROJECTV_SHADER_COMPILER_ARGS}
        -DTAA_ENABLED ${SHADER_DIR}/voxel.frag -o ${VOXEL_FRAG_TAA_SPV}
        DEPENDS ${SHADER_DIR}/voxel.frag
        COMMAND_EXPAND_LISTS
        VERBATIM
)
list(APPEND SPV_SHADERS ${VOXEL_FRAG_TAA_SPV})

set(MODEL_FRAG_TAA_SPV ${CMAKE_CURRENT_BINARY_DIR}/model.frag.taa_on.spv)
add_custom_command(
        OUTPUT ${MODEL_FRAG_TAA_SPV}
        COMMAND ${PROJECTV_SHADER_COMPILER} ${PROJECTV_SHADER_COMPILER_ARGS}
        -DTAA_ENABLED ${SHADER_DIR}/model.frag -o ${MODEL_FRAG_TAA_SPV}
        DEPENDS ${SHADER_DIR}/model.frag
        COMMAND_EXPAND_LISTS
        VERBATIM
)
list(APPEND SPV_SHADERS ${MODEL_FRAG_TAA_SPV})

add_custom_target(Shaders ALL DEPENDS ${SPV_SHADERS})
add_dependencies(ProjectV Shaders)

target_sources(ProjectV PRIVATE
        app/AppUpdate.cpp
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
        physics/PhysicsWorld.cpp
        core/RuntimeDiagnostics.cpp
        core/RuntimeProbe.cpp
        core/RepoRoot.cpp
        core/ShaderIO.cpp
        core/Types.cpp
        debug/DebugHud.cpp
        debug/DebugOverlays.cpp
        platform/PlatformEvents.cpp
        render/Renderer.cpp
        render/RayMarchPass.cpp
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

add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_SHADERS}
        $<TARGET_FILE_DIR:ProjectV>
)

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
