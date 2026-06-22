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
- RTX shadows foundation (BLAS dispatch + AABB geometry + rayQueryEXT shaders) — TLAS population pending

**Активные TODO**: см. секцию ниже. История закрытых задач — `CHANGELOG.md` + `agent/workspace.md`.

---

## Active tasks (2026-06-22)

**Сводка:** 1 ⏸️ Partial · 2 🔓 Open · 0 ✅ Closed

### Задача 2.3. Sparse Virtual Texturing — 🔓 Open (deferred)

**Суть:** Page-table-driven виртуальное текстурирование уникальных поверхностей воксов (Feedback Buffer из фрагментного шейдера → CPU readback → lazy page upload → SSD staging).

**Целесообразность сейчас: НИЗКАЯ.**
- Текущие материалы: 5 шт. (`Air`, `Fluid`, `Glass`, `FloorWhite`, `FloorGray`). Параметризуются таблицами, не текстурами.
- VCT 3D clipmap (`Stage 5.1`) уже даёт volume detail без UV.
- `voxel.frag` использует `materialId` напрямую для BRDF / albedo — UV не нужны.
- SVT вводит feedback-loop GPU↔CPU sync per frame; текущая архитектура сознательно минимизирует sync points.
- DoD требует 256 MiB cap на текстуры, что нерелевантно при 5 параметрических материалах.

**Когда пересмотреть:** когда (а) появятся процедурные материалы (per-voxel normal/detail maps), либо (б) материалы начнут смешиваться через границы чанков (тогда нужны непрерывные UV). Сейчас не блокирует MVP.

### Задача 5.2. RTX shadows + smooth specular GI — ⏸️ Partial

**Что сделано** (commits `6018c27`, `47ce703`, `285ce79`, `5390dab`, `c528396`):
- BLAS dispatch wired в `BuildDirtyBlases` через one-shot cmd buffer + fence (was synchronous, now async-correct post-18x fix)
- AABB geometry per-chunk (`VK_GEOMETRY_TYPE_AABBS_KHR` + `vkCmdUpdateBuffer` + TRANSFER→AS_BUILD barrier)
- `DirtyChunkRebuild { chunkIndex, aabb }` queue
- Shader variants `voxel.frag.rtx.spv` + `voxel.frag.rtx_taa_on.spv` с `rayQueryEXT` + `accelerationStructureEXT rtxTlas` (binding 13)
- `TraceRtxSmoothSpecularRay` для `roughness ≤ kVctCutoffRoughness=0.3`
- `graphicsPipelineRtx` + `graphicsPipelineRtxTaaOn` pipelines (descriptor pool binding 13 только при `PROJECTV_HW_RAY_TRACING=ON`)
- `ProjectVRayTracedShadowTests` 11/11 sub-tests
- Probe (`ProbeHardwareRayTracingSupport`) ловит acceleration structure / ray query / deferred host ops / buffer device address caps
- 18x+ fix: chain chain construction (`VkDeviceCreateInfo::pNext`) — ранее RTX features не доходили до device без `VK_KHR_swapchain_maintenance1`

**Что осталось** (deferred):
1. `vkCmdBuildAccelerationStructuresKHR` для TLAS build НЕ диспатчится из `BuildDirtyBlases` — `UpdateTlas` записывает instances в `tlasInstanceBuffer`, но сам TLAS не строится.
2. Ray query в `voxel.frag.rtx` без TLAS возвращает miss → нет визуальной разницы с non-RTX path.
3. Working visual smoke требует populated TLAS + проход по всем `dirtyBlasChunks` (сейчас drained только BLAS, не TLAS).

**Когда пересмотреть:** после стабилизации VCT (Stage 5.1) и завершения текущих bug-fix сессий. Multi-session задача: добавить `vkCmdBuildAccelerationStructuresKHR` для TLAS с batched instances, дёргать из `BuildDirtyBlases` когда BLAS queue дренируется.

### Задача 6.2. PIMPL для AppState — ⏸️ Partial

**Что сделано:**
- `static_assert` контракт верифицирован в `src/core/Types.hpp` (size checks для major AppState members).
- Forward-declaration pattern применяется частично (PIMPL в Types.cpp для рендер-классов).

**Что осталось** (deferred):
- Mechanical sed `state->render().X` → `state->render()->X` по ~172 call sites.
- Перенос реализации `AppState` в `src/core/Types.cpp` / новый `.ixx` модуль.
- Закрытие `AppState.hpp` от транзитивных зависимостей (Vulkan, Jolt, SDL, Tracy, Flecs).

**Целесообразность сейчас: НИЗКАЯ.**
- Полная компиляция с нуля ~30 секунд; ccache + unity builds уже компенсируют.
- На текущем этапе (MVP slice) нет shared library boundary — ABI stability не нужна.
- Multi-agent coordination работает через scope discipline + per-file commits, не через ABI.
- Механический refactor 172 call sites — реальный риск регрессий (semicolon/dot typos, lifetime issues с `->` через `unique_ptr`).

**Когда пересмотреть:**
- При появлении plugin system / hot-reload editor → нужен ABI stability.
- При появлении shared library / DLL boundary → нужен PIMPL.
- При первой серьёзной compile-time боли от текущих транзитивных includes (что пока не наблюдается).

**Recommendation: DEFERRED INDEFINITELY** — нет явного триггера, ROI низкий. Если появится запрос на plugin system или shared lib — открываем задачу с новой конкретикой.

---

## Принципы реализации и DoD (Definition of Done)

Эти правила остаются обязательными для всех будущих задач:

1. **Производительность:** Оптимизации проверяются бенчмарками (`ProjectVTests`, `ProjectVFrustumCullBenchmark`, `ProjectVShadowProjectionBenchmark`). Критерий принятия — ускорение hot path на **5–10%** (per `agent/knowledge.md Part A §2`).

2. **Детерминизм Fluid CA:** Никаких FP в simulation. Итерация строго детерминирована (z, y, x ascending).

3. **Портируемость:** Изменения собираются и проходят тесты на обоих dev-контурах:
   - `linux-clang-debug` (native clang 22 + libstdc++)
   - `windows-clang-debug` (clang-cl 22 + MSVC STL)

4. **Безопасность типов:**
   - `std::expected<T, E>` для cold path (I/O, asset load, init)
   - `bool` return + `PV_ASSERT` на инвариантах для hot path (render, cull, simulation)
   - Jolt includes: `<Jolt/Jolt.h>` обязан быть ПЕРВЫМ во всех TU, использующих физику

5. **Комментарии:** Код должен быть чистым. Документация извлекается в `COMMENTS.md`. `// EVIL:` только для неочевидных хаков или захардкоженных математических констант.

---

## История (closed roadmap)

Полная история выполненных задач — `CHANGELOG.md` (Keep a Changelog формат). Per-task design rationale — `COMMENTS.md`. Долговечные технические факты — `agent/knowledge.md`. Per-session narrative — `agent/workspace.md`.

Краткая сводка по фазам:

| Phase | Tasks | Status |
|-------|-------|--------|
| 1. Voxel Database & GPU Storage | 1.1 NanoVDB, 1.2 Dedup, 1.3 Audio async | ✅ all closed |
| 2. GPU-Driven Geometry | 2.1 HZB, 2.2 Mesh Shaders | ✅ · 2.3 SVT 🔓 Open |
| 3. Physics & Simulation | 3.1 GPU Fluid CA, 3.2 Incremental Jolt, 3.3 Greedy merger | ✅ all closed |
| 4. Procedural Generation & LOD | 4.1 World Gen, 4.2 LOD, 4.3 Draw distance | ✅ all closed |
| 5. GI & Temporal Effects | 5.1 VCT, 5.3 TAA MV | ✅ · 5.2 RTX ⏸️ Partial |
| 6. Refactoring | 6.1 ECS, 6.3 Async Compute | ✅ · 6.2 PIMPL ⏸️ Partial |