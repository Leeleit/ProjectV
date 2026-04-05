add_executable(ProjectVTests
        VoxelWorldTests.cpp
        ../src/VoxelMaterials.cpp
        ../src/VoxelWorld.cpp
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
        SDL3::SDL3
        GPUOpen::VulkanMemoryAllocator
)

add_test(NAME ProjectVTests COMMAND ProjectVTests)
