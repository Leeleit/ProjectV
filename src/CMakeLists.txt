# =============================================================================
# Main Executable
# =============================================================================

add_executable(ProjectV
        main.cpp
        # Add new files here
)

target_include_directories(ProjectV PRIVATE .)

# =============================================================================
# Strict Compiler Options
# =============================================================================

target_compile_definitions(ProjectV PRIVATE
        VK_NO_PROTOTYPES
)

target_compile_options(ProjectV PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /utf-8
)

# =============================================================================
# Linking
# =============================================================================

target_link_libraries(ProjectV PRIVATE
        volk
        GPUOpen::VulkanMemoryAllocator
        draco::draco
        fastgltf::fastgltf
        flecs::flecs_static
        freetype
        glaze::glaze
        glm::glm
        imgui
        Jolt
        meshoptimizer
        miniaudio
        RmlUi::RmlUi
        SDL3::SDL3
)

# =============================================================================
# Tracy Configuration
# =============================================================================

if (TRACY_ENABLE)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(ProjectV PRIVATE TRACY_CALLSTACK=8 TRACY_MEMORY TRACY_VERBOSE)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(ProjectV PRIVATE TRACY_ONLY_FRAME TRACY_NO_CALLSTACK TRACY_LOW_OVERHEAD)
    endif ()
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif ()

# =============================================================================
# Post-Build
# =============================================================================

add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:SDL3::SDL3>"
        "$<TARGET_FILE_DIR:ProjectV>"
)
