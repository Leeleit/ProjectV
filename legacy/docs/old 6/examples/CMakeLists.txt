# Examples for ProjectV documentation
# Invoked via add_subdirectory(docs/examples) from root CMakeLists
# Inherits: Vulkan, volk, SDL3, VMA from parent

set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(PROJECT_ROOT ${CMAKE_SOURCE_DIR})

# SDL only: minimal window
add_executable(example_sdl_window ${EXAMPLES_DIR}/sdl_window.cpp)
target_link_libraries(example_sdl_window PRIVATE SDL3::SDL3)
add_custom_command(TARGET example_sdl_window POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:example_sdl_window>
        VERBATIM
)

# Volk: initialization only (no window)
add_executable(example_volk_init ${EXAMPLES_DIR}/volk_init.cpp)
target_link_libraries(example_volk_init PRIVATE volk)

# Black window: SDL + Vulkan + VMA (existing)
add_executable(example_black_window ${EXAMPLES_DIR}/black_window.cpp)
target_link_libraries(example_black_window PRIVATE
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        volk
        SDL3::SDL3
)
target_include_directories(example_black_window PRIVATE ${PROJECT_ROOT}/external/VMA/include)
add_custom_command(TARGET example_black_window POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:example_black_window>
        VERBATIM
)

# Compile shaders for vulkan_triangle
find_program(GLSLC glslc)
if (GLSLC)
    add_custom_command(OUTPUT ${EXAMPLES_DIR}/shaders/triangle.vert.spv ${EXAMPLES_DIR}/shaders/triangle.frag.spv
            COMMAND ${GLSLC} ${EXAMPLES_DIR}/shaders/triangle.vert -o ${EXAMPLES_DIR}/shaders/triangle.vert.spv
            COMMAND ${GLSLC} ${EXAMPLES_DIR}/shaders/triangle.frag -o ${EXAMPLES_DIR}/shaders/triangle.frag.spv
            DEPENDS ${EXAMPLES_DIR}/shaders/triangle.vert ${EXAMPLES_DIR}/shaders/triangle.frag
            COMMENT "Compiling Vulkan shaders"
    )
    add_custom_target(compile_vulkan_shaders DEPENDS ${EXAMPLES_DIR}/shaders/triangle.vert.spv ${EXAMPLES_DIR}/shaders/triangle.frag.spv)
endif ()

# Добавление архитектурных примеров (если доступны зависимости)
if (EXISTS ${PROJECT_ROOT}/external/flecs/CMakeLists.txt AND EXISTS ${PROJECT_ROOT}/external/glm/CMakeLists.txt)
    # Архитектурный пример: Modern Vulkan 1.4
    add_executable(example_architecture_modern_vulkan ${EXAMPLES_DIR}/architecture_modern_vulkan.cpp)
    target_link_libraries(example_architecture_modern_vulkan PRIVATE
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
            glm::glm
    )
    target_include_directories(example_architecture_modern_vulkan PRIVATE
            ${PROJECT_ROOT}/external/VMA/include
            ${PROJECT_ROOT}/external/glm
    )
    add_custom_command(TARGET example_architecture_modern_vulkan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_architecture_modern_vulkan>
            VERBATIM
    )

    # Архитектурный пример: Resource Manager
    add_executable(example_architecture_resource_manager ${EXAMPLES_DIR}/architecture_resource_manager.cpp)
    target_link_libraries(example_architecture_resource_manager PRIVATE
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
            glm::glm
    )
    target_include_directories(example_architecture_resource_manager PRIVATE
            ${PROJECT_ROOT}/external/VMA/include
            ${PROJECT_ROOT}/external/glm
    )
    add_custom_command(TARGET example_architecture_resource_manager POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_architecture_resource_manager>
            VERBATIM
    )

    # Архитектурный пример: Core Loop
    add_executable(example_architecture_core_loop ${EXAMPLES_DIR}/architecture_core_loop.cpp)
    target_link_libraries(example_architecture_core_loop PRIVATE
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
            glm::glm
    )
    target_include_directories(example_architecture_core_loop PRIVATE
            ${PROJECT_ROOT}/external/VMA/include
            ${PROJECT_ROOT}/external/glm
    )
    add_custom_command(TARGET example_architecture_core_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_architecture_core_loop>
            VERBATIM
    )

    # Архитектурный пример: Voxel Pipeline
    add_executable(example_architecture_voxel_pipeline ${EXAMPLES_DIR}/architecture_voxel_pipeline.cpp)
    target_link_libraries(example_architecture_voxel_pipeline PRIVATE
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
            glm::glm
    )
    target_include_directories(example_architecture_voxel_pipeline PRIVATE
            ${PROJECT_ROOT}/external/VMA/include
            ${PROJECT_ROOT}/external/glm
    )
    add_custom_command(TARGET example_architecture_voxel_pipeline POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_architecture_voxel_pipeline>
            VERBATIM
    )
endif ()

# Vulkan triangle: SDL + volk + VMA + shaders
add_executable(example_vulkan_triangle ${EXAMPLES_DIR}/vulkan_triangle.cpp)
if (GLSLC)
    add_dependencies(example_vulkan_triangle compile_vulkan_shaders)
endif ()
target_link_libraries(example_vulkan_triangle PRIVATE
        Vulkan::Vulkan
        GPUOpen::VulkanMemoryAllocator
        volk
        SDL3::SDL3
)
target_include_directories(example_vulkan_triangle PRIVATE ${PROJECT_ROOT}/external/VMA/include)
add_custom_command(TARGET example_vulkan_triangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:example_vulkan_triangle>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EXAMPLES_DIR}/shaders/triangle.vert.spv
        ${EXAMPLES_DIR}/shaders/triangle.frag.spv
        $<TARGET_FILE_DIR:example_vulkan_triangle>
        VERBATIM
)

# VMA buffer: standalone Vulkan init + allocator + buffer (no window, no SDL)
# volk only — без Vulkan::Vulkan (volk загружает loader в runtime)
add_executable(example_vma_buffer ${EXAMPLES_DIR}/vma_buffer.cpp)
target_link_libraries(example_vma_buffer PRIVATE
        GPUOpen::VulkanMemoryAllocator
        volk
)
target_include_directories(example_vma_buffer PRIVATE ${PROJECT_ROOT}/external/VMA/include)

# glm: requires glm (add if available)
if (EXISTS ${PROJECT_ROOT}/external/glm/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/glm ${CMAKE_BINARY_DIR}/external/glm EXCLUDE_FROM_ALL)
    add_executable(example_glm_transform ${EXAMPLES_DIR}/glm_transform.cpp)
    target_link_libraries(example_glm_transform PRIVATE glm::glm)
endif ()

# fastgltf: requires fastgltf (add if available)
if (EXISTS ${PROJECT_ROOT}/external/fastgltf/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/fastgltf ${CMAKE_BINARY_DIR}/external/fastgltf EXCLUDE_FROM_ALL)

    # Пример загрузки меша (существующий)
    add_executable(example_fastgltf_load_mesh ${EXAMPLES_DIR}/fastgltf_load_mesh.cpp)
    target_link_libraries(example_fastgltf_load_mesh PRIVATE fastgltf::fastgltf)

    # Пример загрузки текстур (новый)
    add_executable(example_fastgltf_load_textures ${EXAMPLES_DIR}/fastgltf_load_textures.cpp)
    target_link_libraries(example_fastgltf_load_textures PRIVATE fastgltf::fastgltf)

    # Пример работы с материалами (новый)
    add_executable(example_fastgltf_materials ${EXAMPLES_DIR}/fastgltf_materials.cpp)
    target_link_libraries(example_fastgltf_materials PRIVATE fastgltf::fastgltf)

    # Пример работы с анимациями (новый)
    add_executable(example_fastgltf_animations ${EXAMPLES_DIR}/fastgltf_animations.cpp)
    target_link_libraries(example_fastgltf_animations PRIVATE fastgltf::fastgltf)
endif ()

# miniaudio: requires miniaudio (add if available)
# Add "add_subdirectory(external/miniaudio)" to root CMakeLists.txt to enable
if (EXISTS ${PROJECT_ROOT}/external/miniaudio/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/miniaudio ${CMAKE_BINARY_DIR}/external/miniaudio EXCLUDE_FROM_ALL)
    add_executable(example_miniaudio_playback ${EXAMPLES_DIR}/miniaudio_playback.cpp)
    target_link_libraries(example_miniaudio_playback PRIVATE miniaudio miniaudio_common_options)
endif ()

# flecs + SDL: requires flecs (add if available)
if (EXISTS ${PROJECT_ROOT}/external/flecs/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/flecs ${CMAKE_BINARY_DIR}/external/flecs EXCLUDE_FROM_ALL)

    # Базовый пример flecs + SDL
    add_executable(example_flecs_sdl ${EXAMPLES_DIR}/flecs_sdl.cpp)
    target_link_libraries(example_flecs_sdl PRIVATE flecs::flecs_static SDL3::SDL3)
    add_custom_command(TARGET example_flecs_sdl POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_flecs_sdl>
            VERBATIM
    )

    # Продвинутая интеграция SDL событий с flecs ECS
    add_executable(example_sdl_flecs_integration ${EXAMPLES_DIR}/sdl_flecs_integration.cpp)
    target_link_libraries(example_sdl_flecs_integration PRIVATE flecs::flecs_static SDL3::SDL3)
    add_custom_command(TARGET example_sdl_flecs_integration POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_sdl_flecs_integration>
            VERBATIM
    )

    # Архитектурный пример: Flecs + Vulkan 1.4 интеграция
    add_executable(example_architecture_flecs_vulkan ${EXAMPLES_DIR}/architecture_flecs_vulkan.cpp)
    target_link_libraries(example_architecture_flecs_vulkan PRIVATE
            flecs::flecs_static
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
            glm::glm
    )
    target_include_directories(example_architecture_flecs_vulkan PRIVATE
            ${PROJECT_ROOT}/external/VMA/include
            ${PROJECT_ROOT}/external/glm
    )
    add_custom_command(TARGET example_architecture_flecs_vulkan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_architecture_flecs_vulkan>
            VERBATIM
    )
endif ()

# ImGui + SDL3 + Vulkan: requires imgui (add if available)
if (EXISTS ${PROJECT_ROOT}/external/imgui/CMakeLists.txt)
    add_subdirectory(${PROJECT_ROOT}/external/imgui ${CMAKE_BINARY_DIR}/external/imgui EXCLUDE_FROM_ALL)

    # Определяем исходные файлы ImGui
    set(IMGUI_SOURCES
            ${PROJECT_ROOT}/external/imgui/imgui.cpp
            ${PROJECT_ROOT}/external/imgui/imgui_demo.cpp
            ${PROJECT_ROOT}/external/imgui/imgui_draw.cpp
            ${PROJECT_ROOT}/external/imgui/imgui_tables.cpp
            ${PROJECT_ROOT}/external/imgui/imgui_widgets.cpp
            ${PROJECT_ROOT}/external/imgui/backends/imgui_impl_sdl3.cpp
            ${PROJECT_ROOT}/external/imgui/backends/imgui_impl_vulkan.cpp
            ${PROJECT_ROOT}/external/imgui/misc/cpp/imgui_stdlib.cpp
    )

    # Создаём библиотеку ImGui
    add_library(imgui_example STATIC ${IMGUI_SOURCES})
    target_include_directories(imgui_example PUBLIC
            ${PROJECT_ROOT}/external/imgui
            ${PROJECT_ROOT}/external/imgui/backends
            ${PROJECT_ROOT}/external/imgui/misc/cpp
    )
    target_link_libraries(imgui_example PRIVATE SDL3::SDL3 Vulkan::Vulkan)

    # Создаём пример
    add_executable(example_imgui_sdl_vulkan ${EXAMPLES_DIR}/imgui_sdl_vulkan.cpp)
    target_link_libraries(example_imgui_sdl_vulkan PRIVATE
            imgui_example
            Vulkan::Vulkan
            GPUOpen::VulkanMemoryAllocator
            volk
            SDL3::SDL3
    )
    target_include_directories(example_imgui_sdl_vulkan PRIVATE ${PROJECT_ROOT}/external/VMA/include)
    add_custom_command(TARGET example_imgui_sdl_vulkan POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3>
            $<TARGET_FILE_DIR:example_imgui_sdl_vulkan>
            VERBATIM
    )
endif ()

# Multi-window редактор вокселей (только SDL, без Vulkan)
add_executable(example_sdl_multi_window_editor ${EXAMPLES_DIR}/sdl_multi_window_editor.cpp)
target_link_libraries(example_sdl_multi_window_editor PRIVATE SDL3::SDL3)
add_custom_command(TARGET example_sdl_multi_window_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:example_sdl_multi_window_editor>
        VERBATIM
)
