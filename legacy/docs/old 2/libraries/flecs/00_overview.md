# Обзор flecs

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
