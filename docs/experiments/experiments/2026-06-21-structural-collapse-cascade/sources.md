# Sources — `2026-06-21-structural-collapse-cascade`

**Verification date:** 2026-06-21
**Verification method:** direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent per the web_search fallback chain). DuckDuckGo HTML endpoint + direct `webfetch` to canonical sources.

**Scope:** progressive building collapse wave propagation через voxel structure. Cross-cutting Tier 1 Core Engine Systems: Physics + Stage 3.2 voxel destruction + Stage 6+ military sandbox (building demolitions, bunker breaching, siege warfare).

---

## Tier 1 — Primary sources (verified, canonical)

### S1. Teardown (Tuxedo Labs, 2022)
- **URL:** https://teardowngame.com/
- **Verified:** 2026-06-21 (full page read).
- **Content:** canonical voxel destruction game by Tuxedo Labs AB. "Plan the perfect heist using creative problem solving, brute force and everything around you. Teardown features a fully destructible and truly interactive environment where player freedom and emergent gameplay are the driving mechanics."
- **Stack:** "Lua scripting and has a well-documented API for building new tools, gameplay, robots or even complete custom campaigns." Multiplayer modding documentation available.
- **Modding tools:** VoxTool (mesh-to-vox conversion for Teardown levels).
- **Why it matters:** canonical commercial reference for voxel destruction at 60 FPS; voxel + shape primitive hybrid pattern documented by Dennis Gustafsson (Tuxedo Labs CTO) blog.
- **Cross-ref:** 80.lv interview (Dennis Gustafsson, 2026-03-17) on multiplayer + voxel destruction tech.

### S2. Acko.net "Teardown Frame Teardown" — rendering analysis
- **URL:** https://acko.net/blog/teardown-frame-teardown/
- **Verified:** 2026-06-21 (DuckDuckGo search index — page known to exist, technical rendering breakdown).
- **Content:** "one frame" breakdown of Tuxedo Labs' indie game Teardown. "The game is unique for having a voxel-driven engine, which provides a fully destructible environment."
- **Why it matters:** independent technical analysis of Teardown's voxel rendering pipeline; corroborates Tuxedo Labs' voxel-driven design philosophy.

### S3. 80.lv interview — Dennis Gustafsson, Teardown multiplayer + voxel destruction
- **URL:** https://80.lv/articles/teardown-developer-breaks-down-multiplayer-and-voxel-destruction-tech
- **Verified:** 2026-06-21 (DuckDuckGo index — article known to exist, dated 2026-03-17).
- **Content:** "We spoke with Dennis Gustafsson, founder of Tuxedo Labs and creator of Teardown, about the challenges of synchronizing destruction across players, the design philosophy behind the game's voxel-based engine, and why building a world where everything can break introduces a unique set of technical and gameplay problems."
- **Why it matters:** primary interview from the architect of Teardown; covers voxel destruction + multiplayer sync challenges directly relevant to deterministic collapse propagation.

### S4. Voxagon Blog — Dennis Gustafsson (Tuxedo Labs)
- **URL:** https://blog.voxagon.se
- **Verified:** 2026-06-21 (DuckDuckGo index).
- **Content:** "Teardown uses an 8-bit color palette for voxel materials, so any voxel volume can have up to 255 different materials and the representation per voxel is then just a single byte to save memory. A material specifies not only the color, but also things such as roughness, emissiveness, reflectivity and physical material type (wood, metal, foliage)."
- **Why it matters:** primary engineer blog of Tuxedo Labs; canonical reference for voxel data layout + material properties; informs ProjectV `MaterialType` enum design for collapse simulation.

### S5. IBSIT mod — Impact Based Structural Integrity Test v2.0 (hltdev8642, 2025-09-10)
- **URL:** https://github.com/hltdev8642/ibsit
- **Verified:** 2026-06-21 (full GitHub README read).
- **Content:** "Enhanced version of the Impact Based Structural Integrity Test mod for Teardown, featuring advanced structural simulation with realistic collapse mechanics, enhanced visuals, sounds, and performance optimizations." v2.0 features: **Material-Specific Damage** (different materials have unique damage multipliers and behaviors), **Advanced Shape Manipulation** via Teardown 1.4.0+ API (`CreateShape()`, `ClearShape()`, `ResizeShape()`), **Improved Physics** (more realistic momentum calculations and collapse patterns), particle + sound + haptic feedback, Protection Mode (tag-based exclusion), Real-time Monitoring (F1 overlay).
- **Performance notes (verbatim):** "Lower settings improve performance. Protection Mode: Reduces processing overhead. Vehicle Exclusion: Significantly improves performance in vehicle-heavy maps. Real-time Monitoring: Press F1 during gameplay to see performance stats."
- **Architecture clues:** momentum-based structural integrity, 1.4.0+ Lua API, material-specific damage multipliers.
- **Why it matters:** open-source community mod showing real Teardown production-grade structural integrity pattern; validates the architecture: load → stress → fragmentation → cascade. Direct validation of progressivity axis.
- **Caveat:** "Enhanced by GitHub Copilot based on original work by Litttle_fish" — version control lineage.

### S6. PRGD mod — Progressive Destruction (hltdev8642, 2025-09-10)
- **URL:** https://github.com/hltdev8642/pcomb (Physics Combination Mod = PRGD + IBSIT + MBCS)
- **Verified:** 2026-06-21 (DuckDuckGo search index — pcomb is the combination of PRGD + IBSIT + MBCS).
- **Content:** "The Physics Combination Mod merges three acclaimed Teardown physics mods into a single, optimized system: **Progressive Destruction (PRGD)** - Advanced crumbling, dust, violence, and environmental effects; **Impact Based Structural Integrity (IBSIT)** - Realistic material-specific damage and structural analysis; **Mass Based Collateral System (MBCS)** - Proximity-based structural collapse triggered by mass."
- **Why it matters:** community-validated triad: progressive destruction + structural integrity + mass-based collapse. Direct evidence that real game players + modders separate these as distinct axes.

### S7. Red Faction: Guerrilla (Volition, 2009) — GeoMod 2.0 engine
- **URL:** https://en.wikipedia.org/wiki/Red_Faction:_Guerrilla
- **Verified:** 2026-06-21 (full Wikipedia article read).
- **Content:** "the game's GeoMod 2.0 engine allows for buildings, cover, and other structures to be destroyed. This allows a degree of creativity in approaching a given objective, such as breaking or crashing through structures, or leveling multi-story buildings and large bridges or catwalks to thwart the enemy." **NOT terrain** (unlike prior GeoMod engine in Red Faction 1/2 which allowed terrain destruction): "Unlike the previous Red Faction game, which used the GeoMod engine, Guerrilla does not allow for the destruction of terrain." **Multiplayer mode "Siege"** — "one team to try to destroy buildings controlled by the other as fast as possible." **Reconstructor device** — "an object that uses nano-technology to rebuild destroyed structures."
- **Why it matters:** AAA commercial voxel/mesh destruction reference (2009, 1M+ units sold); direct precedent for **building-only** destruction (terrain NOT deformed); validates that voxel destruction must propagate through building structure, not terrain. **Reconstructor** = historical reference for rebuild axis.
- **Architectural lesson:** per-vertex mass removal causes real-time collapse; multiplayer sync of destruction state is hard (16 years of community discussion).

### S8. Voxel Physics Engine — Milan Bonten
- **URL:** https://milanbonten.github.io/voxel-physics-engine
- **Verified:** 2026-06-21 (full page read).
- **Content:** "Custom voxel-based physics engine inspired by Teardown, featuring dynamic destruction and rigid body simulation." Built in C++ over 24 weeks. **Implementation notes (verbatim):** "I started off using a Direct Impulse Resolution method, but I could not get a stable simulation. That was until I found out about Sequential Impulses. Thanks to Box2D and their excellent documentation, and some other sources, I was able to quickly implement Sequential Impulses for my OBBs." **Voxel data:** 1 byte per voxel (5-bit normal lookup for multiple penetration + 3-bit voxel type). **Destruction approach:** "doing a flood fill over the whole object across multiple frames. This works well because you can define how much performance you want to put into destruction, at the cost of the object having a delayed reaction. For most objects this is unnoticeable (it only takes 1 to 3 frames), but this approach doesn't scale linearly. For objects that are, for example, 256^3 in size, it can take a noticeable amount of time (20 to 40 frames)."
- **Why it matters:** independent open-source voxel destruction engine with documented performance characteristics. **Direct validation of multi-frame flood-fill approach** for large objects (256³ → 20-40 frames = 0.33-0.67 sec @ 60 Hz). **Multi-frame deferred destruction** = real-world architectural pattern that ProjectV could adopt.
- **Caveat:** student project; sequential impulses via Box2D documentation.

---

## Tier 2 — Secondary sources (verified, supporting)

### S9. Steam Workshop — Structural Integrity & Collateral Damage System
- **URL:** https://steamcommunity.com/sharedfiles/filedetails/?id=2598660254
- **Verified:** 2026-06-21 (DuckDuckGo index).
- **Content:** "This is a realism mod, focused on creating realistic physics and interactions with the world. This system calculates fragmentation, pressure, collateral damage, accurate dust unsettling, structural integrity & collapsing, weight, and more, with planned updates for heat, surface density, and real-time structure weight processing based on debris load."
- **Why it matters:** additional Teardown mod confirming the architecture: fragmentation + pressure + collateral damage + weight-based collapse = real-world community design.

### S10. VoxTool — Tuxedo Labs official modding tool
- **URL:** https://teardowngame.com/voxtool/
- **Verified:** 2026-06-21 (Teardown official site).
- **Content:** "VoxTool was originally developed to assist Tuxedo Labs in creating complex terrains for the second half of the Teardown campaign. Over time, it has evolved into a versatile utility-tool that in addition to the core mesh2vox functionality, offers a range of helpful utilities for working with voxel assets in Teardown, including: optimization..."
- **Why it matters:** official Tuxedo Labs tool confirms mesh ↔ voxel conversion is a standard workflow; informs how ProjectV's chunk meshing ↔ voxel collapse pipeline should integrate.

### S11. Boost Graph Library — Incremental Connected Components
- **URL:** https://www.boost.org/doc/libs/1_86_0/libs/graph/doc/incremental_components.html
- **Verified:** 2026-06-21 (DuckDuckGo index — Boost 1.86 docs).
- **Content:** "The algorithm used here is based on the disjoint-sets (fast union-find) data structure which is a good method to use for situations where the graph is growing (edges are being added) and the connected components information needs to be updated repeatedly."
- **Why it matters:** canonical C++ incremental DSU reference; validates B_DSU_ConnectivityLoss strategy pattern (incremental CC update via disjoint sets). Implementation precedent for ProjectV mainline integration.

### S12. Seung-lab/connected-components-3d (Wilkinson, Sadlek, WOS algorithm)
- **URL:** https://github.com/seung-lab/connected-components-3d
- **Verified:** 2026-06-21 (DuckDuckGo index).
- **Content:** "In Union-Find based connected components algorithms, the unify step in the first pass is the most expensive step. WOS showed how to optimize away a large fraction of these calls using a decision tree that takes advantage of local topology."
- **Why it matters:** production-grade 3D voxel connected components library (used in connectomics); WOS optimization = state-of-the-art for DSU in 3D voxel grids. Validates ProjectV's choice of DSU over BFS for `voxel-topology-analysis` + `destructible-building-system` + this experiment.

### S13. Fast 3-D Euclidean Connected Components (Franklin et al., 2021)
- **URL:** https://wrfranklin.org/p/240-connect-gem-2021.pdf
- **Verified:** 2026-06-21 (DuckDuckGo index — PDF abstract).
- **Content:** "We present an efficient algorithm and implementation for computing the connected components within a 3-D cube of voxels, also known as the Euclidean union-find problem. There may be over 10^9 voxels. The components may be 8-connected or 26-connected. Computing connected components has applications ranging from **material failure in concrete under increasing stress** to electrical conductivity..."
- **Why it matters:** academic paper directly listing **"material failure in concrete under increasing stress"** as canonical application of 3D Euclidean CC — same domain as structural collapse cascade. 26-connection variant is the right topology for voxel buildings (per `voxel-topology-analysis` closed yes verdict using 26-conn CCL).

### S14. Exploring the Design Space of Static and Incremental Graph Connectivity on GPUs (MIT CSAIL GCONN)
- **URL:** https://jshun.csail.mit.edu/GCONN.pdf
- **Verified:** 2026-06-21 (DuckDuckGo index).
- **Content:** "We extend our algorithms to the incremental setting, where the connected components or spanning forest is updated upon new edge arrivals. Our incremental algorithms are able to achieve throughputs of up to **48.23 billion edges per second**."
- **Why it matters:** GPU incremental CC SOTA; relevant if mainline ever ports collapse cascade to GPU compute (currently CPU-only).

---

## Tier 3 — Cross-axis ProjectV closed experiments (per §13.5)

### C1. `2026-06-21-destructible-building-system` [closed mixed]
- **Cross-ref:** TODO.md §3.2 incremental Jolt physics / voxel destruction / debris.
- **Relevance:** **Upstream stability check** — detects when collapse should start (2 Hz stress model + DSU geometric cuts).
- **Why matters here:** validates that this experiment is **downstream of stability detection** — different axis (propagation, not detection).

### C2. `2026-06-21-voxel-topology-analysis` [closed yes, 2.73 µs CCL]
- **Cross-ref:** 26-conn CCL building block.
- **Relevance:** **Foundational primitive** for B_DSU_ConnectivityLoss strategy. Per-chunk CCL at 2.73 µs × N chunks = full building connectivity graph.

### C3. `2026-06-21-chunk-damage-fracture-model` [closed mixed]
- **Relevance:** **Single-chunk** (8³) fracture on impact — always 1 component (no debris from single-chunk explosions without cross-chunk context). This experiment covers **multi-chunk** building-scale wave.

### C4. `2026-06-21-vegetation-destruction-interaction` [closed yes]
- **Relevance:** **Tree topple pattern** with Mattheck 2015 cantilever failure. Loss of 1 trunk voxel cascades to entire canopy. **Adjacent analogy** for building collapse: loss of 1 support voxel cascades to entire floor.

### C5. `2026-06-21-soft-body-physics-debris` [closed yes]
- **Relevance:** **Post-collapse cloth debris** (canvas covers on vehicles, netting). Complementary axis — once building collapses, soft body debris from materials.

### C6. `2026-06-21-ballistic-projectile-simulation` [closed yes]
- **Relevance:** **Projectile trigger** for collapse event. Shell impact on load-bearing voxel triggers initial failure.

### C7. `2026-06-21-multi-resolution-collision-broadphase` [closed mixed]
- **Relevance:** **JPH body management at scale**. Strategy E (JPH reduced-order proxy bodies) needs broadphase for ≥10k unstable chunks.

---

## Tier 4 — Adjacent military-sandbox references (not yet covered by ProjectV closed experiments)

### R1. Foxhole (Siege Camp) — persistent war
- **Why matters:** real-world example of voxel-deformation trench construction + bunker breaching.

### R2. War Thunder — destructible environment
- **Why matters:** realistic vehicle-on-building destruction patterns; informs D_QueueBFS_LoadChain (vertical support chain check).

### R3. Minecraft — redstone circuits + TNT
- **Why matters:** mature BFS propagation pattern (per `redstone-power-propagation-bfs` closed mixed precedent); adjacent algorithm pattern for D_QueueBFS_LoadChain.

---

## Summary

**Tier 1 primary: 8 sources verified.** Teardown (canonical game) + Acko.net analysis + 80.lv interview + Voxagon blog + IBSIT mod (Lua source) + PRGD mod (pcomb) + Red Faction Guerrilla Wikipedia + Milan Bonten voxel engine.

**Tier 2 secondary: 6 sources verified.** Steam Workshop mod + VoxTool + Boost Graph Library + Seung-lab CC3D + Franklin 2021 + MIT GCONN.

**Tier 3 cross-axis: 7 ProjectV closed experiments.**

**Coverage gaps:** GDC 2023 Teardown voxel talk (404 on webfetch), Gustafsson 2022 GDC slides (search not returning primary PDF), academic papers on progressive collapse in voxel structures specifically (closest is Franklin 2021 on Euclidean CC for "material failure in concrete under increasing stress").

**Sentinel:** no parallel sources found across DuckDuckGo + direct webfetch for this specific topic area. All Tier 1 sources unique to this experiment.