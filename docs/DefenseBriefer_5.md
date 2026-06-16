# Памятка Тиммейта 5 — Ассеты и аудио (говорит T5 Прочие фичи)

**Участник:** [Имя Тимейта 5]
**Слот на сцене:** 3:20–4:00 (40 секунд) — T5 Прочие фичи + что отложено
**Твоя реальная компетенция:** Ассеты и аудио (glTF pipeline, Draco, meshopt, miniaudio, snapshot, hot reload)
**Что НЕ твоё (к кому перенаправлять в Q&A):** стек/демо — к le1t; вступление — к Тиммейту 1; воксельный мир — к Тиммейту 2; рендеринг — к Тиммейту 3; физика — к Тиммейту 4; все баги — к le1t

---

## 1. Шапка выступления

> «Здравствуйте. Меня зовут [Имя Тимейта 5], я расскажу про то, что мы не успели показать в демо, и что отложено на будущее.»

---

## 2. Что говорить дословно (~75-85 русских слов, 0:40)

> «Здравствуйте. Здесь упомяну то, что мы не успели показать в демо. У нас есть пайплайн загрузки полигональных моделей извне: парсер glTF, опциональное Draco-сжатие, оптимизация мешей через meshoptimizer. Аудиодвижок на miniaudio, пока поддерживает только MP3. Также реализованы сохранение и загрузка мира в собственный бинарный снимок и горячая перезагрузка шейдеров. В roadmap отложено: сетевой режим, SVO, частицы, моддинг, HDR. Дальше — о планах подробнее.»

---

## 3. Понятия (10 терминов, чтобы понимать что говоришь)

| Термин | Что это |
|---|---|
| Ассетный конвейер (asset pipeline) | Цепочка загрузки 3D-моделей от файла на диске до видеокарты |
| glTF / glb | Открытый формат 3D-моделей (стандарт Khronos) |
| Draco | Алгоритм сжатия 3D-мешей от Google |
| meshoptimizer | Библиотека оптимизации мешей под видеокарту (vertex cache, fetch) |
| miniaudio | Лёгкая header-only библиотека воспроизведения аудио |
| Бинарный снимок мира (snapshot) | Файл со всем состоянием мира в компактном двоичном виде |
| PVSNAP01 | Наш собственный магический заголовок снимка (8 байт) |
| Горячая перезагрузка шейдеров | Перекомпиляция шейдеров без перезапуска приложения |
| Гонка дескрипторов | Баг, когда ресурс удаляется, а другая часть кода его ещё использует |
| SVO (Sparse Voxel Octree) | Разреженное октодерево вокселей, альтернатива плоскому массиву |

---

## 4. Что показывать на экране

1. **Сохранение/загрузка снимка** — F6 во время демо сохраняет мир, F7 загружает.
2. **Hot shader reload** — нажатие клавиши `1` перекомпилирует `voxel.frag` / `voxel_mesh.comp` без перезапуска; в `stderr` пишется `HotReloadShaders: re-built shaders`.
3. **Аудио-хоткеи** — `Q` play/pause, `E` stop, `7`/`8` громкость, `9`/`0` треки.

---

## 5. Твоя настоящая компетенция (для Q&A): Ассеты и аудио

**Это то, что ты реально знаешь. На сцене ты говоришь про фичи и roadmap, но на вопросы комиссии отвечаешь по своей компетенции.**

**Ключевые файлы:**

**Asset pipeline:**
- `src/asset/AssetLoader.{hpp,cpp}` — entry point `LoadGlb(path, outError) → std::unique_ptr<LoadedAsset>`
- `src/asset/AssetManifest.{hpp,cpp}` — env `PROJECTV_MODELS`, формат `path.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s`
- `src/asset/AssetRegistry.{hpp,cpp}` — реестр моделей
- `src/asset/DracoMeshDecoder.{hpp,cpp}` — Draco decoder (`KHR_draco_mesh_compression`)
- `src/asset/MeshBaker.{hpp,cpp}` — `BakeLoadedAsset(asset, config, outError) → BakedMesh`
- `src/asset/MeshGpuResources.{hpp,cpp}` — GPU buffer upload
- `src/asset/ModelPass.{hpp,cpp}` — graphics pipeline (TAA-aware: `modelPipeline` + `modelPipelineTaaOn`)
- `src/asset/ModelManifestLoader.{hpp,cpp}` — manifest parser + bake pipeline
- `src/asset/AssetStub.cpp` — linker anchor для draco + fastgltf + meshopt (3 строки, гарантирует линковку)

**Audio engine:**
- `src/audio/AudioEngine.{hpp,cpp}` — miniaudio wrapper
- `src/audio/MusicDirectoryPath.{hpp,cpp}` — путь к `music/`
- `external/miniaudio/` — vendored single-header C library

**glTF parser:** `fastgltf::Parser(fastgltf::Extensions::KHR_draco_mesh_compression)` — `AssetLoader.cpp:408`. Если `primitive.dracoCompression != nullptr` → `DecodeDracoPrimitive(asset, primitive, outData, &err)`.

**Meshopt шаги (`MeshBaker.cpp:56-87`):**
1. `meshopt_optimizeVertexCache` — reorder indices for vertex cache locality
2. `meshopt_generateVertexRemap` — vertex deduplication
3. `meshopt_remapVertexBuffer` / `meshopt_remapIndexBuffer` — apply remap
4. `meshopt_optimizeVertexFetch` — compact vertices
5. `meshopt_analyzeVertexFetch` — overfetch ratio (BakedMesh.overfetch)

**Baked mesh struct:** `BakedPrimitive { vertexBuffer, indices, vertexCount, indexCount, materialIndex, overfetch }`. `BakedMesh { primitives, acmr, atvr }` (acmr = Average Cache Miss Ratio, atvr = Average Transform-to-Vertex Ratio).

**Asset manifest format:** `PROJECTV_MODELS=pathA.glb@x,y,z;pathB.glb@x,y,z,rx,ry,rz,s;pathC.glb`. Id = basename без расширения по умолчанию.

**Audio engine (miniaudio):**
- Format: 16-bit signed PCM, 44.1 kHz, stereo
- Linux: PulseAudio backend → `pipewire-pulse` shim
- 3 состояния: `Stopped` / `Playing` / `Paused`
- 5-секундный refresh плейлиста
- Парсер `Artist - Title.mp3` → (artist, title) для HUD
- 2 MP3 в `music/`: `Le1t - Palm Trees.mp3`, `Le1t - aCID.mp3`
- Хоткеи: `Q` play/pause, `E` stop, `7`/`8` громкость, `9`/`0` next/prev

**Snapshot мира (`src/voxel/VoxelWorld.cpp:17-20`):**
- Magic: `PVSNAP01` (8 байт ASCII)
- 80-байтный header: `magic[8]`, `version=1` (u32), `voxelByteCount` (u32), `reserved` (u32), `scenePreset` (u8) + `reservedBytes[3]`, `config` (24 B), `min`, `maxExclusive`, `editVersion`
- `SaveVoxelWorldSnapshot` / `LoadVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (Tier 1.B)
- Хоткеи: F6 save, F7 load

**Hot shader reload (`src/app/main.cpp:68-114`):**
- Клавиша `1` → `RebuildAllShadersFromDisk()`
- Получает `PROJECTV_BUILD_DIR` (env) или `PROJECTV_CMAKE_BUILD_DIR` (compile-time default)
- `cmake --build $BUILD_DIR --target Shaders` (recompiles `.comp/.frag/.vert` через `glslc`)
- Log path: `std::filesystem::temp_directory_path()/projectv_shader_reload.log` (cross-platform)
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Stderr: `[ProjectV][App] HotReloadShaders: re-built shaders, requested ray-march pipeline recreate`

**Roadmap (Phase 4-9, per `docs/DefenseReport.md §3`):**
- Phase 4: Networking (server-authoritative + client prediction)
- Phase 5: SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader)
- Phase 6: HDR-текстуры + полный клеточный автомат жидкости на GPU
- Phase 7: Полная система частиц + асинхронная загрузка ресурсов
- Phase 8: Плагины / моддинг API
- Phase 9: Многопользовательский режим / сеть (academic vision)

**5 отложенных пунктов:** частицы, моддинг, async load, HDR, SVO.

**Известный дефект (на момент защиты):** BUG-005 cycle scene race (гонка дескрипторов при переключении сцен) — частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`. **Не путать с BUG-004** (отвергнут, не существует).

Подробнее — `docs/DefenseCompetency_FAQ.md §5` (textbook для Тиммейта 5).

---

## 6. Вне зоны ответственности (к кому перенаправлять в Q&A)

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

---

**Конец памятки.** Перед защитой: прочитать §2 вслух 3 раза с таймером, уложиться в 0:40 ± 5 секунд. §5 прочитать отдельно для Q&A.
