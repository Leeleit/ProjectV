# Archived: agent/memory.md §12, §12.1, §12.2, §12.3 (Fluid CA audit + Phase-2 follow-ups)

**Archived on:** 2026-06-15 (per `docs(agent): compress+archive` commit, planned).
**Reason:** Per-session log of the Fluid CA audit + the 3 follow-up slices
(V-sync FIFO + CA pause/timeScale, V hotkey auto-detect + libc++ warning,
V hotkey cycle walk across RecreateSwapchain). Each slice's commit-hash
and lesson-learned text is preserved verbatim.
**Active references** in `agent/decisions.md` §30 (Fluid CA contract) — kept live.
**Section numbering preserved** so external cross-refs resolve through
`agent/ARCHIVE-INDEX.md` with same anchors.

---

## 12. Fluid CA audit (`2026-06-13`)

`UpdateFluidCA` (in `src/voxel/VoxelWorld.cpp:1286-1434`) — единственная cellular automata в mainline. Изначально
`f_fall` + `f_spread` (4 cardinal neighbours с "concave ground" support check), snapshot-read pattern, `z, y, x`
ascending iteration. ~110 строк.

**CRITICAL BUG FOUND (`2026-06-13`):** commit loop использовал local coords (x ∈ [0, width)) как world coords при вызове
`SetVoxelMaterial`. Для VoxelLab (`min = (-12, 0, -12)`, `width = 24`) local `x=12` → world `x=12` →
`IsInsideVoxelWorld` rejects (`x < maxExclusive.x = 12` false). **Все falls в VoxelLab silently dropped.** Для x<12 —
silently landed at wrong cell. **Это и был «вода не падает»** в production. Fix: добавить `world.min` offset перед
`SetVoxelMaterial` в commit loop (`VoxelWorld.cpp:1402-1422`).

**Audit findings (все закоммичены в Phase 2 сессии `session-2026-06-13-hardcore-perf-r0`):**

1. **Spread rule удалена** — `f_spread` (the "concave ground" branch) была source of the "fluid respawns off the
   platform" perception. Платформа 4×3, видимый checkerboard 24×24, оба treated as valid `hasSupport` targets. Оператор:
   «только падает, не растекается». ~30 строк удалено (spread branch, hash, side array, support check).
2. **«CA в AppEvent» — false alarm.** Throttle block в `src/app/main.cpp:621-643` уже в `SDL_AppIterate` (function на
   строке 580). Side fix: `static bool fluidTickInitialized` заменил fragile `lastFluidTickCounter == 0u` check.
3. **«Double-step gravity» — false alarm.** Y-ascending iteration (line 1339: `for (int y = 0; y < height; ++y)`) уже
   bottom-up. Без double-step. **НО:** столбец fluid **percolates** вниз за 2N тиков, не N. Tick 0 = 1 cell, tick 1 = 2
   cells, …, tick N-1 = N cells (cascade вверх), symmetric cascade вниз к settled state. Документировано в
   `TestFluidCAColumnPercolatesDownAndSettlesAtY0`.
4. **PV_ASSERTs** добавлены (debug-only): pre-conditions (voxels.size() == width*height*depth, dimensions > 0),
   post-condition (stats.fluidVoxelCount == std::count(voxels, == Fluid)).
5. **Determinism contract** документирован в `src/voxel/VoxelWorld.hpp:154-191` рядом с declaration. Verified by
   `TestFluidCADeterministicAcrossRuns` (run twice, compare bytes).
6. **CRITICAL: commit loop coordinate bug** — fixed by adding `world.min` offset. Verified by
   `TestFluidCAVoxelLabSphereFallOnGlassBreak` (builds world with `min = (-12, 0, -12)` mirror production VoxelLab,
   breaks bottom glass, asserts fluid falls).

**Tests:** `tests/FluidCATests.cpp` (13 sub-tests, 100% pass), новый executable `ProjectVFluidCATests` в
`tests/CMakeLists.txt:706-749`. Self-contained CPU tests, hand-construct VoxelWorld через `MakeFluidCATestWorld` (без
AppState / VoxelLab preset dependency). Compiles `VoxelWorld.cpp` + `RuntimeDiagnostics.cpp` + `VulkanResult.cpp` в test
target (same pattern as `ProjectVCFrustumCullingTests`).

**«Вода не течёт вниз» — был CRITICAL BUG, не expected behavior.** Commit loop silently dropped all fall commits в
production VoxelLab. Fix: добавить `world.min` offset. Теперь падает.

**«Respawn за платформой» — был spread rule, теперь by design.** Удаление spread rule (потом восстановление без support
check) — оператор хочет horizontal spread. «Respawn за платформой» — feature, не bug, теперь documented в decisions.md
§30.

**Spread «swap» bug (2026-06-13 follow-up).** Initial spread rule использовал `world.voxels[neighbour] == Air` для
target check. Два adjacent source cells могли оба «успешно» spread (last write wins), один fluid voxel теряется.
Per-tick count drop. **Fix**: target check использует `next[neighbour] == Air` (новое состояние, не snapshot). Когда
первый source claim destination, второй отклонён. То же fix применён к fall rule. `claimed[]` per-tick bool array (~10
KB) belt-and-suspenders. Verified by `TestFluidCASpreadIsDeterministic`.

**Fall-through-floor РЕВЁРТНУТ (2026-06-13 follow-up #3).** Оператор уточнил: «платформа исчезает из-за воды» — это
нежелательно, fall-through rule эродировала платформу. **Revert**: fall rule проходит только через `Air` (как до
follow-up). Вода на платформе (y=1) не падает через платформу — она распространяется через spread rule на Air-соседей (
за пределы платформы), и оттуда стекает через fall. **Платформа остаётся целой**. Tests:
`TestFluidCAFluidDoesNotFallThroughPlatform` (платформа cell MUST stay FloorWhite),
`TestFluidCAColumnDrainsViaSpreadPlatformStaysIntact` (все 16 floor cells at y=0 = FloorWhite).

**30Hz tick + spread-via-Floor (2026-06-13 follow-up #2).** Оператор: (1) вода моргает слишком быстро; (2) вода не
стекает с платформы, остаётся на ней. **Fix 1**: CA tick rate 60Hz → 30Hz (`src/app/main.cpp:656`). **Fix 2** (
reverted): fall rule проходил через FloorWhite/FloorGray, но эродировал платформу — откатили (см. выше). **Fix 3** (
active): `ShouldEmitVoxelFace` в `voxel_mesh.comp:202-209` — fluid emit faces против ВСЕХ материалов. Каждый water cube
имеет 5-6 видимых граней (раньше 3-4, оператор жаловался «у воды одной грани куба нет»). **Fix 4** (active): `cullMode`
в `VulkanGraphicsPipeline.cpp:1696` — `VK_CULL_MODE_NONE`, back-face culling отключён.

**Cross-refs:** `agent/decisions.md §30` (полный audit + решения), `agent/active-sessions.md`
session-2026-06-13-hardcore-perf-r0 (Phase 2 sub-task), `src/voxel/VoxelWorld.hpp:154-191` (determinism contract),
`src/voxel/VoxelWorld.cpp:1284-1434` (refactored `UpdateFluidCA`), `src/app/main.cpp:656` (30Hz CA throttle),
`tests/FluidCATests.cpp` (16 sub-tests).

### 12.1. CA pause + timeScale + V-sync fixes (`2026-06-14`)

Три operator reports в одной сессии: «vsync слетает при постановке блока», «вода растекается на паузе»,
«замедление/ускорение не действует», «слишком быстро льётся». Root cause'ы в трёх разных подсистемах, но все три связаны
с `simulation->paused` / `timeScale` chain и swapchain state.

**V-sync bug (`src/render/vulkan/VulkanSwapchain.cpp:148-180`):** `ChoosePresentMode` имел condition
`if (g_preferredPresentMode != FIFO)`, иначе MAILBOX-first default chain. На Linux/Wayland VRR surface: V cycle
`FIFO → IMMEDIATE → MAILBOX → FIFO` — третий press (`MAILBOX → FIFO`) silently возвращал MAILBOX (else-branch), не FIFO.
Оператор воспринимал как «vsync слетает при постановке блока», но subagent audit подтвердил: `SetVoxelMaterial` → 0
ссылок на swapchain. **«После блока» — ложная корреляция**: swapchain re-create по `vkAcquireNextImageKHR` →
`OUT_OF_DATE` или window events re-выбирал MAILBOX. **Fix**: убрал `if (!= FIFO)` branch. New:
`if (IMMEDIATE || MAILBOX) → PickBestAvailablePresentMode`; иначе explicit FIFO. V → FIFO теперь работает.

**CA tick перенесён в `UpdateApp`:** `src/app/main.cpp:626-643` (старый wall-clock throttle на 30Hz) удалён. CA tick в
`src/app/AppUpdate.cpp:693-733` после physics accumulator block, перед camera-look-input. `SimulationState` (
`src/core/Types.hpp:1348-1382`) получил `fluidTickRateHz = 20.0f` (новый default, был 30) +
`fluidAccumulatorSeconds = 0.0f`. CA throttle использует **отдельный** accumulator + `1 / fluidTickRateHz` interval,
scaled by уже-scaled `frameDeltaSeconds`. `effectivePaused` gate: на паузе accumulator **zeroed**.

**TimeScale tick multiplication работает корректно:** `timeScale = 0.5` → 10 CA ticks/sec. `timeScale = 0.0` → 0 ticks (
water doesn't move). `timeScale = 2.0` → 40 ticks/sec. `timeScale = 4.0` (clamp) → 80 ticks/sec. *
*`timeScale = 0.0 + frameStep = 0 ticks`** (CA не имеет physics-style force-override; visual-only, no inspector-tooling
requirement).

**Multiple ticks per frame allowed** (no `kMaxSimulationStepsPerFrame` cap). Physics имеет cap 5 ticks/frame для
anti-spiral; CA нет — pure visual, deterministic iteration, no failure mode при under-simulation.

**Tests:** 8 новых sub-tests (24 total, 100% pass). `TestFluidCAFluidDoesNotMoveOnPause`,
`TestFluidCAFluidMovesOnUnpause`, `TestFluidCAFluidRateRespectsTimeScale`, `TestFluidCAFluidRateAboveBase`,
`TestFluidCAFluidRateAtDefault`, `TestFluidCAFluidTimeScaleZeroStops`, `TestFluidCAFluidFrameStepWithTimeScaleZero`,
`TestFluidCAFluidRateConfigurable`. Helper `TickFluidCA(SimulationState &, VoxelWorld &, float, int)` inline-зеркало
production throttle.

**Lessons learned (добавить к §10):**

- **«Subagent для root-cause верификации» — must.** V-sync bug audit (subagent #1) нашёл, что `SetVoxelMaterial` → 0
  ссылок на swapchain, **спас от попытки фиксить несуществующее**. Без subagent я бы «починил» бы то, что не было
  сломано (race condition, deadlock, etc.) и не нашёл бы реальный bug в `ChoosePresentMode` else-branch. Lesson: **когда
  оператор жалуется на неожиданное поведение, subagent audit на «что вообще может это вызвать» — must**.
- **«Default + override» logic в `ChoosePresentMode` — classic subtle bug.** Условие `if (override != default)` — звучит
  правильно, но означает «default нельзя выбрать, override-only». Fix: «if (override == X) return X, else if (
  override == Y) return Y, else default» — explicit per-mode branch. **Rule: default == override-значение, иначе default
  unreachable**. Будущие «default + override» в engine — explicit enum-dispatch, не «!= default» pattern.
- **«Visual-only simulation tickrate» vs «physics simulation tickrate» — разные cap rules.** Physics: cap шагов
  anti-spiral. CA: no cap, поскольку visual + deterministic. **Rule: при добавлении нового simulation subsystem —
  classify «physics (capped)» vs «visual (uncapped)» в дизайне**.
- **«[object] volume, [object] rate, [object] accumulator» — pattern, не magic numbers.** Старый CA throttle: hardcoded
  `30u` + `SDL_GetPerformanceCounter()`. Новый: `SimulationState::fluidTickRateHz` +
  `SimulationState::fluidAccumulatorSeconds`. **Rule: simulation knobs live in `SimulationState`, не в call site**.
  Future physics/CA tweaks — `SimulationState` field, не inline literal.

**Cross-refs:** `agent/decisions.md §30.1` (полный plan + обоснования),
`src/render/vulkan/VulkanSwapchain.cpp:148-180` (V-sync fix), `src/core/Types.hpp:1348-1382` (new fields),
`src/app/main.cpp:626-643` (удалён CA throttle), `src/app/AppUpdate.cpp:693-733` (новый CA tick block),
`tests/FluidCATests.cpp:763-1145` (8 новых sub-tests + helper).

### 12.2. V hotkey auto-detect cycle + libc++ warning suppression + HUD line (`2026-06-14`)

Два operator reports в одной сессии: «у кнопки V 4 переключения — не понимаю, какое из них что делает» +
`clang: warning: argument unused during compilation: '-stdlib=libc++'`. Root cause'ы: hardcoded 3-state cycle +
`add_compile_options(-stdlib=libc++)` warning + missing HUD feedback.

**V hotkey auto-detect cycle (`src/render/vulkan/VulkanSwapchain.hpp:69-148`):** Pre-fix cycle —
`FIFO → IMMEDIATE → MAILBOX → FIFO` (decisions.md §30 2026-06-13 follow-up). На Linux/Wayland без VRR surface не
expose'ит IMMEDIATE → `PickBestAvailablePresentMode` silently falls back IMMEDIATE → MAILBOX. Operator видит 4 press'а в
log'е, но только 2 unique runtime mode'а (FIFO, MAILBOX). «Press V и ничего не меняется» — user-visible. **Fix**:
`BuildPresentModeCycle(support.presentModes)` walks priority list `{FIFO, MAILBOX, IMMEDIATE}` and keeps только
surface-supported modes. Cycle length = number of physically supported modes. `CyclePreferredPresentMode` walks cycle по
индексу, wraps. Header-only: `g_active` + `g_cycle` — `inline` C++17 variables в header, `CyclePreferredPresentMode` /
`BuildPresentModeCycle` / accessors — `inline` functions. Test target `ProjectVPresentModeTests` header-only dependency,
no `.cpp` link.

**HUD line for VSync (`src/debug/DebugHud.cpp:553-577`):** `VSync <mode> (<index>/<size>)` — например `VSync FIFO (1/2)`
на Linux/Wayland, `VSync MAILBOX (2/3)` после V press. Видно сразу: текущий mode + cycle position. Uses header-only
inline accessors.

**V hotkey log message (`src/app/main.cpp:534-578`):** `CycleVsync: <mode> [cycle <idx>/<size>]` — например
`CycleVsync: MAILBOX (tear-free, uncapped) [cycle 2/2]`.

**libc++ warning — kept + suppressed (`CMakeLists.txt:117-150`):** Initial plan: remove
`add_compile_options(-stdlib=libc++)` (CMake's `CMAKE_CXX_STDLIB` already propagates). **Failed**: removing produces
`undefined symbol: std::__1::__fs::filesystem::path` и `undefined symbol: fmt::v12::vformat` link errors в
`external/fastgltf` и `external/fmt`. Root cause: `add_subdirectory` external subdirs не inherit
`projectv_build_options`, **не inherit `CMAKE_CXX_STDLIB`** in their compile commands (CMake 4.3.3 + Ninja + Clang 22
behavior). Без explicit `add_compile_options(-stdlib=libc++)` они компилируются с libstdc++ (system default), генерируют
`std::__cxx11::fs::path` symbols, не match с нашими `std::__1::__fs::path`. **Fix**: keep
`add_compile_options(-stdlib=libc++)` (cross-target ABI), suppress warning via
`add_compile_options(-Wno-unused-command-line-argument)`. Per `AGENTS.md §7.2.7` suppression acceptable: one flag, one
Clang toolchain artifact, well-commented.

**Lessons learned (добавить к §10):**

- **«Auto-detect hardware capabilities > hardcode cycle»** — universal rule. Per
  `legacy/docs/philosophy/01_foundation/05_decision-making.md` (data-driven, не hardcoded): hardware-dependent
  capabilities (display modes, vertex formats, MSAA samples, image format priorities) **всегда auto-detect at startup,
  не hardcode cycle**. Hardcoded cycle — implicit assumption о host'е, который fails silently. **Rule для future**:
  cycle, priority lists, format selection — all auto-detected, не hardcoded.
- **«Inline variables + inline functions для runtime state observables»** — header-only API pattern. Per
  `legacy/docs/philosophy/01_foundation/02_arch-design.md` (decouple): runtime state наблюдаем из HUD/test/HMR. **Inline
  variables** в header: linker dedups per-TU, no ODR violation. **Inline functions** для pure accessors/mutators:
  header-only dependency для consumers, no `.cpp` link. **Trade-off**: header grows, но consumers save `.cpp` link dep.
  **Rule**: small runtime state (cycle, mode, current value) — header-only inline. Large state (chunk meshes, render
  targets) — `.cpp` + extern accessor.
- **«Hardware-dependent toolchain flags: don't remove, suppress false-positive»** — libc++ ABI constraint. Per
  `legacy/docs/standards/cpp/01_coding-style.md` (no magic): `add_compile_options(-stdlib=libc++)` — explicit
  cross-target ABI flag. Removing breaks external subdirs (verified 2026-06-14). Clang warning — toolchain artifact, не
  code defect. **Rule**: suppress warnings only для **specific toolchain artifacts** (cross-cutting, well-commented,
  scoped). Глушить варнинги «потому что мешают» — нет.
- **«Log vs HUD для runtime-togglable state»** — UX hierarchy. Per
  `legacy/docs/philosophy/01_foundation/06_execution-style.md` (visible feedback): log — post-mortem, HUD — live.
  Runtime-togglable state (vsync mode, fluid rate, timeScale) — live, должен быть в HUD. **Rule**: если оператор может
  press hotkey для toggle, state должен быть виден в HUD (не только в логе).

**Cross-refs:** `agent/decisions.md §30.2` (V hotkey auto-detect + libc++ fix plan),
`src/render/vulkan/VulkanSwapchain.hpp:69-148` (header-only API), `src/render/vulkan/VulkanSwapchain.cpp:262-275` (call
to `BuildPresentModeCycle`), `src/app/main.cpp:534-578` (V hotkey log), `src/debug/DebugHud.cpp:553-577` (HUD line),
`CMakeLists.txt:117-150` (libc++ flag + suppression), `tests/PresentModeTests.cpp` (9 sub-tests, new file),
`tests/CMakeLists.txt:771-810` (new test target).

### 12.3. V hotkey cycle walk across `RecreateSwapchain` (`2026-06-14` evening)

Оператор: «нажимаю на V, ничего не меняется» — 10 identical log lines `IMMEDIATE [cycle 2/2]`. Subagent analysis: cycle
`[FIFO, IMMEDIATE]`, `g_active = FIFO` initial. Press V → `CyclePreferredPresentMode` advances to `IMMEDIATE` → log
`IMMEDIATE` → `RecreateSwapchain` → `CreateOrRecreateSwapchain` → **`BuildPresentModeCycle` resets `g_active = FIFO`**.
Next press: `g_active = FIFO` → advance to `IMMEDIATE` → log `IMMEDIATE` → reset. **Cycle appears stuck**.

**Root cause (`src/render/vulkan/VulkanSwapchain.hpp:180-220` pre-fix):** `BuildPresentModeCycle` unconditionally set
`g_active = g_cycle.front()` (FIFO) on every call. Зовётся из `CreateOrRecreateSwapchain` (
`src/render/vulkan/VulkanSwapchain.cpp:262-275`), который зовётся из `RecreateSwapchain`, который зовётся из V hotkey
handler (`src/app/main.cpp:566-571`) **after** `CyclePreferredPresentMode`. Sequence: V press → cycle advances → log →
`RecreateSwapchain` → reset to FIFO. Self-defeating cycle.

**Fix (`2026-06-14` evening):** `BuildPresentModeCycle` теперь **preserves `g_active`** across rebuilds. Capture
`previousActive` before rebuild; if it's still in new cycle, keep it; else (display hot-swap) fall back to
`g_cycle.front()`. V hotkey walks correctly: V press → cycle advances → `RecreateSwapchain` preserves `g_active` → next
press advances from preserved state.

**Display hot-swap correctness:** If host drops a previously-supported mode (e.g. external monitor unplugged, new
surface only exposes FIFO), `g_active` is not in new cycle → fall back to highest-priority supported (FIFO by priority).
Operator sees new mode в HUD + log, can press V для cycle к next mode (if any).

**Tests (3 new sub-tests, `tests/PresentModeTests.cpp:281-415`):** `TestPresentModeCyclePreservesActiveAcrossRebuild`,
`TestPresentModeCycleFallsBackWhenActiveDropped`, `TestPresentModeCycleWalksAcrossRecreates` (operator's actual
scenario, alternating FIFO ↔ IMMEDIATE).

**Test order dependence + explicit reset pattern:** Pre-existing tests assumed pre-fix behavior of unconditional FIFO
reset, which masked test-order dependencies. Post-fix, tests must **explicitly reset** to known state before exercising.
Pattern: `(void)BuildPresentModeCycle({VK_PRESENT_MODE_FIFO_KHR});` as first line of any test that wants
`g_active = FIFO`. The single-mode cycle forces fallback to FIFO regardless of previous test's final state.

**Lessons learned (добавить к §10):**

- **«Capture previous state, restore on rebuild» > «unconditional reset»** — universal rebuild pattern. Per
  `legacy/docs/philosophy/01_foundation/05_decision-making.md` (data-driven, не hardcoded): rebuild **preserves
  invariants** (operator's choice), не **enforce defaults**. Display change — real event, needs real decision (fall back
  gracefully), не silent reset. **Rule для future**: rebuild functions (re-construct runtime state from external source)
  capture previous state, restore на best-effort basis. Default fall back только когда previous state invalid.
- **«Test interaction, not just function»** — gap в initial test coverage. `BuildPresentModeCycle` +
  `CyclePreferredPresentMode` tested в isolation, но **V hotkey's call sequence** (`CyclePreferredPresentMode` →
  `RecreateSwapchain` → `BuildPresentModeCycle`) not tested. Bug only manifested в this sequence. **Rule**: при
  добавлении новой stateful функции, test all callers that mutate the same state, не just the function в isolation.
- **«Test order independence via explicit reset»** — defensive pattern. Inline variables + global state → tests must
  explicitly reset to known state. `BuildPresentModeCycle({FIFO})` forces fallback к FIFO because previous `g_active` is
  not in `{FIFO}`. Cleaner than per-test fixtures (lighter-weight, no extra setup). **Rule**: tests of inline-variable
  global state start with `(void)ResetFunction({initial_state});`.
- **«Self-defeating state machine»** — anti-pattern. Cycle advance + reset на same trigger = no-op. **Detection**:
  function A advances state, function B (called by A's caller) resets state, no visible change. **Fix**: refactor B to
  not reset (preserve previous), or A to not call B. **Rule**: state-machine transitions should be **monotonic** (no
  rollback unless explicit operator action).

**Cross-refs:** `agent/decisions.md §30.3` (preserve-`g_active` plan), `src/render/vulkan/VulkanSwapchain.hpp:180-220` (
preserve logic), `tests/PresentModeTests.cpp:281-415` (3 new sub-tests + explicit-reset pattern).

