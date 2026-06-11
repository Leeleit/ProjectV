# agent/active-sessions.md

Append-only ledger активных и недавно завершённых AI-agent сессий в `ProjectV`.
Используется для координации между параллельными сессиями и для arbitration
при конфликте scope (см. `AGENTS.md` §7.2.6).

**Это НЕ источник истины** для архитектурных решений — для этого `agent/decisions.md`.
Здесь только оперативный signal «кто сейчас что трогает», чтобы параллельные
агенты не вытирали работу друг друга.

---

## Контракт использования

Каждый агент **обязан**:

1. **При старте сессии** — дописать запись со статусом `open` в секцию
   «Активные сессии» ниже.
2. **При завершении сессии** — обновить **свою** запись: статус → `closed`,
   проставить `closed-at` и `commit-hash` (или `uncommitted` / `aborted`
   с пояснением), затем перенести в секцию «Закрытые сессии».
3. **При abort** — пометить `aborted` + причина, не удалять запись.

См. также `agent/session-checklist.md` (секции «Старт» / «Завершение»).
Параллельный запуск нескольких сессий с **пересекающимся** scope —
аномалия, требует arbitration через пользователя (§7.2.6).

---

## Формат записи

| Поле | Описание |
|---|---|
| `id` | Уникальный идентификатор сессии (timestamp ISO 8601 + короткий суффикс) |
| `started-at` | Время старта в ISO 8601 (UTC) |
| `agent` | Тип / модель агента (например, `cline/MiniMax-M3`) |
| `operator` | Пользователь-оператор (например, `le1t`) |
| `branch` | Текущая git-ветка |
| `scope` | Краткое описание атомарной подзадачи (см. AGENTS.md §3.5) |
| `files-touched-intent` | Список файлов / путей, которые планируется править |
| `status` | `open` / `closed` / `aborted` |
| `closed-at` | (только для `closed`/`aborted`) Время завершения в ISO 8601 (UTC) |
| `commit-hash` | (только для `closed`) SHA коммита, закрывшего работу; или `uncommitted` |
| `notes` | Свободное примечание (конфликты, blockers, cross-refs) |

**Append-only правила:**

- Новые записи добавлять **сверху** соответствующей секции.
- Не редактировать чужие записи retroactively (даже если они «устарели») —
  лучше создать новую запись с `supersedes: <id>`.
- Не удалять закрытые записи из этого файла — при необходимости
  переносить в `legacy/docs/archive/agent-sessions/`.

---

## Активные сессии (status: open)

<!-- Новые записи добавлять СВЕРХУ этой секции. Append-only. -->

### session-2026-06-11-asset-pipeline-m0-m5

- **id:** `2026-06-11T19:55Z-asset-pipeline-m0-m5`
- **started-at:** 2026-06-11T19:55:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** M0–M5: импорт полигональных моделей через `fastgltf` + `draco` + `meshoptimizer`. M0 = CMake wiring + smoke build. M1 = sync `AssetLoader` (`.glb` → `fastgltf::Asset`) + `AssetRegistry` + env-var manifest `PROJECTV_MODELS=path.glb@x,y,z;...`. M2 = `MeshBaker` + `MeshGpuResources` (interleaved vertex, meshopt vcache+vfetch, VMA upload). M3 = draco path (`KHR_draco_mesh_compression`). M4 = model graphics pass + `model.vert/frag` + shared GLSL helper для `SceneLightingBuffer` (Q6=U3=b) + `MeshComponent`/`ModelTransformComponent` ECS + Jolt static AABB body. M5 = multi-instance + frustum cull.
- **files-touched-intent:** `CMakeLists.txt`, `src/CMakeLists.txt`, `src/asset/AssetLoader.{hpp,cpp}` (M1+), `src/asset/MeshBaker.{hpp,cpp}` (M2+), `src/asset/MeshGpuResources.{hpp,cpp}` (M2+), `src/asset/DracoMeshDecoder.{hpp,cpp}` (M3+), `src/asset/ModelPass.{hpp,cpp}` (M4+), `src/asset/ModelComponent.hpp` (M4+), `src/asset/AssetRegistry.{hpp,cpp}` (M1+), `src/asset/AssetStub.cpp` (M0), `src/render/Renderer.cpp` (M4 — `RecordModelCommands` between opaque and transparent), `src/render/SceneResources.cpp` (M4 — `ModelRenderState` slot), `src/core/Types.hpp` (M4 — `ModelRenderState` field), `src/app/FramePreparation.cpp` (M4+ — build model draw list), `src/ecs/EcsWorld.cpp` (M4+ — register components), `src/app/AppUpdate.cpp` (M1+ — manifest load), `src/shaders/model.vert`, `src/shaders/model.frag`, `src/shaders/lighting.glsl` (M4 — shared GGX helper + `SceneLightingBuffer` GLSL declaration, U3=b), `tests/AssetLoaderTests.cpp` (M1+), `tests/fixtures/box.glb` (M1). **Не трогаю:** `src/render/vulkan/VulkanBootstrap.cpp`, `src/render/vulkan/VulkanGraphicsPipeline.cpp`, `src/render/vulkan/TaaResolvePipeline.cpp`, `src/render/Taa.*`, `src/render/TaaRenderTargets.cpp` (TAA-сессия scope, см. ниже).
- **status:** open
- **notes:** Решения зафиксированы в диалоге `2026-06-11`: Q1=2, Q2=1 (→ план 3), Q3=1 (→ план 3), Q4=2, Q5=2 (receive-only, выровнено с RTX-будущим), Q6=1 (universal PBR SSBO, GGX reuse), Q7=1 (без save), Q8=2 (можно трогать `SceneLightingBuffer`), U1=c, U2=c, U3=b, U4=b. Параллельная сессия — `2026-06-11T16:43Z-taa-renderer-wiring` (см. ниже) — работает над TAA offscreen main pass + resolve pass; непересекающийся scope. **Прогресс:** M0 = `1c72a4b` (CMake wiring + stub TU) — closed. M1 = sync `.glb` parser + `AssetRegistry` + manifest + tests — closed-pending-commit. TAA-сессия закоммитила `98fb391` между M0 и M1.

### session-2026-06-11-taa-renderer-wiring

- **id:** `2026-06-11T16:43Z-taa-renderer-wiring`
- **started-at:** 2026-06-11T16:43:00Z
- **closed-at:** 2026-06-11T22:14:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Subtask 1 (format mismatch fix через `VK_EXT_dynamic_rendering_unused_attachments`) + Subtask 2 (Renderer.cpp wiring TAA offscreen main pass + resolve pass + history copy). `taaEnabled` остаётся `false` (visual TAA — отдельная сессия).
- **files-touched-intent:** `src/render/vulkan/VulkanBootstrap.cpp` (extension enable + pNext chain), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (dual `pColorAttachmentFormats` + dual `pAttachments`), `src/render/Renderer.cpp` (restructured `RecordGraphicsCommands` + `InvertColumnMajorMat4` helper), `src/core/Types.hpp` (per-image layout trackers, `supportsDynamicRenderingUnusedAttachments` on `VulkanContextState`), `src/render/vulkan/VulkanSwapchain.cpp` (layout tracker reset), `agent/memory.md` (новый §10.14), `agent/status.md` (snapshot update).
- **status:** closed
- **commit-hash:** `98fb391` — `refactor(render): wire TAA offscreen main pass + resolve pass + history copy`
- **notes:** Subtask 1+2 work landed at `98fb391`: build green, ctest 1/1, smoke 6/6 with `PROJECTV_ENABLE_VALIDATION=ON` — 0 VUIDs / 0 Unfreed / 0 taaResolvePipeline errors. Source-of-truth для плана — `agent/memory.md` §10.12, §10.13. **Координация с параллельной сессией `2026-06-11-asset-pipeline-m0-m5`:** asset-pipeline M0 (CMake wiring) закоммичен оператором как `1c72a4b build(asset): wire fastgltf, draco and meshoptimizer into ProjectV`. Asset-pipeline сейчас на M1 (`AssetLoader` + `AssetRegistry` + manifest), untracked в дереве. Когда asset-pipeline начнёт M4 (`RecordModelCommands` + `ModelRenderState`), будет merge conflict в `Renderer.cpp` / `core/Types.hpp` / `SceneResources.cpp` — он rebase'нется поверх `98fb391` через `git pull --rebase` или manual merge per `AGENTS.md §7.2.6`. Решение: commit сейчас выбрано оператором.

---

## Закрытые сессии (status: closed)

<!-- Недавние закрытые сессии (последние ~10). Старые можно переносить
     в `legacy/docs/archive/agent-sessions/` для сохранения истории. -->

### session-2026-06-11-multi-agent-policy

- **id:** `2026-06-11T16:30Z-multi-agent-policy`
- **started-at:** 2026-06-11T16:25:00Z
- **closed-at:** 2026-06-11T16:35:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Добавить §7.2.6 «Multi-agent concurrent work policy» в `AGENTS.md` + создать `agent/active-sessions.md` как append-only ledger координации.
- **files-touched-intent:** `AGENTS.md`, `agent/active-sessions.md` (new)
- **status:** closed
- **commit-hash:** _pending_ (заполняется после коммита пользователем)
- **notes:** Источник — явная команда пользователя «над проектом могут работать несколько агентов, изменения могут быть прерваны, агенты должны быть готовы». Протокол multi-agent зафиксирован; см. `AGENTS.md` §7.2.6.
