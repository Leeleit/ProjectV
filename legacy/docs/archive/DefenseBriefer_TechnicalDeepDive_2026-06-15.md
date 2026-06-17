# DefenseBriefer — Технический Deep-Dive (Q&A reference, 2026-06-15)

**Назначение:** этот файл — консолидация технических деталей из **старых** бриферов 2-5 (воксельный мир, рендеринг,
физика, демо+аудио) для подготовки к Q&A на защите. После пересборки скрипта под 5-минутный формат (commit
`2026-06-15T-defense-team-script-rebuild-r0`) эти темы больше **не озвучиваются на сцене** — только в ответах на вопросы
комиссии.

**Структура:** 4 раздела по темам, в каждом — глубокие факты, реализации, номера строк кода, готовая формулировка для
Q&A.

**Не читай на сцене.** Открывай только когда комиссия задаёт глубокий вопрос по конкретному модулю.

---

## 1. Воксельный мир (бывший `DefenseBriefer_2.md`)

### 1.1. Структура данных

**`src/voxel/VoxelWorld.hpp:17-23`** — материалы:

```cpp
enum class VoxelMaterial : uint8_t {
    Air = 0,
    Glass = 1,
    Fluid = 2,
    FloorWhite = 3,
    FloorGray = 4,
};
```

5 материалов, по 1 байту каждый.

**`src/voxel/VoxelWorld.hpp:35-42`** — `Int3` (стандартная раскладка):

```cpp
struct Int3 {
    int x = 0;
    int y = 0;
    int z = 0;
};
static_assert(sizeof(Int3) == 12);
```

12 байт, без padding'а.

**`src/voxel/VoxelWorld.hpp:44-56`** — `VoxelChunk`:

```cpp
struct VoxelChunk {
    Int3 min{};
    Int3 maxExclusive{};
    bool rebuildQueued = true;
    uint32_t nonAirVoxelCount = 0;
};
static_assert(sizeof(VoxelChunk) == 32);
```

32 байта на чанк, ровно 2 SSE-регистра.

**`src/voxel/VoxelWorld.hpp:98-104`** — воксели лежат одним плоским массивом:

```cpp
std::vector<uint8_t> voxels;
int chunkSize = 0;  // 8
int chunkCountX = 0;
int chunkCountY = 0;
int chunkCountZ = 0;
uint64_t editVersion = 0;
```

Структура массива: `index = localX + width * (localY + height * localZ)`. Кэш-дружелюбно: 512 вокселей на чанк = 512
байт, влезает в L1.

### 1.2. VoxelLab (демо-сцена)

**`src/voxel/VoxelWorld.cpp:22-27`** — конфиг:

```cpp
struct VoxelLabShellConfig {
    int radius = 6;
    Int3 center{0, 8, 0};
    int shellThickness = 1;
    float fluidFillLevel = 0.7f;
};
```

**`src/voxel/VoxelWorld.cpp:417-451`** — `BuildVoxelLabShellAndFluid`:

- Идёт по `dz, dy, dx ∈ [-r, r]`.
- Если `distanceSq > outerRadius²` — пропуск.
- Если `distanceSq > innerRadius²` — `Glass` (стенка шара).
- Иначе если `position.y ≤ fluidTop` — `Fluid` (жидкость).

**`src/voxel/VoxelWorld.cpp:454-474`** — `BuildVoxelLabOpaqueAnchors`:

- 3 кубика `FloorGray` (`x ∈ [5,8]`, `z ∈ [4,6]`, `y = baseY+1`) — правый якорь.
- 4 столбика `FloorWhite` (`x ∈ [6,7]`, `z ∈ [4,5]`, `y ∈ [baseY+1, baseY+5]`) — левый якорь.
- 3 столбика `FloorWhite` (`x = 5`, `z = 6`) — передний якорь.

### 1.3. Fluid CA (клеточный автомат)

**`src/voxel/VoxelWorld.hpp:153-208`** — комментарий к `UpdateFluidCA`:

- **Один тик = один шаг CA.** Каждый воксель `Fluid` сначала пытается упасть вниз (правило `f_fall`).
- Если падение заблокировано (`Glass`, `FloorWhite`, `FloorGray`, `Fluid`), жидкость **растекается** в одну из 4
  кардинальных сторон (правило `f_spread`).
- **Двойная буферизация:** читаем из `world.voxels` (immutable snapshot), пишем в `next` (новое состояние), swap в
  конце.
- **Bottom-up y-pass:** итерация `z, y, x` с `y` ascending → 1 cell per tick, без double-step.
- **Claimed-tracking:** 1 байт на воксель (≈10 KB для VoxelLab) — помечает, что destination уже занят предыдущим
  источником.
- **Детерминизм:** single-threaded, нет FP, нет системных вызовов, нет зависимости от pointer identity. Подтверждается
  `TestFluidCA*Deterministic`.

**Один баг был в координатной конвенции commit-loop (pre-2026-06-13):** local indices передавались как world coords,
edge-cells терялись. После аудита исправлено: commit-loop добавляет `world.min` перед `SetVoxelMaterial`. Пин-тест:
`TestFluidCAVoxelLabSphereFallOnGlassBreak`.

### 1.4. Voxel raycast (DDA)

**`src/voxel/VoxelRaycast.hpp:7-15`** — структура результата:

```cpp
struct VoxelRaycastHit {
    bool hasHit = false;
    bool hasPlacementVoxel = false;
    Int3 voxel{};
    Int3 placementVoxel{};  // для placement
    Int3 hitNormal{};
    VoxelMaterial material = VoxelMaterial::Air;
    float distance = 0.0f;
};
```

**`src/voxel/VoxelRaycast.cpp`** — DDA по `world.voxels`, возвращает:

- `voxel` — куда попал луч (hit cell).
- `placementVoxel` — куда поставить новый блок (предыдущая ячейка).

### 1.5. Voxel interaction (placement/removal)

**`src/voxel/VoxelInteraction.cpp`** — логика:

- `UpdateVoxelInteraction` вызывается каждый кадр с input + camera.
- Делает raycast, читает hit.
- Placement: `FillVoxelBox` от `mutationAnchorVoxel` до `hit.placementVoxel` (правый клик).
- Removal: `FillVoxelBox` от `hit.voxel` (левый клик) → `Air`.
- `CanPlaceInteractionVoxelBox` — проверка, не пересекается ли placement-box с позицией игрока (
  `DoesPhysicsCharacterOverlapVoxel`).
- Box fill: `FillVoxelBox` в `VoxelWorld.cpp:1155-1199` — `O(extent.x * extent.y * extent.z)`.

### 1.6. Snapshot системы (PVSNAP01)

**`src/voxel/VoxelWorld.cpp:17-20`**:

```cpp
constexpr VoxelScenePreset kDefaultVoxelScenePreset = VoxelScenePreset::VoxelLab;
constexpr char kDefaultVoxelWorldSnapshotFilename[] = "ProjectV.snapshot.bin";
constexpr std::array kVoxelWorldSnapshotMagic{'P', 'V', 'S', 'N', 'A', 'P', '0', '1'};
constexpr uint32_t kVoxelWorldSnapshotVersion = 1u;
```

**`src/voxel/VoxelWorld.cpp:29-43`** — header 80 байт:

- `magic[8]` — `PVSNAP01`.
- `version` (u32) — `1u`.
- `voxelByteCount` (u32).
- `reserved` (u32).
- `scenePreset` (u8) + `reservedBytes[3]`.
- `VoxelWorldConfig config{}` (24 B).
- `Int3 min`, `Int3 maxExclusive` (24 B).
- `uint64_t editVersion` (8 B).

**API:** `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` возвращают `std::expected<bool, VoxelSnapshotError>` (Tier
1.B, холодный путь, ~2× cost несущественен).

**Хоткеи:** F6 — сохранить, F7 — загрузить (см. `src/app/InputActions.cpp:135-136`).

### 1.7. Scene config (JSON)

**`src/voxel/SceneConfig.hpp:17-23`**:

```cpp
struct SceneConfig {
    std::string name = "ProjectV Default";
    VoxelScenePreset scenePreset = VoxelScenePreset::VoxelLab;
    VoxelWorldConfig voxelWorldConfig{};
    float sunDirectionY = 0.80f;
    float exposure = 1.0f;
};
```

Путь по умолчанию: `runtime/scene.json` (создаётся при первом запуске).

### 1.8. 5 scene presets

**`src/voxel/VoxelWorld.hpp:26-32`**:

```cpp
enum class VoxelScenePreset : uint8_t {
    VoxelLab = 0,
    FlatBenchmark,
    TransparencyStress,
    ChunkGrid,
    MeshingStress,
};
```

- **VoxelLab** — демо: чекерборд + стеклянный шар + жидкость + якоря.
- **FlatBenchmark** — плоский пол для замеров.
- **TransparencyStress** — колонны `Glass` для теста прозрачности.
- **ChunkGrid** — маркеры по углам чанков.
- **MeshingStress** — большой объём для нагрузки на мешинг.

Переключение: F5 (`CycleScenePreset`).

---

## 2. Рендеринг (бывший `DefenseBriefer_3.md`)

### 2.1. Voxel meshing (compute shader)

**`src/shaders/voxel_mesh.comp:613-619`** — описание алгоритма:
> «A1 (4.1 greedy meshing): 6 per-axis greedy passes, one per face direction. Each pass walks the 2D plane of cells that
> emit a face in that direction and merges adjacent cells with the same exposed state into a single W×H quad. For
> oversized chunks (>kMaxChunkExtentForGreedy in any in-plane axis) the pass falls back to per-voxel emission. This
> replaces the previous triple-nested loop over (X, Y, Z) × 6 directions that emitted one PackedFace per voxel-face.»

**`src/shaders/voxel_mesh.comp:130-132`** — packing (W, H) of merged quad:

- 6 бит на in-plane width + 6 бит на in-plane height → до 64 вокселей в одну сторону.
- Для chunk extent 64 это покрывает все практические случаи.

**Vertex shader** (`voxel.vert:117-152`):

- `ApplyGreedyScale(faceIndex, unitOffset, quadExtents)` — масштабирует unit-quad по merged extents.
- `quadExtents` декодируется из packed face data.

### 2.2. Cascade Shadow Maps (CSM)

**`src/render/ShadowProjection.hpp:42-51`** — API:

- `BuildSunShadowProjection(world, sunDirection, coverageScale)` — single light VP.
- `BuildSunShadowCascadeProjections(world, sunDirection, inputs, coverageScale)` — 4 cascades.

**`src/render/ShadowProjection.hpp:53-56`** — split formula:

- `BuildSunShadowCascadeSplits(nearPlane, farPlane, splitLambda=0.80)` — near-biased, lambda 0.80.

**`src/voxel/VoxelMaterials.cpp:181-236`** — per-preset shadow params:

- VoxelLab: `sunShadowParams = {0.72f, 0.0009f, 0.0060f, 1.10f}`.
- 4 params: strength, depthBias, normalBias, filterRadius.

### 2.3. TAA (Temporal Anti-Aliasing)

**`src/render/Taa.hpp:7-16`** — jitter:
> «8-tap Halton(2,3) sub-pixel jitter sequence, in *pixel* units relative to the rasterization center. ... Returns the
*previous* jitter, then advances the internal index by one. The first 8 calls cover the full 2×2 sub-pixel cell in a
> non-repeating, non-grid pattern.»

**`src/render/Taa.hpp:18-37`** — `BuildTaaHistoryParams` + `BuildTaaLayerHistoryParams`:

- `taaHistoryParams`: `(texelSize.x, texelSize.y, historyValid, neighbourhoodRadius)`.
- `taaLayerHistoryParams`: `(texelSize.x, texelSize.y, historyValid, blendFactor=0.4)`.

**3-й MRT attachment на voxel pass** (per `agent/decisions.md`): `outLayerMask` (Location 2, R = CTSH, G = AOCC, B =
LOCL, A = 1.0). Per-frame `vkCmdCopyImage` в `taaLayerHistoryColorTarget`. `mix(rawCurrent, history, blend=0.4)` к
AOCC + LOCL. CTSH **не** blended (cascade/contact shadow separation refactor deferred).

### 2.4. Lighting debug views

**`src/voxel/VoxelMaterials.hpp:23-34`** — 10 views:

```cpp
enum class LightingDebugView : uint8_t {
    Final = 0,
    Ambient,
    Direct,
    Local,
    Shadow,
    Cascade,
    Contact,
    Occlusion,
    Fog,
    Taa,
};
```

Переключение: клавиша `B` (`CycleLightingDebugView`).

### 2.5. Shadow tuning

**`src/voxel/VoxelMaterials.hpp:36-43`** — 6 targets:

- Strength, DepthBias, NormalBias, FilterRadius, Coverage, CascadeBlend.

**Клавиши:** `O` — cycle target, `U`/`I` — decrease/increase value, `V` — reset.

### 2.6. Ray-march pass (STUB)

**`src/render/RayMarchPass.hpp:9-30`** — статус:
> «The current implementation is an **API + state contract only** — the `RecordRayMarchCommands` entry point logs the
> per-frame state and is a no-op until a small follow-up slice binds the compute pass into the graphics command stream.
> That follow-up is documented in `docs/DefenseReport.md §3` and `agent/decisions.md` (deferred to Phase 7 — full compute
> pipeline + offscreen color attachment + blit to swapchain). The shape of the API is stable; consumers can wire their
> side of the call site now.»

**Compute shader:** `src/shaders/ray_march.comp` — Amanatides-Woo DDA, компилируется в `.spv`.

**API:** `SetRayMarchEnabled(bool)`, `IsRayMarchEnabled()`, `RequestRayMarchPipelineRecreate()`,
`IsRayMarchPipelineRecreatePending()`, `RecordRayMarchCommands(...)`.

**Хоткей:** клавиша `2` (после релокации с F6/F12 2026-06-15).

### 2.7. Hot shader reload

**`src/app/main.cpp:68-114`** — `RebuildAllShadersFromDisk()`:

1. Получает `PROJECTV_BUILD_DIR` (env) или `PROJECTV_CMAKE_BUILD_DIR` (compile-time default).
2. `cmake --build $BUILD_DIR --target Shaders > /tmp/projectv_shader_reload.log 2>&1`.
3. `projectv::render::RequestRayMarchPipelineRecreate()` — помечает ray-march pipeline.
4. `stderr`: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`.

**Хоткей:** клавиша `1` (после релокации с F5 2026-06-15).

---

## 3. Физика (бывший `DefenseBriefer_4.md`)

### 3.1. Jolt integration

**`src/physics/PhysicsWorld.hpp:50-90`** — API:

- `CreatePhysicsState()` / `DestroyPhysicsState()`.
- `SyncPhysicsWorld(physics, world)` — синхронизирует воксели с Jolt collision shapes.
- `RaycastPhysicsWorld(physics, origin, direction, maxDistance)` — Jolt-уровень raycast.
- `DoesPhysicsCharacterOverlapVoxel(physics, camera, voxel)` — наш собственный воксельный решатель дополняет Jolt для
  опоры игрока на блоки.

### 3.2. Walk controller

**`src/physics/PhysicsWorld.hpp:67-72`** — `TickWalkCharacter(physics, world, camera, input, dt)`:

- Использует JPH::CharacterVirtual для детекции столкновений.
- Наш собственный код дополняет Jolt для опоры игрока на блоки.

**`src/physics/PhysicsWorld.hpp:19-40`** — `PhysicsWalkDebugInfo`:

- `supportState`: `Air` / `Grounded` / `EdgeGrace`.
- `footSupportScore`, `footSupportHitSamples` / `footSupportTotalSamples` — диагностика опоры.
- `edgeGraceFramesRemaining`, `groundTakeoffGraceFramesRemaining`, `sneakSupportGraceFramesRemaining`,
  `ledgeReleaseGraceFramesRemaining` — grace-таймеры.
- `autoJumpDelayFramesRemaining`, `autoJumpEnabled`, `autoJumpDelayEnabled` — автопрыжок.

### 3.3. Edge grace

`EdgeGrace` — состояние, при котором контроллер **не дёргает игрока** на тонких краях. Идея: edge grace срабатывает,
когда опора нечёткая (часть стопы на блоке, часть на воздухе). Параметр `edgeGraceFramesRemaining` показывает, сколько
кадров ещё действует grace.

### 3.4. Sneak (Shift)

`Sneak` — режим скрытности. Игрок **не прилипает к стене за углом**:

- `sneakActive` — флаг.
- `sneakSupportGraceFramesRemaining` — grace-таймер.
- `cachedSneakSupportValid`, `feetInsideCachedSneakSupport`, `cachedSneakSupportReferenceFeetY` — кэшированная опора для
  sneak.

### 3.5. Auto-jump (J)

`AutoJump` — при включении клавиши `J` (`ToggleWalkAutoJump`), контроллер каждый кадр проверяет: есть ли впереди блок
высотой 1, можно ли перепрыгнуть. Параметр `autoJumpDelayFramesRemaining` — задержка после прыжка.

### 3.6. 3 режима управления

**`src/app/InputActions.hpp`** — `InputAction::ToggleControlMode` (F4) переключает циклически:

- `Walk` — обычная ходьба с гравитацией.
- `Creative` — полёт с поддержкой столкновений.
- `Spectator` — режим наблюдателя, пролетает сквозь стены (noclip).

Двойной Space — toggle walk ↔ creative.

### 3.7. Voxel raycast для character

**`src/physics/PhysicsWorld.hpp:79-82`** — `DoesPhysicsCharacterOverlapVoxel`:

- Проверяет, пересекается ли AABB персонажа (вокруг центра камеры) с заданным вокселем.
- Используется в `VoxelInteraction` для предотвращения placement внутрь игрока.

---

## 4. Демо + ассеты + аудио (бывший `DefenseBriefer_5.md`)

### 4.1. Voxel Laboratory (демо-сцена)

Подробности в §1.2.

**Уникальные детали:**

- Пол-шахматка 18×18 (XZ extent).
- Стеклянный шар радиуса 6 вокруг (0, 8, 0).
- Жидкость внутри шара до fluidTop (≈70% внутреннего радиуса).
- 3 непрозрачных якоря (правый куб, левый столбик, передний столбик) — для стабильных теней.
- Процедурная генерация <200 мс.

### 4.2. Asset pipeline

**`src/asset/AssetLoader.hpp:35-37`** — entry point:

```cpp
std::unique_ptr<LoadedAsset> LoadGlb(
    const std::string &path,
    LoadAssetError *outError = nullptr);
```

**`src/asset/AssetLoader.cpp:408`** — glTF parser:

```cpp
fastgltf::Parser parser(fastgltf::Extensions::KHR_draco_mesh_compression);
```

**`src/asset/AssetLoader.cpp:446-449`** — Draco:

```cpp
if (primitive.dracoCompression != nullptr) {
    std::string dracoError;
    if (!DecodeDracoPrimitive(asset, primitive, primitiveData, &dracoError)) {
        SetLastError("draco decode failed: " + dracoError);
```

**`src/asset/MeshBaker.hpp:29-37`** — bake config:

```cpp
struct BakeConfig {
    bool optimizeVertexCache = true;
    bool optimizeVertexFetch = true;
};
```

**`src/asset/MeshBaker.cpp:56-87`** — meshopt шаги:

- `meshopt_optimizeVertexCache` — reorder indices for vertex cache locality.
- `meshopt_generateVertexRemap` — vertex deduplication.
- `meshopt_remapVertexBuffer` — apply remap.
- `meshopt_remapIndexBuffer` — apply remap.
- `meshopt_optimizeVertexFetch` — compact vertices.
- `meshopt_analyzeVertexFetch` — overfetch ratio (BakedMesh.overfetch).

**`src/asset/AssetManifest.hpp:13-29`** — manifest entry:

```cpp
struct ManifestEntry {
    projectv::core::StringID id;
    std::string path;
    glm::vec3 position{0.0f};
    glm::vec3 rotationDegrees{0.0f};
    float scale{1.0f};
};
```

**Парсинг manifest** — env var `PROJECTV_MODELS`, формат: `pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s;pathC.glb`.
Default id = basename без расширения.

**`src/asset/AssetStub.cpp`** — linker anchor:

```cpp
[[maybe_unused]] constexpr std::size_t gAssetPipelineLinkerAnchor = [] {
    return MESHOPTIMIZER_VERSION + ... + draco::EncodedGeometryType::POINT_CLOUD;
}();
```

Гарантирует, что draco + fastgltf + meshopt **линкуются** в бинарник, даже если их символы не используются напрямую.

### 4.3. Model pass (TAA-aware)

**`src/asset/ModelPass.hpp:8-21`** — два pipeline'а:

- `CreateModelPipeline` — создаёт оба.
- `PickModelPipeline(render)` — выбирает `modelPipeline` или `modelPipelineTaaOn` в зависимости от `render.taaEnabled`.

### 4.4. Audio engine

**`src/audio/AudioEngine.hpp:4-15`** — комментарий:
> «Thin wrapper over miniaudio for ProjectV's music-player slice. ... The engine uses miniaudio's PulseAudio backend on
> Linux, which routes through the `pipewire-pulse` shim to the active PipeWire server. The playback format is hard-coded
> to 16-bit signed PCM at 44.1 kHz stereo per the v1 spec.»

**`src/audio/AudioEngine.hpp:79-83`** — `MusicState`:

```cpp
enum class MusicState : uint8_t {
    Stopped = 0,
    Playing,
    Paused,
};
```

**`src/audio/AudioEngine.hpp:127`** — `loadMusicFolder(path)`:

- Возвращает `std::expected<size_t, AudioLoadError>`.
- 0 — валидный результат (пустая папка).
- Создаёт папку, если не существует.

**`src/audio/AudioEngine.hpp:161-166`** — hotkey actions:

- `togglePlayPause`: Stopped → load+play; Playing → pause; Paused → resume.
- `stop`: cursor=0, state=Stopped.
- `increaseVolume(step) / decreaseVolume(step)`: clamp [0, 1].
- `nextTrack / previousTrack`: cycle с wrap-around, reload+restart если Playing.

**`src/audio/AudioEngine.hpp:66-67`** — filename parser:

```cpp
void ParseArtistTitle(const std::string &filename,
                      std::string &artist, std::string &title);
```

- Разделяет `"Le1t - Palm Trees.mp3"` → `("Le1t", "Palm Trees")`.
- Без разделителя: artist = "-", title = stem.

**Хоткеи (`src/app/InputActions.cpp:196-210`):**

- `Q` — play/pause
- `E` — stop
- `7` / `8` — громкость down/up
- `9` / `0` — следующий / предыдущий трек

**Плейлист refresh:** 5 секунд через `m_lastPlaylistRefresh` (`steady_clock`).

### 4.5. ECS (Flecs)

**`src/ecs/EcsWorld.hpp:20-34`** — API:

- `InitializeAppEcs(state)` — init.
- `GetPrimaryCameraState(ecs)`, `GetDebugState(ecs)`, `GetWorldState(ecs)` — typed accessors.
- `SyncEcsWorldState(ecs)` — 1× за кадр, копирует состояние из VoxelWorld в ECS.
- `GetEcsWorldChunkSummary(ecs, outStats, outChunkEntityCount)` — для HUD.

**Single Source of Truth:**

- `VoxelWorld` — единственный владелец состояния мира.
- ECS — пассивное зеркало для HUD/отладки.
- Все мутации только через VoxelWorld.

### 4.6. C-ядро (frustum cull)

**`src/c_kernels/frustum_cull.hpp:38-47`** — C ABI:

```c
typedef struct ProjectvCFrustumCullParameters {
    float cameraPosition[3];
    float maxDistance;
    float cameraForward[3];
    float tanHalfVerticalFov;
    ...
} ProjectvCFrustumCullParameters;

typedef struct ProjectvCAabb {
    float min[3];
    float _pad0;
    float max[3];
    float _pad1;
} ProjectvCAabb;
```

**`src/c_kernels/frustum_cull.hpp:56-67`** — функции:

- `projectv_cull_frustum_scalar(...)` — scalar C.
- `projectv_cull_frustum_avx2(...)` (только `__AVX2__`).

**Бенчмарк (`src/bench/FrustumCullBenchmark.cpp`):**

- Scalar: **3.7-3.9× быстрее** C++ baseline (`IsAabbVisibleAgainstCameraFrustum`).
- AVX2: 2.5-2.7× (autovectorizer бьёт hand-rolled в debug builds).
- Crossover threshold: **8 AABBs** (ниже — inline C++ helper).

**`src/c_kernels/FrustumCulling.hpp:108`** — `kBatchDispatchThreshold = 8`.

---

## 5. Changelog (для Q&A про историю решений)

| Дата       | Событие                                                                                |
|------------|----------------------------------------------------------------------------------------|
| 2026-06-09 | Tier 0.B: `Mat4` заменил `std::array<float, 16>` для GPU ABI parity                    |
| 2026-06-09 | Tier 1.B: `std::expected<T, E>` на холодных путях (Vulkan init, snapshot, audio load)  |
| 2026-06-12 | Two-level chunk visibility cache (XOR-fold splitmix64 hash)                            |
| 2026-06-12 | C kernel для frustum cull (3.7× speedup scalar)                                        |
| 2026-06-13 | Fluid CA spread rule restored (decisions.md §30)                                       |
| 2026-06-13 | Tier 2.D: C++26 модули в mainline (Math.ixx, Probe.ixx, StringId.ixx)                  |
| 2026-06-13 | RayMarchPass API state + compute shader (stub, Phase 7 follow-up)                      |
| 2026-06-15 | F5/F6 → F11/F12 → цифры 1/2/3 hotkey relocation для shader reload / ray-march / v-sync |

---

**Конец deep-dive.** Это reference для Q&A подготовки. На сцене **не использовать** — для выступления читай
`docs/DefenseScript_Team.md` (5 минут, простой язык).
