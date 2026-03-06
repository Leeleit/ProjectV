# Быстрый старт flecs

**🟢 Уровень 1: Начинающий**

**Примеры:** [flecs_sdl.cpp](../examples/flecs_sdl.cpp)

## Шаг 1: CMakeLists.txt

```cmake
add_subdirectory(external/flecs)

add_executable(YourApp src/main.cpp)
target_link_libraries(YourApp PRIVATE flecs::flecs_static)
```

## Шаг 2: main.cpp (C++)

```cpp
#include "flecs.h"
#include <iostream>

struct Position { float x, y; };
struct Velocity { float x, y; };

int main() {
    flecs::world ecs;

    // Система
    ecs.system<Position, const Velocity>()
        .each([](Position& p, const Velocity& v) {
            p.x += v.x;
            p.y += v.y;
            std::cout << "Moved to {" << p.x << ", " << p.y << "}\n";
        });

    // Сущность
    ecs.entity("MyEntity")
        .set<Position>({10, 20})
        .set<Velocity>({1, 2});

    // Цикл
    while (ecs.progress()) {
        // ...
    }
}
```

## Шаг 3: Иерархия

```cpp
auto parent = ecs.entity("Parent");
auto child = ecs.entity("Child").child_of(parent);
```

## Дальнейшие шаги

1. **Интеграция**: См. [Интеграция](integration.md).
2. **Основные понятия**: См. [Основные понятия](concepts.md).
3. **Справочник API**: См. [API Reference](api-reference.md).
