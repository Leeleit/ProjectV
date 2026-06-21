# Sources — 2026-06-21-procedural-military-terrain-gen

20+ primary sources verified via Exa `web_search` + `webfetch` this session.

---

## Tier 1 — Canonical, directly relevant

### 1. Ziegler 2020 — "Generating Real-Time Strategy Heightmaps using Cellular Automata"

- **URL:** https://downloads.hci.informatik.uni-wuerzburg.de/Ziegler2020aa.pdf
- **Year:** 2020
- **Why important:** Direct CA-based terrain generation methodology for Supreme Commander-style RTS. 6-component pipeline (Layout / Erosion / Marker / Detail / Texturing / Export). Point/axis symmetry for fairness. Validated via 30-participant survey.
- **Used for:** Strategy B (CellularAutomata_Ridges) implementation. CA rule for ridge amplification. Symmetry argument for fairness in military context.

### 2. Piepenbrink & Bidarra 2025 — "Non-Uniform Tile Wave Function Collapse" (IEEE CoG 2025)

- **URL:** https://repository.tudelft.nl/record/uuid:d82fa984-8b7b-4f00-a012-af496f48b46f
- **Venue:** IEEE CoG 2025, Lisbon, Portugal (2025-08-26 to 2025-08-29)
- **DOI:** 10.1109/CoG64752.2025.11114084
- **Year:** 2025
- **Why important:** nutWFC super-set of WFC supports multi-cellular tiles with varying shapes/sizes (NUTs) without significant performance penalty. Embargo expired 2026-02-23.
- **Used for:** Strategy D (TacticalWFC) theoretical foundation. Justifies multi-tile-shape approach for tactical features (different tile shapes for ridge vs defilade vs kill zone).

### 3. Scholz 2017 — "Wave Function Collapse for Terrain Generation" (TU Wien Master Thesis)

- **URL:** https://www.cg.tuwien.ac.at/research/publications/2019/scholz_2017_bac/scholz_2017_bac-thesis.pdf
- **Year:** 2017
- **Why important:** WFC adapted for infinite, deterministic, run-time terrain generation with chunk border constraints. Direct implementation reference. Seeding + deterministic output.
- **Used for:** Strategy D implementation. Chunk border constraint algorithm. Seeding for determinism.

### 4. Carver & Washtell 2012 — "Real-time visibility analysis using voxel-based viewshed"

- **URL:** https://www.geos.ed.ac.uk/~gisteac/proceedingsonline/GISRUK2012/Papers/presentation-48.pdf
- **Year:** 2012
- **Why important:** Voxel-based viewshed transform, real-time on desktop. Tested on DSM raster data. Sublinear + linear algorithms exploit Moore's law.
- **Used for:** `viewshed_coverage` feature detector (kill_zone requires 50% viewshed in 50m radius).

### 5. Brian — "Accelerating Line of Sight Computation Using Graphics Processing Units" (University of Maryland)

- **URL:** http://gamma-web.iacs.umd.edu/LOS/asclos.pdf
- **Year:** ~2010 (cited in multiple LOS papers)
- **Why important:** Hybrid GPU-CPU LOS algorithm, 17× faster than R3-Tree. Conservative rasterization + hardware occlusion queries. Integrated into OneSAF.
- **Used for:** LOS cost analysis. Justifies analytical CPU prototype as upper-bound measurement.

### 6. Fraunhofer IOSB — "SWA Position Selection Assistant for Ground Forces"

- **URL:** https://www.iosb.fraunhofer.de/en/projects-and-products/position-selection-wizard-ground-forces.html
- **Year:** 2024 (current product)
- **Why important:** Production military terrain analysis. Inputs: field of fire + cover + passability + direction of attack + weapon system parameters. Result: georeferenced position value map (heat map).
- **Used for:** Feature detector list (field of fire, cover, passability). Quality validation reference for feature detector thresholds.

### 7. ArcGIS ModelBuilder — "Automating Military Terrain Analysis: OAKOC Generation"

- **URL:** https://repository.arizona.edu/handle/10150/679988
- **Year:** 2026
- **Why important:** Automated ArcGIS toolbox for OAKOC analysis. 15 min/run vs hours manual. Uses DEM + land cover + hydrology + transport networks.
- **Used for:** OAKOC taxonomy validation (Observation/Avenues/Key terrain/Obstacles/Cover). Cost benchmark reference.

---

## Tier 2 — Production references, methodology

### 8. Kowalski 2018 — "Strategic Features and Terrain Generation"

- **URL:** https://jakubkowalski.tech/Publications/Kowalski2018StrategicFeatures.pdf
- **Year:** 2018
- **Why important:** Graph grammar Logic Map Layout (LML) — zones (local/buffer/outer) + features (mines/towns) + parameterized randomized rules. Directly applicable to military scenario.
- **Used for:** Zone + feature model. LML as alternative to WFC.

### 9. Foxhole Devblog #73 — "Region Zone Designs, Day and Night, and the Deployment System"

- **URL:** https://www.indiedb.com/games/foxhole/news/devblog-73-region-zone-designs-day-and-night-and-the-deployment-system
- **Year:** 2019
- **Why important:** Voronoi diagram for region zones via Fortune's algorithm (O(N log N) vs naive O(N²)). Production military game.
- **Used for:** Zone placement reference. Voronoi for territory partition.

### 10. Foxhole Devblog #70 — "Map Configurations"

- **URL:** https://www.foxholegame.com/post/devblog-70
- **Year:** 2019
- **Why important:** Localized resource generation, RNG balance, hex map breaking up "linear push" gameplay. Real production reference.
- **Used for:** Resource placement patterns. Fairness considerations.

### 11. Kacper Szwajka 2024 — "GPU Run-time Procedural Placement on Terrain" (Medium)

- **URL:** https://medium.com/@kacper.szwajka842/gpu-run-time-procedural-placement-on-terrain-cc874e39bbfb
- **Year:** 2024
- **Why important:** Horizon Zero Dawn-inspired, 512 pointers/chunk, compute shader, prototype picking algorithm. 16M instances for 2x2km map.
- **Used for:** Stamp library implementation pattern (C_StampLibrary). Pointer/instance model.

### 12. Ymirge C++17 GPU terrain

- **URL:** https://github.com/LoxleyXI/Ymirge
- **Year:** 2025-10-27
- **Why important:** 5 brush types + stamp library + GPU compute. 20-30× speedup vs CPU. Real open-source reference.
- **Used for:** Stamp library reference. Brush + stamp + layer compositing model.

### 13. EliasVahlberg/terrain-forge (Rust)

- **URL:** https://github.com/EliasVahlberg/terrain-forge
- **Year:** 2025+
- **Why important:** 15 generation algorithms (BSP, CA, WFC, Delaunay, Glass Seam Bridging, Noise Fill, Percolation, Diamond Square, Fractal, Agent, etc.). Thread-safe, serializable configs, semantic analysis.
- **Used for:** Algorithm comparison reference. Method naming convention.

### 14. JohnLudlow/MonoGameSamples.TerrainGeneration2D

- **URL:** https://github.com/JohnLudlow/MonoGameSamples.TerrainGeneration2D
- **Year:** 2025-12-31
- **Why important:** Production WFC with AC-3 + precomputed rule tables + backtracking + chunk seam consistency + property-based tests. 95%+ coverage.
- **Used for:** WFC implementation reference (D_TacticalWFC). AC-3 constraint propagation.

### 15. nubDotDev/faster-poisson-disk-sampling (Rust + WGSL)

- **URL:** https://github.com/nubDotDev/faster-poisson-disk-sampling/
- **Year:** 2025-09-06
- **Why important:** WGSL GPU implementation. Real benchmark on small grids.
- **Used for:** Poisson disk reference for C_StampLibrary stamp placement.

---

## Tier 3 — Industrial military systems

### 16. Rheinmetall SWA — "Position Selection Assistant for German Army"

- **URL:** https://www.army-technology.com/news/rheinmetalls-swa-position-selection-assistant-speeds-operation-planning-for-german-army-with-ai/
- **Year:** 2024
- **Why important:** 60→10 min plan prep time reduction. AI/ML-based. Production military system.
- **Used for:** Production cost benchmark reference (60→10 min). Justifies the value of automated military terrain analysis.

### 17. Carmenta GVSETS 2025 — "Optimizing Firing Position Usage for Survivability and Effectiveness in Artillery Shoot-and-Scoot Tactics"

- **URL:** https://carmenta.com/wp-content/uploads/2025/08/Carmenta_PM_Optimizing-Firing-Position-Usage-for-Survivability-and-Effectiveness-in-Artillery-1.pdf
- **Year:** 2025
- **Why important:** Shoot-and-scoot tactical optimization. Independent sets in graph theory. Real production reference for firing position analysis.
- **Used for:** `firing_position` feature detector. Independent set model.

### 18. Kewley et al. FLAIRS 2024 — "Terrain-Aware Military Planning Agents"

- **URL:** https://journals.flvc.org/FLAIRS/article/view/135800
- **Year:** 2024
- **Why important:** Mission Command Agents, multi-objective search over observation/fires/mobility. C2SIM standard integration. Production reference.
- **Used for:** Multi-objective search methodology. C2SIM compatibility for downstream consumers.

### 19. Dawid 2024 — "Optimization of the Route Determination Process for the Purposes of Military Terrain Passability"

- **URL:** https://aimt.cz/index.php/aimt/article/view/1865
- **Year:** 2024
- **Why important:** DEM generalization for routing. 3× speedup with preserved route accuracy. User-adjustable parameters.
- **Used for:** DEM generalization cross-ref. Passability as feature detector input.

---

## Supplementary

### 20. arXiv 2412.04688 — "Utilizing WaveFunctionCollapse Algorithm for Procedural Generation of Terrains using Remotely Sensed Elevation Data"

- **URL:** https://arxiv.org/abs/2412.04688
- **Year:** 2024-12-06
- **Why important:** WFC for terrain heightmap using SRTM data, slope-based input for structural feature preservation. Statistical histogram comparison.
- **Used for:** WFC + slope input methodology. Confirms slope-based input better preserves structural features.

### 21. Opa-/foxhole-heightmap-generator

- **URL:** https://github.com/Opa-/foxhole-heightmap-generator
- **Year:** 2023-10-24
- **Why important:** Python heightmap generator for Foxhole. Real production reference for military game terrain file format.
- **Used for:** File format compatibility reference (out of scope for this experiment).

### 22. Wolfgang-IX/Foxhole-Map-Project

- **URL:** https://github.com/Wolfgang-IX/Foxhole-Map-Project
- **Year:** 2025-07-19
- **Why important:** Foxhole map modding with heightmap, contour lines, AO, curvature, road tiers. Real production modding reference.
- **Used for:** Production modding pipeline reference (out of scope for this experiment).

### 23. "Analyzing Procedural Terrain Generation in Games from a Constraint Programming Perspective" (ENIAC 2025)

- **URL:** https://sol.sbc.org.br/index.php/eniac/article/view/38874
- **Year:** 2025-09-29
- **Why important:** WFC + Nested WFC (N-WFC) constraint programming analysis. Directed arc consistency sufficient for many tile sets.
- **Used for:** WFC consistency analysis. Justifies directed-arc-consistency-only approach for tactical tile sets.

### 24. Stachniak & Stuerzlinger — "Sketch-Based Terrain Generation with Constraints"

- **URL:** https://pdfs.semanticscholar.org/35bd/3ce986cc7fd711ab5d0b4280ea3a5b29208a.pdf
- **Year:** ~2005 (per Stachniak citation chain)
- **Why important:** Sketch + constraint-based terrain modification. Mask image for ecotope. Smooth noise transition.
- **Used for:** Constraint-based methodology. Ecotope-as-mask pattern for feature placement.

### 25. "Accelerating Line of Sight Computation" (R3-Tree vs heightmap)

- **URL:** http://gamma-web.iacs.umd.edu/LOS/asclos.pdf
- **Year:** ~2010
- **Why important:** 17× heightmap vs R3-Tree. Algorithmic pattern matching.
- **Used for:** LOS algorithm comparison.

---

## Out-of-scope references (noted, not used)

- **Ymirge GPU terrain** — same as #12, noted.
- **UE 5.7 PCG Editor Mode** — StraySpark 2026 — Poisson Disk Sampling for building seeds, 30-400K points. Not used (commercial engine context, not direct WFC/CA methodology).
- **NeuralPVS (DerThomy)** — deep learning visibility, SIGGRAPH Asia 2025. Not used (deep learning not in scope for this experiment).
- **GPU Line of Sight (EntroPi-Games)** — Unity asset. Not used (commercial engine).
- **LineOfSightAnalyzer (berkbavas)** — Qt6 + OpenGL. Not used (LOS analyzer, not terrain gen).
- **Efficient Line-of-Sight Algorithms for Real Terrain Data** — Floriani et al., R3-Tree. Not used (LOS, not gen).
- **SWA Fraunhofer Army Recognition** — Army Recognition news. Not used (duplicate of #6).

---

## Cross-references to ProjectV closed experiments

- `2026-06-21-wfc-procedural-worlds` [mixed] — generic WFC, no military features.
- `2026-06-21-genlayer-functional-biome-pipeline` [mixed] — biome chain (1.12 GenLayer.java).
- `2026-06-21-biome-transition-blending` [mixed] — C_DistanceBlend_BiL Pareto-optimal.
- `2026-06-21-trilinear-noise-interpolation` [in-progress] — noise interpolation from coarse grid.
- `2026-06-21-gpu-procedural-noise-compute-kernels` [Stage 4.1 noise basis] — GPU compute noise variants.
- `2026-06-21-cover-system-terrain-adaptive` [in-progress] — direct downstream consumer.
- `2026-06-21-explosion-crater-terrain-deformation` [closed yes, E_RasterizedSphereMarch 0.128 µs] — direct downstream consumer (deformation modifies pre-generated terrain).

---

**Verification method:** All URLs visited and content confirmed via Exa `web_search` + `webfetch` on 2026-06-21. Tier 1 sources verified to contain claimed methodology. Tier 2/3 used for production reference + cost benchmark.
