## Практические рецепты flecs

<!-- anchor: 05_tools -->


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

---

## Производительность flecs

<!-- anchor: 06_performance -->


Оптимизации, многопоточность и unsafe доступ.

## Cached vs Uncached Queries

### Cached (по умолчанию)

- Быстрая итерация (archetype list кешируется)
- Больше памяти
- Дольше создание
- Подходит для систем и частых запросов

```cpp
// Cached query (по умолчанию)
auto q = ecs.query_builder<Position, Velocity>().build();
```

### Uncached

- Медленная итерация (поиск archetypes каждый раз)
- Меньше памяти
- Быстрое создание
- Подходит для редких ad-hoc запросов

```cpp
auto q = ecs.query_builder<Position, Velocity>()
    .cache_kind(flecs::QueryCacheNone)
    .build();
```

### Когда что использовать

| Сценарий                     | Тип query |
|------------------------------|-----------|
| Система (каждый кадр)        | Cached    |
| Частый поиск                 | Cached    |
| Разовый запрос (найти детей) | Uncached  |
| Редкая операция              | Uncached  |

---

## Итерация

### each vs iter

```cpp
// each — callback по одной entity
// Проще писать, но overhead на каждый вызов
q.each([](Position& p, Velocity& v) {
    p.x += v.x;
});

// iter — batch итерация
// Быстрее для больших объёмов
q.iter([](flecs::iter& it, Position* p, Velocity* v) {
    for (auto i : it) {
        p[i].x += v[i].x;
    }
});
```

**Рекомендация:** Используйте `iter` для hot paths и больших объёмов данных.

---

## Многопоточность

### Включение

```cpp
flecs::world ecs;
ecs.set_threads(4);  // Пул из 4 потоков
```

### Параллельные системы

```cpp
ecs.system<Position, Velocity>()
    .multi_threaded()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });
```

### Ограничения

- Системы с `immediate = true` не могут быть `multi_threaded`
- Изменение структуры ECS (добавление/удаление компонентов, создание/удаление entities) должно быть отложено через
  `defer`
- Доступ к общим данным требует синхронизации

### Deferred operations в многопоточности

```cpp
ecs.system<Position>()
    .multi_threaded()
    .iter([](flecs::iter& it, Position* p) {
        // Изменения структуры ECS откладываются
        it.world().defer_begin();

        for (auto i : it) {
            if (p[i].x > 100) {
                it.entity(i).add<Dead>();  // Отложено
            }
        }

        it.world().defer_end();  // Применится после итерации
    });
```

---

## Unsafe Access

Для максимальной производительности можно обойти проверки:

```cpp
// Безопасный доступ (по умолчанию)
Position* p = e.get_mut<Position>();

// Unsafe доступ (быстрее, но без проверок)
Position* p = e.get_mut_unsafe<Position>();
```

### Когда использовать unsafe

- Горячие циклы с миллионами итераций
- Гарантированно валидные entities
- После профилирования

**Предупреждение:** Unsafe доступ к невалидной entity = undefined behavior.

---

## Archetype Layout

### Советы по компонентам

```cpp
// Плохо: большой компонент
struct Unit {
    Position pos;
    Velocity vel;
    Health health;
    Inventory inventory;  // Может быть большим
    std::string name;
};

// Хорошо: разделение на компоненты
struct Position { float x, y, z; };
struct Velocity { float x, y, z; };
struct Health { float current, max; };
struct Inventory { /* ... */ };
struct Name { std::string value; };
```

**Принцип:** Компоненты должны быть компактными. Большие данные выносите в отдельные компоненты.

---

## Память

### Таблицы (Archetypes)

Каждая уникальная комбинация компонентов создаёт новую таблицу.

```cpp
// 1 таблица: [Position, Velocity]
ecs.entity().set<Position>({}).set<Velocity>({});

// 2 таблицы: [Position, Velocity, Health] и [Position, Velocity]
ecs.entity().set<Position>({}).set<Velocity>({}).set<Health>({});
```

### Советы

- Избегайте избыточного дробления таблиц
- Группируйте часто используемые вместе компоненты
- Используйте теги для маркировки без данных

---

## Batch операции

### Bulk creation

```cpp
// Медленно
for (int i = 0; i < 10000; i++) {
    ecs.entity().set<Position>({i * 1.0f, 0});
}

// Быстро
auto bulk = ecs.bulk_create()
    .add<Position>()
    .create(10000);
```

### Deferred operations

```cpp
ecs.defer_begin();

for (int i = 0; i < 1000; i++) {
    auto e = ecs.entity();
    e.set<Position>({i, 0});
    e.set<Velocity>({1, 0});
}

ecs.defer_end();  // Одно обновление archetype
```

---

## Pipeline оптимизации

### Порядок систем

Системы внутри фазы выполняются в порядке объявления. Критичные системы объявляйте первыми.

```cpp
// Критичная система
ecs.system<Input>("ProcessInput")
    .kind(flecs::PreUpdate)
    .each([](Input& i) { });

// Менее критичная
ecs.system<AI>("ProcessAI")
    .kind(flecs::PreUpdate)
    .each([](AI& a) { });
```

### Кастомные фазы

```cpp
// Фаза после OnUpdate
auto PostProcess = ecs.entity()
    .add(flecs::Phase)
    .add(flecs::DependsOn, flecs::OnUpdate);

ecs.system<Position>("Clamp")
    .kind(PostProcess)
    .each([](Position& p) { });
```

---

## Профилирование

### Tracy

Flecs поддерживает интеграцию с Tracy:

```cpp
#define FLECS_TRACY 1
#include <flecs.h>
```

### Ручное профилирование

```cpp
auto start = std::chrono::high_resolution_clock::now();

q.each([](Position& p) { });

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
```

### Flecs Explorer

Веб-инструмент для анализа производительности:

- Время выполнения систем
- Количество entities в archetypes
- Использование памяти

Требует REST addon: [flecs.dev/explorer](https://flecs.dev/explorer)

---

## Типичные bottleneck'и

| Проблема                 | Решение                             |
|--------------------------|-------------------------------------|
| Медленная итерация       | Использовать `iter` вместо `each`   |
| Много таблиц             | Уменьшить вариативность компонентов |
| Частые создания/удаления | Использовать пулы объектов          |
| Конкуренция за данные    | Разделить на независимые компоненты |
| Много singleton-запросов | Кешировать указатель                |

---

## Чеклист оптимизации

1. Использовать cached queries для частых запросов
2. Использовать `iter` для горячих путей
3. Включить `multi_threaded` для независимых систем
4. Компактные компоненты без лишних данных
5. Batch операции для массового создания
6. Профилировать перед оптимизацией

---

## Решение проблем flecs

<!-- anchor: 07_troubleshooting -->


Частые ошибки и способы их исправления.

## Сборка

### undefined reference to ecs_* / flecs::*

**Причина:** flecs не линкован с приложением.

**Решение:**

```cmake
add_subdirectory(external/flecs)
target_link_libraries(MyApp PRIVATE flecs::flecs_static)
```

### Не найден flecs.h

**Причина:** Include-путь не добавлен.

**Решение:**

1. Убедитесь, что `flecs::flecs_static` в `target_link_libraries` — include передаётся транзитивно.
2. Используйте `#include <flecs.h>` (не `flecs/flecs.h`).

### C++17 требуется

**Причина:** C++ API flecs требует C++17.

**Решение:**

```cmake
set(CMAKE_CXX_STANDARD 17)
```

---

## Runtime

### Query возвращает 0 entities

**Причина:** Query матчит только entities, удовлетворяющие всем terms.

**Решение:**

1. Проверьте, что у entity есть **все** компоненты из query:
   ```cpp
   entity.has<Position>();  // true?
   entity.type();           // Список компонентов
   ```

2. Проверьте операторы: `without<T>()` исключает entities с компонентом.
3. Для singleton: `world.set<Gravity>({9.81})` — иначе query с Gravity не матчит.

### Система не выполняется

**Причина:** Entities не матчат query системы.

**Решение:**

```cpp
// Система: Position + Velocity
ecs.system<Position, Velocity>()...

// Entity должна иметь ОБА компонента
e.set<Position>({}).set<Velocity>({});
```

### Использование удалённой entity

**Причина:** После `entity.destruct()` id переиспользуется. Старая ссылка невалидна.

**Решение:**

```cpp
if (e.is_alive()) {
    // Безопасно использовать
}
```

**Отладка:** Включите `FLECS_DEBUG` для assert при обращении к удалённой entity.

---

## Логика

### ecs_quit не срабатывает

**Причина:** `ecs_progress` возвращает `true` до вызова `ecs_quit`.

**Решение:**

```cpp
// Из системы или observer
ecs.quit();

// Следующий progress() вернёт false
while (ecs.progress()) {}
```

### Компонент не найден / не зарегистрирован

**Причина:** В C компоненты нужно регистрировать явно. В C++ регистрация автоматическая при первом использовании.

**Решение C:**

```c
ECS_COMPONENT(world, Position);
ecs_set(world, e, Position, {10, 20});
```

**Решение C++:**

```cpp
// Регистрация происходит автоматически
e.set<Position>({10, 20});
```

### Дублирование entity по имени

**Причина:** `world.entity("Name")` возвращает существующую entity с таким именем, а не создаёт новую.

**Решение:** Используйте уникальные имена или scope:

```cpp
world.entity("Enemy1");
world.entity("Enemy2");

// Или иерархия
auto wave1 = world.entity("Wave1");
world.entity("Enemy").child_of(wave1);
```

---

## Производительность

### Медленная итерация query

**Причина:** Uncached query ищет archetypes при каждой итерации.

**Решение:** Используйте cached query (по умолчанию) для частых запросов.

```cpp
// Cached (по умолчанию)
auto q = ecs.query_builder<Position>().build();

// Uncached — только для редких запросов
auto q = ecs.query_builder<Position>()
    .cache_kind(flecs::QueryCacheNone)
    .build();
```

### Много таблиц (archetypes)

**Причина:** Каждая уникальная комбинация компонентов создаёт новую таблицу.

**Решение:**

- Группируйте часто используемые вместе компоненты
- Используйте теги вместо компонентов-маркеров
- Избегайте избыточной вариативности

---

## Отладка

### FLECS_DEBUG

Включает проверки и assert'ы:

```cmake
target_compile_definitions(MyApp PRIVATE FLECS_DEBUG)
```

### Flecs Explorer

Веб-инструмент для просмотра:

- Entities и компоненты
- Иерархии
- Systems и queries

Требует REST addon: [flecs.dev/explorer](https://flecs.dev/explorer)

### Вывод состояния

```cpp
// Вывести все entities с компонентом
ecs.each<Position>([](flecs::entity e, Position& p) {
    std::cout << e.name() << ": " << p.x << ", " << p.y << "\n";
});

// Тип entity
auto type = e.type();
for (int i = 0; i < type.count(); i++) {
    std::cout << ecs.to_string(type.get(i)) << "\n";
}
```

---

## Многопоточность

### Race condition

**Причина:** Параллельные системы обращаются к общим данным без синхронизации.

**Решение:**

- Разделите данные на независимые компоненты
- Используйте singleton только для чтения
- Синхронизируйте доступ к общим ресурсам

### Изменение структуры ECS из параллельной системы

**Причина:** Добавление/удаление компонентов из `multi_threaded` системы.

**Решение:** Используйте defer:

```cpp
ecs.system<Position>()
    .multi_threaded()
    .iter([](flecs::iter& it, Position* p) {
        it.world().defer_begin();

        for (auto i : it) {
            if (p[i].x > 100) {
                it.entity(i).add<Dead>();  // Отложено
            }
        }

        it.world().defer_end();
    });
```

---

## Частые ошибки

| Ошибка               | Причина                | Решение                     |
|----------------------|------------------------|-----------------------------|
| Crash при entity.get | Entity удалена         | Проверить `is_alive()`      |
| Query пустой         | Не все компоненты      | Проверить `entity.has<T>()` |
| Система не работает  | Entity не матчит query | Добавить нужные компоненты  |
| Медленно             | Uncached query         | Использовать cached         |
| Зависание            | Бесконечный цикл       | Проверить условие выхода    |

---

## Чеклист отладки

1. Включить `FLECS_DEBUG`
2. Проверить `entity.is_alive()`
3. Проверить `entity.has<T>()`
4. Вывести `entity.type()`
5. Проверить операторы query (`without`, `optional`)
6. Использовать Flecs Explorer
