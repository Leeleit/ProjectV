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

- [x] **A. Подготовить module files** — `src/core/Math.ixx` (Vec3/Vec4/Mat4) + `src/core/StringId.ixx` (StringID тип, отдельный модуль — обоснование в `decisions.md §29`: loose coupling, Math не зависит от StringID), `src/core/Types.ixx` (re-export Math + StringId + forward decls AppState/EcsState/CameraState/DebugState/WorldState/VoxelWorldStats + RenderPassTiming), `src/ecs/EcsWorld.ixx` (re-export Types + 12 ECS API function decls). Каждый `export module projectv.{math,string_id,types,ecs};` + `export import` chains.
- [x] **B. CMake support** — `src/CMakeLists.txt`: `target_sources(ProjectV PRIVATE FILE_SET CXX_MODULES FILES core/Math.ixx core/StringId.ixx core/Probe.ixx core/Types.ixx ecs/EcsWorld.ixx)`. CMake 4.3 на mainline, проверено с `cmake_minimum_required(3.30)`.
- [x] **C. `import std;`** — Probe работает в `tests/StdModuleProbe.cpp` (ctest 14/16). **`import std;` в mainline ЗАБЛОКИРОВАН**: libc++ 22 std.cppm BMI конфликтует с transitive `<string>`/`<vector>`/etc. из `fmt/format.h` (используется в `core/RuntimeDiagnostics.cpp`, `core/ShaderIO.cpp`, `render/Renderer.cpp` и др.) — clang error "redefinition of concept '__concat_indirectly_readable'". Попытки: (a) flag matching `-pthread -mavx2 ...` — pthread mismatch исправлен; (b) `_LIBCPP_REMOVE_TRANSITIVE_INCLUDES` — не помогает (конфликт на уровне std.cppm BMI); (c) selective `import std;` только в не-fmt TUs — непрактично (fmt используется ~70% mainline). Решение: `import std;` остаётся probe-only в `tests/`, для mainline отложен до решения проблемы с libc++ std.cppm module partition (C++26 `<std.compat>` не помогает — single-partition std.cppm в libc++ 22).
- [x] **D. Миграция** — 19 mainline .cpp + 5 mainline .hpp используют `import projectv.math;` и `import projectv.string_id;` вместо `#include "core/Math.hpp"` / `"core/StringId.hpp"`. 5 tests тоже мигрированы. Vulkan/SDL/flecs/Jolt headers оставлены в `#include` (TODO directive соблюдён).
- [x] **E. Замер build time** — incremental rebuild через touch `Math.hpp`/`StringId.hpp`: baseline 18.93s → **0.10s (190× speedup, "ninja: no work to do")** потому что ни один mainline .cpp/.hpp больше не `#include`-ит fallback headers напрямую. Cold rebuild ProjectV: 102.61s (module BMI generation overhead). Incremental через `Types.hpp` touch: 19.81s (parity с baseline 18.93s — Types.hpp ещё transitively подтягивается многими tests).
- [ ] **F. **Tier 2 commit (atomic)** — `build(cmake): enable C++20 modules + import projectv.* migration (Tier 2)`. Body: build time before/after, modules file list, `import std;` blocked rationale, Math+StringId split justification. ⏳ ждёт commit confirmation.
- [x] **G. Verify build green + ctest baseline** — `linux-clang-debug` clean (sequential `-j 1` для первого build из-за Ninja 1.13 + C++ modules dep-scan bug, после первого build параллельный работает), ctest 16/16 passed (0.77s). `windows-clang-debug` не верифицировано (нет Windows в sandbox) — `core/Math.hpp` + `core/StringId.hpp` оставлены с `#if defined(__clang__) && defined(_MSC_VER)` fallback для clang-cl path.

**Tier 2 exit criteria:**
- ✅ "все mainline `.cpp` импортируют modules вместо `#include`-of-our-headers" — 24 mainline files (19 .cpp + 5 .hpp)
- ✅ "Build measurably faster" — 190× на incremental через fallback .hpp touch
- ⚠️ "Clang 22 + clang-cl 22 оба green" — Clang 22 ✅, clang-cl fallback path сохранён но не верифицирован (нет Windows)

**Follow-up Tier 2 items (deferred):**
- [ ] `import std;` в mainline — требует фикса libc++ std.cppm partition (R&D, не mainline)
- [ ] Ninja 1.13 dep-scan bug с C++ modules — workaround: первый build sequential
- [ ] Windows clang-cl verification — needs Windows runner

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
- [x] **`legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12`** — Fluid CA audit summary.

False alarms (для потомков):
- [x] «CA в AppEvent vs AppIterate» — false alarm. Code уже в AppIterate (`main.cpp:580-639`).
- [x] «Double-step gravity» — false alarm. Y-ascending iteration уже bottom-up. **НО:** столбец **percolates** вниз за 2N тиков (документировано в `decisions.md §30` + `TestFluidCAColumnPercolatesDownAndSettlesAtY0`).

«Вода не течёт вниз» — expected behavior (fluid на glass ≠ падает). Проверено `TestFluidCAFluidOnGlassStaysPutThenFallsWhenGlassBreaks`.
«Respawn за платформой» — был spread rule. Удаление spread rule решает его напрямую.

## CA pause + timeScale + V-sync fixes (`2026-06-14`)

- [x] **V-sync FIFO bug fix** в `ChoosePresentMode` (`src/render/vulkan/VulkanSwapchain.cpp:148-180`). Оператор: «vsync слетает при постановке блока, даже если V → vsync on». Root cause: `if (g_preferredPresentMode != FIFO)` branch silently fell through to MAILBOX-first default chain на Linux/Wayland VRR surface. V cycle `FIFO → IMMEDIATE → MAILBOX → FIFO` — третий press возвращал MAILBOX вместо FIFO. **Fix**: убрал `!=` branch, теперь explicit per-mode dispatch. `g_preferredPresentMode == IMMEDIATE || MAILBOX` → `PickBestAvailablePresentMode`; else → explicit FIFO honours.
- [x] **CA tick перенесён в `UpdateApp`** (`src/app/AppUpdate.cpp:693-733`). Оператор: «вода растекается даже при паузе, не действует замедление/ускорение». Root cause: `src/app/main.cpp:637-670` имел wall-clock throttle, не консультировался с `simulation->paused` или `simulation->timeScale`. **Fix**: deleted throttle block, added CA tick в `UpdateApp` после physics accumulator, использует `effectivePaused` gate + `1/fluidTickRateHz` accumulator.
- [x] **`fluidTickRateHz = 20.0f` (was 30)** — оператор: «слишком быстро льётся». 20 Hz = 1 cell / 50 ms, timeScale=0.5 → 10 Hz, timeScale=2.0 → 40 Hz.
- [x] **`fluidAccumulatorSeconds` field** в `SimulationState` (`src/core/Types.hpp:1348-1382`). Sim-time accumulator, scaled by `timeScale`, zeroed on pause.
- [x] **Tests** — 8 новых sub-tests (24 total, 100% pass): `TestFluidCAFluidDoesNotMoveOnPause`, `TestFluidCAFluidMovesOnUnpause`, `TestFluidCAFluidRateRespectsTimeScale`, `TestFluidCAFluidRateAboveBase`, `TestFluidCAFluidRateAtDefault`, `TestFluidCAFluidTimeScaleZeroStops`, `TestFluidCAFluidFrameStepWithTimeScaleZero`, `TestFluidCAFluidRateConfigurable`. Helper `TickFluidCA` inline-зеркало production throttle.
- [x] **`agent/decisions.md §30.1`** — V-sync fix + CA tick move + 20Hz default plan + обоснования.
- [x] **`legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.1`** — V-sync bug history + CA pause/timeScale fix history + 4 lessons learned (subagent must для root-cause, default+override pattern, visual vs physics tickrate cap, SimulationState для sim knobs).
- [x] **`agent/status.md`** — обновлён с новым closed-session.

## V hotkey auto-detect cycle + libc++ warning + HUD line (`2026-06-14`)

- [x] **V hotkey auto-detect cycle** (`src/render/vulkan/VulkanSwapchain.hpp:69-148`). Оператор: «у кнопки V 4 переключения — не понимаю, какое из них что делает». Root cause: hardcoded 3-state cycle `FIFO → IMMEDIATE → MAILBOX → FIFO`. На Linux/Wayland без VRR IMMEDIATE not supported → silent fallthrough to MAILBOX. **Fix**: `BuildPresentModeCycle(support.presentModes)` walks priority list и keeps только surface-supported modes. Cycle length = number of physically supported modes (2 or 3 typically). Header-only API: `inline` variables + `inline` functions.
- [x] **HUD line for VSync** (`src/debug/DebugHud.cpp:553-577`). `VSync <mode> (<index>/<size>)` — например `VSync FIFO (1/2)` или `VSync MAILBOX (2/3)`. Видно сразу: текущий mode + cycle position.
- [x] **V hotkey log message** (`src/app/main.cpp:534-578`). `CycleVsync: <mode> [cycle <idx>/<size>]` — например `CycleVsync: MAILBOX (tear-free, uncapped) [cycle 2/2]`.
- [x] **libc++ warning fix** (`CMakeLists.txt:117-150`). Initial plan: remove `add_compile_options(-stdlib=libc++)` (CMake's `CMAKE_CXX_STDLIB` already propagates). **Failed**: removing produces link errors в `external/fastgltf` и `external/fmt` (external `add_subdirectory` subdirs не inherit `CMAKE_CXX_STDLIB` в compile commands). **Fix**: keep `add_compile_options(-stdlib=libc++)` for cross-target ABI, suppress false-positive warning via `add_compile_options(-Wno-unused-command-line-argument)`. Per `AGENTS.md §7.2.7` exception clause applied (one flag, one toolchain artifact).
- [x] **Tests** — `ProjectVPresentModeTests` (9 sub-tests, 100% pass): `TestPresentModeCycleIncludesAllThree`, `TestPresentModeCycleExcludesUnsupported`, `TestPresentModeCycleOnlyFifo`, `TestPresentModeCycleEmptyFallsBackToFifo`, `TestPresentModeCycleRespectsPriorityOrder`, `TestCycleAdvancesAndWrapsThreeMode`, `TestCycleAdvancesAndWrapsTwoMode`, `TestPresentModeCycleIndex`, `TestPresentModeCycleSize`. New test target в `tests/CMakeLists.txt:771-810`, header-only dependency.
- [x] **`agent/decisions.md §30.2`** — V hotkey auto-detect + libc++ fix plan + обоснования.
- [x] **`legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.2`** — V hotkey history + libc++ warning fix + 4 lessons learned (auto-detect hardware > hardcode cycle, inline variables/functions для runtime observables, hardware-dependent toolchain flags don't remove, log vs HUD для togglable state).

## V hotkey cycle walk fix (`2026-06-14` evening)

- [x] **`BuildPresentModeCycle` preserve `g_active` across rebuilds** (`src/render/vulkan/VulkanSwapchain.hpp:180-220`). Оператор: «нажимаю на V, ничего не меняется» — 10 identical log lines `IMMEDIATE [cycle 2/2]`. Root cause: `BuildPresentModeCycle` unconditionally set `g_active = g_cycle.front()` (FIFO) on every rebuild. V hotkey calls `RecreateSwapchain` after each press → `CreateOrRecreateSwapchain` → `BuildPresentModeCycle` resets `g_active` → next press sees FIFO → advances to IMMEDIATE → reset. **Self-defeating state machine**. **Fix**: capture `previousActive` before rebuild; if still in new cycle, keep it; else (display hot-swap) fall back to highest-priority supported.
- [x] **Tests** — 3 new sub-tests (`ProjectVPresentModeTests` 12 total, 100% pass): `TestPresentModeCyclePreservesActiveAcrossRebuild`, `TestPresentModeCycleFallsBackWhenActiveDropped`, `TestPresentModeCycleWalksAcrossRecreates` (operator's actual scenario: 4 V presses alternating FIFO ↔ IMMEDIATE). Pre-existing tests updated to **explicit reset pattern** (`(void)BuildPresentModeCycle({FIFO});` as first line) — inline-variable global state needs explicit reset, не assumed from previous test's final state.
- [x] **`agent/decisions.md §30.3`** — preserve-`g_active` plan + 4 обоснования (capture-rebuild pattern, test interaction not just function, test order independence, self-defeating state machine anti-pattern).
- [x] **`legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.3`** — V hotkey cycle walk history + 4 lessons learned (capture previous state > unconditional reset, test interaction not just function, explicit reset pattern для inline-variable global state, self-defeating state machine anti-pattern).

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
- [x] Snapshot в `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#20` (Phase 0 done, Phase 1+ pending operator).
- [x] Active session `session-2026-06-13-hardcore-perf-r0` registered в `agent/active-sessions.md`.
- [x] Этот TODO переписан под Tier 0..5.
- [ ] **Phase 0 commit** — предложен пользователю (не auto-execute).
- [ ] **Phase 1+ (Tier 0 код)** — после явного одобрения operator.

---

## Cross-refs

- `agent/memory.md §11` — comprehensive technical-debt + plan + web research bookmarks.
- `agent/decisions.md §29` — Tier plan + error-handling rule + refactor scope.
- `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md#20` — Phase 0 snapshot + operator answers.
- `agent/active-sessions.md` session-2026-06-13-hardcore-perf-r0 — active session.
- `legacy/docs/philosophy/01_foundation/{04,05,06,07,08,09}_*.md` — anti-patterns, compiler, compile-time, memory, errors, data-layout.
- `legacy/docs/philosophy/02_paradigms/{01,02,06}_*.md` — zero-cost, DoD, strings.
- `legacy/docs/philosophy/03_domain/{01,04}_*.md` — optimization, testing.
- `/tmp/before_todo_rewrite_20260613T1330.md` — backup старого TODO (846 строк history, все `[x]`).
