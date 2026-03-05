# Архитектура ProjectV

Документация по архитектуре воксельного движка ProjectV, разделённая на **Architecture Decision Records (ADR)**,
теоретические основы и практическую реализацию.

---

## Start Here

**[Roadmap & Scope](roadmap_and_scope.md)** — MVP vs Vision, границы проекта, фазы разработки.

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

## Будущие механики (`future/`)

TODO

---

## Практика (`practice/`)

Практическая реализация архитектурных паттернов в ProjectV, организованная по категориям:

### 📦 01_core/ — Базовая архитектура

| # | Документ                                                      | Описание                                    |
|---|---------------------------------------------------------------|---------------------------------------------|
| 1 | [Engine Structure](practice/01_core/01_engine_structure.md)   | Структура движка, C++26 Modules             |
| 2 | [Core Loop](practice/01_core/02_core_loop.md)                 | Гибридный игровой цикл с SDL, Vulkan, Flecs |
| 3 | [Engine Bootstrap](practice/01_core/03_engine_bootstrap.md)   | Инициализация и запуск движка               |
| 4 | [Custom Allocators](practice/01_core/04_custom_allocators.md) | Аллокаторы для горячих путей                |
| 5 | [Job System](practice/01_core/05_job_system.md)               | P2300 std::execution integration            |
| 6 | [Zero Copy Memory](practice/01_core/06_zero_copy_memory.md)   | Разделение памяти CPU/GPU                   |
| 7 | [Error Handling](practice/01_core/07_error_handling.md)       | std::expected, error propagation            |
| 8 | [Shutdown Sequence](practice/01_core/08_shutdown_sequence.md) | Корректное завершение работы                |
| 9 | [C++26 Reality](practice/01_core/09_cpp26_reality.md)         | Glaze/PFR, PIMPL, modules workarounds       |

### 🎨 02_render/ — Vulkan и рендеринг

| # | Документ                                                | Описание                                    |
|---|---------------------------------------------------------|---------------------------------------------|
| 1 | [Vulkan Spec](practice/02_render/01_vulkan_spec.md)     | Vulkan 1.4, dynamic rendering, mesh shaders |
| 2 | [GPU Staging](practice/02_render/02_gpu_staging.md)     | Staging buffers, transfer contracts         |
| 3 | [GPU Debugging](practice/02_render/03_gpu_debugging.md) | RenderDoc, NVIDIA Nsight                    |
| 4 | [Render Graph](practice/02_render/04_render_graph.md)   | Frame graph, pass management                |

### 🧊 03_voxel/ — Воксельная система

| # | Документ                                                           | Описание                                |
|---|--------------------------------------------------------------------|-----------------------------------------|
| 1 | [SVO Architecture](practice/03_voxel/01_svo_architecture.md)       | SVO + Chunks hybrid, std::mdspan        |
| 2 | [Voxel Pipeline](practice/03_voxel/02_voxel_pipeline.md)           | GPU-driven рендеринг вокселей           |
| 3 | [Voxel Sync Pipeline](practice/03_voxel/03_voxel_sync_pipeline.md) | Синхронизация CPU/GPU воксельных данных |
| 4 | [SVO-CA Bridge](practice/03_voxel/04_svo_ca_bridge.md)             | Memory bridge для Cellular Automata     |

### ⚡ 04_physics_ca/ — Физика и Cellular Automata

| # | Документ                                                                            | Описание                             |
|---|-------------------------------------------------------------------------------------|--------------------------------------|
| 1 | [Jolt-Vulkan Bridge](practice/04_physics_ca/01_jolt_vulkan_bridge.md)               | Синхронизация физики и рендеринга    |
| 2 | [Physics-Voxel Integration](practice/04_physics_ca/02_physics_voxel_integration.md) | Интеграция физики с вокселями        |
| 3 | [GPU Cellular Automata](practice/04_physics_ca/03_gpu_cellular_automata.md)         | CA для жидкостей/сыпучих/огня        |
| 4 | [CPU-GPU Physics Sync](practice/04_physics_ca/04_cpu_gpu_physics_sync.md)           | Синхронизация физики между CPU и GPU |
| 5 | [CA-Physics Bridge](practice/04_physics_ca/05_ca_physics_bridge.md)                 | Мост между CA и физикой              |
| 6 | [Multithreading Contracts](practice/04_physics_ca/06_multithreading_contracts.md)   | TODO                                 |
| 7 | [Dynamic Voxel Entities](practice/04_physics_ca/07_dynamic_voxel_entities.md)       | Динамические воксельные объекты      |

### 🎮 05_ecs_gameplay/ — ECS и Gameplay

| # | Документ                                                                  | Описание                               |
|---|---------------------------------------------------------------------------|----------------------------------------|
| 1 | [Flecs-Vulkan Bridge](practice/05_ecs_gameplay/01_flecs_vulkan_bridge.md) | Интеграция ECS с Vulkan                |
| 2 | [Coordinate Systems](practice/05_ecs_gameplay/02_coordinate_systems.md)   | Системы координат в ProjectV           |
| 3 | [Camera Controller](practice/05_ecs_gameplay/03_camera_controller.md)     | FPS-камера, frustum culling чанков     |
| 4 | [Input System](practice/05_ecs_gameplay/04_input_system.md)               | Стек слоёв ввода: ImGui, debug-консоль |
| 5 | [Input Actions](practice/05_ecs_gameplay/05_input_actions.md)             | Action Mapping система                 |
| 6 | [Game UI](practice/05_ecs_gameplay/06_game_ui.md)                         | UI Strategy (ImGui/RmlUi)              |

### 📁 06_assets/ — Ресурсы и материалы

| # | Документ                                                            | Описание                                     |
|---|---------------------------------------------------------------------|----------------------------------------------|
| 1 | [Resource Management](practice/06_assets/01_resource_management.md) | Централизованное управление ресурсами Vulkan |
| 2 | [VOX Import](practice/06_assets/02_vox_import.md)                   | Импорт .VOX файлов (MagicaVoxel)             |
| 3 | [Serialization](practice/06_assets/03_serialization.md)             | Сохранение воксельного мира: Zstd, форматы   |
| 4 | [Reflection](practice/06_assets/04_reflection.md)                   | Система рефлексии для инструментов           |
| 5 | [Hot-Reload](practice/06_assets/05_hot_reload.md)                   | Быстрая итерация без перезапуска             |
| 6 | [Asset Management](practice/06_assets/06_asset_management.md)       | Управление ассетами                          |
| 7 | [Material System](practice/06_assets/07_material_system.md)         | PBR материалы для вокселей                   |
| 8 | [Asset Pipeline](practice/06_assets/08_asset_pipeline.md)           | Offline Compiler для ассетов                 |

### 📋 07_meta/ — Meta-документация

| # | Документ                                                            | Описание                   |
|---|---------------------------------------------------------------------|----------------------------|
| 1 | [Team Workflow](practice/07_meta/01_team_workflow.md)               | Организация работы команды |
| 2 | [Implementation Order](practice/07_meta/02_implementation_order.md) | Порядок имплементации      |
| 3 | [Testing Architecture](practice/07_meta/03_testing_architecture.md) | Архитектура тестирования   |

---

## Рекомендуемый путь изучения

### Для новых разработчиков

```
ADR (0001-0004) → theory/01_ecs-concepts.md → theory/02_memory-layout.md
              ↓
practice/01_core/01_engine_structure.md → practice/01_core/02_core_loop.md
              ↓
practice/03_voxel/01_svo_architecture.md → practice/05_ecs_gameplay/02_coordinate_systems.md
```

### Для опытных разработчиков

```
ADR (0001-0004) → practice/02_render/01_vulkan_spec.md
              ↓
practice/05_ecs_gameplay/01_flecs_vulkan_bridge.md → practice/04_physics_ca/01_jolt_vulkan_bridge.md
              ↓
practice/03_voxel/02_voxel_pipeline.md → practice/04_physics_ca/03_gpu_cellular_automata.md
```

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
