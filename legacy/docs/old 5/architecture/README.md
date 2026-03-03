# Архитектура ProjectV

Документация по архитектуре воксельного движка ProjectV, разделённая на теоретические основы и практическую реализацию.

---

## Start Here

**[Roadmap & Scope](00_roadmap_and_scope.md)** — MVP vs Vision, границы проекта, фазы разработки.

---

## Теория (`theory/`)

Фундаментальные концепции Data-Oriented Design (DOD) и Entity Component System (ECS):

| # | Документ                                                  | Описание                                           |
|---|-----------------------------------------------------------|----------------------------------------------------|
| 1 | [ECS Concepts](theory/01_ecs-concepts.md)                 | Основные элементы Entity Component System          |
| 2 | [Memory Layout](theory/02_memory-layout.md)               | Организация данных в памяти для производительности |
| 3 | [System Communication](theory/03_system-communication.md) | Взаимодействие систем в ECS                        |
| 4 | [Caching Strategies](theory/04_caching.md)                | Стратегии работы с кэшем CPU                       |
| 5 | [Memory Arenas](theory/05_memory-arenas.md)               | Аллокаторы памяти для игрового движка              |
| 6 | [Composition Patterns](theory/06_composition.md)          | Композиция поведения в ECS                         |

---

## Практика (`practice/`)

Практическая реализация архитектурных паттернов в ProjectV:

| #  | Документ                                                  | Описание                                     |
|----|-----------------------------------------------------------|----------------------------------------------|
| 1  | [Core Loop](practice/01_core-loop.md)                     | Гибридный игровой цикл с SDL, Vulkan, Flecs  |
| 2  | [Resource Management](practice/02_resource-management.md) | Централизованное управление ресурсами Vulkan |
| 3  | [Voxel Pipeline](practice/03_voxel-pipeline.md)           | GPU-driven рендеринг вокселей                |
| 4  | [Modern Vulkan Guide](practice/04_modern-vulkan-guide.md) | Vulkan 1.4 с dynamic rendering, mesh shaders |
| 5  | [Flecs-Vulkan Bridge](practice/05_flecs-vulkan-bridge.md) | Интеграция ECS с Vulkan                      |
| 6  | [Jolt-Vulkan Bridge](practice/06_jolt-vulkan-bridge.md)   | Синхронизация физики и рендеринга            |
| 7  | [Coordinate Systems](practice/07_coordinate-systems.md)   | Системы координат в ProjectV                 |
| 8  | [Camera Controller](practice/08_camera-controller.md)     | FPS-камера, frustum culling чанков           |
| 9  | [Input System](practice/09_input-system.md)               | Стек слоёв ввода: ImGui, debug-консоль       |
| 10 | [Serialization](practice/11_serialization.md)             | Сохранение воксельного мира: Zstd, форматы   |
| 11 | [Custom Allocators](practice/12_custom-allocators.md)     | Аллокаторы для горячих путей                 |
| 12 | [Reflection](practice/13_reflection.md)                   | Система рефлексии для инструментов           |
| 13 | [Team Workflow](practice/14_team-workflow.md)             | Организация работы команды                   |
| 14 | [GPU Debugging](practice/15_gpu-debugging.md)             | RenderDoc, NVIDIA Nsight                     |
| 15 | [Hot-Reload](practice/16_hot-reload.md)                   | Быстрая итерация без перезапуска             |

---

## Рекомендуемый путь изучения

### Для новых разработчиков

```
theory/01_ecs-concepts.md → theory/02_memory-layout.md
         ↓
practice/01_core-loop.md → practice/02_resource-management.md
         ↓
practice/03_voxel-pipeline.md → practice/07_coordinate-systems.md
```

### Для опытных разработчиков

```
practice/04_modern-vulkan-guide.md
         ↓
practice/05_flecs-vulkan-bridge.md → practice/06_jolt-vulkan-bridge.md
         ↓
practice/03_voxel-pipeline.md
```

---

## Уровни сложности

| Уровень        | Обозначение | Описание                                        |
|----------------|-------------|-------------------------------------------------|
| **🟢 Level 1** | Начальный   | Базовые концепции, введение в архитектуру       |
| **🟡 Level 2** | Средний     | Практическая реализация, интеграция компонентов |
| **🔴 Level 3** | Продвинутый | Специфичные для ProjectV сценарии, оптимизации  |

---

## Связи с другими разделами

### Библиотеки

- **[Vulkan](../libraries/vulkan/)** — Графический API
- **[Flecs](../libraries/flecs/)** — ECS фреймворк
- **[JoltPhysics](../libraries/joltphysics/)** — Физический движок

### Руководства

- **[C++ Handbook](../guides/cpp/00_overview.md)** — Modern C++ для движка
- **[CMake Guide](../guides/cmake/00_overview.md)** — Сборка проекта

### Философия

- **[DOD Philosophy](../philosophy/03_dod-philosophy.md)** — Data-Oriented Design
- **[ECS Philosophy](../philosophy/04_ecs-philosophy.md)** — Entity Component System

---

## Примеры кода

Примеры архитектурных паттернов в quickstart документации:

- [SDL Quickstart](../libraries/sdl/01_quickstart.md) — Базовое окно
- [Vulkan Quickstart](../libraries/vulkan/01_quickstart.md) — Vulkan треугольник
- [Flecs Quickstart](../libraries/flecs/01_quickstart.md) — Интеграция ECS
- [JoltPhysics Quickstart](../libraries/joltphysics/01_quickstart.md) — Физика
