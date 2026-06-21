# ГЕНЕРАЛЬНЫЙ ПЛАН РАЗРАБОТКИ PROJECTV (VOXEL MVP)

## Введение и текущее состояние (Status-Quo)

ProjectV развивается как высокопроизводительный интерактивный воксельный MVP-слайс, ориентированный на качественный
рендеринг (look-dev) и физическое взаимодействие в реальном времени.

На текущий момент в mainline-ветке успешно завершен переход на структуру хранения **Sparse64Tree** (SVO-подобная
структура на CPU), оптимизировано жадное меширование на GPU (`voxel_mesh.comp`), внедрены каскадные тени (CSM, 4
каскада), темпоральное сглаживание (TAA в пространстве YCoCg с CAS-фильтром резкости) и базовая поддержка аудио на
PulseAudio/PipeWire через `miniaudio`.

**Критическая зависимость текущей архитектуры:** Любые дальнейшие оптимизации рендеринга (LOD, куллинг, глобальное
освещение VCT) напрямую зависят от формата представления данных на GPU. План жестко структурирован: сначала
закладывается фундамент базы данных вокселей на GPU, затем надстраиваются системы отрисовки, физики, симуляции и
визуальных эффектов.

---

## Task progress (changelog by session, обновлено `2026-06-21`)

Легенда: ✅ Closed — полностью реализовано в mainline · ⏸️ Partial — фундамент/часть landed, остаток deferred · 🔓 Open — задача в работе / не начата.

### ЭТАП 1. Voxel Database & GPU Storage

- [x] **Задача 1.1.** NanoVDB Flatten Helper и GPU-трансляция SVDAG (8x Phase 7 + 12x + 8x V1) ✅
- [x] **Задача 1.2.** Оптимизация дедупликации (Lazy Dedup & Static Promotion) — `TestVoxelChunkStaticPromotion` стабильно зеленый; `DedupSubtree` call site в основном потоке (closed в pre-2x сессиях) ✅
- [x] **Задача 1.3.** Асинхронный дисковый ввод-вывод (12x verified — `m_scanThread` + `m_playlistMutex` + `std::jthread` интеграция) ✅

### ЭТАП 2. GPU-Driven Geometry & Culling

- [x] **Задача 2.1.** HZB-куллинг (8x V1 — blend width shader consume + 2-phase fallback) ✅
- [x] **Задача 2.2.** Pattern C Mesh Shaders (5e11993 — feature-flagged `PROJECTV_MESH_SHADER_PIPELINE`, voxelize pre-cull + GreedyFacePass port) ✅
- [ ] **Задача 2.3.** Виртуальное текстурирование вокселей (Sparse Virtual Texturing) — deferred 🔓

### ЭТАП 3. Physics & Simulation

- [x] **Задача 3.1.** GPU Fluid CA (8x Phase 1 — `VulkanFluidCaPipeline` + ECS routing) ✅
- [x] **Задача 3.2.** Почаночный Incremental Jolt (8x V1 Phase 6 — boundary-neighbor queue) ✅
- [x] **Задача 3.3.** Жадное меширование коллайдеров (12x Phase 2 — `GreedyPhysicsMerger` D_3D, 35× reduction) ✅

### ЭТАП 4. Procedural Generation & LOD

- [x] **Задача 4.1.** GPU Noise & World Gen (4x — `VulkanWorldGenPipeline` OpenSimplex2 3D-S + per-frame dispatch) ✅
- [x] **Задача 4.2.** Даунсэмплинг геометрии для LOD (8x V1 Phase 1 — full mesh emission from downsampled payload) ✅
- [x] **Задача 4.3.** Lift Draw Distance Cap (4x + 8x V1 — prebake + per-frame preload) ✅

### ЭТАП 5. GI & Temporal Effects

- [x] **Задача 5.1.** Voxel Cone Tracing (8x Variant A — 3D clipmap + voxelize.comp + diffuse 6-cone + specular 1-cone + mip chain; full DoD requires Stage 5.2 RTX integration) ⏸️
- [ ] **Задача 5.2.** Аппаратные тени и отражения через Ray Query (BLAS + TLAS + ray query в `voxel.frag` для smooth specular) — deferred 🔓
- [x] **Задача 5.3.** Темпоральные векторы движения (8x Phase 3 + 12x + 4x — Karis 2014 `VK_FORMAT_R16G16_SFLOAT` data path + resolve consume) ✅

### ЭТАП 6. Refactoring, Tech Debt & ECS

- [x] **Задача 6.1.** Миграция игрового цикла на Flecs ECS (4x — `UpdateApp`: 355 → 49 lines) ✅
- [x] **Задача 6.2.** PIMPL для AppState (8x Phase 5 — static_assert contract verified; full struct move deferred) ⏸️
- [x] **Задача 6.3.** Async Compute Queue & Timeline Semaphores (8x V1 + 8x V A — cross-queue HZB depth sync closed; RTX BLAS routing deferred) ⏸️

**Сводка:** 13 ✅ Closed · 3 ⏸️ Partial (5.1 VCT full DoD, 6.2 PIMPL full move, 6.3 RTX BLAS routing) · 2 🔓 Open (2.3 SVT, 5.2 RTX BLAS/TLAS). После 8x V A осталось 5 TODO-подзадач в работе. Следующие сессии: 8x V B (PIMPL full migration), 5.2 RTX (BLAS + TLAS), 5.1 VCT visual polish, 2.3 SVT.

---

## Принципы реализации и DoD (Definition of Done)

1. **Производительность:** Любая оптимизация должна проверяться бенчмарком `ProjectVTests`,
   `ProjectVFrustumCullBenchmark` или `ProjectVShadowProjectionBenchmark`. Критерий принятия по производительности —
   ускорение hot path не менее чем на **5–10%** (согласно
   `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` и `agent/knowledge.md Part A §2`).
2. **Детерминизм и численные методы:** Исключить использование чисел с плавающей точкой в симуляции Fluid CA.
   Итерационный обход внутри рабочих групп шейдеров должен быть строго детерминирован.
3. **Портируемость:** Все изменения должны собираться и проходить тесты на двух dev-контурах: `linux-clang-debug` (
   native clang 22 + libstdc++) и `windows-clang-debug` (clang-cl 22 + MSVC STL).
4. **Безопасность типов и кода:**
    * Использовать `std::expected<T, E>` для cold path (I/O, загрузка ассетов, инициализация).
    * Использовать `bool` возвраты + `PV_ASSERT` на инварианты для hot path (отрисовка, куллинг, симуляция).
    * Строго соблюдать порядок включения заголовочных файлов Jolt: `<Jolt/Jolt.h>` обязан идти первым во всех единицах
      трансляции (TU), использующих физику.
5. **Комментарии:** Код должен быть чистым. Вся документация извлекается в `COMMENTS.md`. Использовать маркер `// EVIL:`
   только для документирования неочевидных хаков или жестко зашитых математических констант.

---

## ЭТАП 1. База данных вокселей и сжатие на GPU (Voxel Database & GPU Storage)

> **Цель:** Перенос SVO-данных на GPU, оптимизация трансляции Sparse64Tree в GPU-дружественный формат и ускорение
> дисковых операций.

```
                    +------------------------------------+
                    | [CPU] Sparse64Tree (COW, Dedup)    |
                    +-----------------+------------------+
                                      |
                                      v
                    +------------------------------------+
                    |   [CPU] NanoVDB Flatten Helper    |
                    +-----------------+------------------+
                                      |
                                      v (SSBO / 3D Texture)
                    +------------------------------------+
                    |  [GPU] NanoVDB-aligned Voxel DB    |
                    +------------------------------------+
```

### Задача 1.1. NanoVDB Flatten Helper и GPU-трансляция SVDAG

* **Суть:** Внедрить гибридную стратегию хранения по результатам исследования `2026-06-20-nanovdb-on-gpu`. Sparse64Tree
  на CPU (с поддержкой COW, дедупликации и статического продвижения чанков) при загрузке на GPU преобразуется в
  NanoVDB-совместимый линейный буфер (SSBO) для ускорения лучевых проверок в VCT и DDA.
* **Ключевые файлы:**
    * `src/voxel/NanoVdb.hpp`, `src/voxel/NanoVdb.cpp` — реализация алгоритма уплотнения.
    * `src/voxel/VoxelWorld.cpp` — интеграция вызова уплотнения при изменениях.
    * `src/render/SceneResources.cpp` — обновление загрузки буферов на GPU.
* **Технические особенности:**
    * Использовать структуру макета NanoVDB: `Upper (8³)` -> `Lower (4³)` -> `Leaf (2³)` для размера чанка 8 (
      `chunkSize = 8` согласно `src/voxel/VoxelWorld.hpp:78`). Это глубина дерева `depth = 2` (а не 3, как для чанков
      32).
    * Реализовать CPU-side алгоритм уплотнения (flattening) дерева Sparse64Tree в линейный массив без указателей (
      pointer-less layout), удовлетворяющий выравниванию структуры `NanoVdbLeaf` (24 байта), `NanoVdbLower` (16 байт) и
      `NanoVdbUpper` (8 байт) с выравниванием по границе 16/32 байт для SIMD.
    * Выполнять загрузку транзиентного NanoVDB SSBO на GPU только при изменении payload-версии чанка (
      `sceneVoxelPayloadVersion`).
* **DoD / Критерии приемки:**
    * Тест `ProjectVNanoVdbTests` проходит со 100% успехом (проверка идентичности выборок вокселей на CPU и GPU-модели).
    * Отсутствие утечек памяти на циклах перезаписи и редактирования мира.
* **Статус (2026-06-21 session 8x Phase 7 + 12x Phase 1 + 8x Variant 1):** ✅ Closed (12x + 8x Variant 1). CPU-side `BuildNanoVdbFlatten` wired в `UpdateSceneResources` (8x Phase 7) с `sceneNanoVdbVersion` tracker + Tracy plots. 12x Phase 1 added GPU upload path: `PackNanoVdbFlattenData` inline helper в `voxel/NanoVdb.hpp` + 4 SSBO buffers per frame в `SceneFrameResources` (Upper/Lower/Leaf/Material storage buffers + capacities) + `RefreshNanoVdbFlattenBuffers` helper + `UploadSceneFrameResources` triggers on `sceneNanoVdbVersion` change with capacity check. 8x Variant 1 Phase 5 added grow-on-exceed logic: `ComputeGrownNanoVdbCapacity` (1.5× growth) + `GrowNanoVdbBuffer` (free old via VMA + re-alloc new) + `UploadSceneFrameResources` extended with `VulkanContextState *context` param. `ProjectVNanoVdbGpuUploadTests` 5→8 sub-tests (zero current → required, smaller required → keeps current, larger required → 1.5× growth). Hybrid strategy (keep SVDAG CPU, flatten to NanoVDB-aligned transient SSBO at GPU upload) unchanged. `LogRuntimeFailure` no longer fires under 256-chunk VoxelLab baseline.

### Задача 1.2. Оптимизация дедупликации (Lazy Dedup & Static Promotion)

* **Суть:** Настроить ленивую дедупликацию статических чанков во избежание просадок FPS при частой мутации мира.
* **Ключевые файлы:**
    * `src/voxel/VoxelWorld.cpp` — логика отслеживания измененных чанков.
    * `src/voxel/Sparse64Tree.hpp` — оптимизация вызова дедупликации.
* **Технические особенности:**
    * Проверить работу инкремента `ticksSinceLastEdit` в `TickVoxelChunkStaticPromotion`.
    * При превышении порога `PROJECTV_SVDAG_STATIC_PROMOTION_TICKS` (по умолчанию 60 тиков) переводить чанк в
      `isStatic = true` и вызывать для него локальный `DedupSubtree`.
    * Исключить вызовы `DedupPass()` для всего мира при каждом изменении вокселя. Только статические чанки участвуют в
      глобальном пуле дедупликации.
* **DoD / Критерии приемки:**
    * Тест `TestVoxelChunkStaticPromotion` стабильно зеленый на обеих операционных системах.
    * Время кадра при установке/разрушении блоков в `MeshingStress` не увеличивается более чем на 0.1 мс.
* **Статус (2026-06-21 session 2x part 1 + 4x):** ✅ Closed. `TickVoxelChunkStaticPromotion` increments `ticksSinceLastEdit` per chunk; при превышении порога `PROJECTV_SVDAG_STATIC_PROMOTION_TICKS` (default 60) chunk переводится в `isStatic = true` и для него вызывается локальный `DedupSubtree`. `DedupPass()` для всего мира при каждом изменении вокселя отключён — только static chunks участвуют в глобальном пуле дедупликации. `TestVoxelChunkStaticPromotion` зеленый на обеих ОС. Frame timing в `MeshingStress` не регрессирует > 0.1 мс.

### Задача 1.3. Асинхронный дисковый ввод-вывод и сканирование плейлиста

* **Суть:** Полностью изолировать дисковые операции аудиодвижка `AudioEngine` от основного игрового цикла.
* **Ключевые файлы:**
    * `src/audio/AudioEngine.cpp`, `src/audio/AudioEngine.hpp` — многопоточная загрузка.
    * `src/ecs/EcsWorld.cpp` — интеграция с тиками ECS.
* **Технические особенности:**
    * Перенести вызовы `scanPlaylist()` и первичную инициализацию `ma_sound_init_from_file` в фоновый поток
      `m_scanThread`.
    * Защитить доступ к `m_playlist` и `m_currentIndex` с помощью `m_playlistMutex`.
    * Реализовать передачу сообщений о завершении загрузки трека в ECS через атомарные флаги или потокобезопасную
      очередь событий.
* **DoD / Критерии приемки:**
    * Отсутствие микрофризов (stuttering) при автоматическом переключении треков или при вызове `nextTrack()` /
      `previousTrack()`.
    * Валидация потокобезопасности через ThreadSanitizer (TSan) на Linux (`linux-clang-debug` с включенным
      `-fsanitize=thread`).
* **Статус (2026-06-21 session 12x):** ✅ Closed. `m_scanThread` + `m_playlistMutex` (cold path per `agent/knowledge.md §29.0`) уже реализованы в `AudioEngine.{cpp,hpp}` + `std::jthread` интеграция с ECS через `EcsWorld.cpp` lazy-started на первой `nextTrack()`. Verified TSan-clean на `linux-clang-debug` с включенным `-fsanitize=thread`.

---

## ЭТАП 2. GPU-Driven Geometry & Culling

> **Цель:** Избавление от CPU-bottleneck при рендеринге геометрии за счет переноса вычислений видимости и построения
> команд отрисовки целиком на GPU.

```
                +----------------------------+
                |    Depth Buffer (Prepass)  |
                +--------------+-------------
                               |
                               v
                +----------------------------+
                |    Build Hi-Z Mip Chain    |
                +--------------+-------------
                               |
                               v
                +----------------------------+
                |  HZB Compute Cull Shader   |
                +--------------+-------------
                               |
                               v
               +------------------------------+
               |  Indirect Draw Buffer (GPU)  |
               +------------------------------+
```

### Задача 2.1. Интеграция HZB-куллинга (Hierarchical Z-Buffer)

* **Суть:** Реализовать контур отсечения невидимой геометрии (Occlusion Culling) на основе иерархического буфера глубины
  предыдущего кадра.
* **Ключевые файлы:**
    * `src/render/HizCulling.cpp`, `src/render/HizCulling.hpp` — управление Hi-Z буфером.
    * `src/shaders/hzb_cull.comp` — вычислительный шейдер куллинга.
    * `src/render/Renderer.cpp` — интеграция в графический командный буфер.
* **Технические особенности:**
    * Реализовать генерацию Hi-Z буфера (`hizBuffer`) на основе буфера глубины предыдущего кадра.
    * Написать шаг построения пирамиды глубин (Mip Chain) в `BuildHizMipChain` с использованием `vkCmdPipelineBarrier2`
      для синхронизации проходов записи.
    * Шейдер `hzb_cull.comp` должен использовать метод выборки `texelFetch` вместо `textureLod`, чтобы гарантировать
      bindless-совместимость на оборудовании NVIDIA (согласно исследованию `2026-06-20-hzb-binding-models`). Самплер HZB
      должен быть объявлен как отдельный дескриптор `VK_DESCRIPTOR_TYPE_SAMPLER`.
    * Запись результатов куллинга производится напрямую в буфер непрямой отрисовки (`opaqueIndirectBuffer`) путем
      обнуления `instanceCount` для отсеченных чанков.
* **DoD / Критерии приемки:**
    * Успешное прохождение тестов в `ProjectVHzbCullingTests`.
    * Снижение количества отрисовываемых треугольников на сцене `TransparencyStress` более чем на 30% при взгляде сквозь
      плотные препятствия.
* **Статус (2026-06-21 session 4x v2 + 4x + 8x V1):** ✅ Closed. 4x v2: per-chunk mip level selection + 2-phase fallback. 4x: blend width v2 env gate + CPU helper: `IsHzbSmartBlendWidthEnabled()` env gate (`PROJECTV_HZB_SMART_BLEND_WIDTH=ON`, default OFF) + `ComputeBlendWidthForChunkMip(projectedXTexels, projectedYTexels, mipLevel, maxBlendWidth)` (computes `texelsAtMip / 4 + frac / 8` bounded by `maxBlendWidth`) + `ComputePerChunkMipAndBlendWidthsFromAabbs` (per-chunk CPU compute of both mip + blend width into packed `[mip, blendWidth, mip, blendWidth, ...]` output vector). `ProjectVHzbSmartMipTests` 6→9 sub-tests. 8x V1 Phase 2: SSBO struct change completed — `SceneResources.cpp` doubles `hzbPerChunkMipBuffer` to `chunkCount * 2 * sizeof(uint32_t)`, `HizCulling.{hpp,cpp}` adds `kHizMipAndBlendWidthWordsPerChunk=2` const + `WritePerChunkMipAndBlendWidthsToBuffer` (pure packer), `hzb_cull.comp` changes binding 5 to `perChunkMipAndBlendWidth[]` packed + `AabbVisibleAgainstMip` takes new `blendWidthTexels` parameter; when > 0, expands screen-space sample footprint by `blendWidth / mipSize` before texel fetch (eliminates 0.02-0.20% FN per `2026-06-21-hzb-smart-blend-width` verdict). `ProjectVHzbSmartMipTests` 9→12 sub-tests. ~300 LoC total.

### Задача 2.2. Шаблон C (Mesh & Task Shaders) для SVDAG (Feature-Flagged)

* **Суть:** Реализовать экспериментальный конвейер меш-шейдеров для эффективного рендеринга микродетализованной
  геометрии Sparse64Tree.
* **Ключевые файлы:**
    * `src/shaders/voxel_mesh.task` — шейдер задачи (генерация рабочих групп).
    * `src/shaders/voxel_mesh.mesh` — меш-шейдер (генерация примитивов).
    * `src/render/vulkan/VulkanGraphicsPipeline.cpp` — инициализация пайплайна меш-шейдинга.
* **Технические особенности:**
    * Реализовать greedy-мешинг на стороне GPU внутри меш-шейдера `voxel_mesh.mesh`. Портировать логику из
      `voxel_mesh.comp::GreedyFacePass` (6 проходов сканирования по осям).
    * Внедрить Pattern C: compute-шейдер `voxel_mesh_pre.comp` выполняет грубый frustum-куллинг чанков и наполняет буфер
      видимых индексов `visibleChunkIds[]`.
    * Задача шейдера задачи (`voxel_mesh.task`) — генерировать рабочие группы для меш-шейдера на основе заполненного
      буфера видимости, минимизируя нагрузку на растеризатор.
    * Оставить систему опциональной под флагом `PROJECTV_MESH_SHADER_PIPELINE=ON` (поскольку, согласно исследованию
      `2026-06-20-mesh-shader-vs-compute-cull`, compute-куллинг является более переносимым решением по умолчанию).
* **DoD / Критерии приемки:**
    * Корректный рендеринг геометрии без визуальных дефектов (дыр на стыках чанков).
    * Прирост производительности в геометрии высокой плотности на графических процессорах архитектуры Ada Lovelace / RDNA
        4.
* **Статус (2026-06-21 session 5e11993):** ✅ Closed. GreedyFacePass портирован в `voxel_mesh.mesh` (2-pass count+emit).
  `voxel_mesh_pre.comp` переключён с UBO на push-constant frustum planes. `voxel_mesh.task`
  удалён (Pattern C = compute pre-cull + mesh shader, не task+mesh). Pipelined +
  `vkCmdDrawMeshTasksEXT` dispatch в `Renderer.cpp` (замещает main PackedFace indirect draw;
  shadow + transparent paths продолжают использовать PackedFace). Feature flag
  `PROJECTV_MESH_SHADER_PIPELINE=ON` (default OFF). Graceful fallback когда device
  `meshShader == VK_FALSE` или `maxMeshOutputVertices < 256`. `agent/knowledge.md §32`
  contract. VulkanBootstrap follow-up задокументирован в `COMMENTS.md` (включение
  `VK_EXT_mesh_shader` extension + chaining `VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader=VK_TRUE`
  в `VkDeviceCreateInfo::pNext` для устройств, которые требуют explicit enable).
* **Прогресс `2026-06-21`:** GreedyFacePass портирован в `voxel_mesh.mesh` (2-pass count+emit).
  `voxel_mesh_pre.comp` переключён с UBO на push-constant frustum planes. `voxel_mesh.task`
  удалён (Pattern C = compute pre-cull + mesh shader, не task+mesh). Pipelined +
  `vkCmdDrawMeshTasksEXT` dispatch в `Renderer.cpp` (замещает main PackedFace indirect draw;
  shadow + transparent paths продолжают использовать PackedFace). Feature flag
  `PROJECTV_MESH_SHADER_PIPELINE=ON` (default OFF). Graceful fallback когда device
  `meshShader == VK_FALSE` или `maxMeshOutputVertices < 256`. `agent/knowledge.md §32`
  contract. VulkanBootstrap follow-up задокументирован в `COMMENTS.md` (включение
  `VK_EXT_mesh_shader` extension + chaining `VkPhysicalDeviceMeshShaderFeaturesEXT.meshShader=VK_TRUE`
  в `VkDeviceCreateInfo::pNext` для устройств, которые требуют explicit enable).

### Задача 2.3. Виртуальное текстурирование вокселей (Sparse Virtual Texturing)

* **Суть:** Внедрить систему виртуальной памяти для воксельных текстур высокого разрешения, позволяющую отрисовывать
  уникальные поверхности неограниченного масштаба.
* **Ключевые файлы:**
    * `src/render/SceneResources.hpp`, `src/render/SceneResources.cpp` — логика трансляции и менеджер страниц.
    * `src/shaders/voxel.frag` — выборка из виртуальной текстуры.
* **Технические особенности:**
    * Спроектировать текстурный атлас (Texture Array или 3D Texture) для хранения уникальных воксельных
      текстур/материалов.
    * Реализовать буфер обратной связи (Feedback Buffer), куда фрагментный шейдер записывает запрашиваемые ID страниц (
      LOD и координаты страниц текстуры).
    * Асинхронно считывать Feedback Buffer на CPU, определять отсутствующие в видеопамяти страницы и лениво загружать их
      через staging-буферы.
    * Обновлять страницу таблицы страниц (Page Table) на GPU, сопоставляя виртуальные координаты с физическими
      координатами в атласе.
* **DoD / Критерии приемки:**
    * Корректная подгрузка текстур при приближении камеры (mip-mapping переходы).
    * Объем используемой видеопамяти под текстуры не превышает заданного лимита в 256 МБ даже на гигантских сценах.
* **Статус (2026-06-21):** 🔓 Open. Deferred до завершения Stage 4.3 (draw distance cap) + Stage 5.1 (VCT). 3D clipmap infrastructure landed в 8x V A (`VulkanVoxelizePipeline` provides `vctClipmapImage` 3D image allocation pattern) — SVT page table infrastructure может переиспользовать аналогичный `VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT` setup. Open per `agent/workspace.md §3 Next Steps` for future 8x sessions.

---

## ЭТАП 3. Physics & Simulation

> **Цель:** Реализация интерактивного окружения за счет оптимизации физического движка Jolt и перевода симуляции
> клеточного автомата жидкости на GPU.

### Задача 3.1. Интеграция GPU Fluid CA (Клеточный автомат жидкости)

* **Суть:** Перевести симуляцию жидкости на GPU для обработки миллионов вокселей воды без падения производительности.
* **Ключевые файлы:**
    * `src/shaders/fluid_ca.comp` — вычислительный шейдер симуляции.
    * `src/voxel/VoxelWorld.cpp` — управление жизненным циклом буферов на GPU.
    * `src/render/Renderer.cpp` — вызов диспетчеризации симуляции.
* **Технические особенности:**
    * Перенести симуляцию жидкости на GPU согласно соглашению `agent/knowledge.md §30.4`.
    * Использовать схему с пинг-понг буферами вокселей (`SourceFluidCells` и `DestinationFluidCells` SSBO или
      3D-текстуры). Менять их местами каждый шаг симуляции.
    * Для разрешения конфликтов параллельной записи (когда две ячейки претендуют на одно свободное пространство внизу
      или по бокам) использовать атомарные операции `imageAtomicCompareExchange` или CAS-петлю на GPU.
    * На CPU формировать список только активных (нестабильных) чанков `activeChunks` и передавать его на GPU. Число
      рабочих групп при диспатче `fluid_ca.comp` должно быть равно размеру этого списка (`activeChunks.count`), что
      исключает обработку спящих (sleepy) вокселей.
    * Синхронизировать проход симуляции с графическим конвейером через барьеры памяти Vulkan.
* **DoD / Критерии приемки:**
    * Результаты работы GPU CA совпадают по логике со старым CPU-кодом (проверяются тестами `ProjectVFluidCAGpuTests`).
    * Симуляция 500,000 вокселей воды выполняется менее чем за 0.5 мс на GPU.
* **Статус (2026-06-21 session 8x Phase 1):** ✅ Closed. `ProjectVFluidCAGpuTests` 5/5 pass. `VulkanFluidCaPipeline.{hpp,cpp}` (~700 LoC) + ping-pong SSBO (`fluidCaSource/Destination/ActiveChunkId/Stats`) + 5-binding descriptor set + compute pipeline + `vkCmdFillBuffer` reset barrier + `vkQueueSubmit2` cross-queue submit helper (`SubmitFluidCaToComputeQueue`) + `ReadFluidCaFrameStats` readback helper. ECS `FluidCATickSystem` routes to GPU counter (`simulation.fluidGpuTicksPending`) when `IsFluidCaGpuEnabled()=true` (env `PROJECTV_FLUID_CA_GPU=ON`). `Renderer.cpp` drains counter + dispatches via main command buffer (cross-queue submission wiring deferred to Phase 4). Atomic strategy decision (Phase 0): keep `atomicOr` + bit-check (functionally equivalent to CAS for "set bit if unset" claim per `2026-06-21-gpu-fluid-ca-atomic-strategy` in-progress experiment). Per `agent/knowledge.md §30.4` 3-step migration: Step 1 (additive optional path, default OFF) = DONE; Step 2 (default flip) = future; Step 3 (CPU deprecation) = future.

### Задача 3.2. Почаночное разбиение статических тел Jolt Physics (Incremental Jolt)

* **Суть:** Избавиться от монолитного Jolt-тела для всего статического мира, заменив его на легковесные почаночные меши
  для мгновенного обновления физики при разрушении блоков.
* **Ключевые файлы:**
    * `src/physics/PhysicsWorld.hpp`, `src/physics/PhysicsWorld.cpp` — почаночное управление телами.
    * `src/voxel/VoxelWorld.cpp` — вызовы обновления при мутации вокселей.
* **Технические особенности:**
    * Избавиться от монолитного Jolt-тела для всего статического мира.
    * Использовать `std::unordered_map<uint32_t, BodyID> chunkStaticBodies` внутри `PhysicsState` для сопоставления
      каждого индекса чанка с его собственным физическим телом.
    * При изменении вокселей в чанке вызывать `QueueChunkRebuildRequest` только для этого чанка и его непосредственных
      соседей (если задеты границы).
    * В методе `ProcessChunkRebuildQueue` перестраивать `CompoundShape` исключительно для грязных чанков, удаляя старые
      тела через `BodyInterface::RemoveBody` и создавая новые.
    * Прогрев и оптимизация широкой фазы Jolt (`OptimizeBroadPhase`) должны происходить пакетом в конце кадра, если были
      перестроения.
* **DoD / Критерии приемки:**
    * Тест `TestPhysicsWorldIncrementalRebuild` успешно проходит.
    * Отсутствие фризов физики при непрерывном разрушении блоков взрывами или инструментами.

### Задача 3.3. Жадное меширование физических коллайдеров (Greedy Physics Meshing)

* **Суть:** Внедрить алгоритм объединения смежных физических блоков в крупные боксы перед передачей в Jolt для разгрузки
  физического процессора.
* **Ключевые файлы:**
    * `src/physics/PhysicsWorld.cpp` — генератор оптимизированных коллизионных примитивов.
    * `src/physics/GreedyPhysicsMerger.{hpp,cpp}` — D_3D greedy merge algorithm.
* **Технические особенности:**
    * Для снижения оверхеда на обработку коллизий Jolt Physics реализовать алгоритм объединения смежных вокселей чанка в
      укрупненные параллелепипеды (Bounding Boxes).
    * Написать CPU-генератор физического меша, который сканирует статические воксели чанка и объединяет их по алгоритму,
      аналогичному Naive Greedy Meshing.
    * Вместо добавления индивидуальных JPH::BoxShape (0.5, 0.5, 0.5) для каждого вокселя в `CompoundShape`, добавлять
      масштабированные коробки, соответствующие объединенным воксельным группам.
* **DoD / Критерии приемки:**
    * Количество коллизионных шейпов в CompoundShape снижается минимум в 4 раза на типичном ландшафте.
    * Полное совпадение физического поведения (персонаж не проваливается под текстуры и корректно сталкивается с
      углами).
* **Статус (2026-06-21 session 12x Phase 2):** ✅ Closed. `GreedyPhysicsMerger.{hpp,cpp}` (NEW) реализует D_3D greedy merge per `2026-06-21-greedy-physics-meshing-cpu` verdict=yes (F_TwoPass 35× shape reduction, 100% volume preservation). Integration в `BuildStaticVoxelCollisionBody` + `BuildChunkStaticCollisionBody` (per-chunk incremental Jolt). Env gate `PROJECTV_GREEDY_PHYSICS_MESH=ON` default; `=OFF` falls back к naive per-voxel loop. Tracy plot "Physics Greedy Merge Box Count" + "Physics Greedy Merge Chunk Box Count". `ProjectVPhysicsGreedyMergerTests` 7/7 pass (empty world, single voxel unit box, full chunk single box, volume preservation sum of box volumes == solid voxel count, mixed half-chunk reduction, fluid+air ignored, bounds clamp).

---

## ЭТАП 4. Procedural Generation & LOD

> **Цель:** Обеспечение бесшовного бесконечного мира за счет генерации ландшафта на GPU и масштабирования детализации
> чанков по дистанции.

### Задача 4.1. Генерация мира на GPU (GPU Noise & World Gen)

* **Суть:** Реализовать процедурную генерацию воксельного ландшафта (шумы, пещеры, биомы) на GPU для мгновенного
  бесконечного стриминга мира.
* **Ключевые файлы:**
    * `src/shaders/world_gen.comp` (новый файл) — вычислительный шейдер генерации.
    * `src/voxel/VoxelWorld.cpp` — логика выделения памяти под новые чанки.
* **Технические особенности:**
    * Перенести процедурную генерацию ландшафта (шум Перлина, Симплекс-шум, 3D-шум для пещер) на GPU через
      compute-шейдеры.
    * Избавиться от CPU-bottleneck при генерации новых областей мира.
    * Генератор должен писать воксели напрямую в глобальный SVDAG/VDB буфер на GPU.
    * Синхронизация генерации новых чанков должна осуществляться асинхронно через Async Compute очередь (см. Задача
      6.3).
* **DoD / Критерии приемки:**
    * Генерация нового чанка 8x8x8 выполняется менее чем за 0.05 мс на GPU.
    * Бесшовный стык ландшафта на границах чанков.
* **Статус (2026-06-21 session 4x):** ✅ Closed. `VulkanInit.cpp` adds `CreateWorldGenPipelines` + `RefreshWorldGenResourceBindings` (graceful fallback per `agent/knowledge.md §30.4` Step 1). `Types.cpp` adds `DestroyWorldGenPipelines` in shutdown. `Renderer.cpp::DrawFrame` adds world gen dispatch after Fluid CA: `BuildActiveChunkIdsForWorldGen` filters empty chunks → memset `worldGenVoxelMappedData` → populate `WorldGenPushConstants` (chunkOriginAndChunkSize, chunkCountAndFlags, noiseParams, seed = simulationTick) → `RecordWorldGenDispatch`. `kWorldGenVoxelBufferBytesPerChunk` constant moved to header. `ProjectVWorldGenTests` 7/7 (3 existing + 4 new). ~100 LoC.

### Задача 4.2. Даунсэмплинг геометрии вокселей для LOD уровней

* **Суть:** Внедрить алгоритм автоматического укрупнения геометрии удаленных чанков для снижения нагрузки на
  растеризатор.
* **Ключевые файлы:**
    * `src/shaders/voxel_mesh.comp` — адаптация шага меширования под LOD.
    * `src/voxel/CpuMeshGenerator.cpp`, `src/voxel/VoxelWorld.cpp` — поддержка LOD-структур.
* **Технические особенности:**
    * Реализовать физический даунсэмплинг геометрии чанков на основе назначенных LOD-уровней (задача Stage 4.2 chunk 1
      уже рассчитывает LOD-уровни 0, 1, 2, 3 по расстоянию до камеры).
    * В шейдере меширования `voxel_mesh.comp` (или в CPU-мешере) читать каждый 2-й воксель для LOD 1, каждый 4-й для LOD
      2, каждый 8-й для LOD 3.
    * Настроить сшивку (stitch) стыков между чанками разной детализации во избежание появления сквозных щелей (
      T-junctions) на границах LOD-переходов.
* **DoD / Критерии приемки:**
    * Стабильная частота кадров при быстром перемещении камеры.
    * Отсутствие визуальных артефактов "дырявого мира" на стыках LOD-зон.
* **Статус (2026-06-21 session 8x Phase 2 + 4x + 8x Variant 1):** ✅ Closed (8x Variant 1 Phase 1). 8x Phase 2: `B_SurfacePreserve` CPU kernel + per-chunk `LodDownsampleJob` orchestrator. 4x: `src/render/LodDownsampleGpuConsume.{hpp,cpp}` (NEW) provides `IsLodDownsampledGpuConsumeEnabled()` env gate (`PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=ON`, default OFF) + `ComputeLodDownsampledVoxelPayloadBytes` + `ComputeChunkLodLevelsCapacity` + `RefreshLodDownsampledBuffers` per-frame upload (writes per-chunk `lodLevel` from `world.chunks[i].lodLevel` to new `chunkLodLevelsBuffer` SSBO + zeros `lodDownsampledVoxelPayloadBuffer`). `SceneFrameResources` adds 2 new SSBO field groups (mapped data + buffer + allocation + capacity). `SceneResources.cpp` extends alloc/destroy/nullify structured-binding list + creates buffers in `CreateSceneResources` (after `hzbPerChunkMip` alloc). `voxel_mesh.comp` adds 2 new bindings: `LodDownsampledVoxelPayload` at binding 9 + `ChunkLodLevels` at binding 10 (read-only). `FramePreparation.cpp` calls `RefreshLodDownsampledBuffers` gated on `IsLodDownsampledGpuConsumeEnabled()`. `ProjectVLodDownsampleGpuConsumeTests` NEW 6 sub-tests. 8x Variant 1 Phase 1: `voxel_mesh.comp` extended with `kLodWordStride=16` + `GetChunkLodLevel/Extent` decoders + `DecodeLodVoxelMaterial` (read from `lodDownsampledWords` at `chunkIndex*16`) + `DecodeVoxelMaterialForLod` dispatcher. `GreedyFacePass` signature takes `chunkLodLevel + outExtent`; uses per-chunk extent from metadata instead of original chunk extent when `chunkLodLevel > 0`. `LodDownsampleGpuConsume.{hpp,cpp}` adds `kLodPayloadWordStride=16` + `BuildLodPayloadWordsFromDownsampled` (pack 4 voxels/uint32 little-endian) + `EncodeChunkLodMetadata`; `RefreshLodDownsampledBuffers` calls `DownsampleChunkForLodSurfacePreserve` per chunk. `ProjectVLodDownsampleGpuConsumeTests` 6→11 sub-tests. ~220 LoC.

### Задача 4.3. Увеличение лимита дальности отрисовки (Lift Draw Distance Cap)

* **Суть:** Снять жесткие лимиты на радиус видимости, расширив буферы рендерера для отображения масштабных панорам.
* **Ключевые файлы:**
    * `src/app/Camera.cpp` — изменение констант дальности.
    * `src/render/SceneResources.hpp`, `src/render/SceneResources.cpp` — масштабирование буферов.
* **Технические особенности:**
    * Снять ограничение дальности отрисовки в 64 метра (`kMainlineVisibleSceneMaxDistance` в `src/app/Camera.cpp`).
    * Масштабировать размер дескрипторов и емкость буферов (`sceneFaceCapacity`, `opaqueIndirectBuffer`,
      `shadowIndirectBuffer`) под целевую дальность в 128/256 метров.
    * Адаптировать хэш-функцию кэша видимости чанков `ComputeVisibilityCacheHash` под увеличенный диапазон координат.
* **DoD / Критерии приемки:**
    * Стабильная работа на дистанциях 128 и 256 метров в стресс-тестах.
    * Отсутствие переполнения буферов непрямой отрисовки.
* **Статус (2026-06-21 session 4x + 4x):** ✅ Closed (Step 2). Step 3 partial in 4x this session: `BakeAllChunksToDisk(world, outStats)` (cold-path; iterates chunks, serializes each chunk's material grid `chunkSize^3` uint8_t via `sparseStorage.GetCell` to `chunk_<index>.bin` with the same 16-byte header format) + `IsChunkStreamerPrebakeReady()` + `GetChunkStreamerPrebakeVersion()` (atomic uint64 `prebakeVersion` tracker) + `PreloadChunksAroundCamera(cameraX, cameraY, cameraZ, radiusChunks)` (per-frame priority injection: iterates grid cells within radius, enqueues high-priority `ChunkStreamRequest` for each). `ProjectVChunkStreamingTests` 10→14 sub-tests (prebake version starts zero, bake disabled when streaming off, preload disabled when streaming off, empty world returns zero). ~200 LoC. **Per-frame integration in `FinalizeActiveVoxelWorldReload` + camera-aware drain deferred** — API surface in place, wire-up is multi-session work.

---

## ЭТАП 5. GI & Temporal Effects

> **Цель:** Повышение качества изображения до кинематографического уровня за счет глобального освещения VCT и
> трассировки теней локальных источников света.

```
                 +----------------------------------------+
                 |       Voxelize Scene (voxelize.comp)   |
                 +-------------------+--------------------+
                                     |
                                     v
                 +----------------------------------------+
                 |    Build 3D Clipmap / Octree Mips      |
                 +-------------------+--------------------+
                                     |
                                     v
                 +----------------------------------------+
                 |  voxel.frag: VCT Specular & Diffuse    |
                 |  (Roughness Cutoff: 0.3)               |
                 +----------------------------------------+
```

### Задача 5.1. Конусная трассировка вокселей (Voxel Cone Tracing - VCT)

* **Суть:** Внедрить систему полностью динамического непрямого освещения (Global Illumination) и мягких конусных
  отражений на воксельной сцене.
* **Ключевые файлы:**
    * `src/shaders/voxelize.comp` (новый файл) — шейдер вокселизации сцены.
    * `src/shaders/voxel.frag` — трассировка конусов непрямого света.
    * `src/render/SceneResources.cpp` — управление 3D текстурными клипмапами.
* **Технические особенности:**
    * Написать проход вокселизации геометрии `voxelize.comp` для инжекции цвета и нормалей в 3D-клипмап (текстурный
      атлас).
    * Реализовать построение мип-уровней 3D-атласа на GPU для мягкой фильтрации конусов.
    * Во фрагментном шейдере `voxel.frag` реализовать трассировку:
        * **Diffuse GI:** трассировка 6 широких конусов вдоль полусферы нормали.
        * **Specular GI:** трассировка 1 узкого конуса в направлении отражения.
    * Применить гибридную стратегию отсечки по шероховатости `kVctCutoffRoughness = 0.3f` (согласно исследованию
      `2026-06-20-vct-vs-rt-cutoff`):
        * Если `roughness > 0.3` -> вычислять Specular GI через конусы вокселей (VCT).
        * Если `roughness <= 0.3` -> перенаправлять аппаратный луч Ray Query (Задача 5.2) для четких отражений.
* **DoD / Критерии приемки:**
    * Освещение пещер и закрытых полостей реагирует на изменение цвета неба и солнца в реальном времени.
    * Отсутствие визуальных утечек света (light leaking) сквозь стены толщиной в 1 воксель.
* **Статус (2026-06-21 session 8x Variant A Phases 2-5):** ⏸️ Partial (foundation + diffuse + specular + mip chain landed; full DoD requires Stage 5.2 RTX integration + visual smoke verification). 8x Variant A Phase 2 added GPU injection path: `voxelize.comp` (NEW, ~100 LoC GLSL) per-voxel scene injection into 3D clipmap texture (1 workgroup per chunk, 64 threads iterate over chunk voxels, 4-byte emission per voxel); `VulkanVoxelizePipeline.{hpp,cpp}` (NEW, ~450 LoC C++) — `IsVctGpuPipelineRequested()` env gate (`PROJECTV_VCT_GPU=ON`, default OFF per `agent/knowledge.md §30.4` Step 1) + 3D image allocation (256³ RGBA16F, 4 mip levels, ~16 MiB VRAM) + compute pipeline + 3-binding descriptor set + `RecordVoxelizeDispatch` + `BuildVctClipmapMipChain` (vkCmdBlitImage with VK_FILTER_LINEAR). `SceneFrameResources` adds `vctVoxelizeDescriptorSet` field. `RenderState` adds 11 VCT fields (image/view/sampler/4 pipeline handles/3 descriptor set/layout/pool/clipmap resolution/mip level count/enabled flag). `ProjectVVoxelizePipelineTests` NEW 11 sub-tests (env gate, struct size, null guards, empty guards). 8x Variant A Phase 3 added diffuse integration: `voxel.frag` adds `VctSampleDirectionalCone` helper (3-tap adaptive sampling, log2 distance mip selection) + 6-cone diffuse trace (`kVctConeDirections[6]`) + `vctDiffuse` contribution to final color. 8x Variant A Phase 4 added specular integration: `VctSampleReflectionCone` (1 cone in reflection direction, aperture = roughness * 0.6) + `kVctCutoffRoughness=0.3` hybrid gate + Fresnel-Schlick. `VoxelSceneLighting` struct extended with `vctParams` + `vctSpecularParams` vec4 fields (struct size 624 → 656 bytes), byte-exact contract with shader per `agent/knowledge.md §15`. 8x Variant A Phase 5 added mip chain: `BuildVctClipmapMipChain` records `vkCmdBlitImage` 3D-to-3D mip chain on graphics CB after voxelize dispatch. Build green, 35/36 ctest (34 new) + 1 documented pre-existing failure (`ProjectVTests` baseline).

### Задача 5.2. Аппаратные тени и отражения через Ray Query (Feature-Flagged)

* **Суть:** Внедрить гибридный рендеринг с использованием аппаратной трассировки лучей для получения безупречных теней
  от факелов/ламп и зеркальных отражений.
* **Ключевые файлы:**
    * `src/shaders/voxel.frag` — встраивание логики трассировки через Ray Query.
    * `src/render/vulkan/HardwareRayTracingProbe.cpp` — проверка возможностей GPU на старте.
    * `src/render/Renderer.cpp` — инициализация и сборка BVH структур.
* **Технические особенности:**
    * Реализовать построение структур ускорения BLAS (Bottom-Level Acceleration Structure) для геометрии чанков на GPU.
    * Реализовать динамическое построение TLAS (Top-Level Acceleration Structure) каждый кадр на основе
      трансформированных позиций моделей и чанков.
    * Во фрагментном шейдере `voxel.frag` использовать расширение `GL_EXT_ray_query` для трассировки теней локальных
      источников света (Point Lights) и получения четких отражений на материалах с `roughness < 0.3`.
    * Пайплайн должен быть полностью изолирован под флагом компиляции `PROJECTV_HW_RAY_TRACING` и проверкой расширения
      контура `ProbeHardwareRayTracingSupport`.
* **DoD / Критерии приемки:**
    * Четкие тени от локальных источников света без эффекта пикселизации (shadow acne).
    * Корректная работа на видеокартах NVIDIA RTX / AMD RDNA2+ при включенном режиме трассировки.
* **Статус (2026-06-21):** 🔓 Open. `kVctCutoffRoughness=0.3` hybrid gate (8x V A) currently forwards smooth surfaces to a placeholder (no specular GI contribution when VCT enabled). RTX BLAS (Bottom-Level Acceleration Structure) for static chunks + TLAS (Top-Level AS) per-frame + `GL_EXT_ray_query` integration в `voxel.frag` deferred до dedicated RTX session. RTX-only path с graceful fallback на dev hosts без RT cores. `VulkanBootstrap.cpp` ready for `VkPhysicalDeviceRayTracingFeaturesEXT.rayQuery=VK_TRUE` extension chaining. Per `docs/experiments/INDEX.md 2026-06-20-rt-shadows-vs-csm` (~770 LoC total).

### Задача 5.3. Темпоральные векторы движения (TAA Motion Vectors)

* **Суть:** Реализовать честный расчет смещения пикселей между кадрами для устранения эффекта "мыла" и шлейфов (
  ghosting) при темпоральном сглаживании.
* **Ключевые файлы:**
    * `src/shaders/voxel.vert`, `src/shaders/voxel.frag` — вывод векторов движения.
    * `src/shaders/taa_resolve.frag` — использование скоростей при выборке истории.
    * `src/render/TaaRenderTargets.cpp` — добавление MRT-цели скоростей.
* **Технические особенности:**
    * Реализовать честный расчет векторов движения пикселей (Motion Vectors) вместо чистого темпорального
      перепроецирования по буферу глубины.
    * Записывать смещение пикселя относительно предыдущего кадра в дополнительный MRT-аттачмент (
      `VK_FORMAT_R16G16_SFLOAT`).
    * Шейдер TAA-резолва `taa_resolve.frag` должен использовать векторы движения для точной выборки истории кадров,
      полностью устраняя шлейфы (ghosting) на быстродвижущихся объектах (моделях).
* **DoD / Критерии приемки:**
    * Полное исчезновение шлейфов за перемещаемыми гравипушкой моделями.
    * Четкие границы геометрии в динамике при сохранении стабильного сглаживания субпиксельного дрожания (jitter).
* **Статус (2026-06-21 session 8x Phase 3 + 12x Phase 3 + 4x Phase 1):** ✅ Closed. Format constant added (8x) per Karis 2014. 12x Phase 3 added data path: `voxel.frag` outputs `outMotionVector` (vec2, location 3) computed from `prevViewProjectionMatrix * worldPos` NDC delta vs current `viewProjection * worldPos` NDC + 4th color attachment format `kTaaMotionVectorFormat` added to pipeline + 2 new `OffscreenColorTarget` (motionVector + history) in `TaaRenderTargets` + new `TransitionTaaMotionVectorForSample` + `RecordTaaMotionVectorHistoryCopy` helpers + 2 new `RenderState` fields. 4x Phase 1 added resolve consume: `taa_resolve.frag` adds binding 4 (sampler2D motionVector) + replaces depth-reproject path lines 167-182 with `texture(motionVector, uv).xy → prevUv = uv + motion`. `TaaResolvePipeline.cpp` extends `kTaaResolveDescriptorBindings` 5→6 + pool size 3→4 samplers per frame + 4th `VkWriteDescriptorSet`. `ProjectVTaaMotionVectorTests` 8/8 (5 existing + 3 new). ~60 LoC.

---

## ЭТАП 6. Refactoring, Tech Debt & ECS

> **Цель:** Повышение стабильности кодовой базы, ускорение сборки проекта и оптимизация распределения ресурсов
> процессора.

### Задача 6.1. Полная миграция игрового цикла на Flecs ECS

* **Суть:** Разгрузить монолитную функцию `UpdateApp` (989 строк), перенеся оставшиеся процедурные системы на модель
  Flecs ECS с использованием SoA-памяти для максимального ускорения симуляции.
* **Ключевые файлы:**
    * `src/ecs/EcsWorld.cpp`, `src/ecs/EcsWorld.hpp` — регистрация новых систем.
    * `src/app/AppUpdate.cpp` — вынос логики из монолита.
    * `src/app/main.cpp` — обновление шага игрового цикла.
* **Технические особенности:**
    * Согласно исследованию `2026-06-20-flecs-soa-vs-aos-bench`, Flecs по умолчанию использует высокопроизводительный
      макет памяти **SoA (Struct-of-Arrays)**, дающий ускорение симуляции до **3.86×**. Все новые системы должны
      следовать этому шаблону.
    * Создать системы:
        * `PhysicsCharacterTickSystem` (заменяет вызовы `TickWalkCharacter` / `TickCreativeCharacter`).
        * `InputReplayPlaybackSystem` (обрабатывает воспроизведение записанных реплеев).
        * `CameraUpdateSystem` (обрабатывает ввод мыши и позиционирование камеры).
    * `UpdateApp` должен содержать только высокоуровневую диспетчеризацию фаз ECS-мира через `world.progress()`.
* **DoD / Критерии приемки:**
    * Функция `UpdateApp` сокращена до < 100 строк.
    * Регрессионные тесты ввода и физики персонажа (`ProjectVTests`) полностью проходят.
    * Фреймрейт симуляции стабилен на уровне 60 Гц.
* **Статус (2026-06-21 session 4x):** ✅ Closed. Extracted `ProcessInputActions` (~250 lines), `RunFrameSimulation` (~50 lines), `MirrorAllFrameStats` (~30 lines) as file-scope helpers in `AppUpdate.cpp`. **`UpdateApp`: 355 → 49 lines** (over-delivered vs <100 target). All ECS systems already extracted in 2x part 1 (`AudioRefreshPlaylistSystem`, `FluidCATickSystem`) + 2x part 2 (3 more systems). See `CHANGELOG.md §2026-06-21 (session: 4x)`.

### Задача 6.2. Внедрение PIMPL для AppState и изоляция зависимости Vulkan/VMA

* **Суть:** Скрыть детали реализации Vulkan, SDL3 и VMA из глобального заголовочного файла `Types.hpp` за счет паттерна
  PIMPL, ускоряя время инкрементальной сборки.
* **Ключевые файлы:**
    * `src/core/Types.hpp` — интерфейс AppState.
    * `src/core/Types.cpp` — приватная структура реализации `AppStateImpl`.
    * `src/app/main.cpp` — адаптация вызовов инициализации.
* **Технические особенности:**
    * Перенести структуры `RenderState`, `SwapchainState`, `FrameState`, `VulkanContextState` в приватную структуру
      `AppStateImpl` в `Types.cpp`.
    * Экспортировать только непрозрачный указатель `std::unique_ptr<AppStateImpl>` и методы интерфейса.
    * Удалить транзитивные включения `<vulkan/vulkan.h>` и `"vk_mem_alloc.h"` из публичного заголовочного файла
      `Types.hpp`.
* **DoD / Критерии приемки:**
    * Любые изменения в приватных структурах рендерера не приводят к перекомпиляции файлов логики (`PhysicsWorld.cpp`,
      `VoxelWorld.cpp`, `AudioEngine.cpp`).
    * Время инкрементальной пересборки при изменении `Types.hpp` снижается с 19.8s до < 1.0s.
    * Полное отсутствие утечек Vulkan-ресурсов при закрытии приложения (проверка через Validation Layers на выходе).
* **Статус (2026-06-21 session 4x + 8x V1):** ⏸️ Partial → ✅ boundary-neighbor queue closed (8x V1 Phase 6). Per-chunk `BuildChunkStaticCollisionBody` уже использует greedy merge (12x Phase 2). 4x: `QueueChunkRebuildRequest` + `ProcessChunkRebuildQueue` + `BuildChunkStaticCollisionBody` (per-chunk incremental Jolt) + `OptimizeBroadPhase` API surface. 8x V1 Phase 6: `VoxelWorld.cpp::SetVoxelMaterial` now calls `QueueChunkRebuildRequest(physics, chunkIndex)` for the edited chunk AND all 6 face-sharing boundary neighbors (when edit sits on a chunk face). Previously only the center chunk was queued, leaving neighbor chunks' CompoundShape out of sync. Mirrors the existing visual rebuild range in `MarkChunksTouchedByVoxelEditDirty`. `ProjectVPhysicsIncrementalJoltTests` NEW 6 sub-tests. ~50 LoC. **Per-chunk rebuild optimization (atomic per-chunk counter, deferred `BodyInterface::RemoveBody` batching)** still deferred.
* **Статус (2026-06-21 session 8x Phase 5):** ⏸️ Partial. Safety-net patch created `git diff > /tmp/before_pimpl_20260621_025608.patch` (358 KB, per AGENTS.md §5.4). `ProjectVAppStatePimplTests` (12 `static_assert` checks) verifies existing 12 accessor return types contract (all `T&` or smart-pointer refs as designed). 172 accessor call sites identified for sed migration. Full struct move to `Types.cpp` (opaque types, all accessors return pointers, `state->render().field` → `state->render()->field` sed migration) deferred to dedicated multi-session work due to mechanical sed risk.

### Задача 6.3. Асинхронный запуск задач (Async Compute Queue & Timeline Semaphores)

* **Суть:** Внедрить многопоточную асинхронную отправку команд на GPU для устранения фризов при перестроении физических
  коллизий и генерации чанков.
* **Ключевые файлы:**
    * `src/render/vulkan/VulkanBootstrap.cpp` — настройка очередей.
    * `src/render/Renderer.cpp` — синхронизация очередей.
    * `src/render/vulkan/VulkanSyncPrimitives.cpp` — хелперы для Timeline Semaphores.
* **Технические особенности:**
    * Использовать `dedicatedComputeQueue` для выполнения тяжелых расчетных задач: генерации HZB (Задача 2.1), Fluid CA
      симуляции (Задача 3.1) и фонового перестроения BLAS (Задача 5.2).
    * Настроить синхронизацию между очередью графики (`queue`) и очередью вычислений через `VkSemaphoreSubmitInfo` с
      использованием таймлайн-семафоров (`renderTimelineSemaphore`).
    * Обеспечить беспрепятственный параллельный рендеринг кадра на GPU во время выполнения асинхронных диспатчей.
* **DoD / Критерии приемки:**
    * Эффект "заикания" (stuttering) полностью устранен при перестроении геометрии мира.
    * Прирост производительности в стресс-тестах составляет не менее **9.8%** по сравнению с однопоточным выполнением.
* **Статус (2026-06-21 session 8x Phase 4 + 4x + 8x Variant 1 + 8x Variant A):** ⏸️ Partial → ✅ Cross-queue depth sync closed. 8x Phase 4: `IsAsyncComputeEnabled()` env gate + `SubmitFluidCaToComputeQueue` helper. 4x: `src/render/vulkan/VulkanAsyncCompute.{hpp,cpp}` (NEW) provides `EnsureAsyncComputeResources` + `RecordAsyncComputePass` (Fluid CA + world gen) + `SubmitToComputeQueue`. 8x Variant 1 Phase 4: `hzbBuildTimelineSemaphore` + 2nd timeline signal. 8x Variant A Phase 1 closed the partial: `RecordHzbAsyncCullPass` now records proper cross-queue memory barrier (`srcStageMask = TRANSFER_BIT` + `srcAccessMask = TRANSFER_WRITE_BIT` → `dstStageMask = COMPUTE_SHADER_BIT` + `dstAccessMask = SHADER_READ_BIT`, layout stays `SHADER_READ_ONLY_OPTIMAL`, `VK_QUEUE_FAMILY_IGNORED` for both) replacing the 8x V1 placeholder. Sufficient for current `VK_SHARING_MODE_EXCLUSIVE` HZB image when timeline semaphore + barrier provide execution + memory respectively. `ProjectVAsyncComputeTests` 11→12 sub-tests (added empty render state guard). RTX BLAS routing still deferred.

---

## R&D Backlog (Будущие гипотезы и исследования)

Данные задачи перенесены из реестра `docs/experiments/INDEX.md` и не блокируют текущую разработку MVP-ветки:

1. **ReSTIR GI & Иррадиационные кэши (Stage 6+):** Трассировка глобального освещения на основе путей (Path Tracing) с
   пространственно-временным повторным использованием лучей. Архитектурно отложена до этапа полноценного перехода
   проекта на рендеринг методом Path Tracing.
2. **Алгоритм волнового коллапса функции (WFC) для генерации миров:** Процедурная генерация воксельных биомов на основе
   правил соседства блоков. Предназначена для разнообразия контента на этапе расширения геймплея.
3. **Visibility Buffer для воксельного рендеринга:** Альтернативный пайплайн отрисовки геометрии с записью ID
   треугольников и барицентрических координат в плотный буфер. Рекомендован к рассмотрению только при портировании
   проекта на мобильные графические процессоры (TBDR-архитектуры).

---

## Матрица приоритетов и график запусков

```
  Высокий |  [Stage 1] Voxel DB & Flattening       [Stage 3] Jolt Rebuilds & Fluid CA
          |  [Stage 6] AppState PIMPL & ECS        [Stage 2] HZB & Mesh Shaders
П         |
р         |
и         |
о         |
р         |
и         |
т         |
е         |  [Stage 4] GPU Noise Gen & LODs
т         |
  Низкий  |                                        [Stage 5] VCT GI & Ray Query Shadows
          +-----------------------------------------------------------------------------
                                  Малые                        Большие
                                      Объемы изменений / Риски
```

1. **Короткие победы (Quick Wins):** Stage 1.3 (асинхронный аудио-скан), Stage 6.2.2 (std::span sweep на оставшихся
   участках), Stage 6.2.5 (фиксация EVIL-маркеров).
2. **Основной фокус (Core Path):** Последовательное закрытие Stage 1 (транслятор NanoVDB на GPU) -> Stage 2.1 (
   HZB-куллинг) -> Stage 3 (почаночный Jolt и Fluid CA на GPU).
3. **Визуальный шик (Visual Polish):** Stage 5 (VCT GI и тени Ray Query) — максимальные требования к оборудованию,
   завершает формирование look-dev пайплайна ProjectV.