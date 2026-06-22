# STATUS — 2026-06-22-procedural-weapon-fire-vfx-particle-system

**Status:** `concluded-verdict-mixed` (per strategy; `yes` for D + E as recommended defaults)
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22 (single session, ~1.5h, claim + research + prototype + bench + close)

---

## Last action

- **2026-06-22 02:30** — Closed per `AGENTS.md §13.5` sync to backlog + INDEX.
  - Prototype complete: `prototype/vfx_bench.cpp` ~570 LoC + `prototype/CMakeLists.txt`.
  - Build: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **green 0 warnings**.
  - Bench: 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time < 1 sec.
  - Output: `build/results.csv` (125 rows = 1 header + 124 data, ~7 KB).
  - **Headline (mean across 5 scenes):** A=1.53% 30Hz Q=0.40; B=3.44% Q=0.70; C=4.69% Q=0.90; D=**0.15% Q=0.60**; E=**2.90% Q=0.85 ⭐**.
  - **Recommended defaults:** E (production), B (close-LOD high-density), D (far-LOD fallback), C (short-duration high-quality events), A (legacy).
  - 4-clause hypothesis: ✅ H1, ⚠️ H2 (PARTIAL), ✅ H3, ✅ H4.
  - All 5 strategies within 5-10% threshold of 30Hz on mean.

## Next action

- None — experiment closed. **Integration deferred до Stage 5.x dedicated session + Stage 6+ military sandbox activation per `agent/workspace.md §2` operator 8x planning decision.**

## Blockers

- None.

## Anti-duplicate verification (§13.7)

`rg "particle|vfx|muzzle|smoke|spark"` over `docs/experiments/`:

- `2026-06-21-data-driven-vehicle-weapon-definitions/prototype/smoke.cpp` — это smoke test (test for verify A/B load), не VFX. "muzzle" встречается только как `muzzle_velocity` (характеристика оружия).
- `2026-06-21-vk-fragment-shading-rate-voxel/{README,sources}.md` — VRS в контексте fragment shading, не VFX.
- `2026-06-21-voxel-hydraulic-erosion/{README,STATUS}.md` — hydraulic erosion particle, не weapon VFX.
- `2026-06-22-squad-fire-team-command/{README,RESULTS}.md` — VFX mentioned в context of "fire team" (tactical unit, не weapon fire visual).

Mainline `/src/` — zero VFX code per `rg "particle|smoke|spark|muzzle|trail"`.
Все 136+ closed experiments — zero по dedicated VFX / particle / muzzle-flash / impact-sparks axis (кроме моего нового).

**Verdict: §13.7 sentinel clean. Self-invented axis. New axis opened.**

## Cross-axis notes

- **orth ко всем 136+ closed + ~3 in-progress parallel** (verified via `rg`).
- **complementary** к closed `destructible-building-system` + `chunk-damage-fracture-model` + `explosion-crater-terrain-deformation` + `ballistic-projectile-simulation` + `ballistic-crack-thump` (these produce VFX trigger events).
- **complementary** к closed `mesh-shader-mega-instancing` (instanced rendering host) + `dynamic-entity-lighting` (muzzle flash dynamic light = orth sub-feature) + `cloudscape-rendering` (sky volumetric = scene-scale orth) + `eye-tracked-foveated` (VRS bandwidth reduction).

## Sync (per §13.5)

- [x] `experiments/2026-06-22-procedural-weapon-fire-vfx-particle-system/{README,STATUS}.md` created + filled.
- [x] `prototype/vfx_bench.cpp` (~570 LoC) + `prototype/CMakeLists.txt` + `build/results.csv` (125 rows).
- [x] `sources.md` created (13 primary + 4 supplementary sources verified).
- [x] `RESULTS.md` created (per-strategy + per-scene + hypothesis validation + cross-vendor matrix + 5-10% threshold).
- [x] `research/backlog.md` updated: claim запись в §In progress.
- [x] `INDEX.md` updated: §5 Active row.
- [x] `research/backlog_closed.md` updated: move to §Closed. ✅ Done.
- [x] `INDEX.md` updated: §6 Recent closed row. ✅ Done.
- [x] Operate manual close verification: reread README + STATUS + RESULTS for completeness. ✅ Done.

**All sync operations complete. Experiment fully closed per `AGENTS.md §13.5`.**
