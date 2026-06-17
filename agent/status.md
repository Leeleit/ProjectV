# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-06-17` — Defense clean-slate rewrite (session `ef8b942`, см. §37). DefenseScript_Team.md (148→110 строк) + DefensePresentation_Structure.md (1006→541 строк, 13 слайдов LaTeX Beamer) переписаны из текста оператора; FAQ_T{1..6}.md §1 Verbatim синхронизирован (6/6 verified). Tone natural, problem justification CPU-physics-bound, ТЗ↔тесты aligned.
Updated: `2026-06-17` — Defense LaTeX Beamer PDF r0 (`b221d1f`, см. §38). `docs/tex/defense/DefensePresentation.pdf` готов (13 страниц, 16:9, 205 KB). xelatex TeX Live 2026.
Updated: `2026-06-17` — Defense presentation patches r0 (`0aa863c`, см. §39). Применены 4 операторских фикса: `\resizebox` для таблиц 3/5/12, QR удалён из slide 1, booktabs для slide 12, `\scriptsize` для slide 10.
Updated: `2026-06-17` — Defense presentation round 3 r0 (`341c6cf`, см. §40). 5 фиксов: subtitle белым на синем, slide 4 image 0.45, slide 11 без BUG-005, slide 12 реальные имена без колонки «Роль», новый VoxelLab screenshot.
Updated: `2026-06-17` — Defense le1t name r0 (`538cc25`, см. §41). Slide 12: «Кадочников Л. (le1t)» → «Кадочников Лев Петрович».

Updated: `2026-06-15` — **Windows build verification r0 (session-2026-06-15T10-25Z-windows-build-verification-r0, см. §24)**. 5 atomic-commits landed: libc++/Windows-clang-cl gating fix (P0-1..P0-4) + F5 hot-reload CMake-injected path (P0-5) + Tracy UI split to standalone build (P0-6) + RepoRoot extract + Windows LookDev smoke parity (P1-2/P1-3) + docs/cleanup + deinit 5 unwired submodules 62M. Linux baseline preserved (ctest 14/14, smoke 6/6).

Updated: `2026-06-14` — 4 КТ-документа сконвертированы в LaTeX (session-2026-06-13-kt-latex-r0, см. §19 в archive). 5 PDF в `docs/tex/`.
Updated: `2026-06-14` — V-sync FIFO bug + CA pause/timeScale + 20Hz default (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up, см. `decisions.md §30.1`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.1` → archive). 3 fixes. 8 new CA sub-tests (24 total, 100% pass). Build green, ctest 13/13, smoke clean.
Updated: `2026-06-14` — V hotkey auto-detect cycle + libc++ warning + HUD line (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up #2, см. `decisions.md §30.2`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.2` → archive). 4 fixes. 9 new present-mode sub-tests, 100% pass. Build green, ctest 14/14, smoke clean.
Updated: `2026-06-14` — V hotkey cycle walk fix (session-2026-06-13-hardcore-perf-r0, Phase 2 follow-up #3, см. `decisions.md §30.3`, `legacy/docs/archive/agent-memory/2026-06-fluid-ca-sessions.md#12.3` → archive). 1 fix. 3 new sub-tests (12 total). Build green, ctest 14/14, smoke clean.
Updated: `2026-06-14` — Release presets r0 (session-2026-06-14-release-presets-r0, см. §21). 8 CMakePresets, root `CMakeLists.txt` +Release-блок, `README_NEW.md` создан, `agent/decisions.md §4` +Release policy. linux-clang-release: configure 54s, build 137/137 green, ctest 13/13, smoke 6/6, ELF 19MB (-73% vs debug).
Updated: `2026-06-14` — Build config audit r0 (session-2026-06-14T11-29Z-build-config-audit-r0, см. §22). 5 buildPresets (3 debug × 17, 2 release × 15, 1 smoke). `linux-clang-debug-tracy-profiler.PROJECTV_BUILD_TRACY_PROFILER: ON→OFF`. `linux-clang-debug-ci/` удалён (194M). `linux-clang-debug-tracy-profiler/` сохранён. linux-clang-release ctest 14/14.
Updated: `2026-06-17` — Defense clean-slate script rewrite r0 (`ef8b942`, см. §37). Script_Team 148→110 строк, Presentation_Structure 1006→541 строк (13 слайдов LaTeX Beamer с `\_` `\&` `\%` `$..$` экранированием), FAQ_T{1..6}.md §1 Verbatim 6/6 sync verified через `/tmp/verify_faq_sync.py`. Tone natural, technical-mature, требования↔тесты aligned.

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

## §26. Defense docs russian r0 — `session-2026-06-15T12-06Z-defense-docs-russian-r0` (closed в `d641967`)

**Per operator «надо всё на русском, полностью. Всё английское в скобочки и слева от скобочек русское название, если это термин какой-то.»**

Единый коммит `d641967` (option A по выбору оператора), 8 файлов, +511/-836 строк (нетто -325 — упрощение, не добавление текста). type=docs, auto per §7.3.1.

**Что сделано:**

| Файл | Действие | Строк (до → после) |
|---|---|---|
| `DefenseBriefer_1.md` | REWRITE (простой русский, ~150 слов verbatim) | 165 → 131 |
| `DefenseBriefer_2.md` | REWRITE | 155 → 101 |
| `DefenseBriefer_3.md` | REWRITE | 168 → 106 |
| `DefenseBriefer_4.md` | REWRITE | 164 → 104 |
| `DefenseBriefer_5.md` | REWRITE | 174 → 101 |
| `DefenseBriefer_le1t.md` | REWRITE (вступление 2:00 + Q&A-карта 30 вопросов) | 351 → 280 |
| `DefenseAlgorithms.md` | EDIT (заголовки 1-23 + краткая карта переведены) | 1021 → 1021 |
| `agent/active-sessions.md` | EDIT (новая запись → перенос в «Закрытые сессии») | — |

**DefenseBriefer (1..5).md — главное изменение:**
- Удалена §6 «Если попросят подробнее» (operator: «ненужную хрень по типу объяснений, как и что работает. Этого всё равно никто не поймёт»).
- Дословные выступления сжаты с ~220 до 141-157 русских слов на 1:30 минуты.
- §2, §3, §4, §5, §7 — простой русский, термины в скобках.

**Заголовки 23 алгоритмов переведены:** «Жадный мешинг (greedy meshing, алгоритм Лысенкова)», «Каскадные тени (CSM)», «Трассировка лучей через compute-шейдер (ray-marching)», «Клеточный автомат для жидкости (Fluid CA)» и т.д.

**Формат перевода:** «Русское название (English term)» при первом использовании. Повторно — только русский. Идентификаторы кода, пути файлов, консольные команды, имена шейдеров, имена сторонних библиотек, устоявшиеся аббревиатуры (TAA, CSM, PBR, ECS, DOD, AVX2, SIMD, HUD, AABB, GPU, CPU, RAM) — оставлены на английском.

**5 файлов НЕ переписывались (уже были в хорошем русском состоянии):**
- `DefenseFAQ.md` — по-русски от audit `bf2822f`
- `DefenseReport.md` — по-русски от audit `bf2822f`
- `DefenseScript.md`, `DefenseDemoScript.md`, `DefenseSpeakerNotes.md` — по-русски от original `1db35ee`

Cross-check: `rg "13 824|MP3/WAV/FLAC|72 MB|0\\.1 м" docs/Defense*.md` → 0 matches.

**Build state:** docs-only, build green. Code не тронут, baseline preserved.

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

## §27. Windows build verification r0 — `session-2026-06-15T10-25Z-windows-build-verification-r0` (closed, 5/5 atomic-commits, build green)

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

## §28. Post-WBV-r1 batch — `session-2026-06-15T-post-wbv-r1` (closed в этом commit)

**Per operator «F13-F24 нет ни на одной клавиатуре нормальной. Вариант B. Приступай, идиот.»** + «ТОлько один» — единый `fix` commit, batching T1.1 + T0.3 (3 файла) + T1.2 (55 файлов) = 58 source-файлов + 2 agent/* файла = 60 files в одном коммите.

**3 sub-task'а:**

| # | Item | Files | Что |
|---|------|-------|-----|
| T1.1 | F11/F12/V double-fire | `src/app/main.cpp:545-619` | Relocate defense-r0 hotkey bypass: shader hot-reload `SDLK_F11 → SDLK_1`, ray-march toggle `SDLK_F12 → SDLK_2`, V-sync cycle `SDLK_V → SDLK_3`. All 26 letters A-Z и F1-F12 bound в `InputAction`; digits 1, 2, 3 — единственный свободный top-row cluster. F11/F12/V → InputAction as originally intended (no shadow). |
| T0.3 | shader contract | `src/shaders/{model.frag, model.vert, taa_resolve.frag}` | Add `vec4 taaLayerHistoryParams;` to 3 model/TAA-pipeline shader'ов — match C++ `VoxelSceneLighting` byte layout per `decisions.md §18` (1.5 anti-flicker 16 B @ offset 608, total 624 B). The 3 voxel-pipeline shaders (`voxel.frag:54`, `voxel_shadow.vert:56`, `voxel_mesh.comp:95`) уже in sync; эти 3 были missed — std430 layout mismatch, OOB-read past C++ struct end. Recurrence of `agent/memory.md §10.8` GraphicsPushConstants incident. |
| T1.2 | pragma once | 55 `.hpp` файлов | Convert `#ifndef X / #define X / #endif` → `#pragma once` per project convention `agent/memory.md §10.1`. Inner `#if` blocks (Tracy/RenderDoc/modules guards) preserved untouched. Files across `asset/`, `app/`, `c_kernels/`, `core/`, `debug/`, `ecs/`, `physics/`, `platform/`, `render/`, `render/vulkan/`, `voxel/`. 3 pre-existing `#pragma once` headers (`core/RepoRoot.hpp`, `audio/MusicDirectoryPath.hpp`, `audio/AudioEngine.hpp`) — без изменений. |

**T0.2 renumbering (bonus, inline в этом commit):** Renamed duplicate `## §24` (windows-build-verification-r0) → `## §27`. The other `## §24` (defense-docs-overhaul, 15:50Z) остаётся §24 — chronologically правильно (15:50Z = latest). `rg "status\.md §2[4-7]\b" agent/ TODO.md` → 0 matches (zero cross-refs к §24-§27 anywhere), так что renumbering не ломает ничего.

**Build state:**
- `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8` — **151/151 targets green**, 0 errors, 0 new warnings.
- `ctest --test-dir build/linux-clang-debug -j 8 --output-on-failure` — **14/14 pass за 0.68s**, baseline preserved.
- Safety-net patch: `/tmp/before_post_wbv_r1_<ts>.patch` — пустой (working tree was clean до этой сессии; единственный modified файл AGENTS.md — чужой protocol rewrite, не мой).

**Pre-commit gate (§7.3.1):**
- §7.2.5 message: `fix(post-wbv-r1): F11/F12/V double-fire + shader contract + pragma once batch`.
- Scope discipline: AGENTS.md modified чужой сессией (operator protocol rewrite), `legacy/docs/tex/.tmp/*` (kt-latex-r0), `tests/fixtures/Untitled.colonada.glb` (defense-docs-r0) — все вне scope, не в commit'е.
- type=fix → operator confirm = «Приступай, идиот» в этой сессии.

**Defer (явно per operator «ТОлько один»):**
- T1.3 std::expected migrations (9+ cold-path functions)
- T2.x perf batch (6 items: tracy, shadow, audio StringId, meshing, ECS, input)
- T3.x docs/chore/test (5 items: F-key doc, F-table, misleading VMA comment, TaaRenderTargets test, agent/memory.md append)
- T0.1 active-sessions.md stale-entry cleanup (windows-build-verification-r0 в «Активные» section с `status: closed` — owned by other session per `§7.2.8`, не моя)

**Cross-refs:** `agent/memory.md §10.1` (C++26 baseline + pragma once convention), §10.7 (Vulkan docs before grep), §10.8 (shader-C++ struct byte parity); `agent/decisions.md §18` (TAA contract).

---

## §29. Defense team script rebuild r0 — `session-2026-06-16T22-23Z-defense-team-script-rebuild-r0` (closed в `45a15bc`)

**Per operator «глянь DefenseScript_Team, я подправил текст 1 участника и меня (второго участника), всё дальше плохо написано»** + «нам дают всего 5 минут на рассказ, а не 10: то есть мы должны рассказывать 4:30 (30 секунд на форс-мажоры)» + «строго 5 минут, после сразу останавливают».

Единый atomic commit `45a15bc` (operator: «9. A»), 10 файлов, +881/-522 строк, 3 новых файла + 7 modified. type=`docs` → auto per §7.3.1.

**Новый 3-актный нарратив T3-T5:**

| Слот | Тема | Хрон | Слов |
|---|---|---|---|
| T1 | Вступление и Проблема | 0:00-0:45 | ~85 |
| T2 (le1t) | Live Demo + Стек (C++26, Vulkan 1.4, DOD) | 0:45-2:00 | ~145 |
| T3 | **Архитектура и качество кода** (включая статик-ассерты) | 2:00-2:40 | ~80 |
| T4 | **Тесты и проверки** (ctest, smoke, sidecar) | 2:40-3:20 | ~80 |
| T5 | **Прочие фичи + что отложено** | 3:20-4:00 | ~85 |
| T6 | Планы + Закрытие | 4:00-4:30 | ~60 |
| — | Буфер на форс-мажоры | 4:30-5:00 | тишина |

**Что сделано:**

| Файл | Действие |
|---|---|
| `docs/DefenseScript_Team.md` | REWRITE — header 4:30, T3/T4/T5/T6 verbatim (T1+T2 оставлены как есть) |
| `docs/DefensePresentation_Structure.md` | REWRITE — тайминги 4:30, слайды привязаны к слотам |
| `docs/DefenseBriefer_1.md` | REWRITE — T1 Вступление, 5 секций без §6 шпаргалки |
| `docs/DefenseBriefer_2.md` | REWRITE — T3 Архитектура + статик-ассерты |
| `docs/DefenseBriefer_3.md` | REWRITE — T4 Тесты |
| `docs/DefenseBriefer_4.md` | REWRITE — T5 Прочие фичи + отложено |
| `docs/DefenseBriefer_5.md` | REWRITE — T6 Планы + закрытие |
| `docs/DefenseBriefer_le1t.md` | REWRITE — 1:15 slot + Q&A 30+ вопросов (НЕ сокращены) + новые cue-карты |
| `docs/archive/DefenseBriefer_TechnicalDeepDive_2026-06-15.md` | NEW — консолидация старых бриферов 2-5 для Q&A reference (воксели/рендеринг/физика/демо+аудио) |
| `docs/DefenseScript_Solo.md` | DELETE — оставлен только team-вариант |

**Принципы:**
- 5 бриферов — 5 секций (без §6 шпаргалки, operator: «Шпаргалки для печати не нужны»).
- Q&A-карта в `DefenseBriefer_le1t.md` — 30+ вопросов, НЕ сокращена (operator: «Больше вопросов и ответов – больше покрытие, не уменьшай»).
- Технические детали из старых бриферов 2-5 → `archive/DefenseBriefer_TechnicalDeepDive_2026-06-15.md` для Q&A подготовки, НЕ на сцене.
- Роли в речи НЕ называются (operator: «нам надо красиво подать проект, а когда будут задавать вопросы, тут компетенция каждого уже понадобится»). Role separation живёт в §5 «Вне зоны ответственности» каждого брифера для Q&A.

**Что явно отвергнуто в речи (per operator):**
- ❌ FPS / сцена / время кадра / размер EXE как плюс (T2 территория)
- ❌ Lambda 0.80 / 8-sample Halton / 12 трассировок (operator: «никому не нужная техническая информация, 12 трассировок сраный ты кусок говна»)
- ❌ TAA tremor / BUG-004 (jitter=0 по умолчанию, BUG-004 галлюцинация)
- ❌ Три режима walk/creative/spectator в T5 (уже в T2-демо)
- ❌ macOS (нет в планах)
- ❌ «серьёзно поработали», «очень серьезно подошли» (фразы-паразиты)
- ❌ Linux / PulseAudio в речи (operator: «надо просто сказать, что работает аудиодвижок»)
- ❌ vertex cache / fetch в речи (operator: «это опять подробности, их надо убрать»)
- ❌ «воксельный решатель» / «пассивное зеркало» в T3 — заменены на «наш собственный код дополняет её для опоры игрока на блоки» и «для отладки данные дублируются в систему компонентов — но это всегда копия из основного мира, не наоборот»

**Build state:**
- `cmake --build build/linux-clang-debug --target ProjectV` — green (other session's `VulkanSwapchain.cpp` изменение линковалось успешно). docs-only change в моей сессии, baseline preserved.

**Pre-commit gate (§7.3.1):**
- §7.2.5 message: `docs(defense): пересборка командного скрипта под 5-минутный формат защиты` + body.
- Scope discipline: AGENTS.md (другой сессии protocol rewrite), `src/render/vulkan/VulkanSwapchain.cpp` (другой сессии), `docs/DefenseQnA.md` (untracked, не моя), `legacy/docs/tex/.tmp/*` (kt-latex) — все вне моего scope, не в commit'е.
- type=`docs` → auto, без operator confirm.

**Cross-refs:** `AGENTS.md §7.2.6.1` (atomic subtask), `§8.1` (auto-close), `§7.3.1` (pre-commit gate), `§7.2.8` (shared `agent/` files — правки `active-sessions.md` не claim'ят эксклюзив); `agent/active-sessions.md` session-2026-06-16T22-23Z-defense-team-script-rebuild-r0; `docs/DefenseAlgorithms.md` (23 алгоритма, не переписывались); `docs/DefenseFAQ.md` (готовые ответы, не переписывались); `docs/DefenseReport.md` (отчёт + §3 deferred items, не переписывался).

---

## §31. Defense competency FAQ split r0 — `session-2026-06-17T-defense-competency-faq-split-r0` (closed в `7581963`)

**Per operator: «Всё же лучше на несколько файлов разделить» + «DefenseBirefer_* не нужны, так как у нас есть DefenseScript_Team и появятся Competency».**

Единый atomic commit `7581963`, 17 files changed, +1997/-2726 строк, 7 new + 8 deleted (6 briefers + монолит FAQ). type=`docs` → auto per §7.3.1.

**Новая файловая структура (1944 строки итого):**

| Файл | Строк | Содержимое |
|---|---|---|
| `docs/DefenseCompetencyFAQ.md` | 392 | INDEX + Common (§0 общая карта, hotkeys, glossary, chronology) |
| `docs/DefenseCompetencyFAQ_le1t.md` | 422 | le1t (Архитектура + Q&A host, 40 вопросов) |
| `docs/DefenseCompetencyFAQ_T3.md` | 253 | Тиммейт 3 (Рендеринг) |
| `docs/DefenseCompetencyFAQ_T4.md` | 251 | Тиммейт 4 (Физика и walk-контроллер) |
| `docs/DefenseCompetencyFAQ_T2.md` | 235 | Тиммейт 2 (Воксельный мир) |
| `docs/DefenseCompetencyFAQ_T5.md` | 223 | Тиммейт 5 (Ассеты и аудио) |
| `docs/DefenseCompetencyFAQ_T1.md` | 168 | Тиммейт 1 (Сборка и тестирование) |

**Удалённые файлы (8):**
- `docs/DefenseCompetency_FAQ.md` (монолитный, 1888 строк) — заменён на 7 split
- `docs/DefenseBriefer_1.md` (T1 Build/Test)
- `docs/DefenseBriefer_2.md` (T3 Voxel — было до переназначения)
- `docs/DefenseBriefer_3.md` (T4 Render)
- `docs/DefenseBriefer_4.md` (T6 Physics)
- `docs/DefenseBriefer_5.md` (T5 Asset/Audio)
- `docs/DefenseBriefer_le1t.md` (T2 Demo + Q&A-карта — перенесена в FAQ le1t §6.4)

**Принципы:**
- Speech slot ≠ real competency (per `§30` Defense competency FAQ r0).
- Per-team файлы содержат ТОЛЬКО competency FAQ (Кто ты, Компетенция, Что смотреть, Вопросы, Out of scope).
- Verbatim тексты выступлений — в `docs/DefenseScript_Team.md`. Per-team файлы ссылаются на разделы скрипта для каждого тиммейта.
- INDEX в `DefenseCompetencyFAQ.md` указывает на все 6 per-team файлов с competency и speech slot mapping.

**Speech ↔ Competency mapping (без изменений из §30):**

| Слот | Тема | Кто говорит | Real competency |
|---|---|---|---|
| T1 | Вступление и проблема | Тиммейт 1 | Сборка и тестирование |
| T2 | Live Demo + Стек | le1t | Архитектура + Workflow + Q&A host |
| T3 | Архитектура и качество кода | Тиммейт 2 | Воксельный мир |
| T4 | Тесты и проверки | Тиммейт 3 | Рендеринг |
| T5 | Прочие фичи + что отложено | Тиммейт 5 | Ассеты и аудио |
| T6 | Планы и Завершение | Тиммейт 4 | Физика и walk-контроллер |

**Проверка фактов (per operator «при работе читай код»):** факты FAQ основаны на проверенном содержимом монолитного `DefenseCompetency_FAQ.md` (коммит `c14e1bd`), который был проверен против `src/**` в предыдущей сессии. В этой сессии дополнительная верификация не требовалась (только split + delete).

**Build state:**
- `cmake --build build/linux-clang-debug --target ProjectV` — green (docs-only change, baseline preserved).

**Pre-commit gate (§7.3.1):**
- §7.2.5 message: `docs(defense): split monolithic FAQ на 7 файлов + удалить 6 briefers` + body.
- Scope discipline: AGENTS.md (другой сессии), `src/render/vulkan/VulkanSwapchain.cpp` (другой сессии), `legacy/docs/tex/.tmp/*` (kt-latex), `tests/fixtures/Untitled.colonada.glb` — все вне моего scope, не в commit'е.
- type=`docs` → auto, без operator confirm.

**Cross-refs:** `AGENTS.md §7.2.6.1` (atomic subtask), `§8.1` (auto-close), `§7.3.1` (pre-commit gate), `§7.2.8` (shared `agent/` files); `agent/active-sessions.md` session-2026-06-17T-defense-competency-faq-split-r0; `docs/DefenseScript_Team.md` (verbatim тексты выступлений); `docs/DefenseCompetencyFAQ*.md` (7 файлов, новые).

---

## §30. Defense competency FAQ r0 — `session-2026-06-17T-defense-competency-faq-r0` (closed в `c14e1bd`)

**Per operator «FAQ для каждого участника команды о его компетенции, что ему ботать, что смотреть и списки реалистичных+ каверзных вопросов и ответов. Нужно всё максимально подробное, словно учебник. Для меня тоже, если чё. Также нужно убрать ненужные документы в docs/archive» + «Ты путаешь у участников темы в речи защитной и настоящая компетентность в коде. ... Переназначаем».**

Единый atomic commit `c14e1bd` (operator: «9. A»), 14 файлов, +2444/-156 строк, 4 renames + 4 new + 6 modified. type=`docs` → auto per §7.3.1.

**Speech ≠ Competency (новый принцип):**

| # | Кто | **Реальная компетенция** (для Q&A) | **Speech slot** (на сцене) |
|---|---|---|---|
| 1 | Тиммейт 1 | Сборка и тестирование (CMake, ctest, smoke) | T1 Вступление и проблема |
| 2 | Тиммейт 2 | Воксельный мир (чанки, meshing, fluid CA, snapshot) | T3 Архитектура и качество кода |
| 3 | Тиммейт 3 | Рендеринг (Vulkan, TAA, CSM, AOCC, шейдеры) | T4 Тесты и проверки |
| 4 | Тиммейт 4 | Физика и walk-контроллер (Jolt, edge grace) | T6 Планы и Закрытие |
| 5 | Тиммейт 5 | Ассеты и аудио (glTF, Draco, miniaudio) | T5 Прочие фичи + что отложено |
| 6 | le1t | Архитектура + Workflow + Q&A host | T2 Live Demo + Стек |

Каждая секция FAQ явно показывает BOTH: «На сцене ты говоришь X. На Q&A отвечаешь по своей компетенции Y». Per operator: «нам надо красиво подать проект, а когда будут задавать вопросы, тут компетенция каждого уже понадобится».

**Что сделано:**

| Файл | Действие |
|---|---|
| `docs/DefenseScript.md` | `git mv` → `docs/archive/DefenseOldFormat_2026-06-17/DefenseScript_10min.md` |
| `docs/DefenseDemoScript.md` | `git mv` → `docs/archive/DefenseOldFormat_2026-06-17/DefenseDemoScript_10min.md` |
| `docs/DefenseSpeakerNotes.md` | `git mv` → `docs/archive/DefenseOldFormat_2026-06-17/DefenseSpeakerNotes_10min.md` |
| `docs/DefenseQnA.md` (untracked) | `mv` → `docs/archive/DefenseOldFormat_2026-06-17/DefenseQnA_10min.md` |
| `docs/archive/DefenseOldFormat_2026-06-17/README.md` | NEW — причина архивации |
| `docs/DefenseCompetency_FAQ.md` | **NEW — 1888 строк, textbook** |
| `docs/DefenseScript_Team.md` | REWRITE — reassigned speeches, таблица competency |
| `docs/DefenseBriefer_{1..5}.md` | REWRITE — speech + competency sections |
| `docs/DefenseBriefer_le1t.md` | EDIT — новый mapping + cue-карты, Q&A-карта 30+ сохранена |
| `docs/DefensePresentation_Structure.md` | REWRITE — reassigned слайды |

**FAQ структура (1888 строк, 6 секций + 2 приложения):**
- **§0 Общая карта** (стек, метрики, hotkeys, glossary, Phase 4-9 roadmap)
- **§1-§5 Per-teammate** (5 разделов × ~350 строк каждый = ~1750 строк):
  - Кто ты (легенда, slot, competency, out of scope)
  - Твоя компетенция (файлы, структуры, константы, hotkeys)
  - Что смотреть на защите (слайды, демо-этапы)
  - Реалистичные вопросы (5-7, textbook-style, file:line refs)
  - Каверзные вопросы (3-5, реально каверзные, не выдуманные)
  - Out of scope (таблица перенаправления)
- **§6 le1t** (~400 строк) — расширенная Q&A-карта 40 вопросов
- **Приложение A** — глоссарий ~100 терминов (C++26, DOD, ECS, JPH, VMA, volk, fastgltf, Draco, meshopt, miniaudio, PVSNAP01, CSM, TAA, AOCC, Halton, YCoCg, PBR, MRT)
- **Приложение B** — хронология решений (Tier 0-5, post-MVP roadmap, 2026-04-09 → 2026-06-17)

**Что НЕ сделано (out of scope):**
- ❌ Не модифицировал `docs/DefenseAlgorithms.md`, `DefenseFAQ.md`, `DefenseReport.md` — эталоны
- ❌ Не модифицировал `AGENTS.md` (другой сессии)
- ❌ Не модифицировал `src/render/vulkan/VulkanSwapchain.cpp` (другой сессии)
- ❌ Не удалял файлы (только `git mv` / `mv`)

**Проверка фактов (per operator «при работе читай код, ... надо код смотреть и всё перепроверять»):**
- 14 ctest тестов — подтверждено `ctest -N` (ProjectVTests, ProjectVAssetTests, ProjectVMeshBakerTests, ProjectVDracoTests, ProjectVFrustumCullingTests, ProjectVCFrustumCullingTests, ProjectVSunShadowCascadeSplitsTests, ProjectVBoxUvFixtureTests, ProjectVMathTests, ProjectVStringIdTests, ProjectVModuleSmoke, ProjectVStdModuleProbe, ProjectVFluidCATests, ProjectVPresentModeTests)
- 6/6 smoke captures (FINAL/SHDW/CSM/CTSH/AOCC/LOCL) — подтверждено в `decisions.md §4`
- 73 MB debug / 19 MB release — подтверждено `ls -lh build/.../ProjectV`
- 12 configure-пресетов — подтверждено `grep -c configurePreset CMakePresets.json`
- Все static_asserts, InputAction bindings (1/2/3/4/5/6/7/8/9/0/J/F11/F12), ray-march STUB, hot shader reload — проверены против исходного кода
- BUG-004 retraction сохранён: «Не существует, jitter=0 default» (Тиммейт 3 briefer §6, Тиммейт 5 briefer §5, FAQ §0.6)

**Build state:**
- `cmake --build build/linux-clang-debug --target ProjectV` — green, без warnings (docs-only change в этой сессии, baseline preserved)
- Q&A-карта в `DefenseBriefer_le1t.md` §4 сохранена полностью (30+ вопросов, без сокращений per operator «Больше вопросов и ответов – больше покрытие, не уменьшай»)

**Pre-commit gate (§7.3.1):**
- §7.2.5 message: `docs(defense): per-team competency FAQ (textbook) + архивация 4 устаревших 10-мин скриптов` + body.
- Scope discipline: AGENTS.md (другой сессии), `src/render/vulkan/VulkanSwapchain.cpp` (другой сессии), `legacy/docs/tex/.tmp/*` (kt-latex), `tests/fixtures/Untitled.colonada.glb` (другой сессии) — все вне моего scope, не в commit'е.
- type=`docs` → auto, без operator confirm.

**Cross-refs:** `AGENTS.md §7.2.6.1` (atomic subtask), `§8.1` (auto-close), `§7.3.1` (pre-commit gate), `§7.2.8` (shared `agent/` files); `agent/active-sessions.md` session-2026-06-17T-defense-competency-faq-r0; `docs/DefenseScript_Team.md` (commit `45a15bc` — base для 5-мин формата); `docs/DefenseAlgorithms.md` (23 алгоритма, не переписывался); `docs/DefenseFAQ.md` (готовые ответы, не переписывался); `docs/DefenseReport.md` (отчёт + §3 deferred items, не переписывался); `docs/archive/DefenseBriefer_TechnicalDeepDive_2026-06-15.md` (Q&A reference, не переписывался).

---

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
| `2026-06-15` | Defense docs russian r0 | closed `d641967` (см. `agent/active-sessions.md session-2026-06-15T12-06Z-defense-docs-russian-r0`): полная русификация, 6 бриферов переписаны + Algorithms.md, единый коммит option A |
| `2026-06-15` | **Windows build verification r0** | **closed `69b1726`** (см. `agent/active-sessions.md session-2026-06-15T10-25Z-windows-build-verification-r0`): 5 atomic-commits (P0 libc++/Windows-clang-cl gating + Tracy UI split + RepoRoot extract + docs/cleanup + deinit 5 submodules 62M). Linux baseline preserved (ctest 14/14, smoke 6/6). |
| `2026-06-16` | **Defense team script rebuild r0** | **closed `45a15bc`** (см. `agent/active-sessions.md session-2026-06-16T22-23Z-defense-team-script-rebuild-r0`): пересборка под 5-минутный формат (4:30 + 30с буфер), 10 файлов, T3-T6 переписаны в стиле T1/T2 (простой разговорный русский, без техно-цифр), Q&A-карта 30+ вопросов сохранена, archive deep-dive для reference, `DefenseScript_Solo.md` удалён. |
| `2026-06-17` | **Defense competency FAQ r0** | **closed `c14e1bd`** (см. `agent/active-sessions.md session-2026-06-17T-defense-competency-faq-r0`): per-team competency FAQ (textbook, 1888 строк, 6 секций + 2 приложения), speech ≠ competency principle, 4 устаревших 10-мин скрипта в `docs/archive/DefenseOldFormat_2026-06-17/`, переназначение speech slots под competency-matched mapping (T1=Т1 Build/Test, T3=Т2 Voxel, T4=Т3 Render, T5=Т5 Asset/Audio, T6=Т4 Physics). |
| `2026-06-17` | **Defense competency FAQ split r0** | **closed `7581963`** (см. `agent/active-sessions.md session-2026-06-17T-defense-competency-faq-split-r0`): монолитный FAQ (1888 строк) разделён на 7 файлов (1944 строк итого — `DefenseCompetencyFAQ.md` 392 + `DefenseCompetencyFAQ_T1..T5.md` 168/235/253/251/223 + `DefenseCompetencyFAQ_le1t.md` 422 с 40 вопросами). Удалены 6 `DefenseBriefer_{1..5}.md` + `DefenseBriefer_le1t.md` — verbatim в `DefenseScript_Team.md`, понятия и competency FAQ в per-team файлах. |
| `2026-06-17` | **Defense competency FAQ self-contained r0** | **closed `b0feee8`** (см. `agent/active-sessions.md session-2026-06-17T-defense-competency-faq-self-contained-r0`): 6 FAQ файлов перенумерованы по speech slot (T1-T6), inline verbatim (полный блок из Script_Team.md), inline hotkeys/glossary/chronology (full entries, не summary), out-of-scope обновлены на T1-T6 slot names, Common+INDEX (`DefenseCompetencyFAQ.md`) удалён, `DefenseCompetencyFAQ_le1t.md` → `DefenseCompetencyFAQ_T2.md`, T2.md расширен с 12 до 33 Q&A. 8 files changed, +1700/-1464 строк. Размеры: T1=244, T2=591, T3=322, T4=359, T5=318, T6=346 (2180 total). |
| `2026-06-17` | **Defense root docs archive r0** | **closed `831f897`** (см. `agent/active-sessions.md session-2026-06-17T-defense-root-docs-archive-r0`): inline всех 23 алгоритмов + FAQ Q&A + report секций (ТЗ compliance matrix, команда §12, defense questions §10) в FAQ_T{1..6}. 3 root-level defense docs (DefenseReport v1.2 / DefenseFAQ / DefenseAlgorithms с устаревшим F11 и битыми refs на удалённые briefers) → legacy/docs/archive/DefenseOldFormat_2026-06-17/ через git mv (NO file content edits per operator: «legacy ты никогда не обновляешь»). FAQ_T* 2180 → 3306 строк (+1126 detail inline). 10 files changed (7 modified + 3 git mv renames 100%), +1265/-110 строк. Source code проверен (`src/voxel/VoxelWorld.cpp:1284-1643` fluid CA, `src/audio/AudioEngine.cpp:85-100` формат, `src/render/SceneResources.hpp:374-407` visibility cache hash, `src/physics/PhysicsWorld.hpp:19-40` walk debug info, `src/render/ShadowProjection.cpp:17-23` cascade constants). Safety-net patch `/tmp/before_archive_root_2026-06-17T0828Z.patch` (124 KB). |

## §32. Defense competency FAQ self-contained r0 — `b0feee8` (closed 2026-06-17T07:47Z)

**Snapshot:** финальная структура per-slot FAQ перед inline-archive:
- `docs/DefenseCompetencyFAQ_T{1..6}.md` (6 файлов, 2180 строк итого) — renumber по speech slot
- `docs/DefenseScript_Team.md` — verbatim source of truth (5 мин)
- `docs/DefensePresentation_Structure.md` — структура 8 слайдов
- `docs/DefenseReport.md` / `DefenseFAQ.md` / `DefenseAlgorithms.md` — root-level docs (устаревшие, см. §33)

## §33. Defense root docs archive r0 — `831f897` (closed 2026-06-17T08:28Z)

**Snapshot после commit `831f897`:**
- `docs/DefenseCompetencyFAQ_T{1..6}.md` — 3306 строк итого (2180 → 3306, +1126 detail)
  - T1: 244 → 306 (+62)
  - T2: 591 → 869 (+278) — le1t textbook: 33 Q&A + 23 algorithms overview + DOD/ECS/hot-cold/tech choice/architecture diagram/ТЗ matrix/команда
  - T3: 322 → 738 (+416) — voxel textbook: 9 algorithms (1, 2, 3 FULL greedy meshing, 4, 5, 13 FULL fluid CA, 14, 19, 20)
  - T4: 359 → 581 (+222) — render textbook: 6 algorithms (6 FULL CSM, 7 FULL PCF, 8, 9, 10 FULL TAA, 11 FULL ray-march)
  - T5: 318 → 393 (+75) — asset+audio: 2 algorithms (16 asset pipeline FULL, 17 audio engine FULL)
  - T6: 346 → 419 (+73) — physics: 2 algorithms (12 walk controller FULL, 15 Jolt integration)
- `docs/DefenseScript_Team.md` — broken ref fix: `DefenseCompetency_FAQ.md` → `DefenseCompetencyFAQ_T{1..6}.md`
- `legacy/docs/archive/DefenseOldFormat_2026-06-17/DefenseReport.md` — git mv (10-мин v1.2, 2026-06-15, §12 → удалённые briefers)
- `legacy/docs/archive/DefenseOldFormat_2026-06-17/DefenseFAQ.md` — git mv (40+ Q&A, 10-мин)
- `legacy/docs/archive/DefenseOldFormat_2026-06-17/DefenseAlgorithms.md` — git mv (§18 F11 устарело, line 5/1021 → удалённый briefer)

**Coverage check:** все 23 алгоритма + все 40+ FAQ Q&A + report §1-§12 inline в FAQ_T*.

Cross-refs на архив полных версий: `agent/ARCHIVE-INDEX.md` (single source of truth для navigation).

| `2026-06-17` | **Defense presentation restructure r0** | **closed `f1b92a6`** (см. `agent/active-sessions.md session-2026-06-17T-defense-presentation-restructure-r0`): `docs/DefensePresentation_Structure.md` 8 → 13 слайдов (102 → 872 строк, +770). Каждый слайд описан в 5 секциях: визуальная структура (LaTeX Beamer header/subheader/body/footer), body content (verbatim LaTeX), speaker notes (verbatim речь), тайминг (секунды), источник данных (для traceability). Hand-off notes для LaTeX/PDF экспорта (Madrid theme, tabularx/booktabs, qrcode, \begin{notes}). `docs/DefenseScript_Team.md` slot mapping обновлён под 13 слайдов: T1 [1,2,3], T2 le1t [4,5,6], T3 [7,8], T5 [9,10], T6 [11,12], le1t [13]. 8/8 блоков критериев п.6 на 81-100%. Exa search research: Minecraft (Java+OpenGL→Vulkan 2026+), Minetest (C++17+OpenGL), VoxelCore (C++17+OpenGL), Veloren (Rust+Vulkan, RPG focus), VIXEN/Garden/Shroom/Enigma (M0-M5 milestones). Пробел ниши: ни один open-source voxel не сочетает DOD + Vulkan 1.4 + compute + C++26 в воспроизводимом фундаменте. Safety-net patch: `/tmp/before_presentation_restruct_2026-06-17T1033Z.patch` (84 KB). |

## §34. Defense presentation restructure r0 — `f1b92a6` (closed 2026-06-17T10:33Z)

**Снимок после commit `f1b92a6`:**
- `docs/DefensePresentation_Structure.md` (872 строк) — 13 слайдов, каждый в 5 секциях (visual structure / body content verbatim / speaker notes verbatim / timing / data source). Hand-off для LaTeX Beamer экспорта в следующей сессии
- `docs/DefenseScript_Team.md` (124 строки) — slot mapping обновлён под 13 слайдов, verbatim тексты речи сохранены

**Coverage against criteria п.6 (8 blocks × 81-100% target):**
- ✓ Проблема и ценность (Слайд 2: Кто/Что/Почему)
- ✓ Цели и спецификации (Слайд 3: 48 пунктов ТЗ + 5 критериев)
- ✓ Обоснование решения (Слайд 5: таблица 5 аналогов по 6 критериям + пробел ниши)
- ✓ Реализация и прототип (Слайды 4, 6, 7: Demo + Архитектура + Реализация)
- ✓ Испытания и верификация (Слайды 8, 10: 14 ctest + 6 smoke + метрики с подписями)
- ✓ Ограничения, риски, этика (Слайд 11: 5 deferred + BUG-005 + ТЗ 4.5.4)
- ✓ Качество защиты (13 слайдов, 4:30+30с буфер)
- ✓ Командная работа (Слайд 12: таблица с личным вкладом каждого)

**13 слайдов → 5 мин распределение:**
| Slot | Хрон | Слайды | Спикер |
|------|------|--------|--------|
| T1 | 0:00-0:55 | 1, 2, 3 | Тиммейт 1 |
| T2 le1t | 0:55-2:20 | 4, 5, 6 | le1t |
| T3 | 2:20-3:00 | 7, 8 | Тиммейт 2 |
| T5 | 3:00-3:55 | 9, 10 | Тиммейт 5 |
| T6 | 3:55-4:25 | 11, 12 | Тиммейт 4 |
| le1t | 4:25-4:30 | 13 | le1t |
| (буфер) | 4:30-5:00 | — | — |

**Hand-off для следующей сессии (LaTeX/PDF):** оператор загружает `.md` в LaTeX Beamer document class + Madrid theme → получает готовый PDF для защиты 2026-06-17.

Cross-refs на архив полных версий: `agent/ARCHIVE-INDEX.md` (single source of truth для navigation).

| `2026-06-17` | **Defense Script_Team v2 r0** | **closed `2e3cd3e`** (см. `agent/active-sessions.md session-2026-06-17T-defense-script-team-v2-r0`): fix Script_Team структура (1 абзац на слайд). T1: +3-й абзац (Цели), T2 le1t: Аналоги 80→35 + Архитектура 100→60 слов, T3: +2-й абзац (Тесты), T5: +2-й абзац (Метрики), T6 = slides 11-12-13 (35s, Тиммейт 4 + le1t, drop дубль «Спасибо за внимание»). FAQ_T{1,2,3,5,6}.md §1 Verbatim sync с Script_Team.md (T4 §1 = резервный slot, без изменений). Presentation_Structure.md distribution table обновлён: T3 2:20-3:05, T5 3:05-3:55, T6 3:55-4:30. Slide 11/12/13 speaker notes sync. 7 files changed, +337/-179. Safety-net: `/tmp/before_script_team_v2_2026-06-17T1133Z.patch`. |

## §35. Defense Script_Team v2 r0 — `2e3cd3e` (closed 2026-06-17T11:33Z)

**Снимок после commit `2e3cd3e`:**

**Script_Team.md (134 строк) — структура «1 абзац на слайд»:**
| Slot | Хрон | Слайды | Абзацев | Спикер(ы) |
|------|------|--------|---------|-----------|
| T1 | 0:00-0:55 (55s) | 1, 2, 3 | 3 | Тиммейт 1 |
| T2 le1t | 0:55-2:20 (85s, demo 35s) | 4, 5, 6 | 3 | le1t |
| T3 | 2:20-3:05 (45s) | 7, 8 | 2 | Тиммейт 2 |
| T5 | 3:05-3:55 (50s) | 9, 10 | 2 | Тиммейт 5 |
| T6 | 3:55-4:30 (35s) | 11, 12, 13 | 3 | Тиммейт 4 + le1t |
| — | 4:30-5:00 | — | — | Буфер |

**FAQ_T*.md §1 sync:**
- T1 §1: +3-й абзац (Цели) ✓
- T2 §1: rewrite для slides 4-5-6 ✓
- T3 §1: +2-й абзац (Тесты) ✓
- T4 §1: без изменений (резервный slot)
- T5 §1: +2-й абзац (Метрики) ✓
- T6 §1: rewrite для slides 11-12-13 ✓

**Pattern:** каждый slot имеет «1 абзац на слайд» с маркерами **[Переход на X слайд]**. Новый абзац в slot (т.е. для нового слайда) стартует с «Здравствуйте». Это применено ко всем 5 slots.

**Slot duration verification:** 55 + 85 + 45 + 50 + 35 = 270 sec = 4:30 ✓ + 30 sec buffer ✓

Cross-refs на архив полных версий: `agent/ARCHIVE-INDEX.md` (single source of truth для navigation).

| \`2026-06-17\` | **Defense Script_Team renumber r0** | **closed \`03eb4d3\`** (см. \`agent/active-sessions.md session-2026-06-17T-defense-script-team-renumber-r0\`): slots T1-T6 переименованы строго = Участник 1-6. T1=Тиммейт1: slides 1-2-3, T2=Тиммейт2: slides 4-5 (Demo+Аналоги), T3=Тиммейт3: slides 6-7 (Архитектура+Voxel), T4=Тиммейт4: slide 8 (Тесты), T5=Тиммейт5: slides 9-10, T6=le1t: slides 11-12-13 (Ограничения+Команда+Закрытие). Each «Здравствуйте» 1× per slot. T1 schizophrenia (slide 2 promise demo+arch) REMOVED. FAQ_T{1..6}.md §1 Verbatim полностью переписан под новый slot mapping. FAQ_T6 §1 split into 3 quoted blocks. Presentation_Structure.md distribution table: added T4 row, renumbered timings. 8 files changed, +118/-109. Final sync: 6/6 slots match. |

## §36. Defense Script_Team renumber r0 — \`03eb4d3\` (closed 2026-06-17T12:11Z)

**Critical fix per operator:** Slot T_N must strictly = Участник N (1-6 in order).

**Slot distribution after fix:**

| Slot | Участник | Хрон | Слайды | «Здравствуйте» count |
|------|----------|------|--------|---------------------|
| T1 | 1 (Тиммейт 1) | 0:00-0:50 (50s) | 1, 2, 3 | 1 |
| T2 | 2 (Тиммейт 2) | 0:50-1:50 (60s) | 4, 5 | 1 |
| T3 | 3 (Тиммейт 3) | 1:50-2:40 (50s) | 6, 7 | 1 |
| T4 | 4 (Тиммейт 4) | 2:40-3:15 (35s) | 8 | 1 |
| T5 | 5 (Тиммейт 5) | 3:15-4:00 (45s) | 9, 10 | 1 |
| T6 | 6 (le1t) | 4:00-4:30 (30s) | 11, 12, 13 | 1 |
| — | Буфер | 4:30-5:00 | — | — |

**Total: 50+60+50+35+45+30 = 270 sec = 4:30 ✓ + 30с буфер ✓**

**Files changed:** 8 (Script_Team.md rewrite + 6 FAQ_T*.md §1 syncs + Presentation_Structure.md), +118/-109 строк.

**Final sync verification:** 6/6 slots match between Script_Team.md and FAQ_T*.md §1 (with marker-stripping normalization).

Cross-refs на архив полных версий: \`agent/ARCHIVE-INDEX.md\` (single source of truth для navigation).


## §37. Defense clean-slate script rewrite r0 — `ef8b942` (closed 2026-06-17T12:50Z)

**Per operator «Нет, плохо всё. Перепиши, как порекомендовал другой агент».** Full clean-slate rewrite of `DefenseScript_Team.md` + `DefensePresentation_Structure.md` из текста оператора.

**5 фиксов по критериям оператора:**
1. **Problem justification** привязана к CPU physics / OpenGL limits / отсутствию low-level open альтернатив (не абстрактные «open-source» утверждения).
2. **T3 transition fix** — «Передаю слово» строго в конце slot, не между слайдами одного спикера.
3. **«Здравствуйте»** ровно 1× per slot (T1..T6).
4. **Требования↔тесты aligned** — ThinLTO/Fluid CA/рендеринг метрики привязаны к спецификациям (ELF 19MB release / ctest 14/14 / smoke 6/6 / 38 из 48 ТЗ закрыты).
5. **Balanced timing** ~110-130 слов/мин, slot distribution неизменна (50+60+50+35+45+30=270с = 4:30 + 30с буфер).

**Script_Team.md (110 строк, было 148):**
| Slot | Участник | Хрон | Слайды | «Здравствуйте» count |
|------|----------|------|--------|---------------------|
| T1 | 1 (Тиммейт 1) | 0:00-0:50 (50s) | 1, 2, 3 | 1 |
| T2 | 2 (Тиммейт 2) | 0:50-1:50 (60s) | 4, 5 | 1 |
| T3 | 3 (Тиммейт 3) | 1:50-2:40 (50s) | 6, 7 | 1 |
| T4 | 4 (Тиммейт 4) | 2:40-3:15 (35s) | 8 | 1 |
| T5 | 5 (Тиммейт 5) | 3:15-4:00 (45s) | 9, 10 | 1 |
| T6 | 6 (le1t) | 4:00-4:30 (30s) | 11, 12, 13 | 1 |
| — | Буфер | 4:30-5:00 | — | — |

**Presentation_Structure.md (541 строка, было 1006):** 13 слайдов в LaTeX Beamer-стиле (Madrid theme). Все 5 секций на слайд: визуальная структура (Beamer рекомендация) / body LaTeX (`\frametitle{}` `\framesubtitle{}` `columns` `itemize` `tabular` `block`) / speaker notes (verbatim) / тайминг / источник данных. Все символы экранированы: `\\_` `\&` `\
## §37. Defense clean-slate script rewrite r0 — `ef8b942` (closed 2026-06-17T12:50Z)

**Per operator «Нет, плохо всё. Перепиши, как порекомендовал другой агент».** Full clean-slate rewrite of `DefenseScript_Team.md` + `DefensePresentation_Structure.md` из текста оператора.

**5 фиксов по критериям оператора:**
1. **Problem justification** привязана к CPU physics / OpenGL limits / отсутствию low-level open альтернатив (не абстрактные «open-source» утверждения).
2. **T3 transition fix** — «Передаю слово» строго в конце slot, не между слайдами одного спикера.
3. **«Здравствуйте»** ровно 1× per slot (T1..T6).
4. **Требования↔тесты aligned** — ThinLTO/Fluid CA/рендеринг метрики привязаны к спецификациям (ELF 19MB release / ctest 14/14 / smoke 6/6 / 38 из 48 ТЗ закрыты).
5. **Balanced timing** ~110-130 слов/мин, slot distribution неизменна (50+60+50+35+45+30=270с = 4:30 + 30с буфер).

**Script_Team.md (110 строк, было 148):**
| Slot | Участник | Хрон | Слайды | «Здравствуйте» count |
|------|----------|------|--------|---------------------|
| T1 | 1 (Тиммейт 1) | 0:00-0:50 (50s) | 1, 2, 3 | 1 |
| T2 | 2 (Тиммейт 2) | 0:50-1:50 (60s) | 4, 5 | 1 |
| T3 | 3 (Тиммейт 3) | 1:50-2:40 (50s) | 6, 7 | 1 |
| T4 | 4 (Тиммейт 4) | 2:40-3:15 (35s) | 8 | 1 |
| T5 | 5 (Тиммейт 5) | 3:15-4:00 (45s) | 9, 10 | 1 |
| T6 | 6 (le1t) | 4:00-4:30 (30s) | 11, 12, 13 | 1 |
| — | Буфер | 4:30-5:00 | — | — |

**Presentation_Structure.md (541 строка, было 1006):** 13 слайдов в LaTeX Beamer-стиле (Madrid theme). Все 5 секций на слайд: визуальная структура (Beamer рекомендация) / body LaTeX (`\frametitle{}` `\framesubtitle{}` `columns` `itemize` `tabular` `block`) / speaker notes (verbatim) / тайминг / источник данных. Все символы экранированы: `\_` `\&` `\%`, math в `$..$`.

**FAQ_T{1..6}.md §1 Verbatim sync:** 6/6 SLOTS IN SYNC ✓ — Python verification script `/tmp/verify_faq_sync.py` (normalize для `> ` blockquote markers, `**[Переход]**` markers, whitespace collapse).

| File | §1 chars | Slot | Status |
|------|----------|------|--------|
| `docs/DefenseCompetencyFAQ_T1.md` | 1060 | T1 | ✓ MATCH |
| `docs/DefenseCompetencyFAQ_T2.md` | 931 | T2 | ✓ MATCH |
| `docs/DefenseCompetencyFAQ_T3.md` | 1000 | T3 | ✓ MATCH |
| `docs/DefenseCompetencyFAQ_T4.md` | 517 | T4 | ✓ MATCH |
| `docs/DefenseCompetencyFAQ_T5.md` | 904 | T5 | ✓ MATCH |
| `docs/DefenseCompetencyFAQ_T6.md` | 875 | T6 | ✓ MATCH |

**Files changed:** 8 (Script_Team.md 110 + Presentation_Structure.md 541 + 6 FAQ_T*.md §1 edits), +546/-1049 строк.

**Pre-commit gate (§7.3.1):** type=`docs` → auto, §7.2.5 message готов, scope discipline clean (only my 8 files staged).

**Build state:** docs-only, baseline preserved.

**Safety-net:** `/tmp/before_cleanslate_script_2026-06-17T1235Z.patch` (13 строк исходного dirty diff, footer `POST-COMMIT ef8b942`).

Cross-refs: `AGENTS.md` §7.2.5, §7.2.6.1, §7.2.8, §7.3.1, §7.4, §8.1; `docs/DefenseScript_Team.md` (authoritative verbatim); `docs/DefensePresentation_Structure.md` (LaTeX Beamer-ready); `agent/active-sessions.md` session-2026-06-17T-defense-cleanslate-script-r0.

## §38. Defense LaTeX Beamer PDF r0 — `b221d1f` (closed 2026-06-17T13:15Z)

**Per operator «Теперь делай презентацию»** — LaTeX Beamer compilation of `DefensePresentation_Structure.md` → готовый PDF deliverable для защиты 2026-06-17.

**Deliverable:** `docs/tex/defense/DefensePresentation.pdf` (13 страниц, 16:9, 205 KB).

**Инфраструктура (новое, `docs/tex/defense/`):**

| Файл | Назначение |
|------|-----------|
| `header.tex` | Beamer preamble: Madrid theme + Liberation Sans с polyglossia:russian, projectvblue/projectvgray colors, qrcode package, navigation symbols off |
| `DefensePresentation.tex` | 13 фреймов (title с QR + Problem + Goals + VoxelLab demo + Аналоги + Архитектура + Voxel мир + Тесты + Фичи + Метрики + Ограничения + Команда + Закрытие) |
| `Makefile` | `latexmk -pdfxe -interaction=nonstopmode -halt-on-error`, цели all/notes/clean/clean-all |
| `screenshots/voxel_lab.png` | 1896×1034 RGB, конвертировано из `build/linux-clang-debug/lookdev-captures/2026-06-15-repo-root-walkup-test/0001.bmp` через PIL |
| `DefensePresentation.pdf` | Готовый deliverable, 13 страниц, 453.54×255.12 pt, 205 KB |

**Build verification:**
- `xelatex --version` → XeTeX 3.141592653-2.6-0.999998 (TeX Live 2026/Arch Linux)
- `latexmk -pdfxe` → `xdvipdfmx` pipeline, 13 страниц, 0 errors
- `pdfinfo` → Title: «ProjectV - Открытый высокопроизводительный воксельный движок», Author: «Команда <<Черепашки Ninja>>», 13 pages, PDF version 1.7
- Визуальная проверка через `pdftoppm` (slides 1, 4, 13): QR-code на титульном, VoxelLab screenshot в слайде 4, «Спасибо за внимание!» на закрытии

**Warnings:** minor overfull/underfull hbox в Beamer таблицах (типично), нет блокеров.

**`.gitignore`:** добавлены паттерны для LaTeX artifacts (`*.aux`, `*.log`, `*.out`, `*.toc`, `*.nav`, `*.snm`, `*.fls`, `*.fdb_latexmk`, `*.xdv`, `*.bbl`, `*.blg`, `*.idx`, `*.ilg`, `*.ind`, `*.lof`, `*.lot`, `*.run.xml`, `*.vrb`).

**Multi-agent coordination note (per §7.2.6):** в процессе работы в working tree появились чужие uncommitted изменения:
- `docs/DefenseScript_Team.md` (line-wrap reformat, content identical)
- `docs/DefenseCompetencyFAQ_T3.md` (убрали «snapshot» из competency line, ~7 строк)

Обе модификации НЕ тронуты, оставлены нетронутыми для другой сессии per §7.2.6 «Не делать `git add -A`, не делать `git checkout -- <file>` для файлов вне scope».

**Использование для оператора:**
```bash
cd docs/tex/defense
make              # собрать PDF заново
make notes        # собрать версию с заметками спикера
make clean-all    # удалить все артефакты
```

Cross-refs: `AGENTS.md` §7.2.6, §7.3.1, §8.1; `docs/DefenseScript_Team.md` (verbatim); `docs/DefensePresentation_Structure.md` (структура); `docs/tex/defense/DefensePresentation.pdf` (deliverable).

## §39. Defense presentation patches r0 — `0aa863c` (closed 2026-06-17T13:35Z)

**Per operator «Плохо получилось, перепиши с учётом этого»** — применены 4 косметических фикса к LaTeX Beamer presentation (`b221d1f` → `0aa863c`).

**4 фикса:**

1. **Table overflow fix (Slides 3, 5, 12):** все таблицы обёрнуты в `\resizebox{\textwidth}{!}{...}` для динамического масштабирования — гарантированно влезают в текстовые границы кадра на экранах любого разрешения.

2. **Slide 1 (title) — удаление QR/репозитория:** QR-код + ссылка на GitHub репозиторий полностью удалены. Внизу титульного слайда — лаконичное описание окружения сборки: **«Окружение сборки: CMake 3.30+ • Clang 22 (C++26) • Vulkan SDK 1.4.350»**.

3. **Slide 12 (Команда) — академический booktabs стандарт:** таблица переведена с тяжёлых вертикальных рамок `|l|l|l|p{...}|` на элегантный booktabs (`\toprule`/`\midrule`/`\bottomrule`). Текст ячеек сокращён во избежание избыточной длины при масштабировании.

4. **Slide 10 (Метрики) — block `\scriptsize`:** блок «Условия проведения замеров производительности» теперь `\scriptsize` вместо `\small` для чистого размещения Ryzen+RTX спецификаций без overflow.

**Build verification:**
- `latexmk -pdfxe` + `xdvipdfmx` → 13 страниц, 203 KB
- Warnings: 1 minor overfull vbox (15.8pt, slide 4 image area, non-blocking)
- `pdfinfo` → Title: «ProjectV - Открытый высокопроизводительный воксельный движок», Author: «Команда <<Черепашки Ninja>>», 16:9 (453.54×255.12 pt)

**Visual verification (pdftoppm):**
- Slide 1: QR удалён, build env description внизу ✓
- Slide 3: таблица с `\resizebox` идеально вписывается ✓
- Slide 5: таблица аналогов влезает, ProjectV подсвечен ✓
- Slide 12: booktabs без вертикальных рамок, текст компактный ✓

**Pre-commit gate (§7.3.1):** type=`fix` → требуется operator confirm per §7.3.1 п.3. Operator confirm выполнен в текущей сессии через явное указание «Плохо получилось, перепиши с учётом этого» + визуальная верификация slides 1, 3, 5, 12 через pdftoppm.

**Multi-agent note (per §7.2.6):** uncommitted модификации `docs/DefenseScript_Team.md` (line-wrap) и `docs/DefenseCompetencyFAQ_T3.md` («snapshot» removed) оставлены нетронутыми — НЕ мои.

Cross-refs: `AGENTS.md` §7.2.6 (multi-agent), §7.3.1 (pre-commit gate type=fix), §8.1 (close-routine); `docs/tex/defense/DefensePresentation.pdf` (deliverable v2).

## §40. Defense presentation round 3 r0 — `341c6cf` (closed 2026-06-17T13:55Z)

**Per operator «Проблемы: ...» + «Ещё поменяй фото на это»** — round 3 patches к LaTeX Beamer presentation (`0aa863c` → `341c6cf`).

**5 фиксов:**

1. **Subtitle color (header.tex):** `\setbeamercolor{framesubtitle}{bg=projectvblue,fg=white}` (было bg=projectvgray,fg=black) — субтитры теперь белым на синем фоне.

2. **Slide 4 — уменьшен размер изображения** с 0.55 до 0.45\textwidth — текст «Жидкость (Fluid): ... отбрасывает тень.» помещается без overflow.

3. **Slide 11 — удалён раздел «Минимизация рисков (BUG-005)»:** остаются только «Отложенные требования (Роадмап)» + «Безопасность и правовой статус».

4. **Slide 12 — реальные имена + удалена колонка «Роль на сцене»:**
   - Тиммейт 1 → Черников Максим Андреевич (Сборка, тесты)
   - Тиммейт 2 → Бачерикова Анжелика Сергеевна (Воксельный мир)
   - Тиммейт 3 → Туз Максим Эдуардович (Рендеринг)
   - Тиммейт 4 → Крохалев Пётр Антонович (Физика движка)
   - Тиммейт 5 → Филипьев Иван Евгеньевич (Ассеты, звук)
   - le1t row сохранён «Кадочников Л. (le1t)» (оператор просил заменить только Тиммейтов 1-5)
   - Таблица: 3 колонки (Участник | Компетенция | Зона ответственности в коде)

5. **VoxelLab screenshot заменён:** `/home/le1t/Pictures/Screenshots/2026-06-17_18-16.png` (1920×1080 RGB, 153 KB) → `docs/tex/defense/screenshots/voxel_lab.png`.

**Build verification:**
- `latexmk -pdfxe` + `xdvipdfmx` → 13 страниц, 250 KB (вырос с 203 KB из-за нового скриншота)
- Warnings: 0 overfull/underfull errors

**Visual verification (pdftoppm):**
- Slide 2: subtitle «Для кого и почему это важно» — белый на синем ✓
- Slide 4: новый пользовательский скриншот, текст влезает ✓
- Slide 11: BUG-005 отсутствует, два раздела ✓
- Slide 12: 6 строк с реальными именами, 3 колонки ✓

**Self-correction note:** первоначально в slide 12 ошибочно переименовал le1t «Кадочников Леонид Петрович» (выход за scope оператора, который просил заменить только Тиммейтов 1-5). Исправлено перед коммитом — le1t row сохранён «Кадочников Л. (le1t)».

**Pre-commit gate (§7.3.1):** type=`fix` → operator confirm выполнен через явное указание «Проблемы: ...» + визуальная верификация через pdftoppm.

**Multi-agent note (per §7.2.6):** uncommitted модификации `docs/DefenseScript_Team.md` (line-wrap) и `docs/DefenseCompetencyFAQ_T3.md` («snapshot» removed) оставлены нетронутыми — НЕ мои.

Cross-refs: `AGENTS.md` §7.2.6, §7.3.1 (type=fix), §8.1; `docs/tex/defense/DefensePresentation.pdf` (deliverable v3).

## §41. Defense le1t name r0 — `538cc25` (closed 2026-06-17T14:05Z)

**Per operator «Поменяй меня на 12 слайде на Кадочников Лев Петрович, а не le1t».** Заменил «Кадочников Л. (le1t)» на полное ФИО «Кадочников Лев Петрович» в строке le1t таблицы слайда 12 (`341c6cf` → `538cc25`).

**Изменение:** 1 строка в `DefensePresentation.tex`, recompile → новый PDF.

**Visual verification (pdftoppm slide 12):** все 6 строк корректны:
- **Кадочников Лев Петрович** — Архитектура, Тимлид
- Черников Максим Андреевич — Сборка, тесты
- Бачерикова Анжелика Сергеевна — Воксельный мир
- Туз Максим Эдуардович — Рендеринг
- Крохалев Пётр Антонович — Физика движка
- Филипьев Иван Евгеньевич — Ассеты, звук

**Консистентность:** `docs/DefenseCompetencyFAQ_T2.md` line 4 уже содержал «Кадочников Лев Петрович — ведущий, тимлид, Q&A host» — изменение привело presentation в соответствие с FAQ.

**Build:** latexmk -pdfxe + xdvipdfmx, 13 pages, 250 KB.

**Pre-commit gate (§7.3.1):** type=`fix` → operator confirm выполнен через явное указание «Поменяй меня на 12 слайде на Кадочников Лев Петрович» + визуальная верификация.

**Multi-agent note (per §7.2.6):** uncommitted модификации `docs/DefenseScript_Team.md` (line-wrap) и `docs/DefenseCompetencyFAQ_T3.md` («snapshot» removed) оставлены нетронутыми.

Cross-refs: `AGENTS.md` §7.2.6, §7.3.1 (type=fix), §8.1; `docs/tex/defense/DefensePresentation.pdf` (deliverable v4); `docs/DefenseCompetencyFAQ_T2.md` (консистентное ФИО в line 4).
