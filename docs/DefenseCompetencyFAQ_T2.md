# Defense Competency FAQ — Тиммейт 2 (Воксельный мир)

**Участник:** [Имя Тимейта 2]
**Реальная компетенция:** Воксельный мир (чанки, мешинг, fluid CA, snapshot)
**Speech slot на сцене:** T3 Архитектура и качество кода (2:00-2:40)
**Verbatim текст выступления:** `docs/DefenseScript_Team.md` → раздел «Участник 3 (Архитектура и качество кода)»

**Out of scope (к кому перенаправлять в Q&A):** выбор технологий — к le1t; рендеринг чанков — к Тиммейту 3; физика/raycast — к Тиммейту 4; стек/сборка — к Тиммейту 1; демо — к Тиммейту 5; все баги — к le1t.

**Common (стек, метрики, хоткеи, glossary, chronology):** `docs/DefenseCompetencyFAQ.md`

---

## 2.1. Кто ты

**Легенда:** ты отвечал за воксельный мир — структура данных чанков, материалы, мешинг, raycast, fluid CA, snapshot. Это самая фундаментальная часть движка.

**На сцене:** ты говоришь T3 (Архитектура и качество кода) — общий обзор внутренностей.

**На Q&A:** ты отвечаешь на вопросы про **воксельный мир, мешинг, материалы, fluid CA, snapshot, статик-ассерты**.

## 2.2. Твоя компетенция: Воксельный мир

**Файлы:**
- `src/voxel/VoxelWorld.hpp` / `src/voxel/VoxelWorld.cpp` — main world
- `src/voxel/VoxelMaterials.hpp` / `src/voxel/VoxelMaterials.cpp` — materials + lighting
- `src/voxel/VoxelRaycast.hpp` / `src/voxel/VoxelRaycast.cpp` — DDA raycast
- `src/voxel/VoxelInteraction.hpp` / `src/voxel/VoxelInteraction.cpp` — placement/removal
- `src/voxel/SceneConfig.hpp` / `src/voxel/SceneConfig.cpp` — JSON config
- `src/voxel/VoxelSnapshotError.hpp` — error enum
- `src/shaders/voxel_mesh.comp` — compute-шейдер greedy meshing

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

**Greedy meshing (`voxel_mesh.comp:613-619`):**
> «6 per-axis greedy passes, one per face direction. Each pass walks the 2D plane of cells that emit a face in that direction and merges adjacent cells with the same exposed state into a single W×H quad. For oversized chunks (>kMaxChunkExtentForGreedy in any in-plane axis) the pass falls back to per-voxel emission.»

- 6 проходов (±X, ±Y, ±Z), каждый объединяет смежные грани одного exposed state в W×H quad
- Packing (W, H) в 6+6 бит = 12 бит → max quad extent = 64 вокселя
- `kMaxChunkExtentForGreedy` fallback на per-voxel emission (1×1 quads) для oversized chunks
- Compute-шейдер: чанк-параллельный, thousands of threads

**Voxel raycast (DDA, `VoxelRaycast.cpp`):**
- `VoxelRaycastHit { hasHit, hasPlacementVoxel, voxel, placementVoxel, hitNormal, material, distance }`
- DDA через `world.voxels` (плоский массив)
- `voxel` — куда попал луч, `placementVoxel` — предыдущая ячейка (для placement)
- Используется в `VoxelInteraction` для placement/removal

**Fluid CA (`VoxelWorld.cpp:1284-1643`, ~360 строк):**
- Один тик = один шаг клеточного автомата
- Правила: сначала попытка падения вниз (`f_fall`), иначе распространение в 1 из 4 кардинальных сторон (`f_spread`)
- **Двойная буферизация:** читаем из `world.voxels` (immutable snapshot), пишем в `next`, swap в конце
- **Bottom-up y-pass:** итерация `z, y, x` с `y` ascending → 1 cell per tick, без double-step
- **Claimed-tracking:** 1 байт на воксель (≈10 KB для VoxelLab) — помечает, что destination уже занят
- **Determinism:** single-threaded, нет FP, нет syscalls, нет atomics, нет pointer-identity зависимостей
- **Spread rule restored 2026-06-13** (per `agent/decisions.md §30`)
- Pin-тест: `TestFluidCAVoxelLabSphereFallOnGlassBreak` — гарантирует, что жидкость в шаре VoxelLab корректно падает

**Voxel interaction (placement/removal, `VoxelInteraction.cpp`):**
- `UpdateVoxelInteraction(camera, input, world, interaction, allowEditing, physics)` — каждый кадр
- Placement: правый клик → `FillVoxelBox(anchor, hit.placementVoxel, material)`
- Removal: левый клик → `FillVoxelBox(hit.voxel, Air)` или `FillVoxelMaterial(flood-fill, Air)`
- `CanPlaceInteractionVoxelBox(anchor, placement, camera, physics)` — проверка, не пересекается ли placement-box с игроком
- Использует `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` (Jolt query)

**Snapshot система (PVSNAP01, `VoxelWorld.cpp:17-20`):**
- Magic: `PVSNAP01` (8 байт ASCII)
- 80-байтный header: `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion` (8 B)
- `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B)
- Хоткеи: F6 save, F7 load

**Scene config JSON (per `SceneConfig.hpp:17-23`):**
```cpp
struct SceneConfig {
    std::string name = "ProjectV Default";
    VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
    VoxelWorldConfig voxelWorldConfig{};
    float sunDirectionY = 0.80f;
    float exposure = 1.0f;
};
```
Путь по умолчанию: `runtime/scene.json` (создаётся при первом запуске через `EnsureDefaultSceneConfig`).

## 2.3. Что смотреть на защите

**Слайд 4** (твой) — Архитектура. Показывает чанки 8×8×8, плоский массив, мешинг compute, Jolt, ECS, статик-ассерты.

**Демо во время T2 (le1t):** Voxel Laboratory сцена, облёт камерой, демонстрация voxel raycast (placement/removal блоков).

**HUD:** `CHUNKS: 27` (VoxelLab = 3×3×3 = 27 чанков).

## 2.4. Реалистичные вопросы (5-7)

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

## 2.5. Каверзные вопросы (3-5)

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

## 2.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| DOD layout / `alignas(16)` / SoA в других модулях | «Архитектурное решение — к le1t» |
| C++26 фичи / std::simd / std::expected / модули | «К le1t» |
| Build / Clang / CMake / ctest | «К Тиммейту 1» |
| CSM / PCF / TAA / AOCC / шейдеры рендера | «К Тиммейту 3» |
| Walk controller / Jolt / edge grace / auto-jump | «К Тиммейту 4» |
| glTF / Draco / meshopt / miniaudio / snapshot save | «К Тиммейту 5» |
| BUG-005 cycle scene race | «К le1t» |
| Hot shader reload (клавиша 1) | «К le1t» |
| Демо VoxelLab / FPS / сцена | «К le1t» |
| Phase 4-9 / roadmap | «К Тиммейту 4 (он закрывает)» |
