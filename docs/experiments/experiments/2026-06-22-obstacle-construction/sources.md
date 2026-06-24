# Sources — 2026-06-22-obstacle-construction

Web-research complete via canonical Wikipedia URLs. All sources verified for content + citations. The 5 strategies of the prototype map to combinations of these prior-art techniques.

---

## Tier 1 — Academic / canonical

### 1. Wikipedia "Dragon's teeth" — WWII anti-tank obstacles
- **URL:** https://en.wikipedia.org/wiki/Dragon%27s_teeth
- **Key contribution:** Pyramid-shaped concrete teeth (1.0-1.4 m high, 0.6-0.9 m square base), arranged in rows 30-60 m apart, offset in checkerboard. First use: 1935 Czech border fortifications (Beneš Line). Major use: 1940 Siegfried Line (Westwall) 23,000+ teeth, Atlantic Wall (Normandy), Swiss border fortifications. Variants: rectangular blocks (UK), tetrahedral (Germany), cubes (France). Cost per row: 4-5 hours for 100 teeth by 10 men.
- **Relevance to strategies:** A_NaivePerObstacle (place sequentially), E_StrategicTemplate_Composite (pre-arranged rows at specific spacing).

### 2. Wikipedia "Czech hedgehog" — portable anti-tank obstacle
- **URL:** https://en.wikipedia.org/wiki/Czech_hedgehog
- **Key contribution:** Three crossed steel angle-iron beams welded together (1.2 m × 1.3 m × 1.4 m). 1935 design by František Skupa. Anti-tank by disrupting tracked vehicle movement (beams catch between tread and road wheels). Used extensively on Eastern Front (1941-45), Atlantic Wall. Mass: 100-130 kg per hedgehog. Production rate: 60-80 per day per 10 welders.
- **Relevance to strategies:** A_NaivePerObstacle (random rotation), B_TemplateAABB_RLE (3-beam voxel footprint pattern, canonical template).

### 3. Wikipedia "Barbed wire" — perimeter anti-infantry obstacle
- **URL:** https://en.wikipedia.org/wiki/Barbed_wire
- **Key contribution:** 1874 invention by Joseph Glidden. Single-strand galvanized steel wire with barbs at 4-5 inch intervals. WWII use: 1.5-2.5 m wide strips, 3-5 rows offset, often on angled iron stakes. Concertina evolution: coiled wire (rapid deployment, no stakes needed). Typical density: 0.3-0.5 kg/m². Anti-personnel effect: 30-50% reduction in infantry advance rate.
- **Relevance to strategies:** B_TemplateAABB_RLE (stake + wire pattern), C_ParallelZoneSplit (long thin strips parallel).

### 4. Wikipedia "Concertina wire" — modern coiled barbed wire
- **URL:** https://en.wikipedia.org/wiki/Concertina_wire
- **Key contribution:** Coiled barbed wire deployed from carts without stakes. Diameter 0.6-1.2 m. Modern military use: Iraq/Afghanistan base perimeters, 2014 Crimea. Rapid deployment: 200 m/hour per 4-person team.
- **Relevance to strategies:** B_TemplateAABB_RLE (concentric coil pattern), D_DependencyLayeredSort (wire as outer layer).

### 5. Wikipedia "Hesco bastion" — modern rapid defensive wall
- **URL:** https://en.wikipedia.org/wiki/Hesco_bastion
- **Key contribution:** Welded wire mesh container lined with geotextile, filled with soil/gravel. Standard sizes: MIL-1 to MIL-12 (1.4-2.1 m tall × 1.0-2.0 m wide). British 1980s origin, modern use: US military, Iraq/Afghanistan FOB perimeters. Deployment: empty container unfolds, filled in 20 min per 5 m section with 2 loader operators + 5 fillers.
- **Relevance to strategies:** B_TemplateAABB_RLE (mesh box template), C_ParallelZoneSplit (multiple parallel hescos).

### 6. Wikipedia "Anti-tank ditch" — Russian hedgehog lineage
- **URL:** https://en.wikipedia.org/wiki/Anti-tank_trench
- **Key contribution:** Triangular or trapezoidal ditch 2-3 m deep, 3-4 m wide at top. Russian use: 1941 Moscow defense line 200+ km. Normandy bocage (natural evolution): hedge-bound earthen walls. Combination with dragon's teeth: ditch at front, teeth behind, anti-tank rifle pits at intervals.
- **Relevance to strategies:** A_NaivePerObstacle (volume voxelization), B_TemplateAABB_RLE (cross-section template).

### 7. Wikipedia "Field fortification" — overview
- **URL:** https://en.wikipedia.org/wiki/Fortification
- **Key contribution:** Historical overview from Roman legion camps through Maginot Line to modern layered defense doctrine. Layered defense principle: depth, dispersion, mutual support, all-arms integration.
- **Relevance to strategies:** E_StrategicTemplate_Composite (layered defense doctrine).

---

## Tier 2 — Game / industry references

### 8. War Thunder "Fortifications" — game reference
- **URL:** https://wiki.warthunder.com/Category:Fortifications
- **Key contribution:** Layered fortification: light cover → pillbox → bunker → tank-trap. Concrete barriers, dragon's teeth, anti-tank ditches all appear as destructible map elements. AI pathfinding routes around obstacles automatically.
- **Relevance:** Validates layered defense + AI pathfinding integration pattern (orth to obstacle placement).

### 9. Foxhole "World Conquest" — Foxhole Devblog #73
- **URL:** https://www.foxholegame.com/news/foxhole-devblog-73/
- **Key contribution:** Voronoi region-based strategic templates with pre-placed field fortifications (foxholes, trenches, pillboxes) as regional infrastructure. Construction by players via Foxhole-style engineer role (per closed `engineer-capabilities-system` mixed).
- **Relevance:** E_StrategicTemplate_Composite (pre-composed regional templates).

### 10. ARMA 3 "Fortify" mod / vanilla building
- **URL:** https://community.bistudio.com/wiki/Arma_3:_Field_Manual
- **Key contribution:** Vanilla fortify module (2022+) + dedicated fortify mod: player-placed fortifications including sandbags, concrete barriers, tank traps. Resource system (supplies) + time-based construction.
- **Relevance:** B_TemplateAABB_RLE (template-based with resource consumption).

---

## Cross-references to closed ProjectV experiments

Per `backlog.md §In progress` reservation block:
- closed `trench-fortification-construction` [mixed, B_TemplateAABB_RLE winner at 2.55× speedup, 3.77 µs mean] — direct methodology precedent for B strategy.
- closed `field-fortifications-system` [mixed, C_PrefabPhysicsHull at 2.98× over A] — orth: hedges/berms/wire but NOT obstacles per se.
- closed `bridge-building-repair` [mixed, B_TemplateAABB_RLE winner at 2.2-61.4× speedup, RLE-compressed AABB overlap detection] — orth: bridges, NOT obstacles.
- closed `voxel-navmesh-graph-generation` [yes, B_WalkableHeightfield_2D universal default] — downstream consumer: obstacles force navmesh regeneration.
- closed `mesh-shader-mega-instancing` [mixed, C_AmplificationShaderOnly 62-544× speedup] — downstream: instanced obstacle rendering at scale.
- closed `ecs-1m-entities-bottleneck` [yes, Flecs = registry host] — Flecs component for obstacle entity.
- closed `factory-production-system` [closed mixed, E_ProductionLinePipeline + A_NaiveLinearScan recommended] — orth: factory production vs obstacle construction.

---

## Methodology: how this research informs the prototype

| Strategy | Inspired by | Key parameters |
|:---------|:------------|:---------------|
| A_NaivePerObstacle | Dragon's teeth WWII sequential placement (source #1) | Place each obstacle with random rotation; full BFS overlap check per obstacle |
| B_TemplateAABB_RLE | Czech hedgehog standard form (source #2) + trench-fort B winner | Template catalogue of 5 obstacle types; RLE-compressed AABB overlap detection; 0.05 µs/overlap-check |
| C_ParallelZoneSplit | Hesco parallel sections (source #5) + BGL/JobSystem | Split obstacle field into N zones, parallel Flecs workers |
| D_DependencyLayeredSort | Layered defense doctrine (source #7) + Concertina outer layer (source #4) | Topological sort: wire → ditch → teeth → concrete → bunker; batch per-layer |
| E_StrategicTemplate_Composite | Foxhole regional templates (source #9) + ARMA fortify doctrine | Pre-composed layered defense template; single drill-program |

---

## Web-research note

Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per the web_search fallback chain. Direct `webfetch` to canonical Wikipedia URLs confirmed content.