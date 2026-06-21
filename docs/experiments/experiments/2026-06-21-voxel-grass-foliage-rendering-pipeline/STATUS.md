# STATUS — 2026-06-21-voxel-grass-foliage-rendering-pipeline

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~3 h)
**Stage link:** cross-cutting (Stage 4.1 world gen polish + Stage 5.x Visual Polish rendering axis)
**Estimated effort:** S (single session)
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй» 2026-06-21)

---

## Log

- `2026-06-21` — **claimed from backlog per `AGENTS.md §13.1`** (self-invented, NOT in §Open).
  Anti-duplicate sentinel §13.7 confirmed clean: `rg "grass|foliage|vegetation|wind.animation"`
  over `docs/experiments/` returns only scattered cross-references in 7+ closed experiments
  (e.g. `mesh-shader-mega-instancing` mentions "Vulkan Foliage 2024" as adjacent area,
  `procedural-military-terrain-gen` uses "rolling_hills" / "flat_grasslands" as scene names),
  NO dedicated `voxel-grass-foliage` folder pre-existed. `ls experiments/` confirms absence.
  `rg -c "grass" INDEX.md` = 1 (just the "flat_grasslands" scene name in one in-progress
  reference); `rg -c "foliage"` = 0; `rg -c "vegetation"` = 0. **First dedicated
  grass/foliage/vegetation rendering axis** в 100+ closed experiments.
- `2026-06-21` — **Phase 1 (Web research) complete**: 5 primary + 2 secondary sources
  verified with full content read (AMD GPUOpen mesh-shader grass March 2024, rcm7133
  Modern-Grass-Rendering 2026, GPU Gems Ch 7 Pelzer 2004, GPU Gems 3 Ch 6 Zioma DICE 2008,
  ReeCocho mesh-shaders 2024). See `sources.md`.
- `2026-06-21` — **Phase 2 (Prototype) complete**: standalone C++26 CPU analytical cost model
  `prototype/grass_bench.cpp` ~370 LoC, build green **0 warnings, 0 errors** (Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`).
- `2026-06-21` — **Phase 3 (Measurements) complete**: 6 biomes × 6 strategies × 5 seeds × 1000
  iters = **180,000 main measurements**, wall time ~5 ms on dev host `obvium` Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv`
  (181 rows = 1 header + 180 data, 36 unique configs).
- `2026-06-21` — **Phase 4 (Analysis) complete**: D (GPU instanced HLOD mesh) validated as
  universal default (0.20 ms = 0.6% budget, 0.85 quality). E (mesh shader Bezier) only viable
  for sparse biomes (rocky 0.6%, tundra 0.7%, forest 8% borderline). F (hierarchical) not
  a clear win (mesh shader dispatch overhead dominates at high density).
- `2026-06-21` — **closed `concluded-verdict-mixed`**.

---

## State

- **Slug:** `2026-06-21-voxel-grass-foliage-rendering-pipeline`.
- **Priority:** m (visual polish + world gen polish, deferred to dedicated session per
  `agent/workspace.md §2`).
- **Verdict:** `mixed`.
- **Agent:** self.
- **Scope:** standalone C++26 CPU analytical prototype + measurement harness, не mainline.
- **Cross-axis:** orth to closed `cloudscape-rendering` (atmospheric), `volumetric-fog-atmosphere-rendering`
  (participating media), `precomputed-atmospheric-sky` (background sky), `mesh-shader-mega-instancing`
  (mega-instancing for static units, complement via shared `vkCmdDrawIndexedIndirect` pattern);
  complementary to `eye-tracked-foveated` (VRS Tier 2 attachment for grass detail reduction
  in periphery), `vk-fragment-shading-rate-voxel` (same VRS pipeline), `procedural-military-terrain-gen`
  (military terrain features may want sparse grass), `biome-transition-blending` (grass
  density per biome is downstream consumer).
- **Integration:** 3-step migration ~500 LoC, S effort, 1-2 sessions. Deferred до Stage 5.x
  dedicated session per `agent/workspace.md §2` line 36 operator 8x planning decision.
  See `README.md §7 Integration recommendation`.
- **New axis:** first dedicated **grass/foliage/vegetation rendering + placement pipeline** axis
  в 100+ closed experiments.
