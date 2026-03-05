# Интеграция SDL3

**🟡 Уровень 2: Средний**

---

## Содержание

- [1. Настройка CMake](#1-настройка-cmake)
- [2. Порядок включения заголовков](#2-порядок-включения-заголовков)
- [3. Инициализация приложения](#3-инициализация-приложения)
- [4. Интеграция с Vulkan](#4-интеграция-с-vulkan)
- [5. Интеграция с OpenGL](#5-интеграция-с-opengl)
- [6. Интеграция с другими библиотеками](#6-интеграция-с-другими-библиотеками)
- [7. Очистка ресурсов](#7-очистка-ресурсов)

---

## 1. Настройка CMake

### Базовая конфигурация

```cmake
# Подключение SDL3 как подмодуля
add_subdirectory(external/SDL)

# Создание исполняемого файла
add_executable(YourApp src/main.cpp)

# Линковка с SDL3
target_link_libraries(YourApp PRIVATE SDL3::SDL3)
```

### Расширенная конфигурация с компонентами

```cmake
# Явное указание необходимых компонентов SDL
target_link_libraries(YourApp PRIVATE 
    SDL3::SDL3
    SDL3::SDL3main  # Для SDL_MAIN_USE_CALLBACKS
)

# Дополнительные опции сборки
target_compile_definitions(YourApp PRIVATE 
    SDL_MAIN_USE_CALLBACKS=1  # Включение callback архитектуры
)

# Копирование DLL на Windows (пост-билд шаг)
if(WIN32)
    add_custom_command(TARGET YourApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:YourApp>"
    )
endif()
```

### Конфигурация для статической линковки

```cmake
# Принудительная статическая линковка
set(SDL_STATIC ON CACHE BOOL "Static linking for SDL3")

# Для статической линковки могут потребоваться дополнительные библиотеки
if(SDL_STATIC)
    if(WIN32)
        target_link_libraries(YourApp PRIVATE
            imm32
            version
            winmm
            setupapi
        )
    elseif(APPLE)
        target_link_libraries(YourApp PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework ForceFeedback"
            "-framework Carbon"
            "-framework IOKit"
            "-framework CoreVideo"
        )
    elseif(UNIX AND NOT APPLE)
        target_link_libraries(YourApp PRIVATE
            dl
            m
            pthread
            rt
        )
    endif()
endif()
```

---

## 2. Порядок включения заголовков

### Базовая структура

```cpp
// Для большинства приложений
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // Если используется SDL_MAIN_USE_CALLBACKS
```

### С Vulkan и volk

При использовании volk с `VK_NO_PROTOTYPES` важен порядок включения:

```cpp
#define VK_NO_PROTOTYPES  // Отключение прототипов Vulkan из заголовков

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>  // Vulkan-специфичные функции SDL
#include "volk.h"             // Загрузчик Vulkan функций

// Другие Vulkan заголовки
#include <vulkan/vulkan.h>
```

### С OpenGL

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>  // OpenGL-специфичные функции SDL

// OpenGL заголовки (зависят от платформы)
#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif
```

---

## 3. Инициализация приложения

### Callback архитектура (рекомендуется)

```cpp
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState {
    SDL_Window* window = nullptr;
    // Дополнительное состояние приложения
};

// Инициализация приложения
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    // Настройка метаданных приложения
    SDL_SetAppMetadata("YourApp", "1.0.0", "com.example.yourapp");
    
    // Инициализация подсистем SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER)) {
        return SDL_APP_FAILURE;
    }
    
    // Создание окна
    auto* app = new AppState;
    app->window = SDL_CreateWindow("Your Application", 1280, 720, 
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);
    
    if (!app->window) {
        delete app;
        return SDL_APP_FAILURE;
    }
    
    *appstate = app;
    return SDL_APP_CONTINUE;
}

// Обработка событий
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* app = static_cast<AppState*>(appstate);
    
    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.key == SDLK_ESCAPE) {
                return SDL_APP_SUCCESS;
            }
            break;
        // Обработка других событий
    }
    
    return SDL_APP_CONTINUE;
}

// Игровой цикл (вызывается каждый кадр)
SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);
    
    // Обновление логики
    update_game_logic();
    
    // Рендеринг
    render_frame(app->window);
    
    return SDL_APP_CONTINUE;
}

// Очистка ресурсов
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    
    if (app) {
        SDL_DestroyWindow(app->window);
        delete app;
    }
    
    SDL_Quit();
}
```

### Классическая архитектура (main loop)

```cpp
#include <SDL3/SDL.h>

int main(int argc, char* argv[]) {
    // Инициализация
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return 1;
    }
    
    // Создание окна
    SDL_Window* window = SDL_CreateWindow("Your App", 1280, 720, 
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Quit();
        return 1;
    }
    
    // Игровой цикл
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            // Обработка других событий
        }
        
        // Обновление и рендеринг
        update_game_logic();
        render_frame(window);
    }
    
    // Очистка
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}
```

---

## 4. Интеграция с Vulkan

### Полная последовательность инициализации

```cpp
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    // ... другие Vulkan объекты
};

bool init_vulkan_with_sdl(SDL_Window* window, VulkanContext* ctx) {
    // 1. Получение расширений, необходимых для создания поверхности
    Uint32 extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensions) {
        return false;
    }
    
    // 2. Создание VkInstance с расширениями от SDL
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.enabledExtensionCount = extensionCount;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledLayerCount = 0;  // Или добавьте validation layers
    
    if (vkCreateInstance(&instanceInfo, nullptr, &ctx->instance) != VK_SUCCESS) {
        return false;
    }
    
    // 3. Создание поверхности через SDL
    if (!SDL_Vulkan_CreateSurface(window, ctx->instance, nullptr, &ctx->surface)) {
        vkDestroyInstance(ctx->instance, nullptr);
        return false;
    }
    
    // 4. Выбор физического устройства
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, nullptr);
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, devices.data());
    
    // Выбор подходящего устройства (упрощённо)
    ctx->physicalDevice = devices[0];
    
    // 5. Создание логического устройства
    // ... (стандартная процедура Vulkan)
    
    return true;
}
```

### Интеграция с volk

```cpp
#define VK_NO_PROTOTYPES
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include "volk.h"

bool init_vulkan_volk_sdl(SDL_Window* window) {
    // 1. Инициализация volk через SDL (если SDL уже загрузил loader)
    if (!volkInitializeCustom(SDL_Vulkan_GetVkGetInstanceProcAddr())) {
        // Альтернатива: стандартная инициализация
        if (!volkInitialize()) {
            return false;
        }
    }
    
    // 2. Получение расширений от SDL
    Uint32 extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(&extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(&extensionCount, extensions.data());
    
    // 3. Создание instance и загрузка функций
    VkInstance instance = VK_NULL_HANDLE;
    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instanceInfo.ppEnabledExtensionNames = extensions.data();
    
    if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS) {
        return false;
    }
    
    // 4. Загрузка instance-уровневых функций
    volkLoadInstance(instance);
    
    // 5. Создание поверхности
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        vkDestroyInstance(instance, nullptr);
        return false;
    }
    
    return true;
}
```

### Обработка изменения размера окна

```cpp
void handle_window_resize(SDL_Window* window, VulkanContext* ctx) {
    // Получение нового размера в пикселях (важно для HiDPI)
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    
    // Ожидание завершения работы GPU
    vkDeviceWaitIdle(ctx->device);
    
    // Пересоздание swapchain с новым размером
    recreate_swapchain(ctx, width, height);
}
```

---

## 5. Интеграция с OpenGL

### Создание OpenGL контекста

```cpp
SDL_Window* init_opengl_window() {
    // Установка атрибутов OpenGL перед созданием окна
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    // Создание окна с поддержкой OpenGL
    SDL_Window* window = SDL_CreateWindow("OpenGL App", 1280, 720,
                                         SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    
    if (!window) {
        return nullptr;
    }
    
    // Создание OpenGL контекста
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        SDL_DestroyWindow(window);
        return nullptr;
    }
    
    // Активация контекста
    SDL_GL_MakeCurrent(window, glContext);
    
    // Настройка вертикальной синхронизации
    SDL_GL_SetSwapInterval(1);
    
    return window;
}
```

### Основной цикл рендеринга OpenGL

```cpp
void opengl_render_loop(SDL_Window* window) {
    bool running = true;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
        
        // Очистка буферов
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Рендеринг сцены
        render_opengl_scene();
        
        // Swap buffers
        SDL_GL_SwapWindow(window);
    }
}
```

---

## 6. Интеграция с другими библиотеками

### SDL + ImGui

```cpp
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"  // или imgui_impl_vulkan.h

void init_imgui_with_sdl(SDL_Window* window, SDL_GLContext glContext) {
    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    
    // Инициализация бэкенда для SDL3 и OpenGL
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 460");
    
    // Настройка стиля
    ImGui::StyleColorsDark();
}

void imgui_render_frame(SDL_Window* window) {
    // Начало нового кадра ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    
    // Создание интерфейса
    ImGui::Begin("Debug Window");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
    
    // Рендеринг ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
```

### SDL + flecs (ECS)

```cpp
#include <SDL3/SDL.h>
#include <flecs.h>

struct SDLIntegration {
    flecs::world* world;
    
    void init() {
        world = new flecs::world();
        
        // Система обработки событий SDL
        world->system<SDL_Event>("ProcessSDLEvents")
            .kind(flecs::OnUpdate)
            .iter([this](flecs::iter& it) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    // Отправка события в ECS
                    world->event<SDL_Event>()
                        .id(flecs::Wildcard)
                        .entity(event.type)
                        .ctx(&event)
                        .emit();
                }
            });
    }
};
```

### SDL + miniaudio

```cpp
#include <SDL3/SDL.h>
#include "miniaudio.h"

struct AudioSystem {
    ma_engine engine;
    
    bool init() {
        ma_result result = ma_engine_init(NULL, &engine);
        return result == MA_SUCCESS;
    }
    
    void play_sound(const char* filepath) {
        ma_engine_play_sound(&engine, filepath, NULL);
    }
    
    void update_with_sdl() {
        // Обновление аудио системы (вызывается каждый кадр)
        ma_engine_update(&engine);
    }
};
```

---

## 7. Очистка ресурсов

### Правильный порядок очистки для Vulkan

```cpp
void cleanup_vulkan_resources(SDL_Window* window, VulkanContext* ctx) {
    // 1. Ожидание завершения работы GPU
    vkDeviceWaitIdle(ctx->device);
    
    // 2. Уничтожение Vulkan ресурсов (в обратном порядке создания)
    destroy_swapchain(ctx);
    
    // 3. Уничтожение поверхности
    if (ctx->surface) {
        vkDestroySurfaceKHR(ctx->instance, ctx->surface, nullptr);
        ctx->surface = VK_NULL_HANDLE;
    }
    
    // 4. Уничтожение устройства и instance
    if (ctx->device) {
        vkDestroyDevice(ctx->device, nullptr);
        ctx->device = VK_NULL_HANDLE;
    }
    
    if (ctx->instance) {
        vkDestroyInstance(ctx->instance, nullptr);
        ctx->instance = VK_NULL_HANDLE;
    }
    
    // 5. Уничтожение окна SDL (если не в callback режиме)
    if (window) {
        SDL_DestroyWindow(window);
    }
}
```

### Очистка в callback архитектуре

```cpp
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    auto* app = static_cast<AppState*>(appstate);
    
    // 1. Очистка пользовательских ресурсов
    cleanup_application_resources(app);
    
    // 2. Уничтожение окна
    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = nullptr;
    }
    
    // 3. Освобождение состояния приложения
    delete app;
    
    // 4. SDL_Quit будет вызван автоматически SDL
}
```

### Общие рекомендации по очистке

1. **Порядок имеет значение**: Ресурсы должны уничтожаться в порядке, обратном созданию
2. **Проверка на nullptr**: Всегда проверяйте указатели перед их освобождением
3. **Ожидание GPU**: Для графических API дождитесь завершения работы GPU перед уничтожением ресурсов
4. **Отслеживание владения**: Чётко определяйте, какой компонент владеет каким ресурсом

---

## Следующие шаги

1. **Выбор архитектуры**: Определитесь между callback и классической архитектурой
2. **Настройка сборки**: Настройте CMake для вашей целевой платформы
3. **Интеграция графического API**: Выберите и настройте Vulkan или OpenGL
4. **Добавление библиотек**: Интегрируйте необходимые библиотеки (ImGui, flecs и др.)
5. **Тестирование**: Протестируйте на всех целевых платформах

Для специфичных паттернов интеграции смотрите:

- [Сценарии использования](use-cases.md) — готовые архитектурные паттерны
- [Специализированные паттерны интеграции](projectv-integration.md) — продвинутые сценарии для воксельных движков

---

## Связанные разделы

- [Основные понятия](concepts.md) — фундаментальные концепции SDL
- [Быстрый старт](quickstart.md) — минимальный работающий пример
- [Решение проблем](troubleshooting.md) — диагностика и исправление ошибок
- [Справочник API](api-reference.md) — полный список функций SDL

← [Назад к документации SDL](README.md)