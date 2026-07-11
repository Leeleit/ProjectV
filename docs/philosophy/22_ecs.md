# Entity Component System

Документ описывает ECS (Entity Component System) — способ организации
игрового мира как базы данных.

---

## Композиция вместо наследования

Классическое ООП-наследование для игровых сущностей создаёт проблемы:

- Алмазное наследование требует `virtual`.
- Базовый класс становится god-object.
- Виртуальные вызовы в hot path — overhead.

ECS заменяет иерархии на композицию:

- Сущность (Entity) — ID. Не объект, не контейнер. Просто ключ.
- Компонент (Component) — POD-структура с данными. Без логики.
- Система (System) — функция, обрабатывающая компоненты определённого
  типа.

Поведение собирается из компонентов, не наследуется от базового класса.

---

## Принципы ECS

### Компоненты — данные

Компонент — `struct` с POD-полями. Без виртуальных функций, без
наследования, без зависимостей между компонентами.

```cpp
struct Position { float x, y, z; };
struct Velocity { float vx, vy, vz; };
struct Health { int current, max; };
```

### Системы — функции

Система — функция, обрабатывающая набор компонентов. Без состояния между
вызовами.

```cpp
void MovementSystem(flecs::world& world, float dt) {
    world.query<Position, const Velocity>().each([dt](Position& p, const Velocity& v) {
        p.x += v.vx * dt;
        p.y += v.vy * dt;
        p.z += v.vz * dt;
    });
}
```

### Сущности — ID

Сущность — `uint64_t`. Не объект в памяти. Ключ для поиска компонентов.

---

## Архетипы

ECS автоматически группирует сущности по набору компонентов в архетипы.
Все сущности с `[Position, Velocity]` лежат в одном архетипе. Все с
`[Position, Mesh]` — в другом.

Архетип = SoA layout. Компоненты одного типа хранятся в плотном
массиве.

Система запрашивает «все сущности с Position и Velocity» и получает
плотный массив без проверок `if (has<Position>())`.

---

## Staging (только-для-чтения, defer)

Современные ECS (flecs v4, Bevy ECS, Unity DOTS) поддерживают систему
staging для безопасной многопоточности.

### Read-only режим

Когда мир входит в read-only режим, все мутации запрещены. Любая
попытка `add()`, `remove()`, `set()` вместо прямого выполнения ставится
в командную очередь активного stage.

```cpp
world.readonly_begin(true);  // multi-threaded = true
// queries iterate без блокировок
world.query<Position, const Velocity>().each(...);
// мутации отложены в command queue:
world.entity("Foo").add<Health>(); // не выполняется немедленно
world.readonly_end(); // выполняет все отложенные команды последовательно
```

### Multi-threaded режим

В multi-threaded режиме каждый worker thread получает свой stage.
Команды из worker stages сливаются в main world на `readonly_end()`. Нет
блокировок во время iteration. Нет race conditions.

### Defer mode

`defer_begin()` / `defer_end()` — ручное управление. Операции,
выполненные между begin/end, ставятся в очередь.

`defer_suspend()` / `defer_resume()` — временное отключение defer без
flush очереди.

### Когда что использовать

- **Auto defer в readonly mode:** для system iteration под multi-threading.
- **Manual defer:** для кода, который знает, что нужно batch-мутировать
  много компонентов.
- **Suspend/resume:** для exceptional cases, где defer неуместен.

---

## ECS и DOD: симбиоз

flecs автоматически даёт SoA layout для компонентов через archetypes.
DOD-принципы применяются бесплатно.

- Cache locality: итерация по архетипу = линейный проход по массиву.
- Параллелизм: системы, работающие с разными архетипами, не
  конфликтуют по памяти.
- SIMD-векторизация: компоненты выровнены и плотно упакованы.

---

## Почему ECS масштабируется

### Добавление фич без переписывания

Хочешь добавить физику — добавляешь компонент `PhysicsBody` нужным
сущностям. Система `PhysicsSystem` начинает обрабатывать их
автоматически. Иерархия классов не меняется.

### Оптимизация через данные

Система `RenderingSystem` работает с `[Position, Mesh]`. Она не видит
другие компоненты. Кэш не загрязняется.

### Тестируемость

Система — чистая функция от компонентов. Тест: создать мир с известными
компонентами, вызвать `world.progress(dt)`, проверить изменения.

---

## Когда ECS не подходит

- **UI:** окна, кнопки, текстовые поля — классическое ООП с
  наследованием работает лучше.
- **Сложная бизнес-логика:** если у тебя 10 объектов с уникальным
  поведением, ECS добавляет сложности без выгоды.
- **Прототипирование:** для быстрого прототипа проще написать пару
  классов.

Для ядра движка (рендеринг, физика, AI, обработка тысяч сущностей)
ECS незаменим.

---

## Пример: применение в движке

В ProjectV используется flecs v4.1 — архетипный ECS с C99 API,
lockless scheduler, hierarchies, prefabs и staging для многопоточности.
ECS — основа всего движка.

---

## Источники и дальнейшее чтение

- **flecs v4 documentation** — официальная документация.
  <https://www.flecs.dev/flecs/>
- **Sander Mertens — Multithreading and Staging** (DeepWiki, март 2026).
  <https://deepwiki.com/SanderMertens/flecs/5.3-multithreading-and-staging>
- **The Essence of Entity Component System** (arXiv:2606.14919).
- [10_manifesto.md](10_manifesto.md) — данные важнее кода.
- [21_dod.md](21_dod.md) — Data-Oriented Design.
- [23_concurrency.md](23_concurrency.md) — Job System, lock-free
  структуры.
- [24_data-flow.md](24_data-flow.md) — поток данных между системами.