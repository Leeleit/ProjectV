# ProjectV — Документация

Это центральная точка входа в документацию `ProjectV`. Здесь собрана карта всех руководств, их статус и рекомендуемый
порядок изучения.

> **Иерархия источников правды** (подробнее в `AGENTS.md` §4 и `agent/knowledge.md` §0):
> 1. Код (`.cpp`, `.hpp`, `.ixx`, шейдеры, тесты) — абсолютный приоритет.
> 2. `AGENTS.md` — протокол работы агента.
> 3. `agent/knowledge.md` — действующие engineering contracts.
> 4. `agent/workspace.md` — снимок текущего состояния.
> 5. `docs/VulkanSDK-Linux-Docs-1.4.350.1/` — вендорная документация Vulkan 1.4.
> 6. `TODO.md` — roadmap, приоритеты и риски.
>
> Файлы в `docs/` описывают архитектуру и процессы, но при расхождении приоритет всегда у кода.

---

## Быстрый старт

| Если вы хотите…                                | Начните с                                                 |
|:-----------------------------------------------|:----------------------------------------------------------|
| Собрать и запустить проект в Linux             | [Linux Build & Run Guide](Linux_Build_And_Run.md)         |
| Собрать и запустить проект в Windows           | [Build & Run (Windows)](BuildAndRun.md)                   |
| Понять общую архитектуру и путь кадра          | [Architecture Guide](ArchitectureGuide.md)                |
| Разобраться в файлах и модулях кодовой базы    | [Codebase Guide](CODEBASE_GUIDE.md)                       |
| Понять RTX-рендеринг, тени и DDGI              | [RTX Renderer Architecture](RTX_Renderer_Architecture.md) |
| Понять физику, перемещение и greedy merging    | [Physics & Movement Guide](Physics_And_Movement_Guide.md) |
| Найти горячие клавиши и отладочные инструменты | [Debugging Guide](Debugging.md)                           |
| Провести perf-замеры и использовать Tracy      | [Profiling Guide](Profiling.md)                           |
| Снять GPU/replay трассировку (Nsight, InputReplay) | [Tracing Guide](Tracing.md)                           |
| Понять раскладку `src/` и include boundaries   | [Source Layout Guide](source_layout.md)                   |

---

## Карта документов

### Актуальные руководства

| Документ                                                       | Тема                      | Краткое описание                                                                                          |
|:---------------------------------------------------------------|:--------------------------|:----------------------------------------------------------------------------------------------------------|
| [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md)                         | `[Current][Architecture]` | Исчерпывающий путеводитель по кодовой базе: все модули `src/`, диаграммы кадра, разбор алгоритмов.        |
| [ArchitectureGuide.md](ArchitectureGuide.md)                   | `[Current][Architecture]` | Общая архитектура движка, подсистемы, ownership и высокоуровневый путь кадра.                             |
| [RTX_Renderer_Architecture.md](RTX_Renderer_Architecture.md)   | `[Current][Rendering]`    | Современная RTX-only архитектура: BLAS/TLAS, тени солнца, DDGI probes, рефракция, декомпозиция рендерера. |
| [Linux_Build_And_Run.md](Linux_Build_And_Run.md)               | `[Current][Build]`        | Основное руководство по сборке и тестам в Linux (primary dev-контур).                                     |
| [BuildAndRun.md](BuildAndRun.md)                               | `[Current][Build]`        | Сборка и запуск в Windows (clang-cl + MSVC).                                                              |
| [Physics_And_Movement_Guide.md](Physics_And_Movement_Guide.md) | `[Current][Physics]`      | Интеграция Jolt Physics, режимы Creative/Spectator/Walk, auto-jump, sneak, greedy physics merging.        |
| [source_layout.md](source_layout.md)                           | `[Current][Architecture]` | Физическая раскладка `src/`, include boundaries и ключевые файлы по подсистемам.                          |
| [voxel_mvp_smoke_checklist.md](voxel_mvp_smoke_checklist.md)   | `[Current][Debugging]`    | Ручной и автоматический smoke checklist для Windows runtime.                                              |

### Исторические и устаревшие документы

Эти файлы оставлены как historical reference. Технические детали в них могут описывать удалённые подсистемы (CSM, TAA,
плоское хранение вокселей). Актуальную информацию ищите в документах выше.

| Документ                                       | Тема                      | Актуальная замена                                                                                 |
|:-----------------------------------------------|:--------------------------|:--------------------------------------------------------------------------------------------------|
| [RenderArchitecture.md](RenderArchitecture.md) | `[Historical][Rendering]` | [RTX Renderer Architecture](RTX_Renderer_Architecture.md), [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md) |
| [VoxelWorld.md](VoxelWorld.md)                 | `[Historical][Voxel]`     | [CODEBASE_GUIDE.md](CODEBASE_GUIDE.md), [Physics & Movement Guide](Physics_And_Movement_Guide.md) |
| [QA_LOG_roadmap_v2.md](QA_LOG_roadmap_v2.md)   | `[Historical][Roadmap]`   | `TODO.md`, `ROADMAP.md` (если есть), `agent/knowledge.md` §38-43                                  |

### В разработке / подлежащие актуализации

| Документ                     | Тема                   | Примечание                                                                                           |
|:-----------------------------|:-----------------------|:-----------------------------------------------------------------------------------------------------|
| [Debugging.md](Debugging.md) | `[Current][Debugging]` | Основные инструменты актуальны, но отдельные разделы могут содержать упоминания удалённых подсистем. |
| [Profiling.md](Profiling.md) | `[Current][Profiling]` | Scene presets и Tracy plots актуальны; некоторые Windows-специфичные примеры превалируют.            |
| [Tracing.md](Tracing.md) | `[Current][Profiling]` | Windows InputReplay + Nsight CLI playbook (GpuTrace Qt caveats, AUTOPLAY, gpu_* timestamps). |

---

## Дополнительные материалы

- `docs/experiments/` — исследовательские заметки, прототипы и closed experiments.
- `docs/VulkanSDK-Linux-Docs-1.4.350.1/` — вендорная документация Vulkan 1.4 (читать до поиска в заголовках).
- `docs/philosophy/` — инженерные принципы проекта: DOD, ECS, Vulkan, оптимизация, математика и др.
- `legacy/docs/` — архив pre-reset документации и исторических решений.

---

## Как поддерживать эту документацию

- При добавлении нового `.md` в корень `docs/` обновляйте эту карту.
- Если документ устаревает, пометьте его как `[Historical]` и добавьте ссылку на актуальную замену.
- Не дублируйте технические контракты: долговечные факты живут в `agent/knowledge.md`, roadmap — в `TODO.md`.
