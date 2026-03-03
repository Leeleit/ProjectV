# Порядок Имплементации Модулей [🟢 Уровень 1]

**Статус:** Technical Specification
**Уровень:** 🟢 Фундаментальный
**Дата:** 2026-02-23
**Версия:** 1.0

---

## Обзор

Документ определяет **порядок имплементации модулей** ProjectV с учётом:

- Зависимостей между модулями
- Приоритетов (P0 → P1 → P2)
- MVP Mode (минимальный набор для первого запуска)
- Рисков и блокеров

---

## 1. Диаграмма Зависимостей

```
┌─────────────────────────────────────────────────────────────────────────────────────┐
│                         ProjectV Module Dependency Graph                            │
├─────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                      │
│  ┌───────────────────────────────────────────────────────────────────────────────┐  │
│  │                            Layer 0: Foundation                                 │  │
│  │                                                                                 │  │
│  │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │  │
│  │   │ Core.Types  │──▶│ Core.Memory │──▶│ Core.Jobs   │──▶│ Core.ECS    │       │  │
│  │   │    (P0)     │   │    (P0)     │   │    (P0)     │   │    (P0)     │       │  │
│  │   └─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘       │  │
│  │          │                │                 │                 │                │  │
│  └──────────┼────────────────┼─────────────────┼─────────────────┼────────────────┘  │
│             │                │                 │                 │                   │
│             ▼                ▼                 ▼                 ▼                   │
│  ┌───────────────────────────────────────────────────────────────────────────────┐  │
│  │                            Layer 1: Engine Core                                │  │
│  │                                                                                 │  │
│  │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │  │
│  │   │Render.Vulkan│──▶│Render.Window│──▶│ Render.GUI  │──▶│ Render.Mesh │       │  │
│  │   │    (P0)     │   │    (P0)     │   │    (P1)     │   │    (P1)     │       │  │
│  │   └─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘       │  │
│  │          │                                                                   │  │
│  └──────────┼────────────────────────────────────────────────────────────────────┘  │
│             │                                                                        │
│             ▼                                                                        │
│  ┌───────────────────────────────────────────────────────────────────────────────┐  │
│  │                            Layer 2: Game Systems                               │  │
│  │                                                                                 │  │
│  │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │  │
│  │   │ Voxel.Data  │──▶│ Voxel.Render│──▶│ Voxel.CA    │──▶│ Voxel.World │       │  │
│  │   │    (P0)     │   │    (P0)     │   │    (P1)     │   │    (P1)     │       │  │
│  │   └─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘       │  │
│  │          │                                                                   │  │
│  │          │          ┌─────────────┐   ┌─────────────┐                         │  │
│  │          └─────────▶│Physics.Jolt │──▶│Physics.Bridge│                        │  │
│  │                     │    (P1)     │   │    (P1)     │                         │  │
│  │                     └─────────────┘   └─────────────┘                         │  │
│  └───────────────────────────────────────────────────────────────────────────────┘  │
│                                                                                      │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Порядок Имплементации

### 2.1 Phase 0: Foundation (Week 1-2)

| #   | Модуль        | Приоритет | Зависимости | Описание                          | Риск    |
|-----|---------------|-----------|-------------|-----------------------------------|---------|
| 0.1 | `Core.Types`  | P0        | -           | Базовые типы, math, constants     | Низкий  |
| 0.2 | `Core.Memory` | P0        | Core.Types  | Allocators, pools, RAII wrappers  | Средний |
| 0.3 | `Core.Jobs`   | P0        | Core.Memory | Thread pool, task graph (stdexec) | Высокий |
| 0.4 | `Core.ECS`    | P0        | Core.Jobs   | Flecs integration, base systems   | Средний |

**Критерии завершения Phase 0:**

- [ ] Unit tests проходят для всех Core модулей
- [ ] Job system работает с простыми задачами
- [ ] Flecs world инициализируется без ошибок

### 2.2 Phase 1: Rendering Foundation (Week 3-4)

| #   | Модуль               | Приоритет | Зависимости   | Описание                            | Риск    |
|-----|----------------------|-----------|---------------|-------------------------------------|---------|
| 1.1 | `Render.Vulkan`      | P0        | Core.*        | Instance, device, queues, swapchain | Высокий |
| 1.2 | `Render.Window`      | P0        | Render.Vulkan | SDL3 window, input handling         | Низкий  |
| 1.3 | `Render.Pipeline`    | P0        | Render.Vulkan | Graphics pipelines, shaders         | Высокий |
| 1.4 | `Render.Descriptors` | P0        | Render.Vulkan | Bindless descriptors, sets          | Средний |

**Критерии завершения Phase 1:**

- [ ] Vulkan triangle rendering работает
- [ ] Swapchain resize обрабатывается корректно
- [ ] Basic descriptor sets работают

### 2.3 Phase 2: Voxel Core (Week 5-6)

| #   | Модуль         | Приоритет | Зависимости           | Описание                        | Риск    |
|-----|----------------|-----------|-----------------------|---------------------------------|---------|
| 2.1 | `Voxel.Data`   | P0        | Core.*, Render.Vulkan | Chunk storage, block types      | Низкий  |
| 2.2 | `Voxel.Mesh`   | P0        | Voxel.Data            | Greedy meshing, mesh generation | Средний |
| 2.3 | `Voxel.Render` | P0        | Voxel.Mesh, Render.*  | Chunk rendering, LOD            | Высокий |
| 2.4 | `Voxel.World`  | P0        | Voxel.*               | World management, chunk loading | Средний |

**Критерии завершения Phase 2:**

- [ ] Single chunk отображается на экране
- [ ] Базовые block types рендерятся
- [ ] Camera movement работает

### 2.4 Phase 3: MVP Integration (Week 7-8)

| #   | Модуль               | Приоритет | Зависимости    | Описание                        | Риск    |
|-----|----------------------|-----------|----------------|---------------------------------|---------|
| 3.1 | `Game.Main`          | P0        | All P0 modules | Main loop, initialization       | Средний |
| 3.2 | `Game.Input`         | P0        | Render.Window  | Input handling, camera controls | Низкий  |
| 3.3 | `Game.Settings`      | P0        | Core.*         | Configuration, settings         | Низкий  |
| 3.4 | `Game.Serialization` | P0        | Voxel.Data     | Chunk saving/loading            | Средний |

**MVP Criteria:**

- [ ] Приложение запускается и отображает voxel мир
- [ ] Camera movement (WASD + mouse)
- [ ] Chunk generation при движении
- [ ] Graceful shutdown без утечек памяти

### 2.5 Phase 4: Extended Features (Week 9-12)

| #   | Модуль             | Приоритет | Зависимости           | Описание                     | Риск    |
|-----|--------------------|-----------|-----------------------|------------------------------|---------|
| 4.1 | `Physics.Jolt`     | P1        | Core.*                | Jolt Physics integration     | Средний |
| 4.2 | `Physics.Bridge`   | P1        | Physics.Jolt, Voxel.* | Physics-voxel integration    | Высокий |
| 4.3 | `Voxel.CA`         | P1        | Voxel.*               | Cellular Automata simulation | Средний |
| 4.4 | `Render.GUI`       | P1        | Render.Vulkan         | RmlUI/ImGui integration      | Низкий  |
| 4.5 | `Asset.Loader`     | P1        | Render.*              | glTF/Draco model loading     | Средний |
| 4.6 | `Render.Particles` | P2        | Render.*              | GPU particle systems         | Средний |

---

## 3. MVP Mode Definition

### 3.1 Что входит в MVP

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          MVP Feature Set                                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ✅ Включено:                                                            │
│  ├── Vulkan initialization (instance, device, swapchain)                │
│  ├── Basic rendering pipeline (vertex + fragment shaders)               │
│  ├── Single chunk rendering (32³ blocks)                                │
│  ├── Block types: Air, Stone, Dirt, Grass                               │
│  ├── Camera: FPS-style controls (WASD + mouse)                          │
│  ├── Chunk generation: Simple noise-based                               │
│  ├── Window: SDL3 with resize handling                                  │
│  └── Job System: Basic thread pool                                      │
│                                                                          │
│  ❌ НЕ включено:                                                         │
│  ├── Physics (Jolt)                                                     │
│  ├── Cellular Automata                                                  │
│  ├── GUI/Debug tools                                                    │
│  ├── Asset loading (models, textures)                                   │
│  ├── Multiplayer                                                        │
│  ├── Audio                                                              │
│  └── Advanced rendering (shadows, reflections)                          │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### 3.2 CMake MVP Configuration

```cmake
# cmake/MVPMode.cmake

# MVP Mode - минимальный набор для первого запуска
option(PROJECTV_MVP_MODE "Build only MVP features" OFF)

if(PROJECTV_MVP_MODE)
    message(STATUS "Building in MVP Mode - minimal feature set")

    # Disable non-essential features
    set(PROJECTV_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_CA OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_GUI OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_ASSETS OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_PARTICLES OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_AUDIO OFF CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_NETWORK OFF CACHE BOOL "" FORCE)

    # Enable only core features
    set(PROJECTV_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_VOXEL ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_JOBS ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_ECS ON CACHE BOOL "" FORCE)

    # Minimal dependencies
    set(PROJECTV_USE_TRACY OFF CACHE BOOL "" FORCE)
    set(PROJECTV_USE_JOLT OFF CACHE BOOL "" FORCE)
    set(PROJECTV_USE_RMLUI OFF CACHE BOOL "" FORCE)
    set(PROJECTV_USE_DRACO OFF CACHE BOOL "" FORCE)

    # Build only essential modules
    set(PROJECTV_MODULES
        # Layer 0: Foundation
        Core.Types
        Core.Memory
        Core.Jobs
        Core.ECS

        # Layer 1: Rendering
        Render.Vulkan
        Render.Window
        Render.Pipeline
        Render.Descriptors

        # Layer 2: Voxel
        Voxel.Data
        Voxel.Mesh
        Voxel.Render
        Voxel.World

        # Layer 3: Game
        Game.Main
        Game.Input
        Game.Settings
        Game.Serialization
    CACHE STRING "Modules to build" FORCE)

else()
    # Full build - all features
    message(STATUS "Building Full Mode - all features")

    set(PROJECTV_ENABLE_PHYSICS ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_CA ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_GUI ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_ASSETS ON CACHE BOOL "" FORCE)
    set(PROJECTV_ENABLE_TRACY ON CACHE BOOL "" FORCE)
    set(PROJECTV_USE_JOLT ON CACHE BOOL "" FORCE)
endif()
```

### 3.3 MVP Build Commands

```bash
# MVP build (fast, minimal)
cmake -B build -DPROJECTV_MVP_MODE=ON
cmake --build build --config Release

# Full build (slow, all features)
cmake -B build -DPROJECTV_MVP_MODE=OFF
cmake --build build --config Release

# Debug build with validation layers
cmake -B build -DPROJECTV_MVP_MODE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

## 4. Риски и Митигация

### 4.1 Технические Риски

| Риск                       | Вероятность | Влияние | Митигация                                |
|----------------------------|-------------|---------|------------------------------------------|
| stdexec compiler support   | Высокая     | Высокое | Fallback на thread_pool + std::async     |
| Vulkan validation errors   | Средняя     | Среднее | Enable layers early, address immediately |
| Module compilation issues  | Средняя     | Высокое | Use .cppm + .cpp split from start        |
| Memory leaks in job system | Средняя     | Высокое | Tracy integration, address sanitiser     |
| Slang shader compilation   | Низкая      | Среднее | Fallback на GLSL для MVP                 |

### 4.2 Блокеры

```
┌─────────────────────────────────────────────────────────────────────────┐
│                          Current Blockers                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  🔴 BLOCKER: Clang 19 module support incomplete                         │
│     Impact: Cannot use full C++26 modules                                │
│     Workaround: Use .cppm interface + .cpp implementation               │
│     ETA: Clang 20 (expected Q2 2026)                                    │
│                                                                          │
│  🟡 WARNING: stdexec API stability                                       │
│     Impact: API changes between versions                                │
│     Workaround: Pin to specific commit, abstract via PIMPL              │
│                                                                          │
│  🟢 INFO: P2996 reflection partial support                               │
│     Impact: Cannot use template for in production                       │
│     Workaround: Use Boost.PFR fallback                                  │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Тестирование

### 5.1 Test Pyramid

```
                    ┌───────────────────┐
                    │   E2E Tests       │  ← Integration tests (slow)
                    │   (Manual/QA)     │     Full game scenarios
                    └───────────────────┘
                           │
              ┌────────────────────────────┐
              │    Integration Tests       │  ← Module integration
              │    (Automated)             │     Vulkan rendering, physics
              └────────────────────────────┘
                           │
         ┌─────────────────────────────────────────┐
         │           Unit Tests                     │  ← Fast, isolated
         │           (Automated)                    │     Core types, memory, jobs
         └─────────────────────────────────────────┘
```

### 5.2 Test Requirements per Phase

| Phase    | Unit Tests | Integration Tests | Coverage Target |
|----------|------------|-------------------|-----------------|
| Phase 0  | ✅ Required | ⚪ Optional        | 80%             |
| Phase 1  | ✅ Required | ✅ Required        | 70%             |
| Phase 2  | ✅ Required | ✅ Required        | 60%             |
| Phase 3  | ✅ Required | ✅ Required        | 50%             |
| Phase 4+ | ✅ Required | ✅ Required        | 40%             |

---

## 6. Milestones

### 6.1 Milestone Timeline

```
Week 1-2:   ┌────────────────┐
            │  Phase 0       │  Foundation
            │  Core.*        │  Job System, ECS
            └────────────────┘
                     │
Week 3-4:            ▼
            ┌────────────────┐
            │  Phase 1       │  Rendering Foundation
            │  Render.*      │  Vulkan, Window
            └────────────────┘
                     │
Week 5-6:            ▼
            ┌────────────────┐
            │  Phase 2       │  Voxel Core
            │  Voxel.*       │  Chunks, Meshing
            └────────────────┘
                     │
Week 7-8:            ▼
            ┌────────────────┐
            │  Phase 3       │  🎯 MVP READY
            │  Game.*        │  Playable Demo
            └────────────────┘
                     │
Week 9-12:           ▼
            ┌────────────────┐
            │  Phase 4       │  Extended Features
            │  Physics, CA   │  Full Game
            └────────────────┘
```

### 6.2 Success Criteria

| Milestone      | Criteria                                 | Demo                       |
|----------------|------------------------------------------|----------------------------|
| M0: Foundation | Job system runs 1000 tasks without crash | Parallel prime calculation |
| M1: Rendering  | Vulkan triangle on screen                | Spinning triangle          |
| M2: Voxel      | Single chunk renders correctly           | Static voxel terrain       |
| **M3: MVP**    | **Camera + terrain + save/load**         | **Playable voxel demo**    |
| M4: Physics    | Player physics + collisions              | Physics playground         |
| M5: CA         | Cellular Automata runs                   | Falling sand demo          |

---

## 7. Checklist для каждого модуля

При имплементации каждого модуля проверять:

```markdown
## Module: [ModuleName]

### Implementation
- [ ] Interface (.cppm) written
- [ ] Implementation (.cpp) written
- [ ] All dependencies resolved
- [ ] No circular dependencies
- [ ] PIMPL pattern used where needed

### Testing
- [ ] Unit tests pass
- [ ] Integration tests pass (if applicable)
- [ ] No memory leaks (ASan/Valgrind)
- [ ] No thread races (TSan)

### Documentation
- [ ] Invariants documented
- [ ] Thread safety documented
- [ ] Complexity documented
- [ ] Examples provided

### Code Quality
- [ ] clang-tidy passes
- [ ] clang-format applied
- [ ] No warnings (Wall -Wextra -Werror)
- [ ] Constants validated at compile-time

### Integration
- [ ] CMakeLists.txt updated
- [ ] Module added to PROJECTV_MODULES
- [ ] CI pipeline passes
```

---

## Статус

| Фаза                | Статус        | Начало | Конец |
|---------------------|---------------|--------|-------|
| Phase 0: Foundation | ⚪ Not Started | -      | -     |
| Phase 1: Rendering  | ⚪ Not Started | -      | -     |
| Phase 2: Voxel Core | ⚪ Not Started | -      | -     |
| Phase 3: MVP        | ⚪ Not Started | -      | -     |
| Phase 4: Extended   | ⚪ Not Started | -      | -     |

---

## Ссылки

- [00_specification.md](./00_specification.md)
- [01_basics-structure.md](./01_basics-structure.md)
- [31_job_system_p2300_spec.md](./31_job_system_p2300_spec.md)
- [33_zero_copy_memory_sharing.md](./33_zero_copy_memory_sharing.md)
