# Интеграция flecs

🟡 **Уровень 2: Средний**

Подключение flecs к проекту, CMake, модули и addons.

## CMake

### Добавление как подпроект

```cmake
cmake_minimum_required(VERSION 3.10)
project(MyApp)

set(CMAKE_CXX_STANDARD 17)

# Добавляем flecs как подмодуль Git
add_subdirectory(external/flecs)

add_executable(MyApp src/main.cpp)
target_link_libraries(MyApp PRIVATE flecs::flecs_static)
```

### Цели

| Цель                  | Описание                       |
|-----------------------|--------------------------------|
| `flecs::flecs_static` | Статическая библиотека         |
| `flecs::flecs`        | Статическая библиотека (алиас) |
| `flecs::flecs_shared` | Динамическая библиотека        |

### Зависимости

Flecs автоматически подтягивает платформенные зависимости:

- Windows: `ws2_32`, `dbghelp`
- Linux: `pthread`

---

## Include

Для C++ и C API достаточно одного заголовка:

```cpp
#include <flecs.h>
```

При включении в C++ автоматически подтягивается C++ API из `flecs/addons/cpp/flecs.hpp`.

---

## Модули

Модули позволяют организовать код в переиспользуемые блоки.

### C++ модуль

```cpp
struct game_module {
    game_module(flecs::world& ecs) {
        ecs.module<game_module>();

        // Регистрация компонентов
        ecs.component<Position>();
        ecs.component<Velocity>();

        // Регистрация систем
        ecs.system<Position, Velocity>("Move")
            .each([](Position& p, const Velocity& v) {
                p.x += v.x;
                p.y += v.y;
            });
    }
};

// Импорт модуля
int main() {
    flecs::world ecs;
    ecs.import<game_module>();

    // Теперь доступны компоненты и системы модуля
    ecs.entity().set<Position>({0, 0}).set<Velocity>({1, 0});

    while (ecs.progress()) {}
}
```

### C модуль

```c
// MyModule.h
void MyModuleImport(ecs_world_t *world);

// MyModule.c
#include "MyModule.h"

void MyModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, MyModule);

    ECS_COMPONENT_DEFINE(world, Position);
    ECS_COMPONENT_DEFINE(world, Velocity);

    ECS_SYSTEM(world, Move, EcsOnUpdate, Position, Velocity);
}

// main.c
#include "MyModule.h"

int main() {
    ecs_world_t *world = ecs_init();
    ECS_IMPORT(world, MyModule);

    while (ecs_progress(world, 0)) {}
    ecs_fini(world);
}
```

---

## Addons

Flecs включает дополнительные модули (addons):

| Addon             | Описание                           |
|-------------------|------------------------------------|
| `flecs::c`        | C API                              |
| `flecs::cpp`      | C++ API                            |
| `flecs::rest`     | REST API для удалённого управления |
| `flecs::meta`     | Метаданные компонентов             |
| `flecs::json`     | Сериализация в JSON                |
| `flecs::units`    | Единицы измерения                  |
| `flecs::timer`    | Таймеры и интервалы                |
| `flecs::system`   | Системы                            |
| `flecs::pipeline` | Pipeline                           |
| `flecs::query`    | Queries                            |

### Кастомная сборка

Для уменьшения размера можно собрать только нужные addons:

```cmake
set(FLECS_CUSTOM_BUILD ON CACHE BOOL "" FORCE)
add_subdirectory(external/flecs)
```

Подробнее: [BuildingFlecs.md](../../external/flecs/docs/BuildingFlecs.md)

---

## Порядок вызовов

### Типичный жизненный цикл

1. Создать `flecs::world`
2. Зарегистрировать компоненты и системы
3. Создать entities
4. В цикле: `world.progress(delta_time)`
5. При выходе: world уничтожается автоматически

```cpp
int main() {
    // 1. Создание world
    flecs::world ecs;

    // 2. Регистрация систем
    ecs.system<Position, Velocity>().each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });

    // 3. Создание entities
    ecs.entity().set<Position>({0, 0}).set<Velocity>({1, 0});

    // 4. Игровой цикл
    while (ecs.progress(1.0f / 60.0f)) {
        // Выход по ecs.quit()
    }

    // 5. World уничтожается автоматически (RAII)
    return 0;
}
```

### Выход из цикла

```cpp
// В любом месте
ecs.quit();

// Или через условие
while (should_run && ecs.progress()) {}
```

---

## Интеграция с внешним циклом

Flecs можно интегрировать в любой цикл обновления:

### Пример: пользовательский цикл

```cpp
flecs::world ecs;
setup_systems(ecs);
create_entities(ecs);

while (!should_exit) {
    // Внешние обновления
    process_input();

    // ECS обновление
    ecs.progress(delta_time);

    // Внешняя отрисовка
    render();
}
```

### Пример: фиксированный timestep

```cpp
flecs::world ecs;
double accumulator = 0.0;
const double fixed_dt = 1.0 / 60.0;

while (!should_exit) {
    double frame_time = get_frame_time();
    accumulator += frame_time;

    while (accumulator >= fixed_dt) {
        ecs.progress(fixed_dt);
        accumulator -= fixed_dt;
    }

    render();
}
```

---

## Многопоточность

### Включение многопоточности

```cpp
flecs::world ecs;

// Включить пул потоков
ecs.set_threads(4);

// Система с параллельным выполнением
ecs.system<Position, Velocity>()
    .multi_threaded()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });
```

### Ограничения

- Системы с `immediate = true` не могут быть `multi_threaded`
- Доступ к общим данным требует синхронизации
- Изменение структуры ECS (создание/удаление entities) должно выполняться в основном потоке или через `ecs_defer`

---

## Отладка

### FLECS_DEBUG

Включает дополнительные проверки и assert'ы:

```cmake
target_compile_definitions(MyApp PRIVATE FLECS_DEBUG)
```

### Flecs Explorer

Веб-инструмент для просмотра состояния ECS:

- Entities и компоненты
- Иерархии
- Systems и queries
- Производительность

Требует REST addon: [flecs.dev/explorer](https://flecs.dev/explorer)
