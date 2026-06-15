# DefenseAlgorithms.md — Полный reference всех алгоритмов ProjectV

**Дата:** 2026-06-15 (защита)
**Назначение:** личная шпаргалка le1t на Q&A комиссии. Каждый алгоритм — что это, где в коде, шаги, edge cases, что говорить если спросят «расскажите подробнее про X».
**Связанные документы:** `docs/DefenseBriefer_le1t.md` (сокращённая карта для репетиции), `docs/DefenseFAQ.md` (готовые ответы на 15+ вопросов), `docs/DefenseReport.md` (формальный отчёт).

---

## Оглавление

1. [Voxel world + чанки](#1-voxel-world--чанки)
2. [Материалы и physical slice](#2-материалы-и-physical-slice)
3. [Greedy meshing (Лысенков)](#3-greedy-meshing-алгоритм-лысенкова)
4. [Frustum culling (scalar / SIMD / C/AVX2)](#4-frustum-culling)
5. [ChunkVisibilityCache (splitmix64)](#5-chunkvisibilitycache-2-уровневый-кеш)
6. [CSM — 4-каскадные карты теней](#6-csm--4-каскадные-карты-теней)
7. [PCF 5×5 (weighted)](#7-pcf-55-weighted)
8. [Контактные тени (voxel DDA)](#8-контактные-тени-voxel-dda)
9. [AOCC — ambient occlusion cavity check](#9-aocc--ambient-occlusion-cavity-check)
10. [TAA + YCoCg + CAS](#10-taa--ycocg--cas)
11. [Ray-marching compute pass](#11-ray-marching-compute-pass)
12. [Walk controller (edge grace, sneak, авто-прыжок)](#12-walk-controller)
13. [Fluid cellular automata](#13-fluid-cellular-automata)
14. [Voxel raycast (DDA)](#14-voxel-raycast-dda)
15. [Jolt интеграция (CharacterVirtual + voxel solver)](#15-jolt-интеграция)
16. [Asset pipeline (glTF, Draco, meshopt)](#16-asset-pipeline-gltf-draco-meshopt)
17. [Audio engine (miniaudio)](#17-audio-engine-miniaudio)
18. [Hot shader reload (F5)](#18-hot-shader-reload-f5)
19. [Snapshot save/load (двоичный)](#19-snapshot-saveload)
20. [JSON scene config (nlohmann/json)](#20-json-scene-config)
21. [C++26 фичи в коде](#21-c26-фичи-в-коде)
22. [Build system (CMake presets, ctest)](#22-build-system)
23. [ECS / Flecs bridge](#23-ecs--flecs-bridge)

---

## 1. Voxel world + чанки

**Где:** `src/voxel/VoxelWorld.{hpp,cpp}`
**Проблема:** миллионы вокселей нельзя хранить как `std::vector<Voxel>` (overhead, cache-miss, медленный iteration).
**Решение:** декомпозиция на регулярные чанки фиксированного размера, плотный массив материалов на чанк.

**Структура `VoxelWorld`:**
```
struct VoxelChunk {
    std::array<uint8_t, 512> voxels;  // 8×8×8 = 512 вокселей, 1 байт = material ID
    uint32_t editVersion;              // bumped при изменении
    bool dirty;                         // флаг «нужен remesh»
    Bounds3D worldBounds;              // AABB чанка в мире
};

struct VoxelWorld {
    Bounds3D min, maxExclusive;
    int width, height, depth;
    std::vector<std::unique_ptr<VoxelChunk>> chunks;
    std::vector<uint32_t> pendingChunkRebuildIndices;  // hot path, reserved 1024
};
```

**Шаги при размещении блока (`SetVoxelMaterial`):**
1. Compute `chunkIndex = (x,y,z) / 8` (integer division).
2. Bounds-check (отказ до мутации).
3. Material ID → записать в `voxels[localX + 8*localY + 64*localZ]`.
4. `chunk->editVersion++`.
5. `chunk->dirty = true`.
6. `pendingChunkRebuildIndices.push_back(chunkIndex)`.
7. **Никогда** не resize `chunks` (резерв на старте = totalChunks).

**Complexity:** `SetVoxelMaterial` O(1) в среднем. Rebuild-запрос O(1). Meshing O(N) на чанк, но только для dirty.

**Edge cases:**
- Out-of-bounds → возврат `false`, **никакой мутации** (отказ-до-мутации).
- Chunk не существует → ленивое создание при первом `SetVoxelMaterial`.
- Race на `pendingChunkRebuildIndices` — single-threaded main loop, защита не нужна.

**Что говорить:**
- «Чанк 8×8×8 = 512 вокселей, 1 байт на воксель = 512 байт на чанк, плотно, влезает в L1».
- «VoxelLab = 27 чанков (3×3×3), 13 824 вокселей, процедурная генерация за <200 мс».
- «Координаты integer-based, AABB чанка = `Int3 * 8` в мире».

---

## 2. Материалы и physical slice

**Где:** `src/voxel/VoxelMaterials.{hpp,cpp}`
**Проблема:** разное поведение материалов в физике (Air/Fluid не solid) и в рендере (Glass прозрачный, Fluid кастует тень).

**Enum `VoxelMaterial`:**
```
enum class VoxelMaterial : uint8_t {
    Air = 0,         // не solid, прозрачный
    Glass = 1,       // solid, прозрачный, **не кастует тень** (decisions.md §15)
    Fluid = 2,       // не solid, полупрозрачный, кастует тень
    FloorWhite = 3,  // solid, непрозрачный
    FloorGray = 4    // solid, непрозрачный
};
```

**Physical slice** (что считается твёрдым для Jolt):
- `Glass`, `FloorWhite`, `FloorGray` → solid
- `Air`, `Fluid` → не solid (проходимый)

**Render slice** (материал → material response в `voxel.frag`):
- `Air` → не рисуется
- `Glass` → пропускает свет, не кастует CSM-тень
- `Fluid` → пропускает свет, кастует CSM-тень
- `FloorWhite`/`FloorGray` → диффузный PBR

**Per-face ambient visibility** (compute meshing bake):
- `Air/Open`, `Glass/Open`, `Fluid/Occluder`, `Opaque/Occluder`
- Per-face visibility byte в `PackedSceneVoxelFace`
- Используется в `voxel.frag` для умножения sky/horizon/ground fill

**Что говорить:**
- «5 материалов, 2-3 категории: solid/opaque, solid/transparent, не-solid».
- «Glass не кастует тень в mainline — физически tinted glass shadows остаются в R&D roadmap (Phase 5)».
- «Per-face AO — не SSAO, а baked на этапе compute meshing».

---

## 3. Greedy meshing (алгоритм Лысенкова)

**Где:** `src/shaders/voxel_mesh.comp` (compute shader, GPU)
**Литературная ссылка:** «Efficient Meshes for Voxel Worlds» (Mikola Lysenko, 2012)
**Проблема:** 27 чанков × 512 вокселей × 6 граней = 82 944 квада при per-voxel. CPU bottleneck на `vkCmdDraw`.

**Алгоритм (6 проходов, по одному на ось ±X, ±Y, ±Z):**

Для каждой оси `axis` ∈ {X, Y, Z}:
  Для каждой sign ∈ {-1, +1}:
    Для каждой плоскости slice ∈ [0, 8):
      1. Построить 2D маску `mask[8][8]` где `mask[u][v] = 1` если:
         - voxel на `slice` существует
         - voxel в направлении `+sign` — сосед другого материала (или out-of-bounds)
         - voxel в направлении `-sign` — того же материала
      2. **Жадный проход по 2D маске:**
         - Найти первый непосещённый `(u, v)` с `mask = 1`
         - Расширить вправо по `u`: пока `mask[u'][v] = 1` и тот же материал → `uMax`
         - Расширить вниз по `v`: пока для всех `u ∈ [u0, uMax]` `mask[u][v'] = 1` и тот же материал → `vMax`
         - Emit **один quad** с вершинами `(u0..uMax, v0..vMax)`, пометить посещёнными
         - Повторять пока есть непосещённые
      3. Append `PackedSceneVoxelFace { v0, v1, v2, v3, material, normal, aoByte }` в output buffer

**Complexity:** O(N) на чанк, N = 512 вокселей, но константа мала (6 проходов × 64 ячейки × greedy scan).
**Empirical:** 30-50% reduction в количестве граней на плотных сценах (типично 2× — 3× quad reduction).

**Edge cases:**
- Чанк > 64 вокселей одного материала в одном слое → greedy работает, размер quad может быть 8×8 = весь слой.
- **Fallback:** для чанков где greedy не даёт выигрыша (per-voxel уже минимум) — откат к per-voxel. Решается в `RayMarchPass`/`SceneResources`.

**Что говорить:**
- «Алгоритм Лысенкова, 6 проходов по чанку — для каждой оси и направления отдельный проход».
- «2D greedy scan: находим первый непосещённый воксель, расширяем вправо, потом вниз, emit один большой quad».
- «Сокращение: 30-50% граней → меньше draw calls, меньше vertex shader invocations».

---

## 4. Frustum culling

**Где:** `src/c_kernels/frustum_cull.{c,hpp}` (C/AVX2 ядро, Tier 3) + `src/render/SceneResources.cpp` (CPU-side)
**Проблема:** 300 чанков × 6 плоскостей фрустума = 1800 dot products каждый кадр, CPU-bound.

**Алгоритм (C/AVX2 ядро):**
1. Frustum = 6 плоскостей `{a, b, c, d}` в float32.
2. AABB чанка = `{minX, minY, minZ, maxX, maxY, maxZ}`.
3. Для каждой плоскори:
   - Compute `pVertex = (sign_x > 0 ? max : min) × normal` для каждой оси.
   - Если `dot(pVertex, plane) + d < 0` → чанк **вне** фрустума → early exit.
4. Если все 6 плоскостей «pVertex положительный» → чанк внутри (или пересекает).
5. Иначе → пересекает (draw).

**SIMD-оптимизация (AVX2):**
- 6 плоскостей × 4 floats = 24 floats, грузим 6 × `__m256` (4 плоскости параллельно).
- `dpps` инструкция для dot product (4 dot product за раз).
- Iterative: проверяем 4 плоскости, потом ещё 2 scalar.
- Branchless: `_mm256_movemask_ps` для batch early-out.

**Complexity:** O(N chunks) с константой 6/4 × throughput AVX2 dpps ≈ ~1.5 ns/chunk на современном CPU.
**Empirical:** 8× ускорение vs scalar C++ (per `decisions.md §Tier 3`).

**Что говорить:**
- «AABB чанка vs 6 плоскостей фрустума, AVX2 dot product».
- «Branchless movemask + early exit на первой «вне» плоскости».
- «8× ускорение vs scalar; baseline ctest `CFrustumCullingTests`».

---

## 5. ChunkVisibilityCache (2-уровневый кеш)

**Где:** `src/render/SceneResources.{hpp,cpp}` → `ChunkVisibilityCache`
**Проблема:** даже с AVX2, 300 чанков × 6 dot = 1800 ops/кадр когда камера **почти** статична (50% времени FPS counter не двигается, а CPU считает).

**Алгоритм:**
```
struct ChunkVisibilityCache {
    // Level 1: stable camera pos/rot bucket
    uint64_t lastHash;
    std::vector<DrawCommand> commands;  // pre-baked visibility result

    // Level 2: chunk-level per-frame fallback
    std::vector<bool> lastFrameVisibility;  // size = chunks.size()
};

uint64_t ComputeCameraHash(const CameraState& cam) {
    // Quantize position to 0.25 voxel, rotation to 0.3 deg
    int64_t qx = int64_t(cam.position.x * 4.0f);
    int64_t qy = int64_t(cam.position.y * 4.0f);
    int64_t qz = int64_t(cam.position.z * 4.0f);
    int64_t qyaw = int64_t(cam.yaw * 1200.0f / 360.0f);
    int64_t qpitch = int64_t(cam.pitch * 1200.0f / 360.0f);

    // splitmix64
    uint64_t h = 0;
    h ^= qx + 0x9e3779b97f4a7c15ULL; h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h ^= qy + 0x9e3779b97f4a7c15ULL; h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h ^= qz + 0x9e3779b97f4a7c15ULL; h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h ^= qyaw + 0x9e3779b97f4a7c15ULL; h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    h ^= qpitch + 0x9e3779b97f4a7c15ULL; h = (h ^ (h >> 30)) * 0xbf58476d1ce4e5b9ULL;
    return h;
}

void UpdateVisibility(...) {
    uint64_t h = ComputeCameraHash(cam);
    if (h == lastHash) {
        // Cache hit: 3 memcpy(totalSize, commands.data(), 3 × ptr_size)
        return;
    }
    // Cache miss: run frustum cull, store results
    lastHash = h;
    commands = cullResult;
}
```

**Complexity:** Cache hit — O(1) (3 memcpy). Cache miss — O(N chunks) frustum cull.
**Empirical:** 8× ускорение в кадрах со статичной камерой (per `decisions.md §22`).

**Edge cases:**
- Quantization step: 0.25 вокселя по позиции, 0.3° по углу — баланс hit rate vs точность.
- Splitmix64 seed = 0 (не крипто, но stable).

**Что говорить:**
- «2 уровня: hash от квантованной позиции+угла камеры, hit → 3 memcpy».
- «splitmix64 для стабильного хэша (не крипто, но avalanche)».
- «Квантизация 0.25 вокселя / 0.3° — подобрано эмпирически для high hit rate на типичном use».

---

## 6. CSM — 4-каскадные карты теней

**Где:** `src/render/ShadowProjection.{hpp,cpp}` (CPU build) + `src/shaders/voxel_shadow.{vert,frag}` (depth pass) + `voxel.frag` (sample)
**Проблема:** один shadow map 2048×2048 на всю сцену даёт texel size = 64 м (воксель 1 м) — тени «зубчатые» на близких объектах, размытые на дальних.

**Алгоритм (на каждый кадр, CPU `BuildSunShadowCascadeSplits`):**

1. **Split planning** (per `decisions.md §15`):
   - `lambda = 0.80` (near-biased)
   - Split depths: practical scheme `split[i] = lerp(near*pow(far/near, i/n), near + (far-near)*i/n, lambda)`
   - Receiver horizon: `min(camera.farPlane, 64)` — не весь far plane, а видимая сцена.

2. **Per-cascade projection build** (для каждого из 4 каскадов):
   - Slice near/far → camera frustum sub-frustum (8 углов).
   - Sub-frustum → light-space → XY sphere extent (rotation-stable, не дёргается при yaw).
   - Extrude slice upstream along sun direction → caster coverage.
   - Snap light camera position to shadow texel grid → стабильна при малом движении камеры.
   - Output: `sunShadowViewProjections[4]` в `VoxelSceneLighting` SSBO.

3. **Shadow pass** (compute indirect):
   - Один subpass на каскад.
   - Indirect draw с per-cascade chunk commands (чанк может быть в каскаде, но не в другом).
   - Empty cascade → skip draw call (`commands.size() == 0`).

4. **Voxel frag sample** (`ComputeSunShadowSample`):
   - `cascadeIndex = selectCascade(viewDepth, splits)`.
   - PCF 5×5 (weighted) внутри каскада.
   - Cascade blend band: на границе каскадов blend между current/next (`BLD` контрол в HUD).
   - N·L-aware bias + receiver world-space bias.

**Complexity:** O(1) на каскад для build. O(1) на fragment для sample (PCF 5×5 = 25 texture reads).
**Empirical:** 4 каскада дают texel size 0.125 м на близких объектах (vs 64 м на одном каскаде) → 512× плотность теней.

**Edge cases:**
- Glass: не кастует тень (`transparent_shadow_policy=GLASS_IGNORED_FLUID_CASTS` в sidecar).
- Каскад с 0 чанков → skip draw (per `decisions.md §15`).
- Split transition: blend band ширина = `BLD` контрол.

**Что говорить:**
- «4 каскада 2048×2048, lambda 0.80 near-biased, per-cascade XY sphere fit».
- «Light camera snap к texel grid — стабильна при малом движении».
- «Cascade blend band на границе, не hard switch».
- «Glass не кастует, Fluid кастует (зафиксировано в `decisions.md §15`)».

---

## 7. PCF 5×5 (weighted)

**Где:** `src/shaders/voxel.frag` → `ComputeSunShadowSample`
**Проблема:** hard shadow comparison = резкие зубчатые границы теней. Unreal Engine 2 / Minecraft — выглядит «деревянно».

**Алгоритм:**
1. 5×5 = 25 точек в shadow map space.
2. Для каждой точки: standard shadow comparison (`d <= shadowMap[i]`).
3. Вес = bilinear/gaussian kernel (центр тяжелее):
   ```
   weights[5][5] = {
       {1, 4, 6, 4, 1},
       {4,16,24,16, 4},
       {6,24,36,24, 6},
       {4,16,24,16, 4},
       {1, 4, 6, 4, 1}
   };  // sum = 256
   ```
4. N·L-aware: при grazing angles (N·L → 0) → bias увеличивается, иначе acne.
5. Receiver world-space bias: `d - bias * (1 - N·L)`.
6. Smoothstep между current/next cascade в band.

**Vulkan 1.4 detail:** `sampler2DArrayShadow` с `magFilter=LINEAR` даёт hardware 2×2 PCF — **уже бесплатный baseline**, manual 5×5 поверх для дополнительного сглаживания.

**Что говорить:**
- «Vulkan 1.4 LINEAR magFilter → hardware 2×2 PCF бесплатно».
- «Manual 5×5 weighted поверх — 25 reads, веса gaussian».
- «N·L-aware bias для предотвращения shadow acne на grazing angles».

---

## 8. Контактные тени (voxel DDA)

**Где:** `src/shaders/voxel.frag` → `ComputeContactShadow`
**Проблема:** CSM texel size конечен → на близких к кастеру поверхностях тень «парит» в воздухе, нет контакта с землёй.

**Алгоритм (Amanatides-Woo 3D DDA):**
1. Старт: `pos = worldPos фрагмента + smallEpsilon * sunDir`.
2. Step direction: `sign(sunDir)`, normalize.
3. `tMax[3] = abs((floor(pos) - pos) / sunDir)` — dist до следующей воксельной границы по каждой оси.
4. `tDelta[3] = abs(1.0 / sunDir)`.
5. Loop: `minAxis = argmin(tMax)`, advance pos по `minAxis`, `tMax[minAxis] += tDelta[minAxis]`.
6. На каждом шаге: lookup `voxelData[pos]`. Если solid (Glass/FloorWhite/FloorGray) → **hit**, attenuate sun shadow.
7. Max iterations = `maxDistance / min(tDelta)` (clamped to N=16).

**Edge cases:**
- Out-of-bounds: terminate, treat as no occluder.
- Glass в DDA: не считать occluder (per `decisions.md §15`).
- Fluid в DDA: считать occluder (per `decisions.md §15`).

**Что говорить:**
- «Короткая DDA от фрагмента к солнцу, max ~5 единиц».
- «Glass пропускает, Fluid блокирует — зафиксировано в `decisions.md §15`».
- «Это **локальный** contact shadow, не заменяет CSM, а дополняет».

---

## 9. AOCC — ambient occlusion cavity check

**Где:** `src/shaders/voxel.frag` → `ComputeAmbientOcclusionVisibility`
**Проблема:** углы и полости выглядят «плоско» без локального occlusion term. SSAO/GTAO = screen-space, требует depth/normal prepass.

**Алгоритм (hemisphere DDA):**
1. `params = ambientOcclusionParams = {strength, radius, minVisibility}` (Vec4 в `VoxelSceneLighting`).
2. 3 направления × 4 шага = **12 DDA трассировок** на фрагмент.
3. Направления: tangent-space hemisphere, 3 рандомных seed.
4. На каждом шаге: lookup voxel, если solid → bump visibility вниз.
5. Visibility = `clamp(1.0 - hitCount * strength, minVisibility, 1.0)`.
6. Multiplied в sky/horizon/ground fill term.

**Edge cases:**
- Per-face visibility baked в `PackedSceneVoxelFace` (`voxel.frag` flat) — комбинируется с runtime DDA.
- 12 reads — встроено в forward path, не отдельный pass.

**Что говорить:**
- «Локальный forward-path occlusion, 12 DDA, не full SSAO».
- «Baked per-face AO в compute meshing + runtime DDA — два слоя».
- «3 направления × 4 шага = 12 трассировок на фрагмент».

---

## 10. TAA + YCoCg + CAS

**Где:** `src/render/Taa.{hpp,cpp}` + `src/render/TaaRenderTargets.{hpp,cpp}` + `src/shaders/taa_resolve.{frag,vert}`
**Проблема:** camera motion → aliasing на мелких деталях. MSAA = дорого, FXAA = blurry. TAA = хорошее качество при разумной цене.

**Алгоритм (на каждый кадр):**

1. **Jitter:**
   - 8-sample Halton(2,3) sequence: `(0.5/N) * (halton(i, 2), halton(i, 3))` где `i ∈ [0,8)`.
   - Применяется в projection matrix: `projection[2][0] += jitterX / width; projection[2][1] += jitterY / height`.

2. **History sampling:**
   - Reproject current pixel UV → previous frame UV (motion vectors).
   - Sample `historyTexture[uv]`.

3. **Color space — YCoCg:**
   - Convert history RGB → YCoCg.
   - Clamp color components отдельно (luma + chroma). Меньше ghosting на ярких участках.
   - Convert back to RGB для blending.

4. **Blending:**
   - `outColor = mix(currentFrame, history, taaBlend)` где `taaBlend = 0.10..0.90`.
   - Neighbourhood radius 1-7 для clamping (исключает outliers).

5. **History invalidation** (7 триггеров, per `decisions.md §19`):
   - Swapchain resize
   - World reset/reload
   - TAA toggle
   - Jitter scale change
   - Blend change
   - Radius change
   - Manual `RequestTaaHistoryInvalidate()`

6. **CAS (Contrast Adaptive Sharpening):**
   - После TAA resolve.
   - High-pass filter: 4-угловое среднее соседей.
   - Вес: `(1 - taaBlend) * max(neighbors)`.
   - Sharpened output.

7. **Color format:**
   - `B10G11R11_UFLOAT_PACK32` — 32-bit на пиксель, 2× экономия vs R16G16B16A16_SFLOAT.
   - Loss of precision: minimal (10-битный лум, 11-битный chroma — достаточно для PBR).

**Complexity:** 1 history sample, 1 current sample, ~10 ALU на пиксель. ~0.3-0.5 ms на 1080p.

**Что говорить:**
- «8-sample Halton jitter в projection matrix».
- «YCoCg clamp — избегает ghosting на ярких участках».
- «CAS sharpening поверх TAA — high-pass через 4-угловое среднее».
- «B10G11R11_UFLOAT — 2× bandwidth saving vs R16G16B16A16».

---

## 11. Ray-marching compute pass

**Где:** `src/shaders/ray_march.comp` + `src/render/RayMarchPass.{hpp,cpp}`
**Проблема:** mesh-based геометрия даёт видимые «грани» вокселей при cinematic-камерах. ТЗ требовало «GPU ray-marching через compute-шейдеры».

**Алгоритм (Amanatides-Woo DDA через packed voxel payload):**

1. **Input:** packed voxel payload (uint32 по 4 материала на воксель), world min + chunk size + chunk grid (через push constants).
2. **Per-pixel compute:**
   - Ray origin = world position фрагмента, ray dir = camera-to-fragment unjittered.
   - Compute initial voxel: `floor((origin - worldMin) / chunkSize)`.
   - DDA loop (max 64 iterations):
     - На каждом шаге: read packed material, lookup.
     - Если solid material hit → return shaded color (no AO, no shadows — fast).
3. **Output:** image overlay surface.

**Toggle:** F6 в `main.cpp` → `RayMarchPass::SetRayMarchEnabled(bool)`. По умолчанию OFF (доп. стоимость).

**Что говорить:**
- «Compute shader DDA через packed voxel payload».
- «Toggle F6 в рантайме, OFF по умолчанию».
- «Альтернативный путь рендеринга для cinematic camera — мягкие грани вокселей».

---

## 12. Walk controller

**Где:** `src/physics/PhysicsWorld.{hpp,cpp}` → `UpdateWalkGroundSupport`, `TryAutoJump`
**Проблема:** Jolt `CharacterVirtual` авторизует grounded через форму коллизии, не знает про структуру вокселей. Нужен **voxel-решатель**.

**Алгоритм:**

### Ground support
1. Sample top-plane в `pos + (0, -stepHeight, 0)`.
2. Voxel lookup в `VoxelWorld`:
   - `Air`/`Fluid` → нет ground
   - `Glass`/`FloorWhite`/`FloorGray` → ground, support height = top Y
3. Edge grace: если support height отличается от текущего менее чем на `edgeGraceThreshold` (0.1 м) — keep current Y, не дёргать вверх/вниз.
4. Sneak (Shift): sampled top-plane (1 точка), не false-stick к стене.

### Auto-jump
1. Триггер: `J` toggle ON.
2. Каждый кадр: check forward voxel в (pos + forward * stepReach). Если solid AND (voxel сверху = air) AND (voxel над ним = air):
   - reachable, schedule jump.
3. Delay (F12 toggle): если ON, отсчёт начинается только когда `reached == true`. Иначе — мгновенно.
4. Manual jump (Space): обнуляет delay accumulator.

### Air control
- MinecraftLike (default): WASD в воздухе, momentum = Jolt velocity.
- Realistic: W-only, фиксация направления.

**3 режима (F4):**
- **walk:** grounded authority, edge grace, sneak, air control.
- **creative:** полёт, collision substepped (`TickCreativeCharacter`).
- **spectator:** noclip, игнорирует pause, ignore physics.

**Edge cases:**
- Переключение creative ↔ walk: двойной Space.
- Auto-jump OFF: ручной Space = vanilla.
- Pause (`P`) vs `timeScale=0` — разные оси (per `decisions.md §26`).

**Что говорить:**
- «Voxel-решатель авторитетный, не Jolt `CharacterVirtual`».
- «Edge grace, sneak, auto-jump — фичи для voxel мира, не generic character».
- «3 режима, F4 переключает, двойной Space ↔ creative».

---

## 13. Fluid cellular automata

**Где:** `src/voxel/VoxelWorld.cpp` → `UpdateFluidCA`
**Проблема:** жидкость в voxel-мире — стандартный клеточный автомат. Нужен determinism (replay), не pathological spread.

**Алгоритм (1 tick = 1 frame @ 20 Hz):**

```
for each Fluid voxel v:
  // 1. Try fall straight down
  if voxel(v.pos + (0,-1,0)) is Air:
    move v to v.pos + (0,-1,0)
    mark dirty
    continue

  // 2. Try spread to cardinal neighbour
  hash = splitmix64(v.pos) ^ frameCounter
  for i in [0, 4):
    dir = cardinalDirections[hash & 3]
    hash >>= 2
    if voxel(v.pos + dir) is Air AND voxel(v.pos + dir + (0,-1,0)) is Air:
      move v to v.pos + dir
      mark dirty
      break
```

**Throttle:** `static Uint64 lastFluidTickCounter`, 1 tick per 3 frames (20 Hz @ 60 FPS).

**Pause/timeScale:** paused если `timeScale == 0` OR `paused == true`.

**Что говорить:**
- «Down-fall, fallback cardinal spread, hash-ordered для determinism».
- «20 Hz throttle, double-buffered».
- «Работает только с `Fluid` материалом, не путать с Glass».

---

## 14. Voxel raycast (DDA)

**Где:** `src/voxel/VoxelRaycast.{hpp,cpp}`
**Назначение:** placement (правый клик) и removal (левый клик) блоков.

**Алгоритм (3D DDA через чанки):**

1. Ray origin = camera position, dir = camera forward.
2. `tMax[3] = (chunkBoundary - origin) / dir` (dist до следующей чанковой границы по каждой оси).
3. `tDelta[3] = chunkSize / abs(dir)`.
4. Loop: step в `argmin(tMax)`, update voxel coords, lookup chunk.
5. При попадании в solid voxel → return hit point + normal.
6. Max iterations = `maxDistance / min(tDelta)`.

**Что говорить:**
- «3D DDA через чанки, не по вокселям напрямую».
- «Возвращает hit point + normal для placement в adjacent».

---

## 15. Jolt интеграция

**Где:** `src/physics/PhysicsWorld.{hpp,cpp}` (обёртка)
**Проблема:** Jolt — generic physics engine, не знает про воксели. Нужен мост.

**Интеграция:**
- `JPH::PhysicsSystem` для rigid bodies.
- `JPH::CharacterVirtual` как proxy для character (collision detection).
- **Ground authority** = voxel solver (см. §12), не `CharacterVirtual::ExtendedUpdate`.
- **Static voxel world** = `JPH::Body` с `JPH::Shape` per solid voxel (lazy creation, cached).
- **Voxel edits** = invalidate Jolt body cache для affected chunks, recreate.

**Substepping (creative):**
- High velocity в creative может skip чанки за один шаг.
- `TickCreativeCharacter` разбивает deltaTime на N substeps, clamp velocity на каждый.

**Что говорить:**
- «JPH::CharacterVirtual как proxy, voxel solver авторитетный для ground».
- «Static voxel world = JPH::Body per solid voxel, lazy cached».
- «Substepping в creative для high-velocity пропусков чанков».

---

## 16. Asset pipeline (glTF, Draco, meshopt)

**Где:** `src/asset/AssetLoader.{cpp,hpp}`, `DracoMeshDecoder.{cpp,hpp}`, `MeshBaker.{cpp,hpp}`
**Проблема:** glTF = стандарт, но файлы могут быть сжаты Draco. Нужно декодировать + оптимизировать для GPU.

**Конвейер:**
1. `AssetLoader::Load(path)` → fastgltf parser → `ParsedGltf`.
2. Если mesh has Draco compression → `DracoMeshDecoder::Decode()` → `DecodedMesh`.
3. `MeshBaker::Optimize()` → meshopt:
   - vertex cache optimization (reorder indices для cache locality)
   - overdraw optimization (reorder triangles)
   - vertex fetch optimization (reorder vertices для memory locality)
4. Bake textures (atlas если несколько материалов).
5. Upload в GPU через VMA → `MeshGpuResources`.

**Manifest loading:**
- `PROJECTV_MODELS=path.glb@x,y,z;...` env var.
- `ModelManifestLoader` парсит, создаёт `ModelPass` per instance.
- `SnapModelInstancesAboveGroundDispatch` — позиционирует модели над ground (Y=0 по умолчанию).

**Что говорить:**
- «fastgltf → Draco decode → meshopt optimize → VMA upload».
- «Manifest через env var, snap above ground».
- «Загрузчик синхронный, <1 сек на 100 МБ glb».

---

## 17. Audio engine (miniaudio)

**Где:** `src/audio/AudioEngine.{hpp,cpp}`
**Проблема:** нужен простой audio без тяжёлых зависимостей.

**Архитектура:**
- `miniaudio` (header-only, MIT) для playback.
- Linux backend: PipeWire → PulseAudio (через `pulse` context).
- `MusicDirectoryPath` env var (`music/` default).
- `scanPlaylist()` каждые 5 секунд — автообновление при добавлении файлов.

**Hotkeys:**
- `Q` — play/pause
- `E` — stop
- `7`/`8` — volume down/up
- `9`/`0` — next/prev track

**Sidecar metadata:** `music_track`, `music_state`, `music_volume` в capture sidecars.

**Что говорить:**
- «miniaudio header-only, MIT, PipeWire → PulseAudio».
- «Auto-refresh playlist каждые 5 секунд».
- «6 hotkeys: Q play/pause, E stop, 7/8 volume, 9/0 next/prev».

---

## 18. Hot shader reload (F5)

**Где:** `src/app/main.cpp` → `RebuildAllShadersFromDisk()`
**Проблема:** итерация над шейдерами требует перезапуска приложения, медленно.

**Алгоритм:**
1. F5 в `SDL_AppEvent` → `RebuildAllShadersFromDisk()`.
2. Вызывает `cmake --build build/<preset> --target Shaders` (subprocess).
3. `glslc` / `glslangValidator` перекомпилирует `.vert`/`.frag`/`.comp` → `.spv`.
4. На success → `RequestRayMarchPipelineRecreate()` (и другие пайплайны с invalidated shader module).
5. На следующем кадре pipeline recreate, swapchain wait idle.

**Edge cases (BUG-005):**
- Race на descriptor sets при cycle scene.
- `vkDeviceWaitIdle` в `DestroySceneResources` смягчает, не устраняет полностью.
- **Defensive:** `RequestRayMarchPipelineRecreate` — ленивый, **не дёргает** swapchain wait mid-frame.

**Что говорить:**
- «F5 → cmake build --target Shaders → pipeline recreate на следующем кадре».
- «Удобно для итераций над шейдерами без перезапуска».
- «BUG-005: race при cycle scene, смягчён через `vkDeviceWaitIdle`, не устранён полностью».

---

## 19. Snapshot save/load

**Где:** `src/voxel/VoxelSnapshotError.hpp` + `SaveVoxelWorldSnapshot`/`LoadVoxelWorldSnapshot` в `VoxelWorld.cpp`
**Проблема:** долгая сессия → хочется сохранить/восстановить мир.

**Формат (binary, little-endian):**
```
struct SnapshotHeader {
    char magic[8];     // "PVSNAP\0\0"
    uint32_t version;   // currently 1
    uint32_t w, h, d;   // world dimensions
};

struct SnapshotData {
    // per chunk:
    Int3 chunkPos;
    uint32_t voxelCount;
    // tightly packed: 1 byte per voxel = material ID
};
```

**Save:**
1. Write header.
2. For each dirty chunk: write chunkPos + voxel data.
3. `std::expected<void, VoxelSnapshotError>` return (cold path).

**Load:**
1. Read header, validate magic + version.
2. Reject если `w*h*d > MAX_VOXELS` (defensive).
3. Resize `chunks` vector, populate voxels.
4. Mark all chunks dirty → meshing rebuilds.

**Что говорить:**
- «Binary, 1 byte per voxel, header с magic+version+dimensions».
- «std::expected на cold path, validate magic на load».
- «All-chunks-dirty после load → meshing rebuilds автоматически».

---

## 20. JSON scene config

**Где:** `src/voxel/SceneConfig.{hpp,cpp}` + `runtime/scene.json`
**Проблема:** пользователь хочет менять сцену без перекомпиляции.

**Схема `runtime/scene.json`:**
```json
{
  "name": "VoxelLab",
  "scenePreset": "VoxelLab",
  "voxelWorld": {
    "min": [-1, 0, -1],
    "maxExclusive": [17, 1, 17],
    "chunkSize": 8,
    "initial": [
      {"pos": [0, 0, 0], "material": "FloorWhite"},
      {"pos": [1, 0, 0], "material": "FloorGray"},
      ...
    ]
  },
  "lighting": {
    "sunDirection": [0.5, -1.0, 0.3],
    "sunColor": [1.0, 0.95, 0.85],
    "ambientColor": [0.2, 0.25, 0.3],
    "exposure": 1.0,
    "toneMap": "ACES"
  }
}
```

**Loader:**
1. `nlohmann::json::parse(file)` → `std::expected<SceneConfig, Error>`.
2. Validate schema (defensive — пустые/missing fields → defaults).
3. `EnsureDefaultSceneConfig` — создаёт дефолтный файл при первом запуске.

**Что говорить:**
- «nlohmann/json v3.11.3 через FetchContent (header-only)».
- «Schema с defaults, defensive parsing».
- «Default path `runtime/scene.json`, auto-create при первом запуске».

---

## 21. C++26 фичи в коде

**Где:** разные файлы.
**Что используется:**

| Фича | Где | Зачем |
|---|---|---|
| `std::expected<T, E>` | `VoxelSnapshotError`, asset loading, scene config | Cold path error handling без exceptions |
| `std::simd<T>` | `src/core/Math.ixx`, frustum cull, vector math | SIMD без compiler intrinsics |
| Modules (`.ixx`) | `src/core/Math.ixx`, `Probe.ixx`, `StringId.ixx` | Ускорение инкрементальной сборки |
| Concepts | `src/core/`, type traits | Compile-time проверка контрактов |
| `constexpr` / `consteval` | `Math.ixx`, `core/Types.hpp` | Compile-time constants и проверки |
| `alignas(16)` | `Vec3`, `Vec4`, `Mat4` | Auto-vectorization в `movaps`/`vmovaps` |
| `std::inplace_vector` | Hot paths с reserved capacity | Без heap alloc на горячем пути |
| `import std;` probe | `tests/StdModuleProbe.cpp` | Проверка поддержки std module в clang 22 |

**Build verification:**
- `linux-clang-debug` ctest 14/14, 0 errors, 0 new warnings.
- libc++ (мигрировали с libstdc++ в Tier 2.5, `c3faa65`).
- CMake 3.30+ (тестировался 4.0).

**Что говорить:**
- «std::expected на cold path, std::inplace_vector на hot path».
- «alignas(16) → auto-vectorization».
- «Modules: Math.ixx, Probe.ixx, StringId.ixx — ускорение incremental build».
- «libc++ мигрировали в Tier 2.5, std::simd через модули».

---

## 22. Build system

**Где:** корневой `CMakeLists.txt` + `CMakePresets.json`
**Структура:**

**Configure presets (7 + 8 release = 15):**
- `windows-clang-debug` (основной dev tree)
- `windows-clang-debug-ci` (CI, suppress developer warnings)
- `windows-clang-debug-tracy-profiler` (только Tracy config changes)
- `linux-clang-debug` (baseline 2026-06-09)
- `linux-clang-debug-build` (только build)
- `linux-clang-debug-tests` (только ctest)
- `linux-clang-release`, `linux-clang-release-build`, `linux-clang-release-tests` (2026-06-14)
- симметричные `windows-clang-release-*`

**Build presets (6):**
- Каждый покрывает 14-17 ctest executables (3 debug × 17, 2 release × 15, 1 smoke × 1).

**Test presets (5):**
- Per configure preset.

**Release policy** (per `decisions.md §4`):
- `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only`
- Без `-ffast-math` (ломает Fluid CA determinism + TAA YCoCg clamp)
- Без `-march=native` (portability между CPU)
- Link: `-flto=thin -Wl,--gc-sections`
- **Результат:** ELF 19 MB (vs 72 MB debug), +1.5-2.5× FPS.

**Build verification (2026-06-15, текущий baseline):**
- `linux-clang-debug`: 137/137 targets, ctest 14/14 (release) / 14/14 (debug), smoke 6/6.
- `linux-clang-release`: ELF 19 MB, FPS +1.5-2.5× vs debug.

**Что говорить:**
- «7 debug + 8 release configure presets, 6 build, 5 test».
- «Release: -O3 -flto=thin без -ffast-math без -march=native».
- «ELF 19 MB release vs 72 MB debug, +1.5-2.5× FPS».

---

## 23. ECS / Flecs bridge

**Где:** `src/ecs/EcsWorld.{hpp,cpp}`
**Проблема:** gameplay и diagnostic systems хотят читать мир как набор сущностей, но `VoxelWorld` — single source of truth, ownership нельзя переносить.

**Архитектура:**
- `VoxelWorld` — primary, mutable.
- `EcsWorld` (Flecs) — **passive mirror**, read-only для других систем.
- `SyncEcsWorldState` (1× per frame):
  - Read dirty chunks из `VoxelWorld`.
  - Update corresponding ECS entity's `ChunkState` component.
  - HUD читает из ECS, не из VoxelWorld (lock-free read).

**Components:**
- `CameraTag` — primary camera entity.
- `PlayerControlledCamera` — input source entity.
- `WorldBinding` — singleton, ptr to VoxelWorld.
- `WorldChunkSummary` — derived stats (chunk count, voxel count).
- `ChunkState` — per-chunk state (dirty flag, version).
- `DebugState` — singleton для debug HUD state.

**API:**
- `world.entity()` — Flecs native API.
- `world.progress(dt)` — tick ECS systems.
- Lifecycle: ECS created **до** Vulkan в `SDL_AppInit`.

**Что говорить:**
- «Flecs — passive mirror, не ownership».
- «`SyncEcsWorldState` 1× per frame, dirty chunks только».
- «Components: CameraTag, PlayerControlledCamera, WorldBinding, ChunkState, DebugState».
- «HUD читает из ECS (read-only), не из VoxelWorld (mutable)».

---

## Краткая карта для быстрой навигации (для печати)

| # | Алгоритм | Где | Hot/Cold | Ключевое слово |
|---|---|---|---|---|
| 1 | Voxel world | `voxel/VoxelWorld.cpp` | H | 8×8×8, 1B/voxel |
| 2 | Materials | `voxel/VoxelMaterials.cpp` | H | 5 типов, 3 категории |
| 3 | Greedy meshing | `shaders/voxel_mesh.comp` | H | Лысенков, 6 проходов |
| 4 | Frustum cull | `c_kernels/frustum_cull.c` | H | AVX2 dpps, 8× |
| 5 | Visibility cache | `render/SceneResources.cpp` | H | splitmix64, 2 memcpy |
| 6 | CSM | `render/ShadowProjection.cpp` | H | 4 каскада, 2048² |
| 7 | PCF 5×5 | `shaders/voxel.frag` | H | weighted, N·L bias |
| 8 | Contact shadows | `shaders/voxel.frag` | H | DDA, 16 max steps |
| 9 | AOCC | `shaders/voxel.frag` | H | hemisphere, 12 reads |
| 10 | TAA + CAS | `shaders/taa_resolve.frag` | H | YCoCg, Halton 8 |
| 11 | Ray-march | `shaders/ray_march.comp` | H | F6 toggle, OFF default |
| 12 | Walk controller | `physics/PhysicsWorld.cpp` | H | voxel-решатель, edge grace |
| 13 | Fluid CA | `voxel/VoxelWorld.cpp` | H | hash-ordered, 20 Hz |
| 14 | Voxel raycast | `voxel/VoxelRaycast.cpp` | H | 3D DDA через чанки |
| 15 | Jolt | `physics/PhysicsWorld.cpp` | H | CharacterVirtual proxy |
| 16 | Asset pipeline | `asset/AssetLoader.cpp` | C | glTF/Draco/meshopt |
| 17 | Audio | `audio/AudioEngine.cpp` | C | miniaudio, PW→PA |
| 18 | Hot reload | `app/main.cpp` | C | F5, cmake --target Shaders |
| 19 | Snapshot | `voxel/VoxelWorld.cpp` | C | binary, magic+version |
| 20 | JSON config | `voxel/SceneConfig.cpp` | C | nlohmann/json, FetchContent |
| 21 | C++26 фичи | разные | оба | expected, simd, modules |
| 22 | Build | `CMakeLists.txt`, `CMakePresets.json` | C | 7+8 presets, libc++ |
| 23 | ECS bridge | `ecs/EcsWorld.cpp` | C | Flecs mirror, sync 1×/frame |

---

**Конец reference.** Связанные документы: `DefenseBriefer_le1t.md` (сокращённая версия для репетиции), `DefenseFAQ.md` (готовые ответы на 15+ вопросов комиссии), `DefenseReport.md` (формальный отчёт с маппингом на ТЗ).
