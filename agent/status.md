# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-06-15` — service files compress+archive (см. `agent/ARCHIVE-INDEX.md`).
Подробный per-session history (§5-§20, 1000+ строк) вынесен в
`legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md`. Section
numbering preserved.

Updated: `2026-06-15` — **Windows build verification r0 (session-2026-06-15T10-25Z-windows-build-verification-r0, см. §24)**. 5 atomic-commits landed: libc++/Windows-clang-cl gating fix (P0-1..P0-4) + F5 hot-reload CMake-injected path (P0-5) + Tracy UI split to standalone build (P0-6) + RepoRoot extract + Windows LookDev smoke parity (P1-2/P1-3) + docs/cleanup + deinit 5 unwired submodules 62M. Linux baseline preserved (ctest 14/14, smoke 6/6).

Updated: `2026-06-14` — 4 КТ-документа сконвертированы в LaTeX (session-2026-06-13-kt-latex-r0, см. §19 в archive). 5 PDF в `docs/tex/`.
Updated: `2026-06-14` — V-sync FIFO bug + CA pause/timeScale + 20Hz default (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up, см. `decisions.md §30.1`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.1` → archive). 3 fixes. 8 new CA sub-tests (24 total, 100% pass). Build green, ctest 13/13, smoke clean.
Updated: `2026-06-14` — V hotkey auto-detect cycle + libc++ warning + HUD line (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up #2, см. `decisions.md §30.2`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.2` → archive). 4 fixes. 9 new present-mode sub-tests, 100% pass. Build green, ctest 14/14, smoke clean.
Updated: `2026-06-14` — V hotkey cycle walk fix (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up #3, см. `decisions.md §30.3`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.3` → archive). 1 fix. 3 new sub-tests (12 total). Build green, ctest 14/14, smoke clean.
Updated: `2026-06-14` — Release presets r0 (session-2026-06-14-release-presets-r0, см. §21). 8 CMakePresets, root `CMakeLists.txt` +Release-блок, `README_NEW.md` создан, `agent/decisions.md §4` +Release policy. linux-clang-release: configure 54s, build 137/137 green, ctest 13/13, smoke 6/6, ELF 19MB (-73% vs debug).
Updated: `2026-06-14` — Build config audit r0 (session-2026-06-14T11-29Z-build-config-audit-r0, см. §22). 5 buildPresets (3 debug × 17, 2 release × 15, 1 smoke). `linux-clang-debug-tracy-profiler.PROJECTV_BUILD_TRACY_PROFILER: ON→OFF`. `linux-clang-debug-ci/` удалён (194M). `linux-clang-debug-tracy-profiler/` сохранён. linux-clang-release ctest 14/14.

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- Active sub-plan: `TODO.md` Tier 0..5 (Hardcore perf r0, Phase 1+ pending operator approval after Phase 0 commit, см. `agent/memory.md §11`).
- **Most recent closed sessions** (one-line summary, full detail в archive `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md`):
  - `2026-06-10` — P0.2 shadow sampler `magFilter` `NEAREST → LINEAR` re-applied after lost-and-found incident. Build green, ctest 1/1, smoke 4/4 на `VoxelLab`.
  - `2026-06-10` — P0.3 per-corner AO landed (per `agent/memory.md §10.11`). Lesson learned: incremental `cmake --build` не копирует свежие `.spv` в `bin/` пока `ProjectV` ELF up-to-date — нужен явный `cp` или `--target ProjectV`.
  - `2026-06-09` — Shadow-quality pass closed (six code fixes в `voxel.frag` / `VulkanGraphicsPipeline.cpp` / `SceneResources.hpp`). Полный diff archived в `legacy/docs/archive/agent_memory_§10_shadow_audit_2026-06-09.md`.
  - `2026-06-09` — Swapchain semaphore reuse fix (per-frame acquire + per-image submit pattern). 0 VUIDs vs 20 ранее.
  - `2026-06-09` — Multiplatform dev baseline opened (Linux build green, ctest 1/1, 14 dev-tools installed; подробности в `agent/memory.md §5-§8`).
- `walk` controller tuning работает on Tier 5 follow-up; replay-first diagnosis already covered by `PhysicsWalkDebugInfo` + `TracyPlot`.
- Linux dev baseline: clang 22.1.6 native + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan 1.4.350, см. `agent/memory.md §5-§9`.

## 2. Nearest Gap

- The old P0 process reminders are no longer left open in `TODO.md`: replay-first controller diagnosis, developer-only runtime smoke, and the current warning-cleanup closure are already treated as established baseline, not as unfinished work.
- Runtime smoke policy changed: `ProjectVRuntimeSmoke` is now a targeted lifecycle/Vulkan check, not a default DoD step
  for every lighting/material/doc change.
- Tracy-profiler build policy changed: `build/windows-clang-debug-tracy-profiler` is no longer part of routine
  verification. Use it only for explicit Tracy/profiling work or when the user asks for that build specifically.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- The next concrete mainline feature gap is no longer shadow plumbing, shadow tuning, direct-light BRDF, post-BRDF capture refresh, ambient/environment fill, fixed preset grading, first scene-key auto exposure, missing demo-scene opaque anchors, CSM split planning, the first CSM render hookup, basic CSM texel snapping, basic per-cascade coverage diagnostics, the first stable sphere-fit split-edge follow-up, the first shader-side split transition blend follow-up, the first cascade-specific caster coverage follow-up, or the visible-scene receiver-range fix that aligned cascades with current chunk visibility. The remaining `10.5.1` caveat is that exposure is not histogram/adaptive yet; adding that should wait for an HDR/luminance path rather than being faked in the shader.
- The current shadow limitation is now explicit and accepted: glass does not cast shadows in the mainline sun-shadow
  pass. `Fluid` casts as an opaque shadow-map caster; physical/tinted glass shadows remain future R&D.
- The new contact-shadow and `AOCC` baselines are intentionally bounded and local: both use short forward-shader voxel
  DDA traces against the existing packed world payload, not separate screen-space passes and not replacements for the
  current CSM layer. The new local point light is also bounded: authored, inverse-square, and shadowed by a short
  opaque-only voxel DDA term until a separate local shadow-map/cubemap step is worth adding.

## 3. Next Steps

1. For any further sun/contact shadow work, keep the new stricter close-out rule: inspected runtime captures are required,
   not sidecar numbers alone. Use `FINAL` + `SHDW` at minimum, and include `CSM` / `CTSH` when those paths are involved.
2. The local occlusion and bounded local-light-shadow slices can now pause unless live captures show a specific defect;
   the next truly different `10.5.3` layer is real local shadow-map/cubemap infrastructure, while full `SSAO/GTAO`
   should still wait for a real depth/normal screen-space path.
3. Do not reintroduce fake glass shadow surrogates unless there is a real, separately scoped transparent-shadow path.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe; smoke should be sequential and only when it is a
  relevant lifecycle/Vulkan check.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
- **`2026-06-10` destructive-git-checkout incident** (см. `agent/memory.md §10.11`). Working rule: перед `git checkout -- .` — `cp` или `git stash push -m "KEEP_..."`. Pattern `git checkout -- .` + `git stash drop` **destructive** для uncommitted work предыдущих сессий.
- `agent/active-sessions.md` имеет длинный список closed sessions в «Активные» — при apply `§8.1` retroactively (per `docs(agent): compress+archive`), записи переносятся в «Закрытые» + older закрытые в archive.
- При работе с `agent/active-sessions.md` или `agent/status.md` соблюдать `AGENTS.md §7.2.8` (shared infra, edit **только своей** записи / APPEND-only).

## §21. Release presets r0 — `session-2026-06-14-release-presets-r0` (open, build green)

**Per operator request «release build для linux и windows, чтобы увидеть готовый продукт».** Консервативная политика, без `-ffast-math` (Fluid CA determinism + TAA YCoCg clamp), без `-march=native` (portability между CPU). 5 файлов изменено, 0 правок в `src/`, `tests/`, `external/`, `legacy/`, `docs/`, `tools/`.

**Файлы (5):**
| Файл | Что |
|------|-----|
| `CMakePresets.json` | +8 presets: `linux-clang-release-base` (hidden) + `linux-clang-release` + `linux-clang-release-build` + `linux-clang-release-tests` + симметричные `windows-clang-release-*`. JSON validated через `cmake --list-presets=configure` (8 configure + 6 build + 5 test entries — старые debug-presets не сломаны). |
| root `CMakeLists.txt` | +1 блок `if (CMAKE_BUILD_TYPE STREQUAL "Release")` после `add_compile_options(-stdlib=libc++)`. Compile: `-O3 -flto=thin -DNDEBUG -ffunction-sections -fdata-sections -fno-finite-math-only`. Link: `-flto=thin -Wl,--gc-sections`. |
| `README_NEW.md` (NEW) | создан с нуля (файл отсутствовал, `agent/memory.md §4` объявляет каноническим root-facing overview). Содержит Quickstart (Linux + Windows Debug), Release build секцию с командами, ссылку на `agent/decisions.md §4` за полной политикой. |
| `agent/decisions.md §4` | +подсекция «Release presets (2026-06-14, conservative policy)» с explicit обязательными + запрещёнными флагами, ожидаемым эффектом (25-40 MB ELF, +1.5-2.5× FPS), и обоснованием через operator request. |
| `agent/active-sessions.md` | append-only `session-2026-06-14T10:53Z-release-presets-r0` запись. |

**Build verification (2026-06-14, `linux-clang-release`):**
- `cmake --preset linux-clang-release`: configure green (54.1s, `ProjectV options: validation=OFF, tracy=OFF, renderdoc=OFF, tracy-profiler=OFF, imgui=OFF`).
- `cmake --build build/linux-clang-release --target all --parallel 8`: 137/137 targets green, 0 errors, 0 new warnings (только pre-existing miniaudio CMP0148 deprecation).
- `ctest --test-dir build/linux-clang-release --output-on-failure`: **100% tests passed, 0 tests failed out of 13**. ProjectVTests (157 sub-tests) — **0.03s** на release vs ~1.4s на debug (O3+LTO+NDEBUG payoff).
- `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-release --capture-dir build/linux-clang-release/lookdev-captures/2026-06-14-release-v1`: **6/6 captures** (FINAL/SHDW/CSM/CTSH/AOCC/LOCL), exit 0, smoke 1s wall clock.

**Метрики release vs debug:**
- ELF: **19 MB** (release) vs **72 MB** (debug) — **-73%** (ThinLTO + gc-sections + dead code removal).
- ctest wall clock: **0.06s** (release) vs ~1.5s (debug) — **-96%**.
- Render-pass timings на release (VoxelLab reference shot, single frame): shadow 30 µs, graphics 76 µs, TAA 3 µs.

**Smoke captures под `build/linux-clang-release/lookdev-captures/2026-06-14-release-v1/`:**
- 6 × `ProjectV-VoxelLab-<ts>-000{1..6}.bmp` (5.88 MB каждый, matches VoxelLab baseline из `agent/memory.md §1`).
- 6 × `.txt` sidecar, `taa_*` / `shadow_*` / `exposure_*` keys populated, `scene_preset=VoxelLab`, `transparent_shadow_policy=GLASS_IGNORED_FLUID_CASTS`.

**Scope discipline (per `AGENTS.md §7.2.6`):** **0 пересечений** с 6 активными сессиями (`render-race-debug` = read-only, `camera-fullscreen-jump-fix` = `src/app/main.cpp`, `kt-latex-r0` = `docs/tex/`, `hardcore-perf-r0` = `src/core/Math.hpp` + `src/render/SceneResources.*`, `problems-cleanup-v2`/`v1` = warning cleanup в `src/asset`+`src/audio`+`src/debug`+`src/ecs`+`src/physics`+`src/render/vulkan/*`+`src/app/AppUpdate.cpp`). Pre-commit: `git diff CMakePresets.json CMakeLists.txt README_NEW.md` показывает только мои правки.

**Safety net:** `/tmp/before_release_presets_2026-06-14T1053.patch` (10 KB, captures все 5 uncommitted чужих файлов).

**Windows:** presets готовы и JSON-valid, `cmake --list-presets` подтверждает `windows-clang-release` + `windows-clang-release-build` + `windows-clang-release-tests`. Оператор собирает на Windows-хосте: `cmake --preset windows-clang-release && cmake --build ... --preset windows-clang-release-build && ctest --test-dir build/windows-clang-release --output-on-failure`.

**Commit landed (`2026-06-14`, per operator «Коммить, разрешаю»):** SHA `6fe9201` — `build(cmake): conservative Release presets (linux-clang-release, windows-clang-release)`. Commit body per `§7.2.5` contract; full text in `git log -1 6fe9201`.

**Commit plan (final, 1 commit per `AGENTS.md §7.2.4` + `§7.2.5`):**
```
build(cmake): conservative Release presets (linux-clang-release, windows-clang-release)

Adds CMAKE_BUILD_TYPE=Release presets for both platforms
with -O3 -flto=thin -NDEBUG -ffunction-sections -fdata-
sections -fno-finite-math-only. Deliberately omits
-ffast-math (breaks Fluid CA determinism + TAA YCoCg
clamp per `decisions.md §4`) and -march=native (binaries
must remain portable between CPUs).

Validation / Tracy / RenderDoc markers / Benchmarks all
OFF in release (PROJECTV_ENABLE_*). BUILD_TESTING=ON
preserves the ctest 13/13 baseline.

Root CMakeLists.txt gets a CMAKE_BUILD_TYPE=Release
block with the compile+link policy. CMakePresets.json
adds 8 new presets (configure/build/test × 2 OS, with
hidden *-base parents).

README_NEW.md created (file did not exist; per
`agent/memory.md §4` it is the canonical root-facing
overview). Includes Quickstart (Debug + Release) and
links to `agent/decisions.md §4` for the full policy.

agent/decisions.md §4 captures the conservative
release-flag policy as a permanent invariant.

Scope: only CMakePresets.json + root CMakeLists.txt +
README_NEW.md + agent/{decisions,active-sessions,status}.md
+ safety-net patch in /tmp/. No src/, tests/,
external/, legacy/, docs/ changes (per TODO.md §9 +
active-sessions scope discipline §7.2.6).

Build (linux-clang-release): configure green, all 137
targets built, ctest 13/13 passed in 0.06s, smoke 6/6
captures clean, ELF 19MB (-73% vs 72MB debug). Windows
presets validated via `cmake --list-presets`; the
operator runs the actual Windows build on the Windows
host.

Refs: agent/decisions.md §4, agent/memory.md §6,
       legacy/docs/standards/cmake/04_advanced-optimization.md
```

**Сессия ещё `open`** (status: open в `agent/active-sessions.md`) — жду команды оператора «закоммить» per §7.2.4 + §8.1.

---

## §22. Build config audit r0 — `session-2026-06-14-build-config-audit-r0` (open, build green)

**Per operator «проверить все конфиги билдов на работоспособность и целесообразность».** Read-only audit 4 build-деревьев + 12 buildPresets на Linux-хосте. Findings + Tier 1 fix в одном коммите.

**Findings (read-only phase):**

| # | Severity | Issue |
|---|----------|-------|
| A1 | high | 4 buildPresets имели `targets: [ProjectV, ProjectVTests]`. ctest регистрирует 14 executables, на чистом clone 11+ тестов «cannot find executable» |
| A2 | high | `linux-clang-debug/bin/` уже частично сломан (ProjectVTests + ProjectVAssetTests missing из 13) — implicit A1 fix |
| A3 | medium | `linux-clang-debug-ci/` (194M) — configured, not built, `CMAKE_CXX_COMPILER:UNINITIALIZED` в cache |
| A4 | medium | `linux-clang-debug-tracy-profiler/` (190M) — dead: Tracy UI build fail на Linux/glibc (`agent/memory.md §9`) + nlohmann_json target collision (CMP0002) при re-configure |
| B1-B3 | low | Нет Linux smoke preset, нет release-ci, display name inconsistency — вне scope (operator не хочет) |
| C1 | judgment | Benchmarks в release (PROJECTV_ENABLE_BENCHMARKS=OFF) — вне scope (operator не хочет) |
| C4 | future | CPack/install — отдельная подзадача per `decisions.md §4` |

**Operator decisions (применены в этом slice):**
- **Tier 1 fix**: A1 (build-preset targets), A2 (implicit), A3 (ci tree), A4 (tracy fix)
- **«Удалить ci»**: `build/linux-clang-debug-ci/` удалён (194M, exit 0)
- **«Оставить tracy»**: `build/linux-clang-debug-tracy-profiler/` сохранён; re-configure fix через `PROJECTV_BUILD_TRACY_PROFILER=OFF` в Linux-пресете (root cause: `external/tracy/profiler/CMakeLists.txt:245` ссылается на `nlohmann_json::nlohmann_json` → conflict с root `FetchContent_MakeAvailable(nlohmann_json)`)
- **«Только Tier 1»**: B1-B3, C1 — отложены

**Files (3 modified, 0 created, +1 fs op):**
| Файл | Что |
|------|-----|
| `CMakePresets.json` | 5 buildPresets обновлены (targets: 3 debug × 17, 2 release × 15, smoke × 1 unchanged); `linux-clang-debug-tracy-profiler.PROJECTV_BUILD_TRACY_PROFILER: ON→OFF` (Linux Tracy UI не собирается) |
| `agent/active-sessions.md` | append-only `session-2026-06-14T11:29Z-build-config-audit-r0` (status: open) |
| `agent/decisions.md §4` | +2 подсекции: «Build preset target list invariant» + «`linux-clang-debug-tracy-profiler` Tracy UI fix» |
| `agent/status.md` | +эта §22 + Updated header |
| `build/linux-clang-debug-ci/` | **удалён** (rm -rf, 194M, exit 0) |

**Verification (`2026-06-14`):**
- `cmake --list-presets=build` — 6 buildPresets, JSON valid (3 debug × 17 targets, 2 release × 15, 1 smoke × 1)
- `linux-clang-release` configure: green, re-configure 0.6s
- `cmake --build --preset linux-clang-release-build` (включая 14-й test `ProjectVPresentModeTests`): 9/9 incremental green, **все 15 binaries на диске** (ProjectV + 14 test executables)
- `ctest --test-dir build/linux-clang-release`: **14/14 passed**, 0.07s (vs 13/13 + 1 missing до фикса)
- `linux-clang-debug-tracy-profiler` re-configure: **green** (после `BUILD_TRACY_PROFILER=OFF` fix), `ProjectV` собирается 75.5MB (Tracy instrumentation), 0 ctest (BUILD_TESTING=OFF), 0 Tracy UI
- `linux-clang-debug` — untouched, всё ещё работает (ProjectV 75.5MB, ctest 14/14)
- Disk: 2.1G → 2.6G total (debug + tracy + cpm выросли от re-configures; -194M от удаления ci; net +500M из-за пере-выкачки CPM source в re-configures)

**Target counts финальные:**
| Build preset | targets | hasPresentMode | has benchmarks |
|--------------|---------|----------------|----------------|
| `windows-clang-debug-build` | 17 | ✓ | ✓ |
| `windows-clang-debug-smoke` | 1 | ✗ | ✗ |
| `windows-clang-debug-ci-build` | 17 | ✓ | ✓ |
| `linux-clang-debug-build` | 17 | ✓ | ✓ |
| `windows-clang-release-build` | 15 | ✓ | ✗ |
| `linux-clang-release-build` | 15 | ✓ | ✗ |

**Scope discipline (per `AGENTS.md §7.2.6`):** 0 пересечений с 6 активными сессиями. Мои 3 файла modified (CMakePresets.json + agent/active-sessions.md + agent/decisions.md + agent/status.md = 4) — все non-overlapping с чужими scope. Pre-commit: `git diff CMakePresets.json agent/decisions.md` показывает только мои правки. `agent/active-sessions.md` и `agent/status.md` имеют overlap с uncommitted notes предыдущей `session-2026-06-14-release-presets-r0` («ready to close» маркеры) — `git add -p` для selective staging.

**Safety net:** `/tmp/before_build_audit_20260614T112920Z.patch` (84 KB) — captures все 5 uncommitted dirty файлов от предыдущих сессий + мои новые правки. НЕ удаляю per §8.1.

**Build tree state финальный:**
| Tree | Disk | ProjectV | ctest | Status |
|------|------|----------|-------|--------|
| `linux-clang-debug` | 961M | 75.5MB | 14 | ✅ works |
| `linux-clang-debug-tracy-profiler` | 679M | 75.5MB | 0 | ✅ works (после BUILD_TRACY_PROFILER=OFF fix) |
| `linux-clang-release` | 512M | 19.7MB | 14 | ✅ works |
| ~~`linux-clang-debug-ci`~~ | ~~194M~~ | — | — | ❌ **removed** per operator «удалить ci» |

**Сессия ещё `open`** (status: open в `agent/active-sessions.md`) — жду команды оператора «закоммить» per §7.2.4 + §8.1.

## §25. Defense docs audit r0 — `session-2026-06-15T10-43Z-defense-docs-audit-r0` (closed в `bf2822f`)

**Per operator «перечитай то, что ты написал и глубоко проанализируй соответствие с кодом, приступай».** Найдено 23 расхождения между 12 defense-документами и реальным кодом + 1 F5/F6 hotkey conflict. Один fix(docs) commit `bf2822f`, 13 файлов, +460/-287 строк.

**Исправлены 23 галлюцинации:**

| # | Что | Реальность в коде |
|---|---|---|
| 1 | `VoxelChunk` struct shape | {Int3 min/maxExclusive, bool rebuildQueued, uint32_t nonAirVoxelCount}, **плоский** `voxels[]` в `VoxelWorld` |
| 2 | Frustum cull "8×" | Scalar C 3.7-3.9×, AVX2 2.5-2.7× (8× — future SoA target) |
| 3 | AVX2 inner loop "4 planes" | 8 AABBs × 6 planes (per-plane batch) |
| 4 | Visibility hash "splitmix64" | Custom XOR-fold (Knuth MMIX multipliers) + splitmix64-style avalanche |
| 5 | Ray-march "compute path" | STUB, только `fprintf` в stderr, Phase 7 follow-up |
| 6 | Walk "voxel solver авторитетный" | JPH::CharacterVirtual + voxel solver **augments** foot support (per decisions.md §6) |
| 7 | Edge grace "0.1 м" | `kWalkEdgeGraceFrames = 4` фрейма + `kWalkFootSupportEdgeGraceScore = 0.2f` |
| 8 | Fluid CA "4 cardinal directions" | 2 перпендикулярных попытка, **только 1 destination** пишется (count conservation per decisions.md §30) |
| 9 | Fluid CA "splitmix64 hash" | Teschner spatial hash `(x*73856093) ^ (y*19349663) ^ (z*83492791)` |
| 10 | VoxelLab "13 824 вокселей" | 27 чанков (chunk capacity 27×512 = 13 824, actual allocated 24×17×24 = 9 792) |
| 11 | Shell radius "5" | `VoxelLabShellConfig.radius = 6` |
| 12 | Contact shadow "16 max steps" | `kSunContactShadowMaxSteps = 12` |
| 13 | AOCC "3 × 4 = 12 reads" | Корректно (3-tap × 4 шага), но ранее был 5-tap, уточнён в комментариях |
| 14 | Audio "MP3/WAV/FLAC" | Только MP3 (case-insensitive ext check) |
| 15 | Snapshot magic "PVSNAP\0\0" | `"PVSNAP01"` (8 значащих байт) |
| 16 | AssetLoader "Load" | `LoadGlb(path, outError)` |
| 17 | ELF "72 MB debug" | 73 MB (verified `ls -lh 2026-06-15`) |
| 18 | BUG-005 "F5 VUID" | InputAction F5 cycle scene (НЕ F11 shader reload, после relocate) |
| 19 | BUG-005 ref "F5 hot-reload paths" | F11 (relocate) — в windows-build-verification session этот ref устарел |
| 20 | Visibility cache "3 memcpy" | "3 копируемых буфера (opaque/shadow/transparent), memcpy-логика на cache hit не верифицирована" |
| 21 | ctest "12/14" | 14/14 (текущий baseline, оба debug и release) |
| 22 | Fluid CA "1 tick per 3 frames" | Accumulator-based `fluidTickRateHz * timeScale`, multi-tick per frame allowed |
| 23 | C++26 "std::simd" в hot path | Не используется, заменено на C/AVX2 kernel в Tier 3 |

**Hotkey relocation (operator разрешил) в `src/app/main.cpp`:**
- `SDLK_F5` → `SDLK_F11` для shader reload (InputAction F5 `CycleScenePreset` теперь чисто своё).
- `SDLK_F6` → `SDLK_F12` для ray-march toggle (InputAction F6 `SaveWorldSnapshot` теперь чисто своё).
- F11/F12 InputAction walk bindings shadowed — приемлемо (walk debug не на demo path).
- Полный комментарий в main.cpp:545-585 объясняет relocation и TODO post-defense (route через formal InputAction enum когда `core/Types.hpp` стабилизируется).

**Build state:**
- `cmake --build build/linux-clang-debug` после правки main.cpp: clean, 0 errors.
- `ctest --test-dir build/linux-clang-debug`: **14/14 pass за 0.76s**, baseline preserved.

**Scope coordination:** session-2026-06-15T10-25Z-windows-build-verification-r0 multi-commit plan 1/5 тоже трогает `src/app/main.cpp` для P0-5 (F5 hot-reload hardcoded paths). Я коммичу РАНЬШЕ их commit 1; их cherry-pick тривиален (строки 545-559 main.cpp — единственный overlap).

## §24. Defense docs overhaul r0 — `session-2026-06-15T15-50Z-defense-docs-r0` (closed в `1db35ee`)

**Per operator «Требуется улучшить defense документы в docs: документ, который описывает каждый алгоритм в проекте, абсолютно за всё, плюс речь для пятерых, плюс каждому свою памятку. На тех пятерых следует разделить работу так, чтобы она была весомой, но простой к объяснению, а всё сложное мне оставить. Нас шестеро, я шестой.»** Защита 2026-06-15 10-минутный доклад + 5 мин Q&A, 6 человек в команде.

**Деливерабли (14 пунктов, +2715/-214 строк):**

| Категория | Файлы |
|---|---|
| **7 новых** | `docs/DefenseAlgorithms.md` (866 строк, 23 алгоритма) + `docs/DefenseBriefer_le1t.md` (351 строка: verbatim вступление/закрытие + Q&A-карта 30 вопросов) + 5 × `docs/DefenseBriefer_{1..5}.md` (153-174 строк каждый) + `docs/DefenseScript.md` (190 строк, 10-мин таймлайн) |
| **4 переработки** | `docs/DefenseSpeakerNotes.md` (новые темы, плейсхолдеры имён) + `docs/DefenseDemoScript.md` (новый таймлайн + hotkeys) + `docs/DefenseReport.md` (+§12 «Команда и вклад участников», обновлены 14 ctest suites) + `docs/DefenseFAQ.md` (+8 новых Q&A про команду, ray-march, fluid CA, hot reload) |
| **2 agent-файла** | `agent/active-sessions.md` (новая запись + перенос в «Закрытые сессии» в close-routine) + `agent/status.md` (эта секция §24) |

**Распределение ролей (10 мин):**
- le1t: вступление 2:00 + закрытие 0:30 + Q&A 5:00 (7:30 минут)
- Тиммейт 1: стек, билд, тесты, метрики (1:30)
- Тиммейт 2: voxel-мир, meshing, visibility cache (1:30)
- Тиммейт 3: тени, TAA, AOCC, ray-march (1:30)
- Тиммейт 4: физика, walk controller (1:30)
- Тиммейт 5: демо VoxelLab, ассеты, аудио (1:30)

**Стратегия:** тиммейтам — весомые, но простые к запоминанию секции. le1t оставляет архитектурные обоснования, выбор библиотек, и все Q&A. Каждый тиммейт получает verbatim script + concept definitions + cheat-card для печати.

**Build state:** docs-only commit, build green не нужен per §7.3.1 (type=docs → auto). Code не тронут, baseline preserved. 3536 строк новой документации в `docs/`.

**Закрытие:** Auto-close per §8.1. Commit `1db35ee` создан автоматически по §7.3.1 gate (type=`docs`, scope discipline clean — only my files staged, AGENTS.md + untracked LaTeX .tmp/ + tests/fixtures/Untitled.colonada.glb не в commit'е). Close-routine: (1) `git rev-parse HEAD` → `1db35ee`; (2) `agent/active-sessions.md` запись перенесена в «Закрытые сессии» (top, post-commit) + header обновлён; (3) эта секция обновлена (open → closed); (4) safety-net patch НЕ сохранял — нет uncommitted work (всё закоммичено).

## §23. Agent protocol rewrite r0 — `session-2026-06-15T15:00Z-agent-protocol-rewrite-r0` (open → close по новым правилам)

**Оператор явно попросил переписать протокол**: auto-commit + auto-close, плюс явно зафиксировать что `agent/*` = shared infra. Закреплено в `AGENTS.md` §7.2.8 (новый), §7.3.1 (новый), §8.1 (rewrite). 3 файла / +136 / -37 строк.

**Ключевые поведенческие изменения (для следующих сессий):**

- **`type=fix` ≠ auto-commit.** Per `AGENTS.md §7.3.1`, фиксы ждут явного operator confirm что фикс работает. `feat` / `refactor` / `perf` / `docs` / `test` / `build` / `chore` / `revert` — auto.
- **Auto-close по умолчанию.** После успешного commit — close routine (5 шагов) автоматически. Keep-open: multi-commit sub-plan / operator next-step / `continues: <reason>`.
- **`agent/*` = shared infra.** Конкурентный edit разрешён, не claim'ить. APPEND в свою секцию, не стирай чужое. Per `AGENTS.md §7.2.8`.
- **Destructive не трогаем.** `rebase` / `push --force` / `reset --hard` / `revert` / `branch -D` / network publish / `sudo` / `rm -rf` — **всегда** operator confirm.
- **Edge cases → `open` + `BLOCKED: <gate>`.** Gate fail / commit fail / scope collision / build broken — сессия остаётся `open`, retry после фикса.

**Транзишн к новым правилам:** эта правка идёт по **старому** §1 (явная команда + draft approved). После неё новый §1.3 отменяет draft-approval loop для будущих правок AGENTS.md.

**Build state:** не запускаю — change чисто в `AGENTS.md` + `agent/*`, code не тронут, baseline preserved. 3 файла / +136 / -37 строк (per `git diff --stat`).

**Verification (static):** `git diff HEAD AGENTS.md` — §1.3 / §7.2.4 / §7.2.5 / §7.2.6 (hub-list + active-sessions note) / §7.2.8 (новый) / §7.3.1 (новый) / §8 invariant 2 / §8.1 rewrite / §9 DoD + pre-commit gate. Cross-refs: §7.2.8 ↔ §7.2.6 hub-список; §7.3.1 ↔ §7.2.6, §7.2.8; §8.1 ↔ §7.3.1, §7.2.6. Без orphan rules.

**Сессия:** `status: open` сейчас, после commit → auto-close (move в «Закрытые сессии», `closed-at` + `commit-hash`). Эта сессия — сама пример новых правил в действии.

## §24. Windows build verification r0 — `session-2026-06-15T10-25Z-windows-build-verification-r0` (closed, 5/5 atomic-commits, build green)

**Per operator «Мы сейчас в arch linux, нужно как-то проверить, что сборки windows-clang-debug и windows-clang-release будут работать. Предлагаю проверить досконально всё там, но без возможности запустить код на винде и проверить на практике.»** Read-only static audit (3 параллельных explore-агента на платформенный код / submodule state / scripts + tools) обнаружил **3 P0 + 6 P1 + 10 P2 + 4 P3** риска. Plan утверждён оператором: 5 atomic-commits (Tier A-D), все landed в одной сессии.

**Commits (5/5 per `AGENTS.md §7.2.6.1`):**

| # | SHA | type | Что |
|---|----|------|-----|
| 1 | `adaae65` | `build(cmake)` | gate libc++ + Windows-clang-cl fix (root CMakeLists.txt + src/app/main.cpp + src/CMakeLists.txt) |
| 2 | `e9d957a` | `build(cmake)` | split Tracy UI from ProjectV-tracy-instrumented (CMakePresets.json + tools/tracy-standalone/) |
| 3 | `d31f141` | `refactor(scripts)` | extract repo-root walk-up + Windows LookDev smoke parity (src/core/RepoRoot.{hpp,cpp} + Windows PS1) |
| 4 | `d997056` | `docs(build)` | README sync + MSVC runtime docs + .gitattributes + env-var typo/lie removal + memory.md flecs version sync |
| 5 | `69b1726` | `chore(submodules)` | deinit 5 unwired vendored libs (RmlUi, stdexec, glaze, freetype, zstd — 62M reclaimed) |

**Tier A (P0 — build/feature блокеры):**
- **Commit 1:** `add_compile_options(-stdlib=libc++)` + `set(CMAKE_CXX_STDLIB libc++)` + `-Wno-unused-command-line-argument` + `set(CMAKE_CXX_MODULE_STD ON)` были глобально — на Windows-clang-cl приводили к LLD link error `library not found for -l:libstdc++.so.6` (projectv_build_options `else()` branch при `if (MSVC) = FALSE` для clang-cl добавлял Linux-only link options). Решение: гейтить всё в `if (NOT MSVC AND NOT WIN32)` + реструктурировать `projectv_build_options` в 3 ветки: `if (MSVC)` (pure cl.exe + `/wd4996` для flecs C4996) / `elseif (WIN32)` (clang-cl + MSVC STL, без libc++ link) / `else ()` (Linux/macOS native clang, без изменений).
- **Commit 1:** F5 hot-reload (defense r0) был hardcoded `build/linux-clang-debug` + `/tmp/projectv_shader_reload.log`. На Windows → `cmd.exe` has no `/tmp` + неверный build tree. Решение: `target_compile_definitions(ProjectV PRIVATE PROJECTV_CMAKE_BUILD_DIR="${CMAKE_BINARY_DIR}")` в src/CMakeLists.txt + `std::filesystem::temp_directory_path()` для log path.
- **Commit 2:** `windows-clang-debug-tracy-profiler` `PROJECTV_BUILD_TRACY_PROFILER: ON → OFF` (как Linux-вариант per `decisions.md §4`). Tracy UI build → standalone через `tools/tracy-standalone/{README.md, build-tracy-windows.ps1, build-tracy-linux.sh}` (CMake preset schema v1..v10 не поддерживает `sourceDir` в child preset, поэтому wrapper scripts вместо preset).

**Tier B (P1 — функциональные улучшения):**
- **Commit 3:** `projectv::core::FindRepoRoot` extracted to `src/core/RepoRoot.{hpp,cpp}` (shared helper). `MusicDirectoryPath.cpp` refactored to use it (потерял 36 строк duplicated walk-up логики). `SceneConfig::GetDefaultSceneConfigPath` теперь делает walk-up от `SDL_GetBasePath()` вместо CWD-relative literal — на Windows при запуске из `build\windows-clang-debug\bin\` через Explorer файл теперь резолвится в `<repoRoot>\runtime\scene.json` (раньше молча создавал в `bin\runtime\scene.json`).
- **Commit 3:** `tools/windows/Invoke-ProjectVRuntimeSmoke.ps1` теперь поддерживает LookDev capture env-var contract (опционально, через `-CaptureDir` / `-Views` / `-CameraPosition` / `-CameraLook` / `-WarmupFrames` / `-IntervalFrames` / `-QuitAfterCapture`); раньше Windows smoke был только lifecycle (window resize / minimize / restore / maximize / CloseMainWindow). Default behavior (без `-CaptureDir`) — без изменений.

**Tier C (P1/P2/P3 — docs/cleanup):**
- **Commit 4:** `README_NEW.md` (было: "libstdc++ на Windows, libc++ на Linux" → стало: "libc++ на Linux/macOS, MSVC STL на Windows") + `README.md` (sccache install hint) + `docs/BuildAndRun.md` (Visual C++ Redistributable required, `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` alternative) + `.gitattributes` (LF для source/scripts/CMake, CRLF для .bat/.cmd/.sln/.vcxproj — per `agent/memory.md §6` CRLF/LF ghosts incident) + remove `PROJECTV_RENDERER_TAA` lies from `docs/DefenseBriefer_3.md` / `DefenseDemoScript.md` / `DefenseFAQ.md` (env var не существует в коде, заменён на клавишу `T` через `taaEnabled` shader variant per `decisions.md §18`) + fix `PROJECTV_ENABLE TRACY` typo → `PROJECTV_ENABLE_TRACY` + `agent/memory.md:201` (flecs 2.2.0 → 4.1.5 per actual pinned SHA).

**Tier D (P3-1 — submodule cleanup):**
- **Commit 5:** deinit 5 unwired submodules (RmlUi 23M + stdexec 4.4M + glaze 11M + freetype 14M + zstd 9.8M = **62M reclaimed**). Все подтверждены 0 #include references в src/ + tests/. **DESTRUCTIVE** — operator confirm через Q&A этой сессии. Safety-net `/tmp/before_unwired_submodules_2026-06-15T1050Z.patch` (12778 bytes pre-footer, 12961 bytes post-footer).

**Build state финальный (Linux baseline preserved):**
- `linux-clang-debug`: configure 0.6s green, build 110/110 targets clean, ctest 14/14 in 0.76s, smoke 6/6 (VoxelLab reference shot в `build/linux-clang-debug/lookdev-captures/2026-06-15-repo-root-walkup-test/`).
- `linux-clang-release`: configure 0.5s green (verified after commit 1).
- `linux-clang-debug-tracy-profiler`: configure 0.6s green (UI=OFF inherited от Linux, unchanged).
- `linux-clang-debug-tracy-profiler` (Windows variant): `PROJECTV_BUILD_TRACY_PROFILER: ON → OFF` (per commit 2). Tracy UI standalone build через `tools/tracy-standalone/build-tracy-windows.ps1` (предполагает отдельный build tree `build/windows-clang-tracy`).

**Windows-side verification:** static review only. На Arch Linux реально собрать `windows-clang-debug` / `windows-clang-release` / `windows-clang-tracy` невозможно (нет clang-cl / MSVC). CMakePresets.json + tools/tracy-standalone/ build scripts + .gitattributes готовы для Windows-host verification оператором.

**Pre-commit gates (per `AGENTS.md §7.3.1`):** все 5 commits — `type=build/refactor/docs/chore` (auto per п.3). Commit 5 destructive (submodule ops) — operator confirm в Q&A этой сессии («Все 5 commits в одной сессии» option explicitly flagged DESTRUCTIVE per `§7.2.2`).

**Cross-refs:** `agent/decisions.md §4` (+2 новые sub-section: "Windows-clang-cl libc++ gating fix 2026-06-15" + "Tracy UI standalone build split 2026-06-15"), `agent/memory.md §6` (без изменений; libc++/libstdc++ history сохранён; новая секция о Windows-build-verification append ниже), `agent/active-sessions.md session-2026-06-15T10-25Z-windows-build-verification-r0` (closed per `§8.1` с `commit-hash: 69b1726` + `closed-at: 2026-06-15T10:50:00Z`).

---

## 99. Past closed sessions (rollup)

Полный per-session detail в `legacy/docs/archive/agent-status-snapshots/2026-06-week-1.md`. Здесь — одна строка на сессию для быстрого navigation.

| Date | Session | Outcome |
|---|---|---|
| `2026-06-11` | TAA A2 close-out | `taaEnabled=true`, SPIR-V search path fix, smoke verified |
| `2026-06-12` | P1 shadow fix | SSBO double-buffer, fence reorder, cascade depth, TAA clamp (`b7e672f`) |
| `2026-06-12` | TAA Блок 1.1 YCoCg clamp | `a2972fa` (was in-progress, not closed) |
| `2026-06-12` | TAA 1.4 + 5.1 + M5.2 + 6 | LANDED |
| `2026-06-12` | TAA 1.2 + 1.3 | camera-cut detector + adaptive CAS (LANDED) |
| `2026-06-12` | TAA 1.7 | R11G11B10_UFloat scene color (LANDED) |
| `2026-06-12` | TAA 1.5 | per-layer (CTSH/AOCC/LOCL) anti-flicker (LANDED) |
| `2026-06-12` | Low-level perf + tooling | closed (uncommitted) |
| `2026-06-12` | A1 greedy meshing (4.1) | closed (uncommitted) |
| `2026-06-12` | M5.1d asset-pipeline | closed, `8cc71f8` |
| `2026-06-12` | Frame-step / slow-motion | closed (uncommitted) |
| `2026-06-12` | Per-pass CPU timings | closed (uncommitted) |
| `2026-06-12` | Audio engine (miniaudio) | closed (uncommitted) |
| `2026-06-12` | Music HUD: 1-line → 4-line | closed, `723edc5` |
| `2026-06-13` | Hardcore perf r0 | Phase 0 = doc only (open) |
| `2026-06-13` | Defense preparation r0 | closed (defense 2026-06-15) |
| `2026-06-13` | KT-документы | closed → reopened → closed (3 updates) |
| `2026-06-13` | Defense файлы | closed (reopened 2-й update) |
| `2026-06-14` | KT-LaTeX (KT-2.1/2.2/3.1/3.2 + Combined) | closed (5 PDF) |
| `2026-06-15` | Defense docs overhaul r0 | closed `1db35ee` (см. `agent/active-sessions.md session-2026-06-15T15-50Z-defense-docs-r0`): 7 новых файлов + 4 переработки + 2 agent-файла |
| `2026-06-15` | Defense docs audit r0 | closed `bf2822f` (см. `agent/active-sessions.md session-2026-06-15T10-43Z-defense-docs-audit-r0`): 23 правки в 12 docs/ + F5/F6 → F11/F12 relocate в main.cpp |
| `2026-06-15` | **Windows build verification r0** | **closed `69b1726`** (см. `agent/active-sessions.md session-2026-06-15T10-25Z-windows-build-verification-r0`): 5 atomic-commits (P0 libc++/Windows-clang-cl gating + Tracy UI split + RepoRoot extract + docs/cleanup + deinit 5 submodules 62M). Linux baseline preserved (ctest 14/14, smoke 6/6). |

Cross-refs на архив полных версий: `agent/ARCHIVE-INDEX.md` (single source of truth для navigation).
