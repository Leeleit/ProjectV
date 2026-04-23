## Обзор flecs

<!-- anchor: 00_overview -->

🟢 **Уровень 1: Начинающий**

**Flecs** — быстрый и лёгкий Entity Component System (ECS) фреймворк для игр и симуляций. Позволяет создавать приложения
с миллионами сущностей. Поддерживает связи между сущностями (relationships), иерархии, префабы, запросы (queries) и
системы (systems). API на C99 и C++17, archetype-хранилище (SoA), кешируемые запросы.

Версия: **v4** (см. [flecs.h](../../external/flecs/include/flecs.h))
Исходники: [SanderMertens/flecs](https://github.com/SanderMertens/flecs)
Документация: [flecs.dev](https://www.flecs.dev)

## Основные возможности

- **Entity Component System** — организация данных через сущности, компоненты и системы
- **Archetype storage** — эффективное SoA-хранение для кеш-дружественной итерации
- **Relationships** — связи между сущностями через pairs
- **Иерархии** — parent-child отношения с автоматическим cleanup
- **Префабы** — шаблоны сущностей с наследованием
- **Queries** — кешируемые и ad-hoc запросы
- **Observers** — реактивное программирование на событиях
- **Модули** — организация кода в переиспользуемые блоки
- **Многопоточность** — автоматическое распараллеливание систем

## Карта заголовков

| Файл                                                                                                | Назначение                                              |
|-----------------------------------------------------------------------------------------------------|---------------------------------------------------------|
| [flecs.h](../../external/flecs/include/flecs.h)                                                     | Точка входа C API; при C++ подтягивает flecs.hpp        |
| [flecs/addons/flecs_c.h](../../external/flecs/include/flecs/addons/flecs_c.h)                       | C макросы: ECS_COMPONENT, ECS_SYSTEM, ecs_set, ecs_each |
| [flecs/addons/cpp/flecs.hpp](../../external/flecs/include/flecs/addons/cpp/flecs.hpp)               | C++ точка входа                                         |
| [flecs/addons/cpp/entity.hpp](../../external/flecs/include/flecs/addons/cpp/entity.hpp)             | Класс flecs::entity                                     |
| [flecs/addons/cpp/mixins/query/](../../external/flecs/include/flecs/addons/cpp/mixins/query/)       | Query builder                                           |
| [flecs/addons/cpp/mixins/system/](../../external/flecs/include/flecs/addons/cpp/mixins/system/)     | System builder                                          |
| [flecs/addons/cpp/mixins/observer/](../../external/flecs/include/flecs/addons/cpp/mixins/observer/) | Observer builder                                        |

## Требования

- C++17 или новее (для C++ API)
- C99 для C API
- CMake 3.10+ (при сборке из исходников)

## Содержание документации

| Раздел                                         | Описание                                                |
|------------------------------------------------|---------------------------------------------------------|
| [01_quickstart.md](01_quickstart.md)           | Минимальный пример: world, entity, component, system    |
| [02_concepts.md](02_concepts.md)               | ECS, архитектура flecs, pipeline, иерархии, singleton   |
| [03_integration.md](03_integration.md)         | CMake, порядок include, модули, addons                  |
| [04_api-reference.md](04_api-reference.md)     | Справочник по основным API                              |
| [05_tools.md](05_tools.md)                     | Практические рецепты: query builder, observers, prefabs |
| [06_performance.md](06_performance.md)         | Производительность, многопоточность, unsafe access      |
| [07_troubleshooting.md](07_troubleshooting.md) | Решение распространённых проблем                        |
| [08_glossary.md](08_glossary.md)               | Словарь терминов                                        |

## Для ProjectV

Интеграция с ProjectV (Vulkan, SDL3, VMA, JoltPhysics) описана в отдельных файлах:

- [09_projectv-integration.md](09_projectv-integration.md) — связка flecs с графическими и физическими движками
- [10_projectv-patterns.md](10_projectv-patterns.md) — паттерны для воксельного движка

---

## Основные понятия flecs

<!-- anchor: 02_concepts -->

🟡 **Уровень 2: Средний**

Ключевые концепции ECS и архитектуры flecs.

## Что такое ECS

**ECS (Entity Component System)** — подход к организации кода и данных:

- **Entity** — уникальный объект (юнит, здание, частица, камера). Сам по себе это просто ID.
- **Component** — данные, привязанные к entity (Position, Velocity, Health, Mesh).
- **System** — функция, которая выполняется для всех entities, имеющих определённый набор компонентов.

### Преимущества ECS

- **Разделение данных и логики** — системы получают только нужные компоненты, кеш-дружественная итерация (SoA)
- **Гибкость** — добавление нового поведения через новые компоненты и системы без изменения старых
- **Масштабируемость** — flecs оптимизирован под миллионы entities и многопоточность
- **Композиция** — поведение сущности определяется набором компонентов, а не наследованием

---

## Архитектура flecs

```
World
├── Entities (уникальные ID)
├── Archetypes (таблицы компонентов)
│   ├── Archetype [Position, Velocity]
│   ├── Archetype [Position, Health]
│   └── ...
├── Systems (query + callback)
└── Pipeline (порядок выполнения систем)
```

### World

Контейнер для всех данных ECS. Один world на приложение.

```cpp
flecs::world ecs;  // C++
ecs_world_t* world = ecs_init();  // C
```

### Archetype

Таблицы, группирующие entities с одинаковым набором компонентов.

**Хранение SoA (Structure of Arrays):**

- Для archetype `[Position, Velocity]` — два массива: `Position[]`, `Velocity[]`
- Итерация систем — последовательный доступ к массивам, кеш-дружественно
- Archetype создаётся при первой entity с данной комбинацией компонентов

### Entity

Уникальный идентификатор (64 бита). Младшие 32 бита — ID, старшие — версия (generation).

```cpp
auto e = ecs.entity();           // Анонимная
auto e = ecs.entity("Name");     // Именованная
e.set<Position>({x, y});         // Добавить компонент
e.add<Tag>();                    // Добавить тег (без данных)
e.remove<Position>();            // Удалить компонент
e.destruct();                    // Удалить entity
```

### Component

Тип данных на entity. В C++ — любая структура с тривиальным конструктором копирования.

```cpp
struct Position { float x, y; };
struct Health { float current, max; };
struct PlayerTag {};  // Тег (пустая структура)

// Регистрация автоматическая при первом использовании
e.set<Position>({10, 20});
```

---

## Pipeline и фазы

Pipeline определяет порядок выполнения систем. Системы привязываются к фазам (phase tags).

### Стандартные фазы

| Фаза           | Назначение                        |
|----------------|-----------------------------------|
| **OnLoad**     | Загрузка ресурсов                 |
| **PostLoad**   | Постобработка после загрузки      |
| **PreUpdate**  | Подготовка перед обновлением      |
| **OnUpdate**   | Основная логика (движение, AI)    |
| **OnValidate** | Проверки                          |
| **PostUpdate** | Синхронизация после обновления    |
| **PreStore**   | Подготовка к сохранению           |
| **OnStore**    | Финализация, сохранение состояния |

### Порядок выполнения

```
OnLoad → PostLoad → PreUpdate → OnUpdate → OnValidate → PostUpdate → PreStore → OnStore
```

Системы внутри одной фазы выполняются в порядке объявления.

```cpp
ecs.system<Position, Velocity>("Move")
    .kind(flecs::OnUpdate)  // Фаза
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
        p.y += v.y;
    });
```

---

## Query

Поиск и итерация entities по условиям.

### Типы запросов

| Тип          | Использование  | Особенности                     |
|--------------|----------------|---------------------------------|
| **Cached**   | Каждый кадр    | Быстрая итерация, больше памяти |
| **Uncached** | Редкие запросы | Меньше памяти, медленнее        |

### Query Builder (C++)

```cpp
// Простой запрос
auto q = ecs.query_builder<Position, Velocity>().build();

// С условием
auto q = ecs.query_builder<Position>()
    .with<Velocity>()           // Имеет Velocity
    .without<Dead>()            // Без Dead
    .term_at(1).optional()      // Velocity необязателен
    .build();

// Итерация
q.each([](flecs::entity e, Position& p) {
    // Обработка
});

// Iter (доступ к batch)
q.iter([](flecs::iter& it, Position* p) {
    for (auto i : it) {
        // p[i] — i-я entity в batch
    }
});
```

### Операторы

| Оператор     | C++                           | Описание                         |
|--------------|-------------------------------|----------------------------------|
| **And**      | по умолчанию                  | Entity должна иметь компонент    |
| **Not**      | `.with<T>().oper(flecs::Not)` | Исключить entities с компонентом |
| **Optional** | `.with<T>().optional()`       | Компонент необязателен           |
| **Or**       | специальный синтаксис         | Матч по одному из terms          |

---

## System

Query + callback. Выполняется каждый кадр в своей фазе.

```cpp
// each — callback по одной entity
ecs.system<Position, Velocity>()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x;
    });

// iter — доступ к batch
ecs.system<Position, Velocity>()
    .iter([](flecs::iter& it, Position* p, Velocity* v) {
        for (auto i : it) {
            p[i].x += v[i].x * it.delta_time();
        }
    });

// multi_threaded — параллельное выполнение
ecs.system<Position, Velocity>()
    .multi_threaded()
    .each([](Position& p, const Velocity& v) { ... });
```

---

## Observer

Реакция на изменения ECS, а не каждый кадр.

### События

| Событие      | Когда срабатывает                        |
|--------------|------------------------------------------|
| **OnAdd**    | Компонент добавлен к entity              |
| **OnRemove** | Компонент удалён                         |
| **OnSet**    | Значение компонента установлено/изменено |
| **UnSet**    | Значение сброшено (перед удалением)      |

### Использование

```cpp
// Создание ресурса при добавлении компонента
ecs.observer<MeshComponent>()
    .event(flecs::OnAdd)
    .each([](flecs::entity e, MeshComponent& mesh) {
        mesh.buffer = create_gpu_buffer();
    });

// Освобождение ресурса при удалении
ecs.observer<MeshComponent>()
    .event(flecs::OnRemove)
    .each([](flecs::entity e, MeshComponent& mesh) {
        destroy_gpu_buffer(mesh.buffer);
    });
```

---

## Иерархии и Pairs

### Иерархия (ChildOf)

Entities могут быть родителями и детьми.

```cpp
auto parent = ecs.entity("Parent");
auto child = ecs.entity("Child").child_of(parent);

// Путь: "Parent::Child"
child.path();

// При удалении parent удаляются все дети
parent.destruct();
```

### Pairs

Пара (relationship, target) кодирует связь между entities.

```cpp
auto Likes = ecs.entity();
auto Alice = ecs.entity();

bob.add(Likes, Alice);        // Bob likes Alice
bob.has(Likes, Alice);        // true
bob.target<Likes>();          // Alice (первый target)
```

---

## Singleton

Глобальный экземпляр компонента. Удобно для настроек: гравитация, время, конфигурация.

```cpp
// Установка
ecs.set<Gravity>({9.81f});

// Получение
const Gravity* g = ecs.get<Gravity>();

// В системе — Gravity доступен автоматически
ecs.system<Position, const Gravity>()
    .each([](Position& p, const Gravity& g) {
        // g — singleton
    });
```

---

## Prefabs

Entity-шаблон. Instance наследует компоненты prefab через pair `(IsA, prefab)`.

```cpp
// Создание prefab
auto Tank = ecs.prefab("Tank")
    .set<Health>({100})
    .set<Attack>{20};

// Создание instance
auto unit = ecs.entity().is_a(Tank);

// Instance наследует компоненты
unit.get<Health>();  // 100

// Override — переопределение значения
unit.set<Health>({150});  // Теперь своё значение
```

---

## Traversal

Обход иерархии в query.

### Source

Entity, на которой проверяется term:

- **self** (по умолчанию) — сама entity
- **parent** — компонент с родителя по ChildOf
- **cascade** — обход иерархии breadth-first

```cpp
// Второй Transform — от родителя
ecs.query_builder<Transform, Transform>()
    .term_at(2).parent()
    .cascade()  // Обход: родители → дети
    .build()
    .each([](Transform& local, Transform& parent) {
        // local — локальный; parent — родительский
    });
```

---

## Версионирование Entity

`ecs_entity_t` — 64 бита: младшие 32 — ID, старшие — версия (generation).

При удалении и переиспользовании ID версия увеличивается. Старые ссылки становятся невалидными.

```cpp
auto e = ecs.entity();
e.destruct();

e.is_alive();  // false
```

---

## Общая схема данных

```
Данные:
World → Entity → Component → Archetype (таблица)

Выполнение:
progress() → Pipeline → Phases → Systems → Queries → Archetypes → Callback
```

---

## Глоссарий flecs

<!-- anchor: 08_glossary -->

🟢 **Уровень 1: Начинающий**

Словарь терминов flecs. Подробные примеры — в Основных понятиях и API Reference.

## Ядро ECS

| Термин        | Объяснение                                                                                                             |
|---------------|------------------------------------------------------------------------------------------------------------------------|
| **ECS**       | Entity Component System — подход к организации кода. Entity — уникальные объекты, Component — данные, System — логика. |
| **World**     | Контейнер всех ECS-данных. Один world на приложение. C++: `flecs::world`, C: `ecs_world_t*`.                           |
| **Entity**    | Уникальная сущность. 64-битный id (младшие 32 бита — ID, старшие — версия). Сама по себе не несёт данных.              |
| **Component** | Тип данных на entity. В C++ — структура, в C — регистрируется через `ECS_COMPONENT`.                                   |
| **Tag**       | Компонент без данных. Пустая структура в C++, используется для маркировки.                                             |
| **System**    | Query + callback. Выполняется каждый кадр для entities, матчат query.                                                  |
| **Query**     | Поиск entities по условиям (компоненты, pairs, операторы).                                                             |

---

## Архитектура

| Термин        | Объяснение                                                                                   |
|---------------|----------------------------------------------------------------------------------------------|
| **Archetype** | Таблица, группирующая entities с одинаковым набором компонентов. SoA-хранение.               |
| **SoA**       | Structure of Arrays — хранение компонентов в отдельных массивах для cache-friendly итерации. |
| **Type**      | Список ids entity (её «архетип»). `entity.type()` возвращает тип.                            |
| **Pipeline**  | Список фаз, определяющий порядок выполнения систем.                                          |
| **Phase**     | Тег, определяющий порядок системы в pipeline. Фазы: OnLoad, OnUpdate, PostUpdate и др.       |

---

## Данные

| Термин           | Объяснение                                                                                        |
|------------------|---------------------------------------------------------------------------------------------------|
| **Id**           | 64-битный идентификатор. Кодирует component, tag или pair. Всё, что можно добавить к entity — id. |
| **Pair**         | Пара (relationship, target) — два id. Используется для связей между entities.                     |
| **Relationship** | Первый элемент pair — тип связи (например, `ChildOf`, `Likes`).                                   |
| **Target**       | Второй элемент pair — цель связи (например, родитель, Alice).                                     |
| **Singleton**    | Единственный экземпляр компонента. Доступ через `world.get<T>()`.                                 |
| **Prefab**       | Entity-шаблон. Instance наследует компоненты через pair `(IsA, prefab)`.                          |
| **Instance**     | Entity, созданная от prefab через `is_a()`.                                                       |

---

## Запросы

| Термин             | Объяснение                                                                           |
|--------------------|--------------------------------------------------------------------------------------|
| **Term**           | Элемент query — условие матчинга («имеет Position», «имеет pair (ChildOf, parent)»). |
| **Field**          | Массив значений, возвращаемый итератором для каждого term.                           |
| **Cached Query**   | Query с кешированным списком archetypes. Быстрая итерация, больше памяти.            |
| **Uncached Query** | Query без кеша. Меньше памяти, медленная итерация.                                   |
| **each**           | Callback по одной entity. Проще писать, overhead на вызов.                           |
| **iter**           | Batch итерация. Быстрее для больших объёмов, доступ к `delta_time`.                  |

---

## Операторы

| Термин       | Объяснение                                   |
|--------------|----------------------------------------------|
| **And**      | По умолчанию. Entity должна иметь компонент. |
| **Not**      | Исключить entities с компонентом.            |
| **Optional** | Компонент необязателен (может быть nullptr). |
| **Or**       | Матч по одному из terms.                     |

---

## Иерархии

| Термин        | Объяснение                                                                                   |
|---------------|----------------------------------------------------------------------------------------------|
| **ChildOf**   | Встроенный relationship для иерархии родитель-ребёнок. При удалении родителя удаляются дети. |
| **Parent**    | Entity, к которой привязан ребёнок через `ChildOf`.                                          |
| **Child**     | Entity, привязанная к родителю через `child_of()`.                                           |
| **Path**      | Путь entity в иерархии (например, `"Parent::Child"`).                                        |
| **Cascade**   | Обход иерархии breadth-first в query.                                                        |
| **Traversal** | Обход иерархии: `parent()` — компонент с родителя, `cascade()` — breadth-first.              |

---

## События

| Термин       | Объяснение                                                          |
|--------------|---------------------------------------------------------------------|
| **Observer** | Query + callback, вызываемый при событиях (OnAdd, OnRemove, OnSet). |
| **OnAdd**    | Событие: компонент добавлен к entity.                               |
| **OnRemove** | Событие: компонент удалён.                                          |
| **OnSet**    | Событие: значение компонента установлено/изменено.                  |
| **UnSet**    | Событие: значение сброшено перед удалением.                         |

---

## Версионирование

| Термин         | Объяснение                                                              |
|----------------|-------------------------------------------------------------------------|
| **Generation** | Старшие 32 бита `ecs_entity_t`. Увеличивается при переиспользовании ID. |
| **is_alive**   | Проверка живости entity. Удалённые entities возвращают `false`.         |

---

## API

| Термин            | Объяснение                                                           |
|-------------------|----------------------------------------------------------------------|
| **ecs_entity_t**  | C API: `typedef ecs_id_t ecs_entity_t`. 64-битный id entity.         |
| **ecs_id_t**      | C API: 64-битный идентификатор (component, tag, pair).               |
| **ecs_iter_t**    | C API: итератор. Поля: `entities`, `count`, `delta_time`, `world`.   |
| **ecs_field**     | C макрос: `ecs_field(it, Type, index)` — получить массив компонента. |
| **ECS_COMPONENT** | C макрос: регистрация компонента.                                    |
| **ECS_SYSTEM**    | C макрос: создание системы.                                          |
| **FLECS_DEBUG**   | Макрос для включения проверок и assert'ов.                           |

---

## Модули

| Термин     | Объяснение                                                                           |
|------------|--------------------------------------------------------------------------------------|
| **Module** | Организация кода в переиспользуемый блок. C++: `world.import<T>()`, C: `ECS_IMPORT`. |
| **Addon**  | Опциональный модуль flecs (REST, JSON, Units и др.).                                 |

---

## Производительность

| Термин             | Объяснение                                                     |
|--------------------|----------------------------------------------------------------|
| **multi_threaded** | Параллельное выполнение системы. Требует `ecs.set_threads(n)`. |
| **defer**          | Отложенные операции. Изменения применяются при `defer_end()`.  |
| **unsafe access**  | Доступ без проверок для максимальной производительности.       |
| **bulk_create**    | Массовое создание entities без overhead на каждую.             |

---

## Инструменты

| Термин             | Объяснение                                                                        |
|--------------------|-----------------------------------------------------------------------------------|
| **Flecs Explorer** | Веб-инструмент для просмотра entities, компонентов, иерархий. Требует REST addon. |
| **Tracy**          | Профилировщик. Flecs поддерживает интеграцию через `FLECS_TRACY`.                 |