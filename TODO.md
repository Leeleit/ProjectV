# ГЕНЕРАЛЬНЫЙ ПЛАН РАЗРАБОТКИ PROJECTV (VOXEL MVP)

## Введение и текущее состояние (Status-Quo)

ProjectV — высокопроизводительный интерактивный воксельный MVP-слайс, ориентированный на качественный
рендеринг (look-dev) и физическое взаимодействие в реальном времени.

**Mainline completed** (snapshot `2026-06-22`, post-18x+):
- SVO на CPU через Sparse64Tree + статическое продвижение чанков
- Гибридное хранение: SVO → NanoVDB-aligned SSBO на GPU (grow-on-exceed)
- Greedy meshing (CPU + GPU `voxel_mesh.comp`)
- HZB occlusion culling (smart blend width + 2-phase fallback)
- Pattern C mesh shaders (feature-flagged `PROJECTV_MESH_SHADER_PIPELINE`)
- GPU Fluid CA + ECS tick routing
- Почаночный Incremental Jolt + Greedy physics meshing (35× reduction)
- GPU Noise & World Gen
- LOD downsampling (B_SurfacePreserve)
- Draw distance lift (chunk prebake + per-frame preload)
- Voxel Cone Tracing (3D clipmap, 6-cone diffuse + 1-cone specular, mip chain)
- TAA (Karis 2014 `R16G16` motion vectors, YCoCg, CAS sharpen)
- Async Compute Queue + Timeline Semaphores (cross-queue HZB + RTX BLAS routing)
- ECS migration (UpdateApp: 355 → 49 lines)
- Audio async scan (`std::jthread`)
- RTX shadows foundation (BLAS dispatch + AABB geometry + rayQueryEXT shaders) — TLAS population paused

**Стратегический фокус** (2026-06-22+): **non-RTX rendering polish → 5.2 RTX → deferred roadmap**.
RTX (5.2) поставлен на паузу до выжимания максимума из существующих non-RTX техник — VCT cone counts,
CSM quality, lighting tuning, post-processing. RTX path без работающего TLAS даёт визуальный no-op,
а baseline техники ещё не вышли на production quality.

**Активные TODO**: см. секцию ниже. История закрытых задач — `CHANGELOG.md` + `agent/workspace.md`.

---

## Active tasks (2026-06-22)

**Сводка:** 1 ⏸ Paused · 2 🔒 Deferred (pending feasibility criteria) · 5 ⭐ Next-priority (non-RTX polish)

### Next-priority (non-RTX polish — ДО возобновления 5.2 RTX)

Эти задачи имеют более высокий приоритет, чем 5.2 RTX. Логика: RTX path без TLAS даёт no-op
визуально, а существующий non-RTX pipeline ещё не выжат. Сначала VCT cone density + CSM quality +
lighting tuning → visual baseline улучшится заметно → THEN RTX как премиум-апгрейд.

#### ⭐ Задача 7.1. VCT cone density upgrade — Open

**Цель:** Поднять плотность VCT-трассировки до production quality (текущее: 6-cone diffuse + 1-cone specular).
Reference: WickedEngine 16-32 cone, Snowdrop 12-24 cone.

**Конкретно:**
- 6 → 12 cone diffuse (Octahedral parameterization или Fibonacci sphere; 2× quality)
- 1 → 4 cone specular (trade-off: 4× стоимость; gating per-material roughness band)
- Добавить второй specular cone ring для mipLevel-1..3 (текущий specular path использует только 1 cone)
- `kVctConeDirectionCount` constant в `voxel.frag` (сейчас 6, hard-coded)

**Ключевые файлы:** `src/shaders/voxel.frag` (`kVctConeDirectionCount`, cone generation),
`agent/knowledge.md §15` (VCT contract).

**DoD:**
- Reference scene `VoxelLab` lighting parity c профессиональным reference render
- `LightingDebugView` 9 → 11 (новые views: VCT cone count, cone direction visualization)
- Per-frame cost +20% на VoxelLab — допустимо, GPU-bound остаётся в бюджете

#### ⭐ Задача 7.2. CSM cascade quality pass — Open

**Цель:** Поднять качество каскадных теней (текущее: 4 каскада per `src/render/vulkan/VulkanGraphicsPipeline.cpp`).

**Конкретно:**
- 4 → 6 каскадов (linear split vs PSSM)
- Cascade blend zones (безшовные переходы между каскадами через soft sampling)
- Shadow dithering для low-resolution каскадов (avoid stairstep artifacts)
- PSSM fit-to-scene tuning (compute split lambda per camera position)

**Ключевые файлы:** `src/render/SunShadowCascade.{hpp,cpp}`, `src/shaders/voxel_shadow.frag`,
`src/render/vulkan/VulkanGraphicsPipeline.cpp`.

**DoD:**
- Visual smoke на VoxelLab: тени выглядят не-зернистыми, без stairsteps на стыках каскадов
- `ProjectVSunShadowCascadeSplitsTests` sub-tests на PSSM lambda tuning
- No regression на FlatBenchmark: shadow pass cost +30% допустимо (shadow pass ~10% of frame)

#### ⭐ Задача 7.3. TAA jitter + neighborhood quality — Open

**Цель:** Sub-pixel jittering TAA + neighborhood clamping для стабильности на низкочастотных сценах.

**Конкретно:**
- Halton sequence (2,3) jitter уже есть, проверить distribution
- YCoCg clamping уже есть, добавить history clamping по per-pixel variance threshold
- Neighborhood radius per `agent/knowledge.md §15` — current default 1, попробовать 1.5/2
- Catmull-Rom 9-tap filter на history sample (vs текущий bilinear)

**Ключевые файлы:** `src/render/TaaResolvePipeline.{hpp,cpp}`, `src/shaders/taa_resolve.frag`,
`src/render/Renderer.cpp` (TAA on/off toggle).

**DoD:**
- Visual smoke: стабильность при медленном camera movement (no flicker)
- Performance: TAA pass <0.5 ms на VoxelLab 1920×1080

#### ⭐ Задача 7.4. Lighting exposure + tone mapping pass — Open

**Цель:** Улучшить tone mapping (сейчас базовый Reinhard) + exposure control UI.

**Конкретно:**
- Reinhard → ACES Filmic (per `2026-06-21-tonemap-color-grading` experiment)
- Exposure controls (already in place per LightingDebugView) → связать с `CurrentSceneLighting`
- Auto-exposure based on histogram (опционально)
- Highlight roll-off tuning

**Ключевые файлы:** `src/render/LightingController.{hpp,cpp}`, `src/shaders/voxel.frag`,
`src/render/ToneMapPass.{hpp,cpp}` (новый файл если отсутствует).

**DoD:**
- Reference VoxelLab смотрится кинематографично (vs текущего flat-ish look)
- `LightingDebugView` 11 → 14 (добавить ACES, exposure curve, tone map output)

#### ⭐ Задача 7.5. Post-processing chain polish — Open

**Цель:** Внедрить эксперименты `2026-06-21-bloom-post-processing` (closed) и
`2026-06-21-aerial-perspective` (closed) в mainline.

**Конкретно:**
- Bloom: downsample chain (5 mips), threshold + soft knee, composite additively
- Aerial perspective: distance-based fog (exp или height-based), separate from existing VolumetricFog
- Эти эксперименты marked `STATUS=closed` но не integrated в mainline renderer

**Ключевые файлы:** `docs/experiments/2026-06-21-bloom-post-processing/`,
`docs/experiments/2026-06-21-aerial-perspective/`, новые
`src/render/BloomPass.{hpp,cpp}` + `src/render/AerialPerspectivePass.{hpp,cpp}`.

**DoD:**
- Bloom visible на bright voxels (glass, fluid highlights)
- Aerial perspective даёт depth cue на VoxelLab (distant geometry slightly desaturated)
- Combined cost: post-FX < 1 ms на VoxelLab 1920×1080

### ⏸ Задача 5.2. RTX shadows + smooth specular GI — PAUSED

**Почему на паузе** (operator decision `2026-06-22`): RTX path без работающего TLAS даёт
визуальный no-op (ray query возвращает miss). Существующий non-RTX pipeline ещё не выжат
(см. задачи 7.x). Возобновляется ПОСЛЕ завершения non-RTX polish backlog, чтобы RTX давал
заметный апгрейд на качественной основе, а не маскировал baseline-недоработки.

**Что сделано** (commits `6018c27`, `47ce703`, `285ce79`, `5390dab`, `c528396`):
- BLAS dispatch wired в `BuildDirtyBlases` через one-shot cmd buffer + fence
- AABB geometry per-chunk (`VK_GEOMETRY_TYPE_AABBS_KHR` + `vkCmdUpdateBuffer` + TRANSFER→AS_BUILD barrier)
- `DirtyChunkRebuild { chunkIndex, aabb }` queue
- Shader variants `voxel.frag.rtx.spv` + `voxel.frag.rtx_taa_on.spv` с `rayQueryEXT` + `accelerationStructureEXT rtxTlas` (binding 13)
- `TraceRtxSmoothSpecularRay` для `roughness ≤ kVctCutoffRoughness=0.3`
- `graphicsPipelineRtx` + `graphicsPipelineRtxTaaOn` pipelines
- `ProjectVRayTracedShadowTests` 11/11 sub-tests
- `ProbeHardwareRayTracingSupport` ловит acceleration structure / ray query / deferred host ops / buffer device address caps
- 18x+ fix: device creation pNext chain construction

**Что осталось** (deferred to resume after 7.x):
1. `vkCmdBuildAccelerationStructuresKHR` для TLAS build НЕ диспатчится из `BuildDirtyBlases` — `UpdateTlas` записывает instances в `tlasInstanceBuffer`, но сам TLAS не строится.
2. Ray query в `voxel.frag.rtx` без TLAS возвращает miss → нет визуальной разницы с non-RTX path.
3. Working visual smoke требует populated TLAS + проход по всем `dirtyBlasChunks` (сейчас drained только BLAS, не TLAS).

**Критерий возобновления** (когда открываем снова):
- ☐ Задачи 7.1 (VCT cone density), 7.2 (CSM quality), 7.3 (TAA), 7.4 (tone mapping), 7.5 (post-FX) — все ✅ closed
- ☐ Visual baseline VoxelLab прошёл internal review (operator satisfaction)
- ☐ Performance budget остаётся в норме после non-RTX polish

**После resume** — multi-session задача: добавить `vkCmdBuildAccelerationStructuresKHR` для TLAS
с batched instances, дёргать из `BuildDirtyBlases` когда BLAS queue дренируется. Visual smoke
для подтверждения визуальной разницы RTX vs non-RTX (roughness ≤ 0.3 на гладких материалах
должно показывать ray-traced reflections вместо VCT specular fallback).

### 🔒 Задача 6.2. PIMPL для AppState — DEFERRED PENDING FEASIBILITY

**Статус:** Deferred indefinite. Возобновляется ТОЛЬКО при появлении одного из ниже критериев.

**Критерий возобновления** (любой из):
- **C1. Plugin system / modding API.** Появляется задача на загрузку user-модулей в runtime. PIMPL
  даёт ABI stability boundary, без которого плагины не могут безопасно ссылаться на `AppState`.
- **C2. Shared library / DLL boundary.** Проект начинает линковаться как `.so` / `.dll` для других
  приложений или для hot-reload editor. PIMPL обязателен для ABI compat.
- **C3. Hot-reload editor workflow.** Появляется задача на live-reload `AppState` без перезапуска
  процесса. PIMPL сокращает rebuild time для изменений в `AppState::Impl` (forward declarations
  не инвалидируют зависимые TU).
- **C4. Compile-time penalty > 30s.** Incremental build деградирует до >30 секунд из-за
  транзитивных includes в `AppState.hpp`. PIMPL сокращает header surface.

**Текущие показатели** (проверять quarterly):
- Full rebuild time: ~30s с ccache (target met, C4 не сработал)
- `AppState.hpp` transitive includes: Vulkan + Jolt + SDL + Tracy + Flecs (10+ headers)
- Mechanical refactor scope: ~172 call sites (semicolon/dot typo risk, lifetime issues через `->`)

**Что сделано** (не теряется):
- `static_assert` контракт на размеры major members верифицирован в `src/core/Types.hpp`
- Forward-declaration pattern частично применён (PIMPL в Types.cpp для рендер-классов)

**Recommendation:** Reopen когда появляется C1/C2/C3 trigger. До тех пор — DEFERRED.

### 🔒 Задача 2.3. Sparse Virtual Texturing — DEFERRED PENDING FEASIBILITY

**Статус:** Deferred indefinite. Возобновляется ТОЛЬКО при появлении одного из ниже критериев.

**Критерий возобновления** (любой из):
- **C1. Procedural materials per voxel.** Появляется задача на per-voxel normal maps / detail maps
  / procedural patterns. SVT даёт уникальные текстуры на каждом чанке без раздувания атласа.
- **C2. Material blending across chunk boundaries.** Воксели на границе чанков должны
  визуально смешивать материалы (transition zones, anti-aliased material edges). SVT — стандартное
  решение через page table с seamless UV-интерполяцией.
- **C3. Texture atlas > 256 MiB.** Текущий DoD cap достигнут, требуется масштабирование
  (procedural texture authoring, hi-res scanned surfaces, per-chunk terrain detail).
- **C4. SVO-текстуры для VCT clipmap.** VCT 3D clipmap начинает использовать не volume
  occupancy, а уникальные per-voxel текстуры (sub-voxel detail). Тогда SVT естественно
  расширяется на volume textures.

**Текущие показатели** (проверять quarterly):
- Material count: 5 (Air, Fluid, Glass, FloorWhite, FloorGray) — параметрические, не текстурные
- VCT: 3D clipmap использует occupancy, не текстуры
- `voxel.frag` использует `materialId` напрямую для BRDF / albedo — UV не нужны
- Texture VRAM cap: не достигнут (нет текстур вообще)

**Что сделано** (не теряется):
- 3D clipmap infrastructure landed в `VulkanVoxelizePipeline` (per `agent/knowledge.md §15` lighting
  contract) — pattern переиспользуема для SVT page table когда понадобится
- `feedback buffer` архитектура отработана в VCT (volume occupancy feedback) — готова как
  template для SVT page miss feedback

**Recommendation:** Reopen когда появляется C1/C2/C3/C4 trigger. До тех пор — DEFERRED.

---

## Принципы реализации и DoD (Definition of Done)

Эти правила обязательны для всех будущих задач:

1. **Производительность:** Оптимизации проверяются бенчмарками (`ProjectVTests`,
   `ProjectVFrustumCullBenchmark`, `ProjectVShadowProjectionBenchmark`). Критерий принятия —
   ускорение hot path на **5–10%** (per `agent/knowledge.md Part A §2`).

2. **Детерминизм Fluid CA:** Никаких FP в simulation. Итерация строго детерминирована
   (z, y, x ascending).

3. **Портируемость:** Изменения собираются и проходят тесты на обоих dev-контурах:
   - `linux-clang-debug` (native clang 22 + libstdc++)
   - `windows-clang-debug` (clang-cl 22 + MSVC STL)

4. **Безопасность типов:**
   - `std::expected<T, E>` для cold path (I/O, asset load, init)
   - `bool` return + `PV_ASSERT` на инвариантах для hot path (render, cull, simulation)
   - Jolt includes: `<Jolt/Jolt.h>` обязан быть ПЕРВЫМ во всех TU, использующих физику

5. **Комментарии:** Код должен быть чистым. Документация извлекается в `COMMENTS.md`.
   `// EVIL:` только для неочевидных хаков или захардкоженных математических констант.

6. **Стратегический порядок:** non-RTX polish (7.x) → возобновление 5.2 RTX → deferred
   roadmap (6.2/2.3 при появлении trigger-критериев). RTX без работающего TLAS = no-op
   upgrade; не маскировать baseline-недоработки премиум-фичей.

---

## История (closed roadmap)

Полная история выполненных задач — `CHANGELOG.md` (Keep a Changelog формат). Per-task design
rationale — `COMMENTS.md`. Долговечные технические факты — `agent/knowledge.md`. Per-session
narrative — `agent/workspace.md`.

Краткая сводка по фазам:

| Phase | Tasks | Status |
|-------|-------|--------|
| 1. Voxel Database & GPU Storage | 1.1 NanoVDB, 1.2 Dedup, 1.3 Audio async | ✅ all closed |
| 2. GPU-Driven Geometry | 2.1 HZB, 2.2 Mesh Shaders | ✅ · 2.3 SVT 🔒 deferred-pending |
| 3. Physics & Simulation | 3.1 GPU Fluid CA, 3.2 Incremental Jolt, 3.3 Greedy merger | ✅ all closed |
| 4. Procedural Generation & LOD | 4.1 World Gen, 4.2 LOD, 4.3 Draw distance | ✅ all closed |
| 5. GI & Temporal Effects | 5.1 VCT, 5.3 TAA MV | ✅ · 5.2 RTX ⏸ paused (post-7.x) |
| 6. Refactoring | 6.1 ECS, 6.3 Async Compute | ✅ · 6.2 PIMPL 🔒 deferred-pending |
| 7. Non-RTX Rendering Polish | 7.1-7.5 (VCT cones, CSM, TAA, tonemap, post-FX) | 🔓 all open |