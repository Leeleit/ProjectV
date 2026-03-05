# SDL3: Интеграция в ProjectV

> **Для понимания:** SDL3 — это фундамент оконной системы ProjectV. Мы используем callback-архитектуру SDL3 для полного
> контроля над жизненным циклом приложения, интегрируя MemoryManager для аллокаций и ProjectV Log для обработки ошибок.

## CMake конфигурация

### Git Submodules с интеграцией ProjectV

ProjectV использует подмодули с полной интеграцией в архитектуру движка:

```
ProjectV/
├── external/
│   └── SDL/                     # Оконная система (с кастомными аллокаторами)
├── src/
│   ├── core/                    # Ядро движка
│   │   ├── memory/              # DOD-аллокаторы
│   │   ├── logging/             # Lock-free логгер
│   │   └── profiling/           # Tracy hooks
│   └── platform/
│       └── sdl/                 # SDL3 интеграция
└── CMakeLists.txt
```

```cmake
# CMakeLists.txt ProjectV
cmake_minimum_required(VERSION 3.25)
project(ProjectV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Внешние зависимости (только подмодули) ---
# Отключаем всё лишнее в подмодулях
set(SDL_STATIC ON CACHE BOOL "Build SDL as static library" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Disable SDL shared library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Disable SDL examples" FORCE)

# Подключение подмодулей
add_subdirectory(external/SDL)

# --- Ядро ProjectV ---
# MemoryManager модуль
add_library(projectv_core_memory STATIC
    src/core/memory/page_allocator.cpp
    src/core/memory/global_manager.cpp
    src/core/memory/arena_allocator.cpp
    src/core/memory/pool_allocator.cpp
    src/core/memory/linear_allocator.cpp
)

target_compile_features(projectv_core_memory PUBLIC cxx_std_26)
target_include_directories(projectv_core_memory PUBLIC src/core)

# Logging модуль
add_library(projectv_core_logging STATIC
    src/core/logging/logger.cpp
    src/core/logging/ring_buffer.cpp
)

target_compile_features(projectv_core_logging PUBLIC cxx_std_26)
target_include_directories(projectv_core_logging PUBLIC src/core)
target_link_libraries(projectv_core_logging PRIVATE projectv_core_memory)

# Profiling модуль (только для Profile конфигурации)
if(PROJECTV_ENABLE_PROFILING)
    add_library(projectv_core_profiling STATIC
        src/core/profiling/tracy_hooks.cpp
        src/core/profiling/sdl_profiler.cpp
    )
    target_compile_features(projectv_core_profiling PUBLIC cxx_std_26)
    target_include_directories(projectv_core_profiling PUBLIC src/core)
    target_link_libraries(projectv_core_profiling PRIVATE projectv_core_memory projectv_core_logging)
    target_compile_definitions(projectv_core_profiling PRIVATE TRACY_ENABLE)
endif()

# --- SDL3 модуль платформы ---
add_library(projectv_platform_sdl STATIC
    src/platform/sdl/context.cpp
    src/platform/sdl/input.cpp
    src/platform/sdl/window.cpp
)

target_compile_features(projectv_platform_sdl PUBLIC cxx_std_26)
target_include_directories(projectv_platform_sdl PUBLIC src/platform)
target_link_libraries(projectv_platform_sdl PRIVATE
    projectv_core_memory
    projectv_core_logging
    SDL3::SDL3-static
)

if(PROJECTV_ENABLE_PROFILING)
    target_link_libraries(projectv_platform_sdl PRIVATE projectv_core_profiling)
endif()

# --- Исполняемый файл ProjectV ---
add_executable(ProjectV
    src/main.cpp
)

target_link_libraries(ProjectV PRIVATE
    projectv_platform_sdl
)

# Profile конфигурация
if(PROJECTV_ENABLE_PROFILING)
    target_compile_definitions(ProjectV PRIVATE TRACY_ENABLE)
    target_link_libraries(ProjectV PRIVATE Tracy::TracyClient)
endif()
```

## C++26 Module с интеграцией ProjectV

### 1. SDL Context с MemoryManager интеграцией

```cpp
// src/platform/sdl/context.cppm - Primary Module Interface
module;
// Global Module Fragment: изолируем заголовки SDL
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>

export module projectv.platform.sdl.context;

import std;
import projectv.core.memory;
import projectv.core.log;
import projectv.core.profiling;

namespace projectv::platform::sdl {

// Константы для категорий логгера
constexpr auto LOG_CATEGORY = projectv::core::logging::LogCategory::Platform;

// --- Обработка ошибок через std::expected ---
enum class SdlError {
    InitFailed,
    WindowCreationFailed,
    VulkanSurfaceFailed,
    MemoryAllocationFailed,
    EventProcessingFailed,
    SurfaceNotSupported,
    InvalidWindowHandle
};

template<typename T>
using SdlResult = std::expected<T, SdlError>;

inline std::string to_string(SdlError error) {
    switch (error) {
        case SdlError::InitFailed: return "SDL initialization failed";
        case SdlError::WindowCreationFailed: return "SDL window creation failed";
        case SdlError::VulkanSurfaceFailed: return "SDL Vulkan surface creation failed";
        case SdlError::MemoryAllocationFailed: return "Memory allocation failed (ProjectV MemoryManager)";
        case SdlError::EventProcessingFailed: return "SDL event processing failed";
        case SdlError::SurfaceNotSupported: return "Vulkan surface not supported by selected device";
        case SdlError::InvalidWindowHandle: return "Invalid SDL window handle";
        default: return "Unknown SDL error";
    }
}

// --- MemoryManager интеграция через SDL_Allocator ---
class SdlAllocator {
public:
    static void* allocate(size_t size) noexcept {
        PROJECTV_PROFILE_ZONE("SdlAllocator::allocate");

        // Используем ProjectV MemoryManager для SDL аллокаций
        auto& memoryManager = projectv::core::memory::getGlobalMemoryManager();

        // Для SDL объектов используем PoolAllocator (часто создаются/удаляются)
        void* ptr = memoryManager.getThreadArena().allocate(size);

        if (ptr) {
            PROJECTV_PROFILE_ALLOC(ptr, size);
            projectv::core::Log::trace(LOG_CATEGORY,
                "SDL allocation: {} bytes at {}", size, ptr);
        } else {
            projectv::core::Log::error(LOG_CATEGORY,
                "Failed to allocate {} bytes for SDL", size);
        }

        return ptr;
    }

    static void* reallocate(void* ptr, size_t new_size) noexcept {
        PROJECTV_PROFILE_ZONE("SdlAllocator::reallocate");

        if (!ptr) return allocate(new_size);

        // В реальной реализации нужно копировать данные и отслеживать оригинальный аллокатор
        void* newPtr = allocate(new_size);
        if (newPtr) {
            // В реальном коде нужно знать оригинальный размер
            // std::memcpy(newPtr, ptr, std::min(new_size, /* original size */));
            deallocate(ptr);
        }
        return newPtr;
    }

    static void deallocate(void* ptr) noexcept {
        PROJECTV_PROFILE_ZONE("SdlAllocator::deallocate");

        if (ptr) {
            PROJECTV_PROFILE_FREE(ptr);
            // В реальной реализации нужно определить, из какого аллокатора выделена память
            // и вернуть её туда
        }
    }

    static void setAsGlobalAllocator() noexcept {
        PROJECTV_PROFILE_ZONE("SdlAllocator::setAsGlobalAllocator");

        SDL_SetMemoryFunctions(&allocate, &reallocate, &deallocate);
        projectv::core::Log::info(LOG_CATEGORY,
            "Custom memory allocator set (ProjectV MemoryManager)");
    }
};

// --- SDL Context с интеграцией ProjectV ---
class SdlContext {
public:
    SdlContext() = default;
    ~SdlContext() {
        PROJECTV_PROFILE_ZONE("SdlContext::~SdlContext");
        shutdown();
    }

    // Non-copyable
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;

    // Movable
    SdlContext(SdlContext&& other) noexcept;
    SdlContext& operator=(SdlContext&& other) noexcept;

    // Инициализация с интеграцией ProjectV
    SdlResult<void> initialize(const char* title, int width, int height, uint32_t flags);
    void shutdown();

    // Getters
    SDL_Window* window() const noexcept { return window_; }
    bool is_running() const noexcept { return running_; }

    // Event processing
    SdlResult<void> process_events();

    // Profiling helpers
    void begin_frame_zone(const char* name) const noexcept;
    void end_frame_zone() const noexcept;

private:
    SdlResult<void> create_window(const char* title, int width, int height, uint32_t flags);
    SdlResult<void> setup_vulkan_surface();

    // SDL handles
    SDL_Window* window_ = nullptr;

    // Vulkan integration (optional)
    VkSurfaceKHR vulkan_surface_ = VK_NULL_HANDLE;

    // State
    bool running_ = true;
    bool initialized_ = false;

    // ProjectV интеграция
    bool custom_allocator_set_ = false;

    // Profiling
#ifdef TRACY_ENABLE
    mutable const char* current_frame_zone_ = nullptr;
#endif
};
```

### 2. Реализация инициализации

```cpp
// platform/sdl/context.cpp
#include "context.hpp"

SdlResult<void> SdlContext::initialize(const char* title, int width, int height, uint32_t flags) {
    PROJECTV_PROFILE_ZONE("SdlContext::initialize");

    // 1. Устанавливаем кастомные аллокаторы ProjectV
    if (!custom_allocator_set_) {
        SdlAllocator::setAsGlobalAllocator();
        custom_allocator_set_ = true;
    }

    // 2. Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        projectv::core::Log::error(LOG_CATEGORY, "SDL_Init failed: {}", SDL_GetError());
        return std::unexpected(SdlError::InitFailed);
    }

    projectv::core::Log::info(LOG_CATEGORY, "SDL initialized successfully");

    // 3. Создание окна
    if (auto result = create_window(title, width, height, flags); !result) {
        SDL_Quit();
        return std::unexpected(result.error());
    }

    // 4. Настройка Vulkan surface (если требуется)
    if (flags & SDL_WINDOW_VULKAN) {
        if (auto result = setup_vulkan_surface(); !result) {
            projectv::core::Log::warning(LOG_CATEGORY, "Vulkan surface setup failed: {}", to_string(result.error()));
            // Продолжаем без Vulkan
        }
    }

    initialized_ = true;
    projectv::core::Log::info(LOG_CATEGORY, "SDL context initialized: {}x{} {}", width, height, title);
    return {};
}

SdlResult<void> SdlContext::create_window(const char* title, int width, int height, uint32_t flags) {
    PROJECTV_PROFILE_ZONE("SdlContext::create_window");

    window_ = SDL_CreateWindow(title, width, height, flags);
    if (!window_) {
        projectv::core::Log::error(LOG_CATEGORY, "SDL_CreateWindow failed: {}", SDL_GetError());
        return std::unexpected(SdlError::WindowCreationFailed);
    }

    projectv::core::Log::info(LOG_CATEGORY, "Window created: {}x{} {}", width, height, title);
    return {};
}

SdlResult<void> SdlContext::setup_vulkan_surface() {
    // Эта функция будет реализована в интеграции с Vulkan модулем
    // Сейчас просто возвращаем успех
    return {};
}

SdlResult<void> SdlContext::process_events() {
    PROJECTV_PROFILE_ZONE("SdlContext::process_events");

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                projectv::core::Log::info(LOG_CATEGORY, "Quit event received");
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    running_ = false;
                    projectv::core::Log::info(LOG_CATEGORY, "Escape key pressed, quitting");
                }
                break;

            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                projectv::core::Log::info(LOG_CATEGORY, "Window resized: {}x{}",
                    event.window.data1, event.window.data2);
                break;

            default:
                // Обработка других событий
                break;
        }
    }

    return {};
}

void SdlContext::shutdown() {
    PROJECTV_PROFILE_ZONE("SdlContext::shutdown");

    if (!initialized_) return;

    // Освобождаем Vulkan surface (если есть)
    if (vulkan_surface_ != VK_NULL_HANDLE) {
        // Освобождение будет в Vulkan модуле
        vulkan_surface_ = VK_NULL_HANDLE;
    }

    // Уничтожаем окно
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        projectv::core::Log::info(LOG_CATEGORY, "Window destroyed");
    }

    // Завершаем SDL
    SDL_Quit();
    projectv::core::Log::info(LOG_CATEGORY, "SDL shutdown complete");

    initialized_ = false;
}

void SdlContext::begin_frame_zone(const char* name) const noexcept {
#ifdef TRACY_ENABLE
    current_frame_zone_ = name;
    ZoneScopedN(name);
#endif
}

void SdlContext::end_frame_zone() const noexcept {
#ifdef TRACY_ENABLE
    current_frame_zone_ = nullptr;
#endif
}

SdlContext::SdlContext(SdlContext&& other) noexcept
    : window_(other.window_)
    , vulkan_surface_(other.vulkan_surface_)
    , running_(other.running_)
    , initialized_(other.initialized_)
    , custom_allocator_set_(other.custom_allocator_set_) {

    other.window_ = nullptr;
    other.vulkan_surface_ = VK_NULL_HANDLE;
    other.initialized_ = false;
}

SdlContext& SdlContext::operator=(SdlContext&& other) noexcept {
    if (this != &other) {
        shutdown();

        window_ = other.window_;
        vulkan_surface_ = other.vulkan_surface_;
        running_ = other.running_;
        initialized_ = other.initialized_;
        custom_allocator_set_ = other.custom_allocator_set_;

        other.window_ = nullptr;
        other.vulkan_surface_ = VK_NULL_HANDLE;
        other.initialized_ = false;
    }
    return *this;
}
```

### 3. Пример использования с интеграцией ProjectV

```cpp
// Пример main.cpp с полной интеграцией ProjectV
module;
// Global Module Fragment
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

export module projectv.main;

import std;
import projectv.platform.sdl.context;
import projectv.graphics.vulkan.context;
import projectv.core.log;
import projectv.core.profiling;

namespace projectv {

class Application {
public:
    Application() = default;

    SdlResult<void> run() {
        PROJECTV_PROFILE_ZONE("Application::run");

        // 1. Инициализация SDL
        projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
            "Initializing SDL context...");
        if (auto result = sdl_context_.initialize("ProjectV", 1280, 720,
                                                 SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
            !result) {
            projectv::core::Log::error(projectv::core::logging::LogCategory::Core,
                "SDL initialization failed: {}", to_string(result.error()));
            return std::unexpected(result.error());
        }

        // 2. Инициализация Vulkan (если требуется)
        projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
            "Initializing Vulkan context...");
        if (auto result = vulkan_context_.initialize(sdl_context_.window());
            !result) {
            projectv::core::Log::error(projectv::core::logging::LogCategory::Core,
                "Vulkan initialization failed: {}", to_string(result.error()));
            sdl_context_.shutdown();
            return std::unexpected(result.error());
        }

        // 3. Главный цикл приложения
        projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
            "Entering main loop...");
        while (sdl_context_.is_running()) {
            sdl_context_.begin_frame_zone("Frame");

            // Обработка событий
            if (auto result = sdl_context_.process_events(); !result) {
                projectv::core::Log::warning(projectv::core::logging::LogCategory::Core,
                    "Event processing failed: {}", to_string(result.error()));
            }

            // Обновление логики
            update();

            // Рендеринг
            render();

            sdl_context_.end_frame_zone();

            FrameMark;
        }

        // 4. Завершение работы
        projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
            "Shutting down...");
        vulkan_context_.shutdown();
        sdl_context_.shutdown();

        projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
            "Application terminated successfully");
        return {};
    }

private:
    void update() {
        ZoneScopedN("Update");
        // Обновление логики игры
    }

    void render() {
        ZoneScopedN("Render");
        // Рендеринг кадра
    }

    projectv::platform::sdl::SdlContext sdl_context_;
    projectv::graphics::vulkan::VulkanContext vulkan_context_;
};

} // namespace projectv

// Точка входа с callback-архитектурой SDL3
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    PROJECTV_PROFILE_ZONE("SDL_AppInit");

    auto app = std::make_unique<projectv::Application>();
    if (auto result = app->run(); !result) {
        projectv::core::Log::error(projectv::core::logging::LogCategory::Core,
            "Application failed to run: {}", to_string(result.error()));
        return SDL_APP_FAILURE;
    }

    *appstate = app.release();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    PROJECTV_PROFILE_ZONE("SDL_AppEvent");

    auto* app = static_cast<projectv::Application*>(appstate);
    // События уже обрабатываются в SdlContext::process_events()
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    PROJECTV_PROFILE_ZONE("SDL_AppIterate");

    // Итерации уже обрабатываются в Application::run()
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    PROJECTV_PROFILE_ZONE("SDL_AppQuit");

    auto* app = static_cast<projectv::Application*>(appstate);
    delete app;

    projectv::core::Log::info(projectv::core::logging::LogCategory::Core,
        "SDL application quit with result: {}",
        result == SDL_APP_SUCCESS ? "SUCCESS" : "FAILURE");
}
```

### 4. Чеклист интеграции ProjectV

- [x] **C++26 Module** с Global Module Fragment для изоляции заголовков SDL
- [x] **CMake Integration** с `projectv_core_memory`, `projectv_core_logging`, `projectv_core_profiling`
- [x] **MemoryManager Integration** через `SdlAllocator` с кастомными аллокаторами
- [x] **Logging Integration** - все `std::println` заменены на `projectv::core::Log`
- [x] **Profiling Integration** - Tracy hooks для аллокаций и операций
- [x] **Vulkan Integration** - поддержка SDL_WINDOW_VULKAN и surface creation
- [x] **Callback Architecture** - полная интеграция с SDL_MAIN_USE_CALLBACKS
- [x] **Error Handling** - использование `std::expected` и `SdlResult`

### 5. Ключевые особенности интеграции

1. **Изоляция заголовков:** Global Module Fragment предотвращает загрязнение пространства имён
2. **Кастомные аллокаторы:** `SdlAllocator` интегрирует SDL с ProjectV MemoryManager
3. **UNIX-way логирование:** Все ошибки SDL перенаправляются в `projectv::core::Log`
4. **Zero-overhead profiling:** Tracy макросы только в Profile конфигурации
5. **Type-safe ошибки:** `SdlError` enum вместо строковых ошибок
6. **Resource management:** RAII для `SdlContext` с автоматическим shutdown
7. **Callback архитектура:** Полная поддержка SDL3 callback-based main

### 6. Пример использования в реальном проекте

```cpp
// Простой пример использования в реальном проекте
#include "projectv/platform/sdl/context.hpp"
#include "projectv/graphics/vulkan/context.hpp"
#include "projectv/core/log.hpp"

int main() {
    projectv::platform::sdl::SdlContext sdl;
    projectv::graphics::vulkan::VulkanContext vulkan;

    // Инициализация
    if (auto result = sdl.initialize("My Game", 1920, 1080, SDL_WINDOW_VULKAN); !result) {
        return 1;
    }

    if (auto result = vulkan.initialize(sdl.window()); !result) {
        return 1;
    }

    // Главный цикл
    while (sdl.is_running()) {
        sdl.process_events();
        // ... логика и рендеринг
    }

    return 0;
}
```

### 7. Отладка и профилирование

```bash
# Debug сборка с валидацией SDL
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Profile сборка с Tracy
cmake -B build-profile -DCMAKE_BUILD_TYPE=Release -DPROJECTV_ENABLE_PROFILING=ON
cmake --build build-profile

# Запуск с профайлером
./build-profile/ProjectV
```

### 8. Совместимость с другими библиотеками ProjectV

- **Vulkan:** Полная интеграция через `SDL_WINDOW_VULKAN` и `SDL_Vulkan_CreateSurface`
- **Flecs ECS:** События SDL могут быть отправлены как компоненты ECS
- **Tracy:** Автоматическое профилирование аллокаций и операций SDL
- **MemoryManager:** Все аллокации SDL проходят через ProjectV аллокаторы
- **Logging:** Все ошибки SDL логируются в единую систему ProjectV

