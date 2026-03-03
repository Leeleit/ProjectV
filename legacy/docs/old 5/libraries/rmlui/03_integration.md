# Интеграция RmlUi

🟡 **Уровень 2: Средний**

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
