cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(ProjectV
        VERSION 0.0.1
        LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_C_STANDARD 23)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#------------------------------------------------------------------------

#set(DRACO_WERROR OFF CACHE BOOL "Disable Draco WError" FORCE)
#set(SDL_WERROR OFF CACHE BOOL "Disable SDL WError" FORCE)
#set(RMLUI_WARNINGS_AS_ERRORS OFF CACHE BOOL "Disable RmlUi WError" FORCE)
#
#find_package(Vulkan REQUIRED)
#
## --- Volk ---
#set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
#add_subdirectory(external/volk)
#
#add_subdirectory(external/VulkanMemoryAllocator)
#
## --- Draco ---
#set(CMAKE_POLICY_DEFAULT_CMP0148 OLD)
#add_subdirectory(external/draco)
#
#add_subdirectory(external/fastgltf)
#add_subdirectory(external/flecs)
#
## --- Freetype ---
#set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
#set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
#set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
#set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
#set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
#add_subdirectory(external/freetype)
#add_library(Freetype::Freetype ALIAS freetype)
#
#add_subdirectory(external/glaze)
#add_subdirectory(external/glm)
#
## --- ImGui (Manual Build) ---
#set(IMGUI_DIR "external/imgui")
#add_library(imgui STATIC
#        ${IMGUI_DIR}/imgui.cpp
#        ${IMGUI_DIR}/imgui_draw.cpp
#        ${IMGUI_DIR}/imgui_tables.cpp
#        ${IMGUI_DIR}/imgui_widgets.cpp
#        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
#        ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
#        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
#        ${IMGUI_DIR}/misc/freetype/imgui_freetype.cpp
#)
#target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
#target_link_libraries(imgui PRIVATE SDL3::SDL3 Vulkan::Vulkan Freetype::Freetype)
#
## --- Jolt Physics ---
#set(JPH_DEBUG_RENDERER ON CACHE BOOL "" FORCE)
#set(JPH_PROFILE_ENABLED ON CACHE BOOL "" FORCE)
#add_subdirectory(external/JoltPhysics/Build)
#
#add_subdirectory(external/meshoptimizer)
#add_subdirectory(external/miniaudio)
#
## --- RmlUi ---
#add_subdirectory(external/RmlUi)
#
## --- SDL3 ---
#set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)
#add_subdirectory(external/SDL)
#
## --- Tracy ---
#option(TRACY_ENABLE "Enable Tracy profiling" ON)
#add_subdirectory(external/tracy)
#
#add_subdirectory(external/zstd)

#------------------------------------------------------------------------

add_executable(ProjectV
        src/main.cpp
        # Add new files here
)

target_sources(ProjectV
        PRIVATE
        src/main.cpp
)

#target_compile_definitions(ProjectV PRIVATE
#        VK_NO_PROTOTYPES
#)

target_compile_options(ProjectV PRIVATE
        /W4
        /WX
        /permissive-
        /utf-8
)

#target_link_libraries(ProjectV PRIVATE
#        volk
#        GPUOpen::VulkanMemoryAllocator
#        draco::draco
#        fastgltf::fastgltf
#        flecs::flecs_static
#        freetype
#        glaze::glaze
#        glm::glm
#        imgui
#        Jolt
#        meshoptimizer
#        miniaudio
#        RmlUi::RmlUi
#        SDL3::SDL3
#)

#add_custom_command(TARGET ProjectV POST_BUILD
#        COMMAND ${CMAKE_COMMAND} -E copy_if_different
#        "$<TARGET_FILE:SDL3::SDL3>"
#        "$<TARGET_FILE_DIR:ProjectV>"
#)
