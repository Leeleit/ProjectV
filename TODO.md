# TODO.md

Актуальная дорожная карта `ProjectV`.

Дата обновления: `2026-04-21`
Статус документа: `живой roadmap`

---

## 1. Mainline

Mainline `ProjectV` сейчас — это reproducible interactive voxel MVP.

Что уже есть в коде:

- runnable voxel slice на `Vulkan + SDL + Jolt`;
- `creative` / `spectator` / `walk`;
- voxel world, dirty chunks, meshing, frustum/distance culling;
- block interaction, snapshots, lightweight debug editor;
- HUD, Tracy, runtime smoke и failure probes;
- рабочий, но ещё тюнингуемый `walk`-контроллер.

Что не должно становиться блокером mainline:

- `SVO`;
- mesh shaders;
- bindless-everything;
- тяжёлая simulation R&D;
- большой editor;
- multiplayer;
- plugin/modding stack.

---

## 2. Текущий Milestone

Ближайший честный milestone:

- стабильный интерактивный voxel sandbox slice;
- repeatable `configure/build/test/smoke` loop;
- world edit + snapshots + lightweight debug tools;
- walk/controller feel без грубых runtime regressions;
- документация синхронизирована с кодом и `agent/`.

Критерий готовности milestone:

- проект легко запускается и проверяется;
- текущие control/debug loops не выглядят хрупкими;
- следующий gameplay/debug слой можно добавлять без новой документной или архитектурной зачистки.

---

## 3. Активные Приоритеты

### P0

- [ ] Prefer replay-capture-driven walk regressions over new handwritten `SendKeyEvent` scripts whenever a live runtime bug diverges from synthetic fixtures.

- [ ] Finish the current refactor/lint/static-analysis sweep before starting the next gameplay/debug slice; during that pass prefer warning cleanup, invariant fixes, and code-health follow-ups over new feature work.
- [ ] Re-export `Problems/` before the next warning-cleanup pass; the checked-in JetBrains XML is a point-in-time snapshot and should not be treated as ground truth after more code edits.
- [ ] Держать `TODO.md`, `AGENTS.md` и `agent/` короткими, role-based и без дублей.
- [ ] Решить, нужен ли self-hosted/headless runtime smoke в CI, или текущий GUI smoke остаётся developer-only.
- [ ] Держать `walk` / `creative` controller work narrow and replay-driven: после закрытия текущего collision-gestalt не возвращаться к broad heuristics без нового live repro, HUD/Tracy evidence и точного regression capture.

### P1

- [ ] После текущего refactor/lint/static-analysis pass взять следующий gameplay/debug slice поверх уже существующего sandbox; приоритетный первый кандидат:
  - `inspect tools`;
- [ ] Следующими кандидатами после `inspect tools` остаются:
  - simple sandbox interactions;
  - debug world-mutation helpers.
- [ ] Дотюнить `walk` feel только в узких местах:
  - auto-jump / `F12` delay;
  - `MinecraftLike` air-control;
  - creative flight polish;
  - player-controller polish.
- [ ] Добавлять новые HUD/debug counters только там, где они реально помогают runtime diagnosis.

### P2

- [ ] Начать lighting/shadow foundation-first контур:
  - HDR / exposure / material contract;
  - sun shadows;
  - local shadow/contact-occlusion;
  - reflections / atmosphere;
  - temporal stabilization;
  - quality/debug ladder.

---

## 4. Mainline Backlog

### Gameplay / Debug

- [ ] inspect tools;
- [ ] simple sandbox interactions;
- [ ] debug tools for world mutation;
- [ ] screenshot hotkey;
- [ ] frame-step / slow-motion debug modes;
- [ ] gizmo/debug overlays.

### World / Render / Tooling

- [ ] richer chunk model;
- [ ] world editing tools;
- [ ] greedy meshing follow-up;
- [ ] render stats / per-pass timings / chunk update timings;
- [ ] RenderDoc-friendly markers;
- [ ] benchmark automation.

### Visual Quality

- [ ] HDR / tone mapping / exposure;
- [ ] physically coherent material/lighting contract;
- [ ] cascaded sun shadows;
- [ ] local lights + local shadows;
- [ ] `SSAO/GTAO`;
- [ ] `SSR`;
- [ ] volumetric fog / shafts;
- [ ] `TAA` / temporal stabilization;
- [ ] quality tiers and debug views.

---

## 5. R&D Backlog

Эти темы не блокируют ближайший milestone:

- [ ] `SVO` и альтернативные voxel representations;
- [ ] mesh shaders / GPU-driven rendering / visibility buffer;
- [ ] hardware RT shadows / reflections / GI;
- [ ] large-world streaming / origin shifting / LOD;
- [ ] job system / heavy simulation;
- [ ] destruction playground;
- [ ] большой editor / plugin stack / multiplayer.

---

## 6. Риски

- Документация и `agent/` быстро дрейфуют, если после задачи обновить только код.
- В `build/windows-clang-debug` нельзя гонять несколько `build/test/smoke` параллельно.
- `walk` легко регрессирует от широких эвристик; для него приоритетны live repro, fixed-step tests, HUD и Tracy.
- `README.md`, vendored submodules и часть `docs/` могут содержать user-owned изменения; incidental edits нежелательны.

---

## 7. Недавние Закрытия

Держать здесь только крупные факты, которые влияют на ближайший roadmap:

- [x] `TODO.md` / `AGENTS.md` / `agent/` снова синхронизированы по ролям и очищены от длинного исторического дубляжа;
- [x] snapshots (`F6/F7`);
- [x] lightweight debug editor (`F8/F9/F10`);
- [x] repeatable build/test/smoke contour;
- [x] walk live HUD + Tracy diagnostics;
- [x] dual air-control modes (`F11`);
- [x] placement/body overlap guard;
- [x] delayed auto-jump toggle (`F12`);
- [x] held-jump repeat restored.
- [x] moving narrow-edge traversal with held `W` no longer drops `walk` into synthetic `Air`; jump can still commit from partial edge support (`5.tracy` case).
- [x] exact first jump press on the thinnest edge support no longer depends on pre-tick `supportState`: the replay fixture now proves the jump still launches from remaining support hits, while ordinary walk-off without jump is no longer kept alive by the same rule.
- [x] jump landing back onto recent takeoff-plane no longer stays `Air` on the support plane and then drops late (`6.tracy` case).
- [x] author-refactor fallout no longer breaks `PhysicsWorld.cpp` build; Jolt include order restored and `build/test/smoke` are green again.
- [x] crouch-jump into a glass column no longer turns side-wall voxels into fake sneak support; `Shift` wall-slide regression is covered by fixed-step tests and `build/test/smoke` stay green.
- [x] active ballistic jump can no longer reuse cached ground-takeoff grace for a second airborne jump; the `7.tracy` retry-jump path is now covered by a state-driven fixed-step regression instead of a guessed frame.
- [x] successful `SyncPhysicsWorld` after voxel edits now invalidates cached walk support/anchors before the next tick, so removed support cannot survive only as stale walk ownership.
- [x] Runtime input replay now exists for walk bugs: `R` records snapshot + per-frame input into the latest replay file, `Y` replays it in-game, and tests can load the same capture instead of rebuilding long manual key scripts.
- [x] replay-driven crouch wall-cling no longer authorizes grounded support at the caller's midair `feetY`; sneak support is now anchored to the sampled top-plane and the strengthened two-block regression stays green.
- [x] replay-driven stacked placed-block wall climb no longer reacquires foreign ground-takeoff planes or midair crouch support from too far below the support plane; the same live replay now stays at `feetY=1.050` instead of climbing to `2.050/3.050`.
- [x] replay-driven boosted creative flight through `TransparencyStress` no longer wedges on glass columns or exact glass-corner hits at high speed; `TickCreativeCharacter` now substeps long boosted travel more aggressively and the exact captures live in `tests/fixtures/creative_transparency_boost_stuck.*` and `creative_transparency_boost_corner_stuck.*`.
- [x] Problems-driven warning cleanup removed the remaining current `clang-tidy` findings around `InputActions`, `Types`, `EcsWorld`, and `PhysicsWorld`; `build -> tests -> smoke` is green again, and the next warning pass now has an explicit reminder to regenerate `Problems/` instead of trusting stale XML line numbers.
- [x] refreshed JetBrains `Problems/` export no longer points at the old stale-pointer helpers in `PhysicsWorld.cpp`: internal walk helpers now use reference contracts where `nullptr` was never meaningful, dead descending-ledge/jump-lock DFA-only paths were removed, and `build -> tests -> smoke` remains green after the refactor.
- [x] follow-up `Problems/` + `problems/tools/` cleanup removed the remaining live switch/default warning in `PhysicsWorld.cpp`, collapsed several test-only inspection nits in `VoxelWorldTests.cpp` (bitmask helpers, constexpr/deduced arrays, structured bindings, integer scan loop), and kept `build -> tests -> smoke` green; the leftover `CppDFAUnreachableFunctionCall` rows in the checked-in tools export still need a fresh re-export because the current `main()` already calls those tests directly.
