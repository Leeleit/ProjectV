# Справочник API flecs

**🟡 Уровень 2: Средний**

Краткое описание основных функций и типов flecs. C++ — в приоритете; C — в подразделах. Полный
API: [flecs.h](../../external/flecs/include/flecs.h), [flecs.hpp](../../external/flecs/include/flecs/addons/cpp/flecs.hpp).
Doxygen: [flecs.dev](https://www.flecs.dev/flecs/).

## Карта заголовков

| Файл                                                                                              | Назначение                                   |
|---------------------------------------------------------------------------------------------------|----------------------------------------------|
| [flecs.h](../../external/flecs/include/flecs.h)                                                   | Точка входа, C API                           |
| [flecs/addons/flecs_c.h](../../external/flecs/include/flecs/addons/flecs_c.h)                     | C макросы (ECS_COMPONENT, ecs_set, ecs_each) |
| [flecs/addons/cpp/flecs.hpp](../../external/flecs/include/flecs/addons/cpp/flecs.hpp)             | C++ точка входа                              |
| [flecs/addons/cpp/entity.hpp](../../external/flecs/include/flecs/addons/cpp/entity.hpp)           | flecs::entity                                |
| [flecs/addons/cpp/mixins/query](../../external/flecs/include/flecs/addons/cpp/mixins/query)       | query_builder                                |
| [flecs/addons/cpp/mixins/system](../../external/flecs/include/flecs/addons/cpp/mixins/system)     | system_builder                               |
| [flecs/addons/cpp/mixins/observer](../../external/flecs/include/flecs/addons/cpp/mixins/observer) | observer                                     |
| [flecs/addons/system.h](../../external/flecs/include/flecs/addons/system.h)                       | ecs_system_desc_t                            |

## На этой странице

- [Карта заголовков](#карта-заголовков)
- [Когда что использовать](#когда-что-использовать)
- [World](#world)
- [Entity](#entity)
- [Component](#component)
- [Query](#query)
- [System](#system)
- [Pipeline и фазы](#pipeline-и-фазы)
- [Observer](#observer)
- [Pairs и relationships](#pairs-и-relationships)
- [Hierarchy](#hierarchy)
- [Модули](#модули)
- [Prefab](#prefab)
- [Iterator (ecs_iter_t)](#iterator-ecs_iter_t)
- [Операторы query](#операторы-query)

---

## Когда что использовать

| Задача             | Функция / API                                                | C / C++ |
|--------------------|--------------------------------------------------------------|---------|
| Создать world      | `ecs_init()` / `flecs::world`                                | C / C++ |
| Уничтожить world   | `ecs_fini(world)` / деструктор                               | C / C++ |
| Запустить pipeline | `ecs_progress(world, dt)` / `world.progress(dt)`             | C / C++ |
| Создать entity     | `ecs_new(world)` / `world.entity()`                          | C / C++ |
| Добавить компонент | `ecs_add`, `ecs_set` / `entity.set<T>()`                     | C / C++ |
| Получить компонент | `ecs_get` / `entity.get<T>()`                                | C / C++ |
| Запрос             | `ecs_query` / `world.query_builder<T...>().build()`          | C / C++ |
| Система            | `ecs_system`, `ECS_SYSTEM` / `world.system<T...>()`          | C / C++ |
| Observer           | `ecs_observer` / `world.observer<T...>()`                    | C / C++ |
| Иерархия           | `ecs_new_w_pair`, `ecs_add_pair` / `entity.child_of(parent)` | C / C++ |
| Pair               | `ecs_add_pair`, `ecs_get_target` / `entity.add(Rel, Target)` | C / C++ |
| Prefab             | `ecs_new_w_pair(EcsIsA, prefab)` / `entity.is_a(prefab)`     | C / C++ |

---

## World

### C++ API

```cpp
flecs::world ecs;           // конструктор = ecs_init
// деструктор = ecs_fini

ecs.progress();              // delta_time = 0
ecs.progress(1.0f/60.0f);   // с delta_time
```

### C API

```c
ecs_world_t* ecs_init(void);
int ecs_fini(ecs_world_t *world);
bool ecs_progress(ecs_world_t *world, ecs_ftime_t delta_time);
```

- **ecs_init** — создаёт world. **ecs_fini** — уничтожает. **ecs_progress** — pipeline, `delta_time` в системы.
  Возвращает `true` до `ecs_quit`.

---

## Entity

### C++ API

```cpp
auto e = world.entity();                    // создать
auto e2 = world.entity("Name");             // с именем (или найти существующую)
auto e3 = world.entity().child_of(parent);  // ребёнок parent

e.set<Position>({10, 20});
e.add<Velocity>();
const Position* p = e.get<Position>();
e.remove<Position>();
e.destruct();                               // удалить
e.is_alive();
```

### C API

```c
ecs_entity_t ecs_new(ecs_world_t *world);
ecs_entity_t ecs_new_w_pair(world, EcsChildOf, parent);
void ecs_delete(world, entity);
void ecs_add/ecs_remove/ecs_clear(world, entity, id);
void ecs_set(world, e, Position, {10, 20});  // макрос, [flecs_c.h](../../external/flecs/include/flecs/addons/flecs_c.h)
const void* ecs_get(world, entity, id);
void* ecs_ensure(world, entity, id);
```

---

## Component

### C API

Компоненты регистрируются макросами:

```c
ECS_COMPONENT(world, Position);
ECS_TAG(world, Enemy);   // tag без данных

ecs_entity_t id = ecs_id(Position);  // id компонента
```

### C++ API

Регистрация автоматическая при первом `entity.set<Position>()` или `world.component<Position>()`.

```cpp
world.component<Position>();  // явная регистрация
world.entity<Position>();     // entity, представляющая компонент Position
```

---

## Query

### C++ API

**Query builder** — [mixins/query](../../external/flecs/include/flecs/addons/cpp/mixins/query):

```cpp
// Простой each (ad-hoc query)
world.each([](Position& p, Velocity& v) { ... });

// Query builder
auto q = world.query_builder<Position, Velocity>()
    .with(flecs::ChildOf, parent)           // pair (ChildOf, parent)
    .without<Dead>()                        // без Dead
    .term_at(2).optional()                  // Velocity необязателен
    .term_at(2).parent().cascade()          // второй term — от родителя, обход иерархии
    .build();
q.each([](flecs::entity e, Position& p, Velocity* v) { ... });
q.destruct();  // или RAII
```

Методы: `with<T>()`, `without<T>()`, `optional()` (на term), `oper(flecs::Not)`, `term_at(i).parent()`,
`term_at(i).cascade()`.

### C API

В `ecs_query_desc_t` — `.terms`, не `.filter.terms` ([flecs.h](../../external/flecs/include/flecs.h)):

```c
ecs_query_t *q = ecs_query(world, {
    .terms = {{ ecs_id(Position) }, { ecs_id(Velocity) }}
});
ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_query_next(&it)) {
    Position *p = ecs_field(&it, Position, 0);
    Velocity *v = ecs_field(&it, Velocity, 1);
    for (int i = 0; i < it.count; i++) { /* p[i], v[i] */ }
}
ecs_query_fini(q);
```

Простая итерация: `ecs_each(world, Position)` + `ecs_each_next`.

---

## Операторы query

Операторы задают условия для terms. В C — через `term.oper`, в C++ — методы `oper()`, `optional()`.

| Оператор      | C                                                 | C++                                  | Описание                                        |
|---------------|---------------------------------------------------|--------------------------------------|-------------------------------------------------|
| `EcsAnd`      | по умолчанию                                      | —                                    | Entity должна иметь компонент                   |
| `EcsNot`      | `{ .id = ecs_id(Position), .oper = EcsNot }`      | `.with<Position>().oper(flecs::Not)` | Исключить entities с компонентом                |
| `EcsOptional` | `{ .id = ecs_id(Position), .oper = EcsOptional }` | `.with<Position>().optional()`       | Компонент необязателен                          |
| `EcsOr`       | —                                                 | —                                    | Матч по одному из terms (специальный синтаксис) |

---

## System

### C++ API

**System builder** — [mixins/system](../../external/flecs/include/flecs/addons/cpp/mixins/system):

```cpp
world.system<Position, const Velocity>("Move")
    .kind(flecs::OnUpdate)       // фаза
    .interval(0.1)              // запуск раз в 0.1 сек
    .multi_threaded()           // параллельное выполнение
    .ctx(&my_context)           // user data → it.ctx()
    .each([](Position& p, const Velocity& v) {
        p.x += v.x; p.y += v.y;
    });

// С iter — доступ к batch, delta_time, entity:
world.system<Position, Velocity>("Move")
    .each([](flecs::iter& it, size_t i, Position& p, Velocity& v) {
        p.x += v.x * it.delta_time();
        p.y += v.y * it.delta_time();
        flecs::entity e = it.entity(i);  // entity для индекса i
    });
```

Методы: `kind(Phase)`, `interval(sec)`, `rate(tick_source, freq)`, `multi_threaded()`, `ctx(ptr)`.

### C API

`ECS_SYSTEM(world, Name, Phase, Comp1, Comp2)`. Доп. параметры через
`ecs_system_desc_t` ([system.h](../../external/flecs/include/flecs/addons/system.h)): phase, interval, rate,
multi_threaded.

---

## Pipeline и фазы

Фазы (phase tags) для `kind()` / `ecs_dependson`:

| Фаза            | Описание              |
|-----------------|-----------------------|
| `EcsOnLoad`     | Загрузка              |
| `EcsPostLoad`   | После загрузки        |
| `EcsPreUpdate`  | До обновления         |
| `EcsOnUpdate`   | Основная логика       |
| `EcsOnValidate` | Валидация             |
| `EcsPostUpdate` | После обновления      |
| `EcsPreStore`   | До сохранения         |
| `EcsOnStore`    | Отрисовка, сохранение |

---

## Observer

### C++ API

```cpp
world.observer<Position>()
    .event(flecs::OnSet)
    .each([](Position& p) { /* Position установлен */ });
```

**События** ([ObserversManual.md](../../external/flecs/docs/ObserversManual.md)):

| Событие           | Когда                               |
|-------------------|-------------------------------------|
| `flecs::OnAdd`    | Компонент добавлен                  |
| `flecs::OnRemove` | Компонент удалён                    |
| `flecs::OnSet`    | Значение установлено/изменено       |
| `flecs::UnSet`    | Значение сброшено (перед удалением) |
| `flecs::OnAdd     | OnRemove`                           | Добавление или удаление |
| `flecs::OnRemove  | OnDelete`                           | Entity удаляется |

### C API

`ECS_OBSERVER(world, Callback, Event, Component)`. Или `ecs_observer` с `.query.terms`, `.events`, `.callback`.

---

## Pairs и relationships

### C API

```c
ecs_add_pair(world, entity, relationship, target);
ecs_remove_pair(world, entity, relationship, target);
bool ecs_has_pair(world, entity, relationship, target);
ecs_entity_t ecs_get_target(world, entity, relationship, int32_t index);

ecs_id_t pair = ecs_pair(relationship, target);
```

### C++ API

```cpp
entity.add(Likes, Alice);       // add pair (Likes, Alice)
entity.remove(Likes, Alice);
entity.has(Likes, Alice);
entity.target<Likes>();         // первый target для Likes

world.pair<Likes>(Alice);       // id для pair
```

---

## Hierarchy

### C API

```c
ecs_entity_t child = ecs_new_w_pair(world, EcsChildOf, parent);
ecs_entity_t parent = ecs_get_parent(world, entity);
char* path = ecs_get_path(world, parent, entity);
ecs_entity_t e = ecs_lookup(world, "parent.child");
```

### C++ API

```cpp
auto child = world.entity().child_of(parent);
auto parent = entity.parent();
entity.path();                  // "parent::child"
world.lookup("parent::child");
parent.lookup("child");
```

---

## Модули

### C API

```c
void MyModuleImport(ecs_world_t *world);

void MyModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, MyModule);
    ECS_COMPONENT_DEFINE(world, Position);
    // системы, observers...
}

ECS_IMPORT(world, MyModule);
```

### C++ API

```cpp
struct my_module {
    my_module(flecs::world& w) {
        w.module<my_module>();
        w.component<Position>();
        w.system<Position>("Move").each(...);
    }
};

world.import<my_module>();
```

---

## Prefab

Prefab — entity-шаблон. Instance наследует компоненты prefab через pair `(EcsIsA, prefab)`.

### C++ API

```cpp
auto SpaceShip = world.prefab("SpaceShip").set(Defense{50});
auto inst = world.entity().is_a(SpaceShip);
const Defense& d = inst.get<Defense>();  // наследовано от prefab

// Override на instance — переопределить значение:
inst.set<Defense>({100});  // теперь у instance своё значение
```

### C API

`ecs_entity(world, {.name = "...", .add = ecs_ids(EcsPrefab)})`, `ecs_new_w_pair(world, EcsIsA, prefab)`.
Подробнее: [PrefabsManual.md](../../external/flecs/docs/PrefabsManual.md).

---

## Iterator (ecs_iter_t)

### C++ API (flecs::iter)

В callback с сигнатурой `(flecs::iter& it, size_t i, T& t, ...)`:

| Метод             | Описание                               |
|-------------------|----------------------------------------|
| `it.entity(i)`    | Entity для индекса `i` в batch         |
| `it.count()`      | Количество entities в текущей итерации |
| `it.delta_time()` | Время из `ecs_progress`                |
| `it.world()`      | World                                  |
| `it.ctx<T>()`     | User data из `system.ctx(ptr)`         |

**each vs iter:** `each([](T& t){})` — callback по одной entity, без доступа к batch.
`iter([](iter& it, size_t i, T& t){})` — доступ к `it.entity(i)`, `it.delta_time()` и т.д.

### C API (ecs_iter_t)

Поля: `entities`, `count`, `delta_time`, `world`, `param`. Компоненты: `ecs_field(it, Type, index)` — `Type*` на
массив. [flecs.h](../../external/flecs/include/flecs.h).

---

## См. также

- [Глоссарий](glossary.md)
- [Основные понятия](concepts.md)
- [Карта заголовков](#карта-заголовков) — [flecs.h](../../external/flecs/include/flecs.h), [flecs.hpp](../../external/flecs/include/flecs/addons/cpp/flecs.hpp)
