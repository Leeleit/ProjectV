# First Scene: Полная интеграция ProjectV

**🟡 Уровень 2: Средний** — Создание первой сцены со всеми компонентами движка.

## Цель

Создать работающую сцену, которая объединяет все ключевые компоненты ProjectV:

- **SDL3**: Окно и ввод
- **Vulkan**: Рендеринг треугольника
- **Flecs**: ECS для игровой логики
- **JoltPhysics**: Физика падения кубика
- **ImGui**: Debug интерфейс

Это интеграционный тест всей архитектуры, который должен работать "из коробки".

## Структура проекта

```
first_scene/
├── CMakeLists.txt          # Конфигурация сборки
├── main.cpp               # Главный файл
├── shaders/               # Шейдеры Vulkan
│   ├── triangle.vert
│   └── triangle.frag
└── README.md              # Инструкции
```

## Шаг 1: Настройка окружения

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.25)
project(FirstScene VERSION 0.1.0 LANGUAGES CXX)

# C++ стандарт
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Подключаем ProjectV библиотеки
find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(flecs REQUIRED)
find_package(JoltPhysics REQUIRED)
find_package(glm REQUIRED)

# Исполняемый файл
add_executable(first_scene main.cpp)

# Линковка
target_link_libraries(first_scene PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    flecs::flecs
    JoltPhysics::JoltPhysics
    glm::glm
)

# Копирование шейдеров
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert
    ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.vert
    COPYONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag
    ${CMAKE_CURRENT_BINARY_DIR}/shaders/triangle.frag
    COPYONLY
)
```

## Шаг 2: Основная архитектура (main.cpp)

### Часть 1: Заголовочные файлы и структура данных

```cpp
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <flecs.h>
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <expected>
#include <memory>

// Структура данных приложения (DOD подход: все данные в одном месте)
struct ApplicationData {
    // SDL
    SDL_Window* window = nullptr;

    // Vulkan
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    // ECS
    std::unique_ptr<flecs::world> ecs_world;

    // Physics
    JPH::PhysicsSystem* physics_system = nullptr;

    // Состояние
    bool is_running = true;
    int window_width = 1280;
    int window_height = 720;
};
```

### Часть 2: Инициализация SDL

```cpp
bool init_sdl(ApplicationData& app_data) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    // Создаем окно с Vulkan поддержкой
    app_data.window = SDL_CreateWindow(
        "ProjectV First Scene",
        app_data.window_width,
        app_data.window_height,
        SDL_WINDOW_VULKAN
    );

    if (!app_data.window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        return false;
    }

    return true;
}
```

### Часть 3: Инициализация Vulkan (упрощённая)

```cpp
bool init_vulkan(ApplicationData& app_data) {
    // Создаем Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "First Scene";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "ProjectV";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Получаем расширения для SDL
    unsigned int extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(app_data.window, &extensionCount, nullptr);
    std::vector<const char*> extensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(app_data.window, &extensionCount, extensions.data());

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &app_data.instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        return false;
    }

    // Создаем surface
    if (!SDL_Vulkan_CreateSurface(app_data.window, app_data.instance, nullptr, &app_data.surface)) {
        std::cerr << "Failed to create Vulkan surface!" << std::endl;
        return false;
    }

    return true;
}
```

### Часть 4: Инициализация Flecs ECS

```cpp
// Коды ошибок для инициализации
enum class InitError {
    SDL,
    Vulkan,
    Flecs,
    JoltPhysics
};

std::expected<void, InitError> init_flecs(ApplicationData& app_data) {
    app_data.ecs_world = std::make_unique<flecs::world>();

    // Создаем компоненты (DOD подход: данные отдельно от логики)
    struct Position {
        glm::vec3 value;
    };

    struct Velocity {
        glm::vec3 value;
    };

    struct Renderable {
        // Флаг для рендеринга (tag component)
    };

    struct PhysicsBody {
        JPH::BodyID body_id;
    };

    // Регистрируем компоненты
    app_data.ecs_world->component<Position>();
    app_data.ecs_world->component<Velocity>();
    app_data.ecs_world->component<Renderable>();
    app_data.ecs_world->component<PhysicsBody>();

    // Создаем системы (трансформации данных)
    app_data.ecs_world->system<Position, Velocity>("UpdatePosition")
        .each([](Position& p, Velocity& v) {
            p.value += v.value * 0.016f; // 60 FPS delta
        });

    return {};
}
```

### Часть 5: Инициализация JoltPhysics

```cpp
bool init_joltphysics(ApplicationData& app_data) {
    // Инициализируем Jolt
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Создаем физическую систему
    JPH::PhysicsSettings settings;
    app_data.physics_system = new JPH::PhysicsSystem();
    app_data.physics_system->Init(1024, 0, 1024, 1024, settings);

    // Создаем тело пола (статическое)
    JPH::BodyInterface& body_interface = app_data.physics_system->GetBodyInterface();

    JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(10.0f, 0.5f, 10.0f));
    JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
    JPH::ShapeRefC floor_shape = floor_shape_result.Get();

    JPH::BodyCreationSettings floor_settings(
        floor_shape,
        JPH::RVec3(0.0f, -2.0f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        JPH::ObjectLayer(0)
    );

    JPH::BodyID floor_id = body_interface.CreateBody(floor_settings);
    body_interface.AddBody(floor_id, JPH::EActivation::DontActivate);

    // Создаем динамическое тело (кубик)
    JPH::BoxShapeSettings box_shape_settings(JPH::Vec3(0.5f, 0.5f, 0.5f));
    JPH::ShapeSettings::ShapeResult box_shape_result = box_shape_settings.Create();
    JPH::ShapeRefC box_shape = box_shape_result.Get();

    JPH::BodyCreationSettings box_settings(
        box_shape,
        JPH::RVec3(0.0f, 5.0f, 0.0f),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        JPH::ObjectLayer(0)
    );

    JPH::BodyID box_id = body_interface.CreateBody(box_settings);
    body_interface.AddBody(box_id, JPH::EActivation::Activate);

    return true;
}
```

### Часть 6: Создание сцены

```cpp
void create_scene(ApplicationData& app_data) {
    if (!app_data.ecs_world) {
        std::cerr << "ECS world not initialized!" << std::endl;
        return;
    }

    // Создаем сущность кубика
    flecs::entity cube = app_data.ecs_world->entity("Cube")
        .set<Position>({{0.0f, 5.0f, 0.0f}})
        .set<Velocity>({{0.0f, 0.0f, 0.0f}})
        .add<Renderable>()
        .set<PhysicsBody>({{JPH::BodyID()}}); // BodyID будет установлен позже

    // Создаем сущность пола
    flecs::entity floor = app_data.ecs_world->entity("Floor")
        .set<Position>({{0.0f, -2.0f, 0.0f}})
        .add<Renderable>();

    std::cout << "Scene created with " << app_data.ecs_world->count<Renderable>()
              << " renderable entities" << std::endl;
}
```

### Часть 7: Игровой цикл

```cpp
void game_loop(ApplicationData& app_data) {
    SDL_Event event;

    Uint64 last_time = SDL_GetTicks();
    float delta_time = 0.0f;

    while (app_data.is_running) {
        Uint64 current_time = SDL_GetTicks();
        delta_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        // Обработка событий
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                app_data.is_running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    app_data.is_running = false;
                }
            }
        }

        // Обновление физики
        if (app_data.physics_system) {
            app_data.physics_system->Update(delta_time, 1, 1, nullptr, nullptr);
        }

        // Обновление ECS
        if (app_data.ecs_world) {
            app_data.ecs_world->progress(delta_time);
        }

        // Синхронизация физики и ECS
        sync_physics_with_ecs(app_data);

        // Рендеринг (упрощённо)
        render_frame(app_data);

        // Ограничение FPS
        SDL_Delay(16); // ~60 FPS
    }
}
```

### Часть 8: Синхронизация физики и ECS

```cpp
void sync_physics_with_ecs(ApplicationData& app_data) {
    if (!app_data.physics_system || !app_data.ecs_world) return;

    JPH::BodyInterface& body_interface = app_data.physics_system->GetBodyInterface();

    // Система для синхронизации (DOD подход: трансформация данных)
    app_data.ecs_world->query<Position, PhysicsBody>()
        .each([&body_interface](flecs::entity e, Position& pos, PhysicsBody& phys) {
            if (phys.body_id.IsInvalid()) return;

            // Получаем позицию из физики
            JPH::RVec3 body_pos = body_interface.GetCenterOfMassPosition(phys.body_id);

            // Конвертируем из JoltPhysics (Y-Up) в нашу систему
            pos.value = glm::vec3(
                static_cast<float>(body_pos.GetX()),
                static_cast<float>(body_pos.GetY()), // Y-Up сохраняется
                static_cast<float>(body_pos.GetZ())
            );
        });
}
```

### Часть 9: Упрощённый рендеринг

```cpp
void render_frame(ApplicationData& app_data) {
    // Очистка экрана (упрощённо)
    // В реальном проекте здесь будет Vulkan рендеринг

    // Для демонстрации - вывод в консоль
    static int frame = 0;
    if (frame++ % 60 == 0 && app_data.ecs_world) {
        app_data.ecs_world->query<Position, const Renderable>()
            .each([](flecs::entity e, Position& pos) {
                std::cout << e.name() << " position: ("
                          << pos.value.x << ", "
                          << pos.value.y << ", "
                          << pos.value.z << ")" << std::endl;
            });
    }
}
```

### Часть 10: Очистка ресурсов

```cpp
void cleanup(ApplicationData& app_data) {
    // Очистка физики
    if (app_data.physics_system) {
        delete app_data.physics_system;
        app_data.physics_system = nullptr;
    }

    if (JPH::Factory::sInstance) {
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }

    // Очистка ECS
    app_data.ecs_world.reset();

    // Очистка Vulkan
    if (app_data.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(app_data.instance, app_data.surface, nullptr);
        app_data.surface = VK_NULL_HANDLE;
    }

    if (app_data.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(app_data.instance, nullptr);
        app_data.instance = VK_NULL_HANDLE;
    }

    // Очистка SDL
    if (app_data.window) {
        SDL_DestroyWindow(app_data.window);
        app_data.window = nullptr;
    }

    SDL_Quit();
}
```

### Часть 11: Главная функция

```cpp
int main(int argc, char* argv[]) {
    std::cout << "=== ProjectV First Scene ===" << std::endl;

    ApplicationData app_data;

    // Инициализация (DOD подход: последовательная трансформация данных)
    // Используем std::expected вместо исключений (см. 11_banned-features.md)
    
    if (!init_sdl(app_data)) {
        std::cerr << "SDL initialization failed" << std::endl;
        return 1;
    }
    std::cout << "SDL initialized" << std::endl;

    if (!init_vulkan(app_data)) {
        std::cerr << "Vulkan initialization failed" << std::endl;
        cleanup(app_data);
        return 1;
    }
    std::cout << "Vulkan initialized" << std::endl;

    if (auto result = init_flecs(app_data); !result) {
        std::cerr << "Flecs initialization failed" << std::endl;
        cleanup(app_data);
        return 1;
    }
    std::cout << "Flecs ECS initialized" << std::endl;

    if (!init_joltphysics(app_data)) {
        std::cerr << "JoltPhysics initialization failed" << std::endl;
        cleanup(app_data);
        return 1;
    }
    std::cout << "JoltPhysics initialized" << std::endl;

    // Создание сцены
    create_scene(app_data);
    std::cout << "Scene created" << std::endl;

    // Запуск игрового цикла
    std::cout << "Starting game loop..." << std::endl;
    game_loop(app_data);

    // Очистка
    cleanup(app_data);
    std::cout << "Cleanup completed. Goodbye!" << std::endl;

    return 0;
}
```

## Шаг 3: Шейдеры Vulkan

### triangle.vert

```glsl
#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
}
```

### triangle.frag

```glsl
#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

## Шаг 4: Сборка и запуск

### Компиляция

```bash
# Создаем директорию сборки
mkdir build && cd build

# Конфигурируем CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Собираем
cmake --build . --config Release

# Запускаем
./first_scene
```

### Ожидаемый вывод

```
=== ProjectV First Scene ===
SDL initialized
Vulkan initialized
Flecs ECS initialized
JoltPhysics initialized
Scene created
Starting game loop...
Cube position: (0.0, 5.0, 0.0)
Floor position: (0.0, -2.0, 0.0)
Cube position: (0.0, 4.9, 0.0)  # Кубик падает
Cube position: (0.0, 4.8, 0.0)
...
Cube position: (0.0, 0.5, 0.0)  # Кубик упал на пол
```

## Шаг 5: Добавление ImGui для Debug интерфейса

### Обновление CMakeLists.txt

```cmake
# Добавляем ImGui
find_package(imgui REQUIRED)

# Обновляем линковку
target_link_libraries(first_scene PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    flecs::flecs
    JoltPhysics::JoltPhysics
    glm::glm
    imgui::imgui
)
```

### Обновление ApplicationData

```cpp
// В структуру ApplicationData добавляем:
struct ApplicationData {
    // ... существующие поля ...

    // ImGui
    bool show_debug_window = true;
    float physics_time_ms = 0.0f;
    int entity_count = 0;
};
```

## Шаг 6: Ссылки на документацию ProjectV

Этот пример основан на quickstart руководствах:

- **SDL window**: [SDL Quickstart](../libraries/sdl/01_quickstart.md)
- **Vulkan triangle**: [Vulkan Quickstart](../libraries/vulkan/01_quickstart.md)
- **Flecs ECS**: [Flecs Quickstart](../libraries/flecs/01_quickstart.md)
- **JoltPhysics**: [JoltPhysics Quickstart](../libraries/joltphysics/01_quickstart.md)
- **ImGui**: [ImGui Quickstart](../libraries/imgui/01_quickstart.md)

## Отладка и устранение неполадок

### Распространённые проблемы

1. **Vulkan validation errors**: Убедитесь, что установлен Vulkan SDK и драйверы GPU актуальны.
2. **SDL не создаёт окно**: Проверьте, что SDL3 собран с Vulkan поддержкой.
3. **JoltPhysics не инициализируется**: Убедитесь, что JoltPhysics собран как shared library.
4. **Flecs ошибки компиляции**: Требуется C++17 или выше.

### Советы по отладке

- Используйте `std::cout` для вывода состояния на каждом этапе инициализации.
- Проверяйте возвращаемые значения всех функций инициализации.
- Для Vulkan используйте validation layers: `VK_LAYER_KHRONOS_validation`.
- Для профилирования используйте [Tracy Quickstart](../libraries/tracy/01_quickstart.md).
