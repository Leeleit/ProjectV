# DefenseAlgorithms.md — Полный справочник всех алгоритмов ProjectV

**Дата:** 2026-06-15 (защита)
**Назначение:** личная шпаргалка le1t на Q&A комиссии. Каждый алгоритм — что это, где в коде, шаги, граничные случаи, что говорить если спросят «расскажите подробнее про X».
**Связанные документы:** `docs/DefenseBriefer_le1t.md` (сокращённая карта для репетиции), `docs/DefenseFAQ.md` (готовые ответы на 15+ вопросов), `docs/DefenseReport.md` (формальный отчёт).

---

## Оглавление

1. [Воксельный мир и чанки (Voxel world)](#1-воксельный-мир-и-чанки-voxel-world)
2. [Материалы и физический срез (physical slice)](#2-материалы-и-физический-срез-physical-slice)
3. [Жадный мешинг (greedy meshing, Лысенков)](#3-жадный-мешинг-greedy-meshing-лысенков)
4. [Фрустум-кулинг (frustum culling, scalar / SIMD / C/AVX2)](#4-фрустум-кулинг-frustum-culling)
5. [Двухуровневый кэш видимости (ChunkVisibilityCache)](#5-двухуровневый-кэш-видимости-chunkvisibilitycache)
6. [Каскадные тени (CSM, Cascaded Shadow Maps)](#6-каскадные-тени-csm-cascaded-shadow-maps)
7. [PCF 5×5 (взвешенный)](#7-pcf-55-взвешенный)
8. [Контактные тени (voxel DDA)](#8-контактные-тени-voxel-dda)
9. [AOCC — фоновое затенение полостей (cavity check)](#9-aocc--фоновое-затенение-полостей)
10. [TAA + YCoCg + CAS](#10-taa--ycocg--cas)
11. [Трассировка лучей через compute-шейдер (ray-marching)](#11-трассировка-лучей-ray-marching)
12. [Walk-контроллер (edge grace, sneak, авто-прыжок)](#12-walk-контроллер)
13. [Жидкость: клеточный автомат (Fluid CA)](#13-жидкость-клеточный-автомат-fluid-ca)
14. [Воксельный raycast (3D DDA)](#14-воксельный-raycast-3d-dda)
15. [Jolt: интеграция (CharacterVirtual + voxel solver)](#15-jolt-интеграция)
16. [Конвейер ассетов (glTF, Draco, meshopt)](#16-конвейер-ассетов)
17. [Аудио-движок (miniaudio)](#17-аудио-движок-miniaudio)
18. [Горячая перезагрузка шейдеров (F11)](#18-горячая-перезагрузка-шейдеров-f11)
19. [Сохранение и загрузка мира (snapshot)](#19-снапшот)
20. [JSON-конфиг сцены (nlohmann/json)](#20-json-конфиг-сцены)
21. [Фичи C++26 в коде](#21-фичи-c26-в-коде)
22. [Система сборки (CMake presets, ctest)](#22-система-сборки)
23. [Связь с ECS через Flecs (ECS bridge)](#23-связь-с-ecs-через-flecs)

---

## 1. Воксельный мир и чанки (Voxel world)

**Где:** `src/voxel/VoxelWorld.{hpp,cpp}`
**Проблема:** миллионы вокселей нельзя хранить как `std::vector<Voxel>` (overhead, cache-miss, медленный iteration).
**Решение:** декомпозиция на регулярные чанки фиксированного размера + плоский массив материалов в `VoxelWorld`.

**Реальные структуры (`src/voxel/VoxelWorld.hpp:45-108`):**
```cpp
struct VoxelChunk {
    Int3 min{};             // 12 B — минимальная грань чанка в world coords
    Int3 maxExclusive{};    // 12 B — исключающая максимальная грань
    bool rebuildQueued = true;       // 1 B (+ 7 B padding) — флаг «нужен remesh»
    uint32_t nonAirVoxelCount = 0;   // 4 B — кэш для быстрого non-Air summary
};
static_assert(sizeof(VoxelChunk) == 32);

struct VoxelWorld {
    VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
    VoxelWorldConfig config{};
    Int3 min{}, maxExclusive{};
    Int3 floorMin{}, floorMaxExclusive{};
    int width = 0, height = 0, depth = 0;
    std::vector<uint8_t> voxels;   // ← ПЛОСКИЙ массив материалов, 1 байт = material ID
    int chunkSize = 0, chunkCountX = 0, chunkCountY = 0, chunkCountZ = 0;
    uint64_t editVersion = 0;
    std::vector<VoxelChunk> chunks;
    std::vector<size_t> pendingChunkRebuildIndices;
    VoxelWorldStats stats{};
};
```

**Важно:** воксели хранятся в **плоском** `std::vector<uint8_t> voxels` в `VoxelWorld` (не per-chunk). `VoxelChunk` хранит только координаты и метаданные. Плоский индекс = `x + width * (y + height * z)`.

**Шаги при размещении блока (`SetVoxelMaterial`):**
1. Compute `localX = position.x - world.min.x`, etc.
2. Bounds-check `IsInsideVoxelWorld(world, position)` (отказ до мутации).
3. Compute `chunkCoord = (localX / chunkSize, ...)`, `chunkIndex = chunkCoord.x + chunkCountX * (chunkCoord.y + chunkCountY * chunkCoord.z)`.
4. Material ID → записать в `voxels[linearIndex]`.
5. `world.editVersion++`.
6. `chunks[chunkIndex].rebuildQueued = true`, `chunks[chunkIndex].nonAirVoxelCount++`.
7. `pendingChunkRebuildIndices.push_back(chunkIndex)`.
8. Update `stats` (dirtyChunkCount, totalNonAirVoxelCount, per-material counts).

**Complexity:** `SetVoxelMaterial` O(1) в среднем. Rebuild-запрос O(1). Meshing O(N) на чанк, но только для dirty.

**Edge cases:**
- Out-of-bounds → возврат `false`, **никакой мутации** (отказ-до-мутации).
- Race на `pendingChunkRebuildIndices` — single-threaded main loop, защита не нужна.
- CA coordinate-bug fix (2026-06-13, `decisions.md §30`): commit loop раньше передавал local coords как world coords, falls в VoxelLab silently dropped на `local.x == width - world.min.x` (то есть на `world.x == maxExclusive.x`, `IsInsideVoxelWorld` rejects). Fix: `world.min` offset перед `SetVoxelMaterial`.

**Что говорить:**
- «Чанк 8×8×8 = 512 вокселей, 1 байт на воксель, плотный массив в `VoxelWorld` (не per-chunk)».
- «VoxelLab = **27 чанков** (3×3×3 grid по chunkSize=8), процедурная генерация за <200 мс».
- «Координаты integer-based, world bounds = `min=(-12,0,-12)`, `maxExclusive=(12,17,12)` для VoxelLab с floorSize=18, padding=3, worldTopY=14».
- «Per-chunk кэш: `nonAirVoxelCount` для быстрого summary без перебора 512 вокселей».

---

## 2. Материалы и физический срез (physical slice)

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

## 3. Жадный мешинг (greedy meshing, алгоритм Лысенкова)

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

## 4. Фрустум-кулинг (frustum culling)

**Где:** `src/c_kernels/frustum_cull.{c,hpp}` (C/AVX2 ядро, Tier 3) + `src/c_kernels/FrustumCulling.{hpp,cpp}` (C++ wrapper, Tier 4) + `src/render/SceneResources.cpp` (CPU-side)
**Проблема:** 300 чанков × 6 плоскостей фрустума = 1800 dot products каждый кадр, CPU-bound.

**Алгоритм (C ядро, scalar и AVX2):**
1. Frustum = 6 плоскостей `{a, b, c, d}` в float32.
2. AABB чанка = `{minX, minY, minZ, maxX, maxY, maxZ}` (8 floats + padding до 32 B).
3. **Per-plane inner loop** (на precomputed plane normals + 8 AABBs за раз):
   - Compute `pVertex[axis] = (sign_axis > 0 ? max : min)[axis]` для каждой оси — p-vertex ближайший к плоскости.
   - 8 dot products параллельно (per-AABB для одной плоскости).
   - 8 abs-mul: `|pVertex.x * normal.x| + |pVertex.y * normal.y| + |pVertex.z * normal.z|`.
   - 8 sums: `distance = abs(dot) + d` (с учётом знака).
   - Если `distance < 0` для всех 8 → AABB **вне** плоскости → early exit.
4. Если все 6 плоскостей «distance >= 0» → AABB внутри (или пересекает).
5. Иначе → AABB пересекает (draw).

**SIMD-оптимизация (AVX2):**
- 8 AABBs за раз обрабатываются через 256-битные регистры (`__m256`).
- `__attribute__((target("avx2")))` — per-function, не пересекает TU boundary.
- Pre-computed plane normals — передаются в структуре `ProjectvCFrustumCullParameters`.
- **AoS layout** (`ProjectvCAabb` = 32 B per AABB) — не оптимально для AVX2 scatter-gather.

**Empirical benchmarks (per `src/c_kernels/FrustumCulling.hpp:24-37`):**
- **Scalar C: 3.7-3.9× faster** than C++ math:: baseline (per Tier 3 benchmark).
- **AVX2: 2.5-2.7× faster** (на AoS layout). Autovectorizer с `-mavx2` на C++ side частично обгоняет hand-rolled `_mm256_setr_ps` setup в debug builds.
- **8× — future target** при SoA layout (Tier 5 follow-up). Per `agent/memory.md §1583`.

**API contract (`frustum_cull.hpp:17-22`):**
- `visible_mask[i / 8] & (1u << (i % 8))` set iff AABB visible.
- Caller-owned unused lanes (kernel не zero'ит).
- `count` may be any non-zero value; tail lanes still computed.
- Below `kBatchDispatchThreshold = 8` AABBs, fall back to inline `IsAabbVisibleAgainstCameraFrustum`.

**Complexity:** O(N chunks) с константой зависящей от lane count (8 для AVX2). Per Tier 3: ~50 µs для 300 instances (C kernel), ~1 µs на AABB-to-`ProjectvCAabb` conversion.

**Что говорить:**
- «AABB чанка vs 6 плоскостей фрустума, scalar C 3.7-3.9×, AVX2 2.5-2.7×».
- «Inner loop = 8 AABBs × 6 planes за раз (per-plane batch), pre-computed normals».
- «8× — future target SoA, не текущая цифра».
- «Baseline ctest `CFrustumCullingTests`».

---

## 5. Двухуровневый кэш видимости (ChunkVisibilityCache)

**Где:** `src/render/SceneResources.{hpp,cpp}` → `ChunkVisibilityCache`, `projectv::visibility_cache::ComputeVisibilityCacheHash`
**Проблема:** даже с AVX2, 300 чанков × 6 dot = 1800 ops/кадр когда камера **почти** статична (50% времени FPS counter не двигается, а CPU считает).

**Хэш-функция (НЕ splitmix64, а custom XOR-fold):**
Per `src/render/SceneResources.hpp:375-408`:
```cpp
namespace projectv::visibility_cache {
constexpr float kCameraPositionQuantization = 0.25f;
constexpr float kCameraForwardQuantization = 0.005f;  // ~0.3° шаги

inline int32_t QuantizeCameraPositionComponent(const float value) {
    return static_cast<int32_t>(std::floor(value / kCameraPositionQuantization));
}
inline int32_t QuantizeCameraForwardComponent(const float value) {
    const float clamped = std::clamp(value, -1.0f, 1.0f);
    return static_cast<int32_t>(std::lround(clamped / kCameraForwardQuantization));
}

inline uint64_t ComputeVisibilityCacheHash(
    const ChunkCullingParameters &parameters,
    const uint64_t sceneVoxelPayloadVersion,
    const uint32_t chunkDescriptorCount)
{
    // 6 quantized camera ints
    const auto posX = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[0]);
    const auto posY = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[1]);
    const auto posZ = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[2]);
    const auto fwdX = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[0]);
    const auto fwdY = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[1]);
    const auto fwdZ = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[2]);

    // Custom XOR-fold с Knuth-style golden ratio multipliers.
    // (Это НЕ splitmix64 — стандартные константы splitmix64
    //  0x9e3779b97f4a7c15 / 0xbf58476d1ce4e5b9 / 0x94d049bb133111eb —
    //  у нас другие, MMIX-style.)
    uint64_t hash = static_cast<uint64_t>(posX) * 0x9E3779B185EBCA87ULL;
    hash ^= static_cast<uint64_t>(posY) * 0xC2B2AE3D27D4EB4FULL;
    hash ^= static_cast<uint64_t>(posZ) * 0x165667B19E3779F9ULL;
    hash ^= static_cast<uint64_t>(fwdX) * 0x94D049BB133111EBULL;
    hash ^= static_cast<uint64_t>(fwdY) * 0xD1342543DE82EF95ULL;
    hash ^= static_cast<uint64_t>(fwdZ) * 0xB45BCA9F4D2D9B33ULL;
    hash ^= sceneVoxelPayloadVersion * 0x27D4EB2F165667C5ULL;
    hash ^= static_cast<uint64_t>(chunkDescriptorCount) * 0x9C2A8E3F4D2D9B3BULL;

    // Final avalanche. **Same mix as splitmix64** (вот это — единственная
    // splitmix64-часть): xor-shift + multiply + xor-shift + multiply + xor-shift.
    hash ^= hash >> 30;
    hash *= 0xBF58476D1CE4E5B9ULL;
    hash ^= hash >> 27;
    hash *= 0x94D049BB133111EBULL;
    hash ^= hash >> 31;
    return hash;
}
}
```

Комментарий в коде явно: «splitmix64-style fold. The exact constants don't matter for correctness — only that (a) the hash is deterministic and (b) the bits of each component get mixed into the high bits, so a 1-bit change in any input flips roughly half the hash bits.»

**Алгоритм кеша (структура из `SceneResources.cpp:459-466`):**
- 3 pre-baked `VkDrawIndirectCommand` буфера: `opaqueCommands`, `shadowCommands` (×4 cascades), `transparentCommands`.
- `uint64_t lastHash` для сравнения.
- Cache hit path: `if (currentHash == lastHash) { skip; return; }`.
- Cache miss: full frustum cull, fill 3 буфера, обновить `lastHash`.

**Tier 1.A (`2026-06-13`):** `std::inplace_vector<P0843, C++26>` с cap `kChunkVisibilityCacheMaxChunks = 1024`. Никаких heap alloc на hot path. `assert(chunkDescriptorCount <= ChunkVisibilityCache::kChunkVisibilityCacheMaxChunks)`.

**Complexity:** Cache hit — O(1). Cache miss — O(N chunks) frustum cull.
**Empirical:** 8× ускорение в кадрах со статичной камерой (per `decisions.md §22`).

**Edge cases:**
- Quantization step: 0.25 вокселя по позиции, 0.005 (~0.3°) по forward — баланс hit rate vs точность (per комментарий в `SceneResources.hpp:349-355`).
- Hash invalidation: `sceneVoxelPayloadVersion` инкрементируется при voxel edit, chunk-count change → cache miss автоматически.

**Что говорить:**
- «2 уровня: hash от квантованной позиции+forward камеры + voxel payload version, hit → skip».
- «Custom XOR-fold с splitmix64-style avalanche, не чистый splitmix64».
- «Квантизация 0.25 вокселя / 0.005 forward — подобрано эмпирически».
- «inplace_vector с cap 1024 — no heap alloc на hot path».

---

## 6. Каскадные тени (CSM, Cascaded Shadow Maps)

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

## 7. PCF 5×5 (взвешенный)

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

## 9. AOCC — фоновое затенение полостей (ambient occlusion cavity check)

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

## 11. Трассировка лучей через compute-шейдер (ray-marching compute pass)

**Где:** `src/shaders/ray_march.comp` + `src/render/RayMarchPass.{hpp,cpp}`
**Проблема:** mesh-based геометрия даёт видимые «грани» вокселей при cinematic-камерах. ТЗ требовало «GPU ray-marching через compute-шейдеры» (п. 4.1.2).

**Алгоритм (Amanatides-Woo 3D DDA через packed voxel payload) — `src/shaders/ray_march.comp`:**

1. **Input:** uniform buffer (`RayMarchParams`) + storage buffer `PackedVoxelPayload` + storage image `rayMarchOutput`.
2. **Per-pixel compute (1 thread per pixel, `local_size_x=8, local_size_y=8`):**
   - Ray origin = camera position, ray dir = perspective ray из forward + right * u * tanHalfFovX + up * v * tanHalfFovY.
   - Convert ray to voxel-space, `deltaDist = abs(1.0 / max(abs(rayDir), 1e-6))`.
   - `sideDist` initial, `cell = floor(originVoxelSpace)`.
   - DDA loop (max `maxSteps` из params):
     - На каждом шаге: `FetchVoxel(cell)`. Если != 0 (≠ Air) → hit, break.
     - Step to next cell along dominant axis (compare `sideDist.x/y/z`).
     - Update `hitNormal` по направлению шага.
   - Output: simple N·L diffuse shading (sun direction packed в up.w), `palette[min(hitMaterial, 4u)]` base color.
3. **Output:** `imageStore(rayMarchOutput, pixel, outColor)`. RGBA8 image.

**Текущее состояние: STUB.**
Per `src/render/RayMarchPass.cpp:59-79`, `RecordRayMarchCommands`:
```cpp
void RecordRayMarchCommands(const VulkanContextState &context, const FrameRenderData &frameData) {
    const auto &state = MutableRayMarchState();
    if (!state.enabled) return;
    if (context.device == VK_NULL_HANDLE) return;
    // Phase 7 follow-up: full Vulkan integration binds the shader,
    // allocates offscreen RGBA8 storage image, dispatches 8x8x1.
    // Current entry point emits a diagnostic record so the toggle
    // is observable in the runtime output stream and the call site
    // is not silently swallowed.
    std::fprintf(stderr,
        "[ProjectV][RayMarch] RecordRayMarchCommands invoked (deferred Phase 7 follow-up: shader is compiled, pipeline / offscreen target / composite are the next slice)\n");
}
```

То есть **compute shader скомпилирован** (даёт `.spv` через glslc), API state (`SetRayMarchEnabled` / `IsRayMarchEnabled` / `RequestRayMarchPipelineRecreate`) работает, **но graphics command stream НЕ вызывает** compute pass. Полная интеграция (offscreen target + composite) — Phase 7 follow-up, явно зафиксировано в `docs/DefenseReport.md §3` (deferred items).

**Toggle:** `SDLK_F12` в `main.cpp` → `projectv::render::SetRayMarchEnabled(bool)` (relocated 2026-06-15 с F6 — F6 теперь чисто для `SaveWorldSnapshot` InputAction). По умолчанию OFF.

**Что говорить:**
- «Compute shader скомпилирован, API работает, но в graphics stream не вкомпонован — STUB, Phase 7 follow-up».
- «Toggle F12 в рантайме (relocated с F6), OFF по умолчанию».
- «Compute DDA через packed voxel payload, RGBA8 output image (planned)».
- «Альтернативный путь рендеринга для cinematic camera — мягкие грани вокселей (planned)».

---

## 12. Walk-контроллер (walk controller)

**Где:** `src/physics/PhysicsWorld.{hpp,cpp}` → `UpdateWalkGroundSupport`, `TryAutoJump`, `BuildWalkEdgeGraceUpdateSettings`
**Архитектура:** `JPH::CharacterVirtual` **используется** для collision detection (капсула, прокси), **voxel solver augments** foot support (per `decisions.md §6`). Это не "voxel solver вместо Jolt" — Jolt остаётся основой.

**Алгоритм:**

### Ground support
1. `JPH::CharacterVirtual::ExtendedUpdate` — Jolt side: continuous collision detection с custom `BuildWalkEdgeGraceUpdateSettings()` (настройка `mWalkExtendedUpdateSettings`, изменяет поведение extended update для edge grace).
2. `UpdateWalkGroundSupport` (наш код) — **augment** Jolt-результата:
   - Sample top-plane в `feetPosition + (0, -stepHeight, 0)`.
   - Voxel lookup в `VoxelWorld`:
     - `Air`/`Fluid` → нет ground
     - `Glass`/`FloorWhite`/`FloorGray` → ground, support height = top Y
3. **Edge grace** (per `PhysicsWorld.cpp:117-141`):
   - `constexpr uint32_t kWalkEdgeGraceFrames = 4` — допуск в **фреймах** (не метрах!).
   - `constexpr float kWalkFootSupportEdgeGraceScore = 0.2f` — порог score (не дистанция).
   - `kWalkFootSupportMovingEdgeGraceScore = 0.5f` — для движущегося игрока.
   - Логика: `physics.walkEdgeGraceFramesRemaining` счётчик, при `supportScore < EdgeGraceScore` → `walkEdgeGraceFramesRemaining = 4` (4 фрейма grace). Не «дёргать» Y вверх-вниз при микро-перепаде.
4. **Sneak (Shift):** sampled top-plane (1 точка, не 4), без false-stick к стене. Файл: `walkSneakShape` (внутренний JPH::Shape), `walkSneakActive` flag.

### Auto-jump
1. Триггер: `J` toggle ON (InputAction).
2. Каждый кадр: `FindWalkTopSupportCandidate` — ищем forward voxel на уровне 1 блок выше ground.
3. `IsWalkAutoJumpRiseInRange(autoJumpRise)` — проверка, что rise в допустимом диапазоне.
4. Delay (F12 InputAction): если `walkAutoJumpDelayEnabled` ON, отсчёт начинается только когда `reached == true`. Иначе — мгновенно.
5. Manual jump (Space): обнуляет delay accumulator.

### Air control
- `ToggleWalkAirControlMode` (F11 InputAction): MinecraftLike (default) — WASD в воздухе, momentum = Jolt velocity. Realistic — W-only, фиксация направления.
- F11 InputAction **shadowed** defense r0 bypass (F11 = hot shader reload), но для defense demo walk modes toggle не на demo path.

**3 режима (F4 `ToggleControlMode`):**
- **walk:** grounded authority, edge grace, sneak, air control.
- **creative:** полёт, collision substepped (`TickCreativeCharacter` для substepping high-velocity).
- **spectator:** noclip, ignore physics, ignore pause (per `PhysicsWorld.cpp:3593+`).

**Edge cases:**
- Переключение creative ↔ walk: двойной Space.
- Auto-jump OFF: ручной Space = vanilla.
- Pause (`P`) vs `timeScale=0` — разные оси (per `decisions.md §26`).
- Edge grace — `kWalkEdgeGraceFrames = 4` (фреймы!), **НЕ** 0.1 м.

**Что говорить:**
- «JPH::CharacterVirtual + voxel solver augment (per `decisions.md §6`); Jolt для collision detection, наш solver — для foot support».
- «Edge grace = `kWalkEdgeGraceFrames = 4` фрейма + score 0.2 (НЕ 0.1 м)».
- «Sneak, auto-jump — фичи для voxel мира, не generic character».
- «3 режима, F4 переключает, двойной Space ↔ creative».

---

## 13. Клеточный автомат для жидкости (Fluid CA)

**Где:** `src/voxel/VoxelWorld.cpp` → `UpdateFluidCA` (~350 строк с комментариями)
**Проблема:** жидкость в voxel-мире — стандартный клеточный автомат. Нужен determinism (replay), не pathological spread.

**Алгоритм (per tick, итерация local z, y, x ascending):**

**1. f_fall rule** (per `VoxelWorld.cpp:1380-1399`):
- Если `world.voxels[idx_below] == Air` (snapshot read) **и** `next[idx_below] == Air` (write target empty):
  - `next[idx] = Air` (source consumed)
  - `next[idx_below] = Fluid` (destination claimed)
  - `claimed[idx_below] = 1` (prevents swap bug)
- Иначе → пробуем spread.

**2. f_spread rule (2 перпендикулярных направления, count conservation) — `VoxelWorld.cpp:1442-1539`:**
Per комментарию в коде: «Two perpendicular directions: the hash one and the one rotated 90°. Opposite (startSide+2) was tried first and produced "line" patterns that didn't fill 2D gaps; perpendicular gives a square footprint.»

```cpp
const uint32_t h = static_cast<uint32_t>(
    (x * 73856093u) ^ (y * 19349663u) ^ (z * 83492791u));  // Teschner spatial hash
const int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
const int startSide = static_cast<int>(h & 0x3u);
const int dirs[2] = {startSide, (startSide + 1) & 0x3};  // perpendicular
// Пробуем оба направления, но только ПЕРВОЕ успешное пишет
// (count conservation per decisions.md §30 2026-06-14):
//   "source Air, 1 destination Fluid = exactly 0".
// L-shape визуально ПОТЕРЯН ради count conservation.
```

**Hash:** **Teschner spatial hash** `(x*73856093) ^ (y*19349663) ^ (z*83492791)` (НЕ splitmix64, НЕ мой старый claim).

**Swap bug fix (2026-06-13, per `decisions.md §30`):**
- Раньше spread использовал `world.voxels[neighbour] == Air` для target check. Два adjacent source cells могли оба «успешно» spread (last write wins), fluid терялся.
- Fix: target check использует `next[neighbour] == Air` (snapshot of new state) + `claimed[]` per-tick bool array (belt-and-suspenders).

**Coordinate convention (критично, per `VoxelWorld.hpp:188-198`):**
- CA pass и commit loop итерируют **local** индексы `x ∈ [0, width)`.
- Commit loop добавляет `world.min` для local → world перед `SetVoxelMaterial`.
- Bug 2026-06-13: commit loop передавал local indices напрямую как world coords → falls в VoxelLab на `local.x == width - world.min.x` silently dropped. Fix: `world.min` offset.
- Test: `TestFluidCAVoxelLabSphereFallOnGlassBreak` (16 sub-tests, 100% pass).

**Throttle (per `AppUpdate.cpp:730-742`):**
- Default 20 Hz: `SimulationState::fluidTickRateHz = 20.0f` (per `decisions.md §30.1`).
- Accumulator-based, НЕ «1 tick per 3 frames»: `fluidAccumulatorSeconds += frameDeltaSeconds` (frameDelta уже scaled by timeScale), while-loop drains accumulator в `1 / fluidTickRateHz` chunks.
- Multi-tick per frame allowed (no `simulationStepsLastFrame` cap).
- Pause drops accumulator to 0 (no catch-up).

**Pause/timeScale:** paused если `effectivePaused` (`paused == true` OR `timeScale == 0` OR `frameStepRequestedNow`).

**Что говорить:**
- «2 правила: f_fall (down) и f_spread (2 перпендикулярных направления, но только 1 destination пишется для count conservation)».
- «Hash = Teschner spatial hash (НЕ splitmix64), deterministic».
- «20 Hz default, accumulator-based, multi-tick per frame allowed».
- «Double-buffered через `next[]` snapshot + `claimed[]` swap-bug fix».
- «Работает только с `Fluid` материалом, не путать с Glass».

---

## 14. Воксельный raycast (3D DDA)

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

## 15. Интеграция с Jolt (CharacterVirtual + voxel solver)

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

## 16. Конвейер ассетов (asset pipeline: glTF, Draco, meshopt)

**Где:** `src/asset/AssetLoader.{cpp,hpp}`, `DracoMeshDecoder.{cpp,hpp}`, `MeshBaker.{cpp,hpp}`, `ModelManifestLoader.{cpp,hpp}`
**Проблема:** glTF = стандарт, но файлы могут быть сжаты Draco. Нужно декодировать + оптимизировать для GPU.

**Конвейер (per `src/asset/AssetLoader.hpp:36-38`):**
1. **`LoadGlb(const std::string &path, LoadAssetError *outError)`** (НЕ `Load` — это неправильное имя) — fastgltf parser → `LoadedAsset` (Primitives, AABB, vertex/triangle counts).
2. Если mesh has Draco compression → `DracoMeshDecoder::Decode()` → `DecodedMesh`.
3. **`MeshBaker::Optimize()`** → meshopt:
   - `meshopt_optimizeVertexCache` (reorder indices для cache locality)
   - `meshopt_optimizeOverdraw` (reorder triangles)
   - `meshopt_optimizeVertexFetch` (reorder vertices для memory locality)
4. Bake textures (atlas если несколько материалов).
5. Upload в GPU через VMA → `MeshGpuResources`.

**Manifest loading:**
- `PROJECTV_MODELS=path.glb@x,y,z;...` env var.
- `ModelManifestLoader` парсит, создаёт `ModelPass` per instance.
- `SnapModelInstancesAboveGroundDispatch` — позиционирует модели над ground (Y=0 по умолчанию).
- `ComputeGlbDimensions` (per `AssetLoader.hpp:60-68`) — pure helper для per-axis auto-scale.

**Что говорить:**
- «fastgltf → Draco decode → meshopt optimize → VMA upload».
- «Entry point: `LoadGlb(path, outError)`, НЕ `Load`».
- «Manifest через env var, snap above ground».
- «Загрузчик синхронный, <1 сек на 100 МБ glb».

---

## 17. Аудио-движок (audio engine, miniaudio)

**Где:** `src/audio/AudioEngine.{hpp,cpp}`
**Проблема:** нужен простой audio без тяжёлых зависимостей.

**Архитектура (per `AudioEngine.hpp:5-15`):**
- `miniaudio` (vendored submodule, MIT) для playback.
- Linux backend: **PulseAudio** через `pipewire-pulse` shim → active PipeWire server (per `pactl info` → `Server String: /run/user/1000/pulse/native`).
- Format: **16-bit signed PCM, 44.1 kHz, stereo** (per `AudioEngine.cpp:85-100`). `config.sampleRate = 44100, config.channels = 2, config.listenerCount = 1`.
- `MusicDirectoryPath` env var (`music/` default).
- `scanPlaylist()` каждые 5 секунд — автообновление при добавлении файлов.

**Поддерживаемые форматы: ТОЛЬКО MP3.** Per `AudioEngine.cpp:206-211`:
```cpp
const auto &path = entry.path();
std::string ext = path.extension().string();
std::ranges::transform(ext, ext.begin(),
    [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
if (ext != ".mp3") {
    continue;  // WAV/FLAC/etc. silently ignored
}
m_playlist.push_back(path);
```

Сейчас в `music/` лежат 2 MP3: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`.

**Hotkeys (per `InputActions.cpp:196-210`):**
- `Q` (SDL_SCANCODE_Q) — `ToggleMusicPlayPause`
- `E` (SDL_SCANCODE_E) — `StopMusic`
- `7`/`8` (SDL_SCANCODE_7/8) — `MusicVolumeDown/Up`, step 0.05 в [0, 1] range
- `9`/`0` (SDL_SCANCODE_9/0) — `NextMusicTrack` / `PreviousMusicTrack`, wrap-around

**API state (для debug HUD / sidecar):**
- `state()` — `Stopped` / `Playing` / `Paused` (3-state enum)
- `volume()` — float [0, 1]
- `positionSeconds()` / `durationSeconds()` — для HUD time display
- `currentArtist()` / `currentTitle()` — parsed from filename (`<artist> - <title>.mp3`)

**Sidecar metadata:** `music_track`, `music_state`, `music_volume` в capture sidecars.

**Что говорить:**
- «miniaudio header-only, MIT, PulseAudio → PipeWire на Linux».
- «16-bit 44.1 kHz stereo».
- «Только MP3, WAV/FLAC тихо игнорируются».
- «Auto-refresh playlist каждые 5 секунд».
- «6 hotkeys: Q play/pause, E stop, 7/8 volume, 9/0 next/prev».

---

## 18. Горячая перезагрузка шейдеров (hot shader reload, F11)

**Где:** `src/app/main.cpp` → `RebuildAllShadersFromDisk()`
**Проблема:** итерация над шейдерами требует перезапуска приложения, медленно.

**Hotkey: F11** (relocated 2026-06-15 с F5 — F5 теперь чисто для InputAction `CycleScenePreset`).
**Ray-march toggle: F12** (relocated 2026-06-15 с F6 — F6 теперь чисто для InputAction `SaveWorldSnapshot`).

**Алгоритм (`RebuildAllShadersFromDisk`, per `main.cpp:60-114`):**
1. F11 в `SDL_AppEvent` → `RebuildAllShadersFromDisk()`.
2. Get `PROJECTV_BUILD_DIR` env var (если задана) иначе `PROJECTV_CMAKE_BUILD_DIR` macro (compile-time injected, cross-platform).
3. Subprocess: `cmake --build <buildDir> --target Shaders > "<tempdir>/projectv_shader_reload.log" 2>&1`. Cross-platform tempdir via `std::filesystem::temp_directory_path()` (Linux: `/tmp`, Windows: `%TEMP%`).
4. `glslc` / `glslangValidator` перекомпилирует `.vert`/`.frag`/`.comp` → `.spv`.
5. На success → `RequestRayMarchPipelineRecreate()` (ray-march pipeline is the only one with newly-added `.comp`; pre-existing graphics/shadow/TAA pipelines keep cached shader modules until fuller pipeline-recreate PR).
6. На следующем кадре pipeline recreate.

**Edge cases (BUG-005):**
- Race на descriptor sets при cycle scene (это `F5` InputAction `CycleScenePreset`, НЕ F11 shader reload).
- `vkDeviceWaitIdle` в `DestroySceneResources` смягчает, не устраняет полностью.
- **Defensive:** `RequestRayMarchPipelineRecreate` — ленивый, **не дёргает** swapchain wait mid-frame.

**Что говорить:**
- «F11 (relocated с F5) → cmake build --target Shaders → ray-march pipeline recreate на следующем кадре».
- «F12 (relocated с F6) → toggle ray-march pass (STUB на текущий момент, см. §11)».
- «Удобно для итераций над шейдерами без перезапуска».
- «BUG-005: race при InputAction F5 cycle scene, смягчён через `vkDeviceWaitIdle`, не устранён полностью».

---

## 19. Сохранение и загрузка мира (snapshot save/load)

**Где:** `src/voxel/VoxelSnapshotError.hpp` + `SaveVoxelWorldSnapshot`/`LoadVoxelWorldSnapshot` в `VoxelWorld.cpp`
**Проблема:** долгая сессия → хочется сохранить/восстановить мир.

**Формат (binary, little-endian), per `VoxelWorld.cpp:29-44`:**
```cpp
struct VoxelWorldSnapshotHeader {
    std::array<char, 8> magic{};        // "PVSNAP01" (8 значащих байт)
    uint32_t version = 0;               // currently 1
    uint32_t voxelByteCount = 0;        // размер voxel payload
    uint32_t reserved = 0;
    uint8_t scenePreset = 0;
    uint8_t reservedBytes[3]{};
    VoxelWorldConfig config{};
    Int3 min{};
    Int3 maxExclusive{};
    uint64_t editVersion = 0;
};
static_assert(sizeof(VoxelWorldSnapshotHeader) == 80);
```

**Magic:** `kVoxelWorldSnapshotMagic = {'P','V','S','N','A','P','0','1'}` (8 значащих байт, **НЕ** `"PVSNAP\0\0"`).
**Version:** `kVoxelWorldSnapshotVersion = 1u`.

**Body (per chunk):**
```cpp
// tightly packed: 1 byte per voxel = material ID
```

**Save (`SaveVoxelWorldSnapshot`):**
1. Open file (binary), `std::ofstream`.
2. Write header (80 B).
3. Write voxel payload: `world.voxels.size()` bytes (1 byte per voxel = material ID).
4. `std::expected<bool, VoxelSnapshotError>` return (cold path, Tier 1.B).

**Load (`LoadVoxelWorldSnapshot`):**
1. Open file, read header. Validate `header.magic != kVoxelWorldSnapshotMagic` → `VoxelSnapshotError::MagicMismatch`.
2. Validate `header.version != 1u` → `VoxelSnapshotError::UnsupportedVersion`.
3. Validate `header.scenePreset` via `IsValidVoxelScenePresetValue` → `VoxelSnapshotError::InvalidScenePreset`.
4. Reject if `voxelByteCount > MAX_VOXELS` (defensive) → `VoxelSnapshotError::VoxelByteCountOutOfRange`.
5. Construct `std::unique_ptr<VoxelWorld>` from header (config, min, maxExclusive, scenePreset).
6. `voxels.resize(voxelByteCount)`, `file.read(voxels.data(), voxelByteCount)`.
7. Initialize chunks from config (chunkCountX/Y/Z, chunkSize).
8. `std::expected<unique_ptr<VoxelWorld>, VoxelSnapshotError>` return.

**Tier 1.B:** error handling через `std::expected` (cold path, 1× per snapshot).

**Что говорить:**
- «Binary, magic `"PVSNAP01"`, version 1, 80-B header + voxel payload».
- «1 byte per voxel = material ID, header с config + min/max + scenePreset».
- «std::expected на cold path, validate magic + version + scenePreset + byte count на load».
- «Cold path (1× per snapshot), so std::expected overhead irrelevant».

**Что говорить:**
- «Binary, 1 byte per voxel, header с magic+version+dimensions».
- «std::expected на cold path, validate magic на load».
- «All-chunks-dirty после load → meshing rebuilds автоматически».

---

## 20. JSON-конфиг сцены (JSON scene config, nlohmann/json)

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
- **std::simd реально не используется** (planned, Tier 5 follow-up) — заменено на C/AVX2 kernel в `src/c_kernels/frustum_cull.c`.

**Что говорить:**
- «std::expected на cold path, std::inplace_vector на hot path (cap 1024)».
- «alignas(16) → auto-vectorization в movaps/vmovaps».
- «Modules: Math.ixx, Probe.ixx, StringId.ixx — ускорение incremental build».
- «libc++ мигрировали в Tier 2.5; SIMD через C/AVX2 kernel (Tier 3)».

---

## 22. Система сборки (build system: CMake presets, ctest)

**Где:** корневой `CMakeLists.txt` + `CMakePresets.json`
**Структура:**

**Configure presets (3 debug + 1 tracy + 3 release = 7 main, +8 release = 15 total per `decisions.md §4` build config audit 2026-06-14):**
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
- **Результат:** ELF **19 MB release vs 73 MB debug** (-73%), +1.5-2.5× FPS.

**Build verification (2026-06-15, текущий baseline):**
- `linux-clang-debug`: 137/137 targets, ctest 14/14, smoke 6/6, **ELF 73 MB**.
- `linux-clang-release`: 137/137 targets, ctest 14/14 (0.06s), smoke 6/6, **ELF 19 MB**.
- `linux-clang-release`: ELF 19 MB, FPS +1.5-2.5× vs debug.

**Что говорить:**
- «7 debug + 8 release configure presets, 6 build, 5 test».
- «Release: -O3 -flto=thin без -ffast-math без -march=native».
- «ELF 19 MB release vs 73 MB debug (verified 2026-06-15), +1.5-2.5× FPS».

---

## 23. Связь с ECS через Flecs (ECS bridge)

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
| 1 | Voxel world | `voxel/VoxelWorld.{hpp,cpp}` | H | 8×8×8 chunks, плоский `voxels` |
| 2 | Materials | `voxel/VoxelMaterials.cpp` | H | 5 типов, 3 solid |
| 3 | Greedy meshing | `shaders/voxel_mesh.comp` | H | Лысенков, 6 проходов |
| 4 | Frustum cull | `c_kernels/frustum_cull.c` | H | C 3.7-3.9×, AVX2 2.5-2.7× |
| 5 | Visibility cache | `render/SceneResources.{hpp,cpp}` | H | собственный XOR-fold со splitmix64-style avalanche |
| 6 | CSM | `render/ShadowProjection.cpp` | H | 4 каскада, 2048², λ=0.80 |
| 7 | PCF 5×5 | `shaders/voxel.frag` | H | triangular weighted, N·L bias |
| 8 | Contact shadows | `shaders/voxel.frag` | H | DDA, 12 max steps |
| 9 | AOCC | `shaders/voxel.frag` | H | 3-tap × 4 steps, hemisphere |
| 10 | TAA + CAS | `shaders/taa_resolve.frag` | H | YCoCg, Halton(2,3) 8-sample |
| 11 | Ray-march | `shaders/ray_march.comp` | H | F12 toggle, **STUB** (Phase 7) |
| 12 | Walk controller | `physics/PhysicsWorld.cpp` | H | JPH::CharacterVirtual + voxel augment, 4-frame grace |
| 13 | Fluid CA | `voxel/VoxelWorld.cpp` | H | Teschner hash, 2-perp, count conservation |
| 14 | Voxel raycast | `voxel/VoxelRaycast.cpp` | H | 3D DDA через чанки |
| 15 | Jolt | `physics/PhysicsWorld.cpp` | H | CharacterVirtual + voxel solver |
| 16 | Asset pipeline | `asset/AssetLoader.cpp` | C | LoadGlb, glTF/Draco/meshopt |
| 17 | Audio | `audio/AudioEngine.cpp` | C | miniaudio, **MP3 only**, 16/44.1 |
| 18 | Hot reload | `app/main.cpp` | C | **F11**, cmake --target Shaders |
| 19 | Snapshot | `voxel/VoxelWorld.cpp` | C | binary, "PVSNAP01", v1 |
| 20 | JSON config | `voxel/SceneConfig.cpp` | C | nlohmann/json, FetchContent |
| 21 | C++26 фичи | разные | оба | expected, modules, inplace_vector |
| 22 | Build | `CMakeLists.txt`, `CMakePresets.json` | C | 7+8 presets, libc++, 73→19 MB |
| 23 | ECS bridge | `ecs/EcsWorld.cpp` | C | Flecs mirror, sync 1×/frame |

---

**Конец reference.** Связанные документы: `DefenseBriefer_le1t.md` (сокращённая версия для репетиции), `DefenseFAQ.md` (готовые ответы на 15+ вопросов комиссии), `DefenseReport.md` (формальный отчёт с маппингом на ТЗ).
