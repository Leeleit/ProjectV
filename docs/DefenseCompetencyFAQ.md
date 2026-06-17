# Defense Competency FAQ — ProjectV (Common + INDEX)

**Версия:** 2026-06-17.
**Для:** всех участников команды «Черепашки Ninja» и le1t.
**Назначение:** общая карта проекта + глоссарий + хронология. Per-team competency FAQ — в отдельных файлах.
**Формат:** справочный. Читать с экрана (компьютер / телефон) во время подготовки к Q&A. **НЕ читать на сцене** — для выступления есть `docs/DefenseScript_Team.md`.
**Соглашение:** все цитаты исходного кода приведены с `file:line` — перепроверяйте, исходный код изменяется.

---

## INDEX — per-team competency FAQ

| Файл | Участник | Реальная компетенция | Speech slot |
|---|---|---|---|
| `docs/DefenseCompetencyFAQ_T1.md` | Тиммейт 1 | Сборка и тестирование (CMake, ctest, smoke) | T1 Вступление и проблема |
| `docs/DefenseCompetencyFAQ_T2.md` | Тиммейт 2 | Воксельный мир (чанки, meshing, fluid CA, snapshot) | T3 Архитектура и качество кода |
| `docs/DefenseCompetencyFAQ_T3.md` | Тиммейт 3 | Рендеринг (Vulkan, TAA, CSM, AOCC, шейдеры, C-ядро) | T4 Тесты и проверки |
| `docs/DefenseCompetencyFAQ_T4.md` | Тиммейт 4 | Физика и walk-контроллер (Jolt, edge grace, sneak) | T6 Планы и Закрытие |
| `docs/DefenseCompetencyFAQ_T5.md` | Тиммейт 5 | Ассеты и аудио (glTF, Draco, meshopt, miniaudio) | T5 Прочие фичи + что отложено |
| `docs/DefenseCompetencyFAQ_le1t.md` | le1t (Кадочников Л.П.) | Архитектура + Workflow + Q&A host | T2 Live Demo + Стек |

**Speech slot ≠ real competency.** Per operator: «нам надо красиво подать проект, а когда будут задавать вопросы, тут компетенция каждого уже понадобится». На сцене все говорят «мы», роли не акцентируются. На Q&A каждый отвечает по своей реальной компетенции.

**Verbatim текст выступлений** — в `docs/DefenseScript_Team.md`. Per-team FAQ файлы ссылаются на разделы скрипта для каждого тиммейта.

---

## §0. Общая карта проекта (читать всем)

### 0.1. Что такое ProjectV

ProjectV — высокопроизводительный воксельный игровой движок. Один разработчик (le1t = Кадочников Лев Петрович). Команда «Черепашки Ninja» (6 человек) — для защиты. Код — реальный, ~3,5 месяца работы, 100+ коммитов.

**Стек:**
- C++26 (`CMAKE_CXX_STANDARD 26` в `CMakeLists.txt:29`)
- Vulkan 1.4 (`VOLK_STATIC_DEFINES` + `VK_API_VERSION_1_4` в `src/render/vulkan/`)
- Clang 22.1.6 (Linux native) + clang-cl (Windows)
- CMake 4.0 (12 configure-пресетов: 8 debug + 4 release)
- 22 git-сабмодуля в `external/`

**Библиотеки (vendored, не системные):**
- Jolt Physics (MIT, deterministic, SIMD) — физика твёрдых тел
- Flecs (MIT, header-only C++) — ECS как пассивное зеркало
- fastgltf + Draco + meshoptimizer — asset pipeline
- miniaudio (single-header C) — audio (MP3)
- volk — Vulkan loader
- VulkanMemoryAllocator (VMA) — GPU memory
- tracy — performance profiler
- fmt, glm, nlohmann/json, spdlog, stb_image, и др.

### 0.2. Целевая машина и метрики

**Target (per `decisions.md`, `agent/memory.md`):**
- CPU: Ryzen 7 5800X (Zen 3, 8 cores, 16 threads, L1 = 32 KB/core)
- GPU: NVIDIA RTX 3060 Ti
- RAM: 16 GB
- Разрешение: 1920×1080

**Производительность (release build):**
- VoxelLab debug: **500+ FPS**, ~2 мс кадр (per `DefenseScript_Team.md` T2)
- VoxelLab release: 19 MB ELF, дополнительно ×1.5-2.5 ускорение (per `status.md §21`)
- ctest: **14/14 pass** в 0.78 сек (debug) / 0.06 сек (release)
- Runtime smoke: 6/6 captures пиксель-в-пиксель

**Бинарные размеры (Linux):**
- Debug: 73 MB (с Tracy, RenderDoc markers, validation)
- Release: 19 MB (`-O3 -flto=thin -ffunction-sections -fdata-sections -Wl,--gc-sections`)
- Разница: -73% (per `decisions.md §4`)

### 0.3. Архитектура в одном абзаце

`VoxelWorld` — единственный источник истины (Single Source of Truth, SoT). Все мутации мира — только через него. `VoxelWorld` хранит `std::vector<uint8_t> voxels` (плоский массив) + `std::vector<VoxelChunk>` (32-байтные чанки) + статистику. Compute-шейдер `voxel_mesh.comp` greedy-мешит чанки в quad'ы. Vulkan 1.4 graphics pipeline рендерит меши. Jolt симулирует персонажа. Flecs дублирует мир в типизированные компоненты для HUD/отладки. ECS sync 1× за кадр через `SyncEcsWorldState`.

### 0.4. Документы — где что

| Документ | Содержит |
|---|---|
| `docs/DefenseScript_Team.md` | Скрипт выступления (verbatim, 5 мин) |
| `docs/DefenseCompetencyFAQ.md` | **Этот файл** — Common + INDEX |
| `docs/DefenseCompetencyFAQ_T{1..5}.md`, `_le1t.md` | Per-team competency FAQ (6 файлов) |
| `docs/DefensePresentation_Structure.md` | 8 слайдов с таймингами |
| `docs/DefenseAlgorithms.md` | 23 алгоритма (эталон) |
| `docs/DefenseFAQ.md` | 15+ готовых ответов (эталон) |
| `docs/DefenseReport.md` | Итоговый отчёт + §3 deferred items |
| `docs/archive/DefenseBriefer_TechnicalDeepDive_2026-06-15.md` | Q&A reference (технические детали) |
| `docs/archive/DefenseOldFormat_2026-06-17/` | Устаревшие 10-мин скрипты |

### 0.5. Хоткеи (полный список, из `src/app/InputActions.cpp:127-181`)

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
- `F11` (InputAction) vs `1` (defense) — это разные клавиши! `F11` = walk air control, `1` = hot shader reload (relocation после conflict с F11 InputAction, см. `src/app/main.cpp:545-619`)

**Voxel interaction:**
- Левый клик — removal (VoxelMaterial::Air)
- Правый клик — placement
- `F` — pick model (HL2-style physicsgun)
- `F2` — cycle placement material
- `F8` — cycle editor tool
- `M` — pick target material
- `X` — toggle mutation anchor

**Сцена и snapshot:**
- `F5` — cycle scene preset (VoxelLab, FlatBenchmark, TransparencyStress, ChunkGrid, MeshingStress)
- `F6` — save world snapshot (PVSNAP01)
- `F7` — load world snapshot

**Визуализация:**
- `F1` — toggle HUD
- `G` — toggle detailed HUD
- `B` — cycle lighting debug view (Final → Ambient → Direct → Local → Shadow → Cascade → Contact → Occlusion → Fog → Taa)
- `C` — capture screenshot (.bmp + .txt sidecar)
- `L` — toggle cascade split planes
- `Z` — toggle cursor hit normal
- `O` — cycle shadow tuning target
- `U` / `I` — decrease / increase shadow tuning value
- `V` — reset lighting debug controls
- `H` / `K` — decrease / increase lighting exposure
- `N` — cycle tone map operator (Linear / Reinhard / AcesApprox)

**TAA (Temporal Anti-Aliasing):**
- `T` — toggle TAA on/off (relocation с original binding)
- `;` / `'` — decrease / increase TAA jitter scale
- `-` / `=` — decrease / increase TAA blend
- `,` — cycle TAA neighbourhood radius (1/3/5/7)
- `.` — invalidate TAA history

**Frame-step / slow-motion:**
- `P` — toggle pause
- `[` / `]` — decrease / increase time scale
- `\` — step single frame
- `` ` `` — reset time scale

**Audio (per `src/app/InputActions.cpp:196-210`):**
- `Q` — play/pause toggle
- `E` — stop
- `7` / `8` — volume down / up
- `9` / `0` — next / previous track

**Chunk debug:**
- `F9` — toggle chunk bounds
- `F10` — toggle dirty chunk overlay

**Input replay (debug):**
- `R` — toggle input replay recording
- `Y` — play last input replay

**Defense r0 hotkeys (relocated 2026-06-15, per `src/app/main.cpp:545-619`):**
- `1` — hot shader reload (было F5/F11, освобождено для InputAction)
- `2` — toggle ray-march pass (было F6/F12)
- `3` — cycle V-sync mode (было V)
- `ESC` — exit

**Горячие клавиши, освобождённые relocation:** `F5`, `F6`, `F11`, `F12`, `V` теперь работают как InputAction (`CycleScenePreset`, `SaveWorldSnapshot`, `ToggleWalkAirControlMode`, `ToggleWalkAutoJumpDelay`, `ResetLightingDebugControls`).

### 0.6. Известные проблемы (на момент защиты)

**BUG-005 (cycle scene race)** — гонка дескрипторов при переключении сцен. Частично смягчена через `vkDeviceWaitIdle` в `DestroySceneResources` (per `agent/decisions.md` + `agent/memory.md`). **Полное устранение — отдельная подзадача (Phase 5)**.

**Ray-march pass — STUB.** `RecordRayMarchCommands` в `src/render/RayMarchPass.cpp:59` — `fprintf` в stderr, реальной работы не делает. Compute-шейдер `ray_march.comp` (Amanatides-Woo DDA) скомпилирован, API state (`SetRayMarchEnabled`/`IsRayMarchEnabled`/`RequestRayMarchPipelineRecreate`) работает, но в graphics command stream не вкомпонован. Phase 7 follow-up.

**TAA (по умолчанию) ВЫКЛЮЧЕН.** `taaEnabled=false`, `taaJitter=0`. Стабильная картинка без дрожания. **BUG-004 (VoxelLab tremor) — отвергнут, не существует.** Галлюцинация предыдущей сессии.

### 0.7. Phase 4-9 roadmap (per `docs/DefenseReport.md §3`)

| Phase | Цель | Триггер |
|---|---|---|
| 4 | Networking (server-authoritative + client prediction) | Post-MVP |
| 5 | SVO (Sparse Voxel Octree) + Mesh shaders (VK_EXT_mesh_shader) | Post-MVP |
| 6 | HDR-текстуры + полный клеточный автомат жидкости на GPU | Post-MVP |
| 7 | Полная система частиц + асинхронная загрузка ресурсов | Post-MVP |
| 8 | Плагины / моддинг API | Post-MVP |
| 9 | Многопользовательский режим (Academic vision) | Post-MVP |

5 отложенных пунктов из ТЗ (per `docs/DefenseReport.md §3`): частицы, моддинг, async load, HDR, SVO. Все явно в roadmap.

---

## Приложение A. Глоссарий

**AC** — Audio Command
**AABB** — Axis-Aligned Bounding Box (выровненный по осям ограничивающий параллелепипед)
**ACM** — Allocated Chunk Memory
**API** — Application Programming Interface
**ASC** — Actual Stream Count (CSM)
**AVX2** — Advanced Vector Extensions 2 (256-bit SIMD)
**B10G11R11** — 11-бит float + 10-бит float цвет (HDR format)
**CA** — Cellular Automaton (клеточный автомат)
**CFR** — Compact Frame Representation
**CMake** — Build system generator
**CPU** — Central Processing Unit
**CSM** — Cascaded Shadow Maps (каскадные карты теней)
**CSP** — Centralized Service Provider
**CTSH** — Contact Shadow (контактная тень)
**DDA** — Digital Differential Analyzer (raycast алгоритм)
**DOD** — Data-Oriented Design (дизайн, ориентированный на данные)
**DOA** — Data-Oriented Architecture
**DRACO** — Google mesh compression library
**DSA** — Dataflow Static Analyzer
**DT** — Decision Tree
**ECS** — Entity-Component System (Flecs)
**ES** — Entity System
**F11** — Walk air control mode
**F12** — Walk auto-jump delay
**FASTGFTF** — glTF 2.0 parser library
**FLECS** — MIT, header-only C++ ECS library
**FPS** — Frames Per Second
**GCC** — GNU Compiler Collection
**GLB** — glTF binary format
**GLM** — OpenGL Mathematics library (C++)
**GLTF** — Graphics Language Transmission Format (3D model standard)
**GPU** — Graphics Processing Unit
**HLSL** — High-Level Shading Language
**HUD** — Heads-Up Display
**JPH** — Jolt Physics namespace
**JOLT** — Jolt Physics library (MIT)
**JSON** — JavaScript Object Notation
**LD** — LLVM Disassembler
**LE** — Less-Equal
**LIBSTDC++** — GNU C++ Standard Library
**LIBC++** — LLVM C++ Standard Library
**LLDB** — LLVM Debugger
**LLVM** — Low-Level Virtual Machine
**LTO** — Link-Time Optimization
**LUT** — Look-Up Table
**M5** — Model-Vertex-Fragment (GPU shader stages)
**MESHOPT** — Mesh optimization library
**MIA** — Meshopt-Image-Atlas
**MINIAUDIO** — Single-header C audio library
**MIT** — Massachusetts Institute of Technology
**MMAP** — Memory-Mapped file
**MRS** — Mesh Rasterization State
**MRT** — Multiple Render Targets
**MVP** — Minimum Viable Product
**NGX** — NVIDIA GPU Extensions
**NDEBUG** — No Debug (macro for release builds)
**OBJ** — Wavefront Object file format
**OPENAL** — Open Audio Library
**OOO** — Out-Of-Order (CPU execution)
**PB** — Pipeline Barriers
**PBR** — Physically-Based Rendering
**PHYSX** — NVIDIA Physics library (proprietary)
**PI** — Pipeline Identifier
**PIPELINE** — Vulkan graphics/compute pipeline
**PMREM** — Pre-filtered Mipmapped Radiance Environment Map
**PNG** — Portable Network Graphics
**POM** — Parallax Occlusion Mapping
**PPE** — Personal Protective Equipment
**PRE-INSTANCE** — Vulkan stage (per-instance data)
**PRF** — Performance counter
**PVO** — Per-Vertex Offset
**QA** — Quality Assurance
**Q&A** — Questions and Answers
**R8G8B8A8** — 8-bit per channel color
**R11G11B10** — 11+11+10-bit float color (HDR)
**R16G16B16A16** — 16-bit float per channel color
**RAM** — Random Access Memory
**RAYCAST** — Ray casting (line-triangle test)
**RCS** — Revision Control System
**RDO** — Rate-Distortion Optimization
**REF** — Reference
**RHI** — Render Hardware Interface
**ROP** — Raster Operations Pipeline
**RP** — Render Pass
**RSX** — PlayStation 3 hardware (named for historical reasons)
**RT** — Ray Tracing
**SAH** — Surface Area Heuristic (BVH construction)
**SB** — Storage Buffer
**SC** — Shader Compiler
**SDF** — Signed Distance Field
**SHADER** — GPU program
**SIMD** — Single Instruction, Multiple Data (parallel processing)
**SLA** — Service Level Agreement
**SMAA** — Subpixel Morphological Anti-Aliasing
**SOA** — Structure of Arrays
**SOV** — Stack Overflow (joke reference)
**SPEC** — Specification
**SPIR-V** — Standard Portable Intermediate Representation (Vulkan)
**SSAO** — Screen-Space Ambient Occlusion
**SSBO** — Shader Storage Buffer Object
**SSE** — Streaming SIMD Extensions
**STATIC_ASSERT** — Compile-time assertion
**STB** — Sean T. Barrett (image library)
**STD** — Standard
**STL** — Standard Template Library
**SVO** — Sparse Voxel Octree
**SYNTHESIZE** — Auto-generate
**TAa** — Temporal Anti-Aliasing
**TASK** — Job
**TBDR** — Tile-Based Deferred Rendering
**TIER** — Implementation level (0-5)
**TM** — Tone Mapping
**TRACY** — Performance profiler (MIT)
**TRS** — Translation-Rotation-Scale
**TTS** — Text-To-Speech
**UB** — Uniform Buffer (Vulkan)
**UE** — Unreal Engine
**UI** — User Interface
**UPLOAD** — CPU-to-GPU memory transfer
**UV** — Texture coordinates (U, V)
**V** — Single-letter keyboard key
**VAO** — Vertex Array Object
**VAR** — Variable
**VBO** — Vertex Buffer Object
**VFS** — Virtual File System
**VMA** — VulkanMemoryAllocator
**VMA** — Vulkan Memory Allocator
**VOLK** — Vulkan loader (meta-loader for Vulkan API)
**VS** — Vertex Shader
**VULKAN** — Low-overhead graphics API
**WAV** — Wave audio format
**WGSL** — WebGPU Shading Language
**YCoCg** — Luma-Chroma-Orange-Chroma-Green color space

---

## Приложение B. Хронология решений

**2026-03 (начало):** ProjectV инициализирован. C++20 baseline. Single-developer.

**2026-04-09 (Tier 0.B):** `Mat4` (16-byte aligned) заменил `std::array<float, 16>` для GPU ABI parity в `VoxelSceneLighting` и `SunShadowCascadeProjections`. ABI change: `Vec3` (12→16 B), `VoxelSceneLighting` (+16 B = 624 B total).

**2026-04-12 (Tier 0.A):** Math foundation. `core/Math.hpp` + `core/Math.ixx`. per `agent/memory.md §10.1`.

**2026-04-12 (M5.1d, Tier 5):** Two-level chunk visibility cache (XOR-fold splitmix64 hash). Quantization: camera position 0.25 voxel units, camera forward 0.005 (~0.3°).

**2026-04-12 (Tier 4):** С-ядро `frustum_cull` scalar (3.7-3.9× faster than C++ baseline). AVX2 version kept in tree (2.5-2.7× faster, autovectorizer beats hand-rolled in debug). Crossover threshold 8 AABBs.

**2026-04-12 (M1):** `AudioEngine` + `miniaudio` integration. PulseAudio backend → PipeWire. 16/44100/stereo. MP3 only.

**2026-04-12 (Music HUD 1-line):** Single-line music HUD `MUSIC <state> VOL 0.80 TRK <name>`.

**2026-04-12 (P1 shadow fix):** SSBO double-buffer, fence reorder, cascade depth, TAA YCoCg clamp (commits b7e672f и др.).

**2026-04-12 (A1 greedy meshing, 4.1):** 6 per-axis greedy passes в compute shader. Заменён triple-nested loop over (X, Y, Z) × 6 directions.

**2026-04-12 (M5.1d asset-pipeline):** 4 commits landed: `8cc71f8` + др.

**2026-04-13 (Tier 1.B):** `std::expected<T, E>` migration на холодных путях. VulkanInit (16 variants), snapshot (3 variants), audio load (3 variants), scene config, ECS sync, physics state.

**2026-04-13 (Tier 2.D):** C++26 modules в mainline (Math.ixx, Probe.ixx, StringId.ixx). `import projectv.math;` probe работает.

**2026-04-13 (Tier 1.D/E):** `projectv::core::StringID` для manifest entry id. 16 B (hash + length + pad), O(1) equality, hashable.

**2026-04-13 (Fluid CA audit):** spread rule restored per `agent/decisions.md §30`. Без claimed-tracking — swap bug (два fluid'а обмениваются, один исчезает).

**2026-04-13 (Music HUD 1-line → 4-line):** commit `723edc5`. 4 lines per state: `MUSIC <state> VOL 0.80` (always), `ARTIST <name>`, `TITLE <name>`, `POS m:ss / m:ss` (when engine initialized + playlist non-empty).

**2026-04-13 (Hardcore perf r0):** Phase 0 = doc only. ctest baseline 14/14, 0.78s debug, 0.06s release.

**2026-04-14 (Release presets):** commits `6fe9201`. linux-clang-release / windows-clang-release. Conservative policy: -O3 -flto=thin -DNDEBUG. Без -ffast-math, без -march=native.

**2026-04-14 (Build config audit):** 5 buildPresets обновлены. linux-clang-debug-tracy-profiler PROJECTV_BUILD_TRACY_PROFILER ON→OFF (Linux Tracy UI не собирается). 14/14 ctest на release.

**2026-04-15 (KT-LaTeX):** KT-2.1/2.2/3.1/3.2 + Combined PDF.

**2026-04-15 (Defense preparation r0):** 10-мин скрипт + briefers + algorithms.md.

**2026-04-15 (Post-WBV-r1 batch):** F11/F12/V relocate → 1/2/3 (F5/F6 conflicts with InputAction). pragma once conversion (55 files). Shader contract fix (3 model/TAA-pipeline shaders).

**2026-04-15 (Defense docs overhaul r0):** 7 новых файлов + 4 переработки. commit `1db35ee`.

**2026-04-15 (Defense docs audit r0):** 23 hallucination corrections в 12 docs/. F5/F6 → F11/F12. commit `bf2822f`.

**2026-04-15 (Defense docs russian r0):** полная русификация 12 defense-документов. commit `d641967`.

**2026-04-15 (Windows build verification r0):** 5 atomic-commits. P0 libc++/Windows-clang-cl gating + Tracy UI split + RepoRoot extract + docs/cleanup + deinit 5 submodules 62M. commit `69b1726`.

**2026-04-16 (Defense team script rebuild r0):** Пересборка под 5-мин формат. 10 файлов, T3-T6 переписаны в стиле T1/T2, Q&A-карта 30+ вопросов. `DefenseScript_Solo.md` удалён. commit `45a15bc`.

**2026-06-17 (Defense team script close-routine):** active-sessions → closed, status.md §29. commit `a3849cd`.

**2026-06-17 (Defense competency FAQ r0):** Per-team competency FAQ (textbook, 1888 строк, 1 файл), архивация 4 устаревших 10-мин скриптов. commit `c14e1bd`.

**2026-06-17 (Defense competency FAQ split r0):** Монолитный FAQ разделён на 7 файлов (Common+INDEX + 6 per-team). Удалены 6 briefers (`DefenseBriefer_{1..5}.md` + `_le1t.md`) — verbatim в `DefenseScript_Team.md`, понятия и competency FAQ в per-team файлах. (current commit)

---

**Конец Common + INDEX.** Per-team FAQ файлы: `DefenseCompetencyFAQ_T1.md` ... `_T5.md`, `DefenseCompetencyFAQ_le1t.md`.
