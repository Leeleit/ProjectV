add_executable(ProjectV)

# **Tier 2.A (`2026-06-13`).** First real `.ixx` module for
# the project: `core/Math.ixx` exports
# `projectv::math::{Vec3,Vec4,Mat4,dot,cross,lengthSq,length,
# normalize,identity,zero,transpose,inverse,operator*+,-,fromArray16}`.
# The header `core/Math.hpp` is kept as a re-export shim for
# TUs that still use the `#include` form (Tier 2.D will
# migrate them to `import projectv.math;` and delete the
# header).
target_sources(ProjectV PRIVATE
        FILE_SET CXX_MODULES FILES
        core/Math.ixx
        core/Probe.ixx
)

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
        ${SHADER_DIR}/model.vert
        ${SHADER_DIR}/model.frag
        # Defense r0 (2026-06-13): GPU ray-march compute shader for the
        # optional ray-march pass (toggle F6). See
        # `src/render/RayMarchPass.hpp` and `docs/DefenseReport.md §2.1`.
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

# TAA-on variant of voxel.frag: declares outSceneColor (Location 1) instead
# of outColor (Location 0), eliminating unused MRT write warnings.
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

# TAA-on variant of model.frag: declares `outSceneColor` (Location 1) so
# the model pass writes to the same slot as the voxel pass on the TAA-on
# path (where the first color attachment is NULL and the second holds
# the offscreen scene color). Without this variant, the model pass
# writes to a NULL slot and trips the validation layer's "writes to
# output variable but imageView is VK_NULL_HANDLE" warning.
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
# --- КОНЕЦ БЛОКА ШЕЙДЕРОВ ---

target_sources(ProjectV PRIVATE
        app/AppUpdate.cpp
        app/BenchmarkAutomation.cpp
        app/Camera.cpp
        app/FramePreparation.cpp
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
        # Add new files here
)

target_include_directories(ProjectV PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# **Audio engine, 2026-06-12.** miniaudio is vendored as
# a submodule at `external/miniaudio/`. Built as a static
# library (the default per miniaudio's CMakeLists.txt) and
# linked into `ProjectV`. On Linux, miniaudio's
# `find_package(PulseAudio)` resolves `libpulse.so.0` (the
# `pipewire-pulse` shim), which routes audio to PipeWire.
# `EXCLUDE_FROM_ALL` so the miniaudio examples and tests
# don't pollute the build.
add_subdirectory(${PROJECT_SOURCE_DIR}/external/miniaudio
        ${CMAKE_BINARY_DIR}/external/miniaudio EXCLUDE_FROM_ALL)
target_link_libraries(ProjectV PRIVATE miniaudio)

if (LINUX)
    target_link_libraries(ProjectV PRIVATE pthread dl m)
endif ()

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
        # Defense r0 (2026-06-13): nlohmann/json for runtime scene-config
        # loading (per ТЗ 4.5.1). Pulled in via FetchContent at the root
        # `CMakeLists.txt`; here we only attach the target.
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
