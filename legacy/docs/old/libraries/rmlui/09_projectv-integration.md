# Интеграция RmlUi с ProjectV

🔴 **Уровень 3: Продвинутый**

Интеграция RmlUi с Vulkan рендерером, SDL3, VMA и flecs ECS в ProjectV.

## Обзор интеграции

ProjectV использует RmlUi для игрового HUD (не debug UI). ImGui используется для debug инструментов.

| Компонент  | Назначение       | Документация                         |
|------------|------------------|--------------------------------------|
| **Vulkan** | Графический API  | [docs/libraries/vulkan/](../vulkan/) |
| **SDL3**   | Окна, ввод       | [docs/libraries/sdl/](../sdl/)       |
| **VMA**    | Выделение памяти | [docs/libraries/vma/](../vma/)       |
| **flecs**  | ECS              | [docs/libraries/flecs/](../flecs/)   |
| **ImGui**  | Debug UI         | [docs/libraries/imgui/](../imgui/)   |

## Архитектура

```
┌─────────────────────────────────────────────────────────┐
│                    Game Loop                             │
├─────────────────────────────────────────────────────────┤
│  SDL Events → RmlUi Context (ProcessMouseMove/Key)      │
│  flecs::world::progress() → Update Game State            │
│  Data Bindings → Sync ECS → UI                           │
├─────────────────────────────────────────────────────────┤
│  Vulkan Render Pass:                                     │
│    1. Game Scene (3D voxels)                             │
│    2. ImGui Debug UI (if enabled)                        │
│    3. RmlUi HUD (game UI)                                │
└─────────────────────────────────────────────────────────┘
```

## Vulkan RenderInterface

### Заголовок

```cpp
// src/ui/rmlui_vulkan_renderer.hpp
#pragma once

#include <RmlUi/Core/RenderInterface.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

class RmlUiVulkanRenderer : public Rml::RenderInterface {
public:
    RmlUiVulkanRenderer(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkQueue queue,
        uint32_t queueFamilyIndex,
        VmaAllocator allocator,
        VkRenderPass renderPass,
        VkSampleCountFlagBits samples
    );

    ~RmlUiVulkanRenderer() override;

    // --- Обязательные методы ---
    void RenderGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation
    ) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(int x, int y, int width, int height) override;

    // --- Текстуры ---
    bool LoadTexture(
        Rml::TextureHandle& textureHandle,
        Rml::Vector2i& textureDimensions,
        const Rml::String& source
    ) override;

    bool GenerateTexture(
        Rml::TextureHandle& textureHandle,
        const Rml::byte* source,
        const Rml::Vector2i& sourceDimensions
    ) override;

    void ReleaseTexture(Rml::TextureHandle textureHandle) override;

    // --- Compiled Geometry (оптимизация) ---
    CompiledGeometryHandle CompileGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture
    ) override;

    void RenderCompiledGeometry(
        CompiledGeometryHandle geometry,
        const Rml::Vector2f& translation
    ) override;

    void ReleaseCompiledGeometry(CompiledGeometryHandle geometry) override;

    // --- Frame management ---
    void beginFrame(VkCommandBuffer cmd, VkExtent2D extent);
    void endFrame();

private:
    struct Texture {
        VkImage image;
        VkImageView imageView;
        VmaAllocation allocation;
        VkSampler sampler;
        VkExtent2D extent;
    };

    struct Geometry {
        VkBuffer vertexBuffer;
        VkBuffer indexBuffer;
        VmaAllocation vertexAllocation;
        VmaAllocation indexAllocation;
        int indexCount;
        Rml::TextureHandle texture;
    };

    VkDevice device_;
    VmaAllocator allocator_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkDescriptorSetLayout descriptorSetLayout_;
    VkDescriptorPool descriptorPool_;

    VkCommandBuffer currentCmd_;
    VkExtent2D currentExtent_;

    std::vector<Texture> textures_;
    std::vector<Geometry> geometries_;

    bool scissorEnabled_ = false;
    VkRect2D scissorRect_;

    bool createPipeline(VkRenderPass renderPass, VkSampleCountFlagBits samples);
    VkDescriptorSet createDescriptorSet(VkImageView textureView, VkSampler sampler);
};
```

### Реализация (ключевые методы)

```cpp
// src/ui/rmlui_vulkan_renderer.cpp

void RmlUiVulkanRenderer::RenderGeometry(
    Rml::Vertex* vertices, int numVertices,
    int* indices, int numIndices,
    Rml::TextureHandle texture,
    const Rml::Vector2f& translation
) {
    // Создание temporary buffers для одного вызова
    // В продакшене используйте CompileGeometry

    VkBuffer vertexBuffer, indexBuffer;
    VmaAllocation vertexAlloc, indexAlloc;

    // Vertex buffer
    VkBufferCreateInfo vbInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = numVertices * sizeof(Rml::Vertex),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo vbAllocInfo = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    vmaCreateBuffer(allocator_, &vbInfo, &vbAllocInfo,
                    &vertexBuffer, &vertexAlloc, nullptr);

    // Копирование данных
    void* data;
    vmaMapMemory(allocator_, vertexAlloc, &data);
    std::memcpy(data, vertices, numVertices * sizeof(Rml::Vertex));
    vmaUnmapMemory(allocator_, vertexAlloc);

    // Аналогично для index buffer...

    // Отрисовка
    vkCmdBindPipeline(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(currentCmd_, 0, 1, &vertexBuffer, &offset);
    vkCmdBindIndexBuffer(currentCmd_, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Scissor
    if (scissorEnabled_) {
        vkCmdSetScissor(currentCmd_, 0, 1, &scissorRect_);
    }

    // Push constants для translation
    vkCmdPushConstants(currentCmd_, pipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(translation), &translation);

    // Texture descriptor set
    if (texture) {
        Texture& tex = textures_[texture - 1];
        VkDescriptorSet descSet = createDescriptorSet(tex.imageView, tex.sampler);
        vkCmdBindDescriptorSets(currentCmd_, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout_, 0, 1, &descSet, 0, nullptr);
    }

    vkCmdDrawIndexed(currentCmd_, numIndices, 1, 0, 0, 0);

    // Cleanup temporary buffers (в реальном коде используйте staging pool)
    vmaDestroyBuffer(allocator_, vertexBuffer, vertexAlloc);
    vmaDestroyBuffer(allocator_, indexBuffer, indexAlloc);
}

void RmlUiVulkanRenderer::SetScissorRegion(int x, int y, int width, int height) {
    // RmlUi использует top-left origin, Vulkan тоже
    scissorRect_ = {
        .offset = {x, y},
        .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
    };
}

bool RmlUiVulkanRenderer::GenerateTexture(
    Rml::TextureHandle& textureHandle,
    const Rml::byte* source,
    const Rml::Vector2i& sourceDimensions
) {
    Texture texture;

    // Создание image
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = {
            static_cast<uint32_t>(sourceDimensions.x),
            static_cast<uint32_t>(sourceDimensions.y),
            1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    vmaCreateImage(allocator_, &imageInfo, &allocInfo,
                   &texture.image, &texture.allocation, nullptr);

    // Создание image view
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCreateImageView(device_, &viewInfo, nullptr, &texture.imageView);

    // Создание sampler
    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };

    vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler);

    // Upload texture data (требует command buffer и staging buffer)
    // ... staging buffer code ...

    texture.extent = {
        static_cast<uint32_t>(sourceDimensions.x),
        static_cast<uint32_t>(sourceDimensions.y)
    };

    textures_.push_back(texture);
    textureHandle = static_cast<Rml::TextureHandle>(textures_.size());

    return true;
}
```

## SDL3 SystemInterface

```cpp
// src/ui/rmlui_sdl_system.hpp
#pragma once

#include <RmlUi/Core/SystemInterface.h>
#include <SDL3/SDL.h>
#include <unordered_map>
#include <string>

class RmlUiSDLSystem : public Rml::SystemInterface {
public:
    explicit RmlUiSDLSystem(SDL_Window* window);
    ~RmlUiSDLSystem() override;

    double GetElapsedTime() override;

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

    void SetClipboardText(const Rml::String& text) override;
    void GetClipboardText(Rml::String& text) override;

    // Локализация
    Rml::String TranslateString(const Rml::String& key) override;
    void LoadTranslations(const std::string& language, const std::string& path);
    void SetLanguage(const std::string& language);

private:
    SDL_Window* window_;
    std::string currentLanguage_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> translations_;
    Uint64 startTime_;
};
```

```cpp
// src/ui/rmlui_sdl_system.cpp

double RmlUiSDLSystem::GetElapsedTime() {
    return static_cast<double>(SDL_GetTicks()) / 1000.0;
}

bool RmlUiSDLSystem::LogMessage(Rml::Log::Type type, const Rml::String& message) {
    SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;

    switch (type) {
        case Rml::Log::LT_ERROR:
            priority = SDL_LOG_PRIORITY_ERROR;
            break;
        case Rml::Log::LT_WARNING:
            priority = SDL_LOG_PRIORITY_WARN;
            break;
        case Rml::Log::LT_INFO:
            priority = SDL_LOG_PRIORITY_INFO;
            break;
        case Rml::Log::LT_DEBUG:
            priority = SDL_LOG_PRIORITY_DEBUG;
            break;
    }

    SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority,
                    "[RmlUi] %s", message.c_str());

    return true;
}

void RmlUiSDLSystem::SetClipboardText(const Rml::String& text) {
    SDL_SetClipboardText(text.c_str());
}

void RmlUiSDLSystem::GetClipboardText(Rml::String& text) {
    char* clipboard = SDL_GetClipboardText();
    if (clipboard) {
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

    return Rml::String(transIt->second.c_str());
}
```

## ECS Integration

### Компоненты

```cpp
// src/ui/ui_components.hpp
#pragma once

#include <flecs.h>

struct UIElement {
    std::string elementId;
    std::string dataModel;
};

struct UIHealthBar {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
};

struct UIInventory {
    std::vector<std::string> items;
    int selectedIndex = 0;
};

struct UICrosshair {
    bool visible = true;
    glm::vec2 position{0.0f, 0.0f};
};
```

### Система UI

```cpp
// src/ui/ui_system.hpp
#pragma once

#include <flecs.h>
#include <RmlUi/Core.h>

class UISystem {
public:
    UISystem(flecs::world& world, Rml::Context* context);
    ~UISystem();

    void update(float deltaTime);
    void render(VkCommandBuffer cmd);

private:
    flecs::world& world_;
    Rml::Context* context_;
    Rml::ElementDocument* hudDocument_;

    // Data models
    Rml::DataModelHandle playerModel_;
    Rml::DataModelHandle inventoryModel_;

    // Привязанные данные
    struct PlayerData {
        int health = 100;
        int maxHealth = 100;
        float stamina = 1.0f;
        std::string name = "Player";
    } playerData_;

    void setupDataModels();
    void syncFromECS();
};
```

```cpp
// src/ui/ui_system.cpp

void UISystem::setupDataModels() {
    // Player data model
    if (auto model = context_->CreateDataModel("player")) {
        model.Bind("health", &playerData_.health);
        model.Bind("max_health", &playerData_.maxHealth);
        model.Bind("stamina", &playerData_.stamina);
        model.Bind("name", &playerData_.name);

        playerModel_ = model;
    }

    // Inventory model
    if (auto model = context_->CreateDataModel("inventory")) {
        // Регистрация Item типа
        if (auto itemModel = model.RegisterStruct<InventoryItem>()) {
            itemModel.RegisterMember("name", &InventoryItem::name);
            itemModel.RegisterMember("count", &InventoryItem::count);
            itemModel.RegisterMember("icon", &InventoryItem::icon);
        }

        model.Bind("items", &inventoryData_.items);
        model.Bind("selected_index", &inventoryData_.selectedIndex);

        // Событие использования предмета
        model.BindEventFunc("use_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
                if (!args.empty()) {
                    int index = args[0].Get<int>();
                    useInventoryItem(index);
                }
            });

        inventoryModel_ = model;
    }
}

void UISystem::syncFromECS() {
    // Получение данных из ECS
    auto& registry = world_;

    // Player health
    registry.each([&](flecs::entity e, const PlayerComponent& player) {
        if (playerData_.health != static_cast<int>(player.health)) {
            playerData_.health = static_cast<int>(player.health);
            playerModel_.DirtyVariable("health");
        }
        if (playerData_.stamina != player.stamina) {
            playerData_.stamina = player.stamina;
            playerModel_.DirtyVariable("stamina");
        }
    });

    // Inventory
    // ... sync inventory items
}

void UISystem::update(float deltaTime) {
    syncFromECS();
    context_->Update();
}

void UISystem::render(VkCommandBuffer cmd) {
    context_->Render();
}
```

## Интеграция в Main Loop

```cpp
// src/game.cpp

void Game::run() {
    while (running_) {
        float deltaTime = timer_.deltaTime();

        // 1. Обработка SDL событий
        processEvents();

        // 2. Обновление ECS
        world_.progress(deltaTime);

        // 3. Обновление UI (синхронизация с ECS)
        uiSystem_.update(deltaTime);

        // 4. Рендеринг
        beginFrame();

        //    a) Сцена игры
        renderScene();

        //    b) Debug UI (ImGui) - только в debug builds
        #ifndef NDEBUG
        debugUI_.render();
        #endif

        //    c) Game HUD (RmlUi)
        uiSystem_.render(currentCommandBuffer_);

        endFrame();
    }
}

void Game::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // ImGui first (если активен)
        #ifndef NDEBUG
        if (debugUI_.handleEvent(event)) {
            continue;  // ImGui поглотил событие
        }
        #endif

        // RmlUi
        switch (event.type) {
            case SDL_EVENT_MOUSE_MOTION:
                rmlContext_->ProcessMouseMove(
                    static_cast<int>(event.motion.x),
                    static_cast<int>(event.motion.y),
                    getSDLKeyModifiers()
                );
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                rmlContext_->ProcessMouseButtonDown(
                    getRmlMouseButton(event.button.button)
                );
                break;

            case SDL_EVENT_MOUSE_BUTTON_UP:
                rmlContext_->ProcessMouseButtonUp(
                    getRmlMouseButton(event.button.button)
                );
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                rmlContext_->ProcessMouseWheel(
                    event.wheel.y,
                    getSDLKeyModifiers()
                );
                break;

            case SDL_EVENT_KEY_DOWN:
                rmlContext_->ProcessKeyDown(
                    getRmlKeyCode(event.key.keysym),
                    getSDLKeyModifiers()
                );
                break;

            case SDL_EVENT_KEY_UP:
                rmlContext_->ProcessKeyUp(
                    getRmlKeyCode(event.key.keysym),
                    getSDLKeyModifiers()
                );
                break;

            case SDL_EVENT_TEXT_INPUT:
                rmlContext_->ProcessTextInput(event.text.text);
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                rmlContext_->SetDimensions({
                    static_cast<int>(event.window.data1),
                    static_cast<int>(event.window.data2)
                });
                break;
        }

        // Game-specific event handling
        handleGameEvent(event);
    }
}
```

## Порядок рендеринга

```cpp
void Game::beginFrame() {
    // Begin command buffer
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Begin render pass
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // Full screen scissor
    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Game::renderScene() {
    // 3D scene rendering
    // ...
}

void Game::endFrame() {
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    // Submit and present
    // ...
}
