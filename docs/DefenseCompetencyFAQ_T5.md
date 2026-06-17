# Defense Competency FAQ — T5 (Прочие фичи + что отложено)

**Slot:** T5 (3:15–4:00, Участник 5 = Тиммейт 5, slides 9-10: Фичи+Метрики)
**Кто говорит:** Тиммейт 5
**Реальная компетенция:** Ассеты и аудио (glTF pipeline, Draco, meshopt, miniaudio, snapshot, hot reload)
**Out of scope (к кому перенаправить в Q&A):** архитектура — к T2 (le1t); стек/сборка — к T1; воксельный мир — к T3; рендеринг — к T4; физика — к T6; все баги — к T2.

---

## 1. Verbatim твоей речи (T5)

> «Здравствуйте. Здесь упомяну то, что мы не успели показать в демо. У нас есть пайплайн загрузки полигональных моделей извне: парсер glTF, опциональное Draco-сжатие, оптимизация мешей через meshoptimizer. Аудиодвижок на miniaudio, пока поддерживает только MP3. Также реализованы сохранение и загрузка мира в собственный бинарный снимок и горячая перезагрузка шейдеров. В roadmap отложено: сетевой режим, SVO, частицы, моддинг, HDR.
>
> **[Переход на 10 слайд — Метрики]**
>
> Чтобы зафиксировать измерения. 38 пунктов ТЗ закрыты, 5 отложены в roadmap, 0 критических провалов. На VoxelLab в отладочной сборке — 500+ FPS, около двух миллисекунд на кадр. Release-бинарник ужался с 73 до 19 мегабайт, минус 73 процента. Все 14 ctest проходят менее чем за секунду. Передаю слово.»

---

## 2. Кто ты

**Легенда:** ты отвечал за ассетный конвейер (glTF + Draco + meshopt) и аудио (miniaudio + MP3). Также знаешь про snapshot save/load и hot shader reload.

**На сцене:** ты говоришь T5 (Прочие фичи + что отложено) — что не вошло в демо + roadmap.

**На Q&A:** ты отвечаешь на вопросы про **ассеты, аудио, snapshot мира, hot shader reload, deferred items**.

---

## 3. Твоя компетенция: Ассеты и аудио

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

### 3.1. Алгоритм 16 — Конвейер ассетов (asset pipeline: glTF, Draco, meshopt)

**Где:** `src/asset/AssetLoader.{cpp,hpp}`, `DracoMeshDecoder.{cpp,hpp}`, `MeshBaker.{cpp,hpp}`, `ModelManifestLoader.{cpp,hpp}`.
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

**Meshopt (per `MeshBaker.cpp:56-87`):**
1. `meshopt_optimizeVertexCache` — reorder indices for vertex cache locality
2. `meshopt_generateVertexRemap` — vertex deduplication
3. `meshopt_remapVertexBuffer` — apply remap к vertex buffer
4. `meshopt_remapIndexBuffer` — apply remap к index buffer
5. `meshopt_optimizeVertexFetch` — compact vertices (cache locality)
6. `meshopt_analyzeVertexFetch` — overfetch ratio (BakedMesh.overfetch)

**Говорить:**
- «fastgltf → Draco decode → meshopt optimize → VMA upload».
- «Entry point: `LoadGlb(path, outError)`, НЕ `Load`».
- «Manifest через env var, snap above ground».
- «Загрузчик синхронный, <1 сек на 100 МБ glb».

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

### 3.2. Алгоритм 17 — Аудио-движок (audio engine, miniaudio)

**Где:** `src/audio/AudioEngine.{hpp,cpp}`.
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
- `volume()` — float [0, 1] (default 0.8)
- `positionSeconds()` / `durationSeconds()` — для HUD time display
- `currentArtist()` / `currentTitle()` — parsed from filename (`<artist> - <title>.mp3`)

**Cursor semantics (2026-06-13, two iterations):**
- Q during Playing → Paused: `ma_sound_stop` only sets node state to stopped (miniaudio.h:78774); audio thread's last-read position kept in `pSound->cursor`. Q again resumes from there.
- E during Playing → Stopped: `stop()` calls `ma_sound_seek_to_pcm_frame(&m_sound, 0)` after `ma_sound_stop`, atomically sets `pSound->seekTarget` to 0 (miniaudio.h:79437); mixing thread applies seek on next read cycle (miniaudio.h:76908-76916) and resets decoder to start. E+Q (stop+rewind, then play) starts track from beginning.

**Sidecar metadata:** `music_track`, `music_state`, `music_volume` в capture sidecars.

**Audio engine fields (per `AudioEngine.hpp:255-308`):**
- `ma_sound m_sound{}` — current sound
- `bool m_engineInitialized = false`
- `bool m_musicGroupInitialized = false`
- `bool m_soundLoaded = false`
- `std::filesystem::path m_musicFolder`
- `std::vector<std::filesystem::path> m_playlist`
- `size_t m_currentIndex = 0`
- `float m_volume = 0.8f`
- `MusicState m_state = MusicState::Stopped`
- `ma_uint64 m_pausedCursorMs = 0` (dead code 2026-06-13, kept for field-shape stability)
- `std::chrono::steady_clock::time_point m_lastPlaylistRefresh` — 5-second refresh
- `std::string m_currentTrackName`
- `std::string m_currentArtist`, `m_currentTitle` — artist/title cache (2026-06-13)

**Говорить:**
- «miniaudio header-only, MIT, PulseAudio → PipeWire на Linux».
- «16-bit 44.1 kHz stereo».
- «Только MP3, WAV/FLAC тихо игнорируются».
- «Auto-refresh playlist каждые 5 секунд».
- «6 hotkeys: Q play/pause, E stop, 7/8 volume, 9/0 next/prev».
- «Per-frame tick обновляет playlist + обрабатывает "current track removed from disk" gracefully».

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

**5 отложенных пунктов (Phase 4-9):**
1. **Полная система частиц** (ТЗ 4.1.4) — Phase 7
2. **Плагины / моддинг API** (ТЗ 4.1.8) — Phase 8
3. **Асинхронная загрузка ресурсов** — Phase 7
4. **HDR-текстуры** (`.hdr`) — Phase 6
5. **SVO (Sparse Voxel Octree)** — Phase 5
6. **Mesh shaders (VK_EXT_mesh_shader)** — Phase 5

**Roadmap (Phase 4-9):**
- Phase 4: Networking (server-authoritative + client prediction)
- Phase 5: SVO (Sparse Voxel Octree) + Mesh shaders
- Phase 6: HDR-текстуры + полный клеточный автомат жидкости на GPU
- Phase 7: Полная система частиц + асинхронная загрузка ресурсов
- Phase 8: Плагины / моддинг API
- Phase 9: Многопользовательский режим (Academic vision)

---

## 4. Hotkeys в твоей зоне

- `1` — hot shader reload (defense r0, 2026-06-15 relocation)
- `2` — toggle ray-march pass
- `Q` — audio play/pause
- `E` — audio stop
- `7` — volume down
- `8` — volume up
- `9` — next track
- `0` — previous track
- `F` — pick model (HL2-style physicsgun, использует PROJECTV_MODELS manifest)
- `F5` — cycle scene preset
- `F6` — save world snapshot (PVSNAP01)
- `F7` — load world snapshot
- `C` — capture screenshot

---

## 5. Глоссарий (твоя зона)

**GLTF / GLB** — Graphics Language Transmission Format (стандарт Khronos). glTF = JSON, GLB = binary.

**FASTGLTF** — MIT, C++17 парсер glTF 2.0. Vendored.

**DRACO** — Google mesh compression library. `KHR_draco_mesh_compression` extension в glTF.

**MESHOPT / MESHOPTIMIZER** — vertex cache + vertex fetch optimization. Reorder indices, deduplicate vertices, compact vertices.

**ACMR** — Average Cache Miss Ratio (meshopt). Метрика cache-efficiency после mesh optimization.

**ATVR** — Average Transform-to-Vertex Ratio (meshopt). Метрика vertex transform redundancy.

**OVERFETCH** — extra vertex data fetched beyond what's needed (meshopt metric). 1.0 = идеал.

**VERTEX_CACHE_OPTIMIZATION** — reordering triangles to maximize GPU vertex cache hits.

**VERTEX_FETCH_OPTIMIZATION** — interleaving/compressing vertex data to minimize memory bandwidth.

**STRINGID** — `projectv::core::StringID` (16 B = hash + length + pad). O(1) equality, hashable. Tier 1.D/E.

**PROJECTV_MODELS** — env var. Manifest формат: `pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s`. Список моделей для загрузки.

**MINIAUDIO** — single-header C audio library (MIT). Built-in MP3 decoder. Vendored.

**PULSEAUDIO** — Linux audio backend для miniaudio. Routes через `pipewire-pulse` shim → active PipeWire server.

**PIPEWIRE** — современный Linux sound server (`/run/user/1000/pulse/native`). `pipewire-pulse` shim для PulseAudio-совместимости.

**MP3 (MPEG-1 Layer III)** — единственный поддерживаемый формат аудио. OGG/WAV/FLAC — deferred.

**PCM (Pulse-Code Modulation)** — несжатый формат. miniaudio: 16-bit signed PCM, 44.1 kHz, stereo.

**PLAYLIST_REFRESH** — 5-секундный interval (`m_lastPlaylistRefresh`). Новые MP3 подхватываются автоматически.

**ARTIST_TITLE_PARSER** — `ParseArtistTitle(filename, &artist, &title)`. Strip `.mp3`, split on ` - `, fallback `-`.

**HL2_PHYSICSGUN** — `F` клавиша. Pick model через PROJECTV_MODELS manifest. Позволяет swap между загруженными моделями.

**PVSNAP01** — магический заголовок snapshot мира. 8 B ASCII. 80-B header + voxel payload.

**SNAPSHOT_F6/F7** — F6 = save, F7 = load. Tier 1.B error enum для failure cases.

**HOT_SHADER_RELOAD** — клавиша `1` (relocation). `cmake --build $BUILD_DIR --target Shaders` + `RequestRayMarchPipelineRecreate()`. Per-frame no-op для других pipelines.

**RAY_MARCH_STUB** — `RecordRayMarchCommands` — `fprintf` в stderr. Compute-шейдер скомпилирован. Phase 7.

**DEFERRED_ITEMS** — 6 пунктов из ТЗ: частицы, моддинг, async load, HDR, SVO, mesh shaders. Phase 4-9.

**SVO (Sparse Voxel Octree)** — разреженное октодерево вокселей. Альтернатива плоскому массиву. Phase 5.

**MESH_SHADERS (VK_EXT_mesh_shader)** — Vulkan extension для mesh-level шейдеров вместо vertex. Phase 5.

**HDR-TEXTURES (.hdr)** — High Dynamic Range текстуры. Формат Radiance. Phase 6.

---

## 6. Реалистичные вопросы (5-7)

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

---

## 7. Каверзные вопросы (3-5)

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

---

## 8. Хронология (релевантные события)

**2026-04-12 (M5.1d asset-pipeline):** 4 commits landed: `8cc71f8` + др. Asset loader + bake + GPU upload chain.

**2026-04-12 (M1 audio engine):** `AudioEngine` + `miniaudio` integration. PulseAudio backend → PipeWire. 16/44100/stereo. MP3 only.

**2026-04-13 (Tier 1.D/E):** `projectv::core::StringID` для manifest entry id. 16 B (hash + length + pad), O(1) equality, hashable.

**2026-04-13 (Music HUD 1-line → 4-line):** commit `723edc5`. 4 lines per state: `MUSIC <state> VOL 0.80` (always), `ARTIST <name>`, `TITLE <name>`, `POS m:ss / m:ss` (when engine initialized + playlist non-empty).

**2026-04-15 (Post-WBV-r1):** F11/F12/V relocate → 1/2/3. pragma once conversion (55 files).

---

## 9. Out of scope (Q&A redirect)

| Вопрос про… | Говори |
|---|---|
| Почему fastgltf, а не tinygltf | «К T2 (le1t)» |
| PipeWire vs PulseAudio подробно | «К T2 (le1t)» |
| Стек/Clang/cmake/ctest | «К T1» |
| Voxel-мир / чанки / мешинг | «К T3» |
| Тени / TAA / AOCC | «К T4» |
| Физика / walk controller | «К T6» |
| BUG-005 cycle scene race | «К T2 (le1t, InputAction F5)» |
| Hot shader reload (клавиша 1) | «К T2 (le1t)» |
| JSON config / snapshot PVSNAP01 | «К T2 (le1t)» |
| Phase 4-9 / roadmap | «К T6» |
