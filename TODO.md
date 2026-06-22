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
- RTX shadows foundation (BLAS dispatch + AABB geometry + rayQueryEXT shaders) — TLAS population in flight (5.2.A/B/C/D milestones)

**Стратегический фокус** (2026-06-22+): **RTX-only path forward**. Hardware target = NVIDIA RTX
20/30/40/50 series (Turing RT cores или новее). Никаких non-RTX fallback, никаких уступок legacy.
Стратегический разворот: CSM полностью удаляется (Milestone 5.2.D), VCT diffuse заменяется на DDGI
probes (Milestone 5.5), остальное освещение мигрирует на RTX ray queries (Milestones 5.4, 5.6, 5.7).
Причина: CSM bias tuning зашёл в тупик (Peter Panning не решается), RTX даёт ground-truth тени
+ AO + GI + specular + refraction на dedicated RT cores с лучшей производительностью чем CSM +
DDA + VCT в сумме.

**Активные TODO**: см. секцию ниже. История закрытых задач — `CHANGELOG.md` + `agent/workspace.md`.

---

## Active tasks (2026-06-22)

**Сводка:** 0 ⏸ Paused · 2 🔒 Deferred (pending feasibility) · 4 ⭐ Next-priority (7.x post-RTX) ·
   🔓 1 RTX-only milestone cascade (5.2.D + 5.4, 5.5, 5.6, 5.7). **2026-06-22 update:** 5.2.A, 5.2.B,
   5.2.C closed in session 19x. RTX shadows = default path. Remaining cascade: CSM removal (5.2.D) + RTX AO + DDGI + refraction + multi-bounce GI.

### Next-priority (7.x post-RTX-shadow milestones — опциональные polish)

Эти задачи не блокируют core RTX-driven rendering path (5.2/5.4-5.7). Имеют смысл после того, как
Milestone 5.2.D (полное удаление CSM) закроется и RTX shadows proven. Порядок: 7.2 (TAA) → 7.3
(tone mapping) → 7.4 (post-FX) → 7.1 (VCT cones, может быть заменён DDGI в Milestone 5.5).

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

#### 🔒 Задача 7.2. CSM cascade quality pass — REMOVED 2026-06-22

**Причина удаления** (operator decision `2026-06-22`): CSM полностью заменяется RTX shadows
(см. Задачу 5.2 + её milestones A/B/C/D). Улучшать каскадное качество legacy path, который
уходит — пустая трата. Pet-проект = pet-проект, нет нужды в non-RTX fallback для legacy hardware.
Все sub-tasks (4→6 cascades, blend zones, dithering, PSSM lambda tuning) — moot.

**Если когда-нибудь появится non-RTX target** (Steam Deck без RT, mobile port, web port) —
можно reopen, но в текущем scope не преследуется. RTX-only = RTX-only.

#### ⭐ Задача 7.2. TAA jitter + neighborhood quality — Open

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

#### ⭐ Задача 7.3. Lighting exposure + tone mapping pass — Open

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

#### ⭐ Задача 7.4. Post-processing chain polish — Open

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

### 🔓 Задача 5.2. RTX shadows + smooth specular GI — RESUMED 2026-06-22

**Strategic pivot 2026-06-22** (operator decision, RTX-only): CSM bias tuning упёрся в Peter
Panning, который не удаётся устранить bias adjustments на текущей VoxelLab сцене. Решение:
**полностью удалить CSM и заменить на RTX shadows**. Это pet-project, нет нужды в legacy fallback
для non-RTX GPU. **Hardware target = NVIDIA RTX 20/30/40/50 series** (Turing RT cores или новее).
Никаких уступок в сторону legacy — «всё новое и современное». Dedicated RT cores дают ground-truth
тени без всех CSM-артефактов (Peter Panning, acne, PCF tuning, cascade bleeding). См. milestones
A/B/C/D ниже.

**Background (откуда этот разворот):**
- VoxelLab peter-panning не починился reduction of `receiverLightBias` floor (`max(normalBias*0.5,
  depthBias*4.0)` → `max(normalBias*0.2, depthBias*2.0)`) — слишком маленькая компонента
  относительно доминирующего `receiverDepthBias`.
- Доминирующий источник Peter Panning — `receiverDepthBias` formula direction (anti-shadow), см.
  `agent/knowledge.md §15` lines 1318-1319 + handoff `/tmp/handoff-shadow-peter-panning-fix.md`.
- Несколько предыдущих агентов пытались чинить — не получилось. Стратегический разворот на RTX —
  более чистое решение, чем продолжать крутить bias coefficients. Плюс полное удаление CSM
  экономит ~1300 LoC и убирает целый класс артефактов.
- RTX performance: 1080p × 120 FPS = 248 MRays/sec shadow rays на RTX 3060 Ti, что ~0.65% от
  peak RT capacity (38 cores × ~10 GRays/sec). RT cores скучают на CSM (5×5 PCF + 4 cascades).
  Полная RTX-driven сцена с shadows + AO + GI = ~18 rays/pixel = ~5-15% utilization — здоровый
  budget без потери FPS.

**Что сделано** (commits `6018c27`, `47ce703`, `285ce79`, `5390dab`, `c528396`, `18x+`):
- BLAS dispatch wired в `BuildDirtyBlases` через one-shot cmd buffer + fence
- AABB geometry per-chunk (`VK_GEOMETRY_TYPE_AABBS_KHR` + `vkCmdUpdateBuffer` + TRANSFER→AS_BUILD barrier)
- `DirtyChunkRebuild { chunkIndex, aabb }` queue
- Shader variants `voxel.frag.rtx.spv` + `voxel.frag.rtx_taa_on.spv` с `rayQueryEXT` + `accelerationStructureEXT rtxTlas` (binding 13)
- `TraceRtxSmoothSpecularRay` для `roughness ≤ kVctCutoffRoughness=0.3`
- `graphicsPipelineRtx` + `graphicsPipelineRtxTaaOn` pipelines
- `ProjectVRayTracedShadowTests` 11/11 sub-tests
- `ProbeHardwareRayTracingSupport` ловит acceleration structure / ray query / deferred host ops / buffer device address caps
- 18x+ fix: device creation pNext chain construction

**Что осталось** (см. milestones ниже — A/B/C/D + последующие 5.4-5.6 для полной RTX-driven освещения).

#### ✅ Milestone 5.2.A. TLAS реально собирается — Closed (2026-06-22, session 19x)

**Цель:** `RayTracedShadows::RecordTlasBuild` перестаёт быть stub'ом, TLAS handle создаётся,
`vkCmdBuildAccelerationStructuresKHR` для instances geometry диспатчится. `UpdateTlas` правильно
проставляет `accelerationStructureReference` через `vkGetAccelerationStructureDeviceAddressKHR`.

**Конкретно:**
- Per-chunk BLAS handle storage (`VkAccelerationStructureKHR[]` + `VkBuffer[]` для storage buffer'ов,
  либо `vkCreateAccelerationStructureKHR` с `VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR`
  плюс backing buffer per BLAS).
- `RayTracedShadows::BuildDirtyBlases` после per-chunk BLAS build кэширует device address через
  `vkGetAccelerationStructureDeviceAddressKHR` в slot для chunkIndex.
- `RayTracedShadows::UpdateTlas`: `instances[i].accelerationStructureReference` теперь = real
  BLAS device address (строка 433 в `RayTracedShadows.cpp` сейчас пишет `0u`).
- `RayTracedShadows::RecordTlasBuild`: реальный `vkCreateAccelerationStructureKHR` +
  `vkCmdBuildAccelerationStructuresKHR` с `VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR`,
  `VK_GEOMETRY_TYPE_INSTANCES_KHR`, `pGeometries.instances.data.deviceAddress =
  m_config.tlasInstanceDeviceAddress`. Barrier AS_BUILD → fragment shader (для `rayQueryEXT`
  чтения TLAS).
- `RayTracedShadows::RecordRayTracedShadowPass`: пока остаётся stub (ray query dispatch — это
  Milestone B). Только инкрементит счётчик + barrier.
- На RTX-capable GPU, `PROJECTV_HW_RAY_TRACING=ON bin/ProjectV`: smoke log показывает
  `RayTracedShadows: instances=N blasRebuilds=M tlasRebuilds=K` где все три счётчика ненулевые.

**Ключевые файлы:**
- `src/render/RayTracedShadows.cpp:415-449` — `UpdateTlas` fix
- `src/render/RayTracedShadows.cpp:451-463` — `RecordTlasBuild` real impl
- `src/render/RayTracedShadows.cpp:327-413` — `BuildChunkBlas` (уже работает, надо дополнить
  cache'ом device address)
- `src/render/RayTracedShadows.hpp:15-40` — `RayTracedShadowConfig` — добавить `std::vector<VkAccelerationStructureKHR> blasHandles`,
  `std::vector<VkBuffer> blasStorageBuffers`, `std::vector<VkDeviceAddress> blasDeviceAddresses`

**DoD:**
- `PROJECTV_HW_RAY_TRACING=ON bin/ProjectV` → smoke log `RayTracedShadows: instances=120+
  blasRebuilds=120+ tlasRebuilds=N` где N растёт с числом кадров (на VoxelLab ~120 visible chunks)
- `vulkaninfo`-based validation: TLAS handle non-null, `vkGetAccelerationStructureDeviceAddressKHR`
  возвращает non-zero
- `ctest` regression: 38/38 pass + `ProjectVRayTracedShadowTests` 11/11 pass
- 0 Vulkan validation errors в console
- **НЕ требуется:** визуальное подтверждение теней (это Milestone B — там ray query dispatch)

#### ✅ Milestone 5.2.B. Ray-traced shadow visibility в шейдере — Closed (2026-06-22, session 19x)

**Цель:** `voxel.frag::ComputeSunShadowSample` ветвится на RTX path: `if (rtxEnabled) traceShadowRay();
else csmShadow()`. Ray query против TLAS даёт ground-truth shadow visibility (lit/in-shadow),
без CSM-артефактов.

**Конкретно:**
- Новая GLSL функция `TraceRtxSunShadowRay(origin, dir, maxDistance) → float` (1.0 = lit, 0.0 = in shadow).
  Использует `rayQueryEXT` против `rtxTlas` (binding 13 уже есть в шейдере для specular).
- Compile нового shader варианта `voxel.frag.ray_shadow.spv` + `voxel.frag.ray_shadow_taa_on.spv`
  через `glslangValidator` (см. `src/CMakeLists.txt:31-100` как templates).
- `ComputeSunShadowSample` ранний return для RTX path:
  ```glsl
  if (rtxEnabled) {
      return vec4(TraceRtxSunShadowRay(worldPosition, normalize(sceneLighting.sunDirectionAndWrap.xyz), 256.0), 0.0, 0.0, 1.0);
  }
  ```
- PCF заменяется на single-hit ray query (ray tracing не нуждается в PCF — anti-aliasing через
  TAA + temporal accumulation).
- Bias полностью убирается: ray query T_min = 0.001 (offset along ray direction to avoid self-hit),
  T_max = 256.0 (или scene bounding box diagonal). Никаких `receiverNormalBias`, `receiverLightBias`,
  `receiverDepthBias` для RTX path.

**Ключевые файлы:**
- `src/shaders/voxel.frag:776-832` — `ComputeSunShadowSample` split
- `src/shaders/voxel.frag:77-86` — `TraceRtxSmoothSpecularRay` (template для новой shadow ray)
- `src/CMakeLists.txt:60-100` — добавить `voxel.frag.ray_shadow` + `voxel.frag.ray_shadow_taa_on`
  в glslangValidator custom command

**DoD:**
- `PROJECTV_HW_RAY_TRACING=ON bin/ProjectV` + VoxelLab scene → визуально тени **привязаны к вокселям**,
  нет Peter Panning, нет acne, нет необходимости в cascade tuning.
- 60+ FPS на VoxelLab 1920×1080 (RTX 3060 Ti, ray query cost estimate: ~0.5-1ms per frame)
- `ctest` regression: 38/38 pass + 11/11 RTX tests
- Side-by-side smoke: `PROJECTV_HW_RAY_TRACING=OFF` vs `ON` показывает визуальную разницу

#### ✅ Milestone 5.2.C. RTX shadows = default на RTX-capable железе — Closed (2026-06-22, session 19x)

**Цель:** `PROJECTV_HW_RAY_TRACING` env gate убирается. Если GPU RTX-capable (RTX 20/30/40/50),
RTX shadows включаются автоматически — никаких env vars. Если GPU НЕ RTX-capable — hard fail
(не запускается), а не fallback на CSM. Это pet-project, никаких уступок legacy.

**Конкретно:**
- `IsRayTracedShadowEnabled()` (`:24-31` в `RayTracedShadows.cpp`) переделать: возвращать
  `context.rayTracing.accelerationStructure && context.rayTracing.rayQuery` (auto-detect).
  Env gate **полностью убирается** — больше никаких `PROJECTV_HW_RAY_TRACING=ON/OFF`.
- Если `context.rayTracing` пустой (non-RTX GPU) → engine **отказывается стартовать** с сообщением
  "RTX-capable GPU required (NVIDIA RTX 20 series or newer with RT cores)". Никаких fallback path.
- Дев-хост (RTX 3060 Ti): `bin/ProjectV` запускается сразу с RTX shadows.
- VoxelLab baseline shadow визуально улучшается из коробки (без Peter Panning).

**Ключевые файлы:**
- `src/render/RayTracedShadows.cpp:24-31` — `IsRayTracedShadowEnabled` auto-detect, env gate удалить
- `src/render/RayTracedShadows.cpp:Initialize` — отказ при non-RTX GPU вместо silent fallback
- `src/render/Renderer.cpp` — wire auto-detected RTX path
- `src/app/main.cpp` или где проверяется GPU support — добавить hard fail для non-RTX
- `agent/knowledge.md §15` — обновить запись 1318 (peter-panning fix → неактуальна, RTX shadows
  делают её moot)

**Hardware target policy (новый):**
- **Minimum:** NVIDIA RTX 2060 (Turing, первые RT cores, generation 2019)
- **Recommended:** NVIDIA RTX 3060 Ti / 3070 / 3080 / 4070+ (Ampere/Ada, 2nd/3rd gen RT cores)
- **Unsupported:** anything without dedicated RT cores (GTX 10xx/16xx, AMD pre-RDNA3, Intel Arc A-series
  без decent RT perf). Hard fail с чётким error message.

**DoD:**
- Дефолтный запуск `bin/ProjectV` на RTX-capable GPU → RTX shadows active, без env vars, без
  `#ifdef LEGACY_CSM`.
- Скриншот из user-facing demo: тени без Peter Panning, FPS ≥ 120 на RTX 3060 Ti.
- Non-RTX GPU (или VM без GPU passthrough) → engine refuses to start с понятным error message,
  никакого partial functionality, никакого CSM fallback.
- Документация обновлена: README.md «Hardware requirements», `agent/knowledge.md` policy section.

#### ⭐ Milestone 5.2.D. Полное удаление CSM — Open (после 5.2.C proven)

**Цель:** CSM код выпиливается из проекта целиком. Никаких #ifdef LEGACY_CSM, никаких fallback paths.
RTX shadows — единственный shadow path. Минус ~1300 LoC legacy кода.

**Конкретно (полный список удаляемого):**
- `src/render/ShadowProjection.hpp` + `src/render/ShadowProjection.cpp` (~673 строки) — целиком
- `src/shaders/voxel_shadow.vert` + `src/shaders/voxel_shadow.frag` (~154 строки) — целиком,
  если не переиспользуется для other shadow types (point lights, area lights)
- `src/shaders/voxel.frag` — удалить функции:
  - `ComputeSunShadowSample` (~50 строк)
  - `SampleSunShadowCascade` (~40 строк)
  - `SelectSunShadowCascadeByViewDepth` (~10 строк)
  - `GetSunShadowCascadeNearDepth` (~10 строк)
  - `ComputeSunShadowCascadeBlendWeight` (~15 строк)
  - `GetSunShadowCascadeDebugColor` (~10 строк)
- `src/voxel/VoxelMaterials.cpp` — удалить `kMaxShadowDepthBias`, `kMaxShadowNormalBias`,
  `kMaxShadowFilterRadius` constants; удалить baked `sunShadowParams` shadow bias entries
- `src/voxel/VoxelMaterials.hpp` — удалить поля `VoxelSceneLighting::sunShadowParams`,
  `sunShadowViewProjections[4]`, `shadowCascadeDepthSplits`, `shadowCascadeBlendParams`
- `src/render/SceneResources.cpp` — удалить `RefreshSunShadowProjections`, `StoreSunShadowProjection`,
  `StoreSunShadowCascadeProjections`, `StoreSunShadowCascadeSplits`,
  `BuildSunShadowCascadeProjectionInputs`
- `src/render/Renderer.cpp` — убрать CSM shadow pass dispatch, оставить только RTX
- `src/render/ScreenshotCapture.cpp` — убрать shadow cascade metadata из sidecar
- `src/render/ShadowProjection.hpp` — удалить `BuildSunShadowProjection` и
  `BuildSunShadowCascadeProjections` (не используется после миграции)
- `src/debug/DebugHud.cpp` — убрать `CSM` debug view, `sunShadowCoverageScale` statistic
- `src/app/AppUpdate.cpp` — убрать keyboard ladder controls F11 (shadow strength),
  F12 (delay), 6 (shadow detail)
- `src/core/Types.hpp` — удалить `VoxelLightingDebugControls::shadowDepthBiasOffset` +
  `shadowNormalBiasOffset` + `shadowFilterRadiusOffset` + `shadowStrengthOffset` +
  `shadowCoverageScale` + `shadowCascadeBlendOffset` + `sunShadowCoverageScale` fields
- `tests/SunShadowCascadeSplitsTests.cpp` (~161 строка) — целиком удалить
- `tests/CMakeLists.txt` — убрать `ProjectVSunShadowCascadeSplitsTests` (5 preset occurrences)
- Шейдерные defines (если есть) `PROJECTV_CSM_FALLBACK` — не создаём
- `LightingDebugView::Cascade` enum value — удалить

**Что НЕ удалять (даже в этом milestone):**
- VCT (Voxel Cone Tracing) — пока остаётся как GI path. Заменится на DDGI в Milestone 5.5.
- CSM math может переиспользоваться для non-sun lights (point lights с малым radius) — оставить
  как building block, переименовать в `BuildLocalShadowProjection` или подобное.
- Voxel shadow caster shader (`voxel_shadow.vert`) — может пригодиться для area light shadows.

**DoD:**
- `grep -r "BuildSunShadow\|SampleSunShadow\|sunShadowParams\|shadowCascadeDepthSplits" src/`
  → пусто (или только в COMMENTS.md / agent/knowledge.md исторические ссылки)
- `git grep "CSM" src/ tests/ CMakePresets.json` → пусто
- `cmake --build build/linux-clang-debug --target ProjectV --parallel 8` → green, 0 warnings
- `ctest` → 38/38 pass (без `ProjectVSunShadowCascadeSplitsTests`), `ProjectVRayTracedShadowTests`
  11/11 pass
- `PROJECTV_HW_RAY_TRACING=ON bin/ProjectV` → VoxelLab работает на RTX shadows, без CSM fallback,
  никаких `#ifdef` в коде
- Документация (CHANGELOG, COMMENTS, agent/knowledge.md §15, agent/workspace.md) описывает
  полное удаление CSM и почему это правильное решение
- Сравнительный smoke `PROJECTV_HW_RAY_TRACING=ON` vs pre-5.2.D — визуально идентично
  (RTX shadows заменили CSM полностью)

---

## RTX-Driven Lighting (после Milestone 5.2.D, sequential)

После того как RTX shadows proven и CSM удалён, остальное освещение тоже мигрирует на RTX ray queries.
Цель — максимально загрузить RT cores (вместо ~0.05% utilization при shadow-only), при этом сохранить
120+ FPS. По [NVIDIA RTX GI benchmarks](https://developer.nvidia.com/blog/rtx-global-illumination-part-i/):
полная RTX-driven сцена (shadows + AO + diffuse GI + specular) = 3 ms/frame на RTX 2080 Ti = 90 FPS,
на RTX 3060 Ti реалистично 120+ FPS.

**Общий ray budget (per pixel, VoxelLab 1080p × 120 FPS = 248 MRays/sec):**
- Sun shadow: 1 ray
- Contact shadow: 1 ray
- Local light shadow: 5 rays
- AO: 3 rays
- DDGI diffuse: 6 rays (probes shared across frames, ~0.1 rays/pixel effective)
- Specular reflection: 1 ray (smooth surfaces only, ray-guarded by roughness > 0.3)
- Refraction: 1 ray (transparent materials only)
- Total worst-case: ~18 rays/pixel = ~5-15% RT core utilization

#### ⭐ Milestone 5.4. RTX ambient occlusion (заменяет DDA) — Open

**Цель:** `ComputeAmbientOcclusionVisibility` в `voxel.frag` использует ray query вместо DDA voxel
traversal. BVH culling на AABB BLAS даёт дешевле для sparse scenes.

**Конкретно:**
- Новая GLSL функция `TraceRtxAmbientOcclusionRay(origin, direction, maxDistance)` через rayQueryEXT.
- Заменить `TraceAmbientOcclusionRay` в `voxel.frag:389-411` (DDA) на ray query.
- Удалить DDA helper `ComputeRayStepTMax` если больше не нужен.
- AO cone count остаётся 3 (normal + 2 side) — RTX обрабатывает их параллельно в hardware.

**Ключевые файлы:**
- `src/shaders/voxel.frag:438-465` — `ComputeAmbientOcclusionVisibility` switch на RTX
- `src/shaders/voxel.frag:77-86` — `TraceRtxSmoothSpecularRay` (template)

**DoD:**
- VoxelLab AO выглядит более consistent (no banding artifacts на voxel edges от DDA quantization)
- AO pass cost: <0.3 ms/frame (3 rays/pixel на 1080p)
- ctest regression green

#### ⭐ Milestone 5.5. DDGI probes (заменяет VCT diffuse GI) — Open

**Цель:** Заменить VCT (Voxel Cone Tracing) clipmap на DDGI (Dynamic Diffuse Global Illumination)
probes. RTX rays per probe обновляются каждый N кадров. Trilinear interpolation between probes at
receiver. Это RTX-стандарт для GI per [Morgan McGuire NVIDIA RTX GI blog](https://developer.nvidia.com/blog/rtx-global-illumination-part-i/).

**Конкретно:**
- Новая структура `RtxGiProbes` в `src/render/RtxGiProbes.{hpp,cpp}`:
  - 8×8×8 grid probes (512 для VoxelLab scene bounds)
  - Per-probe: 64 ray directions, radiance + depth storage (2 × vec4 = 32 bytes per probe)
  - Total: 512 × 32 = 16 KiB GPU memory (ничего)
- Per-probe update: trace 64 rays в pre-baked directions, store hit radiance
- Update rate: 1 probe per frame (round-robin), 8 frames to update all 512 probes
- Probe sample: trilinear interpolation между 8 nearest probes at receiver
- Voxel shader: `SampleRtxGiDiffuseIrradiance(worldPosition, normal)` returns vec3 irradiance
- Заменить `VctSampleDirectionalCone` в `voxel.frag:137-162` (6-cone texture sampling) на
  probe-based sampling
- VCT 3D clipmap остаётся как fallback для non-RTX, но RTX path его не использует

**Ключевые файлы:**
- `src/render/RtxGiProbes.{hpp,cpp}` (new file ~300 LoC)
- `src/render/SceneResources.{hpp,cpp}` — add `RtxGiProbes` resource
- `src/shaders/voxel.frag:137-179` — replace VCT with DDGI sampling
- `src/shaders/voxel.frag:912-927` — `vctDiffuseIrradiance` → `ddgiDiffuseIrradiance`

**DoD:**
- VoxelLab diffuse GI визуально comparable to VCT (или лучше — нет bleeding artifacts на
  разных плотностях геометрии)
- DDGI update cost: <0.5 ms/frame amortized
- DDGI sample cost: <0.2 ms/frame (8 trilinear fetches per pixel)
- ctest regression green
- Dynamic scenes (fluid moving, voxels changing) update GI in real-time без clipmap rebuild lag

#### ⭐ Milestone 5.6. RTX refraction (заменяет fake transmission) — Open

**Цель:** Glass/fluid voxels используют real ray-traced refraction вместо fake Beer-Lambert
attenuation. Луч входит в voxel, выходит с IOR-based bend, читает background.

**Конкретно:**
- Новая GLSL функция `TraceRtxRefractionRay(origin, direction, ior)` через rayQueryEXT
- Modify `voxel.frag` glass path (around lines 969-977) — current fake transmission replaced with
  ray-traced lookup of background voxel color
- IOR: glass = 1.5, fluid = 1.33 (standard values)
- Внутри fragment shader: trace ray, if hit non-air voxel → return hit color blended with
  refraction; if miss → use sky color

**Ключевые файлы:**
- `src/shaders/voxel.frag:76-100` — new `TraceRtxRefractionRay`
- `src/shaders/voxel.frag:969-977` — replace fake transmission

**DoD:**
- Glass voxels показывают distorted background (видны объекты за стеклом)
- Fluid voxels показывают underwater look (IOR-bent background)
- Refraction pass cost: <0.3 ms/frame (1 ray/pixel, terminate-on-first-hit)
- Visual smoke в `lookdev-captures/` для сравнения fake vs real

#### ⭐ Milestone 5.7. RTX multi-bounce GI (path tracing для indirect) — Open (опционально)

**Цель:** Достичь ground-truth indirect lighting через multi-bounce path tracing для specular
surfaces (mirrors, polished metal). Дополняет DDGI (только diffuse) для глянцевых surfaces.

**Конкретно:**
- `TraceRtxSmoothSpecularRay` уже есть (`:77-86`) — расширить до N bounces
- Per bounce: trace ray, evaluate BRDF (specular component), accumulate
- Bounce count: 2-3 (RTX GI reference: 2 bounces достаточно для visual quality)
- Заменить VCT specular в `voxel.frag:918-928` на ray-traced multi-bounce

**Ключевые файлы:**
- `src/shaders/voxel.frag:77-86` — `TraceRtxSmoothSpecularRay` → `TraceRtxMultiBounceSpecular`
- `src/shaders/voxel.frag:918-928` — VCT specular path → ray-traced path

**DoD:**
- Зеркальные surfaces показывают visible reflections других surfaces (ground truth GI)
- Multi-bounce cost: <0.5 ms/frame (1 ray × 3 bounces = 3 rays/pixel)
- Аналог `2026-06-21-rt-shadows-vs-csm` experiment verdict: visual quality boost стоит perf cost

---

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

6. **Стратегический порядок:** RTX shadows primary (5.2.A→B→C, multi-session) → CSM removal (5.2.D)
   → RTX AO replacement (5.4) → DDGI (5.5) → RTX refraction (5.6) → TAA + tonemap + post-FX polish
   (7.x, опционально) → deferred roadmap (6.2/2.3 при появлении trigger-критериев). Разворот 2026-06-22:
   non-RTX CSM bias tuning признан неэффективным (peter-panning не решается через bias coefficients);
   CSM **полностью удаляется** (задача 7.2 closed как REMOVED 2026-06-22). Hardware target = RTX 20/30/40/50 series.
   Non-RTX fallback **не предоставляется** — это pet-project, нет нужды в legacy уступках.

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
| 5. RTX-Driven Lighting & Temporal | 5.2 RTX shadows (A/B/C/D) · 5.3 TAA MV · 5.4 RTX AO · 5.5 DDGI · 5.6 RTX refraction | 🔓 resumed 2026-06-22, A/B/C in flight |
| 6. Refactoring | 6.1 ECS, 6.3 Async Compute | ✅ · 6.2 PIMPL 🔒 deferred-pending |
| 7. Rendering Polish (post-RTX-shadow) | 7.1 VCT cones · 7.2 TAA · 7.3 tonemap · 7.4 post-FX | 🔓 all open |