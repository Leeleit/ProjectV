# Памятка Тиммейта 3 — Рендеринг (говорит T4 Тесты)

**Участник:** [Имя Тимейта 3]
**Слот на сцене:** 2:40–3:20 (40 секунд) — T4 Тесты и проверки
**Твоя реальная компетенция:** Рендеринг (Vulkan 1.4, TAA, CSM, AOCC, шейдеры, C-ядро frustum cull)
**Что НЕ твоё (к кому перенаправлять в Q&A):** стек/демо — к le1t; вступление — к Тиммейту 1; воксельный мир — к Тиммейту 2; физика — к Тиммейту 4; ассеты/аудио — к Тиммейту 5; все баги — к le1t

---

## 1. Шапка выступления

> «Здравствуйте. Меня зовут [Имя Тимейта 3], я расскажу, как мы проверяли результат — и в первую очередь визуальные тесты рендеринга.»

---

## 2. Что говорить дословно (~75-85 русских слов, 0:40)

> «Здравствуйте. Коротко о том, как мы проверяли результат. У нас 14 наборов автоматических тестов ядра: математика, инвалидация грязных чанков, walk-контроллер, жадный мешинг, frustum culling, клеточный автомат для жидкостей. Все зелёные при каждой сборке, ноль предупреждений в нашем коде. Плюс рантайм smoke: 6 эталонных снимков — финальный кадр, тени, контактные тени, затенение, локальный свет, отладочный слой. Передаю слово.»

---

## 3. Понятия (8 терминов, чтобы понимать что говоришь)

| Термин | Что это |
|---|---|
| ctest | Утилита запуска тестов из CMake |
| Тест ядра | Набор проверок математики, структур данных, ключевых алгоритмов |
| Инвалидация грязных чанков | Помечание чанков вокселей как требующих пересборки меша |
| Walk-контроллер | Логика управления игроком от первого лица |
| Frustum culling | Отсечение объектов за пределами пирамиды видимости камеры |
| Fluid CA | Клеточный автомат для симуляции жидкости |
| Runtime smoke | Сквозная проверка жизненного цикла: запустить, отрендерить, сравнить снимок |
| Sidecar-файл | Текстовый файл `.txt` рядом с `.bmp`, содержит метаданные снимка (60+ ключей) |

---

## 4. Что показывать на экране

1. **Вывод ctest** (~10 секунд):
   ```bash
   cd /home/le1t/Projects/ProjectV
   ctest --test-dir build/linux-clang-debug --output-on-failure
   ```
   Ожидаемый результат: **14/14 тестов пройдены** за ~0.78 секунды.

2. **Каталог smoke-снимков** (~5 секунд):
   ```bash
   ls build/linux-clang-debug/lookdev-captures/
   ```
   Показывает подкаталоги с эталонами.

3. **Sidecar одного снимка** (~5 секунд): открыть `lookdev-captures/.../sidecar.txt`, показать 60+ ключей.

4. **Цикл debug-видов** клавишей `B`: FINAL → SHDW → CSM → CTSH → AOCC → LOCL → FINAL. Каждый вид визуализирует отдельный слой.

---

## 5. Твоя настоящая компетенция (для Q&A): Рендеринг

**Это то, что ты реально знаешь. На сцене ты говоришь про тесты, но на вопросы комиссии отвечаешь по своей компетенции.**

**Ключевые файлы:**
- `src/render/Renderer.{hpp,cpp}` — `DrawFrame` (главный per-frame entry point)
- `src/render/SceneResources.{hpp,cpp}` — chunk visibility cache, XOR-fold splitmix64 hash
- `src/render/ShadowProjection.{hpp,cpp}` — 4-cascade projection, `BuildSunShadowCascadeSplits(near, far, lambda=0.80)`
- `src/render/Taa.{hpp,cpp}` — 8-tap Halton(2,3) jitter, `BuildTaaHistoryParams`
- `src/render/RayMarchPass.{hpp,cpp}` — STUB (Phase 7), `SetRayMarchEnabled` API state
- `src/render/ScreenshotCapture.{hpp,cpp}` — `.bmp` + `.txt` sidecar writer
- `src/render/vulkan/VulkanInit.{hpp,cpp}` — Vulkan 1.4 init, `VulkanInitError` enum
- `src/render/vulkan/VulkanGraphicsPipeline.{hpp,cpp}` — graphics pipelines
- `src/render/vulkan/VulkanVoxelMeshingPipeline.{hpp,cpp}` — compute-шейдер пайплайн для мешинга
- `src/render/vulkan/TaaResolvePipeline.{hpp,cpp}` — TAA resolve
- `src/render/vulkan/VulkanSwapchain.{hpp,cpp}` — swapchain, present modes
- `src/shaders/voxel.frag`, `voxel.vert`, `voxel_shadow.frag`, `voxel_shadow.vert`, `voxel_mesh.comp`, `ray_march.comp`, `taa_resolve.frag`, `taa_resolve.vert`, `debug_overlay.*`, `debug_hud.*`, `model.frag`, `model.vert`, `lighting.glsl`
- `src/c_kernels/frustum_cull.{hpp,c}` — C-ядро для frustum culling
- `src/c_kernels/FrustumCulling.hpp` — C++ обёртка

**Vulkan 1.4 init:** `VulkanInitError` enum (Tier 1.B, std::expected). 16 вариантов: PreconditionFailed, BootstrapFailed, TracyContextFailed, SwapchainFailed, WorldCreationFailed, EcsSyncFailed, PhysicsStateFailed, SceneResourcesFailed, GraphicsPipelineFailed, VoxelMeshingPipelineFailed, ModelPipelineFailed, ModelManifestFailed и т.д.

**Shadow cascade:** 4 каскада (per `decisions.md §18`), shadow map 2048×2048. `BuildSunShadowCascadeSplits(near, far, lambda=0.80)` — near-biased распределение. Per-cascade projection: sub-frustum → light-space → sphere stabilization.

**Per-preset shadow params (из `VoxelMaterials.cpp:181-236`):**
- VoxelLab: `sunShadowParams = {0.72, 0.0009, 0.0060, 1.10}` (strength, depthBias, normalBias, filterRadius)
- FlatBenchmark: `{0.64, 0.0008, 0.0055, 1.25}`
- Contact shadows: `sunContactShadowParams = {0.28, 2.25, ...}` (strength, maxDistance)

**TAA (Temporal Anti-Aliasing):** 8-tap Halton(2,3) sequence. По умолчанию `taaEnabled=false` (jitter=0, стабильная картинка). YCoCg-зажим в `taa_resolve.frag`. Per-layer history (CTSH/AOCC/LOCL) — `mix(rawCurrent, history, blend=0.4)`.

**Lighting debug views (10):** `Final`, `Ambient`, `Direct`, `Local`, `Shadow`, `Cascade`, `Contact`, `Occlusion`, `Fog`, `Taa`. Цикл клавишей `B`.

**6 smoke captures:** `FINAL`, `SHDW`, `CSM`, `CTSH`, `AOCC`, `LOCL` — пиксель-в-пиксель с эталоном. Запуск: `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh`. Outputs в `build/<preset>/lookdev-captures/<timestamp>/`.

**C-ядро (frustum cull):**
- `projectv_cull_frustum_scalar` — scalar C, **3.7-3.9× быстрее** C++ baseline (per `src/bench/FrustumCullBenchmark.cpp`)
- `projectv_cull_frustum_avx2` (с `__AVX2__`) — 2.5-2.7× (autovectorizer в debug бьёт hand-rolled)
- Crossover threshold: `kBatchDispatchThreshold = 8` AABBs (ниже — inline C++ helper)

**Ray-march pass (STUB):** `RecordRayMarchCommands` — no-op, `fprintf` в stderr. Compute-шейдер `ray_march.comp` (Amanatides-Woo DDA) скомпилирован, API state работает, но в graphics stream не вкомпонован. **Phase 7 follow-up** (per `decisions.md` и `docs/DefenseReport.md §3`).

**Hot shader reload:** клавиша `1` → `cmake --build $BUILD_DIR --target Shaders` → `RequestRayMarchPipelineRecreate()`. Лог в `std::filesystem::temp_directory_path()/projectv_shader_reload.log`. Релокация с F5 2026-06-15.

**3 tone-map оператора:** `Linear`, `Reinhard`, `AcesApprox` (default). Клавиша `N` — cycle.
**3 exposure metering mode:** `Manual`, `SceneKey` (default).
**6 shadow tuning targets:** `Strength`, `DepthBias`, `NormalBias`, `FilterRadius`, `Coverage`, `CascadeBlend`. Клавиши `O` cycle target, `U`/`I` decrease/increase.

Подробнее — `docs/DefenseCompetency_FAQ.md §3` (textbook для Тиммейта 3).

---

## 6. Вне зоны ответственности (к кому перенаправлять в Q&A)

| Вопрос про… | Говори |
|---|---|
| Почему Vulkan 1.4, не OpenGL/DX12/Metal | «Архитектурное решение — к le1t» |
| DOD layout / `alignas(16)` / push constants | «Архитектура, к le1t» |
| Стек/Clang/cmake/ctest baseline | «К Тиммейту 1» |
| Voxel-мир / чанки / мешинг | «К Тиммейту 2» |
| Voxel raycast | «К Тиммейту 2» |
| Физика / walk controller / Jolt | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| BUG-004 VoxelLab tremor (отвергнут) | «Не существует, jitter=0 default» |
| SSAO / GTAO отложено | «Roadmap, к Тиммейту 4 (он закрывает)» |
| Hot shader reload | «К le1t» |

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 0:40 ± 5 секунд. §5 прочитать отдельно для Q&A (это самая длинная секция, т.к. рендеринг — самый большой модуль).
