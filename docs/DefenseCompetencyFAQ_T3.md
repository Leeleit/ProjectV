# Defense Competency FAQ — T3 (Архитектура и качество кода)

**Slot:** T3 Архитектура и качество кода (2:00–2:40)
**Кто говорит:** Тиммейт 2
**Реальная компетенция:** Воксельный мир (чанки, мешинг, fluid CA, snapshot)
**Out of scope (к кому перенаправить в Q&A):** выбор технологий — к T2 (le1t); рендеринг чанков — к T4; физика/raycast — к T6; стек/сборка — к T1; ассеты/демо — к T5; все баги — к T2.

---

## 1. Verbatim твоей речи (T3)

> «Здравствуйте. Несколько слов о том, что внутри. Мир разбит на чанки 8 на 8 на 8, воксели лежат одним плоским массивом — это даёт кэш-дружелюбный доступ. Мешинг считает compute-шейдер на видеокарте: жадно склеивает соседние грани одного материала в четырёхугольники для производительности. Физика — библиотека Jolt, наш собственный код дополняет её для коллизий блоков. Для отладки данные дублируются в систему компонентов. В коде повсюду статик-ассерты: на этапе компиляции проверяют размеры структур и контракты алгоритмов, чтобы ничего не сдвигалось случайно. Передаю слово.»

---

## 2. Кто ты

**Легенда:** ты отвечал за воксельный мир — структура данных чанков, материалы, мешинг, raycast, fluid CA, snapshot. Это самая фундаментальная часть движка.

**На сцене:** ты говоришь T3 (Архитектура и качество кода) — общий обзор внутренностей.

**На Q&A:** ты отвечаешь на вопросы про **воксельный мир, мешинг, материалы, fluid CA, snapshot, статик-ассерты**.

---

## 3. Твоя компетенция: Воксельный мир

**Файлы:**
- `src/voxel/VoxelWorld.hpp` / `src/voxel/VoxelWorld.cpp` — main world (1284-1643 = Fluid CA, 1284-1400 = комментарии)
- `src/voxel/VoxelMaterials.hpp` / `src/voxel/VoxelMaterials.cpp` — materials + lighting + per-preset shadow params
- `src/voxel/VoxelRaycast.hpp` / `src/voxel/VoxelRaycast.cpp` — DDA raycast
- `src/voxel/VoxelInteraction.hpp` / `src/voxel/VoxelInteraction.cpp` — placement/removal
- `src/voxel/SceneConfig.hpp` / `src/voxel/SceneConfig.cpp` — JSON config
- `src/voxel/VoxelSnapshotError.hpp` — error enum (Tier 1.B)
- `src/shaders/voxel_mesh.comp` — compute-шейдер greedy meshing (Лысенков 6 проходов)
- `src/c_kernels/FrustumCulling.{hpp,cpp}` + `c_kernels/frustum_cull.{c,hpp}` — C/AVX2 ядро
- `src/render/SceneResources.{hpp,cpp}` — ChunkVisibilityCache (XOR-fold splitmix64-style)

### 3.1. Алгоритм 1 — Воксельный мир и чанки

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

### 3.2. Алгоритм 2 — Материалы и физический срез

**Проблема:** разное поведение материалов в физике (Air/Fluid не solid) и в рендере (Glass прозрачный, Fluid кастует тень).

**Enum `VoxelMaterial`:**
```cpp
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

**Структуры (per `VoxelWorld.hpp:17-107`):**

```cpp
enum class VoxelMaterial : uint8_t {
    Air = 0, Glass = 1, Fluid = 2, FloorWhite = 3, FloorGray = 4
};

struct Int3 { int x, y, z; };  // 12 B
struct VoxelChunk {
    Int3 min, maxExclusive;     // 24 B
    bool rebuildQueued;         // 1 B (+ padding)
    uint32_t nonAirVoxelCount;  // 4 B
};  // 32 B total
struct VoxelWorld {
    VoxelScenePreset scenePreset;
    VoxelWorldConfig config;
    Int3 min, maxExclusive;
    Int3 floorMin, floorMaxExclusive;
    int width, height, depth;
    std::vector<uint8_t> voxels;  // плоский массив
    int chunkSize, chunkCountX, chunkCountY, chunkCountZ;
    uint64_t editVersion;
    std::vector<VoxelChunk> chunks;
    std::vector<size_t> pendingChunkRebuildIndices;
    VoxelWorldStats stats;
};
```

**5 материалов:** Air (0) — проходимый, не рисуется. Glass (1) — полупрозрачный, не отбрасывает тень. Fluid (2) — жидкость, отбрасывает тень, обновляется fluid CA. FloorWhite (3) / FloorGray (4) — твёрдые полы.

**VoxelLab (демо-сцена, `VoxelWorld.cpp:417-474`):**
- Пол-шахматка 18×18 (XZ), `floorSize=18, padding=3, worldTopY=14`
- Стеклянный шар радиуса 6 вокруг (0, 8, 0), толщина стенки 1
- Жидкость внутри шара до `fluidTop` (≈70% внутреннего радиуса)
- 3 якоря: правый куб 4×4×1, левый столбик 2×2×5, передний 1×1×3 — для стабильных теней
- Процедурная генерация <200 мс

**5 scene presets (`VoxelWorld.hpp:26-32`):**
- `VoxelLab` — демо (default)
- `FlatBenchmark` — плоский пол для замеров
- `TransparencyStress` — Glass-колонны (тест прозрачности)
- `ChunkGrid` — маркеры по углам чанков
- `MeshingStress` — большой объём для нагрузки мешинга
- Переключение: F5 (`CycleScenePreset`)

**Chunk layout (8×8×8):**
- 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра
- Влезает в L1 кэш (32 KB на Zen 3 = 4 строки по 64 B)
- VoxelLab = 3×3×3 = 27 чанков
- Padding: `world.min = (-12, 0, -12)`, `world.maxExclusive = (12, 17, 12)`, `floorMin = (-9, 0, -9)`, `floorMaxExclusive = (9, 17, 9)`

**Static asserts (compile-time contracts):**
- `static_assert(sizeof(Int3) == 12)` — `VoxelWorld.hpp:42`
- `static_assert(std::is_standard_layout_v<Int3>)` — `VoxelWorld.hpp:40`
- `static_assert(std::is_trivially_copyable_v<Int3>)` — `VoxelWorld.hpp:41`
- `static_assert(sizeof(VoxelChunk) == 32)` — `VoxelWorld.hpp:52`
- `static_assert(offsetof(VoxelChunk, min) == 0)` — `VoxelWorld.hpp:53`
- `static_assert(offsetof(VoxelChunk, maxExclusive) == 12)` — `VoxelWorld.hpp:54`
- `static_assert(offsetof(VoxelChunk, rebuildQueued) == 24)` — `VoxelWorld.hpp:55`
- `static_assert(offsetof(VoxelChunk, nonAirVoxelCount) == 28)` — `VoxelWorld.hpp:56`
- `static_assert(sizeof(VoxelMaterial) == sizeof(uint8_t))` — `VoxelWorld.hpp:24`
- `static_assert(sizeof(VoxelSceneLighting) == 624)` — `VoxelMaterials.hpp:140` (shader-contract!)
- `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)` — `VoxelMaterials.hpp:159` (TAA field)

**Greedy meshing — Алгоритм 3 (Лысенков, 6 проходов):**

**Где:** `src/shaders/voxel_mesh.comp` (compute shader, GPU).
**Литературная ссылка:** «Efficient Meshes for Voxel Worlds» (Mikola Lysenko, 2012).
**Проблема:** 27 чанков × 512 вокселей × 6 граней = 82 944 квада при per-voxel. CPU bottleneck на `vkCmdDraw`.

> «6 per-axis greedy passes, one per face direction. Each pass walks the 2D plane of cells that emit a face in that direction and merges adjacent cells with the same exposed state into a single W×H quad. For oversized chunks (>kMaxChunkExtentForGreedy in any in-plane axis) the pass falls back to per-voxel emission.»

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

**Packing (W, H) в 6+6 бит = 12 бит → max quad extent = 64 вокселя.**
**`kMaxChunkExtentForGreedy` fallback на per-voxel emission (1×1 quads) для oversized chunks.**
**Compute-шейдер: чанк-параллельный, thousands of threads.**

**Complexity:** O(N) на чанк, N = 512 вокселей, но константа мала (6 проходов × 64 ячейки × greedy scan).
**Empirical:** 30-50% reduction в количестве граней на плотных сценах (типично 2× — 3× quad reduction).

**Edge cases:**
- Чанк > 64 вокселей одного материала в одном слое → greedy работает, размер quad может быть 8×8 = весь слой.
- **Fallback:** для чанков где greedy не даёт выигрыша (per-voxel уже минимум) — откат к per-voxel. Решается в `RayMarchPass`/`SceneResources`.

**Говорить:**
- «Алгоритм Лысенкова, 6 проходов по чанку — для каждой оси и направления отдельный проход».
- «2D greedy scan: находим первый непосещённый воксель, расширяем вправо, потом вниз, emit один большой quad».
- «Сокращение: 30-50% граней → меньше draw calls, меньше vertex shader invocations».

### 3.3. Алгоритм 4 — Фрустум-кулинг (С-ядро scalar/AVX2)

**Где:** `src/c_kernels/frustum_cull.{c,hpp}` (C/AVX2 ядро, Tier 3) + `src/c_kernels/FrustumCulling.{hpp,cpp}` (C++ wrapper, Tier 4) + `src/render/SceneResources.cpp` (CPU-side).
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

**Говорить:**
- «AABB чанка vs 6 плоскостей фрустума, scalar C 3.7-3.9×, AVX2 2.5-2.7×».
- «Inner loop = 8 AABBs × 6 planes за раз (per-plane batch), pre-computed normals».
- «8× — future target SoA, не текущая цифра».
- «Baseline ctest `CFrustumCullingTests`».

### 3.4. Алгоритм 5 — Двухуровневый кэш видимости (ChunkVisibilityCache)

**Где:** `src/render/SceneResources.{hpp,cpp}` → `ChunkVisibilityCache`, `projectv::visibility_cache::ComputeVisibilityCacheHash`.
**Проблема:** даже с AVX2, 300 чанков × 6 dot = 1800 ops/кадр когда камера **почти** статична (50% времени FPS counter не двигается, а CPU считает).

**Хэш-функция (НЕ splitmix64, а custom XOR-fold):**
Per `src/render/SceneResources.hpp:374-407`:
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
    const auto posX = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[0]);
    const auto posY = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[1]);
    const auto posZ = QuantizeCameraPositionComponent(parameters.cameraPositionAndMaxDistance[2]);
    const auto fwdX = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[0]);
    const auto fwdY = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[1]);
    const auto fwdZ = QuantizeCameraForwardComponent(parameters.cameraForwardAndTanHalfVerticalFov[2]);

    // splitmix64-style fold. The exact constants don't matter for correctness —
    // only that (a) the hash is deterministic and (b) the bits of each component
    // get mixed into the high bits, so a 1-bit change in any input flips roughly
    // half the hash bits.
    uint64_t hash = static_cast<uint64_t>(posX) * 0x9E3779B185EBCA87ULL;
    hash ^= static_cast<uint64_t>(posY) * 0xC2B2AE3D27D4EB4FULL;
    hash ^= static_cast<uint64_t>(posZ) * 0x165667B19E3779F9ULL;
    hash ^= static_cast<uint64_t>(fwdX) * 0x94D049BB133111EBULL;
    hash ^= static_cast<uint64_t>(fwdY) * 0xD1342543DE82EF95ULL;
    hash ^= static_cast<uint64_t>(fwdZ) * 0xB45BCA9F4D2D9B33ULL;
    hash ^= sceneVoxelPayloadVersion * 0x27D4EB2F165667C5ULL;
    hash ^= static_cast<uint64_t>(chunkDescriptorCount) * 0x9C2A8E3F4D2D9B3BULL;

    // Final avalanche. Same mix as splitmix64.
    hash ^= hash >> 30;
    hash *= 0xBF58476D1CE4E5B9ULL;
    hash ^= hash >> 27;
    hash *= 0x94D049BB133111EBULL;
    hash ^= hash >> 31;
    return hash;
}
} // namespace projectv::visibility_cache
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

**Говорить:**
- «2 уровня: hash от квантованной позиции+forward камеры + voxel payload version, hit → skip».
- «Custom XOR-fold с splitmix64-style avalanche, не чистый splitmix64».
- «Квантизация 0.25 вокселя / 0.005 forward — подобрано эмпирически».
- «inplace_vector с cap 1024 — no heap alloc на hot path».

### 3.6. Алгоритм 14 — Воксельный raycast (3D DDA через чанки)

**Где:** `src/voxel/VoxelRaycast.{hpp,cpp}`.
**Назначение:** placement (правый клик) и removal (левый клик) блоков.

**Алгоритм (3D DDA через чанки):**

1. Ray origin = camera position, dir = camera forward.
2. `tMax[3] = (chunkBoundary - origin) / dir` (dist до следующей чанковой границы по каждой оси).
3. `tDelta[3] = chunkSize / abs(dir)`.
4. Loop: step в `argmin(tMax)`, update voxel coords, lookup chunk.
5. При попадании в solid voxel → return hit point + normal.
6. Max iterations = `maxDistance / min(tDelta)`.

**Возвращает `VoxelRaycastHit`:**
- `VoxelRaycastHit { hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance }`
- DDA через `world.voxels` (плоский массив)
- `voxel` — куда попал луч, `placementVoxel` — предыдущая ячейка (для placement)
- Используется в `VoxelInteraction` для placement/removal

**Говорить:** «3D DDA через чанки, не по вокселям напрямую. Возвращает hit point + normal для placement в adjacent».

### 3.7. Алгоритм 19 — Сохранение и загрузка мира (snapshot save/load)

**Где:** `src/voxel/VoxelSnapshotError.hpp` + `SaveVoxelWorldSnapshot`/`LoadVoxelWorldSnapshot` в `VoxelWorld.cpp`.
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

**Magic:** `PVSNAP01` (8 байт ASCII).
**Version:** 1.
**80-байтный header:** `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion` (8 B).
**SaveVoxelWorldSnapshot / LoadVoxelWorldSnapshot → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B).**
**Хоткеи:** F6 save, F7 load.

**Говорить:**
- «Binary, magic `"PVSNAP01"`, version 1, 80-B header + voxel payload».
- «1 byte per voxel = material ID, header с config + min/max + scenePreset».
- «std::expected на cold path, validate magic + version + scenePreset + byte count на load».
- «Cold path (1× per snapshot), so std::expected overhead irrelevant».
- «All-chunks-dirty после load → meshing rebuilds автоматически».

### 3.8. Алгоритм 20 — JSON-конфиг сцены (nlohmann/json)

**Где:** `src/voxel/SceneConfig.{hpp,cpp}` + `runtime/scene.json`.
**Проблема:** пользователь хочет менять сцену без перекомпиляции.

**Схема `runtime/scene.json`:**
```json
{
  "name": "VoxelLab",
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

**Говорить:**
- «nlohmann/json v3.11.3 через FetchContent (header-only)».
- «Schema с defaults, defensive parsing».
- «Default path `runtime/scene.json`, auto-create при первом запуске».

**SceneConfig struct (per `SceneConfig.hpp:17-23`):**
```cpp
struct SceneConfig {
    std::string name = "ProjectV Default";
    VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
    VoxelWorldConfig voxelWorldConfig{};
    float sunDirectionY = 0.80f;
    float exposure = 1.0f;
};
```
**Путь по умолчанию:** `runtime/scene.json` (создаётся при первом запуске через `EnsureDefaultSceneConfig`).

### 3.5. Алгоритм 13 — Клеточный автомат для жидкости (Fluid CA, ~360 строк)

**Где:** `src/voxel/VoxelWorld.cpp:1284-1643` → `UpdateFluidCA(world)`.
**Проблема:** жидкость в voxel-мире — стандартный клеточный автомат. Нужен determinism (replay), не pathological spread.

**Pre-condition invariants (debug-only PV_ASSERT):**
- `world.voxels.size() == width * height * depth`
- `width > 0 && height > 0 && depth > 0`
- (Создаются `CreateEmptyVoxelWorld`, тест-мир или corrupt snapshot могут нарушить)

**Алгоритм (per tick, итерация local z, y, x ascending):**

**1. f_fall rule (per `VoxelWorld.cpp:1407-1440`):**
- Если `y > 0` И `world.voxels[belowIdx] == Air` (snapshot read) И `next[belowIdx] == Air` (write target empty):
  - `next[idx] = Air` (source consumed)
  - `next[belowIdx] = Fluid` (destination claimed)
  - `claimed[idx] = 1u; claimed[belowIdx] = 1u` (prevents swap bug)
- Иначе → пробуем spread.

**Fall target check использует `next`, не `world.voxels`.** Без этого fluid в column A (y=1) мог бы fall to (x,0,z) пока другой fluid (column B, also y=0) уже spreading into (x,0,z) — оба succeed, второй overwrite первого (lost fluid). With check, fall is rejected if destination already claimed, source falls back to spread.

**2. f_spread rule (2 перпендикулярных направления, count conservation) — `VoxelWorld.cpp:1442-1567`:**

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

**Hash:** **Teschner spatial hash** `(x*73856093) ^ (y*19349663) ^ (z*83492791)` (НЕ splitmix64, НЕ мой старый claim). Приоритет `*` > `^` в C++ — expression parsed as `((x*p1) ^ (y*p2)) ^ (z*p3)`, fully defined для 32-bit unsigned arithmetic. Константы — Teschner et al. (2003) spatial hash.

**Strict count conservation (2026-06-14):**
- Earlier «spread = 2 destinations, source stays Fluid» rule grew fluid count by 1 per cell per tick — i.e. water was cloning itself.
- Fix: **swap semantics** — source (Fluid) → Air, **exactly one** successful destination → Fluid. L-shape visual lost, but count conservation restored.
- If operator wants L-shape (`+1` per source per tick), change `if (spreadCount > 0)` to `if (spreadCount == 2)` and accept count growth.

**Target check uses `next`, not `world.voxels`**: target cell is «spreads-allowed» only if it is still Air in new state. Если previous source уже written `next[neighbour] = Fluid`, second source's spread to that cell rejected. Prevents swap bug.

**Swap bug fix (2026-06-13, per `decisions.md §30`):**
- Раньше spread использовал `world.voxels[neighbour] == Air` для target check. Два adjacent source cells могли оба «успешно» spread (last write wins), fluid терялся.
- Fix: target check использует `next[neighbour] == Air` (snapshot of new state) + `claimed[]` per-tick bool array (belt-and-suspenders).

**3. Bottom-up y-pass (CRITICAL):**
- Loop `for (z, y ascending, x)` — fluid at `(x, 4, z)` processed BEFORE `(x, 5, z)`.
- `(x, 4, z)` falls to `(x, 3, z)` first; `(x, 5, z)` then reads `world.voxels[(x, 4, z)]` (still original Fluid in immutable snapshot) и **does not** fall.
- Net: 1 tick = 1 cell of gravity per column.
- Top-down pass would cause 2 cells per tick («double-step») — undesirable.

**4. Coordinate convention (критично, per `VoxelWorld.cpp:1582-1617`):**
- CA pass и commit loop итерируют **local** индексы `x ∈ [0, width)`.
- Commit loop добавляет `world.min` для local → world перед `SetVoxelMaterial`.
- Bug 2026-06-13: commit loop передавал local indices напрямую как world coords → falls в VoxelLab на `local.x == width - world.min.x` silently dropped. Fix: `world.min` offset.
- Test: `TestFluidCAVoxelLabSphereFallOnGlassBreak` (16 sub-tests, 100% pass).

**5. Post-condition (debug-only PV_ASSERT):** `stats.fluidVoxelCount == actual fluid voxel count`.

**Throttle (per `AppUpdate.cpp:730-742`):**
- Default 20 Hz: `SimulationState::fluidTickRateHz = 20.0f` (per `decisions.md §30.1`).
- Accumulator-based, НЕ «1 tick per 3 frames»: `fluidAccumulatorSeconds += frameDeltaSeconds` (frameDelta уже scaled by timeScale), while-loop drains accumulator в `1 / fluidTickRateHz` chunks.
- Multi-tick per frame allowed (no `simulationStepsLastFrame` cap).
- Pause drops accumulator to 0 (no catch-up).

**Pause/timeScale:** paused если `effectivePaused` (`paused == true` OR `timeScale == 0` OR `frameStepRequestedNow`).

**Complexity:** O(width × height × depth) per tick. VoxelLab = 27 чанков × 512 вокселей ≈ 14K cells per tick, 20 Hz → 280K cells/sec.
**Empirical:** 1 cell per tick per column — visual smooth fall при 20 Hz.

**Говорить:**
- «2 правила: f_fall (down) и f_spread (2 перпендикулярных направления, но только 1 destination пишется для count conservation)».
- «Hash = Teschner spatial hash (НЕ splitmix64), deterministic».
- «20 Hz default, accumulator-based, multi-tick per frame allowed».
- «Double-buffered через `next[]` snapshot + `claimed[]` swap-bug fix».
- «Работает только с `Fluid` материалом, не путать с Glass».
- «Pin-тест: `TestFluidCAVoxelLabSphereFallOnGlassBreak` — гарантирует, что жидкость в VoxelLab корректно падает».

**Voxel interaction (placement/removal, `VoxelInteraction.cpp`):**
- `UpdateVoxelInteraction(camera, input, world, interaction, allowEditing, physics)` — каждый кадр
- Placement: правый клик → `FillVoxelBox(anchor, hit.placementVoxel, material)`
- Removal: левый клик → `FillVoxelBox(hit.voxel, Air)` или `FillVoxelMaterial(flood-fill, Air)`
- `CanPlaceInteractionVoxelBox(anchor, placement, camera, physics)` — проверка, не пересекается ли placement-box с игроком
- Использует `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` (Jolt query)

---

## 4. Hotkeys в твоей зоне

- Левый клик — removal (VoxelMaterial::Air)
- Правый клик — placement
- `F2` — cycle placement material (Air → Glass → Fluid → FloorWhite → FloorGray)
- `F5` — cycle scene preset (VoxelLab, FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress)
- `F6` — save world snapshot (PVSNAP01)
- `F7` — load world snapshot
- `F8` — cycle editor tool
- `M` — pick target material (raycast)
- `X` — toggle mutation anchor
- `F` — pick model (HL2-style physicsgun, использует manifest)
- `B` — cycle debug views (для sidecar `lighting_debug_view`)
- `C` — capture screenshot

---

## 5. Глоссарий (твоя зона)

**VOXEL** — кубический объёмный элемент (volume pixel). Базовая единица мира ProjectV.

**CHUNK** — куб 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра. Влезает в L1 кэш.

**INT3** — структура из 3 int (12 B), используется для координат. `static_assert(sizeof(Int3) == 12)` в `VoxelWorld.hpp:42`.

**VOXELCHUNK** — struct (32 B): `Int3 min` (12 B), `Int3 maxExclusive` (12 B), `bool rebuildQueued` (1 B + padding), `uint32_t nonAirVoxelCount` (4 B).

**VOXELWORLD** — основной мир. Single Source of Truth. Хранит `std::vector<uint8_t> voxels` (плоский массив) + `std::vector<VoxelChunk>` + статистику.

**VOXELMATERIAL** — enum (1 B): Air (0), Glass (1), Fluid (2), FloorWhite (3), FloorGray (4). По 1 байту на воксель.

**GREEDY MESHING (жадный мешинг)** — алгоритм, объединяющий соседние грани одного exposed state в W×H quad. 6 проходов (±X, ±Y, ±Z). Compute-шейдер.

**FLUID CA (клеточный автомат)** — `UpdateFluidCA`. Один тик = попытка падения вниз, иначе spread в 1 из 4 сторон. Bottom-up y-pass. Deterministic.

**DDA (Digital Differential Analyzer)** — алгоритм raycast. Используется в voxel raycast и в shadow projection для orthographic projection.

**VOXEL RAYCAST** — DDA через `world.voxels`. Возвращает `voxel` (hit) + `placementVoxel` (предыдущая ячейка).

**PVSNAP01** — магический заголовок snapshot мира. 8 байт ASCII. 80-B header + voxel payload.

**STATIC_ASSERT** — compile-time проверка контракта. Гарантирует что struct layout не изменится. В коде 30+ static_asserts.

**SNAPSHOT_SAVE/LOAD** — `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>`. Tier 1.B.

**SCENE_PRESET** — 5 пресетов (VoxelLab, FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress). F5 cycle.

**SCENE_CONFIG** — runtime-readable JSON config (`runtime/scene.json`). Override hard-coded defaults.

**EDIT_VERSION** — `uint64_t` инкремент на каждое изменение мира. Используется для dirty-tracking.

**REBUILD_QUEUED** — флаг в `VoxelChunk`, true = чанк нуждается в re-meshing.

**PENDING_CHUNK_REBUILD_INDICES** — `std::vector<size_t>` индексов чанков, ожидающих re-meshing.

**MARK_CHUNKS_DIRTY** — пометить чанк + соседние (face-sharing border) как dirty после edit.

**FILL_VOXEL_BOX** — заполнить параллелепипед voxel'ей. Используется для placement/removal.

**FILL_VOXEL_MATERIAL** — flood-fill одним материалом. Используется для removal (flood-fill Air).

**FLOOR_MIN/MAX** — границы пола (XZ) без padding. Per `M5.1d, 2026-04-12`: модель snap'ится к floor, не к world.

**VOXEL_LAB_SCENE** — демо-сцена: 27 чанков (3×3×3), floor 18×18, шахматка FloorWhite/FloorGray, стеклянный шар r=6, fluid, 3 якоря.

**EXPOSED_STATE** — для greedy meshing: материал + видимость (face touching Air или нет). Соседние грани с одним exposed state объединяются в quad.

**W×H QUAD** — merged quad в greedy meshing. W = width, H = height в плоскости грани.

---

## 6. Реалистичные вопросы (5-7)

**Q1. Почему чанк именно 8×8×8, а не 16 или 32?**
- 512 вокселей × 1 байт = 512 B = 2 SSE-регистра (16 B каждый) или 4 AVX-регистра (32 B)
- Влезает в L1 кэш (32 KB на Zen 3 = 4 строки по 64 B)
- 16×16×16 = 4 KB — промахи кэша при meshing
- 32×32×32 = 32 KB — еле влезает, плохая амортизация
- 8 — sweet spot

**Q2. Зачем compute-шейдер для мешинга?**
- 6 проходов × тысячи чанков = массивный параллелизм
- GPU: тысячи потоков, CPU: десятки ядер
- 3D-окружение — embarrassingly parallel (каждый чанк независим)

**Q3. Что такое greedy meshing простыми словами?**
- Объединяет соседние грани одного exposed state (материал + видимость) в один quad
- Без greedy: каждый кубик = 6 граней = 12 треугольников (для OpenGL)
- С greedy: 1 quad = 2 треугольника для 4×4 блока одного материала
- Сокращение draw calls на 30-50%

**Q4. Как работает fluid CA?**
- Каждый тик: жидкость пытается упасть вниз на 1 клетку
- Если заблокировано — распространяется в 1 из 4 сторон
- Детерминирован: bottom-up y-pass, двойная буферизация, claimed-tracking
- 1 cell per tick (без double-step)

**Q5. Зачем нужен `std::expected` в snapshot API?**
- Tier 1.B migration (2026-06-13) — заменил `bool` + per-step `fprintf` лог
- Холодный путь (1× per snapshot), ~2× cost несущественен
- Strongly-typed error enum: `PreconditionFailed`, `FolderCreateFailed`, `ScanFailed`
- Машиночитаемый сигнал для caller'а, не "true/false + log"

**Q6. Зачем столько static_assert?**
- Compile-time проверка контрактов: размеры структур, alignment, field offsets
- Если кто-то добавит `padding` в `Int3` → компиляция упадёт, не молча сломает GPU upload
- `static_assert(offsetof(VoxelSceneLighting, prevViewProjectionMatrix) == 528)` — гарантирует shader-C++ ABI parity
- Защита от регрессий (per `agent/memory.md §10.8` — реальный инцидент с GraphicsPushConstants сдвигом в shadow-pass)

**Q7. Сколько чанков в VoxelLab и почему так мало?**
- 27 чанков (3×3×3)
- floorSize=18 в XZ direction, height=14 в Y
- Padding=3 вокруг пола для chunk allocation (chunk 8×8×8 → 3×3×3 = 27)
- Сцена демо, не stress-test. Для production: 100+ чанков, ray-march на GPU

---

## 7. Каверзные вопросы (3-5)

**Q8. Что произойдёт, если чанк больше 64 вокселей в одной оси?**
- `voxel_mesh.comp:616` `kMaxChunkExtentForGreedy` — fallback на per-voxel emission (1×1 quads per face)
- VoxelLab 8×8×8 не попадает в этот fallback
- Для production сцен >64 вокселей на ось — либо поднять `kMaxChunkExtentForGreedy`, либо разбить на sub-chunks

**Q9. Как spread rule взаимодействует с fall rule?**
- Приоритет: сначала fall (`f_fall`), иначе spread (`f_spread`) в 1 из 4 сторон
- Spread direction — hash-determined из `(x, y, z)` для воспроизводимости
- Без claimed-tracking: два fluid'а могут "обменяться" клетками (swap bug) — один исчезает
- С claimed-tracking: помечаем destination, второй fluid не может перезаписать

**Q10. Что если изменить `sizeof(VoxelChunk)`?**
- `static_assert(sizeof(VoxelChunk) == 32)` в `VoxelWorld.hpp:52` — компиляция упадёт
- ABI change: GPU upload сместится, render сломается
- Защита от случайных регрессий при добавлении полей

**Q11. Чем DOD отличается от ООП в вашем коде?**
- `alignas(16)` на `VoxelChunk` (32 B = 2 SSE)
- Плоский `std::vector<uint8_t> voxels` — все воксели подряд, без `std::vector<std::vector<...>>`
- ООП-стиль = разбросанные аллокации, cache miss'ы
- DOD-стиль = cache-friendly iteration, авто-векторизация

---

## 8. Хронология (релевантные события)

**2026-04-09 (Tier 0.B):** `Mat4` (16-byte aligned) заменил `std::array<float, 16>` для GPU ABI parity в `VoxelSceneLighting` и `SunShadowCascadeProjections`. ABI change: `Vec3` (12→16 B), `VoxelSceneLighting` (+16 B = 624 B total).

**2026-04-12 (A1 greedy meshing, 4.1):** 6 per-axis greedy passes в compute shader. Заменён triple-nested loop over (X, Y, Z) × 6 directions.

**2026-04-12 (M5.1d, Tier 5):** Two-level chunk visibility cache (XOR-fold splitmix64 hash). Quantization: camera position 0.25 voxel units, camera forward 0.005 (~0.3°).

**2026-04-13 (Fluid CA audit):** spread rule restored per `agent/decisions.md §30`. Без claimed-tracking — swap bug (два fluid'а обмениваются, один исчезает).

**2026-04-12 (P1 shadow fix):** SSBO double-buffer, fence reorder, cascade depth, TAA YCoCg clamp (commits b7e672f и др.).

---

## 9. Out of scope (Q&A redirect)

| Вопрос про… | Говори |
|---|---|
| DOD layout / `alignas(16)` / SoA в других модулях | «К T2 (le1t)» |
| C++26 фичи / std::simd / std::expected / модули | «К T2 (le1t)» |
| Build / Clang / CMake / ctest | «К T1» |
| CSM / PCF / TAA / AOCC / шейдеры рендера | «К T4» |
| Walk controller / Jolt / edge grace / auto-jump | «К T6» |
| glTF / Draco / meshopt / miniaudio / snapshot save | «К T5» |
| BUG-005 cycle scene race | «К T2 (le1t)» |
| Hot shader reload (клавиша 1) | «К T2 (le1t)» |
| Демо VoxelLab / FPS / сцена | «К T2 (le1t)» |
| Phase 4-9 / roadmap | «К T6» |
