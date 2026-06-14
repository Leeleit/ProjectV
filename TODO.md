# TODO — Hardcore perf / architecture r0 (`2026-06-13`)

**Снимок:** `2026-06-13` — Phase 0 (документация) закрыт, Phase 1+ (код) **после явного одобрения operator + commit'a Phase 0**.

**Source-of-truth shift:** По явной команде оператора «сейчас то, что ты написал в отчёте — приоритет номер 1, плюём на всё, что в TODO, сейчас занимаемся хардкором, который ты расписал». Все 846 строк старого TODO (всё `[x]`, history closed) — **отменены в части дальнейших приоритетов**; **сохранены** в `/tmp/before_todo_rewrite_20260613T1330.md` как historical record. **Новый roadmap** ниже построен вокруг Tier 0..5 плана из `agent/memory.md §11` + `agent/decisions.md §29`.

**Build preset baseline:** `linux-clang-debug` (Clang 22.1.6 + libstdc++ 16 + sccache, ctest 6/6 ≈ 1.38-1.50s). **Не трогать** `windows-clang-debug` / `linux-clang-debug-tracy-profiler` до явного «переключись».

**Scope discipline per `AGENTS.md §7.2.6`:** `external/`, `legacy/`, `docs/`, build-артефакты — **out of scope**. Build pipeline / submodule'и — frozen. Трогаю mainline `src/`, `tests/`, корневой `CMakeLists.txt`/`CMakePresets.json` (если нужно), `cmake/` (если нужно).

---

## Tier 0 — `Vec3/Vec4/Mat4` (alignas) + SIMD frustum cull + pre-reserve hot vectors

**Цель:** устранить zero-SIMD в hot path (P1, P2, P3 из `memory.md §11.2`). Локальный scope, измеримый bottleneck, нулевой ABI-влияние (Vec3 = 16 bytes = alignment pad, Mat4 = 64 bytes = уже aligned). Один atomic-подзадача = один commit.

- [ ] **A. `projectv::math::Vec3/Vec4/Mat4` (alignas 16/32)** — new `src/core/Math.hpp` (header-only). `Vec3` 16-byte aligned (pad), `Vec4` 16-byte aligned, `Mat4` 16-byte aligned. Scalar member access (`x`, `y`, `z`, `w`, `m[0]..m[15]`). Free functions: `dot(Vec3,Vec3)`, `cross(Vec3,Vec3)`, `length(Vec3)`, `normalize(Vec3)`, `operator*(Mat4,Vec4)`, `inverse(Mat4)`, `transpose(Mat4)`. **Verify:** `static_assert(sizeof(Vec3)==16)`, `static_assert(alignof(Vec3)==16)`, `static_assert(alignof(Mat4)==16)`, Godbolt: компилятор использует `movaps` / `vmovaps` (alignment-required SSE/AVX).
- [ ] **B. Заменить `std::array<float, N>` в hot structures** на `Vec3/Vec4/Mat4`: `src/core/Types.hpp` (CameraState, ChunkCullingParameters, GraphicsPushConstants, PackedSceneChunkDescriptor, etc.), `src/render/SceneResources.hpp` (PackedSceneChunkDescriptor fields, ChunkCullingParameters, FrameRenderData), `src/voxel/VoxelWorld.hpp` (Int3 → остаётся, но chunk координаты можно конвертировать), `src/app/Camera.cpp` (mat4 mul, inverse, perspective). **Zero ABI-change** (same sizes, только alignment).
- [ ] **C. Frustum cull — single templated SIMD function** — `IsSceneChunkVisible` + `IsAabbVisibleAgainstCameraFrustum` + `IsSceneChunkVisibleInShadowCascade` объединяются в одну `template<typename GetOrigin, typename GetHalfExtent> bool FrustumCull(GetOrigin, GetHalfExtent, ChunkCullingParameters)`. SIMD-вариант через `std::simd<float, 8>` (8 chunks параллельно), fallback на scalar. **Verify:** `TracyPlot("FrustumCulling (ms)")` до/после; ожидаем 8× speedup.
- [ ] **D. Pre-reserve `std::vector` в hot paths** — `src/voxel/VoxelWorld.cpp::QueueChunkRebuildRequest` (`pendingChunkRebuildIndices.push_back` per voxel edit), `src/render/SceneResources.cpp` (`ChunkVisibilityCache.opaqueCommands/shadowCommands/transparentCommands` push_back per chunk per frame), `src/render/Renderer.cpp` (`DebugOverlayBoxes` push_back per frame), `src/app/InputReplay.cpp` (`InputReplayCapture.frames` per recorded frame). Либо `reserve(maxPossible)` на init, либо `std::inplace_vector<T, N>` (Tier 1, если cap известен).
- [ ] **E. Godbolt-ревью intrinsics vs auto-vectorize** — для каждой hot-функции после Tier 0.C проверить, что SIMD реально компилируется в AVX (не остался scalar из-за зависимостей). Если auto-vectorize даёт то же — `[[likely]]` / loop restructuring помогут.
- [ ] **F. **Tier 0 commit (atomic)** — `<type>(<scope>): <summary>` per `§7.2.5`. Заголовок: `perf(render): alignas Vec3/Mat4 + SIMD frustum cull + pre-reserve hot vectors` (или split на 2-3 commit'а если diff > 800 lines). Body: per-commit motivation, expected regression risk, build state.
- [ ] **G. Verify build green** — `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8` clean. `ctest 6/6` baseline preserved. `TracyPlot` показывает >5% FrustumCulling speedup (Tier 0.C). Sidecar captures VoxelLab/FlatBenchmark/MeshingStress unchanged.

**Tier 0 exit criteria:** SIMD working в cull + cull time measurably снижен + zero new warnings + ctest baseline. **Следующий Tier начинается только после явного одобрения operator + apply commit'a.**

---

## Tier 1 — `std::inplace_vector` + `std::expected` (cold) + StringID

**Цель:** устранить hot-path `std::vector` realloc (Tier 0.D follow-up), cold-path `bool` → `std::expected<T,E>` (A1, P4 из `memory.md §11.1-§11.2`), ввести StringID тип (P4 — 0 StringID в проекте).

- [ ] **A. `std::inplace_vector<VkDrawIndirectCommand, 1024>`** — заменить `std::vector` в `src/core/Types.hpp::ChunkVisibilityCache` (cap 1024 покрывает worst-case VoxelLab + MeshingStress). Fixed-cap, stack-friendly, no realloc. `static_assert(inplace_vector<VkDrawIndirectCommand, 1024>::capacity() == 1024)`.
- [ ] **B. `std::expected<T, E>` для cold path** — рефакторинг следующих функций (cold = init/load/file I/O, не per-frame per-entity):
  - `src/voxel/VoxelWorld.cpp::SaveVoxelWorldSnapshot` → `std::expected<bool, VoxelSnapshotError>` (enum `FileNotFound`, `WriteError`, `HeaderCorrupted`)
  - `src/voxel/VoxelWorld.cpp::LoadVoxelWorldSnapshot` → `std::expected<std::unique_ptr<VoxelWorld>, VoxelSnapshotError>`
  - `src/asset/AssetLoader.cpp::Load*` → `std::expected<AssetData, AssetLoadError>` per asset type
  - `src/audio/AudioEngine.cpp::loadMusicFolder` → `std::expected<size_t, AudioLoadError>` (size_t = track count, 0 = valid)
  - `src/asset/ModelManifestLoader.cpp::load*` → `std::expected<ModelManifest, ManifestError>`
  - `src/render/vulkan/VulkanInit.cpp::InitVulkan` → `std::expected<VulkanContext, VulkanInitError>` (per-step error)
  - `src/render/vulkan/VulkanSwapchain.cpp::CreateOrRecreateSwapchain` → `std::expected<VkFormat, SwapchainError>`
  - `src/render/TaaRenderTargets.cpp::CreateOrRecreateTaaRenderTargets` → `std::expected<TaaRenderTarget, TaaError>`
- [ ] **C. `.and_then()` / `.or_else()` / `.transform()` композиция** в cold-path вызовах (main.cpp::SDL_AppInit, SDL_AppIterate snapshot load, asset load). Не заставлять каждый caller обрабатывать все error variants — `.or_else(fallback)` где есть fallback.
- [ ] **D. `StringID` тип** — new `src/core/StringId.hpp`: `struct alignas(8) StringID { uint64_t hash; uint32_t length; };` + `constexpr StringID` ctor от `const char (&)[N]` (FNV-1a 64-bit на compile time). `operator==`/`!=` по (hash, length). `StringID("rock_diffuse")` constexpr.
- [ ] **E. Заменить `std::string` в hot path на StringID**:
  - `src/core/Types.hpp::ModelRegistryEntry::id: std::string` → `StringID`
  - `src/audio/AudioEngine::m_currentTrackName` → `StringID` (track filename); `m_currentArtist` / `m_currentTitle` → `StringID` (parsed sub-fields); `currentTrackName()` возвращает `StringID`, mirror в `DebugStats::audioMusicTrackName` — `std::array<char, 128>` остаётся (HMI readable).
  - `src/core/Types.hpp::InputReplayState::replayPath` / `InputReplayCapture::snapshotPath` → `std::filesystem::path` (это OS path, не StringID).
  - `VoxelScenePresetToString` → `constexpr std::string_view` (compile-time), `ParseVoxelScenePreset` → `std::optional<VoxelScenePreset>`.
- [ ] **F. **Tier 1 commit (atomic)** — `feat(core): std::inplace_vector + std::expected cold + StringID type`. Body: scope list, refactor count, error contract per функция. Verify ctest 6/6.
- [ ] **G. Verify build green + ctest baseline** — `linux-clang-debug` clean, ctest 6/6.

**Tier 1 exit criteria:** 0 `std::string` в hot path; cold path типобезопасный; inplace_vector заменил 1+ hot vector; StringID constexpr-constructed в hot path.

---

## Tier 2 — C++20 modules (`.ixx`) — mainline

**Цель:** ускорить сборку 2-5× per `§06_compile-time-philosophy.md`. Оператор явно сказал «mainline, не probe build tree».

- [ ] **A. Подготовить module files** — `src/core/Math.ixx` (Vec3/Vec4/Mat4 + StringID), `src/core/Types.ixx` (forward declarations + opaque types), `src/ecs/EcsWorld.ixx` (ECS API). Каждый `export module projectv.{core,math,ecs};` + `export` declarations.
- [ ] **B. CMake support** — root `CMakeLists.txt` или `src/CMakeLists.txt`: для каждого target добавить `target_sources(... PRIVATE FILE_SET CXX_MODULES FILES ...)`. `CMAKE_CXX_SCAN_FOR_MODULES ON` (default в CMP0155 NEW для C++20+). CMake 4.x на mainline, проверено 3.30+.
- [ ] **C. `import std;`** — `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD d0edc3af-4c50-42ea-a356-e2862fe7a444` + `CMAKE_CXX_MODULE_STD ON` в корневом `CMakeLists.txt` (до `project()`). Probe в `src/CMakeLists.txt` — добавить probe-TU, который только `import std;` и печатает `sizeof(int)`. Build green? Продолжаем. Нет — фикс CMake, отчёт в `decisions.md`.
- [ ] **D. Миграция** — по одному файлу `import projectv.core;` / `import projectv.math;` / `import projectv.ecs;` в `src/app/*.cpp`, `src/voxel/*.cpp`, `src/render/*.cpp`. **Не мигрируем** `.cpp` который `#include` Vulkan/SDL/flecs/Jolt headers (оставляем в `#include` через `target_include_directories`).
- [ ] **E. Замер build time** — `time cmake --build build/linux-clang-debug --target ProjectV --parallel 8` до/после. Ожидаем 2-5× speedup на cold rebuild (полная сборка), 1.5-2× на incremental (один файл).
- [ ] **F. **Tier 2 commit (atomic)** — `build(cmake): enable C++20 modules + import std`. Body: build time before/after, modules file list, `import std;` experimental gate rationale.
- [ ] **G. Verify build green + ctest baseline** — `linux-clang-debug` clean, ctest 6/6. `windows-clang-debug` тоже green (operator явно не сказал «не трогать» для Tier 2; cross-platform verify).

**Tier 2 exit criteria:** все mainline `.cpp` импортируют modules вместо `#include`-of-our-headers. Build measurably faster. Clang 22 + clang-cl 22 оба green.

---

## Tier 3 — C / intrinsics (Godbolt + benchmark)

**Цель:** устранить оставшийся scalar hot path (если после Tier 0 ещё есть bottleneck по Tracy). Godbolt-ревью каждого intrinsics.

- [ ] **A. `src/bench/FrustumCullBenchmark.cpp`** — Google Benchmark. Замер: 300 chunks × 5 visibility tests, scalar vs `std::simd<float, 8>` vs AVX2 intrinsics. Compile flags: `-O2 -mavx2 -fno-sanitize=address` (Clang 22 Issue #194008 workaround для ASan + AVX2 vectorizer stack smash). `--benchmark_min_time=2s` для стабильности.
- [ ] **B. CMake support** — `cmake/ProjectVThirdParty.cmake` (existing) или новый `cmake/GoogleBenchmark.cmake` — `find_package(benchmark REQUIRED)`. Если нет — `FetchContent` или git submodule. **Verify:** `ctest -L BENCHMARK` (отдельный label, не в основном ctest 6/6).
- [ ] **C. `src/c_kernels/frustum_cull.c`** — C26 kernel, extern "C" wrapper, `__attribute__((target("avx2")))`. Header `src/c_kernels/frustum_cull.h` для C++ caller. **Godbolt-ревью** (https://godbolt.org/, Clang 22, x86-64-clang 22.1.6, -O2 -mavx2): убедиться, что компилятор выдаёт `vbroadcastss` / `vdpps` / `vmovmskps` для dot product, не остаётся scalar.
- [ ] **D. Если SIMD win > 5%** — оставляем. Если ≤5% — rollback, `[[likely]]` + branchless reorg дают то же.
- [ ] **E. **Tier 3 commit (atomic)** — `perf(bench): FrustumCullBenchmark + C kernel + intrinsics`. Body: до/после measurements, Godbolt screenshots, decision rationale.

**Tier 3 exit criteria:** benchmark показывает выигрыш ИЛИ документирует, что SIMD не нужен (auto-vectorize достаточно).

---

## Tier 4 — R&D (отложено, не блокирует mainline)

- [ ] `std::execution` (P2300, Senders/Receivers) — нужна Job System, отдельный slice. **Не делаем**, пока нет demand на многопоточность.
- [ ] Static reflection (P2996) — Clang fork only (Dan Katz). Mainline Clang не имеет. Подождать Clang 23+ или stdlib C++26.
- [ ] Contracts (P2900) — Clang 22 experimental, не zero-cost в debug. Подождать Clang 23+ или production-ready.
- [ ] `std::hive` (P0447) — MSVC preview, libstdc++ stable, libc++ in progress. Подождать libc++ 19+.
- [ ] Mesh shaders (VK_EXT_mesh_shader) — mainline MVP не требует. R&D для future, не в Tier 0..3.
- [ ] SVO GPU (Sparse Voxel Octree on GPU) — R&D, огромный scope, не mainline.
- [ ] C26 / C-kernels (отдельно от Tier 3 C-frustum-cull) — нет C файлов в mainline, отложено до demand (audio DSP kernel, например).
- [ ] Inline asm (`__asm__`) — отложено до конкретного use-case (prefetcht0 в SPSC queue, pause в spinlock).

---

## Tier 5 — прочее (по ходу Tier 0..3)

- [ ] **`[[likely]] / [[unlikely]]`** в `IsSceneChunkVisible` early-out (`chunkExtentAndNonAir[3] == 0u` return false — 50% chunks = air).
- [ ] **`[[assume]]`** в hot loops: `IsSceneChunkVisible(assume(parameters.cameraUpAndNearPlane[3] >= 0.0f))` (assertion безопасна, не проверяется в Release).
- [ ] **3 копии DDA trace в `voxel.frag`** → шаблонизировать через `#define IS_OCCLUDER` или function pointer parameter (медленнее, не делать). Или macro `#define DDA_BODY(IS_OCCLUDER_FN)` 3× substitute.
- [ ] **`// EVIL:` комментарии** на magic numbers per `§04_evil-hacks-philosophy.md §3`. VoxelLab имеет `0.05, 0.14, 0.03, 0.02, 0.001, 0.0001, 0.75, 0.35, 0.65, 0.55, 0.08, 0.28, 0.45, 1.10, 1.50, 8.0, 12.0, 0.10, 0.4, 0.5` — все без `EVIL:`.
- [ ] **Google Benchmarks** для всех hot path: `ShadowProjectionBenchmark`, `VoxelStorageBenchmark`, `MatMulBenchmark`, `BuildGraphicsPushConstantsBenchmark`. Каждый Tier 0/1/2/3 закрывает соответствующий benchmark slice.
- [ ] **Tests** для `BuildGraphicsPushConstants`, `ComputeVisibilityCacheHash`, `BuildSunShadowCascadeSplits`, `CreateOrRecreateTaaRenderTargets` (per `§04_testing-philosophy.md` — критичные инварианты).
- [ ] **`std::array` → `std::span`** для non-owning buffer views (после Tier 1 inplace_vector, остальные `std::array<T,N>` где `T` — opaque handle).
- [ ] **`vkWaitForFences` с timeout=10ms** вместо `UINT64_MAX` (per `§01_optimization-philosophy.md` «low latency > throughput»).
- [ ] **Проверить и починить** `InputAction` bit-mask overflow в `InputReplayFrame` (uint32_t vs 60+ actions) — `src/app/InputActions.cpp` — bit-маска или raw indices?
- [ ] **`AppState` PIMPL refactor**: `AppState = std::unique_ptr<AppStateImpl>` (forward-decl в `core/Types.hpp`), `AppStateImpl` владеет `RenderContext` + `SimulationContext` + `BootstrapContext`. `~AppStateImpl()` в правильном порядке.
- [ ] **`UpdateApp` mirror helpers** — `MirrorDebugStatsFromRender/Audio/Physics/Camera/Input`. Сейчас ~200 строк mirror блоков вручную.
- [ ] **Read-only safety net pattern** — `git diff > /tmp/before_<subtask>_<timestamp>.patch` per atomic-подзадача per `AGENTS.md §7.2.4`. Уже в `§11.5 memory.md`.

## CA audit (Phase 2 sub-task, `2026-06-13`)

- [x] **Audit `UpdateFluidCA`** (`src/voxel/VoxelWorld.cpp:1286-1434`) — CPU fluid CA, единственная в mainline.
- [x] **Spread rule удалена** — оператор: «Только падает, не растекается». ~30 строк (spread branch, hash, side array, support check).
- [x] **PV_ASSERTs** добавлены (debug-only): pre-conditions (voxels.size() == width*height*depth, dimensions > 0), post-condition (stats.fluidVoxelCount == std::count(voxels, == Fluid)).
- [x] **Determinism contract** документирован в `src/voxel/VoxelWorld.hpp:154-191`.
- [x] **Throttle tightened** — `static bool fluidTickInitialized` заменил fragile `lastFluidTickCounter == 0u` check (`src/app/main.cpp:633-643`).
- [x] **Tests** — `tests/FluidCATests.cpp` (12 sub-tests, 100% pass) + `tests/CMakeLists.txt:706-749` (new `ProjectVFluidCATests`).
- [x] **`agent/decisions.md §30`** — full audit + 8 operator decisions.
- [x] **`agent/memory.md §12`** — Fluid CA audit summary.

False alarms (для потомков):
- [x] «CA в AppEvent vs AppIterate» — false alarm. Code уже в AppIterate (`main.cpp:580-639`).
- [x] «Double-step gravity» — false alarm. Y-ascending iteration уже bottom-up. **НО:** столбец **percolates** вниз за 2N тиков (документировано в `decisions.md §30` + `TestFluidCAColumnPercolatesDownAndSettlesAtY0`).

«Вода не течёт вниз» — expected behavior (fluid на glass ≠ падает). Проверено `TestFluidCAFluidOnGlassStaysPutThenFallsWhenGlassBreaks`.
«Respawn за платформой» — был spread rule. Удаление spread rule решает его напрямую.

---

## Pre-flight checklist per atomic-подзадача (per `AGENTS.md §7.2.6.1` + `§7.2.4`)

1. **Pre:** `git diff > /tmp/before_hardcore_r0_<subtask>_<timestamp>.patch` (safety-net для destructive rollback).
2. **Pre:** `git status -uall` clean baseline.
3. **Work:** только файлы в `files-touched-intent` active-session записи. Никаких `external/`, `legacy/`, `docs/`, build-артефактов.
4. **Verify:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8` clean. `ctest 6/6` baseline.
5. **Commit:** предложен пользователю per `§7.2.5`, **не auto-execute**. Commit message формат: `<type>(<scope>): <summary>` + body + Refs.
6. **Update active-sessions.md:** status `closed` + commit-hash только после явного `git commit` от оператора.

---

## Done (Phase 0, doc-only, `2026-06-13`)

- [x] Полный технический отчёт сохранён в `agent/memory.md §11` (13 KB).
- [x] Новое правило `std::expected` cold/bool hot в `agent/decisions.md §29`.
- [x] Snapshot в `agent/status.md §20` (Phase 0 done, Phase 1+ pending operator).
- [x] Active session `session-2026-06-13-hardcore-perf-r0` registered в `agent/active-sessions.md`.
- [x] Этот TODO переписан под Tier 0..5.
- [ ] **Phase 0 commit** — предложен пользователю (не auto-execute).
- [ ] **Phase 1+ (Tier 0 код)** — после явного одобрения operator.

---

## Cross-refs

- `agent/memory.md §11` — comprehensive technical-debt + plan + web research bookmarks.
- `agent/decisions.md §29` — Tier plan + error-handling rule + refactor scope.
- `agent/status.md §20` — Phase 0 snapshot + operator answers.
- `agent/active-sessions.md` session-2026-06-13-hardcore-perf-r0 — active session.
- `legacy/docs/philosophy/01_foundation/{04,05,06,07,08,09}_*.md` — anti-patterns, compiler, compile-time, memory, errors, data-layout.
- `legacy/docs/philosophy/02_paradigms/{01,02,06}_*.md` — zero-cost, DoD, strings.
- `legacy/docs/philosophy/03_domain/{01,04}_*.md` — optimization, testing.
- `/tmp/before_todo_rewrite_20260613T1330.md` — backup старого TODO (846 строк history, все `[x]`).
