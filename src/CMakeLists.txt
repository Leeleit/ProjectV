add_executable(ProjectV)

# --- БЛОК АВТОКОМПИЛЯЦИИ ШЕЙДЕРОВ ---
find_program(GLSLC glslc REQUIRED)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADERS
        ${SHADER_DIR}/triangle.vert
        ${SHADER_DIR}/triangle.frag
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
        main.cpp
        Types.cpp
        Renderer.cpp
        VulkanBootstrap.cpp
        VulkanMesh.cpp
        VulkanPipeline.cpp
        VulkanSwapchain.cpp
        VulkanInit.cpp
        # Add new files here
)

target_compile_definitions(ProjectV PRIVATE
        NOMINMAX
        VK_NO_PROTOTYPES
)

target_compile_options(ProjectV PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
)

target_link_libraries(ProjectV PRIVATE
        # Vulkan не добавляем, потому что volk его инклюдит в себе
        volk
        # GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
)

add_custom_command(TARGET ProjectV POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SPV_SHADERS}
        $<TARGET_FILE_DIR:ProjectV>
)
