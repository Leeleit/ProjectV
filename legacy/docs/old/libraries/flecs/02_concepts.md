# Основные понятия flecs

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
