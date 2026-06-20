macro(projectv_test_add_modules TARGET)
    if (NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
        target_sources(${TARGET} PRIVATE
            FILE_SET CXX_MODULES FILES
            ${CMAKE_SOURCE_DIR}/src/core/Math.ixx
            ${CMAKE_SOURCE_DIR}/src/core/StringId.ixx
            BASE_DIRS
            ${CMAKE_SOURCE_DIR}/src
        )
    endif ()
endmacro()

add_executable(ProjectVTests
        VoxelWorldTests.cpp
        ../src/app/AppUpdate.cpp
        ../src/app/AppUpdateHelpers.cpp
        ../src/app/BenchmarkAutomation.cpp
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
        glm
        miniaudio
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVTests PRIVATE Tracy::TracyClient)
endif ()

projectv_test_add_modules(ProjectVTests)

add_test(NAME ProjectVTests COMMAND ProjectVTests)
set_tests_properties(ProjectVTests PROPERTIES LABELS "unit")

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

projectv_test_add_modules(ProjectVAssetTests)

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

projectv_test_add_modules(ProjectVMeshBakerTests)

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

projectv_test_add_modules(ProjectVDracoTests)

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
target_link_libraries(ProjectVFrustumCullingTests PRIVATE volk glm GPUOpen::VulkanMemoryAllocator)

add_test(NAME ProjectVFrustumCullingTests COMMAND ProjectVFrustumCullingTests)
set_tests_properties(ProjectVFrustumCullingTests PROPERTIES LABELS "unit")

add_executable(ProjectVCFrustumCullingTests
        CFrustumCullingTests.cpp
        ${CMAKE_SOURCE_DIR}/src/c_kernels/FrustumCulling.cpp
)

target_compile_definitions(ProjectVCFrustumCullingTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVCFrustumCullingTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVCFrustumCullingTests PRIVATE
        projectv_build_options
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
        projectv_c_kernels
)

add_test(NAME ProjectVCFrustumCullingTests COMMAND ProjectVCFrustumCullingTests)
set_tests_properties(ProjectVCFrustumCullingTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVCFrustumCullingTests)

add_executable(ProjectVSunShadowCascadeSplitsTests
        SunShadowCascadeSplitsTests.cpp
        ${CMAKE_SOURCE_DIR}/src/render/ShadowProjection.cpp
)

target_compile_definitions(ProjectVSunShadowCascadeSplitsTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVSunShadowCascadeSplitsTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVSunShadowCascadeSplitsTests PRIVATE
        projectv_build_options
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

add_test(NAME ProjectVSunShadowCascadeSplitsTests COMMAND ProjectVSunShadowCascadeSplitsTests)
set_tests_properties(ProjectVSunShadowCascadeSplitsTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVSunShadowCascadeSplitsTests)

add_executable(ProjectVGraphicsPushConstantsTests
        GraphicsPushConstantsTests.cpp
        ${CMAKE_SOURCE_DIR}/src/app/Camera.cpp
        ${CMAKE_SOURCE_DIR}/src/app/InputActions.cpp
)

target_compile_definitions(ProjectVGraphicsPushConstantsTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVGraphicsPushConstantsTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVGraphicsPushConstantsTests PRIVATE
        projectv_build_options
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
)

add_test(NAME ProjectVGraphicsPushConstantsTests COMMAND ProjectVGraphicsPushConstantsTests)
set_tests_properties(ProjectVGraphicsPushConstantsTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVGraphicsPushConstantsTests)

add_executable(ProjectVVisibilityCacheHashTests
        VisibilityCacheHashTests.cpp
)

target_compile_definitions(ProjectVVisibilityCacheHashTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVVisibilityCacheHashTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVVisibilityCacheHashTests PRIVATE
        projectv_build_options
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

add_test(NAME ProjectVVisibilityCacheHashTests COMMAND ProjectVVisibilityCacheHashTests)
set_tests_properties(ProjectVVisibilityCacheHashTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVVisibilityCacheHashTests)

projectv_test_add_modules(ProjectVFrustumCullingTests)

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

projectv_test_add_modules(ProjectVBoxUvFixtureTests)

add_executable(ProjectVMathTests
        MathTest.cpp
)

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

add_executable(ProjectVStringIdTests
        StringIdTest.cpp
)

projectv_test_add_modules(ProjectVStringIdTests)

target_compile_definitions(ProjectVStringIdTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVStringIdTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVStringIdTests PRIVATE projectv_build_options)

add_test(NAME ProjectVStringIdTests COMMAND ProjectVStringIdTests)
set_tests_properties(ProjectVStringIdTests PROPERTIES LABELS "unit")

if (NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
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

add_test(NAME ProjectVStdModuleProbe COMMAND ProjectVStdModuleProbe)
set_tests_properties(ProjectVStdModuleProbe PROPERTIES LABELS "unit")
endif ()

if (PROJECTV_ENABLE_BENCHMARKS)
    add_executable(ProjectVFrustumCullBenchmark
            ${CMAKE_SOURCE_DIR}/src/bench/FrustumCullBenchmark.cpp
    )

    target_include_directories(ProjectVFrustumCullBenchmark PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )

    target_link_libraries(ProjectVFrustumCullBenchmark PRIVATE
            projectv_build_options
            benchmark::benchmark
            projectv_c_kernels
            volk
            glm
            GPUOpen::VulkanMemoryAllocator
    )

    projectv_test_add_modules(ProjectVFrustumCullBenchmark)

    target_compile_options(ProjectVFrustumCullBenchmark PRIVATE
            -mavx2
    )

    add_executable(ProjectVShadowProjectionBenchmark
            ${CMAKE_SOURCE_DIR}/src/bench/ShadowProjectionBenchmark.cpp
            ${CMAKE_SOURCE_DIR}/src/render/ShadowProjection.cpp
            ${CMAKE_SOURCE_DIR}/src/voxel/VoxelWorld.cpp
            ${CMAKE_SOURCE_DIR}/src/physics/PhysicsWorld.cpp
            ${CMAKE_SOURCE_DIR}/src/app/InputActions.cpp
            ${CMAKE_SOURCE_DIR}/src/core/RuntimeDiagnostics.cpp
            ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanResult.cpp
    )

    target_include_directories(ProjectVShadowProjectionBenchmark PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/external/JoltPhysics
            ${CMAKE_SOURCE_DIR}/external/tracy/public
            ${CMAKE_SOURCE_DIR}/external/volk
    )

    target_link_libraries(ProjectVShadowProjectionBenchmark PRIVATE
            projectv_build_options
            benchmark::benchmark
            volk
            glm
            GPUOpen::VulkanMemoryAllocator
            fmt::fmt
            SDL3::SDL3
            Jolt::Jolt
    )

    projectv_test_add_modules(ProjectVShadowProjectionBenchmark)

endif ()

add_executable(ProjectVFluidCATests
        FluidCATests.cpp
        ${CMAKE_SOURCE_DIR}/src/voxel/VoxelWorld.cpp
        ${CMAKE_SOURCE_DIR}/src/physics/PhysicsWorld.cpp
        ${CMAKE_SOURCE_DIR}/src/app/InputActions.cpp
        ${CMAKE_SOURCE_DIR}/src/core/RuntimeDiagnostics.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanResult.cpp
)

target_compile_definitions(ProjectVFluidCATests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVFluidCATests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/tracy/public
        ${CMAKE_SOURCE_DIR}/external/volk
        ${CMAKE_SOURCE_DIR}/external/JoltPhysics
)

target_link_libraries(ProjectVFluidCATests PRIVATE
        projectv_build_options
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
        fmt::fmt
        Jolt::Jolt
)

add_test(NAME ProjectVFluidCATests COMMAND ProjectVFluidCATests)
set_tests_properties(ProjectVFluidCATests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVFluidCATests)

add_executable(ProjectVPresentModeTests
        PresentModeTests.cpp
)

target_compile_definitions(ProjectVPresentModeTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVPresentModeTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVPresentModeTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVPresentModeTests PRIVATE
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVPresentModeTests PRIVATE Tracy::TracyClient)
endif ()

add_test(NAME ProjectVPresentModeTests COMMAND ProjectVPresentModeTests)
set_tests_properties(ProjectVPresentModeTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVPresentModeTests)

add_executable(ProjectVSparse64TreeTests
        Sparse64TreeTests.cpp
)

target_compile_definitions(ProjectVSparse64TreeTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVSparse64TreeTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVSparse64TreeTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVSparse64TreeTests PRIVATE
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVSparse64TreeTests PRIVATE Tracy::TracyClient)
endif ()

add_test(NAME ProjectVSparse64TreeTests COMMAND ProjectVSparse64TreeTests)
set_tests_properties(ProjectVSparse64TreeTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVSparse64TreeTests)

add_executable(ProjectVNanoVdbTests
        NanoVdbTests.cpp
        ../src/voxel/NanoVdb.cpp
)

target_compile_definitions(ProjectVNanoVdbTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVNanoVdbTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVNanoVdbTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVNanoVdbTests PRIVATE
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVNanoVdbTests PRIVATE Tracy::TracyClient)
endif ()

add_test(NAME ProjectVNanoVdbTests COMMAND ProjectVNanoVdbTests)
set_tests_properties(ProjectVNanoVdbTests PROPERTIES LABELS "unit")

add_executable(ProjectVCpuMeshGeneratorTests
        CpuMeshGeneratorTests.cpp
        ${CMAKE_SOURCE_DIR}/src/voxel/CpuMeshGenerator.cpp
)

target_compile_definitions(ProjectVCpuMeshGeneratorTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVCpuMeshGeneratorTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVCpuMeshGeneratorTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVCpuMeshGeneratorTests PRIVATE
        volk
        glm
        GPUOpen::VulkanMemoryAllocator
)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVCpuMeshGeneratorTests PRIVATE Tracy::TracyClient)
endif ()

add_test(NAME ProjectVCpuMeshGeneratorTests COMMAND ProjectVCpuMeshGeneratorTests)
set_tests_properties(ProjectVCpuMeshGeneratorTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVCpuMeshGeneratorTests)

add_executable(ProjectVHzbCullingTests
        HzbCullingTests.cpp
        vma_implementation.cpp
        ${CMAKE_SOURCE_DIR}/src/render/HizCulling.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanDebug.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanResult.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ShaderIO.cpp
        ${CMAKE_SOURCE_DIR}/src/core/RuntimeDiagnostics.cpp
)

target_sources(ProjectVHzbCullingTests PRIVATE
)

target_compile_definitions(ProjectVHzbCullingTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVHzbCullingTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ProjectVHzbCullingTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVHzbCullingTests PRIVATE volk)
target_link_libraries(ProjectVHzbCullingTests PRIVATE glm)
target_link_libraries(ProjectVHzbCullingTests PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(ProjectVHzbCullingTests PRIVATE fmt::fmt)
target_link_libraries(ProjectVHzbCullingTests PRIVATE SDL3::SDL3)

if (PROJECTV_ENABLE_TRACY)
    target_link_libraries(ProjectVHzbCullingTests PRIVATE Tracy::TracyClient)
endif ()

add_test(NAME ProjectVHzbCullingTests COMMAND ProjectVHzbCullingTests)
set_tests_properties(ProjectVHzbCullingTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVHzbCullingTests)

add_executable(ProjectVMeshShaderTests
        MeshShaderTests.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanMeshShaderPipeline.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanDebug.cpp
        ${CMAKE_SOURCE_DIR}/src/render/vulkan/VulkanResult.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ShaderIO.cpp
        ${CMAKE_SOURCE_DIR}/src/core/RuntimeDiagnostics.cpp
)

target_compile_definitions(ProjectVMeshShaderTests PRIVATE
        PROJECTV_TESTS_SOURCE_DIR="${PROJECTV_TESTS_SOURCE_DIR_CMAKE}"
)

target_include_directories(ProjectVMeshShaderTests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/tracy/public
        ${CMAKE_SOURCE_DIR}/external/volk
)

target_link_libraries(ProjectVMeshShaderTests PRIVATE projectv_build_options)
target_link_libraries(ProjectVMeshShaderTests PRIVATE volk)
target_link_libraries(ProjectVMeshShaderTests PRIVATE glm)
target_link_libraries(ProjectVMeshShaderTests PRIVATE fmt::fmt)
target_link_libraries(ProjectVMeshShaderTests PRIVATE GPUOpen::VulkanMemoryAllocator)
target_link_libraries(ProjectVMeshShaderTests PRIVATE SDL3::SDL3)

add_test(NAME ProjectVMeshShaderTests COMMAND ProjectVMeshShaderTests)
set_tests_properties(ProjectVMeshShaderTests PROPERTIES LABELS "unit")

projectv_test_add_modules(ProjectVMeshShaderTests)

