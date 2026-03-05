# Архитектура ProjectV

Документация по архитектуре воксельного движка ProjectV, разделённая на **Architecture Decision Records (ADR)**,
теоретические основы и практическую реализацию.

---

## Start Here

**[Roadmap & Scope](00_roadmap_and_scope.md)** — MVP vs Vision, границы проекта, фазы разработки.

---

## Architecture Decision Records (`adr/`)

Ключевые архитектурные решения с обоснованием:

| ADR                                        | Название             | Краткое описание                             |
|--------------------------------------------|----------------------|----------------------------------------------|
| [0001](adr/0001-vulkan-renderer.md)        | **Vulkan Renderer**  | Vulkan 1.4 + Volk + VMA + RAII wrappers      |
| [0002](adr/0002-svo-storage.md)            | **SVO Storage**      | 64-bit nodes, std430 GPU layout, std::mdspan |
| [0003](adr/0003-ecs-architecture.md)       | **ECS Architecture** | Flecs integration, components, systems       |
| [0004](adr/0004-build-and-modules-spec.md) | **Build & Modules**  | C++26 Modules, Global Module Fragment        |

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

### Фундамент (P0)

| #  | Документ                                                  | Описание                                     |
|----|-----------------------------------------------------------|----------------------------------------------|
| 0a | [Engine Structure](practice/00_engine-structure.md)       | Структура движка, C++26 Modules              |
| 0b | [SVO Architecture](practice/00_svo-architecture.md)       | SVO + Chunks hybrid, std::mdspan 🔴          |
| 1  | [Core Loop](practice/01_core-loop.md)                     | Гибридный игровой цикл с SDL, Vulkan, Flecs  |
| 2  | [Resource Management](practice/02_resource-management.md) | Централизованное управление ресурсами Vulkan |
| 3  | [Voxel Pipeline](practice/03_voxel-pipeline.md)           | GPU-driven рендеринг вокселей 🔴             |
| 4  | [Modern Vulkan Guide](practice/04_modern-vulkan-guide.md) | Vulkan 1.4 с dynamic rendering, mesh shaders |

### Интеграции (P1)

| # | Документ                                                  | Описание                               |
|---|-----------------------------------------------------------|----------------------------------------|
| 5 | [Flecs-Vulkan Bridge](practice/05_flecs-vulkan-bridge.md) | Интеграция ECS с Vulkan                |
| 6 | [Jolt-Vulkan Bridge](practice/06_jolt-vulkan-bridge.md)   | Синхронизация физики и рендеринга      |
| 7 | [Coordinate Systems](practice/07_coordinate-systems.md)   | Системы координат в ProjectV           |
| 8 | [Camera Controller](practice/08_camera-controller.md)     | FPS-камера, frustum culling чанков     |
| 9 | [Input System](practice/09_input-system.md)               | Стек слоёв ввода: ImGui, debug-консоль |

### Инфраструктура (P1-P2)

| #  | Документ                                              | Описание                                   |
|----|-------------------------------------------------------|--------------------------------------------|
| 10 | [VOX Import](practice/10_vox-import.md)               | Импорт .VOX файлов (MagicaVoxel)           |
| 11 | [Serialization](practice/11_serialization.md)         | Сохранение воксельного мира: Zstd, форматы |
| 12 | [Custom Allocators](practice/12_custom-allocators.md) | Аллокаторы для горячих путей               |
| 13 | [Reflection](practice/13_reflection.md)               | Система рефлексии для инструментов         |
| 14 | [Team Workflow](practice/14_team-workflow.md)         | Организация работы команды                 |
| 15 | [GPU Debugging](practice/15_gpu-debugging.md)         | RenderDoc, NVIDIA Nsight 🔴                |
| 16 | [Hot-Reload](practice/16_hot-reload.md)               | Быстрая итерация без перезапуска           |
| 17 | [Asset Pipeline](practice/17_asset-pipeline.md)       | Offline Compiler для ассетов               |
| 18 | [Input Actions](practice/18_input-actions.md)         | Action Mapping система                     |
| 19 | [Game UI](practice/19_game-ui.md)                     | UI Strategy (ImGui/RmlUi)                  |
| 20 | [Material System](practice/20_material-system.md)     | PBR материалы для вокселей                 |

### Продвинутые (P2, Post-MVP)

| #  | Документ                                                      | Описание                             |
|----|---------------------------------------------------------------|--------------------------------------|
| 21 | [Networking Concept](practice/21_networking-concept.md)       | Networking (Post-MVP Vision) 🔴      |
| 23 | [GPU Cellular Automata](practice/23_gpu-cellular-automata.md) | GPU CA для жидкостей/сыпучих/огня 🔴 |
| 24 | [Network-Ready ECS](practice/24_network-ready-ecs.md)         | Репликация, Rollback/Prediction 🔴   |

---

## Рекомендуемый путь изучения

### Для новых разработчиков

```
ADR (0001-0004) → theory/01_ecs-concepts.md → theory/02_memory-layout.md
              ↓
practice/00_engine-structure.md → practice/01_core-loop.md
              ↓
practice/00_svo-architecture.md → practice/07_coordinate-systems.md
```

### Для опытных разработчиков

```
ADR (0001-0004) → practice/04_modern-vulkan-guide.md
              ↓
practice/05_flecs-vulkan-bridge.md → practice/06_jolt-vulkan-bridge.md
              ↓
practice/03_voxel-pipeline.md → practice/23_gpu-cellular-automata.md
```

---

## Уровни сложности

| Уровень    | Обозначение | Описание                                            |
|------------|-------------|-----------------------------------------------------|
| 🟢 Level 1 | Начальный   | Базовые концепции, введение в архитектуру           |
| 🟡 Level 2 | Средний     | Практическая реализация, интеграция компонентов     |
| 🔴 Level 3 | Продвинутый | Специфичные для ProjectV сценарии, GPU optimization |

---

## Связи с другими разделами

### Библиотеки (Integration Specifications)

| Библиотека | Спецификация                                                              |
|------------|---------------------------------------------------------------------------|
| SDL3       | [sdl_integration_spec.md](../libraries/sdl/sdl_integration_spec.md)       |
| Slang      | [slang_integration_spec.md](../libraries/slang/slang_integration_spec.md) |

### Руководства

- **[C++ Handbook](../guides/cpp/00_overview.md)** — Modern C++ для движка
- **[CMake Guide](../guides/cmake/00_overview.md)** — Сборка проекта

### Философия

- **[DOD Philosophy](../philosophy/03_dod-philosophy.md)** — Data-Oriented Design
- **[ECS Philosophy](../philosophy/04_ecs-philosophy.md)** — Entity Component System

---

## Краткое содержание ADR

### ADR-0001: Vulkan Renderer

**Решение:** Vulkan 1.4 + volk (статическая загрузка) + VMA (memory allocation) + RAII wrappers.

**Ключевые паттерны:**

- `std::expected<T, Error>` для error handling
- RAII wrappers для всех Vulkan objects
- Global Module Fragment для инкапсуляции C API

### ADR-0002: SVO Storage

**Решение:** Sparse Voxel Octree с 64-bit nodes, hybrid SVO + Chunks для масштабируемости.

**Ключевые паттерны:**

- `std::mdspan` для многомерных данных
- std430 GPU layout для direct buffer access
- Async rebuild на CPU → GPU upload

### ADR-0003: ECS Architecture

**Решение:** Flecs как ECS фреймворк с интеграцией SDL, Vulkan, JoltPhysics.

**Ключевые паттерны:**

- Components как POD structs
- Systems как flecs::system с explicit ordering
- Singletons для глобального состояния

### ADR-0004: Build & Modules

**Решение:** C++26 Modules с Global Module Fragment для C-библиотек.

**Ключевые паттерны:**

- `import std;` без fallback
- `module; #include <...>` для C headers
- Module partitions для внутренней организации
