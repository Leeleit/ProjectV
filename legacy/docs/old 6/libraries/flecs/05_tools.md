# Практические рецепты flecs

🟡 **Уровень 2: Средний**

Распространённые паттерны и рецепты использования flecs.

## Query Builder

### Базовый запрос

```cpp
auto q = ecs.query_builder<Position, Velocity>().build();
```

### С условием Not

```cpp
// Entities с Position, но без Dead
auto q = ecs.query_builder<Position>()
    .without<Dead>()
    .build();
```

### С условием Optional

```cpp
// Velocity необязателен
auto q = ecs.query_builder<Position, Velocity>()
    .term_at(1).optional()
    .build();

q.each([](flecs::entity e, Position& p, Velocity* v) {
    // v может быть nullptr
});
```

### С pair

```cpp
// Все дети parent
auto q = ecs.query_builder<>()
    .with(flecs::ChildOf, parent)
    .build();
```

### С wildcard

```cpp
// Все entity с любым pair (Likes, *)
auto q = ecs.query_builder<>()
    .with(Likes, flecs::Wildcard)
    .build();
```

### Cascade (обход иерархии)

```cpp
// Обход от корня к листьям
auto q = ecs.query_builder<Transform>()
    .cascade()
    .build();
```

---

## Observers

### Инициализация компонента

```cpp
ecs.observer<MeshComponent>()
    .event(flecs::OnSet)
    .each([](flecs::entity e, MeshComponent& mesh) {
        // Загрузка GPU ресурса при установке компонента
        mesh.handle = load_mesh(mesh.path);
    });
```

### Освобождение ресурса

```cpp
ecs.observer<MeshComponent>()
    .event(flecs::OnRemove)
    .each([](flecs::entity e, MeshComponent& mesh) {
        // Освобождение GPU ресурса при удалении компонента
        unload_mesh(mesh.handle);
    });
```

### Реакция на несколько событий

```cpp
ecs.observer<Position>()
    .event(flecs::OnAdd | flecs::OnSet)
    .each([](flecs::entity e, Position& p) {
        // Компонент добавлен или изменён
    });
```

### Передача контекста

```cpp
GPUContext ctx{device, allocator};

ecs.observer<MeshComponent>()
    .event(flecs::OnSet)
    .ctx(&ctx)
    .each([](flecs::entity e, MeshComponent& mesh) {
        auto ctx = e.world().ctx<GPUContext>();
        mesh.handle = load_mesh(ctx, mesh.path);
    });
```

---

## Prefabs

### Создание иерархии prefab

```cpp
// Базовый юнит
auto Unit = ecs.prefab("Unit")
    .set<Health>({100})
    .set<Speed>({5.0f});

// Tank наследует от Unit
auto Tank = ecs.prefab("Tank")
    .is_a(Unit)
    .set<Health>({200})      // Override
    .set<Armor>({50});

// Создание instance
auto my_tank = ecs.entity().is_a(Tank);
```

### Prefab с children

```cpp
auto Car = ecs.prefab("Car")
    .set<Speed>({10.0f});

// Ребёнок prefab
auto Wheel = ecs.prefab("Wheel");
auto wheel1 = ecs.prefab().child_of(Car).is_a(Wheel);
auto wheel2 = ecs.prefab().child_of(Car).is_a(Wheel);

// Instance наследует детей
auto my_car = ecs.entity().is_a(Car);
```

---

## Модули

### Структура модуля

```cpp
// physics_module.h
#pragma once
#include <flecs.h>

struct PhysicsModule {
    PhysicsModule(flecs::world& ecs);
};

// physics_module.cpp
#include "physics_module.h"

struct Velocity { float x, y; };
struct Mass { float value; };

PhysicsModule::PhysicsModule(flecs::world& ecs) {
    ecs.module<PhysicsModule>();
    
    ecs.system<Velocity, Mass>("ApplyGravity")
        .kind(flecs::OnUpdate)
        .each([](Velocity& v, const Mass& m) {
            v.y -= 9.81f * m.value;
        });
}

// main.cpp
#include "physics_module.h"

int main() {
    flecs::world ecs;
    ecs.import<PhysicsModule>();
    
    // Компоненты модуля доступны
    ecs.entity().set<Velocity>({0, 0}).set<Mass>({1.0f});
    
    while (ecs.progress()) {}
}
```

---

## Singleton

### Глобальные настройки

```cpp
struct GameConfig {
    float gravity = 9.81f;
    float time_scale = 1.0f;
    bool debug_mode = false;
};

// Установка
ecs.set<GameConfig>({9.81f, 1.0f, false});

// Использование в системе
ecs.system<Velocity>()
    .each([](Velocity& v) {
        const GameConfig* cfg = ecs.get<GameConfig>();
        v.y -= cfg->gravity * cfg->time_scale;
    });
```

---

## Иерархии

### Transform propagation

```cpp
struct LocalTransform { float x, y; };
struct WorldTransform { float x, y; };

// Система для обновления world transform
ecs.system<LocalTransform, WorldTransform>()
    .kind(flecs::PreUpdate)
    .term_at(1).parent()  // WorldTransform от родителя
    .each([](flecs::entity e, LocalTransform& local, WorldTransform& parent_world) {
        // Получаем свой WorldTransform
        auto* world = e.get_mut<WorldTransform>();
        world->x = parent_world.x + local.x;
        world->y = parent_world.y + local.y;
    });
```

### Удаление с детьми

```cpp
// По умолчанию дети удаляются с родителем
auto parent = ecs.entity("Parent");
auto child = ecs.entity("Child").child_of(parent);

parent.destruct();  // Удалит и child
```

---

## Batch создание

### Создание множества entities

```cpp
// Медленно: множество отдельных вызовов
for (int i = 0; i < 10000; i++) {
    ecs.entity().set<Position>({i * 1.0f, 0});
}

// Быстро: bulk creation
auto bulk = ecs.bulk_create()
    .add<Position>()
    .create(10000);
```

---

## Deferred operations

### Отложенные изменения

```cpp
ecs.defer_begin();

for (int i = 0; i < 1000; i++) {
    auto e = ecs.entity();
    e.set<Position>({i, 0});
    e.set<Velocity>({1, 0});
}

ecs.defer_end();  // Применит все изменения разом
```

---

## Phases

### Пользовательская фаза

```cpp
// Создание фазы
auto MyPhase = ecs.entity().add(flecs::Phase);

// Зависимость от другой фазы
ecs.entity()
    .add(flecs::Phase)
    .add(flecs::DependsOn, flecs::OnUpdate);  // После OnUpdate

// Использование в системе
ecs.system<Position>("MySystem")
    .kind(MyPhase)
    .each([](Position& p) { });
```

---

## Delta time

### Доступ в системе

```cpp
ecs.system<Velocity>()
    .each([](flecs::iter& it, size_t i, Velocity& v) {
        float dt = it.delta_time();
        // или использовать v.x *= dt; при each
    });

// Через each
ecs.system<Velocity>()
    .each([](Velocity& v) {
        // delta_time недоступен напрямую
        // используйте iter()
    });
```

---

## Выход из игры

### Из системы

```cpp
ecs.system<GameState>()
    .kind(flecs::OnUpdate)
    .each([](flecs::entity e, GameState& state) {
        if (state.should_quit) {
            e.world().quit();
        }
    });
```

### Из внешнего кода

```cpp
while (should_run && ecs.progress()) {}
// ecs.quit() → progress() вернёт false
```

---

## Поиск entity

### По имени

```cpp
auto e = ecs.lookup("Player");
auto e = ecs.lookup("Parent::Child");
```

### По компоненту

```cpp
// Первая entity с компонентом
auto e = ecs.lookup<GameState>();

// Все entities с компонентом
ecs.each<GameState>([](flecs::entity e, GameState& state) { });
```

---

## Проверка типа entity

```cpp
if (e.has<Position>() && e.has<Velocity>()) {
    // ...
}

if (e.is_a<Tank>()) {
    // Instance prefab Tank
}

if (e.has(flecs::ChildOf, parent)) {
    // Ребёнок parent
}