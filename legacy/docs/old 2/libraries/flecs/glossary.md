# Глоссарий

**🟢 Уровень 1: Начинающий**

Словарь терминов flecs. Подробные примеры — в [Основных понятиях](concepts.md) и [Быстром старте](quickstart.md).

## На этой странице

- [Ядро ECS](#ядро-ecs)
- [Сущности и данные](#сущности-и-данные)
- [Запросы и системы](#запросы-и-системы)
- [Иерархии и связи](#иерархии-и-связи)
- [Прочее](#прочее)
- [См. также](#см-также)

---

## Ядро ECS

| Термин            | Объяснение                                                                                                                                                                                                                             |
|-------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **ECS**           | Entity Component System — подход к организации кода и данных. Сущности (entities) — уникальные объекты; компоненты (components) — данные; системы (systems) — логика над entities, matching компоненты.                                |
| **flecs::world**  | C++: контейнер всех ECS-данных. `flecs::world ecs;` — один world на приложение. См. [world.hpp](../../external/flecs/include/flecs/addons/cpp/impl/world.hpp). В C: `ecs_world_t*`, `ecs_init()` / `ecs_fini()`.                       |
| **flecs::entity** | C++: класс с read/write операциями. `world.entity()` — создать; `e.set<T>()`, `e.get<T>()`, `e.destruct()`. См. [entity.hpp](../../external/flecs/include/flecs/addons/cpp/entity.hpp). В C: [api-reference](api-reference.md#entity). |
| **Entity**        | Уникальная сущность в мире. 64-битный id. Сама по себе не несёт данных; данные — в компонентах.                                                                                                                                        |
| **ecs_entity_t**  | C API: `typedef ecs_id_t ecs_entity_t` ([flecs.h:381](../../external/flecs/include/flecs.h)). Младшие 32 бита — ID, старшие — версия. Ноль — невалидный.                                                                               |
| **ecs_is_alive**  | Проверяет, жива ли entity. C++: `entity.is_alive()`. Удалённые id переиспользуются — старая ссылка невалидна.                                                                                                                          |

---

## Сущности и данные

| Термин           | Объяснение                                                                                                                                                                                         |
|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Component**    | Тип данных на entity. C++: `entity.set<Position>({10,20})`, `entity.get<Position>()` — регистрация при первом использовании. C: `ECS_COMPONENT(world, Position)`, `ecs_set` / `ecs_get`.           |
| **Tag**          | Компонент без данных. C++: пустая структура. Добавляется `entity.add<Tag>()` без значения.                                                                                                         |
| **Id**           | 64-битный идентификатор, кодирующий component, tag или pair. `ecs_id_t` в C. Всё, что можно добавить к entity, является id.                                                                        |
| **Pair**         | Пара (relationship, target) — два id. Используется для связей между entities. Например, `(Likes, Alice)` — «Bob likes Alice». Компонент/тег может быть добавлен многократно, если в разных pairs.  |
| **Relationship** | Первый элемент pair — тип связи (например, `Likes`, `Eats`).                                                                                                                                       |
| **Target**       | Второй элемент pair — цель связи (например, `Alice`, `Apples`).                                                                                                                                    |
| **Archetype**    | Таблица (table), группирующая entities с одинаковым набором компонентов. Хранение SoA (Structure of Arrays) для кеш-дружественной итерации.                                                        |
| **Type**         | Список ids entity (её «архетип»). Можно получить через `ecs_get_type` / `entity.type()`.                                                                                                           |
| **Singleton**    | Единственный экземпляр компонента. C++: `world.set<Gravity>({9.81})`, `world.get<Gravity>()`. C: `ecs_set(world, ecs_id(Gravity), Gravity, {9.81})`. Подробнее: [concepts](concepts.md#singleton). |
| **ecs_clear**    | Удаляет все компоненты с entity, не удаляя её. `ecs_clear(world, e)` / `entity.clear()`.                                                                                                           |
| **ecs_ensure**   | Добавляет компонент, если его нет; не перезаписывает значение. Возвращает указатель на память.                                                                                                     |
| **ecs_emplace**  | Добавляет компонент без инициализации — вызывающий код заполняет память. Макрос: `ecs_emplace(world, e, Position, &is_new)`.                                                                       |
| **ecs_insert**   | Создаёт entity с набором компонентов. Принимает `ecs_entity_desc_t` и `ecs_value(T, {...})`.                                                                                                       |
| **ecs_value**    | Макрос: `ecs_value(Position, {10, 20})` для передачи значения компонента в `ecs_insert` и подобные вызовы.                                                                                         |

---

## Запросы и системы

| Термин             | Объяснение                                                                                                                                                                                                              |
|--------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Query**          | Поиск и итерация entities по условиям. C++: `world.query_builder<Position, Velocity>().build()`, `q.each([](Position& p, Velocity& v){})`. Cached (по умолчанию) — быстрая итерация; uncached — меньше памяти.          |
| **Term**           | Элемент query — условие матчинга («имеет Position», «имеет pair (ChildOf, parent)»).                                                                                                                                    |
| **Field**          | Массив значений, возвращаемый итератором для каждого term.                                                                                                                                                              |
| **each vs iter**   | `each([](T& t){})` — callback по одной entity. `iter([](iter& it, size_t i, T& t){})` — доступ к batch: `it.entity(i)`, `it.count()`, `it.delta_time()`.                                                                |
| **System**         | Query + callback. C++: `world.system<Position, Velocity>().each(...)`. Запуск через `world.progress()` или вручную `system.run()`.                                                                                      |
| **Pipeline**       | Список фаз (phase tags), при матче которых возвращаются системы для выполнения. При `ecs_progress`/`world.progress()` системы выполняются в порядке фаз.                                                                |
| **Phase**          | Тег (например, `EcsOnUpdate`), добавленный к системе. Определяет порядок выполнения в pipeline. Фазы по умолчанию: OnLoad, PostLoad, PreUpdate, OnUpdate, OnValidate, PostUpdate, PreStore, OnStore.                    |
| **Observer**       | Query + callback, вызываемый при событиях (OnAdd, OnSet, OnRemove и др.). Реагирует на изменения ECS, а не выполняется каждый кадр.                                                                                     |
| **Event**          | Встроенные: `EcsOnAdd`, `EcsOnRemove`, `EcsOnSet` и др. Пользовательские события тоже поддерживаются.                                                                                                                   |
| **ecs_progress**   | Функция C API. Запускает pipeline (все системы в фазах) и возвращает `true`, пока приложение должно продолжать работу. Передаётся `delta_time` для систем. Для выхода вызвать `ecs_quit(world)` — тогда вернёт `false`. |
| **world.progress** | Метод C++ API. Аналог `ecs_progress(world, delta_time)`. По умолчанию `delta_time = 0`.                                                                                                                                 |
| **EcsNot**         | Оператор term: исключить entities с компонентом. В C: `{ .id = ecs_id(Position), .oper = EcsNot }`.                                                                                                                     |
| **EcsOptional**    | Оператор term: компонент необязателен.                                                                                                                                                                                  |
| **EcsOr**          | Оператор: матч по одному из terms.                                                                                                                                                                                      |
| **Source**         | Entity, на которой проверяется term. По умолчанию — self. Можно задать `parent()` — компонент с родителя; `cascade()` — обход иерархии.                                                                                 |
| **Traversal**      | Обход иерархии в query. `term_at(1).parent()` — матч компонента с родителя; `cascade()` — breadth-first. Подробнее: [concepts](concepts.md#traversal-cascade-parent).                                                   |

---

## Иерархии и связи

| Термин             | Объяснение                                                                                                                                                  |
|--------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **EcsChildOf**     | Встроенный тег (relationship) для иерархии родитель–ребёнок. `ecs_entity_t EcsChildOf` в C, `flecs::ChildOf` в C++. При удалении родителя удаляются и дети. |
| **child_of**       | Метод C++: `entity.child_of(parent)` — добавляет pair (ChildOf, parent).                                                                                    |
| **ecs_new_w_pair** | Создаёт entity и сразу добавляет pair. `ecs_new_w_pair(world, EcsChildOf, parent)`.                                                                         |
| **ecs_get_parent** | Возвращает родителя entity по `EcsChildOf`.                                                                                                                 |
| **ecs_get_path**   | Возвращает путь entity в иерархии (например, `"parent.child"`).                                                                                             |
| **ecs_lookup**     | Поиск entity по имени. С иерархией: `ecs_lookup(world, "parent.child")`.                                                                                    |
| **ecs_get_target** | Возвращает target для relationship. Например, `ecs_get_target(world, Alice, Likes, 0)` — первый, кого Alice «любит».                                        |
| **Wildcard**       | Специальный id `EcsWildcard` для запросов — матчит любое значение (например, «все дети любого родителя»).                                                   |
| **Cascade**        | Обход иерархии в breadth-first при query traversal. Удобно для transform-систем.                                                                            |
| **EcsPrefab**      | Тег для entity-шаблона (prefab). Компоненты prefab наследуются instance'ами.                                                                                |
| **EcsIsA**         | Relationship: entity «является» instance prefab. `ecs_new_w_pair(world, EcsIsA, prefab)` создаёт instance.                                                  |

---

## Прочее

| Термин             | Объяснение                                                                                                                                                                                  |
|--------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **flecs::iter**    | C++ итератор в callback системы. `it.entity(i)`, `it.count()`, `it.delta_time()`, `it.world()`. См. [api-reference](api-reference.md#iterator-ecs_iter_t).                                  |
| **ecs_iter_t**     | C итератор. Поля: `entities`, `count`, `delta_time`, `world`, `param`. Компоненты — макрос `ecs_field(it, Type, index)` ([flecs_c.h](../../external/flecs/include/flecs/addons/flecs_c.h)). |
| **ecs_field**      | C макрос: `ecs_field(it, Position, 0)` возвращает `Position*` на массив размера `it->count`. Индекс — номер term (0-based).                                                                 |
| **ecs_each**       | C макрос: `ecs_iter_t it = ecs_each(world, Position); while (ecs_each_next(&it)) { ... }` — итерация по одному компоненту без query.                                                        |
| **Module**         | C++: `struct M { M(flecs::world& w){ ... } }; world.import<M>()`. C: `ECS_IMPORT(world, MyModule)`. Дети модуля — по ChildOf.                                                               |
| **Addon**          | Опциональный модуль flecs. При `FLECS_CUSTOM_BUILD` (distr) — кастомный набор.                                                                                                              |
| **FLECS_DEBUG**    | Макрос. Assert'ы и проверки. Замедляет, помогает при отладке.                                                                                                                               |
| **Flecs Explorer** | Веб-инструмент для entities, компонентов, запросов. Требует REST addon.                                                                                                                     |

---

## См. также

- [Основные понятия](concepts.md) — ECS, pipeline, singleton, traversal.
- [Справочник API](api-reference.md) — сигнатуры, ссылки
  на [flecs.h](../../external/flecs/include/flecs.h), [flecs.hpp](../../external/flecs/include/flecs/addons/cpp/flecs.hpp).
