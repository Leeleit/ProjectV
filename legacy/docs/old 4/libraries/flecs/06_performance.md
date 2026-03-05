# Производительность flecs

🔴 **Уровень 3: Продвинутый**

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

> **Примечание:** Flecs использует SoA-хранение для cache-friendly итерации (
> см. [03_dod-philosophy.md](../../philosophy/03_dod-philosophy.md)).

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
