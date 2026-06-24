# STATUS — Wildfire Propagation

**Closed:** 2026-06-21 (single session, ~3h)
**Agent:** self
**Verdict:** `mixed` per strategy; `yes` for C_RothermelFuelModel_RD ⭐ as universal recommended default.

**Phase tracker:**
- Phase 0 (reservation per §13.1): DONE — `research/backlog.md §Open → §Closed` (reservation record) + `INDEX.md §5 Active` + folder `experiments/2026-06-21-wildfire-propagation/`.
  - **Sentinel §13.7 clean:** `rg "wildfire|fire-propagation"` → only orth cross-refs in `vegetation-destruction-interaction/README.md` (mention of fire in trees) + `vulkan-memory-aliasing-transient/sources.md` (mention of transient aliasing for fire data); `ls experiments/2026-06-21-wildfire*` = ENOENT before claim; `INDEX.md §5` = no parallel reservation. No dedicated wildfire/fire-CA axis in 130+ closed experiments.
- Phase 1 (skeleton README + STATUS): DONE.
- Phase 2 (web-research): DONE via direct `webfetch` to canonical Wikipedia URLs (Exa HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per the web_search fallback chain); **8 primary + 4 cross-reference sources verified** в `sources.md`: Wildfire modeling Wikipedia [Rothermel 1972, FARSITE, PROMETHEUS, WRF-Fire, FIRETEC, WFDS, Richards 1990 elliptical] + Forest-fire model Wikipedia [Drossel-Schwabl 1992 canonical CA] + Cellular automaton Wikipedia [Wolfram 1-4 classification, Game of Life] + Reaction-diffusion system Wikipedia [Fisher, Zeldovich-Frank-Kamenetskii combustion] + CFD Wikipedia [Rothermel parameterization] + Far Cry 2 Wikipedia [Dunia engine, fire-spreading reactive environment] + Teardown voxel production reference [Tuxedo Labs 2022 physical fire propagation] + Voxel game history.
- Phase 3 (prototype): DONE — `prototype/wildfire_bench.cpp` ~870 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings 0 errors** after 5 fix iterations: StrategyA no-op unused params `[[maybe_unused]]` + flat world arrays `w.fire[world_idx3]` instead of chunked `w.chunks[i].fire` + removed buggy Bresenham dy branch + added fallback `find_ignition` for scenes with no center-column fuel + added 1-cell halo to StrategyE bitmask).
- Phase 4 (build + run + collect results.csv): DONE — wall time **40 sec** on dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, ~16 KB).
- Phase 5 (write-up): DONE — `README.md`, `RESULTS.md`, `STATUS.md`, `sources.md` complete.
- Phase 6 (single-pass sync per §13.5: backlog §Closed + INDEX §6 + this STATUS): pending (next).

**Blocker:** нет.

**Outputs:**
- `prototype/wildfire_bench.cpp` (~870 LoC)
- `prototype/build/wildfire_bench` (~78 KB binary, Clang 22.1.6 -O3 -march=native)
- `prototype/build/results.csv` (126 rows × 10 cols)
- `README.md` (9 sections per §7 template)
- `RESULTS.md` (per-strategy × per-scene tables + interpretation)
- `STATUS.md` (this file)
- `sources.md` (8 primary + 4 cross-references)

**Headline numbers:**
- **C_RothermelFuelModel_RD ⭐ = universal recommended default** = 98,786 ns/tick mean (79k-134k range) = **99 µs/tick = 0.30% of 30 Hz budget**. 15% faster than B (116 µs), 61% cheaper than D (321 µs), 61% cheaper than E (255 µs). Physically motivated (Rothermel 1972 fuel model). Single-pass with deferred ignitions.
- **B_DrosselSchwabl_CA = fallback** = 116 µs/tick mean. Two-pass with scratch buffer; well-known Drossel-Schwabl 1992 CA semantics.
- **D_WindAdvectedCA_Bresenham3D = quality opt-in** = 321 µs/tick mean (3.2× C). Maintains active fire at end of 1000 ticks in dry windy/ammunition scenes (B/C/E exhausted fuel); physically correct ember-driven spread for sustained wildfires.
- **E_ChunkLazy_Bitmask = REJECTED** = 255 µs/tick mean (2.6× C). Bitmask overhead exceeds savings for typical scenarios; useful only for very concentrated fire (single fire pit in 64³ world).
- **A_NoFire = baseline** = 0.02 ns/tick. Zero work, zero false spread (correct for fire-disabled scenes).

**Sync (per §13.5) — pending this turn:**
- `backlog.md §Open → §Closed` (with full closure note + reservation record moved)
- `INDEX.md §5 Active → §6 Recent closed` (one-line table row + full §6 entry)
- This STATUS.md (closure note — present)
- `agent/workspace.md`: NOT in scope (this is for mainline agent per `docs/experiments/AGENTS.md §2`)

**Cross-axis (recap):**
- **orth** ко всем ~3 in-progress parallel (`tracy-gpu-vs-manual` profiling + ...); **complementary** к closed `gpu-fluid-ca-atomic-strategy` [mixed, CA methodology precedent] + `vegetation-destruction-interaction` [yes, ignition source] + `chunk-damage-fracture-model` [mixed, post-impact ignition] + `explosion-crater-terrain-deformation` [yes, fire-as-aftermath] + `destructible-building-system` [mixed, fire consumes structure] + `ballistic-projectile-simulation` [yes, incendiary ammo] + `countermeasure-dispenser` [mixed, flare] + `dynamic-entity-lighting` [mixed, fire as light] + `volumetric-fog-atmosphere-rendering` [mixed, smoke as fog] + `cloudscape-rendering` [mixed, smoke column rises into clouds] + `voxel-grass-foliage-rendering-pipeline` [mixed, foliage can burn] + `lockstep-state-sync-hybrid-netcode` [mixed, fire state must be deterministic] + `save-game-persistence-architecture` [mixed, fire state saved with chunk] + `data-driven-vehicle-weapon-definitions` [mixed, incendiary weapon def] + `tank-terrain-interaction-physics` [yes, vehicle drives through fire] + `helicopter-rotor-physics` [yes, rotor downwash spreads fire] + `aircraft-damage-model` [yes, fire damages aircraft]. **Prerequisite** для open `electronic-warfare-jamming` [m Tier 2, fire as IR signature] + `trench-fortification-construction` [m Tier 2, foxholes protect from fire] + `field-fortifications-system` [m Tier 2, fire breaks fortifications] + `battlefield-ambient-audio` [m Tier 4, fire crackling audio] + `squad-fire-team-command` [m Tier 2, fire-and-maneuver depends on fire-spread] + `flanking-maneuver-ai` [h Tier 2, fire as cover/blocker].
