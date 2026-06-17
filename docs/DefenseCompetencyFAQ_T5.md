# Defense Competency FAQ — Тиммейт 5 (Ассеты и аудио)

**Участник:** [Имя Тимейта 5]
**Реальная компетенция:** Ассеты и аудио (glTF pipeline, Draco, meshopt, miniaudio, snapshot, hot reload)
**Speech slot на сцене:** T5 Прочие фичи + что отложено (3:20-4:00)
**Verbatim текст выступления:** `docs/DefenseScript_Team.md` → раздел «Участник 5 (Прочие фичи + что отложено)»

**Out of scope (к кому перенаправлять в Q&A):** архитектура — к le1t; стек/сборка — к Тиммейту 1; воксельный мир — к Тиммейту 2; рендеринг — к Тиммейту 3; физика — к Тиммейту 4; все баги — к le1t.

**Common (стек, метрики, хоткеи, glossary, chronology):** `docs/DefenseCompetencyFAQ.md`

---

## 5.1. Кто ты

**Легенда:** ты отвечал за ассетный конвейер (glTF + Draco + meshopt) и аудио (miniaudio + MP3). Также знаешь про snapshot save/load и hot shader reload.

**На сцене:** ты говоришь T5 (Прочие фичи + что отложено) — что не вошло в демо + roadmap.

**На Q&A:** ты отвечаешь на вопросы про **ассеты, аудио, snapshot мира, hot shader reload, deferred items**.

## 5.2. Твоя компетенция: Ассеты и аудио

**Asset pipeline файлы:**
- `src/asset/AssetLoader.{hpp,cpp}` — entry point `LoadGlb(path, outError) → std::unique_ptr<LoadedAsset>`
- `src/asset/AssetManifest.{hpp,cpp}` — env `PROJECTV_MODELS`, формат `path.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s`
- `src/asset/AssetRegistry.{hpp,cpp}` — реестр моделей
- `src/asset/DracoMeshDecoder.{hpp,cpp}` — Draco decoder
- `src/asset/MeshBaker.{hpp,cpp}` — `BakeLoadedAsset(asset, config, outError) → BakedMesh`
- `src/asset/MeshGpuResources.{hpp,cpp}` — GPU buffer upload
- `src/asset/ModelPass.{hpp,cpp}` — TAA-aware pipeline
- `src/asset/ModelManifestLoader.{hpp,cpp}` — manifest parser
- `src/asset/AssetStub.cpp` — linker anchor (3 строки, гарантирует линковку draco + fastgltf + meshopt)

**Audio файлы:**
- `src/audio/AudioEngine.{hpp,cpp}` — miniaudio wrapper (~800 строк)
- `src/audio/MusicDirectoryPath.{hpp,cpp}` — путь к `music/`
- `external/miniaudio/` — vendored single-header C library

**glTF parser (`AssetLoader.cpp:392-431`):**
- `fastgltf::Parser parser(fastgltf::Extensions::KHR_draco_mesh_compression)` — line 408
- `parser.loadGltf(dataBuffer.get(), directory, options, categories)` — line 413
- `fastgltf::Options::None`, `fastgltf::Category::OnlyRenderable`
- 2 pass: Pass 1 read POSITION/NORMAL/UV/indices, Pass 2 apply node TRS

**Draco decode (per `AssetLoader.cpp:446-449`):**
- `if (primitive.dracoCompression != nullptr) { DecodeDracoPrimitive(...) }` — line 446
- `DracoMeshDecoder` в `src/asset/DracoMeshDecoder.cpp`
- Извлекает POSITION (3 floats), NORMAL (3 floats), TEX_COORD (2 floats), face indices
- Degenerate geometry detection (no faces)

**Meshopt (per `MeshBaker.cpp:56-87`):**
1. `meshopt_optimizeVertexCache` — reorder indices for vertex cache locality
2. `meshopt_generateVertexRemap` — vertex deduplication
3. `meshopt_remapVertexBuffer` — apply remap к vertex buffer
4. `meshopt_remapIndexBuffer` — apply remap к index buffer
5. `meshopt_optimizeVertexFetch` — compact vertices (cache locality)
6. `meshopt_analyzeVertexFetch` — overfetch ratio (BakedMesh.overfetch)

**Baked mesh struct (`MeshBaker.hpp`):**
```cpp
struct BakedPrimitive {
    std::vector<uint8_t> vertexBuffer;
    std::vector<uint32_t> indices;
    uint32_t vertexCount, indexCount;
    std::optional<size_t> materialIndex;
    float overfetch = 1.0f;  // from meshopt_analyzeVertexFetch
};
struct BakedMesh {
    std::vector<BakedPrimitive> primitives;
    float acmr = 0.0f;  // Average Cache Miss Ratio
    float atvr = 0.0f;  // Average Transform-to-Vertex Ratio
};
```

**Stride:** `kBakedVertexStride = sizeof(float) * 8` = 32 B (float3 pos + float3 normal + float2 uv)

**Asset manifest format (`AssetManifest.hpp:31-42`):**
```
PROJECTV_MODELS=pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s;pathC.glb
```
- Id = basename без расширения по умолчанию
- `projectv::core::StringID` (16 B = hash + length + pad) для O(1) equality, hashable

**Model pipeline (`ModelPass.hpp:8-21`):**
- `CreateModelPipeline` — создаёт оба варианта
- `PickModelPipeline(render)` — выбирает `modelPipeline` или `modelPipelineTaaOn` по `render.taaEnabled`

**Audio engine (`AudioEngine.hpp:95-306`):**
- `ma_engine` + `ma_sound_group` (music volume bus) + `ma_sound` (current track)
- Singleton on `AppState`, plain (not ECS system)
- Format: 16-bit signed PCM, 44.1 kHz, stereo
- Linux backend: PulseAudio → `pipewire-pulse` shim → active PipeWire
- 3 states: `Stopped` / `Playing` / `Paused`
- 5-second playlist refresh (`m_lastPlaylistRefresh`)
- `ParseArtistTitle(filename, &artist, &title)`:
  - Strip case-insensitive `.mp3` tail
  - Split on first ` - ` (space-dash-space)
  - Fallback: `artist = "-"` (em-dash sentinel) when no separator
- 2 MP3 в `music/`: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- Хоткеи: `Q` play/pause, `E` stop, `7`/`8` volume, `9`/`0` next/prev

**Snapshot мира (per `VoxelWorld.cpp:17-20`):**
- Magic: `PVSNAP01` (8 B ASCII)
- 80-B header: `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion` (8 B)
- `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B)
- Хоткеи: F6 save, F7 load

**Hot shader reload (per `main.cpp:68-114`):**
- Клавиша `1` (relocated from F5/F11 2026-06-15)
- `RebuildAllShadersFromDisk()`:
  1. `PROJECTV_BUILD_DIR` (env) или `PROJECTV_CMAKE_BUILD_DIR` (compile-time default)
  2. `cmake --build $BUILD_DIR --target Shaders` (recompiles `.comp/.frag/.vert` через `glslc`)
  3. Log: `std::filesystem::temp_directory_path()/projectv_shader_reload.log` (cross-platform: Windows `%TEMP%`, Linux `/tmp`)
  4. `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Stderr: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`

**Ray-march toggle (клавиша `2`):**
- `SetRayMarchEnabled(bool)` / `IsRayMarchEnabled()` в `RayMarchPass.hpp`
- Toggles `static RayMarchState::enabled` flag
- Per-frame `RecordRayMarchCommands` — `fprintf` в stderr, no-op
- Compute-шейдер `ray_march.comp` скомпилирован, но в graphics stream не вкомпонован

**5 отложенных пунктов (per `docs/DefenseReport.md §3`):**
1. **Полная система частиц** (ТЗ 4.1.4) — Phase 7
2. **Плагины / моддинг API** (ТЗ 4.1.8) — Phase 8
3. **Асинхронная загрузка ресурсов** — Phase 7
4. **HDR-текстуры** (`.hdr`) — Phase 6
5. **SVO (Sparse Voxel Octree)** — Phase 5
6. **Mesh shaders (VK_EXT_mesh_shader)** — Phase 5

**Roadmap (per `docs/DefenseReport.md §3`):**
- Phase 4: Networking (server-authoritative + client prediction)
- Phase 5: SVO (Sparse Voxel Octree) + Mesh shaders
- Phase 6: HDR-текстуры + полный клеточный автомат жидкости на GPU
- Phase 7: Полная система частиц + асинхронная загрузка ресурсов
- Phase 8: Плагины / моддинг API
- Phase 9: Многопользовательский режим (Academic vision)

## 5.3. Что смотреть на защите

**Слайд 6** (твой) — Прочие фичи + что отложено. Показывает asset pipeline, audio, snapshot, hot reload, deferred items.

**Демо во время T2 (le1t):** Hot shader reload (клавиша `1`), audio playback (Q для play, 9/0 для треков), asset loading.

## 5.4. Реалистичные вопросы (5-7)

**Q1. Что такое Draco и зачем?**
- Алгоритм сжатия 3D-мешей от Google
- `KHR_draco_mesh_compression` extension в glTF
- Позволяет загружать меши на 50-80% меньше по размеру
- Декодирование на лету в `DracoMeshDecoder`

**Q2. Что делает meshopt?**
- Оптимизация мешей под видеокарту
- Vertex cache optimization — reorder indices для cache locality
- Vertex fetch optimization — compact vertices (избегаем overfetch)
- Overfetch ratio (BakedMesh.overfetch) — мера эффективности

**Q3. Поддерживает ли audio что-то кроме MP3?**
- Только MP3 (miniaudio built-in MP3 decoder)
- OGG/WAV/FLAC требуют linking `extras/decoders/libvorbis` / `libopus` — deferred
- per `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md`

**Q4. Как работает hot shader reload?**
- Клавиша `1` → `RebuildAllShadersFromDisk()`
- `cmake --build $BUILD_DIR --target Shaders` — recompiles all `.comp/.frag/.vert`
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Другие pipelines (graphics, shadow, TAA) переиспользуют кэшированные модули до Phase 7+ рефакторинга

**Q5. Где хранится snapshot мира?**
- По умолчанию: `ProjectV.snapshot.bin` в working directory
- Magic: `PVSNAP01` (8 B ASCII header)
- 80-B header + voxel payload
- Std::expected<bool, VoxelSnapshotError> — Tier 1.B error enum

**Q6. Что будет если snapshot повреждён?**
- `LoadVoxelWorldSnapshot` вернёт `std::unexpected(VoxelSnapshotError::*)`
- Possible errors: `PreconditionFailed` (null file), `FolderCreateFailed` (write), `ScanFailed` (corrupt header)
- Не падает, graceful degradation

**Q7. Сколько MP3 файлов в проекте?**
- 2 файла: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- Формат имён: `<artist> - <title>.mp3` — парсер для HUD
- 5-секундный refresh плейлиста

## 5.5. Каверзные вопросы (3-5)

**Q8. Почему именно meshopt, а не просто GPU draw call batching?**
- Meshopt на этапе bake'а — один раз, бесплатно в runtime
- Draw call batching — overhead в runtime
- meshopt уменьшает overfetch (vertex fetch locality) и cache miss'ы
- BakedMesh.overfetch ratio — мера эффективности (1.0 = идеал)

**Q9. Почему формат `<artist> - <title>.mp3`?**
- Оператор использует эту конвенцию
- Парсер разделяет по ` - ` (space-dash-space)
- Fallback: `artist = "-"` (em-dash) если нет разделителя
- Em-dash distinct от empty string (= "no track loaded")

**Q10. Что произойдёт, если добавить новый MP3 во время runtime?**
- Плейлист refresh каждые 5 секунд (`m_lastPlaylistRefresh`)
- Новый файл подхватится автоматически
- Если текущий трек не загружен → загрузится при следующем Q

**Q11. Зачем asset manifest, если можно захардкодить?**
- `PROJECTV_MODELS` env var → flexibility для разных сцен
- HL2-style physicsgun (`F` key) — переключение моделей на лету
- Default id = basename без расширения

## 5.6. Out of scope

| Вопрос про… | Говори |
|---|---|
| Почему fastgltf, а не tinygltf | «Архитектурное решение — к le1t» |
| PipeWire vs PulseAudio подробно | «Linux audio, к le1t» |
| Стек/Clang/cmake/ctest | «К Тиммейту 1» |
| Voxel-мир / чанки / мешинг | «К Тиммейту 2» |
| Тени / TAA / AOCC | «К Тиммейту 3» |
| Физика / walk controller | «К Тиммейту 4» |
| BUG-005 cycle scene race | «К le1t (InputAction F5)» |
| Hot shader reload (клавиша 1) | «К le1t» |
| JSON config / snapshot PVSNAP01 | «К le1t» |
