# Решение проблем flecs

🟡 **Уровень 2: Средний**

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
