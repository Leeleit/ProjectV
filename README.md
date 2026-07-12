# ProjectV

[![Build & Test](https://github.com/Leeleit/ProjectV/actions/workflows/build.yml/badge.svg)](https://github.com/Leeleit/ProjectV/actions/workflows/build.yml)

**C++26 · Vulkan 1.4 · RTX-only voxel sandbox**

ProjectV — высокопроизводительная воксельная песочница, вдохновлённая War Thunder, Foxhole, HoI4, Warno, Supreme
Commander, Minecraft и Garry's Mod. Это **не** Minecraft-клон и не чистый военный симулятор: движок сочетает воксельный
мир с элементами симуляции, где игрок может изменять мир, строить, сражаться, программировать логику, создавать моды и
сценарии.

> **Hardware target:** NVIDIA RTX 20/30/40/50 series (Turing RT cores или новее). Non-RTX GPU не стартуют — legacy
> fallback не предоставляется.

---

## Статус

Проект находится в стадии активного MVP-прототипа. Mainline уже включает:

- Разреженное SVO-хранилище мира (`Sparse64Tree`) с GPU-aligned NanoVDB buffer.
- GPU greedy meshing и HZB occlusion culling чанков.
- Аппаратные RTX-тени солнца через BLAS/TLAS + voxel-aware procedural intersection.
- DDGI probes для динамического диффузного глобального освещения.
- RTX рефракцию и multi-bounce specular GI для воды/стекла/зеркал.
- Jolt Physics интеграцию с `CharacterVirtual` и режимами Creative / Spectator / Walk.
- Минимальный ECS bridge на `flecs`.
- Async compute + timeline semaphores для Fluid CA, World Gen и HZB.

---

## Быстрый старт

Основной dev-контур — **Linux + Clang 22 + Ninja**.

```bash
# 1. Зависимости и сабмодули
git submodule update --init --recursive

# 2. Конфигурация
cmake --preset linux-clang-debug

# 3. Сборка
cmake --build --preset linux-clang-debug-build

# 4. Тесты
ctest --preset linux-clang-debug-tests

# 5. Запуск
./build/linux-clang-debug/bin/ProjectV
```

Подробнее:

- [Linux Build & Run Guide](docs/Linux_Build_And_Run.md) — основное руководство по сборке и тестам.
- [Build & Run (Windows)](docs/BuildAndRun.md) — если вы работаете на Windows.

---

## Что внутри

| Подсистема                   | Технологии                                            | Подробнее                                                                               |
|:-----------------------------|:------------------------------------------------------|:----------------------------------------------------------------------------------------|
| **Воксельный мир**           | SVO, NanoVDB, CPU raycast, runtime edits              | [VoxelWorld (Historical)](docs/VoxelWorld.md), [Codebase Guide](docs/CODEBASE_GUIDE.md) |
| **Рендеринг**                | Vulkan 1.4, dynamic rendering, RTX KHR, DDGI          | [RTX Renderer Architecture](docs/RTX_Renderer_Architecture.md)                          |
| **Физика и движение**        | Jolt Physics, greedy physics merging, walk controller | [Physics & Movement Guide](docs/Physics_And_Movement_Guide.md)                          |
| **Архитектура**              | C++26 modules, DOD, ECS bridge, SoA                   | [Architecture Guide](docs/ArchitectureGuide.md)                                         |
| **Отладка и профилирование** | Tracy, in-app HUD, smoke scripts                      | [Debugging](docs/Debugging.md), [Profiling](docs/Profiling.md)                          |

---

## Документация

Вся документация живёт в [`docs/`](docs/). Точка входа — [`docs/README.md`](docs/README.md).

Для нового разработчика рекомендуется начать с:

1. [`docs/ArchitectureGuide.md`](docs/ArchitectureGuide.md) — общая архитектура.
2. [`docs/CODEBASE_GUIDE.md`](docs/CODEBASE_GUIDE.md) — полный разбор файлов и алгоритмов.
3. [`docs/Linux_Build_And_Run.md`](docs/Linux_Build_And_Run.md) — сборка и запуск.

Инженерные контракты, runtime facts и текущий статус проекта:

- [`AGENTS.md`](AGENTS.md) — протокол работы агентов.
- [`agent/knowledge.md`](agent/knowledge.md) — долговечные инженерные факты.
- [`agent/workspace.md`](agent/workspace.md) — снимок текущего состояния.
- [`TODO.md`](TODO.md) — roadmap, приоритеты и риски.

---

## Принципы

- **Код первичен:** при расхождении приоритет всегда у исходников, тестов и шейдеров.
- **Data-Oriented Design:** SoA по умолчанию, явные границы подсистем, компактные файлы (≤600 строк).
- **RTX-only:** никаких non-RTX fallback. Dedicated RT cores — baseline.
- **Сохранение контекста важнее скорости:** `AGENTS.md`, `agent/knowledge.md`, `agent/workspace.md` и `TODO.md`
  поддерживают единое состояние проекта.

---

## Roadmap

Актуальные приоритеты и milestones — в [`TODO.md`](TODO.md).

---

## License

TBD.
