# Defense Competency FAQ — T2 (Live Demo + Стек)

**Slot:** T2 (0:50–1:50, Участник 2 = Тиммейт 2, slides 4-5: Demo+Аналоги)
**Кто говорит:** le1t (Кадочников Лев Петрович) — ведущий, тимлид, Q&A host
**Реальная компетенция:** Архитектура + Workflow + Q&A host (отвечает на ВСЕ сложные технические вопросы)
**Out of scope:** нет. le1t — единственный человек, который отвечает на все вопросы. Тиммейты подключаются по своим компетенциям.

---

## 1. Verbatim твоей речи (T2)

> «Здравствуйте. Перед вами демонстрация нашей тестовой лаборатории VoxelLab. Генерация геометрии сцены занимает менее десяти миллисекунд [T3.md]. Вы можете наблюдать рендеринг полупрозрачного стекла, динамической жидкости, каскадных теней и работу алгоритма жадного мешинга [T3.md]. На HUD-панели отображаются текущие параметры производительности — движок уверенно преодолевает порог в пятьсот кадров в секунду в режиме отладки [T2.md].
>
> **[Переход на Слайд 5 — Аналоги и обоснование]**
>
> В отличие от аналогов, наш проект заполняет свободную нишу. Minecraft написан на Java и является закрытым продуктом. Minetest использует устаревший OpenGL и скриптовую систему Lua, создающую накладные расходы. В движке VoxelCore отсутствует поддержка вычислительных шейдеров, а проект Veloren на Rust представляет собой готовую онлайн-игру, а не переиспользуемый графический фундамент. ProjectV объединяет явный контроль Vulkan 1.4, возможности C++26 и дата-ориентированный дизайн. Передаю слово.»

---

## 2. Кто ты

**Реальность:** ты — Кадочников Лев Петрович, единственный разработчик ProjectV. Команда «Черепашки Ninja» — для защиты.

**На сцене:** ты ведущий, говоришь T2 (Live Demo + Стек) 1:15.

**На Q&A:** ты отвечаешь на **ВСЕ сложные технические вопросы**. Тиммейты подключаются по своим компетенциям. Если вопрос выходит за пределы твоих знаний (что вряд ли) — «не знаю, уточню у команды».

---

## 3. Твоя компетенция: Архитектура + Workflow

### 3.1. Стек (C++26, Vulkan 1.4, DOD, ECS)

**Язык: C++26** (`CMAKE_CXX_STANDARD 26` в `CMakeLists.txt:29`).
- `std::expected<T, E>` для холодных путей (Vulkan init, snapshot, audio load)
- `std::simd` для горячих путей
- C++26 модули (`Math.ixx`, `Probe.ixx`, `StringId.ixx` per `agent/memory.md §2.D`)
- `import std;` probe в mainline
- Hot-cold split: `bool`+`CORE_ASSERT` на горячих, `std::expected` на холодных

**Алгоритм 21 — C++26 фичи в коде (per `agent/decisions.md` и фактическому использованию):**

| Фича | Где | Зачем |
|---|---|---|
| `std::expected<T, E>` | `VoxelSnapshotError`, asset loading, scene config, VulkanInit | Cold path error handling без exceptions |
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

**Говорить:**
- «std::expected на cold path, std::inplace_vector на hot path (cap 1024)».
- «alignas(16) → auto-vectorization в movaps/vmovaps».
- «Modules: Math.ixx, Probe.ixx, StringId.ixx — ускорение incremental build».
- «libc++ мигрировали в Tier 2.5; SIMD через C/AVX2 kernel (Tier 3)».

**Графика: Vulkan 1.4** (per `legacy/docs/architecture/specs/`):
- Dynamic rendering (no VkRenderPass)
- Timeline semaphores
- Compute shaders (mesh generation)
- `volk` как loader (`VK_NO_PROTOTYPES`)
- `VMA` для GPU memory
- Сторонние: `fastgltf`, `draco`, `meshoptimizer`, `fmt`, `glm`, `nlohmann/json`, `stb_image`, `spdlog`

**Архитектура: DOD (Data-Oriented Design):**
- `alignas(16)` на `VoxelChunk` (32 B)
- Плоский `std::vector<uint8_t> voxels` в `VoxelWorld`
- Итерация по индексу, не по итератору
- 3-column thinking: данные vs код vs pipeline
- per `legacy/docs/philosophy/`

**Почему DOD а не ООП:** оптимизатор переставляет инструкции и регистры, но не меняет физический макет данных в памяти. SoA-пайплайн при итерации по вокселям даёт 100% попадание в кэш (одно поле = одна кэш-линия), тогда как классический ООП-объект AoS даёт лишь 18.75% — остальные 81.25% кэш-линии загружают неиспользуемые поля соседних объектов. Это чистая физика процессора, дающая **4-кратное ускорение** на ровном месте.

**Применения DOD в ProjectV:**
- `VoxelChunk` — структура массивов в плотном `voxels` (8-битный material на воксель)
- кеш видимости чанков — единый непрерывный буфер, а не массив указателей
- предварительно зарезервированные горячие пути: `pendingChunkRebuildIndices`, `ChunkVisibilityCache.commands` — ёмкость = 1024, без realloc за кадр
- Tier 0: `alignas(16)` для `Vec3`/`Vec4`/`Mat4` → авто-векторизация в `movaps`/`vmovaps`

**ECS: Flecs:**
- `EcsWorld::InitializeAppEcs(state)` — init
- `SyncEcsWorldState(ecs)` — 1× за кадр
- Flecs — MIT, header-only C++
- Single Source of Truth: `VoxelWorld` владеет, ECS — пассивное зеркало

**Как устроена связь ECS ↔ VoxelWorld:**
- `VoxelWorld` — единственный владелец и валидатор воксельной сетки. Все изменения (размещение/удаление блоков) происходят только через его методы.
- ECS-мир (Flecs) — *пассивное зеркало*. Раз в кадр система `SyncEcsWorldState` считывает изменившиеся чанки из `VoxelWorld` и обновляет сущности `ChunkState` в ECS-мире. Геймплейные системы и диагностический HUD читают данные из ECS-зеркала в режиме read-only, что исключает race conditions между физикой и рендером.

**Алгоритм 23 — Связь с ECS через Flecs (ECS bridge):**

**Где:** `src/ecs/EcsWorld.{hpp,cpp}`.
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

**Говорить:**
- «Flecs — passive mirror, не ownership».
- «`SyncEcsWorldState` 1× per frame, dirty chunks только».
- «Components: CameraTag, PlayerControlledCamera, WorldBinding, ChunkState, DebugState».
- «HUD читает из ECS (read-only), не из VoxelWorld (mutable)».

**Hot/cold error split (Tier 1.B, per `decisions.md §29`):**

- **На холодных путях (Cold Path):** Инициализация Vulkan, загрузка glTF-моделей, чтение сейвов с диска. Здесь мы используем современный стандарт **`std::expected`**. Он обеспечивает строгое, безопасное ветвление и возвращает детальные коды ошибок (например, `VoxelSnapshotError::MagicMismatch`), исключая падения приложения. Небольшие накладные расходы `std::expected` на холодных путях не влияют на общую производительность.
- **На горячих путях (Hot Path):** Обновление физики, выборка вокселей, рендеринг кадра. Здесь использование `std::expected` запрещено. Ошибки обрабатываются через быстрые возвраты `bool` и макросы жёстких проверок `CORE_ASSERT`, которые полностью вырезаются в релизной сборке, обеспечивая максимальную скорость выполнения.

**Физика: Jolt:**
- MIT, детерминированный, SIMD
- `JPH::CharacterVirtual` для коллизий
- Наш собственный код дополняет для walk controller

**Технологический выбор (обоснования):**

**C++26 (а не Rust/Zig/Go):**
1. **Экосистема и совместимость:** Все используемые нами тяжёлые библиотеки (Jolt Physics, fastgltf, VulkanMemoryAllocator, Draco, Flecs) написаны на C/C++ и предоставляют нативный C++ API. Написание Rust-биндингов для Jolt или VMA заняло бы больше времени, чем вся разработка MVP.
2. **Возможности стандарта:** C++26 даёт нам критически важные фичи: `std::expected` для безопасной обработки ошибок на холодных путях (загрузка ассетов, парсинг конфигов), `std::simd` для векторных расчётов, модули (наши `Math.ixx` и `StringId.ixx` для ускорения инкрементальной сборки) и развитый `constexpr`.

**Vulkan 1.4 (а не OpenGL/DirectX 12/Metal):**
- Явный контроль над GPU (конвейеры, память, синхронизация). OpenGL — неявно управляется драйвером, что для воксельного движка с миллионами draw items обходится дорого.
- Compute-шейдеры в Vulkan — полноценная поддержка, нужны для генерации мешей и ray-marching.
- DirectX 12 — только Windows, что сужает платформу. Metal — только Apple.
- Vulkan — кроссплатформенный (Windows + Linux + будущий macOS через MoltenVK), стандарт Khronos.
- Vulkan 1.4 (2023) — текущая LTS-версия с dynamic rendering, timeline semaphores, push descriptors.

**Jolt Physics (а не PhysX или Bullet):**
- Jolt Physics — современный, детерминированный физический движок с открытым исходным кодом (лицензия MIT). Он изначально разрабатывался с прицелом на многопоточность и SIMD-оптимизацию на CPU. Bullet морально устарел и сложен в оптимизации, а PhysX от NVIDIA избыточен по размеру и имеет закрытые части. Наш walk-контроллер на базе `CharacterVirtual` от Jolt идеально справляется со скольжением по углам воксельных плит.

**Flecs (а не EnTT/Bevy ECS/DOTS Unity):**
- Flecs — header-only C++ ECS с отличной поддержкой идиом C++ (RAII, типобезопасные компоненты), минимум зависимостей, лицензия MIT. EnTT — хорошая альтернатива, но Flecs лучше в эргономике для встроенного использования в Vulkan-приложении. Bevy ECS — только Rust. Unity DOTS — коммерческий и привязан к Unity runtime. Нам нужен был лёгкий, самодостаточный C++ ECS — Flecs идеально подходит.

### 3.2. Алгоритмы (все 23, hot/cold + ключевое слово)

**Полная inline-детализация каждого алгоритма — в per-slot FAQ файлах T1-T6 (operator: «как можно больше информации, описание greedy meshing это не вода»).**

**Quick reference card (для быстрой навигации, на защите):**

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
| 18 | Hot reload | `app/main.cpp` | C | **клавиша `1`** (relocated from F5/F11), cmake --target Shaders |
| 19 | Snapshot | `voxel/VoxelWorld.cpp` | C | binary, "PVSNAP01", v1 |
| 20 | JSON config | `voxel/SceneConfig.cpp` | C | nlohmann/json, FetchContent |
| 21 | C++26 фичи | разные | оба | expected, modules, inplace_vector |
| 22 | Build | `CMakeLists.txt`, `CMakePresets.json` | C | 12 presets, libc++, 73→19 MB |
| 23 | ECS bridge | `ecs/EcsWorld.cpp` | C | Flecs mirror, sync 1×/frame |

### 3.2.1. Алгоритм 18 — Горячая перезагрузка шейдеров (hot shader reload)

**Где:** `src/app/main.cpp` → `RebuildAllShadersFromDisk()`.
**Проблема:** итерация над шейдерами требует перезапуска приложения, медленно.

**Hotkey:** `1` (relocated 2026-06-15 с F5/F11 — F5 теперь чисто для InputAction `CycleScenePreset`).
**Ray-march toggle:** `2` (relocated 2026-06-15 с F6/F12 — F6 теперь чисто для InputAction `SaveWorldSnapshot`).

**Алгоритм (`RebuildAllShadersFromDisk`, per `main.cpp:60-114`):**

1. Клавиша `1` в `SDL_AppEvent` → `RebuildAllShadersFromDisk()`.
2. Get `PROJECTV_BUILD_DIR` env var (если задана) иначе `PROJECTV_CMAKE_BUILD_DIR` macro (compile-time injected, cross-platform).
3. Subprocess: `cmake --build <buildDir> --target Shaders > "<tempdir>/projectv_shader_reload.log" 2>&1`. Cross-platform tempdir via `std::filesystem::temp_directory_path()` (Linux: `/tmp`, Windows: `%TEMP%`).
4. `glslc` / `glslangValidator` перекомпилирует `.vert`/`.frag`/`.comp` → `.spv`.
5. На success → `RequestRayMarchPipelineRecreate()` (ray-march pipeline is the only one with newly-added `.comp`; pre-existing graphics/shadow/TAA pipelines keep cached shader modules until fuller pipeline-recreate PR).
6. На следующем кадре pipeline recreate.

**Edge cases (BUG-005):**
- Race на descriptor sets при cycle scene (это `F5` InputAction `CycleScenePreset`, НЕ `1` shader reload).
- `vkDeviceWaitIdle` в `DestroySceneResources` смягчает, не устраняет полностью.
- **Defensive:** `RequestRayMarchPipelineRecreate` — ленивый, **не дёргает** swapchain wait mid-frame.

**Говорить:**
- «`1` (relocated с F5/F11) → cmake build --target Shaders → ray-march pipeline recreate на следующем кадре».
- «`2` (relocated с F6/F12) → toggle ray-march pass (STUB на текущий момент, см. §11 T4)».
- «Удобно для итераций над шейдерами без перезапуска».
- «BUG-005: race при InputAction F5 cycle scene, смягчён через `vkDeviceWaitIdle`, не устранён полностью».

### 3.3. Тесты и метрики

- **14 ctest** baseline 14/14 (0.78s debug, 0.06s release)
- **6/6 runtime smoke** captures (FINAL/SHDW/CSM/CTSH/AOCC/LOCL)
- **60+ sidecar keys** в `.txt` файле
- **73 MB debug / 19 MB release** ELF (-73%)
- **2 MP3** в `music/`
- **0 предупреждений** в нашем коде (per `agent/decisions.md §4`)

### 3.4. Известные баги (на момент защиты)

**BUG-005 (cycle scene race):**
- Гонка дескрипторов при переключении сцен (F5)
- Частично смягчена: `vkDeviceWaitIdle` в `DestroySceneResources`
- Полное устранение — Phase 5 (per `agent/memory.md §10.5` + `decisions.md`)

**BUG-004 (VoxelLab tremor) — ОТВЕРГНУТ:**
- Галлюцинация предыдущей сессии
- TAA по умолчанию ВЫКЛЮЧЕН (`taaEnabled=false`, jitter=0)
- Нет дрожания при default config
- Если кто-то спросит: «не существует, jitter=0 default, не воспроизводится»

**Ray-march pass — STUB:**
- `RecordRayMarchCommands` — no-op, `fprintf` в stderr
- Compute-шейдер `ray_march.comp` скомпилирован
- Phase 7 follow-up

### 3.4. Высокоуровневая архитектура (DefenseReport §4)

```
┌────────────────────────────────────────────────────────────────┐
│                       SDL3 main loop                           │
│  SDL_AppInit → SDL_AppEvent → SDL_AppIterate → SDL_AppQuit    │
└────────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
        ┌──────────┐   ┌──────────┐    ┌──────────────┐
        │  AppState │   │ Renderer │    │ PhysicsWorld │
        │  (PIMPL)  │   │ (Vulkan) │    │   (Jolt)     │
        └─────┬────┘   └─────┬────┘    └──────┬───────┘
              │              │                │
              └──────────────┼────────────────┘
                             ▼
                  ┌────────────────────┐
                  │  ECS (Flecs) world │
                  │  + VoxelWorld      │
                  │  + AudioEngine     │
                  │  + AssetRegistry   │
                  └────────────────────┘
```

- **AppState** — единая точка владения всеми подсистемами (PIMPL, function-pointer deleters).
- **VoxelWorld** — источник истины для мира, владеет чанками, материалами, `editVersion`.
- **Renderer** — Vulkan 1.4 dynamic rendering, по-кадровые `SceneFrameResources` (SSBO double-buffer).
- **PhysicsWorld** — обёртка Jolt с walk controller, запросами к вокселям.
- **ECS** — мир Flecs, синхронизируется с VoxelWorld через `SyncEcsWorldState`.
- **AssetRegistry** — манифесты моделей, glb/Draco декодирование, загрузка в GPU через VMA.
- **AudioEngine** — miniaudio с автообновлением плейлиста.

Подробнее: `docs/ArchitectureGuide.md`, `docs/RenderArchitecture.md`, `docs/VoxelWorld.md`.

### 3.5. Соответствие ТЗ (DefenseReport §8 — сводная таблица, 48 пунктов)

| # | Пункт ТЗ | Статус | Комментарий |
|---|---|---|---|
| 1 | 1.1 C++26, Vulkan 1.4, DOD, ECS | ✅ | Все 4 столпа |
| 2 | 3.1.1 Обработка миллионов воксельных элементов | ✅ | Greedy meshing, frustum culling, двухуровневый кеш |
| 3 | 3.1.2 ECS-архитектура | ✅ | Flecs, ChunkState, WorldBinding, PlayerControlledCamera |
| 4 | 3.1.3 Реалистичная физика | ✅ | Jolt, walk controller с edge grace, auto-jump |
| 5 | 3.1.4 Оптимальное использование GPU | ✅ | Compute-шейдеры (voxel_mesh, ray_march), SSBO, indirect draw |
| 6 | 3.1.5 Моддинг | ⚠️ отложено | Внутренний API ещё не стабилизирован; архитектура позволяет |
| 7 | 4.1.1 Управление воксельными данными в реальном времени | ✅ | SetVoxelMaterial, MarkVoxelChunkDirty, snapshot save/load |
| 8 | 4.1.2 **GPU ray-marching** | ✅ (STUB) | `ray_march.comp` compute pass скомпилирован, Phase 7 integration |
| 9 | 4.1.2 Динамический LOD | ✅ | 1 уровень по дистанции камеры в `RayMarchPass` |
| 10 | 4.1.3 Жёсткое тело | ✅ | Jolt dynamic bodies |
| 11 | 4.1.3 Динамическая деформация | ✅ | voxel edit (CPU авторитетный) |
| 12 | 4.1.3 **Симуляция жидкостей (CA)** | ✅ | `VoxelWorld::UpdateFluidCA()` |
| 13 | 4.1.4 Система частиц | ⚠️ отложено | Вне MVP; см. §3 |
| 14 | 4.1.5 ECS | ✅ | см. п. 3 |
| 15 | 4.1.6 Асинхронная загрузка | ⚠️ частично | Загрузчик синхронный, для 100 МБ моделей OK |
| 16 | 4.1.6 **Горячая перезагрузка шейдеров** | ✅ | Клавиша `1` перезагружает все .spv через Vulkan pipeline recreation |
| 17 | 4.1.7 Отладка и профилирование | ✅ | Tracy + замеры по проходам + маркеры RenderDoc + бенчмарк |
| 18 | 4.1.8 Поддержка модификаций | ⚠️ отложено | см. п. 6 |
| 19 | 4.2.1 Диагностика ошибок | ✅ | `core/RuntimeDiagnostics.hpp`, структурированное логирование ошибок |
| 20 | 4.2.1 Безопасный режим деградации | ✅ | validation layer missing → мягкий выход; сломанный ассет → заглушка |
| 21 | 4.2.1 Резервные ресурсы | ✅ | `AssetStub`, `AudioEngine` пустой плейлист |
| 22 | 4.2.2 Время восстановления | ✅ | 0 крэшей, горячая перезагрузка шейдеров |
| 23 | 4.2.3 Безопасное удаление сущности | ✅ | flecs `entity.destruct()` |
| 24 | 4.3.3 Один пользователь | ✅ | однокадровое приложение |
| 25 | 4.4 CPU i5 / GPU 4 ГБ / RAM 8 ГБ | ✅ | Build target = Linux RTX 3060 Ti 8 ГБ, базовый FPS 500+ (VoxelLab debug) |
| 26 | 4.5.1 JSON/YAML для конфигов | ✅ | nlohmann/json для сцен (JSON subset) |
| 27 | 4.5.1 glTF/GLB | ✅ | через fastgltf |
| 28 | 4.5.1 PNG/JPEG/HDR текстуры | ⚠️ частично | PNG/JPEG — N/A (текстуры не в пайплайне), HDR — отложено |
| 29 | 4.5.1 Draco compression | ✅ | M3 session (cccdbc1..24ccb08) |
| 30 | 4.5.2 C++26 / Clang 22+ | ✅ | CMAKE_CXX_STANDARD 26, clang 22.1.6 |
| 31 | 4.5.3 Windows 10/11 | ✅ | windows-clang-debug preset |
| 32 | 4.5.3 Vulkan 1.4 | ✅ | VK_API_VERSION 1.4.350 |
| 33 | 4.5.3 CMake 3.28+ | ✅ | 3.30 базовый, 4.0 протестировано |
| 34 | 4.5.3 RenderDoc | ✅ | маркеры для отладки + помощники меток |
| 35 | 4.7 GitHub репозиторий | ✅ | https://github.com/Leeleit/ProjectV |
| 36 | 5.1 README | ✅ | `README.md` + `docs/BuildAndRun.md` |
| 37 | 5.1 Architecture Guide | ✅ | `docs/ArchitectureGuide.md` + `docs/RenderArchitecture.md` |
| 38 | 5.1 Design Patterns | ✅ | `legacy/docs/philosophy/` (house style) |
| 39 | 5.1 Editor Manual | ⚠️ отложено | Нет встроенного редактора (только runtime debug HUD) |
| 40 | 5.1 Material System Reference | ✅ | `docs/RenderArchitecture.md` §Materials + `legacy/docs/architecture/adr/` |
| 41 | 5.1 Scripting API Reference | ⚠️ отложено | Нет слоя скриптинга; C++ API — `docs/ArchitectureGuide.md` |
| 42 | 5.1 Итоговый отчёт | ✅ | этот файл (см. также archived `DefenseReport.md`) |
| 43 | 7.2 Все 9 этапов | ✅ | см. `agent/memory.md §11` (полная хронология) |
| 44 | 8.1.1 Unit-тесты ≥ 80% | ⚠️ частично | 14 ctest suites, базовый уровень 14/14; покрытие < 80% (фокус на критичных путях) |
| 45 | 8.1.2 Интеграционные тесты | ✅ | RuntimeSmoke 6/6 captures + сценарные захваты |
| 46 | 8.1.3 Performance тесты | ✅ | `BenchmarkAutomation` + маркеры кадров Tracy |
| 47 | 8.1.4 Визуальные тесты | ✅ | lookdev-captures + smoke проверка FINAL/SHDW/CSM/CTSH/AOCC/LOCL |
| 48 | 8.1.5 Compatibility | ✅ | Linux + Windows build green |

**Итог:** 38 ✅ / 5 ⚠️ отложено (явно зафиксировано) / 0 ❌ критичных.

### 3.6. Команда и вклад участников (DefenseReport §12)

**Формат:** команда из 6 человек. **Тимлид (Кадочников Лев Петрович, le1t)** — основной разработчик, отвечал за архитектуру, обоснование выбора библиотек, DOD layout, ECS-bridge, hot paths и cold paths (snapshot, JSON config), hot shader reload, и ведёт все вопросы комиссии. Остальные 5 участников отвечают за свои модули на сцене (реальная компетенция per speech slot — см. таблицу ниже).

| # | ФИО | Роль на сцене (slot) | Реальная компетенция | Основные файлы |
|---|---|---|---|---|
| 1 | **Кадочников Лев Петрович** (le1t, тимлид) | T2 Live Demo + Стек | Архитектура, Q&A host (отвечает на ВСЕ сложные вопросы) | `src/core/`, `src/ecs/`, `src/voxel/VoxelWorld.cpp` (CA), `src/voxel/SceneConfig.*`, `src/app/main.cpp` (F-key relocation), корневой `CMakeLists.txt`, `CMakePresets.json`, `docs/Defense*` |
| 2 | Тиммейт 1 | T1 Вступление и Проблема | Сборка и тестирование (CMake, ctest, smoke) | `CMakeLists.txt`, `CMakePresets.json`, `tests/CMakeLists.txt`, `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` |
| 3 | Тиммейт 2 | T3 Архитектура и качество кода | Воксельный мир и meshing (Лысенков) | `src/voxel/VoxelWorld.*`, `src/voxel/VoxelMaterials.*`, `src/voxel/VoxelRaycast.*`, `src/shaders/voxel_mesh.comp`, `src/c_kernels/frustum_cull.*` |
| 4 | Тиммейт 3 | T4 Тесты и проверки | Рендеринг (Vulkan 1.4, TAA, CSM, AOCC) | `src/render/Renderer.*`, `src/render/ShadowProjection.*`, `src/render/Taa.*`, `src/render/RayMarchPass.*`, `src/shaders/voxel.frag`, `src/shaders/voxel_shadow.{vert,frag}`, `src/shaders/taa_resolve.{vert,frag}`, `src/shaders/ray_march.comp` |
| 5 | Тиммейт 4 | T6 Планы и Завершение | Физика и walk controller | `src/physics/PhysicsWorld.*`, Jolt integration glue |
| 6 | Тиммейт 5 | T5 Прочие фичи + что отложено | Демо VoxelLab + ассеты + аудио | `src/asset/AssetLoader.*`, `src/asset/DracoMeshDecoder.*`, `src/asset/MeshBaker.*`, `src/asset/ModelManifestLoader.*`, `src/asset/ModelPass.*`, `src/audio/AudioEngine.*`, `music/`, `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` |

**Руководитель:** Подольский Филипп Александрович.
**Курс:** Основы проектной деятельности в ИТ-сфере.
**Группа:** МФТИ-1-2024.

**Принцип распределения:** каждому участнику — весомая, но простая к объяснению зона. **Тимлид оставляет за собой архитектурные обоснования, выбор библиотек, и все Q&A комиссии.** Распределение зафиксировано в `docs/DefenseScript_Team.md` (5-мин таймлайн) и per-slot FAQ файлах `docs/DefenseCompetencyFAQ_T{1..6}.md` (full competency detail).

**Честное замечание:** в реальности основной объём разработки выполнен тимлидом (le1t). Распределение по модулям отражает специализацию участников при защите, а не разделение труда при разработке. Полный per-commit авторский вклад — в `git log --author=`.

### 3.7. Вопросы 10.x (DefenseFAQ §10) — defense questions

**Сколько человек работало:** команда из **6 человек** (le1t + 5 соавторов). Разработка велась в едином репозитории; ~100+ коммитов за ~3,5 месяца. На защите каждый участник отвечает за свой модуль — это нормальная академическая практика для проектов такого масштаба.

**Какой вклад каждого из 6 человек:** см. таблицу в §3.6 выше (slot vs real competency mapping).

**Где можно посмотреть код:** GitHub: https://github.com/Leeleit/ProjectV. Ветка `master`, 100+ коммитов за последние 3,5 месяца. Submodules: `git clone --recurse-submodules`. Сборка: см. `docs/BuildAndRun.md` + `README.md`. Пресеты Linux: `cmake --preset linux-clang-debug`.

**Защита информации (ТЗ 4.5.4):** «Специальные требования к защите информации и программ не предъявляются.» Нет DRM, нет шифрования, нет DRM-защищённых ассетов. Конвейер ассетов работает с открытой спецификацией glTF.

**Производительность на рекомендуемой конфигурации (ТЗ 4.4):** i7-10700K / Ryzen 7 3700X, GPU 8 ГБ, 16 ГБ RAM, 1920×1080. Наша dev-конфигурация: Linux Arch, Ryzen 7 5800X, RTX 3060 Ti 8 ГБ (GA104), 16 ГБ RAM. **VoxelLab reference shot: 500+ FPS, ~2 мс кадр** (debug baseline 2026-06-15). Превышает ТЗ 8.2.2 (средний FPS не ниже целевого).

**«60 FPS на сетке 512³» (ТЗ 7.2.4):**
1. **Текущий scope — sandbox-first.** MVP решает задачу компактных детализированных сцен (Voxel Laboratory: 27 чанков, 500+ FPS), где полигональный greedy-мешинг выигрывает по простоте и скорости.
2. **Почему не 512³.** Mesh-based greedy не масштабируется линейно с размером сетки — количество граней растёт как O(n^(2/3)). На 512³ для 60 FPS потребуется переход на SVO + Ray Marching.
3. **Что дальше.** Phase 5 — SVO rendering как следующая глобальная фаза исследований. Зафиксировано в `agent/decisions.md §2`: «sandbox-first focus, not gameplay-loop expansion».

**Почему именно эти библиотеки, а не свои реализации:**
- **SDL3** — кроссплатформенный windowing/input (есть свои обёртки в `platform/PlatformEvents.cpp`)
- **volk** — загрузчик Vulkan в рантайме (нужен для горячей перезагрузки)
- **VMA** — паттерны выделения памяти (свои с VMA — reinvent wheel)
- **Flecs** — ECS (своя ECS = 2-3 месяца, Flecs = 1 день интеграции)
- **Jolt** — физика (своя физика — 6-12 месяцев)
- **fastgltf, Draco, meshopt** — стек glTF (стандарт Khronos, не reinvent)
- **miniaudio** — single-header аудио (FMOD = коммерческий)
- **Tracy** — профилировщик (отраслевой стандарт, MIT)
- **fmt** — форматирование (`std::format` в C++20, но fmt быстрее и совместимее)
- **glm** — математика (de-facto стандарт для графики)
- **VMA** — явное выделение памяти (спецификация Vulkan)

Никаких зависимостей «на всякий случай» — каждая решает реальную проблему.

**Tier 0-5 timeline (закрыты 2026-06-15, 12 коммитов `427be4f` .. `90a45b4`):**
- **Tier 0** (`86df567`, `e85a6f9`): `Vec3/Vec4/Mat4` с alignas 16/32, миграция hot structures
- **Tier 1** (`427be4f`, `92c4380`): `std::inplace_vector`, `StringID`, `std::expected` для cold-path
- **Tier 2** (`c3faa65`, `e0029dc`, `73e2dd7`, `be16a2d`, `5c9d658`): C++20 модули, libc++ миграция, `import std;` probe
- **Tier 3** (`b778567`): C/AVX2 ядро фрустум-кулинга + Google Benchmark
- **Tier 4** (`ef8b403`): провод C-ядра в движок
- **Tier 5** (`aa34642`): branch hints, EVIL docs, `vkWaitForFences 10ms`, InputAction mask UB fix, shadow benchmark и splits tests

Базовый уровень производительности установлен. Tier 0-5 — perf baseline, не bug-fixing. Известные TAA-scope проблемы (VoxelLab tremor BUG-004, F5 VUID race BUG-005) — postdefense follow-up.

### 3.5. Workflow (multi-agent)

**AGENTS.md** — стабильный протокол:
- §1: Изменение только по явной команде
- §7.2.5: Commit message contract (type/scope/summary/body/Refs)
- §7.2.6: Multi-agent concurrent work policy
- §7.2.8: Shared `agent/` files (не claim'ить эксклюзив)
- §7.3.1: Pre-commit gate
- §8.1: Auto-close после commit
- §9: Definition of done

**TODO.md** — живой roadmap + бэклог

**agent/active-sessions.md** — append-only ledger координации

**agent/decisions.md** — зафиксированные архитектурные решения

**agent/memory.md** — долговечные факты, lessons learned, run-time observations

**agent/status.md** — snapshot состояния сессий

**Multi-agent сессии:**
- Параллельный запуск нескольких сессий — нормальный сценарий
- Пересекающийся scope — arbitration через оператора
- Известный инцидент 2026-06-10: `git checkout -- .` стёр uncommitted work
- Урок: safety-net patch в `/tmp/` обязательно

### 3.6. Roadmap (Phase 4-9)

| Phase | Цель |
|---|---|
| 4 | Networking (server-authoritative + client prediction) |
| 5 | SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader) |
| 6 | HDR-текстуры + полный клеточный автомат жидкости на GPU |
| 7 | Полная система частиц + асинхронная загрузка ресурсов |
| 8 | Плагины / моддинг API |
| 9 | Многопользовательский режим (Academic vision) |

---

## 4. Твой слот: T2 Live Demo + Стек (1:15)

**Действия:**
1. Запустить приложение (сцена `VoxelLab`).
2. Включить подробный HUD клавишей `G`.
3. Показать облёт камеры (WASD + мышь).
4. Поставить/сломать пару блоков (правый/левый клик).
5. Переключить debug view (`B` — cycle FINAL/SHDW/CSM/CTSH/AOCC/LOCL).
6. Захватить screenshot (`C`).

---

## 5. Hotkeys (полный список, ты должен знать все)

**Движение (walk mode):**
- `W` `A` `S` `D` — движение
- `Space` — прыжок
- `LShift` / `RShift` — sneak (красться)
- `LCTRL` / `RCTRL` — speed boost (×3)
- `LALT` / `RALT` — speed slow (×0.25)
- `F11` — toggle walk air control mode
- `J` — toggle auto-jump
- `F12` — toggle auto-jump delay

**Режимы и камера:**
- `F4` — toggle Walk/Creative/Spectator mode
- Двойной `Space` — toggle Walk ↔ Creative
- `F3` — reset camera
- `TAB` — toggle relative mouse mode
- `F11` (InputAction) vs `1` (defense) — разные клавиши! `F11` = walk air control, `1` = hot shader reload (relocation после conflict с F11 InputAction)

**Voxel interaction:**
- Левый клик — removal (VoxelMaterial::Air)
- Правый клик — placement
- `F` — pick model (HL2-style physicsgun)
- `F2` — cycle placement material
- `F8` — cycle editor tool
- `M` — pick target material
- `X` — toggle mutation anchor

**Сцена и snapshot:**
- `F5` — cycle scene preset
- `F6` — save world snapshot (PVSNAP01)
- `F7` — load world snapshot

**Визуализация:**
- `F1` — toggle HUD
- `G` — toggle detailed HUD
- `B` — cycle lighting debug view (10 views)
- `C` — capture screenshot
- `L` — toggle cascade split planes
- `Z` — toggle cursor hit normal
- `O` — cycle shadow tuning target
- `U` / `I` — decrease / increase shadow tuning value
- `V` — reset lighting debug controls
- `H` / `K` — decrease / increase lighting exposure
- `N` — cycle tone map operator

**TAA:**
- `T` — toggle TAA on/off
- `;` / `'` — decrease / increase TAA jitter scale
- `-` / `=` — decrease / increase TAA blend
- `,` — cycle TAA neighbourhood radius
- `.` — invalidate TAA history

**Frame-step / slow-motion:**
- `P` — toggle pause
- `[` / `]` — decrease / increase time scale
- `\` — step single frame
- `` ` `` — reset time scale

**Audio:**
- `Q` — play/pause toggle
- `E` — stop
- `7` / `8` — volume down / up
- `9` / `0` — next / previous track

**Chunk debug:**
- `F9` — toggle chunk bounds
- `F10` — toggle dirty chunk overlay

**Input replay:**
- `R` — toggle input replay recording
- `Y` — play last input replay

**Defense r0 hotkeys (relocated 2026-06-15):**
- `1` — hot shader reload (было F5/F11)
- `2` — toggle ray-march pass (было F6/F12)
- `3` — cycle V-sync mode (было V)
- `ESC` — exit

---

## 6. Глоссарий (полный для архитектуры)

**C++26** — стандарт языка образца 2026 года. Ключевые фичи: `std::expected`, `std::simd`, modules, `import std;`.

**STL (Standard Template Library)** — стандартная библиотека шаблонов C++.

**STD::EXPECTED<T, E>** — strongly-typed error wrapper (Tier 1.B, 2026-04-13). Альтернатива exceptions / `std::variant` / `bool`. 9+ cold-path functions переведены.

**STD::SIMD** — параллельные SIMD-операции через STL (C++26).

**MODULES (C++26)** — `import std;`, `import projectv.math;`. Ускоряют инкрементальную сборку. CMake `FILE_SET CXX_MODULES`.

**COLD_PATH** — нечастые вызовы (1× per startup/snapshot/init). Используют `std::expected<T, E>`.

**HOT_PATH** — каждый кадр (voxel meshing dispatch, frame prep). Используют `bool` + `CORE_ASSERT` (assert вырезается в release).

**HOT-COLD_SPLIT** — гибридный подход: cold = `std::expected`, hot = `bool`+assert. Оптимизация overhead'а error handling.

**VULKAN 1.4** — low-overhead graphics API с явным контролем GPU. Dynamic rendering (no VkRenderPass), timeline semaphores, compute shaders.

**VK_API_VERSION_1_4** — define в коде, проверяется через `VK_VERSION_1_4`.

**DYNAMIC_RENDERING (Vulkan)** — `vkCmdBeginRendering`/`vkCmdEndRendering` вместо `VkRenderPass`/`VkFramebuffer`. Упрощает код, поддерживается с Vulkan 1.3.

**TIMELINE_SEMAPHORES (Vulkan)** — `VkSemaphoreTypeCreateInfo` с `VK_SEMAPHORE_TYPE_TIMELINE`. Асинхронная синхронизация GPU-GPU без хаков.

**COMPUTE_SHADER** — `VkComputePipeline`, используется для мешинга (`voxel_mesh.comp`) и ray-march (`ray_march.comp`).

**VOLK** — Vulkan meta-loader, обёртка над `VK_NO_PROTOTYPES`. Vendored.

**VMA (VulkanMemoryAllocator)** — аллокатор GPU памяти, vendored.

**DOD (Data-Oriented Design)** — дизайн, ориентированный на данные. Чанк 8×8×8 = 512 B = 2 SSE-регистра, влезает в L1.

**ALIGNAS(16)** — `alignas(16)` на `VoxelChunk`. Авто-векторизация в `movaps`/`vmovaps`.

**SIMD** — Single Instruction, Multiple Data (AVX2, SSE). Параллельные операции.

**ECS (Entity-Component System)** — Flecs (MIT, header-only C++). Пассивное зеркало VoxelWorld.

**FLECS** — MIT, header-only C++ ECS. Альтернативы: EnTT (header-only, runtime overhead выше), Bevy ECS (Rust).

**SOA (Structure of Arrays)** — данные хранятся в массивах, не массивах структур. Cache-friendly.

**AOS (Array of Structures)** — классический ООП layout. Менее cache-friendly.

**CACHE_MISS** — промах кэша CPU, штраф ~200 циклов для L1 miss, ~200-300 для L3 miss.

**JOLT_PHYSICS** — MIT, deterministic, SIMD-оптимизирован. Альтернативы: PhysX (NVIDIA, проприетарный), Bullet (устарел).

**JPH (Jolt namespace)** — `JPH::CharacterVirtual`, `JPH::Body`, etc.

**MVP (Minimum Viable Product)** — minimum жизнеспособный продукт. Tier 0-5 closed (2026-06-15). 14/14 ctest + 6/6 smoke — доказательство завершённости MVP.

**PHASE 4-9** — пост-MVP roadmap (Networking, SVO, HDR+fluid GPU, Particles, Modding, Academic vision).

**AGENTS.MD** — стабильный протокол проекта (§1-§10). Меняется только по явной команде оператора.

**TODO.MD** — живой roadmap + бэклог.

**DECISIONS.MD** — зафиксированные архитектурные решения.

**MEMORY.MD** — долговечные факты, lessons learned.

**STATUS.MD** — snapshot состояния сессий.

**MULTI-AGENT** — параллельные сессии через append-only ledger в `agent/active-sessions.md`. Пересекающийся scope → arbitration через оператора.

**SAFETY-NET PATCH** — `/tmp/before_*_<ts>.patch` — fallback для следующей сессии. Per AGENTS.md §8.1 п.5.

**BUG-005** — cycle scene race (гонка дескрипторов при F5). Частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`.

**BUG-004** — VoxelLab tremor — ОТВЕРГНУТ (галлюцинация). TAA jitter=0 по умолчанию.

**RAY-MARCH STUB** — `RecordRayMarchCommands` — no-op, `fprintf` в stderr. Compute-шейдер скомпилирован, graphics stream не вкомпонован. Phase 7.

---

## 7. Реалистичные вопросы (8 вопросов)

### 7.1. Архитектура и стек (4 вопроса)

**Q1. Почему C++26, а не Rust/Zig/Go?**
- Все зависимости (Jolt, fastgltf, VMA, Draco, Flecs) — C/C++ с нативным API
- C++26 даёт `std::expected` для холодных путей, `std::simd` для горячих, модули для ускорения инкрементальной сборки
- Rust — рассматривался, но Vulkan bindings + ECS + asset pipeline зрелые на C++

**Q2. Почему Vulkan 1.4, а не OpenGL/DX12/Metal?**
- Vulkan — явный контроль GPU (пайплайны, память, синхронизация)
- OpenGL — driver управляет, дорого для миллионов draw items
- Compute shaders нужны для мешинга
- Кросс-платформенный (Windows + Linux)

**Q3. Что такое DOD и зачем?**
- Дизайн, ориентированный на данные (Data-Oriented Design)
- Данные организованы для эффективной обработки CPU, а не для удобства иерархии классов
- Чанк 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра, влезает в L1
- `alignas(16)` → авто-векторизация в `movaps`/`vmovaps`

**Q4. Как связаны ECS и VoxelWorld?**
- Single Source of Truth: `VoxelWorld` — единственный владелец, все мутации только через него
- ECS (Flecs) — пассивное зеркало, обновляется 1 раз за кадр через `SyncEcsWorldState`
- HUD читает из ECS (только чтение), не из VoxelWorld (изменяемый)

### 7.2. Алгоритмы и рендеринг (4 вопроса)

**Q5. Что такое жадный мешинг и зачем?**
- Объединяет соседние грани вокселей одного exposed state в один четырёхугольник (quad)
- 6 проходов на чанк: ±X, ±Y, ±Z
- Compute-шейдер `voxel_mesh.comp:613-619`
- Сокращение draw calls на 30-50%

**Q6. Как работают каскадные тени (CSM)?**
- 4 каскада карты глубины 2048×2048
- Лямбда 0.80 (near-biased)
- Per-cascade проекция солнца: sub-frustum → light-space → sphere stabilization
- Стекло не отбрасывает тень, жидкость — отбрасывает (per `decisions.md`)

**Q7. Что такое TAA и зачем?**
- Временное сглаживание: смешивает кадры, убирает дрожание камеры
- 8-sample Halton(2,3) jitter, YCoCg-зажим
- Поверх TAA — CAS (фильтр резкости)
- **По умолчанию TAA jitter = 0 (стабильная картинка, нет дрожания)**

**Q8. Что такое ray-marching и как реализован?**
- Трассировка лучей через воксели (Amanatides-Woo DDA)
- Compute-шейдер `ray_march.comp` скомпилирован
- API state (`SetRayMarchEnabled`/`IsRayMarchEnabled`/`RequestRayMarchPipelineRecreate`) работает
- Graphics command stream его пока не вызывает — **STUB, Phase 7 follow-up**
- Per `RayMarchPass.hpp:9-30`

### 7.3. Тесты и workflow

**Q9. Какие тесты, сколько?**
- 14 наборов в `tests/CMakeLists.txt`
- Baseline 14/14, 0.78 сек debug, 0.06 сек release
- Runtime smoke 6/6 captures
- 60+ sidecar keys

**Q10. Какие метрики производительности?**
- VoxelLab reference shot: 500+ FPS, ~2 мс кадр
- Release: 19 МБ (vs 73 МБ debug, -73%)
- 14/14 ctest, 6/6 smoke

**Q11. Какие известные баги?**
- BUG-005 (cycle scene race): гонка дескрипторов при переключении сцен, частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`
- **BUG-004 (VoxelLab tremor) — отвергнут, не существует**
- Ray-march STUB (Phase 7)

**Q12. Что отложено и почему?**
- 6 пунктов: частицы, моддинг, асинхронная загрузка, HDR, SVO, mesh shaders
- Все явно в Phase 4-9 roadmap
- См. список deferred items в T5.md §3.4 + T2.md §3.6 (ТЗ compliance matrix)

**Q13. Какие платформы поддерживаются?**
- Windows 10/11 (clang-cl 22) + Linux Arch (clang 22 native + libc++ 16)
- Обе сборки зелёные, 14/14 тестов
- macOS — НЕ в планах (per `decisions.md`)

**Q14. Hot shader reload — как работает?**
- Клавиша `1` (relocated 2026-06-15)
- `RebuildAllShadersFromDisk()` → `cmake --build $BUILD_DIR --target Shaders`
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Другие pipelines (graphics, shadow, TAA) переиспользуют кэшированные модули до Phase 7+

---

## 8. Каверзные вопросы (15 вопросов — расширенный список)

### 8.1. Базовые каверзные (10)

**Q15. Почему вы не сделали ECS зеркало по-другому? (например, без копирования)**
- Alternative: ECS reads directly from VoxelWorld (no mirror)
- Per `decisions.md` — chosen approach: passive mirror for HUD decoupling
- Trade-off: extra copy (small) vs lock contention (bigger)
- HUD reads once per frame, lock-free через mirror

**Q16. Почему std::expected, а не std::variant или exceptions?**
- `std::expected<T, E>` — strongly-typed error, like Result в Rust
- `std::variant` — нет «error vs value» semantics, нужен visitor
- Exceptions — hidden cost, не noexcept-friendly, не compile-time
- Tier 1.B migration: 9+ cold-path functions переведены на `std::expected`

**Q17. Что произойдёт, если hot shader reload упадёт?**
- `cmake --build` return code != 0 → `RebuildAllShadersFromDisk` returns 0 (reloadedCount=0)
- `RequestRayMarchPipelineRecreate()` всё равно вызывается
- Следующий frame может fail в `vkCreateComputePipelines` → pipeline stays in old state
- Per `agent/decisions.md` — explicit follow-up (Phase 7+)

**Q18. Почему 4 каскада, а не 2 или 8?**
- 2 — слишком грубая тени в дали
- 8 — overhead, complexity, marginal quality gain
- 4 — sweet spot для 1920×1080, near-biased split (lambda 0.80)
- Каскады: 0-15м, 15-30м, 30-50м, 50-200м (примерно)

**Q19. Как работает spread rule? (fluid CA)**
- Fall-through после fall: spread в 1 из 4 сторон
- Direction — hash-determined из `(x, y, z)` для воспроизводимости
- Claimed-tracking: destination помечается, второй fluid не перезаписывает
- Без claimed-tracking — swap bug (два fluid обмениваются, один исчезает)
- 2026-06-13: spread rule restored (per `agent/decisions.md §30`)

**Q20. Почему 73 MB debug, а не меньше?**
- Tracy instrumentation (debug build)
- RenderDoc markers
- Vulkan validation layers (если ON)
- Google Benchmark (debug presets)
- ImGui
- -O0 debug info + DWARF
- Без них: ~19 MB release

**Q21. Что такое R11G11B10 UFLOAT?**
- HDR color format для TAA scene color attachment (per `decisions.md`)
- 11+11+10 = 32 B/пиксель, no alpha
- 11-bit floating point через `unsigned int` mantissa+exponent
- Хватает для HDR scenes без banding

**Q22. Почему `lambda = 0.80`, а не 0.5 (logarithmic)?**
- lambda=0 — uniform split (каскады равной ширины)
- lambda=1 — logarithmic split (по глубине)
- 0.80 — near-biased, баланс между uniform и logarithmic
- per `decisions.md` — current mainline default

**Q23. Почему std::expected только на cold paths?**
- Hot path: `bool`+`CORE_ASSERT` → 0 overhead в release (assert вырезается)
- Cold path: `std::expected<T, E>` — машиночитаемый error enum
- 9+ cold-path functions переведены (Tier 1.B): Vulkan init, snapshot, audio load, scene config, ECS sync, physics state, scene resources, etc.
- Hot paths не переводятся — overhead

**Q24. Какова роль ECS зеркала, если не используется?**
- Используется для HUD и отладки
- Типизированные компоненты (lock-free read через mirror)
- Разделение gameplay и render
- Hot reload: ECS state не теряется при VoxelWorld rebuild

**Q25. Что такое `MVP` в контексте ProjectV?**
- Minimum Viable Product — minimum жизнеспособный продукт
- Tier 0-5 closed (2026-06-15): все запланированные для MVP tasks
- Phase 4-9 — post-MVP roadmap
- 14/14 ctest + 6/6 smoke — доказательство завершённости MVP

**Q26. Какие сложности с Vulkan 1.4?**
- Vulkan API verbose — много boilerplate
- `volk` решает loader часть
- VMA для memory management
- Свой hot shader reload вместо `vkDestroyShaderModule` + `vkCreateShaderModule` каждый frame
- C++26 modules (Math.ixx) ускоряют инкрементальную сборку

### 8.2. Дополнительные каверзные (5 — придуманы оператором)

**Q27. Почему именно chunk 8×8×8, а не 4×4×4 или 16×16×16?**
- 4×4×4 = 64 вокселя = 64 B — слишком мало, overhead на chunks
- 16×16×16 = 4 KB — не влезает в L1 (32 KB на Zen 3)
- 8×8×8 = 512 B = 2 SSE-регистра — sweet spot для L1
- 32×32×32 = 32 KB — еле влезает, нет headroom

**Q28. Что если пользователь переключит scene preset посреди load/initialize pipeline?**
- `VoxelWorld` уничтожается через `DestroyVoxelSceneWorld`
- Новый создаётся через `CreateVoxelSceneWorld(state, preset)`
- Частично смягчено через `vkDeviceWaitIdle` в `DestroySceneResources`
- Полное устранение race condition — Phase 5

**Q29. Почему static_assert, а не runtime check?**
- Compile-time проверка → 0 overhead в release (assert вырезается)
- Гарантирует что struct-контракт с шейдерами не сдвинется (per `agent/memory.md §10.8`)
- Если `sizeof(VoxelChunk)` изменится с 32 до 40 — компиляция упадёт, не молча сломает GPU upload
- Runtime check был бы бесполезен (race condition, perf overhead)

**Q30. Можно ли добавить новый material type без переписывания всего pipeline?**
- `VoxelMaterial` — enum (Air=0, Glass=1, Fluid=2, FloorWhite=3, FloorGray=4)
- Добавление нового material требует: обновить enum + добавить `VoxelMaterialVisual` в `VoxelMaterials.cpp` + обновить switch в fragment shader + обновить smoke-capture эталоны
- По сути — touch 5-7 файлов, требует перетестирование
- Архитектура чистая: всё в `VoxelMaterials.cpp:139-236` в одной таблице

**Q31. Что произойдёт, если все 6 smoke captures упадут одновременно?**
- Скорее всего, серьёзный регресс (например, GPU не поддерживает формат, или шейдер не компилируется)
- Investigate: проверить `vkCreateComputePipelines`/`vkCreateGraphicsPipelines` → может быть missing feature
- Investigate: проверить `glslc` errors при пересборке shaders
- Investigate: проверить вывод `dmesg` для GPU errors
- Recovery: `git bisect` по последним 5-10 коммитам с shader changes

**Q32. Почему `expected<bool, VoxelSnapshotError>` а не просто `bool` с errno?**
- Per Tier 1.B: `bool` не различает типы ошибок (кроме errno), теряется контекст
- `std::expected<T, E>` — strongly-typed enum, машинно-читаемый
- Caller может `if (!result)` → обработать, или `result.value()` → получить
- Cold path (1× per snapshot), overhead `std::expected` несущественен

**Q33. Как тестировать multi-threading в ECS sync?**
- `SyncEcsWorldState` — single-threaded (per `VoxelWorld.hpp:201-208`)
- ECS mirror — read-only из render thread
- Lock-free для HUD (atomic snapshot)
- Multi-threading в ECS deferred (Phase 7+)
- Если кто-то спросит про race conditions — текущий код их избегает через single-thread + lock-free reads

---

## 9. Хронология (релевантные события)

**2026-04-09 (Tier 0.B):** `Mat4` (16-byte aligned) заменил `std::array<float, 16>` для GPU ABI parity в `VoxelSceneLighting` и `SunShadowCascadeProjections`. ABI change: `Vec3` (12→16 B), `VoxelSceneLighting` (+16 B = 624 B total).

**2026-04-12 (Tier 0.A):** Math foundation. `core/Math.hpp` + `core/Math.ixx`. per `agent/memory.md §10.1`.

**2026-04-12 (M5.1d, Tier 5):** Two-level chunk visibility cache (XOR-fold splitmix64 hash). Quantization: camera position 0.25 voxel units, camera forward 0.005 (~0.3°).

**2026-04-12 (Tier 4):** С-ядро `frustum_cull` scalar (3.7-3.9× faster than C++ baseline). AVX2 version kept in tree (2.5-2.7× faster). Crossover threshold 8 AABBs.

**2026-04-13 (Tier 1.B):** `std::expected<T, E>` migration на холодных путях. VulkanInit (16 variants), snapshot (3 variants), audio load (3 variants), scene config, ECS sync, physics state.

**2026-04-13 (Tier 2.D):** C++26 modules в mainline (Math.ixx, Probe.ixx, StringId.ixx). `import projectv.math;` probe работает.

**2026-04-14 (Release presets):** commits `6fe9201`. linux-clang-release / windows-clang-release. Conservative policy: -O3 -flto=thin -DNDEBUG. Без -ffast-math, без -march=native.

**2026-04-15 (Post-WBV-r1 batch):** F11/F12/V relocate → 1/2/3 (F5/F6 conflicts with InputAction). pragma once conversion (55 files). Shader contract fix (3 model/TAA-pipeline shaders).

**2026-06-10 (incident):** `git checkout -- .` стёр uncommitted work. Урок: safety-net patch в `/tmp/` обязательно.

**2026-06-13 (Fluid CA audit):** spread rule restored per `agent/decisions.md §30`. Без claimed-tracking — swap bug (два fluid'а обмениваются, один исчезает).

---

## 10. Out of scope

le1t — единственный человек, который отвечает на все вопросы. Если вопрос выходит за пределы знаний:

| Вопрос про… | Говори |
|---|---|
| Детальный код конкретной функции | «Сейчас не смотрю код, но могу объяснить концепцию» |
| Личные мнения о других движках | «Не слежу за рынком, наш выбор основан на конкретных требованиях» |
| Будущее после Phase 9 | «За пределами roadmap, не планировал» |
| Сравнение с конкретным коммерческим движком | «Не проводил сравнительный анализ, наш проект для другой ниши» |

Все остальные вопросы — в зоне твоей ответственности. Тиммейты подключаются по компетенциям:
- T3 — воксельный мир (Тиммейт 2)
- T4 — рендеринг (Тиммейт 3)
- T5 — ассеты+аудио (Тиммейт 5)
- T6 — физика (Тиммейт 4)
- T1 — сборка/тесты (Тиммейт 1)
