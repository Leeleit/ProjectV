# Flecs: Entity Component System для C++

> **Для понимания:** Представьте, что Flecs — это высокоорганизованная база данных для вашей игры. Вместо хаотичных
> объектов с разбросанными данными, Flecs хранит всё в аккуратных таблицах, где каждая колонка — это тип данных (
> компонент), а каждая строка — сущность. Системы — это запросы к этой базе данных, которые обрабатывают данные пакетами
> для максимальной производительности.

## 🎯 Что такое Flecs?

**Flecs** — это быстрый и лёгкий фреймворк Entity Component System (ECS), написанный на C с современным C++ API. Он
спроектирован для high-performance приложений, где важны кэш-дружественность, многопоточность и минимальный overhead.

## 📚 Основные концепции ECS

### Entity (Сущность)

> **Для понимания:** Entity — это уникальный идентификатор, как номер паспорта. Сам по себе он не содержит данных,
> только ссылается на них. Представьте, что entity — это пустая карточка в картотеке, к которой можно прикрепить
> различные
> анкеты (компоненты).

```cpp
#include <flecs.h>
#include <print>

int main() {
    flecs::world ecs;

    // Создание entity без имени
    auto entity = ecs.entity();
    std::println("Entity created with id: {}", entity.id());

    // Создание entity с именем
    auto player = ecs.entity("Player");
    std::println("Entity name: {}", player.name());

    return 0;
}
```

### Component (Компонент)

> **Для понимания:** Компонент — это чистые данные без логики, как анкета с полями (имя, возраст, адрес). В Flecs
> компоненты — это обычные структуры C++. Они хранятся отдельно от сущностей в оптимизированных таблицах.

```cpp
#include <flecs.h>
#include <print>

// Компоненты — это обычные структуры
struct Position {
    float x, y, z;
};

struct Velocity {
    float dx, dy, dz;
};

struct Health {
    float current;
    float max;
};

// Тег — компонент без данных (пустая структура)
struct Enemy {};
struct Dead {};

int main() {
    flecs::world ecs;

    // Регистрация происходит автоматически при первом использовании
    auto entity = ecs.entity()
        .set<Position>({10.0f, 5.0f, 0.0f})
        .set<Velocity>({1.0f, 0.0f, 0.0f})
        .set<Health>({100.0f, 100.0f})
        .add<Enemy>();

    // Проверка наличия компонентов
    if (entity.has<Position>()) {
        std::println("Entity has Position component");
    }

    if (entity.has<Enemy>()) {
        std::println("Entity is an Enemy");
    }

    return 0;
}
```

### System (Система)

> **Для понимания:** Система — это чистая функция, которая обрабатывает компоненты. Представьте конвейер на заводе:
> каждый рабочий (система) выполняет одну операцию над деталями (компонентами), которые проходят по конвейеру.

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y, z; };
struct Velocity { float dx, dy, dz; };

int main() {
    flecs::world ecs;

    // Создаём несколько сущностей
    for (int i = 0; i < 5; ++i) {
        ecs.entity()
            .set<Position>({static_cast<float>(i), 0.0f, 0.0f})
            .set<Velocity>({0.5f, 0.0f, 0.0f});
    }

    // Система движения: обновляет Position на основе Velocity
    ecs.system<Position, const Velocity>("MoveSystem")
        .each([](Position& pos, const Velocity& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
            pos.z += vel.dz;
        });

    // Запускаем один кадр симуляции
    ecs.progress();

    // Проверяем результат
    ecs.each<Position>([](flecs::entity e, Position& pos) {
        std::println("Entity {}: Position = ({}, {}, {})",
                     e.id(), pos.x, pos.y, pos.z);
    });

    return 0;
}
```

## 🏗️ Архитектура Flecs

### World (Мир)

> **Для понимания:** World — это контейнер для всех ECS-данных. Это как целый завод со всеми конвейерами, рабочими и
> складами. Один World на приложение.

```cpp
#include <flecs.h>
#include <print>

int main() {
    // Создание мира
    flecs::world ecs;

    // Настройка мира
    ecs.set_threads(4);  // Использовать 4 потока для систем

    // Установка delta_time по умолчанию
    ecs.set_target_fps(60.0f);

    std::println("World created with {} threads",
                 ecs.get_threads());

    return 0;
}
```

### Archetype Storage (SoA)

> **Для понимания:** Flecs использует Structure of Arrays (SoA) хранение. Представьте библиотеку, где все книги одного
> жанра стоят на одной полке, а не разбросаны по разным шкафам. Это позволяет быстро обрабатывать данные пакетами.

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Velocity { float dx, dy; };
struct Health { float value; };

int main() {
    flecs::world ecs;

    // Flecs автоматически создаёт archetype таблицы:
    // Таблица 1: [Position, Velocity, Health]
    // Таблица 2: [Position, Velocity]
    // Таблица 3: [Position, Health]
    // и т.д.

    // Создаём сущности с разными комбинациями компонентов
    ecs.entity().set<Position>({0, 0}).set<Velocity>({1, 0}).set<Health>({100});
    ecs.entity().set<Position>({5, 5}).set<Velocity>({0, 1});
    ecs.entity().set<Position>({10, 10}).set<Health>({50});

    // Каждая комбинация компонентов создаёт отдельную таблицу
    // Данные хранятся в SoA:
    // positions: [ {0,0}, {5,5}, {10,10} ] ← непрерывный массив
    // velocities: [ {1,0}, {0,1} ] ← другой непрерывный массив
    // healths: [ {100}, {50} ] ← третий массив

    std::println("Created entities with different component combinations");

    return 0;
}
```

## 🔍 Query (Запросы)

> **Для понимания:** Query — это способ найти сущности по критериям, как поиск в базе данных. "Найди всех сотрудников из
> отдела маркетинга старше 30 лет".

### Базовые запросы

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Velocity { float dx, dy; };
struct Enemy {};
struct Dead {};

int main() {
    flecs::world ecs;

    // Создаём тестовые данные
    for (int i = 0; i < 10; ++i) {
        auto e = ecs.entity()
            .set<Position>({static_cast<float>(i), static_cast<float>(i)});

        if (i % 2 == 0) {
            e.set<Velocity>({1.0f, 0.0f});
        }

        if (i < 5) {
            e.add<Enemy>();
        }

        if (i == 2) {
            e.add<Dead>();
        }
    }

    // Запрос 1: Все сущности с Position
    auto q1 = ecs.query<Position>();
    std::println("Query 1 - Entities with Position:");
    q1.each([](flecs::entity e, Position& pos) {
        std::println("  Entity {}: ({}, {})", e.id(), pos.x, pos.y);
    });

    // Запрос 2: Position + Velocity
    auto q2 = ecs.query<Position, Velocity>();
    std::println("\nQuery 2 - Entities with Position and Velocity:");
    q2.each([](flecs::entity e, Position& pos, Velocity& vel) {
        std::println("  Entity {}: Pos({}, {}), Vel({}, {})",
                     e.id(), pos.x, pos.y, vel.dx, vel.dy);
    });

    // Запрос 3: Position + Enemy, но без Dead
    auto q3 = ecs.query_builder<Position>()
        .with<Enemy>()
        .without<Dead>()
        .build();

    std::println("\nQuery 3 - Enemies that are not dead:");
    q3.each([](flecs::entity e, Position& pos) {
        std::println("  Entity {}: ({}, {})", e.id(), pos.x, pos.y);
    });

    return 0;
}
```

### Query Builder с продвинутыми возможностями

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Velocity { float dx, dy; };
struct Health { float value; };

int main() {
    flecs::world ecs;

    // Optional компоненты
    auto q1 = ecs.query_builder<Position, Velocity>()
        .term_at(1).optional<Health>()  // Health необязателен
        .build();

    // Итерация с optional компонентом
    q1.each([](flecs::entity e, Position& pos, Velocity& vel, Health* health) {
        if (health) {
            std::println("Entity has health: {}", health->value);
        }
    });

    // Иерархические запросы
    auto parent = ecs.entity("Parent");
    auto child = ecs.entity("Child").child_of(parent);
    child.set<Position>({5.0f, 5.0f});

    // Запрос с обходом иерархии
    auto q2 = ecs.query_builder<Position>()
        .cascade()  // Обход от корня к листьям
        .build();

    return 0;
}
```

## 🚀 Systems (Системы)

### Типы систем

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Velocity { float dx, dy; };

int main() {
    flecs::world ecs;

    // 1. Система с .each() — простой callback для каждой сущности
    ecs.system<Position, Velocity>("MoveEach")
        .each([](Position& pos, const Velocity& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
        });

    // 2. Система с .iter() — batch обработка (быстрее для многих сущностей)
    ecs.system<Position, Velocity>("MoveIter")
        .iter([](flecs::iter& it, Position* pos, Velocity* vel) {
            for (auto i : it) {
                pos[i].x += vel[i].dx * it.delta_time();
                pos[i].y += vel[i].dy * it.delta_time();
            }
        });

    // 3. Многопоточная система
    ecs.system<Position, Velocity>("MoveMultiThreaded")
        .multi_threaded()
        .each([](Position& pos, const Velocity& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
        });

    // 4. Система с условием
    struct Active {};

    ecs.system<Position, Velocity>("MoveIfActive")
        .with<Active>()  // Только сущности с тегом Active
        .each([](Position& pos, const Velocity& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
        });

    return 0;
}
```

### Pipeline и фазы выполнения

> **Для понимания:** Pipeline определяет порядок выполнения систем, как расписание поездов. Каждая система привязана к
> определённой фазе (станции), и поезда (данные) проходят через станции в строгом порядке.

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Velocity { float dx, dy; };
struct Rendered {};

int main() {
    flecs::world ecs;

    // Стандартные фазы Flecs:
    // 1. OnLoad     - Загрузка ресурсов
    // 2. PostLoad   - Постобработка после загрузки
    // 3. PreUpdate  - Подготовка перед обновлением
    // 4. OnUpdate   - Основная логика (по умолчанию)
    // 5. OnValidate - Валидация
    // 6. PostUpdate - Синхронизация после обновления
    // 7. PreStore   - Подготовка к сохранению
    // 8. OnStore    - Финализация

    // Система в фазе PreUpdate
    ecs.system<Position, Velocity>("Physics")
        .kind(flecs::PreUpdate)
        .each([](Position& pos, const Velocity& vel) {
            pos.x += vel.dx;
            pos.y += vel.dy;
        });

    // Система в фазе OnUpdate (по умолчанию)
    ecs.system<Position>("AI")
        .each([](Position& pos) {
            // AI логика
        });

    // Система в фазе PostUpdate
    ecs.system<Position>("Cleanup")
        .kind(flecs::PostUpdate)
        .each([](Position& pos) {
            // Очистка
        });

    // Пользовательская фаза
    auto CustomPhase = ecs.entity("CustomPhase")
        .add(flecs::Phase)
        .add(flecs::DependsOn, flecs::OnUpdate);  // После OnUpdate

    ecs.system<Position>("CustomSystem")
        .kind(CustomPhase)
        .each([](Position& pos) {
            // Пользовательская логика
        });

    // Запуск pipeline
    for (int i = 0; i < 3; ++i) {
        std::println("\n--- Frame {} ---", i + 1);
        ecs.progress(0.016f);  // 60 FPS
    }

    return 0;
}
```

## 👀 Observers (Наблюдатели)

> **Для понимания:** Observer — это реактивный обработчик событий. Он срабатывает не каждый кадр, а только когда
> происходят изменения: добавление компонента, изменение значения, удаление. Как датчик движения, который включает свет
> только когда кто-то проходит.

```cpp
#include <flecs.h>
#include <print>

struct Position { float x, y; };
struct Health { float value; };
struct Enemy {};

int main() {
    flecs::world ecs;

    // Observer для OnAdd: когда компонент добавляется
    ecs.observer<Position>("OnPositionAdded")
        .event(flecs::OnAdd)
        .each([](flecs::entity e, Position& pos) {
            std::println("Position added to entity {}", e.id());
        });

    // Observer для OnSet: когда значение компонента устанавливается/изменяется
    ecs.observer<Position>("OnPositionChanged")
        .event(flecs::OnSet)
        .each([](flecs::entity e, Position& pos) {
            std::println("Position changed for entity {}: ({}, {})",
                         e.id(), pos.x, pos.y);
        });

    // Observer для OnRemove: когда компонент удаляется
    ecs.observer<Health>("OnHealthRemoved")
        .event(flecs::OnRemove)
        .each([](flecs::entity e, Health& health) {
            std::println("Health removed from entity {}, last value: {}",
                         e.id(), health.value);
        });

    // Observer для нескольких событий
    ecs.observer<Enemy>("OnEnemyEvent")
        .event(flecs::OnAdd | flecs::OnRemove)
        .each([](flecs::entity e, Enemy&) {
            if (e.has<Enemy>()) {
                std::println("Enemy added to entity {}", e.id());
            } else {
                std::println("Enemy removed from entity {}", e.id());
            }
        });
    return 0;
}
```
