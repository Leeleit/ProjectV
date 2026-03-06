add_executable(ProjectV)

target_sources(ProjectV PRIVATE
        main.cpp
        Types.cpp
        Renderer.cpp
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
        GPUOpen::VulkanMemoryAllocator
        SDL3::SDL3
)
