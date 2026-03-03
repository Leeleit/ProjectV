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

    // Создаём несколько сущности
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

## 🔗 Pairs и Relationships (Пары и отношения)

> **Для понимания:** Пары — это мощная концепция Flecs, позволяющая создавать отношения между сущностями. Это как
> соединительные кабели между устройствами: (USB, Принтер), (Сеть, Сервер), (Владелец, Собака). Первый элемент —
> отношение, второй — цель.

### Базовые пары

```cpp
#include <flecs.h>
#include <print>

struct Eats {};
struct Grows {};

int main() {
    flecs::world ecs;

    // Создание сущностей для еды
    auto apples = ecs.entity("Apples");
    auto pears = ecs.entity("Pears");

    // Создание сущности с отношениями
    auto bob = ecs.entity("Bob")
        .add<Eats>(apples)      // Пара (Eats, Apples)
        .add<Eats>(pears)       // Пара (Eats, Pears)
        .add<Grows>(pears);     // Пара (Grows, Pears)

    // Проверка отношений
    std::println("Bob eats apples? {}", bob.has<Eats>(apples));
    std::println("Bob eats pears? {}", bob.has<Eats>(pears));
    std::println("Bob grows pears? {}", bob.has<Grows>(pears));

    // Получение целей отношений
    std::println("Bob eats: {}", bob.target<Eats>().name());
    std::println("Bob also eats: {}", bob.target<Eats>(1).name());

    return 0;
}
```

### Пары с данными (Relationship Components)

```cpp
#include <flecs.h>
#include <print>

// Компонент для отношения "Требует"
struct Requires {
    float amount;
};

// Тег для типа энергии
struct Gigawatts {};

int main() {
    flecs::world ecs;

    // Создание пары с данными: (Requires, Gigawatts)
    auto e1 = ecs.entity()
        .set<Requires, Gigawatts>({1.21f});

    // Получение данных из пары
    const Requires* r = e1.get<Requires, Gigawatts>();
    std::println("Requires: {} gigawatts", r->amount);

    // Пара с компонентом во второй позиции: (Gigawatts, Requires)
    auto e2 = ecs.entity()
        .set<Gigawatts, Requires>({1.21f});

    // Получение данных из пары (вторая позиция)
    const Requires* r2 = e2.get_second<Gigawatts, Requires>();
    std::println("Also requires: {} gigawatts", r2->amount);

    return 0;
}
```

### Встроенные отношения

Flecs предоставляет несколько встроенных отношений:

```cpp
#include <flecs.h>
#include <print>

int main() {
    flecs::world ecs;

    // 1. ChildOf - иерархия родитель-потомок
    auto parent = ecs.entity("Parent");
    auto child = ecs.entity("Child")
        .child_of(parent);  // Эквивалент add(flecs::ChildOf, parent)

    std::println("Child is child of parent? {}",
                 child.has(flecs::ChildOf, parent));

    // 2. IsA - наследование (префабы)
    auto prefab = ecs.prefab("Spaceship")
        .set<Position>({0, 0})
        .set<Velocity>({1, 0});

    auto instance = ecs.entity("MySpaceship")
        .is_a(prefab);  // Эквивалент add(flecs::IsA, prefab)

    // Instance наследует компоненты от prefab
    std::println("Instance has Position? {}", instance.has<Position>());
    std::println("Instance has Velocity? {}", instance.has<Velocity>());

    // 3. DependsOn - зависимости между системами
    auto system1 = ecs.system("System1")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e) { /* логика */ });

    auto system2 = ecs.system("System2")
        .kind(flecs::OnUpdate)
        .depends_on(system1)  // System2 выполнится после System1
        .each([](flecs::entity e) { /* логика */ });

    return 0;
}
```

### Свойства отношений

```cpp
#include <flecs.h>
#include <print>

struct TradesWith {};
struct Platoon {};

int main() {
    flecs::world ecs;

    // 1. Симметричные отношения (Symmetric)
    // Добавление (TradesWith, B) к A автоматически добавляет (TradesWith, A) к B
    ecs.component<TradesWith>().add(flecs::Symmetric);

    auto player1 = ecs.entity("Player1");
    auto player2 = ecs.entity("Player2");

    player1.add<TradesWith>(player2);

    std::println("Player1 trades with Player2? {}",
                 player1.has<TradesWith>(player2));
    std::println("Player2 trades with Player1? {}",
                 player2.has<TradesWith>(player1));

    // 2. Эксклюзивные отношения (Exclusive)
    // Сущность может иметь только одну цель для этого отношения
    ecs.component<Platoon>().add(flecs::Exclusive);

    auto platoon1 = ecs.entity("Platoon1");
    auto platoon2 = ecs.entity("Platoon2");
    auto unit = ecs.entity("Unit");

    unit.add<Platoon>(platoon1);
    std::println("Unit in platoon1? {}", unit.has<Platoon>(platoon1));

    // Добавление в другой взвод автоматически удалит из первого
    unit.add<Platoon>(platoon2);
    std::println("Unit in platoon1? {}", unit.has<Platoon>(platoon1));
    std::println("Unit in platoon2? {}", unit.has<Platoon>(platoon2));

    // 3. Транзитивные отношения (Transitive)
    // Если A имеет (LocatedIn, B) и B имеет (LocatedIn, C),
    // то A считается имеющим (LocatedIn, C)
    struct LocatedIn {};
    ecs.component<LocatedIn>().add(flecs::Transitive);

    auto earth = ecs.entity("Earth");
    auto europe = ecs.entity("Europe").add<LocatedIn>(earth);
    auto netherlands = ecs.entity("Netherlands").add<LocatedIn>(europe);
    auto amsterdam = ecs.entity("Amsterdam").add<LocatedIn>(netherlands);

    // Запрос найдет amsterdam через транзитивность
    auto q = ecs.query_builder<LocatedIn>()
        .term_at(0).second(earth)  // Ищем (LocatedIn, Earth)
        .build();

    q.each([](flecs::entity e, LocatedIn&) {
        std::println("{} is located in Earth", e.name());
    });

    return 0;
}
```

### Запросы с парами

```cpp
#include <flecs.h>
#include <print>

struct Eats {};
struct Requires { float amount; };

int main() {
    flecs::world ecs;

    // Создание тестовых данных
    auto apples = ecs.entity("Apples");
    auto pears = ecs.entity("Pears");
    auto gigawatts = ecs.entity("Gigawatts");

    ecs.entity("Bob")
        .add<Eats>(apples)
        .add<Eats>(pears)
        .set<Requires, Gigawatts>({1.21f});

    ecs.entity("Alice")
        .add<Eats>(apples)
        .set<Requires, Gigawatts>({0.5f});

    // 1. Запрос с конкретной парой
    auto q1 = ecs.query_builder<Eats>()
        .term_at(0).second(apples)  // Только (Eats, Apples)
        .build();

    std::println("Entities that eat apples:");
    q1.each([](flecs::entity e, Eats&) {
        std::println("  {}", e.name());
    });

    // 2. Запрос с wildcard (любая цель)
    auto q2 = ecs.query_builder<Eats>()
        .term_at(0).second(flecs::Wildcard)  // Любая цель
        .build();

    std::println("\nAll eating relationships:");
    q2.each([](flecs::iter& it, size_t index, Eats&) {
        flecs::entity e = it.entity(index);
        flecs::entity food = it.pair(0).second();
        std::println("  {} eats {}", e.name(), food.name());
    });

    // 3. Запрос с парой-компонентом
    auto q3 = ecs.query_builder<Requires>()
        .term_at(0).second<Gigawatts>()  // (Requires, Gigawatts)
        .build();

    std::println("\nEntities that require gigawatts:");
    q3.each([](Requires& rq) {
        std::println("  Requires: {} gigawatts", rq.amount);
    });

    // 4. Запрос с optional парой
    auto q4 = ecs.query_builder<Eats>()
        .term_at(0).second(pears).optional()  // (Eats, Pears) необязательно
        .build();

    std::println("\nEntities (with optional pear eating):");
    q4.each([](flecs::entity e, Eats*) {
        std::println("  {}", e.name());
    });

    return 0;
}
```

### Итерация по парам сущности

```cpp
#include <flecs.h>
#include <print>

struct Eats {};
struct Grows {};

int main() {
    flecs::world ecs;

    auto apples = ecs.entity("Apples");
    auto pears = ecs.entity("Pears");

    auto bob = ecs.entity("Bob")
        .add<Eats>(apples)
        .add<Eats>(pears)
        .add<Grows>(pears);

    // 1. Итерация всех пар сущности
    std::println("All pairs of Bob:");
    bob.each([](flecs::id id) {
        if (id.is_pair()) {
            flecs::entity rel = id.first();
            flecs::entity tgt = id.second();
            std::println("  ({}, {})", rel.name(), tgt.name());
        }
    });

    // 2. Итерация пар с конкретным отношением
    std::println("\nWhat Bob eats:");
    bob.each<Eats>([](flecs::entity food) {
        std::println("  {}", food.name());
    });

    // 3. Итерация пар с конкретной целью
    std::println("\nWho grows pears:");
    ecs.each(flecs::Wildcard, pears, [](flecs::id id, flecs::entity e) {
        flecs::entity rel = id.first();
        std::println("  {} grows pears with relation {}", e.name(), rel.name());
    });

    // 4. Получение всех целей для отношения
    std::println("\nAll foods Bob eats:");
    size_t count = bob.target_count<Eats>();
    for (size_t i = 0; i < count; ++i) {
        flecs::entity food = bob.target<Eats>(i);
        std::println("  {}", food.name());
    }

    return 0;
}
```

### Продвинутые паттерны

```cpp
#include <flecs.h>
#include <print>

// Паттерн 1: Слоты (Slots) для префабов
void slots_example(flecs::world& ecs) {
    // Создание префаба космического корабля со слотами
    auto spaceship = ecs.prefab("Spaceship");

    auto engine = ecs.prefab("Engine")
        .child_of(spaceship)
        .slot_of(spaceship);  // Создает слот для двигателя

    auto cockpit = ecs.prefab("Cockpit")
        .child_of(spaceship)
        .slot_of(spaceship);  // Создает слот для кокпита

    // При инстанцировании создаются связи:
    // (Spaceship.Engine, Instance.Engine)
    // (Spaceship.Cockpit, Instance.Cockpit)

    auto instance = ecs.entity("MySpaceship")
        .is_a(spaceship);

    // Получение слотов инстанса
    auto instance_engine = instance.target(flecs::SlotOf, engine);
    auto instance_cockpit = instance.target(flecs::SlotOf, cockpit);

    std::println("Instance engine: {}", instance_engine.name());
    std::println("Instance cockpit: {}", instance_cockpit.name());
}

// Паттерн 2: Группировка по отношениям
void group_by_example(flecs::world& ecs) {
    struct Group {};
    struct First {};
    struct Second {};
    struct Third {};

    // Создание сущностей с разными группами
    ecs.entity("E1").add<Group>(Third).set<Position>({1, 1});
    ecs.entity("E2").add<Group>(Second).set<Position>({2, 2});
    ecs.entity("E3").add<Group>(First).set<Position>({3, 3});
    ecs.entity("E4").add<Group>(Third).set<Position>({4, 4});
    ecs.entity("E5").add<Group>(Second).set<Position>({5, 5});
    ecs.entity("E6").add<Group>(First).set<Position>({6, 6});

    // Запрос с группировкой по цели отношения Group
    auto q = ecs.query_builder<Position>()
        .group_by<Group>([](flecs::world_t* w, flecs::table_t* t,
                           flecs::id_t id, void* ctx) {
            // Возвращает цель отношения Group как ID группы
            flecs::id_t match;
            if (flecs::search(w, t, flecs::pair(id, flecs::Wildcard), &match) != -1) {
                return flecs::pair_second(w, match);
            }
            return flecs::id_t(0);
        })
        .build();

    // Итерация будет сгруппирована по First, Second, Third
    q.each([](flecs::iter& it, size_t index, Position& pos) {
        std::println("Group {}: Position ({}, {})",
                     it.group_id(), pos.x, pos.y);
    });
}

// Паттерн 3: Иерархические запросы (Cascade)
void cascade_example(flecs::world& ecs) {
    // Создание иерархии
    auto root = ecs.entity("Root").set<Position>({0, 0});
    auto child1 = ecs.entity("Child1").child_of(root).set<Position>({1, 1});
    auto child2 = ecs.entity("Child2").child_of(root).set<Position>({2, 2});
    auto grandchild = ecs.entity("Grandchild").child_of(child1).set<Position>({3, 3});

    // Запрос с каскадом (обход от корня к листьям)
    auto q = ecs.query_builder<Position>()
        .cascade()  // Включает каскадный обход по ChildOf
        .build();

    std::println("Cascade traversal:");
    q.each([](flecs::entity e, Position& pos) {
        std::println("  {}: ({}, {})", e.name(), pos.x, pos.y);
    });
    // Вывод: Root, Child1, Grandchild, Child2 (в порядке глубины)
}

int main() {
    flecs::world ecs;

    slots_example(ecs);
    group_by_example(ecs);
    cascade_example(ecs);

    return 0;
}
```

### Лучшие практики

1. **Используйте встроенные отношения когда возможно:**
  - `ChildOf` для иерархий
  - `IsA` для префабов и наследования
  - `DependsOn` для зависимостей систем

2. **Выбирайте правильные свойства отношений:**
  - `Symmetric` для двусторонних отношений (дружба, торговля)
  - `Exclusive` для взаимоисключающих отношений (взвод, команда)
  - `Transitive` для транзитивных отношений (расположение, наследование)

3. **Оптимизируйте запросы:**
  - Используйте конкретные цели вместо `Wildcard` когда возможно
  - Применяйте `optional()` для необязательных отношений
  - Используйте `cascade()` для иерархических данных

4. **Избегайте антипаттернов:**
  - Не создавайте циклических зависимостей в отношениях
  - Не используйте `Wildcard` в performance-critical запросах
  - Не смешивайте разные типы отношений без четкой структуры

## 📦 Modules (Модули)

> **Для понимания:** Модули — это способ организовать код в логические единицы. Представьте библиотеку: каждый раздел (
> физика, рендеринг, AI) — это отдельный модуль со своими компонентами и системами.

### Базовые модули

```cpp
#include <flecs.h>
#include <print>

// Модуль Physics
struct PhysicsModule {
    // Компоненты модуля
    struct Velocity {
        float x, y, z;
    };

    struct Acceleration {
        float x, y, z;
    };

    struct Mass {
        float value;
    };

    // Системы модуля
    struct UpdateSystem {
        UpdateSystem(flecs::world& world) {
            world.system<Velocity, const Acceleration>("PhysicsUpdate")
                .each([](Velocity& vel, const Acceleration& acc) {
                    vel.x += acc.x;
                    vel.y += acc.y;
                    vel.z += acc.z;
                });
        }
    };

    // Конструктор модуля
    PhysicsModule(flecs::world& world) {
        // Регистрация компонентов
        world.component<Velocity>();
        world.component<Acceleration>();
        world.component<Mass>();

        // Регистрация систем
        world.module<PhysicsModule>();
        world.emplace<UpdateSystem>(world);
    }
};

// Модуль Render
struct RenderModule {
    struct Position {
        float x, y, z;
    };

    struct Mesh {
        std::string path;
    };

    struct RenderSystem {
        RenderSystem(flecs::world& world) {
            world.system<Position, const Mesh>("RenderSystem")
                .each([](Position& pos, const Mesh& mesh) {
                    // Рендеринг логика
                    std::println("Rendering {} at ({}, {}, {})",
                                 mesh.path, pos.x, pos.y, pos.z);
                });
        }
    };

    RenderModule(flecs::world& world) {
        world.component<Position>();
        world.component<Mesh>();

        world.module<RenderModule>();
        world.emplace<RenderSystem>(world);
    }
};

int main() {
    flecs::world ecs;

    // Импорт модулей
    ecs.import<PhysicsModule>();
    ecs.import<RenderModule>();

    // Создание сущности с компонентами из обоих модулей
    auto entity = ecs.entity()
        .set<PhysicsModule::Velocity>({1.0f, 0.0f, 0.0f})
        .set<PhysicsModule::Acceleration>({0.1f, 0.0f, 0.0f})
        .set<RenderModule::Position>({0.0f, 0.0f, 0.0f})
        .set<RenderModule::Mesh>({"spaceship.obj"});

    // Запуск систем
    for (int i = 0; i < 3; ++i) {
        std::println("\n--- Frame {} ---", i + 1);
        ecs.progress();
    }

    return 0;
}
```

### Именованные модули

```cpp
#include <flecs.h>
#include <print>

// Модуль с именем
struct GameModule {
    struct Player {
        std::string name;
        int score;
    };

    struct Enemy {
        float aggression;
    };

    GameModule(flecs::world& world) {
        // Создание модуля с именем "Game"
        world.module<GameModule>("Game");

        world.component<Player>();
        world.component<Enemy>();

        // Система подсчета очков
        world.system<Player>("ScoreSystem")
            .each([](Player& player) {
                player.score += 10;
                std::println("{} score: {}", player.name, player.score);
            });
    }
};

int main() {
    flecs::world ecs;

    // Импорт с явным указанием имени
    auto game_module = ecs.import<GameModule>();

    // Проверка имени модуля
    std::println("Module name: {}", game_module.name());
    std::println("Module path: {}", game_module.path());

    // Создание сущности в модуле
    auto player = ecs.entity("Player")
        .set<GameModule::Player>({"Hero", 0});

    auto enemy = ecs.entity("Enemy")
        .set<GameModule::Enemy>({0.8f});

    // Запуск систем модуля
    ecs.progress();

    return 0;
}
```

### Вложенные модули

```cpp
#include <flecs.h>
#include <print>

// Корневой модуль
struct CoreModule {
    struct Transform {
        float x, y, z;
    };

    CoreModule(flecs::world& world) {
        world.module<CoreModule>("Core");
        world.component<Transform>();
    }
};

// Подмодуль Graphics
struct GraphicsModule {
    struct MeshRenderer {
        uint32_t vao;
        uint32_t texture;
    };

    GraphicsModule(flecs::world& world) {
        // Создание как подмодуля Core::Graphics
        world.module<GraphicsModule>("Core::Graphics");
        world.component<MeshRenderer>();

        // Система рендеринга
        world.system<const CoreModule::Transform, const MeshRenderer>("Render")
            .each([](const CoreModule::Transform& transform,
                     const MeshRenderer& renderer) {
                std::println("Rendering at ({}, {}, {}) with VAO: {}",
                             transform.x, transform.y, transform.z,
                             renderer.vao);
            });
    }
};

// Подмодуль Physics
struct PhysicsModule {
    struct Rigidbody {
        float mass;
        bool is_kinematic;
    };

    PhysicsModule(flecs::world& world) {
        // Создание как подмодуля Core::Physics
        world.module<PhysicsModule>("Core::Physics");
        world.component<Rigidbody>();

        // Система физики
        world.system<CoreModule::Transform, const Rigidbody>("PhysicsUpdate")
            .each([](CoreModule::Transform& transform,
                     const Rigidbody& body) {
                if (!body.is_kinematic) {
                    transform.y -= 9.81f * body.mass * 0.016f; // gravity
                }
            });
    }
};

int main() {
    flecs::world ecs;

    // Импорт модулей
    ecs.import<CoreModule>();
    ecs.import<GraphicsModule>();
    ecs.import<PhysicsModule>();

    // Создание сущности с компонентами из разных модулей
    auto entity = ecs.entity("GameObject")
        .set<CoreModule::Transform>({0.0f, 10.0f, 0.0f})
        .set<GraphicsModule::MeshRenderer>({123, 456})
        .set<PhysicsModule::Rigidbody>({1.0f, false});

    // Запуск симуляции
    for (int i = 0; i < 5; ++i) {
        std::println("\nFrame {}:", i + 1);
        ecs.progress(0.016f);
        auto& transform = *entity.get<CoreModule::Transform>();
        std::println("  Position: ({}, {}, {})",
                     transform.x, transform.y, transform.z);
    }

    return 0;
}
```

### Динамические модули

```cpp
#include <flecs.h>
#include <print>
#include <memory>

// Интерфейс плагина
struct IPlugin {
    virtual ~IPlugin() = default;
    virtual void initialize(flecs::world& world) = 0;
    virtual void update(flecs::world& world) = 0;
    virtual void shutdown(flecs::world& world) = 0;
};

// Менеджер плагинов
class PluginManager {
    flecs::world& world_;
    std::vector<std::unique_ptr<IPlugin>> plugins_;

public:
    PluginManager(flecs::world& world) : world_(world) {}

    template<typename Plugin>
    void register_plugin() {
        auto plugin = std::make_unique<Plugin>();
        plugin->initialize(world_);
        plugins_.push_back(std::move(plugin));
    }

    void update_all() {
        for (auto& plugin : plugins_) {
            plugin->update(world_);
        }
    }

    void shutdown_all() {
        for (auto& plugin : plugins_) {
            plugin->shutdown(world_);
        }
    }
};

// Пример плагина
struct AudioPlugin : IPlugin {
    struct SoundSource {
        std::string file;
        float volume;
    };

    struct AudioSystem {
        AudioSystem(flecs::world& world) {
            world.system<const SoundSource>("AudioPlayback")
                .each([](const SoundSource& source) {
                    std::println("Playing sound: {} at volume {}",
                                 source.file, source.volume);
                });
        }
    };

    void initialize(flecs::world& world) override {
        world.module<AudioPlugin>("Plugins::Audio");
        world.component<SoundSource>();
        world.emplace<AudioSystem>(world);
        std::println("Audio plugin initialized");
    }

    void update(flecs::world& world) override {
        // Обновление состояния аудио
    }

    void shutdown(flecs::world& world) override {
        std::println("Audio plugin shutdown");
    }
};

// Другой плагин
struct NetworkPlugin : IPlugin {
    struct NetworkComponent {
        uint64_t connection_id;
        bool is_connected;
    };

    void initialize(flecs::world& world) override {
        world.module<NetworkPlugin>("Plugins::Network");
        world.component<NetworkComponent>();
        std::println("Network plugin initialized");
    }

    void update(flecs::world& world) override {
        // Сетевая логика
    }

    void shutdown(flecs::world& world) override {
        std::println("Network plugin shutdown");
    }
};

int main() {
    flecs::world ecs;

    // Менеджер плагинов
    PluginManager manager(ecs);

    // Динамическая регистрация плагинов
    manager.register_plugin<AudioPlugin>();
    manager.register_plugin<NetworkPlugin>();

    // Создание сущности с компонентами плагинов
    auto entity = ecs.entity("Player")
        .set<AudioPlugin::SoundSource>({"explosion.wav", 0.8f})
        .set<NetworkPlugin::NetworkComponent>({12345, true});

    // Обновление плагинов
    manager.update_all();

    // Запуск систем
    ecs.progress();

    // Завершение работы
    manager.shutdown_all();

    return 0;
}
```

### Лучшие практики модулей

1. **Организация по функциональности:**
  - Группируйте связанные компоненты и системы в один модуль
  - Используйте вложенные модули для сложных систем
  - Разделяйте модули по ответственности (физика, рендеринг, AI)

2. **Изоляция зависимостей:**
  - Модули должны быть максимально независимыми
  - Используйте интерфейсы для связи между модулями
  - Избегайте циклических зависимостей между модулями

3. **Конфигурируемость:**
  - Позволяйте модулям настраиваться через параметры
  - Используйте фабричные методы для создания модулей
  - Поддерживайте hot-reload для модулей где возможно

4. **Документация модулей:**
  - Четко документируйте публичный API модуля
  - Указывайте зависимости и требования
  - Предоставляйте примеры использования

