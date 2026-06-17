# Defense Competency FAQ — T2 (Live Demo + Стек)

**Slot:** T2 Live Demo + Стек (0:45–2:00)
**Кто говорит:** le1t (Кадочников Лев Петрович) — ведущий, тимлид, Q&A host
**Реальная компетенция:** Архитектура + Workflow + Q&A host (отвечает на ВСЕ сложные технические вопросы)
**Out of scope:** нет. le1t — единственный человек, который отвечает на все вопросы. Тиммейты подключаются по своим компетенциям.

---

## 1. Verbatim твоей речи (T2)

> «Здравствуйте. Перед вами запущенная тестовая лаборатория нашего движка. Генерация сцены менее 10 миллисекунд. Вы видите стекло, жидкость, тени и жадный мешинг геометрии. Сверху слева и сверху справа HUD — мы держим 500+ FPS на первой сцене.
>
> **[Я возвращаю презентацию и переключаю на 3 слайд]**
>
> Технически проект написан на современном C++ 26. Мы используем графический API Vulkan 1.4 для низкоуровневого управления видеокартой и работой с шейдерами. Архитектура построена на дата-ориентированном дизайне (DOD) — мы выравниваем данные в памяти для максимальной скорости кэша процессора и минимальных cache miss-ов. Также мы используем SIMD-инструкции и C-вставки на C 23. Дальше следующий участник расскажет про то, что внутри движка.»

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

**ECS: Flecs:**
- `EcsWorld::InitializeAppEcs(state)` — init
- `SyncEcsWorldState(ecs)` — 1× за кадр
- Flecs — MIT, header-only C++
- Single Source of Truth: `VoxelWorld` владеет, ECS — пассивное зеркало

**Физика: Jolt:**
- MIT, детерминированный, SIMD
- `JPH::CharacterVirtual` для коллизий
- Наш собственный код дополняет для walk controller

### 3.2. Алгоритмы (все 23, per `docs/DefenseAlgorithms.md`)

| # | Алгоритм | Где реализован |
|---|---|---|
| 1 | Жадный мешинг (greedy meshing) | `voxel_mesh.comp:613-619` |
| 2 | Каскадные тени (CSM) | `ShadowProjection.hpp:42-51` |
| 3 | Контактные тени (CTSH) | `voxel.frag` (sun-to-fragment ray) |
| 4 | Фоновое затенение (AOCC) | `voxel.frag:ComputeAmbientOcclusionVisibility` |
| 5 | Локальный точечный свет (LOCL) | `voxel.frag` (per-fragment lighting) |
| 6 | TAA (Temporal Anti-Aliasing) | `Taa.hpp` + `taa_resolve.frag` |
| 7 | Ray-marching (DDA) | `ray_march.comp` (STUB) |
| 8 | Fluid CA (клеточный автомат) | `VoxelWorld.cpp:1284-1643` |
| 9 | Voxel raycast (DDA) | `VoxelRaycast.cpp` |
| 10 | Frustum culling (С-ядро) | `c_kernels/frustum_cull.{hpp,c}` |
| 11 | Chunk visibility cache | `SceneResources.hpp:359-407` |
| 12 | Walk controller (Jolt + voxel solver) | `PhysicsWorld.cpp` |
| 13 | Auto-jump (1-block detection) | `PhysicsWorld.cpp` |
| 14 | Edge grace (тонкие края) | `PhysicsWorld.cpp` |
| 15 | Sneak (Shift) | `PhysicsWorld.cpp` |
| 16 | glTF parser | `AssetLoader.cpp:408-431` |
| 17 | Draco decode | `DracoMeshDecoder.cpp` |
| 18 | meshopt (vertex cache/fetch) | `MeshBaker.cpp:56-87` |
| 19 | Audio engine (miniaudio) | `AudioEngine.cpp` |
| 20 | Snapshot save/load (PVSNAP01) | `VoxelWorld.cpp:1284-1643` |
| 21 | Hot shader reload | `main.cpp:68-114` |
| 22 | Hot/cold error split (std::expected) | `Tier 1.B` |
| 23 | DOD layout (alignas, SoA) | `VoxelChunk`, `VoxelWorld` |

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
- per `docs/DefenseReport.md §3`

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
