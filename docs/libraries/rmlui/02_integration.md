# Интеграция RmlUi в ProjectV

> **Для понимания:** Интеграция RmlUi — это как подключение профессионального UI-редактора к игровому движку. Мы создаём
> мост: с одной стороны Vulkan для рисования, с другой — Flecs ECS для данных. SDL3 обеспечивает ввод.

## Стек технологий

| Компонент      | Назначение           | Документация        |
|----------------|----------------------|---------------------|
| **Vulkan 1.4** | Графический API      | [vulkan](../vulkan) |
| **SDL3**       | Окна и ввод          | [sdl](../sdl)       |
| **VMA**        | Выделение памяти GPU | [vma](../vma)       |
| **Flecs**      | ECS                  | [flecs](../flecs)   |

## CMake конфигурация

### Добавление RmlUi через FetchContent

```cmake
# CMakeLists.txt
Include(FetchContent)

FetchContent_Declare(
  rmlui
  # rmlui repository configuration
)

# Отключаем всё лишнее
set(RMLUI_SAMPLES OFF CACHE BOOL "")
set(RMLUI_TESTS OFF CACHE BOOL "")
set(RMLUI_LUA OFF CACHE BOOL "")
set(RMLUI_SVG OFF CACHE BOOL "")
set(RMLUI_LOTTIE OFF CACHE BOOL "")

FetchContent_MakeAvailable(rmlui)

target_link_libraries(${PROJECT_NAME} PRIVATE RmlUi)
```

### Альтернатива: подмодуль

```cmake
add_subdirectory(external/RmlUi)
target_link_libraries(${PROJECT_NAME} PRIVATE RmlUi)
```

## Vulkan RenderInterface

### Заголовок

```cpp
// src/ui/rmlui_vulkan_renderer.hpp
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <span>
#include <vector>
#include <expected>

class RmlUiVulkanRenderer final : public Rml::RenderInterface {
public:
    RmlUiVulkanRenderer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkQueue renderQueue,
        uint32_t queueFamilyIndex,
        VmaAllocator allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits msaaSamples
    ) noexcept;

    ~RmlUiVulkanRenderer() override;

    // --- Frame management ---
    void beginFrame(VkCommandBuffer cmd, VkExtent2D viewportExtent) noexcept;
    void endFrame() noexcept;

    // --- Rml::RenderInterface ---
    void RenderGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation
    ) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;

    // --- Текстуры ---
    [[nodiscard]] std::expected<bool, std::errc> LoadTexture(
        Rml::TextureHandle& textureHandle,
        Rml::Vector2i& textureDimensions,
        const Rml::String& source
    ) override;

    [[nodiscard]] std::expected<bool, std::errc> GenerateTexture(
        Rml::TextureHandle& textureHandle,
        const Rml::byte* source,
        const Rml::Vector2i& sourceDimensions
    ) override;

    void ReleaseTexture(Rml::TextureHandle textureHandle) override;

    // --- Compiled Geometry (оптимизация) ---
    Rml::CompiledGeometryHandle CompileGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture
    ) override;

    void RenderCompiledGeometry(
        Rml::CompiledGeometryHandle geometry,
        const Rml::Vector2f& translation
    ) override;

    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;

private:
    struct Texture {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct Geometry {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VmaAllocation vertexAllocation = VK_NULL_HANDLE;
        VmaAllocation indexAllocation = VK_NULL_HANDLE;
        uint32_t indexCount = 0;
        Rml::TextureHandle texture = 0;
    };

    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkQueue renderQueue_ = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex_ = 0;

    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

    VkCommandBuffer currentCmd_ = VK_NULL_HANDLE;
    VkExtent2D currentExtent_ = {0, 0};

    std::vector<Texture> textures_;
    std::vector<Geometry> geometries_;

    bool scissorEnabled_ = false;
    VkRect2D scissorRect_ = {};

    [[nodiscard]] bool createPipeline(VkRenderPass renderPass, VkSampleCountFlagBits samples) noexcept;
    [[nodiscard]] VkDescriptorSet createDescriptorSet(VkImageView imageView, VkSampler sampler) noexcept;
};
```

### Реализация

```cpp
// src/ui/rmlui_vulkan_renderer.cpp
#include "rmlui_vulkan_renderer.hpp"
#include <print>
#include <cstring>

RmlUiVulkanRenderer::RmlUiVulkanRenderer(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkQueue renderQueue,
    uint32_t queueFamilyIndex,
    VmaAllocator allocator,
    VkRenderPass renderPass,
    VkSampleCountFlagBits msaaSamples
) noexcept
    : device_(device)
    , allocator_(allocator)
    , renderQueue_(renderQueue)
    , queueFamilyIndex_(queueFamilyIndex)
{
    createPipeline(renderPass, msaaSamples);

    // Descriptor pool для текстур
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 256
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 256,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };

    vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_);
}

RmlUiVulkanRenderer::~RmlUiVulkanRenderer() {
    // Очистка текстур
    for (auto& tex : textures_) {
        if (tex.sampler) vkDestroySampler(device_, tex.sampler, nullptr);
        if (tex.imageView) vkDestroyImageView(device_, tex.imageView, nullptr);
        if (tex.image) vmaDestroyImage(allocator_, tex.image, tex.allocation);
    }

    // Очистка геометрии
    for (auto& geom : geometries_) {
        vmaDestroyBuffer(allocator_, geom.vertexBuffer, geom.vertexAllocation);
        vmaDestroyBuffer(allocator_, geom.indexBuffer, geom.indexAllocation);
    }

    if (descriptorPool_) vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
    if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
    if (descriptorSetLayout_) vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
}

void RmlUiVulkanRenderer::beginFrame(VkCommandBuffer cmd, VkExtent2D extent) noexcept {
    currentCmd_ = cmd;
    currentExtent_ = extent;
    scissorEnabled_ = false;
}

void RmlUiVulkanRenderer::endFrame() noexcept {
    currentCmd_ = VK_NULL_HANDLE;
}

void RmlUiVulkanRenderer::RenderGeometry(
    Rml::Vertex* vertices, int numVertices,
    int* indices, int numIndices,
    Rml::TextureHandle texture,
    const Rml::Vector2f& translation
) {
    // Для простоты создаём temporary buffers
    // В production используйте CompileGeometry

    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;

    // Staging buffer для вершин
    VkBufferCreateInfo vbInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = static_cast<VkDeviceSize>(numVertices * sizeof(Rml::Vertex)),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo vbAllocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY
    };

    vmaCreateBuffer(allocator_, &vbInfo, &vbAllocInfo,
                    &stagingBuffer, &stagingAllocation, nullptr);

    // Копирование данных
    void* data;
    vmaMapMemory(allocator_, stagingAllocation, &data);
    std::memcpy(data, vertices, numVertices * sizeof(Rml::Vertex));
    vmaUnmapMemory(allocator_, stagingAllocation);

    // Создание vertex buffer
    VkBuffer vertexBuffer;
    VmaAllocation vertexAllocation;

    VkBufferCreateInfo dstVbInfo = vbInfo;
    dstVbInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo dstAllocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    vmaCreateBuffer(allocator_, &dstVbInfo, &dstAllocInfo,
                    &vertexBuffer, &vertexAllocation, nullptr);

    // Copy staging → vertex buffer
    VkCommandBuffer copyCmd;
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = /* создайте отдельный command pool */,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    vkAllocateCommandBuffers(device_, &allocInfo, &copyCmd);
    VkBeginCommandBuffer(copyCmd, &beginInfo);

    VkBufferCopy copyRegion = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = static_cast<VkDeviceSize>(numVertices * sizeof(Rml::Vertex))
    };
    vkCmdCopyBuffer(copyCmd, stagingBuffer, vertexBuffer, 1, &copyRegion);

    vkEndCommandBuffer(copyCmd);

    // Submit copy
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &copyCmd
    };
    vkQueueSubmit(renderQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderQueue_);

    // Очистка staging
    vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);

    // Аналогично для index buffer...

    // Рендеринг
    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(currentCmd_, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (scissorEnabled_) {
        vkCmdSetScissor(currentCmd_, 0, 1, &scissorRect_);
    }

    // Push constants для translation
    struct PushConstants {
        Rml::Vector2f translation;
        float padding[2];
    } pc = {translation, {0.0f, 0.0f}};

    vkCmdPushConstants(currentCmd_, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    // Texture descriptor
    if (texture && texture <= textures_.size()) {
        Texture& tex = textures_[texture - 1];
        VkDescriptorSet descSet = createDescriptorSet(tex.imageView, tex.sampler);
        vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               pipelineLayout_, 0, 1, &descSet, 0, nullptr);
    }

    vkCmdDrawIndexed(currentCmd_, numIndices, 1, 0, 0, 0);

    // Cleanup - в production сохраняйте и переиспользуйте
    vmaDestroyBuffer(allocator_, vertexBuffer, vertexAllocation);
    vmaDestroyBuffer(allocator_, indexBuffer, indexAllocation);
}

void RmlUiVulkanRenderer::EnableScissorRegion(bool enable) {
    scissorEnabled_ = enable;
}

void RmlUiVulkanRenderer::SetScissorRegion(int x, int y, int width, int height) {
    scissorRect_ = {
        .offset = {x, y},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };
}

std::expected<bool, std::errc> RmlUiVulkanRenderer::GenerateTexture(
    Rml::TextureHandle& textureHandle,
    const Rml::byte* source,
    const Rml::Vector2i& sourceDimensions
) {
    Texture texture;
    texture.width = static_cast<uint32_t>(sourceDimensions.x);
    texture.height = static_cast<uint32_t>(sourceDimensions.y);

    // Создание staging buffer
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VkDeviceSize imageSize = texture.width * texture.height * 4;

    VkBufferCreateInfo stagingInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = imageSize,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    };

    VmaAllocationCreateInfo stagingAllocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY
    };

    if (vmaCreateBuffer(allocator_, &stagingInfo, &stagingAllocInfo,
                        &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS) {
        return std::unexpected(std::errc::not_enough_memory);
    }

    // Копирование данных
    void* data;
    vmaMapMemory(allocator_, stagingAllocation, &data);
    std::memcpy(data, source, imageSize);
    vmaUnmapMemory(allocator_, stagingAllocation);

    // Создание image
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {texture.width, texture.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    if (vmaCreateImage(allocator_, &imageInfo, &allocInfo,
                       &texture.image, &texture.allocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);
        return std::unexpected(std::errc::not_enough_memory);
    }

    // Image view
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };

    if (vkCreateImageView(device_, &viewInfo, nullptr, &texture.imageView) != VK_SUCCESS) {
        vmaDestroyImage(allocator_, texture.image, texture.allocation);
        vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);
        return std::unexpected(std::errc::io_error);
    }

    // Sampler
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    };

    if (vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        vkDestroyImageView(device_, texture.imageView, nullptr);
        vmaDestroyImage(allocator_, texture.image, texture.allocation);
        vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);
        return std::unexpected(std::errc::io_error);
    }

    // Copy staging buffer → image (через command buffer)
    // Создаём temporary command buffer для копирования
    VkCommandBuffer copyCmd;
    VkCommandBufferAllocateInfo copyAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = /* создайте отдельный command pool для transfers */,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device_, &copyAllocInfo, &copyCmd);

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(copyCmd, &beginInfo);

    // Transition image to TRANSFER_DST
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_NONE,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = texture.width,
        .bufferImageHeight = texture.height,
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {texture.width, texture.height, 1}
    };
    vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ_ONLY
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(copyCmd);

    // Submit copy
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &copyCmd
    };
    vkQueueSubmit(renderQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderQueue_);

    // Cleanup
    vkFreeCommandBuffers(device_, /* commandPool */, 1, &copyCmd);
    vmaDestroyBuffer(allocator_, stagingBuffer, stagingAllocation);

    textures_.push_back(texture);
    textureHandle = static_cast<Rml::TextureHandle>(textures_.size());

    return true;
}

void RmlUiVulkanRenderer::ReleaseTexture(Rml::TextureHandle textureHandle) {
    if (textureHandle > 0 && textureHandle <= textures_.size()) {
        Texture& tex = textures_[textureHandle - 1];
        if (tex.sampler) vkDestroySampler(device_, tex.sampler, nullptr);
        if (tex.imageView) vkDestroyImageView(device_, tex.imageView, nullptr);
        if (tex.image) vmaDestroyImage(allocator_, tex.image, tex.allocation);
        textures_.erase(textures_.begin() + textureHandle - 1);
    }
}

// Compiled Geometry
Rml::CompiledGeometryHandle RmlUiVulkanRenderer::CompileGeometry(
    Rml::Vertex* vertices, int numVertices,
    int* indices, int numIndices,
    Rml::TextureHandle texture
) {
    Geometry geom;
    geom.texture = texture;
    geom.indexCount = numIndices;

    // Vertex buffer
    VkBufferCreateInfo vbInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = static_cast<VkDeviceSize>(numVertices * sizeof(Rml::Vertex)),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };

    vmaCreateBuffer(allocator_, &vbInfo, &allocInfo,
                    &geom.vertexBuffer, &geom.vertexAllocation, nullptr);

    // Index buffer
    VkBufferCreateInfo ibInfo = vbInfo;
    ibInfo.size = static_cast<VkDeviceSize>(numIndices * sizeof(int));

    vmaCreateBuffer(allocator_, &ibInfo, &allocInfo,
                    &geom.indexBuffer, &geom.indexAllocation, nullptr);

    // Заполнение данными (через staging)
    // ...

    geometries_.push_back(geom);
    return static_cast<Rml::CompiledGeometryHandle>(geometries_.size());
}

void RmlUiVulkanRenderer::RenderCompiledGeometry(
    Rml::CompiledGeometryHandle geometryHandle,
    const Rml::Vector2f& translation
) {
    if (geometryHandle == 0 || geometryHandle > geometries_.size()) return;

    Geometry& geom = geometries_[geometryHandle - 1];

    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &geom.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(currentCmd_, geom.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (scissorEnabled_) {
        vkCmdSetScissor(currentCmd_, 0, 1, &scissorRect_);
    }

    struct PushConstants {
        Rml::Vector2f translation;
        float padding[2];
    } pc = {translation, {0.0f, 0.0f}};

    vkCmdPushConstants(currentCmd_, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    if (geom.texture > 0 && geom.texture <= textures_.size()) {
        Texture& tex = textures_[geom.texture - 1];
        VkDescriptorSet descSet = createDescriptorSet(tex.imageView, tex.sampler);
        vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               pipelineLayout_, 0, 1, &descSet, 0, nullptr);
    }

    vkCmdDrawIndexed(currentCmd_, geom.indexCount, 1, 0, 0, 0);
}

void RmlUiVulkanRenderer::ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometryHandle) {
    if (geometryHandle > 0 && geometryHandle <= geometries_.size()) {
        Geometry& geom = geometries_[geometryHandle - 1];
        vmaDestroyBuffer(allocator_, geom.vertexBuffer, geom.vertexAllocation);
        vmaDestroyBuffer(allocator_, geom.indexBuffer, geom.indexAllocation);
        geometries_.erase(geometries_.begin() + geometryHandle - 1);
    }
}
```

## SDL3 SystemInterface

```cpp
// src/ui/rmlui_sdl_system.hpp
#pragma once

#include <RmlUi/Core/SystemInterface.h>
#include <SDL3/SDL.h>
#include <span>
#include <unordered_map>
#include <string>

class RmlUiSDLSystem final : public Rml::SystemInterface {
public:
    explicit RmlUiSDLSystem(SDL_Window* window) noexcept;
    ~RmlUiSDLSystem() override;

    double GetElapsedTime() override;

    [[nodiscard]] bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

    void SetClipboardText(const Rml::String& text) override;
    void GetClipboardText(Rml::String& text) override;

    // Локализация
    [[nodiscard]] Rml::String TranslateString(const Rml::String& key) override;
    void LoadTranslations(std::string_view language, std::span<const std::string> keys,
                          std::span<const Rml::String> values);
    void SetLanguage(std::string_view language) noexcept;

private:
    SDL_Window* window_ = nullptr;
    std::string currentLanguage_;
    std::unordered_map<std::string, std::unordered_map<std::string, Rml::String>> translations_;
    uint64_t startTime_ = 0;
};
```

```cpp
// src/ui/rmlui_sdl_system.cpp
#include "rmlui_sdl_system.hpp"
#include <print>

RmlUiSDLSystem::RmlUiSDLSystem(SDL_Window* window) noexcept
    : window_(window)
    , startTime_(SDL_GetTicks())
{}

RmlUiSDLSystem::~RmlUiSDLSystem() = default;

double RmlUiSDLSystem::GetElapsedTime() {
    return static_cast<double>(SDL_GetTicks() - startTime_) / 1000.0;
}

bool RmlUiSDLSystem::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;

    switch (type) {
        case Rml::Log::LT_ERROR:   priority = SDL_LOG_PRIORITY_ERROR; break;
        case Rml::Log::LT_WARNING: priority = SDL_LOG_PRIORITY_WARN; break;
        case Rml::Log::LT_INFO:    priority = SDL_LOG_PRIORITY_INFO; break;
        case Rml::Log::LT_DEBUG:   priority = SDL_LOG_PRIORITY_DEBUG; break;
    }

    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[RmlUi] %s", message.c_str());
    return true;
}

void RmlUiSDLSystem::SetClipboardText(const Rml::String& text) {
    SDL_SetClipboardText(text.c_str());
}

void RmlUiSDLSystem::GetClipboardText(Rml::String& text) {
    if (char* clipboard = SDL_GetClipboardText()) {
        text = clipboard;
        SDL_free(clipboard);
    }
}

Rml::String RmlUiSDLSystem::TranslateString(const Rml::String& key) {
    auto langIt = translations_.find(currentLanguage_);
    if (langIt == translations_.end()) {
        return key;
    }

    auto transIt = langIt->second.find(key.c_str());
    if (transIt == langIt->second.end()) {
        return key;
    }

    return transIt->second;
}

void RmlUiSDLSystem::LoadTranslations(std::string_view language,
                                     std::span<const std::string> keys,
                                     std::span<const Rml::String> values) {
    auto& langMap = translations_[std::string(language)];

    for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
        langMap[keys[i]] = values[i];
    }
}

void RmlUiSDLSystem::SetLanguage(std::string_view language) noexcept {
    currentLanguage_ = language;
}
```

## ECS интеграция

### Компоненты

```cpp
// src/ecs/ui_components.hpp
#pragma once

#include <flecs.h>
#include <string>
#include <vector>

struct UIElement {
    std::string elementId;
    std::string documentPath;
    bool visible = true;
};

struct UIHealthBar {
    float current = 100.0f;
    float maximum = 100.0f;
};

struct UIInventory {
    std::vector<std::string> items;
    int selectedSlot = 0;
};

struct UICrosshair {
    bool visible = true;
};
```

### Система синхронизации

```cpp
// src/ui/ui_sync_system.hpp
#pragma once

#include <flecs.h>
#include <RmlUi/Core.h>
#include <memory>
#include <expected>

class UISyncSystem {
public:
    UISyncSystem(flecs::world& world, Rml::Context* context) noexcept;
    ~UISyncSystem();

    void update(float deltaTime);

    // Data model management
    [[nodiscard]] std::expected<Rml::DataModelHandle, std::errc> createModel(
        std::string_view name
    ) noexcept;

    template<typename T>
    requires std::is_aggregate_v<T>
    void bindData(std::string_view modelName, std::string_view varName, T* data);

private:
    flecs::world& world_;
    Rml::Context* context_ = nullptr;

    struct ModelState {
        Rml::DataModelHandle handle;
        std::vector<std::string> dirtyVars;
    };

    std::unordered_map<std::string, ModelState> models_;

    void syncPlayerData();
    void syncInventoryData();
    void syncGameState();
};
```

```cpp
// src/ui/ui_sync_system.cpp
#include "ui_sync_system.hpp"
#include <print>

UISyncSystem::UISyncSystem(flecs::world& world, Rml::Context* context) noexcept
    : world_(world)
    , context_(context)
{}

UISyncSystem::~UISyncSystem() = default;

std::expected<Rml::DataModelHandle, std::errc> UISyncSystem::createModel(
    std::string_view name
) noexcept {
    if (!context_) {
        return std::unexpected(std::errc::invalid_argument);
    }

    Rml::DataModelConstructor model = context_->CreateDataModel(std::string(name).c_str());
    if (!model) {
        return std::unexpected(std::errc::address_in_use);
    }

    models_[std::string(name)] = {model, {}};
    return model;
}

void UISyncSystem::update(float deltaTime) {
    if (!context_) return;

    syncPlayerData();
    syncInventoryData();
    syncGameState();

    context_->Update();
}

void UISyncSystem::syncPlayerData() {
    auto it = models_.find("player");
    if (it == models_.end()) return;

    world_.each([&](flecs::entity e, const struct Player& player) {
        auto& model = it->second.handle;

        // Health
        model.DirtyVariable("health");
        model.DirtyVariable("max_health");

        // Stamina
        model.DirtyVariable("stamina");
    });
}

void UISyncSystem::syncInventoryData() {
    auto it = models_.find("inventory");
    if (it == models_.end()) return;

    world_.each([&](flecs::entity e, const struct UIInventory& inventory) {
        auto& model = it->second.handle;

        // Selected slot
        model.DirtyVariable("selected_slot");
        model.SetVariable("selected_slot", &inventory.selectedSlot);

        // Items count
        model.DirtyVariable("item_count");
        int32_t count = static_cast<int32_t>(inventory.items.size());
        model.SetVariable("item_count", &count);
    });
}

void UISyncSystem::syncGameState() {
    auto it = models_.find("game_state");
    if (it == models_.end()) return;

    world_.each([&](flecs::entity e, const struct GameState& state) {
        auto& model = it->second.handle;

        // Game time
        model.DirtyVariable("game_time");
        model.SetVariable("game_time", &state.time);

        // Pause state
        model.DirtyVariable("paused");
        model.SetVariable("paused", &state.paused);

        // Score
        model.DirtyVariable("score");
        model.SetVariable("score", &state.score);
    });
}
```

## Игровой цикл

```cpp
// src/game.hpp
#pragma once

#include <flecs.h>
#include <RmlUi/Core.h>
#include <memory>
#include "ui/rmlui_vulkan_renderer.hpp"
#include "ui/rmlui_sdl_system.hpp"
#include "ui/ui_sync_system.hpp"

class Game {
public:
    Game() noexcept;
    ~Game();

    void init();
    void run();

private:
    void processEvents();
    void update(float deltaTime);
    void render();

    // SDL
    SDL_Window* window_ = nullptr;
    SDL_Event event_;

    // Vulkan
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer_ = VK_NULL_HANDLE;

    // RmlUi
    std::unique_ptr<RmlUiVulkanRenderer> uiRenderer_;
    std::unique_ptr<RmlUiSDLSystem> uiSystem_;
    Rml::Context* rmlContext_ = nullptr;

    // ECS
    flecs::world world_;
    std::unique_ptr<UISyncSystem> uiSync_;

    bool running_ = false;
};
```

```cpp
// src/game.cpp
#include "game.hpp"
#include <print>

void Game::processEvents() {
    while (SDL_PollEvent(&event_)) {
        // Обработка ввода RmlUi
        switch (event_.type) {
            case SDL_EVENT_MOUSE_MOTION:
                rmlContext_->ProcessMouseMove(
                    static_cast<int>(event_.motion.x),
                    static_cast<int>(event_.motion.y),
                    getKeyModifiers()
                );
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                rmlContext_->ProcessMouseButtonDown(
                    getRmlMouseButton(event_.button.button)
                );
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                rmlContext_->ProcessMouseButtonUp(
                    getRmlMouseButton(event_.button.button)
                );
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                rmlContext_->ProcessMouseWheel(
                    event_.wheel.y,
                    getKeyModifiers()
                );
                break;

            case SDL_EVENT_KEY_DOWN:
                rmlContext_->ProcessKeyDown(
                    getRmlKeyCode(event_.key.keysym.sym),
                    getKeyModifiers()
                );
                break;

            case SDL_EVENT_KEY_UP:
                rmlContext_->ProcessKeyUp(
                    getRmlKeyCode(event_.key.keysym.sym),
                    getKeyModifiers()
                );
                break;

            case SDL_EVENT_TEXT_INPUT:
                rmlContext_->ProcessTextInput(event_.text.text);
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                rmlContext_->SetDimensions({
                    static_cast<int>(event_.window.data1),
                    static_cast<int>(event_.window.data2)
                });
                break;

            case SDL_EVENT_QUIT:
                running_ = false;
                break;
        }
    }
}

void Game::update(float deltaTime) {
    // ECS update
    world_.progress(deltaTime);

    // UI sync
    uiSync_->update(deltaTime);
}

void Game::render() {
    // Begin frame
    uiRenderer_->beginFrame(commandBuffer_, viewportExtent_);

    // Render 3D scene
    renderScene();

    // Render UI
    rmlContext_->Render();

    uiRenderer_->endFrame();
}

void Game::run() {
    running_ = true;
    while (running_) {
        float deltaTime = calculateDeltaTime();

        processEvents();
        update(deltaTime);
        render();

        present();
    }
}
```

## Порядок инициализации

```cpp
void Game::init() {
    // 1. SDL
    SDL_Init(SDL_INIT_VIDEO);
    window_ = SDL_CreateWindow("ProjectV", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_DPI);

    // 2. Vulkan
    initVulkan(); // Ваша функция

    // 3. RmlUi renderer
    uiRenderer_ = std::make_unique<RmlUiVulkanRenderer>(
        device_, physicalDevice_, graphicsQueue_, queueFamilyIndex_,
        allocator_, renderPass_, msaaSamples_
    );

    // 4. RmlUi system
    uiSystem_ = std::make_unique<RmlUiSDLSystem>(window_);

    // 5. Установка интерфейсов
    Rml::SetRenderInterface(uiRenderer_.get());
    Rml::SetSystemInterface(uiSystem_.get());

    // 6. Инициализация RmlUi
    Rml::Initialise();

    // 7. Контекст
    rmlContext_ = Rml::CreateContext("main", {1280, 720});
    if (!rmlContext_) {
        std::println(stderr, "Failed to create RmlUi context");
        return;
    }

    // 8. Загрузка шрифтов
    Rml::LoadFontFace("assets/fonts/Roboto-Regular.ttf");

    // 9. UI sync system
    uiSync_ = std::make_unique<UISyncSystem>(world_, rmlContext_);

    // 10. Загрузка документов
    Rml::ElementDocument* hud = rmlContext_->LoadDocument("ui/hud.rml");
    if (hud) hud->Show();

    Rml::ElementDocument* menu = rmlContext_->LoadDocument("ui/main_menu.rml");
    if (menu) menu->Show();
}
```

## Очистка

```cpp
Game::~Game() {
    // Удаление документов
    if (rmlContext_) {
        // Documents are automatically cleaned up with context
        Rml::RemoveContext("main");
    }

    // Shutdown RmlUi
    Rml::Shutdown();

    // Очистка Vulkan
    destroyVulkan();

    // Очистка SDL
    SDL_DestroyWindow(window_);
    SDL_Quit();
}
```

## Рекомендации

| Аспект            | Рекомендация                           |
|-------------------|----------------------------------------|
| **Contexts**      | HUD и меню — разные contexts           |
| **Data Bindings** | Используйте для синхронизации ECS ↔ UI |
| **Textures**      | Переиспользуйте через CompileGeometry  |
| **Events**        | Обрабатывайте до ECS update            |
| **Render**        | Рендерите после 3D сцены               |
| **Fonts**         | Загружайте только нужные начертания    |
