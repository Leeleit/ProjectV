# Defense Competency FAQ — le1t (Архитектура + Q&A host)

**Участник:** Кадочников Лев Петрович (le1t)
**Реальная компетенция:** Архитектура + Workflow + Q&A host (отвечает на ВСЕ сложные технические вопросы)
**Speech slot на сцене:** T2 Live Demo + Стек (0:45-2:00)
**Verbatim текст выступления:** `docs/DefenseScript_Team.md` → раздел «Я (Live Demo и Стек)»

**Out of scope:** нет. le1t — единственный человек, который отвечает на все вопросы. Тиммейты подключаются по своим компетенциям. Если вопрос выходит за пределы знаний — «не знаю, уточню у команды».

**Common (стек, метрики, хоткеи, glossary, chronology):** `docs/DefenseCompetencyFAQ.md`

---

## 6.1. Кто ты

**Реальность:** ты — Кадочников Лев Петрович, единственный разработчик ProjectV. Команда «Черепашки Ninja» — для защиты.

**На сцене:** ты ведущий, говоришь T2 (Live Demo + Стек) 1:15.

**На Q&A:** ты отвечаешь на **ВСЕ сложные технические вопросы**. Тиммейты подключаются по своим компетенциям. Если вопрос выходит за пределы твоих знаний (что вряд ли) — «не знаю, уточню у команды».

## 6.2. Твоя компетенция: Архитектура + Workflow

### 6.2.1. Стек (C++26, Vulkan 1.4, DOD, ECS)

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

### 6.2.2. Алгоритмы (все 23, per `docs/DefenseAlgorithms.md`)

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

### 6.2.3. Тесты и метрики

- **14 ctest** baseline 14/14 (0.78s debug, 0.06s release)
- **6/6 runtime smoke** captures (FINAL/SHDW/CSM/CTSH/AOCC/LOCL)
- **60+ sidecar keys** в `.txt` файле
- **73 MB debug / 19 MB release** ELF (-73%)
- **2 MP3** в `music/`
- **0 предупреждений** в нашем коде (per `agent/decisions.md §4`)

### 6.2.4. Известные баги (на момент защиты)

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

### 6.2.5. Workflow (multi-agent)

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

### 6.2.6. Roadmap (Phase 4-9)

| Phase | Цель |
|---|---|
| 4 | Networking (server-authoritative + client prediction) |
| 5 | SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader) |
| 6 | HDR-текстуры + полный клеточный автомат жидкости на GPU |
| 7 | Полная система частиц + асинхронная загрузка ресурсов |
| 8 | Плагины / моддинг API |
| 9 | Многопользовательский режим (Academic vision) |

## 6.3. Твой слот: T2 Live Demo + Стек (1:15)

**Действия:**
1. Запустить приложение (сцена `VoxelLab`).
2. Включить подробный HUD клавишей `G`.
3. Показать облёт камеры (WASD + мышь).
4. Поставить/сломать пару блоков (правый/левый клик).
5. Переключить debug view (`B` — cycle FINAL/SHDW/CSM/CTSH/AOCC/LOCL).
6. Захватить screenshot (`C`).

**Речь (verbatim):** см. `docs/DefenseScript_Team.md` → раздел «Я (Live Demo и Стек)».

## 6.4. Q&A-карта (40 вопросов)

### 6.4.1. Архитектура и стек (8 вопросов)

**Q1. Почему C++26, а не Rust/Zig/Go?**
- Все зависимости (Jolt, fastgltf, VMA, Draco, Flecs) — C/C++ с нативным API
- C++26 даёт `std::expected` для холодных путей, `std::simd` для горячих, модули для ускорения инкрементальной сборки
- Rust — рассматривался, но Vulkan bindings + ECS + asset pipeline зрелые на C++

**Q2. Почему Vulkan 1.4, а не OpenGL/DX12/Metal?**
- Vulkan — явный контроль GPU (пайплайны, память, синхронизация)
- OpenGL — driver управляет, дорого для миллионов draw items
- Compute shaders нужны для мешинга
- Кросс-платформенный (Windows + Linux)

**Q3. Почему Jolt, а не PhysX/Bullet?**
- Jolt — MIT, современный, детерминированный, SIMD-оптимизирован
- Bullet устарел, PhysX избыточен + проприетарный

**Q4. Почему Flecs, а не EnTT/Bevy ECS/DOTS?**
- Flecs — header-only C++ ECS, MIT, отличная эргономика для встроенного использования в Vulkan-приложении
- EnTT — header-only, но runtime overhead выше
- Bevy ECS — только для Rust

**Q5. Что такое DOD и зачем?**
- Дизайн, ориентированный на данные (Data-Oriented Design)
- Данные организованы для эффективной обработки CPU, а не для удобства иерархии классов
- Чанк 8×8×8 = 512 вокселей = 512 B = 2 SSE-регистра, влезает в L1
- `alignas(16)` → авто-векторизация в `movaps`/`vmovaps`

**Q6. Как связаны ECS и VoxelWorld?**
- Single Source of Truth: `VoxelWorld` — единственный владелец, все мутации только через него
- ECS (Flecs) — пассивное зеркало, обновляется 1 раз за кадр через `SyncEcsWorldState`
- HUD читает из ECS (только чтение), не из VoxelWorld (изменяемый)

**Q7. Как боретесь с накладными расходами на обработку ошибок?**
- Гибридный подход: на холодных путях — `std::expected<T, E>`, на горячих — `bool` + `CORE_ASSERT` (вырезаются в release)
- Cold paths: Vulkan init, snapshot, audio load, scene config
- Hot paths: voxel meshing dispatch, frame prep

**Q8. Зачем нужен ECS-bridge, если есть VoxelWorld?**
- Flecs даёт типизированные компоненты, lock-free чтение через зеркало, разделение gameplay и render

### 6.4.2. Алгоритмы и рендеринг (10 вопросов)

**Q9. Что такое жадный мешинг и зачем?**
- Объединяет соседние грани вокселей одного exposed state в один четырёхугольник (quad)
- 6 проходов на чанк: ±X, ±Y, ±Z
- Compute-шейдер `voxel_mesh.comp:613-619`
- Сокращение draw calls на 30-50%

**Q10. Как работают каскадные тени (CSM)?**
- 4 каскада карты глубины 2048×2048
- Лямбда 0.80 (near-biased)
- Per-cascade проекция солнца: sub-frustum → light-space → sphere stabilization
- Стекло не отбрасывает тень, жидкость — отбрасывает (per `decisions.md`)

**Q11. Что такое TAA и зачем?**
- Временное сглаживание: смешивает кадры, убирает дрожание камеры
- 8-sample Halton(2,3) jitter, YCoCg-зажим
- Поверх TAA — CAS (фильтр резкости)
- **По умолчанию TAA jitter = 0 (стабильная картинка, нет дрожания)**

**Q12. Что такое ray-marching и как реализован?**
- Трассировка лучей через воксели (Amanatides-Woo DDA)
- Compute-шейдер `ray_march.comp` скомпилирован
- API state (`SetRayMarchEnabled`/`IsRayMarchEnabled`/`RequestRayMarchPipelineRecreate`) работает
- Graphics command stream его пока не вызывает — **STUB, Phase 7 follow-up**
- Per `RayMarchPass.hpp:9-30`

**Q13. Что такое контактная тень (CTSH)?**
- Короткая трассировка луча от фрагмента к солнцу
- Дополняет CSM где разрешения карт глубины не хватает
- Per-layer history не blended (deferred — separation refactor)

**Q14. Что такое AOCC?**
- Ambient Occlusion Cavity Check — локальное затенение полостей
- 12 traces per fragment (per `decisions.md`)
- Не полноценный SSAO — компактный, встроенный в lighting term
- Per-layer history blended (mix with 0.4)

**Q15. Зачем нужен локальный точечный свет?**
- Сцена Voxel Laboratory имеет один на пресет обратно-квадратичный точечный свет
- В дополнение к направленному солнцу
- Объёмный эффект, подсвечивает тёмные стороны

**Q16. Как работает walk controller?**
- `JPH::CharacterVirtual` для обнаружения столкновений
- Наш собственный код дополняет Jolt для опоры игрока на блоки
- Edge grace / sneak / auto-jump — наш код, не Jolt

**Q17. Как работает авто-прыжок?**
- При включении (`J`), контроллер каждый кадр проверяет: есть ли впереди блок высотой 1
- `autoJumpDelayFramesRemaining` — задержка после прыжка
- `autoJumpDelayEnabled` — toggle `F12`

**Q18. Что такое клеточный автомат (Fluid CA)?**
- Для жидкости: один тик = попытка падения вниз, иначе распространение в 1 из 4 сторон
- Bottom-up y-pass → 1 cell per tick
- Double-buffered snapshot, claimed-tracking
- Deterministic, no FP, no atomics

### 6.4.3. Тесты и метрики (6 вопросов)

**Q19. Какие тесты, сколько?**
- 14 наборов в `tests/CMakeLists.txt`
- Baseline 14/14, 0.78 сек debug, 0.06 сек release
- Runtime smoke 6/6 captures
- 60+ sidecar keys

**Q20. Какие метрики производительности?**
- VoxelLab reference shot: 500+ FPS, ~2 мс кадр
- Release: 19 МБ (vs 73 МБ debug, -73%)
- 14/14 ctest, 6/6 smoke

**Q21. Какие известные баги?**
- BUG-005 (cycle scene race): гонка дескрипторов при переключении сцен, частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources`
- **BUG-004 (VoxelLab tremor) — отвергнут, не существует**
- Ray-march STUB (Phase 7)

**Q22. Что отложено и почему?**
- 6 пунктов: частицы, моддинг, асинхронная загрузка, HDR, SVO, mesh shaders
- Все явно в Phase 4-9 roadmap
- per `docs/DefenseReport.md §3`

**Q23. Какие платформы поддерживаются?**
- Windows 10/11 (clang-cl 22) + Linux Arch (clang 22 native + libc++ 16)
- Обе сборки зелёные, 14/14 тестов
- macOS — НЕ в планах (per `decisions.md`)

**Q24. Какие решения принимали лично вы?**
- Стек: C++26, Vulkan 1.4, Flecs, Jolt
- DOD layout: `alignas(16)`, чанк 8×8×8
- Hot/cold split: `std::expected` на холодных, `bool`+assert на горячих
- Walk: наш код дополняет Jolt
- Glass: не отбрасывает тень, жидкость отбрасывает
- TAA: B10G11R11 UFLOAT цвет
- 4-каскадный CSM
- Release-пресеты: без `-ffast-math`, без `-march=native`

### 6.4.4. Команда и workflow (6 вопросов)

**Q25. Какие были трудности?**
- Согласование областей ответственности (scope) между модулями
- Multi-agent сессии: протокол в `AGENTS.md §7.2.6`
- Инцидент 2026-06-10: `git checkout -- .` стёр uncommitted work
- Урок: safety-net patch в `/tmp/`

**Q26. Hot shader reload — как работает?**
- Клавиша `1` (relocated 2026-06-15)
- `RebuildAllShadersFromDisk()` → `cmake --build $BUILD_DIR --target Shaders`
- `RequestRayMarchPipelineRecreate()` — re-bind ray-march compute
- Другие pipelines (graphics, shadow, TAA) переиспользуют кэшированные модули до Phase 7+

**Q27. Сколько коммитов и как организован workflow?**
- 100+ коммитов за 3,5 месяца
- Conventional commits с type/scope (per `AGENTS.md §7.2.5`)
- Multi-agent координация через `agent/active-sessions.md`
- Auto-close после коммита per `AGENTS.md §8.1`

**Q28. Что бы вы улучшили в следующей итерации?**
- Phase 4 (Networking): server-authoritative + client prediction
- Phase 5 (SVO): hybrid SVO + chunks, SVO ray-marching для теней
- Phase 6 (Fluid): полный клеточный автомат на GPU с диффузией и вязкостью
- Phase 7 (Particles + Modding): система частиц, modding API, полная ray-march интеграция
- Phase 8 (SCP mechanics): неевклидова геометрия, порталы
- Phase 9 (Strategic): тысячи юнитов, командный zoom

### 6.4.5. Самые каверзные вопросы (12 вопросов)

**Q29. Почему вы не сделали ECS зеркало по-другому? (например, без копирования)**
- Alternative: ECS reads directly from VoxelWorld (no mirror)
- Per `decisions.md` — chosen approach: passive mirror for HUD decoupling
- Trade-off: extra copy (small) vs lock contention (bigger)
- HUD reads once per frame, lock-free через mirror

**Q30. Почему std::expected, а не std::variant или exceptions?**
- `std::expected<T, E>` — strongly-typed error, like Result в Rust
- `std::variant` — нет «error vs value» semantics, нужен visitor
- Exceptions — hidden cost, не noexcept-friendly, не compile-time
- Tier 1.B migration: 9+ cold-path functions переведены на `std::expected`

**Q31. Что произойдёт, если hot shader reload упадёт?**
- `cmake --build` return code != 0 → `RebuildAllShadersFromDisk` returns 0 (reloadedCount=0)
- `RequestRayMarchPipelineRecreate()` всё равно вызывается
- Следующий frame может fail в `vkCreateComputePipelines` → pipeline stays in old state
- Per `agent/decisions.md` — explicit follow-up (Phase 7+)

**Q32. Почему 4 каскада, а не 2 или 8?**
- 2 — слишком грубая тени в дали
- 8 — overhead, complexity, marginal quality gain
- 4 — sweet spot для 1920×1080, near-biased split (lambda 0.80)
- Каскады: 0-15м, 15-30м, 30-50м, 50-200м (примерно)

**Q33. Как работает spread rule? (fluid CA)**
- Fall-through после fall: spread в 1 из 4 сторон
- Direction — hash-determined из `(x, y, z)` для воспроизводимости
- Claimed-tracking: destination помечается, второй fluid не перезаписывает
- Без claimed-tracking — swap bug (два fluid обмениваются, один исчезает)
- 2026-06-13: spread rule restored (per `agent/decisions.md §30`)

**Q34. Почему 73 MB debug, а не меньше?**
- Tracy instrumentation (debug build)
- RenderDoc markers
- Vulkan validation layers (если ON)
- Google Benchmark (debug presets)
- ImGui
- -O0 debug info + DWARF
- Без них: ~19 MB release

**Q35. Что такое R11G11B10 UFLOAT?**
- HDR color format для TAA scene color attachment (per `decisions.md`)
- 11+11+10 = 32 B/пиксель, no alpha
- 11-bit floating point через `unsigned int` mantissa+exponent
- Хватает для HDR scenes без banding

**Q36. Почему `lambda = 0.80`, а не 0.5 (logarithmic)?**
- lambda=0 — uniform split (каскады равной ширины)
- lambda=1 — logarithmic split (по глубине)
- 0.80 — near-biased, баланс между uniform и logarithmic
- per `decisions.md` — current mainline default

**Q37. Почему std::expected только на cold paths?**
- Hot path: `bool`+`CORE_ASSERT` → 0 overhead в release (assert вырезается)
- Cold path: `std::expected<T, E>` — машиночитаемый error enum
- 9+ cold-path functions переведены (Tier 1.B): Vulkan init, snapshot, audio load, scene config, ECS sync, physics state, scene resources, etc.
- Hot paths не переводятся — overhead

**Q38. Какова роль ECS зеркала, если не используется?**
- Используется для HUD и отладки
- Типизированные компоненты (lock-free read через mirror)
- Разделение gameplay и render
- Hot reload: ECS state не теряется при VoxelWorld rebuild

**Q39. Что такое `MVP` в контексте ProjectV?**
- Minimum Viable Product — minimum жизнеспособный продукт
- Tier 0-5 closed (2026-06-15): все запланированные для MVP tasks
- Phase 4-9 — post-MVP roadmap
- 14/14 ctest + 6/6 smoke — доказательство завершённости MVP

**Q40. Какие сложности с Vulkan 1.4?**
- Vulkan API verbose — много boilerplate
- `volk` решает loader часть
- VMA для memory management
- Свой hot shader reload вместо `vkDestroyShaderModule` + `vkCreateShaderModule` каждый frame
- C++26 modules (Math.ixx) ускоряют инкрементальную сборку

## 6.5. Out of scope

| Вопрос про… | Говори |
|---|---|
| Детальный код конкретной функции | «Сейчас не смотрю код, но могу объяснить концепцию» |
| Личные мнения о других движках | «Не слежу за рынком, наш выбор основан на конкретных требованиях» |
| Будущее после Phase 9 | «За пределами roadmap, не планировал» |
| Сравнение с конкретным коммерческим движком | «Не проводил сравнительный анализ, наш проект для другой ниши» |

---

**Конец FAQ le1t.** Per-team FAQ файлы: `DefenseCompetencyFAQ_T{1..5}.md`. Common + INDEX: `DefenseCompetencyFAQ.md`.
