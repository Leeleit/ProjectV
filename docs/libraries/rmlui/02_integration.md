## Интеграция RmlUi

<!-- anchor: 03_integration -->


Интеграция RmlUi в проект: CMake, зависимости, backends, реализация интерфейсов.

## CMake

### Добавление через FetchContent

```cmake
Include(FetchContent)

FetchContent_Declare(
    rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG master
)

FetchContent_MakeAvailable(rmlui)

target_link_libraries(your_target PRIVATE RmlUi)
```

### Добавление как подмодуль

```cmake
add_subdirectory(external/rmlui)

target_link_libraries(your_target PRIVATE RmlUi)
```

### Опции CMake

| Опция           | По умолчанию | Описание                                             |
|-----------------|--------------|------------------------------------------------------|
| `RMLUI_BACKEND` | (пусто)      | Backend для samples: `SDL_GL3`, `SDL_VK`, `GLFW_GL3` |
| `RMLUI_SAMPLES` | `OFF`        | Сборка примеров                                      |
| `RMLUI_TESTS`   | `OFF`        | Сборка тестов                                        |
| `RMLUI_LUA`     | `OFF`        | Lua scripting plugin                                 |
| `RMLUI_SVG`     | `OFF`        | SVG plugin                                           |
| `RMLUI_LOTTIE`  | `OFF`        | Lottie animations plugin                             |

### Пример конфигурации

```cmake
set(RMLUI_BACKEND SDL_GL3 CACHE STRING "")
set(RMLUI_SAMPLES ON CACHE BOOL "")

add_subdirectory(external/rmlui)
```

## Зависимости

### Обязательные

- **FreeType** — рендеринг шрифтов (подключается автоматически)

### Опциональные

| Библиотека             | Назначение        | Опция CMake       |
|------------------------|-------------------|-------------------|
| **LuaJIT/Lua 5.1-5.4** | Lua scripting     | `RMLUI_LUA=ON`    |
| **lunasvg**            | SVG рендеринг     | `RMLUI_SVG=ON`    |
| **rlottie**            | Lottie animations | `RMLUI_LOTTIE=ON` |
| **HarfBuzz**           | Text shaping      | Автоопределение   |

### Для backends (samples)

| Backend    | Зависимости        |
|------------|--------------------|
| `SDL_GL3`  | SDL2 или SDL3      |
| `SDL_VK`   | SDL2/SDL3 + Vulkan |
| `GLFW_GL3` | GLFW               |
| `GLFW_VK`  | GLFW + Vulkan      |

## RenderInterface

`RenderInterface` — абстрактный интерфейс для отрисовки геометрии.

```cpp
class RenderInterface : public Rml::RenderInterface {
public:
    RenderInterface(VkDevice device, VmaAllocator allocator);
    ~RenderInterface() override;

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

    // --- Опциональные: Transforms ---

    void PushTransform(const Rml::Matrix4f& transform) override;
    void PopTransform() override;

private:
    VkDevice device_;
    VmaAllocator allocator_;
    std::vector<Texture> textures_;
};
```

### Пример: OpenGL 3 RenderInterface

```cpp
class GL3RenderInterface : public Rml::RenderInterface {
public:
    void RenderGeometry(
        Rml::Vertex* vertices, int numVertices,
        int* indices, int numIndices,
        Rml::TextureHandle texture,
        const Rml::Vector2f& translation
    ) override {
        // Установка шейдера
        glUseProgram(program_);

        // Установка uniform
        glUniform2f(translation_loc_, translation.x, translation.y);

        // VAO/VBO
        glBindVertexArray(vao_);

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, numVertices * sizeof(Rml::Vertex),
                     vertices, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof(int),
                     indices, GL_DYNAMIC_DRAW);

        // Текстура
        if (texture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        }

        // Отрисовка
        glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, nullptr);
    }

    void EnableScissorRegion(bool enable) override {
        if (enable)
            glEnable(GL_SCISSOR_TEST);
        else
            glDisable(GL_SCISSOR_TEST);
    }

    void SetScissorRegion(int x, int y, int width, int height) override {
        glScissor(x, viewportHeight_ - y - height, width, height);
    }

    bool GenerateTexture(
        Rml::TextureHandle& textureHandle,
        const Rml::byte* source,
        const Rml::Vector2i& dimensions
    ) override {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     dimensions.x, dimensions.y, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, source);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        textureHandle = static_cast<Rml::TextureHandle>(texture);
        return true;
    }

    void ReleaseTexture(Rml::TextureHandle textureHandle) override {
        GLuint texture = static_cast<GLuint>(textureHandle);
        glDeleteTextures(1, &texture);
    }
};
```

## SystemInterface

`SystemInterface` — интерфейс системных функций.

```cpp
class SystemInterface : public Rml::SystemInterface {
public:
    // Время в секундах
    double GetElapsedTime() override {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    // Логирование
    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        switch (type) {
            case Rml::Log::LT_ERROR:
                std::fprintf(stderr, "[RmlUi Error] %s\n", message.c_str());
                return true;
            case Rml::Log::LT_WARNING:
                std::fprintf(stderr, "[RmlUi Warning] %s\n", message.c_str());
                return true;
            case Rml::Log::LT_INFO:
                std::printf("[RmlUi Info] %s\n", message.c_str());
                return true;
            default:
                return false;
        }
    }

    // Clipboard
    void SetClipboardText(const Rml::String& text) override {
        SDL_SetClipboardText(text.c_str());
    }

    void GetClipboardText(Rml::String& text) override {
        char* clipboard = SDL_GetClipboardText();
        if (clipboard) {
            text = clipboard;
            SDL_free(clipboard);
        }
    }

    // Локализация
    Rml::String TranslateString(const Rml::String& key) override {
        // Возврат перевода или оригинала
        return translationMap_.value(key, key);
    }
};
```

## FileInterface

`FileInterface` — интерфейс для загрузки файлов.

```cpp
class FileInterface : public Rml::FileInterface {
public:
    Rml::FileHandle Open(const Rml::String& path) override {
        FILE* file = std::fopen(path.c_str(), "rb");
        return reinterpret_cast<Rml::FileHandle>(file);
    }

    void Close(Rml::FileHandle file) override {
        std::fclose(reinterpret_cast<FILE*>(file));
    }

    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override {
        return std::fread(buffer, 1, size, reinterpret_cast<FILE*>(file));
    }

    bool Seek(Rml::FileHandle file, long offset, int origin) override {
        return std::fseek(reinterpret_cast<FILE*>(file), offset, origin) == 0;
    }

    size_t Tell(Rml::FileHandle file) override {
        return std::ftell(reinterpret_cast<FILE*>(file));
    }

    size_t Length(Rml::FileHandle file) override {
        FILE* f = reinterpret_cast<FILE*>(file);
        long pos = std::ftell(f);
        std::fseek(f, 0, SEEK_END);
        long len = std::ftell(f);
        std::fseek(f, pos, SEEK_SET);
        return len;
    }
};
```

## Использование готовых backends

RmlUi включает готовые backends в папке `Backends/`.

### Структура backend

```
Backends/
├── RmlUi_Backend.h              # Интерфейс backend
├── RmlUi_Platform_SDL.h/cpp     # Platform backend для SDL
├── RmlUi_Renderer_VK.h/cpp      # Renderer backend для Vulkan
└── RmlUi_Renderer_GL3.h/cpp     # Renderer backend для OpenGL 3
```

### SDL + Vulkan backend

```cpp
#include "Backends/RmlUi_Platform_SDL.h"
#include "Backends/RmlUi_Renderer_VK.h"

int main() {
    // Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("RmlUi", 1280, 720, SDL_WINDOW_VULKAN);

    // Инициализация Vulkan
    VkInstance instance = createVulkanInstance();
    VkDevice device = createVulkanDevice();
    // ...

    // Создание RmlUi backends
    RmlUi_Vulkan_Renderer renderer(device, physicalDevice, queue, commandPool);
    RmlUi_SDL_Platform platform(window);

    // Установка интерфейсов
    Rml::SetRenderInterface(&renderer);
    Rml::SetSystemInterface(&platform);

    // Инициализация RmlUi
    Rml::Initialise();

    // Создание контекста
    Rml::Context* context = Rml::CreateContext("main", {1280, 720});

    // ... main loop

    Rml::Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
}
```

## Порядок инициализации

```cpp
// 1. Создание окна и графического API
SDL_Window* window = SDL_CreateWindow(...);
initVulkan();

// 2. Создание интерфейсов (на стеке или в куче)
MyRenderInterface renderInterface(device, allocator);
MySystemInterface systemInterface;

// 3. Установка интерфейсов ДО Initialise
Rml::SetRenderInterface(&renderInterface);
Rml::SetSystemInterface(&systemInterface);

// 4. Инициализация RmlUi
Rml::Initialise();

// 5. Создание контекста
Rml::Context* context = Rml::CreateContext("main", {width, height});

// 6. Загрузка шрифтов (обязательно!)
Rml::LoadFontFace("fonts/Roboto-Regular.ttf");

// 7. Загрузка документов
Rml::ElementDocument* doc = context->LoadDocument("ui/main.rml");
doc->Show();
```

## Порядок очистки

```cpp
// 1. Удалить все контексты (или RemoveContext)
// Документы удаляются автоматически с контекстом

// 2. Shutdown
Rml::Shutdown();

// 3. Освободить ресурсы интерфейсов
// renderInterface.cleanup();

// 4. Уничтожить графический API и окно
destroyVulkan();
SDL_DestroyWindow(window);
SDL_Quit();
```

## Важно: Lifetime интерфейсов

Интерфейсы должны существовать до вызова `Rml::Shutdown()`:

```cpp
// ОШИБКА: интерфейс уничтожается до Shutdown
void initRmlUi() {
    MyRenderInterface renderInterface;  // Локальная переменная!
    Rml::SetRenderInterface(&renderInterface);
    Rml::Initialise();
    // renderInterface уничтожается при выходе из функции
}

// ПРАВИЛЬНО: интерфейс живёт достаточно долго
class UIManager {
    MyRenderInterface renderInterface;
    Rml::Context* context = nullptr;

    void init() {
        Rml::SetRenderInterface(&renderInterface);
        Rml::Initialise();
        context = Rml::CreateContext("main", {1920, 1080});
    }

    ~UIManager() {
        Rml::Shutdown();  // renderInterface ещё жив
    }
};

---

## Интеграция RmlUi с ProjectV

<!-- anchor: 09_projectv-integration -->


Интеграция RmlUi с Vulkan рендерером, SDL3, VMA и flecs ECS в ProjectV.

## Обзор интеграции

ProjectV использует RmlUi для игрового HUD (не debug UI). ImGui используется для debug инструментов.

| Компонент | Назначение | Документация |
|-----------|------------|--------------|
| **Vulkan** | Графический API | [docs/libraries/vulkan/](../vulkan/) |
| **SDL3** | Окна, ввод | [docs/libraries/sdl/](../sdl/) |
| **VMA** | Выделение памяти | [docs/libraries/vma/](../vma/) |
| **flecs** | ECS | [docs/libraries/flecs/](../flecs/) |
| **ImGui** | Debug UI | [docs/libraries/imgui/](../imgui/) |

## Архитектура

```

┌─────────────────────────────────────────────────────────┐
│ Game Loop │
├─────────────────────────────────────────────────────────┤
│ SDL Events → RmlUi Context (ProcessMouseMove/Key)      │
│ flecs::world::progress() → Update Game State │
│ Data Bindings → Sync ECS → UI │
├─────────────────────────────────────────────────────────┤
│ Vulkan Render Pass:                                     │
│ 1. Game Scene (3D voxels)                             │
│ 2. ImGui Debug UI (if enabled)                        │
│ 3. RmlUi HUD (game UI)                                │
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

---

## Паттерны RmlUi для ProjectV

<!-- anchor: 10_projectv-patterns -->


Паттерны HUD для воксельного движка: health bar, inventory, меню, data bindings с ECS.

## HUD для воксельного движка

### Структура документов

```

ui/
├── hud/
│ ├── health_bar.rml
│ ├── crosshair.rml
│ ├── hotbar.rml
│ └── minimap.rml
├── menus/
│ ├── main_menu.rml
│ ├── pause_menu.rml
│ ├── inventory.rml
│ └── settings.rml
├── styles/
│ ├── hud.rcss
│ ├── menus.rcss
│ └── common.rcss
└── sprites/
├── icons.png
└── buttons.png

```

### Health Bar

**hud/health_bar.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="player">
    <div id="health-container">
        <div id="health-icon"></div>
        <div id="health-bar-wrapper">
            <div id="health-bar-bg">
                <div id="health-bar-fill" style="width: {{health_percent}}%;"></div>
            </div>
            <span id="health-text">{{health}} / {{max_health}}</span>
        </div>
    </div>

    <div id="stamina-container">
        <div id="stamina-bar" style="width: {{stamina_percent}}%;"></div>
    </div>
</body>
</rml>
```

**styles/hud.rcss:**

```
@import url("common.rcss");

@sprite_sheet hud_icons {
    src: "../sprites/icons.png";
    resolution: 2dp;
    health: 0px 0px 32px 32px;
    stamina: 32px 0px 32px 32px;
}

#health-container {
    position: absolute;
    left: 20dp;
    bottom: 20dp;
    display: flex;
    align-items: center;
    gap: 8dp;
}

#health-icon {
    width: 32dp;
    height: 32dp;
    decorator: image(hud_icons:health);
}

#health-bar-wrapper {
    display: flex;
    flex-direction: column;
    gap: 4dp;
}

#health-bar-bg {
    width: 200dp;
    height: 20dp;
    background: rgba(0, 0, 0, 0.6);
    border: 2dp #444;
    border-radius: 4dp;
    overflow: hidden;
}

#health-bar-fill {
    height: 100%;
    background: linear-gradient(to right, #ff4444, #44ff44);
    transition: width 0.2s ease-out;
}

#health-text {
    font-size: 12dp;
    color: white;
    text-shadow: 1px 1px 2px rgba(0, 0, 0, 0.8);
}

#stamina-container {
    position: absolute;
    left: 60dp;
    bottom: 50dp;
    width: 180dp;
    height: 6dp;
    background: rgba(0, 0, 0, 0.4);
    border-radius: 3dp;
    overflow: hidden;
}

#stamina-bar {
    height: 100%;
    background: #4a9eff;
    transition: width 0.1s ease-out;
}
```

### Crosshair

**hud/crosshair.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="game">
    <div id="crosshair" data-if="show_crosshair">
        <div class="crosshair-line horizontal"></div>
        <div class="crosshair-line vertical"></div>
    </div>

    <div id="interaction-hint" data-if="can_interact">
        {{interaction_text}}
    </div>
</body>
</rml>
```

```css
#crosshair {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 20dp;
    height: 20dp;
    pointer-events: none;
}

.crosshair-line {
    position: absolute;
    background: rgba(255, 255, 255, 0.8);
}

.crosshair-line.horizontal {
    width: 20dp;
    height: 2dp;
    top: 50%;
    left: 0;
    transform: translateY(-50%);
}

.crosshair-line.vertical {
    width: 2dp;
    height: 20dp;
    left: 50%;
    top: 0;
    transform: translateX(-50%);
}

#interaction-hint {
    position: absolute;
    top: calc(50% + 40dp);
    left: 50%;
    transform: translateX(-50%);
    padding: 8dp 16dp;
    background: rgba(0, 0, 0, 0.6);
    border-radius: 4dp;
    font-size: 14dp;
    color: white;
    white-space: nowrap;
}
```

### Hotbar (Quick Inventory)

**hud/hotbar.rml:**

```html
<rml>
<head>
    <link type="text/rcss" href="../styles/hud.rcss"/>
</head>
<body data-model="inventory">
    <div id="hotbar">
        <div data-for="slot in hotbar" class="hotbar-slot" data-attr-selected="slot.index == selected_index">
            <div class="slot-number">{{slot.index + 1}}</div>
            <div class="slot-icon" data-if="slot.item">
                <img src="{{slot.item.icon}}"/>
                <span class="slot-count" data-if="slot.item.count > 1">{{slot.item.count}}</span>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#hotbar {
    position: absolute;
    bottom: 20dp;
    left: 50%;
    transform: translateX(-50%);
    display: flex;
    gap: 4dp;
    padding: 4dp;
    background: rgba(0, 0, 0, 0.4);
    border-radius: 4dp;
}

.hotbar-slot {
    width: 48dp;
    height: 48dp;
    background: rgba(0, 0, 0, 0.5);
    border: 2dp solid rgba(255, 255, 255, 0.2);
    border-radius: 4dp;
    display: flex;
    align-items: center;
    justify-content: center;
    position: relative;
}

.hotbar-slot[selected="true"] {
    border-color: #4a9eff;
    background: rgba(74, 158, 255, 0.2);
}

.slot-number {
    position: absolute;
    top: 2dp;
    left: 4dp;
    font-size: 10dp;
    color: rgba(255, 255, 255, 0.6);
}

.slot-icon {
    width: 32dp;
    height: 32dp;
}

.slot-count {
    position: absolute;
    bottom: 2dp;
    right: 4dp;
    font-size: 10dp;
    color: white;
    text-shadow: 1px 1px 1px rgba(0, 0, 0, 0.8);
}
```

## Inventory Screen

**menus/inventory.rml:**

```html
<rml>
<head>
    <title>Inventory</title>
    <link type="text/rcss" href="../styles/menus.rcss"/>
</head>
<body data-model="inventory">
    <div id="inventory-panel">
        <div id="inventory-header">
            <h1>Inventory</h1>
            <button id="close-btn" data-event-click="close_inventory">×</button>
        </div>

        <div id="inventory-content">
            <!-- Player slots -->
            <div id="player-slots">
                <div class="equipment-slot" data-for="slot in equipment">
                    <img data-if="slot.item" src="{{slot.item.icon}}"/>
                    <span class="slot-label">{{slot.name}}</span>
                </div>
            </div>

            <!-- Inventory grid -->
            <div id="inventory-grid">
                <div data-for="item in items"
                     class="inventory-item"
                     data-event-click="select_item(item.index)">
                    <img src="{{item.icon}}"/>
                    <span class="item-count" data-if="item.count > 1">{{item.count}}</span>
                </div>
            </div>

            <!-- Item details -->
            <div id="item-details" data-if="selected_item">
                <h2>{{selected_item.name}}</h2>
                <p>{{selected_item.description}}</p>
                <div id="item-actions">
                    <button data-event-click="use_item">Use</button>
                    <button data-event-click="drop_item">Drop</button>
                </div>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#inventory-panel {
    position: absolute;
    top: 50%;
    left: 50%;
    transform: translate(-50%, -50%);
    width: 600dp;
    height: 400dp;
    background: rgba(20, 20, 20, 0.95);
    border: 2dp solid #444;
    border-radius: 8dp;
    display: flex;
    flex-direction: column;
}

#inventory-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 16dp;
    border-bottom: 1dp solid #333;
}

#inventory-header h1 {
    margin: 0;
    font-size: 24dp;
    color: white;
}

#close-btn {
    width: 32dp;
    height: 32dp;
    background: none;
    border: none;
    font-size: 24dp;
    color: #888;
    cursor: pointer;
}

#close-btn:hover {
    color: white;
}

#inventory-content {
    display: flex;
    flex: 1;
    padding: 16dp;
    gap: 16dp;
}

#player-slots {
    width: 100dp;
    display: flex;
    flex-direction: column;
    gap: 8dp;
}

.equipment-slot {
    width: 64dp;
    height: 64dp;
    background: rgba(255, 255, 255, 0.05);
    border: 2dp solid #333;
    border-radius: 4dp;
    position: relative;
}

.slot-label {
    position: absolute;
    bottom: -16dp;
    left: 50%;
    transform: translateX(-50%);
    font-size: 10dp;
    color: #666;
}

#inventory-grid {
    flex: 1;
    display: flex;
    flex-wrap: wrap;
    gap: 4dp;
    align-content: flex-start;
}

.inventory-item {
    width: 48dp;
    height: 48dp;
    background: rgba(255, 255, 255, 0.05);
    border: 2dp solid #333;
    border-radius: 4dp;
    cursor: pointer;
    position: relative;
}

.inventory-item:hover {
    border-color: #4a9eff;
}

#item-details {
    width: 180dp;
    padding: 12dp;
    background: rgba(255, 255, 255, 0.03);
    border-radius: 4dp;
}

#item-details h2 {
    margin: 0 0 8dp 0;
    font-size: 16dp;
    color: #4a9eff;
}

#item-details p {
    font-size: 12dp;
    color: #888;
    margin: 0 0 12dp 0;
}
```

## Pause Menu

**menus/pause_menu.rml:**

```html
<rml>
<head>
    <title>Pause Menu</title>
    <link type="text/rcss" href="../styles/menus.rcss"/>
</head>
<body data-model="game">
    <div id="pause-overlay">
        <div id="pause-menu">
            <h1>Paused</h1>

            <div id="menu-buttons">
                <button class="menu-button" data-event-click="resume_game">
                    <span class="button-text">Resume</span>
                </button>

                <button class="menu-button" data-event-click="open_settings">
                    <span class="button-text">Settings</span>
                </button>

                <button class="menu-button" data-event-click="save_game">
                    <span class="button-text">Save Game</span>
                </button>

                <button class="menu-button danger" data-event-click="quit_to_menu">
                    <span class="button-text">Quit to Menu</span>
                </button>
            </div>
        </div>
    </div>
</body>
</rml>
```

```css
#pause-overlay {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.7);
    display: flex;
    align-items: center;
    justify-content: center;
}

#pause-menu {
    width: 300dp;
    padding: 32dp;
    background: rgba(20, 20, 20, 0.95);
    border: 2dp solid #444;
    border-radius: 8dp;
    text-align: center;
}

#pause-menu h1 {
    margin: 0 0 24dp 0;
    font-size: 32dp;
    color: white;
}

#menu-buttons {
    display: flex;
    flex-direction: column;
    gap: 8dp;
}

.menu-button {
    height: 48dp;
    background: linear-gradient(180deg, #3a3a3a, #2a2a2a);
    border: 2dp solid #444;
    border-radius: 4dp;
    color: white;
    font-size: 16dp;
    cursor: pointer;
    transition: background 0.2s, border-color 0.2s;
}

.menu-button:hover {
    background: linear-gradient(180deg, #4a4a4a, #3a3a3a);
    border-color: #4a9eff;
}

.menu-button.danger {
    border-color: #663333;
}

.menu-button.danger:hover {
    background: linear-gradient(180deg, #553333, #442222);
    border-color: #ff4444;
}
```

## Data Bindings с ECS

### Регистрация моделей данных

```cpp
// src/ui/ui_data_models.hpp
#pragma once

#include <flecs.h>
#include <RmlUi/Core.h>

class UIDataModels {
public:
    UIDataModels(Rml::Context* context, flecs::world& world);

    void syncAll();  // Синхронизация всех данных из ECS

private:
    Rml::Context* context_;
    flecs::world& world_;

    // Models
    Rml::DataModelHandle playerModel_;
    Rml::DataModelHandle inventoryModel_;
    Rml::DataModelHandle gameModel_;

    // Cached data
    struct PlayerData {
        int health = 100;
        int maxHealth = 100;
        float stamina = 1.0f;
        Rml::String name = "Player";
    } playerData_;

    struct InventoryItemData {
        Rml::String name;
        Rml::String icon;
        Rml::String description;
        int count = 0;
    };

    struct InventoryData {
        Rml::Vector<InventoryItemData> items;
        Rml::Vector<InventoryItemData> hotbar;
        Rml::Vector<InventoryItemData> equipment;
        int selectedIndex = -1;
    } inventoryData_;

    struct GameData {
        bool showCrosshair = true;
        bool canInteract = false;
        Rml::String interactionText;
        bool isPaused = false;
    } gameData_;

    void setupPlayerModel();
    void setupInventoryModel();
    void setupGameModel();
};
```

### Реализация

```cpp
// src/ui/ui_data_models.cpp

UIDataModels::UIDataModels(Rml::Context* context, flecs::world& world)
    : context_(context), world_(world)
{
    setupPlayerModel();
    setupInventoryModel();
    setupGameModel();
}

void UIDataModels::setupPlayerModel() {
    if (auto model = context_->CreateDataModel("player")) {
        // Регистрация computed variable для процента здоровья
        model.Bind("health", &playerData_.health);
        model.Bind("max_health", &playerData_.maxHealth);
        model.Bind("stamina", &playerData_.stamina);
        model.Bind("name", &playerData_.name);

        // Вычисляемые переменные
        model.BindGetFunc("health_percent",
            [this](Rml::Variant& variant) {
                variant = static_cast<float>(playerData_.health) /
                          static_cast<float>(playerData_.maxHealth) * 100.0f;
            });

        model.BindGetFunc("stamina_percent",
            [this](Rml::Variant& variant) {
                variant = playerData_.stamina * 100.0f;
            });

        playerModel_ = model;
    }
}

void UIDataModels::setupInventoryModel() {
    if (auto model = context_->CreateDataModel("inventory")) {
        // Регистрация типа InventoryItemData
        if (auto itemModel = model.RegisterStruct<InventoryItemData>()) {
            itemModel.RegisterMember("name", &InventoryItemData::name);
            itemModel.RegisterMember("icon", &InventoryItemData::icon);
            itemModel.RegisterMember("description", &InventoryItemData::description);
            itemModel.RegisterMember("count", &InventoryItemData::count);
        }

        // Регистрация массивов
        model.Bind("items", &inventoryData_.items);
        model.Bind("hotbar", &inventoryData_.hotbar);
        model.Bind("equipment", &inventoryData_.equipment);
        model.Bind("selected_index", &inventoryData_.selectedIndex);

        // События
        model.BindEventFunc("select_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
                if (!args.empty()) {
                    inventoryData_.selectedIndex = args[0].Get<int>();
                    model.DirtyVariable("selected_index");
                }
            });

        model.BindEventFunc("use_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (inventoryData_.selectedIndex >= 0) {
                    // Отправить событие в ECS
                    world_.entity().set<UseItemEvent>({.slot = inventoryData_.selectedIndex});
                }
            });

        model.BindEventFunc("drop_item",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                if (inventoryData_.selectedIndex >= 0) {
                    world_.entity().set<DropItemEvent>({.slot = inventoryData_.selectedIndex});
                }
            });

        // Selected item getter
        model.BindGetFunc("selected_item",
            [this](Rml::Variant& variant) {
                if (inventoryData_.selectedIndex >= 0 &&
                    inventoryData_.selectedIndex < static_cast<int>(inventoryData_.items.size())) {
                    variant = inventoryData_.items[inventoryData_.selectedIndex];
                }
            });

        inventoryModel_ = model;
    }
}

void UIDataModels::setupGameModel() {
    if (auto model = context_->CreateDataModel("game")) {
        model.Bind("show_crosshair", &gameData_.showCrosshair);
        model.Bind("can_interact", &gameData_.canInteract);
        model.Bind("interaction_text", &gameData_.interactionText);
        model.Bind("is_paused", &gameData_.isPaused);

        // События меню
        model.BindEventFunc("resume_game",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<PauseGameEvent>({.paused = false});
            });

        model.BindEventFunc("open_settings",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<OpenMenuEvent>({.menu = "settings"});
            });

        model.BindEventFunc("close_inventory",
            [this](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                world_.entity().set<OpenMenuEvent>({.menu = "none"});
            });

        gameModel_ = model;
    }
}

void UIDataModels::syncAll() {
    // Sync player data from ECS
    world_.filter<PlayerComponent>().each([this](flecs::entity e, const PlayerComponent& player) {
        bool dirty = false;

        if (playerData_.health != static_cast<int>(player.health)) {
            playerData_.health = static_cast<int>(player.health);
            dirty = true;
        }
        if (playerData_.maxHealth != static_cast<int>(player.maxHealth)) {
            playerData_.maxHealth = static_cast<int>(player.maxHealth);
            dirty = true;
        }
        if (playerData_.stamina != player.stamina) {
            playerData_.stamina = player.stamina;
            dirty = true;
        }

        if (dirty) {
            playerModel_.DirtyVariable("health");
            playerModel_.DirtyVariable("stamina");
        }
    });

    // Sync inventory from ECS
    world_.filter<InventoryComponent>().each([this](flecs::entity e, const InventoryComponent& inv) {
        // Обновление items, hotbar, equipment
        // ...
        inventoryModel_.DirtyVariable("items");
    });

    // Sync game state
    world_.filter<GameStateComponent>().each([this](flecs::entity e, const GameStateComponent& state) {
        if (gameData_.isPaused != state.isPaused) {
            gameData_.isPaused = state.isPaused;
            gameModel_.DirtyVariable("is_paused");
        }
        if (gameData_.canInteract != state.canInteract) {
            gameData_.canInteract = state.canInteract;
            gameModel_.DirtyVariable("can_interact");
        }
        if (gameData_.interactionText.c_str() != state.interactionText) {
            gameData_.interactionText = Rml::String(state.interactionText.c_str());
            gameModel_.DirtyVariable("interaction_text");
        }
    });
}
```

## Анимированные переходы

### Fade-in/Fade-out для меню

```css
#pause-menu {
    opacity: 0;
    transform: scale(0.95);
    transition: opacity 0.2s ease-out, transform 0.2s ease-out;
}

#pause-menu.visible {
    opacity: 1;
    transform: scale(1.0);
}
```

```cpp
// Показ меню с анимацией
document->Show();
auto menu = document->GetElementById("pause-menu");
menu->SetClass("visible", true);
```

### Анимация повреждения

```css
@keyframes damage-flash {
    0% { background-color: rgba(255, 0, 0, 0.5); }
    100% { background-color: transparent; }
}

#damage-overlay {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    pointer-events: none;
}

#damage-overlay.hit {
    animation: damage-flash 0.3s ease-out;
}
```

```cpp
// При получении урона
void onPlayerDamage(int damage) {
    auto overlay = hudDocument->GetElementById("damage-overlay");
    overlay->SetClass("hit", true);

    // Убрать класс после анимации
    setTimeout(300ms, [overlay]() {
        overlay->SetClass("hit", false);
    });
}
```

## Резюме паттернов

| Паттерн                 | Применение                    |
|-------------------------|-------------------------------|
| **Data Model**          | Синхронизация ECS ↔ UI        |
| **Computed Variables**  | health_percent, selected_item |
| **Event Functions**     | use_item, close_menu          |
| **CSS Transitions**     | Плавные анимации UI           |
| **Keyframe Animations** | damage-flash, loading-spinner |
| **Sprite Sheets**       | Иконки, кнопки                |
| **Templates**           | Переиспользуемые окна         |
| **Virtual Lists**       | Длинные списки инвентаря      |
