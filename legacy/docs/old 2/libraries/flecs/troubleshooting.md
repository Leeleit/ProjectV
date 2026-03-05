# Решение проблем

**🟡 Уровень 2: Средний**

Частые ошибки при использовании flecs и способы их исправления.

## На этой странице

**Сборка:
** [undefined reference](#undefined-reference-to-ecs_--flecs) · [flecs не линкуется](#flecs-не-линкуется--закомментирован-в-cmakelists) · [Не найден flecs.h](#не-найден-flecsh--ошибка-include) · [C++17](#c17-требуется)

**Runtime:
** [Query возвращает 0 entities](#query-возвращает-0-entities) · [Система не выполняется](#система-не-выполняется--entities-не-матчатся-query) · [Удалённая entity](#использование-удалённой-entity-crash-некорректное-поведение)

**Логика:
** [ecs_quit](#ecs_quit-не-срабатывает) · [Компонент не найден](#компонент-не-найден--не-зарегистрирован) · [Дублирование по имени](#дублирование-entity-по-имени)

**Производительность:
** [Cached vs uncached](#когда-использовать-cached-и-uncached-query) · [Медленная итерация](#медленная-итерация-query)

**Vulkan:** [Компонент с Vulkan handle](#компонент-с-vulkan-handle)

**Прочее:** [Отладка](#flecs_debug-и-flecs-explorer) · [Многопоточность](#многопоточность)

---

## Сборка

### undefined reference to ecs_* / flecs::*

**Причина:** flecs не линкован с приложением. Цели `flecs::flecs_static` или `flecs::flecs` не добавлены в
`target_link_libraries`.

**Решение:**

1. Убедитесь, что в CMakeLists.txt есть:
   ```cmake
   add_subdirectory(external/flecs)
   target_link_libraries(ProjectV PRIVATE flecs::flecs_static)
   ```
2. Пересоберите проект (`cmake --build build`).
3. На Windows flecs статически линкует `ws2_32`, `dbghelp`; на Linux — `pthread`. CMake flecs подтягивает их сам.

---

### flecs не линкуется — закомментирован в CMakeLists

**Причина:** В ProjectV flecs по умолчанию закомментирован в [CMakeLists.txt](../../CMakeLists.txt).

**Решение:** Раскомментируйте:

```cmake
add_subdirectory(external/flecs)
# ...
target_link_libraries(ProjectV PRIVATE
    # ...
    flecs::flecs_static
)
```

---

### Не найден flecs.h / ошибка include

**Причина:** Include-путь flecs не добавлен к таргету. Обычно при `target_link_libraries(ProjectV flecs::flecs_static)`
include передаётся транзитивно.

**Решение:**

1. Убедитесь, что `flecs::flecs_static` в `target_link_libraries`.
2. Используйте `#include "flecs.h"` (не `flecs/flecs.h` — зависит от установки).
3. При ручном указании: `target_include_directories(ProjectV PRIVATE external/flecs/include)`.

---

### C++17 требуется

**Причина:** C++ API flecs требует C++17. Ошибки вида «structured bindings» или «if constexpr» указывают на старый
стандарт.

**Решение:** В CMakeLists.txt:

```cmake
set(CMAKE_CXX_STANDARD 17)
# или 20, 23
```

---

## Runtime

### Query возвращает 0 entities

**Причина:** Query матчит только entities, удовлетворяющие всем terms. Пустой результат — entity без нужных компонентов,
неверные операторы или hierarchy.

**Решение:**

1. У entity должны быть **все** компоненты из query (кроме optional). Проверьте: `entity.has<Position>()`,
   `entity.type()`.
2. `oper(flecs::Not)` и `without<T>()` — исключают entities. Убедитесь, что не исключаете всех.
3. `term_at(i).parent()` — компонент матчится с родителя. Entity должна быть ребёнком (`child_of(parent)`).
4. Singleton: `world.set<Gravity>({9.81})` — иначе query с Gravity не матчит.

См. [Основные понятия — Query](concepts.md#query-optional-и-not), [API — Query](api-reference.md#query).

---

### Система не выполняется / entities не матчатся query

**Причина:** Query требует определённый набор компонентов. Entity без одного из них не попадёт в итерацию.

**Решение:**

1. Проверьте, что у entity есть **все** компоненты из query:
   ```cpp
   // Система: Position + Velocity
   world.system<Position, Velocity>()...
   // Entity должна иметь ОБА компонента
   e.set<Position>({0,0}).set<Velocity>({1,0});
   ```
2. Убедитесь, что используется `const` только для read-only: `world.system<Position, const Velocity>()` — оба компонента
   должны присутствовать.
3. Проверьте операторы в query: `oper(flecs::Not)` исключает entities с компонентом; `Optional` делает компонент
   необязательным.
4. Для иерархии: `term_at(1).parent()` матчит компонент с родителя — entity должна быть ребёнком.

---

### Использование удалённой entity (crash, некорректное поведение)

**Причина:** После `ecs_delete` / `entity.destruct()` id переиспользуется. Верхние 32 бита `ecs_entity_t` — версия; при
переиспользовании она увеличивается. Старая ссылка с `entity.is_alive() == false` — работа с нею ведёт к undefined
behavior.

**Решение:**

1. Проверяйте живость: `ecs_is_alive(world, e)` или `entity.is_alive()`.
2. Не храните id дольше, чем entity существует, без проверки.
3. FLECS_DEBUG — assert при обращении к удалённой entity.

См. [Основные понятия — Версионирование](concepts.md#версионирование-entity).

---

## Производительность

### Когда использовать cached и uncached query

**Cached query** (по умолчанию для систем): быстрая итерация, больше памяти, дольше создание. Подходит для запросов,
выполняемых каждый кадр.

**Uncached query**: мало памяти, быстрое создание, медленная итерация. Подходит для ad-hoc запросов (например, «найти
всех детей parent» один раз).

**Решение:**

- Системы по умолчанию кешируются — менять не нужно.
- Для ручных uncached запросов: `.cache_kind(flecs::QueryCacheNone)` до `build()`.

---

### Медленная итерация query

**Причина:** Uncached query ищет archetypes при каждой итерации — медленно при частом вызове.

**Решение:** Для запросов, выполняемых каждый кадр, используйте cached (по умолчанию). Uncached — только для редких
ad-hoc запросов (например, «все дети parent» один раз).

См. [API — Query](api-reference.md#query).

---

## Отладка

### FLECS_DEBUG и Flecs Explorer

**FLECS_DEBUG:** Включает проверки и assert'ы. При ошибках (обращение к удалённой entity, неверные параметры) flecs
может сразу указать причину.

**Решение:**

```cmake
target_compile_definitions(ProjectV PRIVATE FLECS_DEBUG)
```

**Flecs Explorer:** Веб-инструмент для просмотра entities, компонентов, систем. Требует addon REST/HTTP.
Документация: [flecs.dev/explorer](https://flecs.dev/explorer).

---

## Логика и выход

### ecs_quit не срабатывает

**Причина:** `ecs_progress` по умолчанию всегда возвращает `true`. Без вызова `ecs_quit` цикл не завершится.

**Решение:** Вызовите `ecs_quit(world)` (C) или `world.quit()` (C++) — из системы, observer или при обработке события
закрытия. Следующий `ecs_progress` вернёт `false`.

---

### Компонент не найден / не зарегистрирован

**Причина:** В C компоненты нужно регистрировать через `ECS_COMPONENT(world, Position)` до использования. В C++
регистрация автоматическая при первом `entity.set<Position>()` или `world.component<Position>()` — тип должен быть виден
компилятору.

**Решение:**

- C: вызывайте `ECS_COMPONENT` для каждого компонента перед `ecs_set`, `ecs_add` и в query/system.
- C++: убедитесь, что тип используется хотя бы раз (например, в системе или `world.component<T>()`).

---

### Дублирование entity по имени

**Причина:** `world.entity("Name")` при существующей entity с таким именем возвращает её, а не создаёт новую. Это
ожидаемое поведение — имена уникальны в scope.

**Решение:** Для создания новой entity с тем же «логическим» именем используйте разный scope или суффикс:
`world.entity("Enemy1")`, `world.entity("Enemy2")` или вложенная иерархия.

---

## Vulkan

### Компонент с Vulkan handle

**Причина:** Компонент хранит `VkBuffer`, `VmaAllocation` и т.д. При удалении entity или world handle остаётся
неосвобождённым.

**Решение:**

1. **Observer OnRemove** — при удалении компонента освободить handle (`vmaDestroyBuffer` и т.д.).
2. **Порядок выхода:** `vkQueueWaitIdle` → destroy swapchain → destroy world (вызовет OnRemove) → destroy
   device/instance.
3. Не храните device/allocator только в компоненте — передавайте в observer через closure или ctx.

См. [Интеграция — flecs + Vulkan](integration.md#7-связка-flecs--vulkan).

---

## Многопоточность

`ecs_set_threads(world, n)` и `multi_threaded` в system builder. Системы с `immediate = true` не могут быть
`multi_threaded`. [Systems.md](../../external/flecs/docs/Systems.md).

**Vulkan:** запись в CommandBuffer не потокобезопасна. Системы рендеринга не должны использовать `multi_threaded()`.
Физику можно вынести в отдельные потоки, рендер — в main.

---

## См. также

- [Интеграция](integration.md) — CMake, порядок вызовов
- [Основные понятия](concepts.md) — Query, singleton, hierarchy
- [Справочник API](api-reference.md)
- [BuildingFlecs.md](../../external/flecs/docs/BuildingFlecs.md)
