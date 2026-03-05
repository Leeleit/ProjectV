# Интеграция flecs

**🟡 Уровень 2: Средний**

## Оглавление

- [1. CMake](#1-cmake)
- [2. Include](#2-include)
- [3. Порядок вызовов](#3-порядок-вызовов)
- [4. Модули](#4-модули)
- [5. Addons](#5-addons-и-кастомная-сборка)
- [6. Порядок уничтожения](#6-порядок-уничтожения)
- [7. Связка flecs + Vulkan](#7-связка-flecs--vulkan)

---

## 1. CMake

### Добавление flecs как подпроекта

```cmake
add_subdirectory(external/flecs)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE
    flecs::flecs_static
    # ... другие библиотеки
)
```

flecs собирается как статическая библиотека. Зависимости (ws2_32, dbghelp, pthread) подтягиваются автоматически.

---

## 2. Include

Для C++ и C API достаточно одного заголовка:

```cpp
#include "flecs.h"
```

При использовании `SDL_MAIN_USE_CALLBACKS` и `volk`:

```cpp
#define VK_NO_PROTOTYPES
#define SDL_MAIN_USE_CALLBACKS 1
#include "SDL3/SDL.h"
#include "volk.h"
#include "flecs.h"
```

---

## 3. Порядок вызовов

### С SDL_MAIN_USE_CALLBACKS

1. **Init**: Создать `flecs::world`, зарегистрировать компоненты и системы.
2. **Iterate**: `world.progress(dt)` -> Рендеринг.
3. **Quit**: Уничтожить world.

### Пример AppState

```cpp
struct AppState {
    flecs::world* ecs = nullptr;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* app = new AppState;
    app->ecs = new flecs::world;
    *appstate = app;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* app = static_cast<AppState*>(appstate);
    app->ecs->progress(1.0f / 60.0f);
    return SDL_APP_CONTINUE;
}
```

---

## 4. Модули

Разделяйте логику на модули:

```cpp
struct game_module {
    game_module(flecs::world& world) {
        world.module<game_module>();
        world.component<Position>();
        world.system<Position>("Move").each([](Position& p) { ... });
    }
};

// В main:
world.import<game_module>();
```

---

## 5. Addons и кастомная сборка

При сборке через CMake (`add_subdirectory`) все addons включены по умолчанию.
Для кастомной сборки (defines) см. [BuildingFlecs.md](../../external/flecs/docs/BuildingFlecs.md).

---

## 6. Порядок уничтожения

1. Завершить рендеринг (WaitIdle).
2. Уничтожить `flecs::world` (освободит компоненты).
3. Уничтожить графические ресурсы (Device, Window).

---

## 7. Связка flecs + Vulkan

Компоненты могут хранить Vulkan handles (`VkBuffer` и т.д.).

**Паттерн:** Использовать `Observer` на `OnAdd` (создание) и `OnRemove` (уничтожение).

```cpp
struct MeshComponent {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
};

// OnAdd
world.observer<MeshComponent>().event(flecs::OnAdd)
    .each([](flecs::entity e, MeshComponent& m) {
        // vmaCreateBuffer(...)
    });

// OnRemove
world.observer<MeshComponent>().event(flecs::OnRemove)
    .each([](flecs::entity e, MeshComponent& m) {
        if (m.buffer) {
            // vmaDestroyBuffer(...)
        }
    });
```
