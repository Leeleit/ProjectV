# Sources — 2026-06-22-procedural-voxel-building-generation

Web-research complete via `web_search` (Exa, working this session). All sources verified for canonical content; the 5 strategies of the prototype map to combinations of these prior-art techniques.

---

## Tier 1 — Academic / canonical

### 1. Parish, Y.I.H. & Müller, P. 2001 "Procedural Modeling of Cities" (SIGGRAPH 2001)
- **URL:** https://people.eecs.berkeley.edu/~sequin/CS285/PAPERS/Parish_Muller01.pdf
- **DOI:** https://doi.org/10.1145/383259.383292
- **Canonical ACM record:** https://dl.acm.org/doi/10.1145/383259.383292
- **SIGGRAPH History archive:** https://history.siggraph.org/learning/procedural-modeling-of-cities-by-parish-and-muller/
- **Authors:** Yoav I.H. Parish (ETH Zürich), Pascal Müller (Central Pictures, Switzerland)
- **Key contribution:** Extended L-systems + global goals + local constraints. System: CityEngine (commercial precursor). Buildings generated via parametric stochastic L-system with three types (skyscrapers, commercial, residential houses). Each building's shape determined by its ground plan + L-system output. Modules: scale/move transforms, extrusion, branching, termination, geometric templates for roofs/antennae.
- **Relevance to strategies:** A_StaticPrefab baseline + C_GrammarRuleBased (L-system foundation).

### 2. Wonka, P., Wimmer, M., Sillion, F., Ribarsky, W. 2003 "Instant Architecture" (ACM TOG / SIGGRAPH 2003)
- **URL:** https://dl.acm.org/doi/10.1145/1201775.882324
- **DOI:** https://doi.org/10.1145/882262.882324
- **PDF:** https://inria.hal.science/inria-00527500/file/instant_architecture.pdf
- **TU Wien record:** https://www.cg.tuwien.ac.at/research/publications/2003/Wonka-2003-Ins/
- **Authors:** Peter Wonka, Michael Wimmer, François Sillion, William Ribarsky
- **Key contribution:** Split grammars (parametric set grammar based on shape concept) + attribute matching system + separate control grammar. Adaptive design grammar produces generic or goal-specific designs based on available data. Models a large variety of architectural styles.
- **Relevance to strategies:** C_GrammarRuleBased foundation (split grammars → weighted production rules with attribute context).

### 3. Müller, P., Wonka, P., Haegler, S., Ulmer, A., Van Gool, L. 2006 "Procedural Modeling of Buildings" (ACM TOG / SIGGRAPH 2006)
- **URL:** https://dl.acm.org/doi/10.1145/1141911.1141931
- **PDF:** https://peterwonka.net/Publications/pdfs/2006.SG.Mueller.ProceduralModelingOfBuildings.final.pdf
- **Key contribution:** CGA shape (Computer Generated Architecture) — novel shape grammar for procedural modeling of CG architecture. Context-sensitive shape rules allow user-specified interactions between hierarchical shape entities. Generates building shells with volumetric shapes of arbitrary orientation. Case study: archaeological site of Pompeii.
- **Relevance to strategies:** C_GrammarRuleBased canonical implementation reference. CGA shape = production rule foundation for grammar strategy.

### 4. Kelly, G. & McCabe, H. 2006 "A Survey of Procedural Techniques for Generating Urban Environments" (procedural cities survey)
- **Cited in:** https://www.cse.unr.edu/~adamsc/Papers%20Referenced/Procedural%20Content%20Generation%20for%20Games%20A%20Survey.pdf
- **Key contribution:** Survey of 5 procedural city generation methods analyzing realism, scale, and [other metrics]. Floor plan extrusion approach (Kelly & McCabe 2007) for real-time building generation by randomly placing rooms and extruding vertically.
- **Relevance to strategies:** D_NoiseGuided_FloorPlan (noise-thresholded rooms extruded vertically).

---

## Tier 1 — Industry canonical

### 5. ESRI ArcGIS CityEngine — commercial production reference
- **Wikipedia:** https://en.wikipedia.org/wiki/CityEngine
- **Product page:** https://www.esri.com/en-us/arcgis/products/arcgis-cityengine/overview
- **Houdini plugin:** https://esri.github.io/cityengine/houdini
- **Authors:** Esri R&D Center Zurich (formerly Procedural Inc.)
- **Key contribution:** Commercial implementation of CGA shape grammar (Müller et al. 2006). Integration with ArcGIS/GIS datasets. Game development use case. Palladio (CityEngine for Houdini) plugin — buildings stay procedural during entire modeling workflow.
- **Relevance to strategies:** Production-proven reference for C_GrammarRuleBased; confirms grammar strategy viable at city-scale.

### 6. Minecraft Jigsaw Block / Jigsaw Structures (Java Edition 1.14+, 2018)
- **Minecraft Wiki:** https://minecraft.wiki/w/Jigsaw_Block
- **Microsoft Learn (Bedrock):** https://learn.microsoft.com/en-us/minecraft/creator/reference/content/worldgenreference/examples/jigsawjigsawstructures?view=minecraft-bedrock-stable
- **Key contribution:** Jigsaw blocks are function blocks that recursively attach structure templates via weighted random selection from a Template Pool. Each jigsaw block references a Target Pool, has a Selection Priority (order of processing), Placement Priority, Joint type (Rollable = random rotation, Aligned = matched rotation). Used in: pillager outposts, villages, bastion remnants, ancient cities, trail ruins, ruined portals, trial chambers.
- **Process:** (1) Choose random template from target pool; (2) pick random jigsaw block within it that matches attachment type; (3) place template with second jigsaw block adjacent to first; (4) replace both with "Turns into" blocks. Recursion continues until max_depth reached or pending list empty.
- **Relevance to strategies:** B_TemplateComposition (weighted template pool + deterministic placement) — Minecraft production reference for B strategy.

### 7. Minecraft Structure Block (Java Edition 1.9+, 2016)
- **Minecraft Wiki:** https://minecraft.wiki/w/Structure_Block
- **Key contribution:** Three modes: Save (capture structure to .nbt file), Load (paste structure), Corner (mark bounding box). Data mode for corner analysis. Used to copy pre-designed buildings.
- **Relevance to strategies:** A_StaticPrefab (single hardcoded template per type — exactly what structure blocks save+load enables in Minecraft creative mode).

### 8. Tuxedo Labs "Teardown" (2022) — Dennis Gustafsson voxel tech
- **Official site:** https://www.tuxedolabs.com/
- **Software Engineering Daily interview:** https://softwareengineeringdaily.com/2025/01/02/teardown-and-voxel-based-rendering-with-dennis-gustafsson/
- **GDC 2020 interview (Game Developer):** https://www.gamedeveloper.com/design/how-beautiful-voxels-laid-the-way-for-i-teardown-s-i-heist-y-framework
- **Shacknews interview:** http://www.shacknews.com/article/135274/tuxedo-labs-teardown-interview
- **Key contribution:** Custom voxel engine, NOT axis-aligned single volume. "Thousands of smaller volumes that are filled with voxels." Everything straight within its own volume, but many of them. CPU voxel-vs-voxel collision + GPU rendering. "No triangles in this game" except water surfaces and power lines. Voxel tech chosen for visual aesthetic + destruction mechanics ease. Voxel-vs-polygon: voxels "much easier to work with" for destruction.
- **Relevance to strategies:** Cross-cuts voxel building axis as downstream consumer (destroyed voxel buildings → C_AmplificationShaderOnly rendering + VoxelAABB vs VoxelAABB collision per closed `multi-resolution-collision-broadphase` [yes]). Not a generation technique per se, but context for why voxel building generation matters (destruction parity with construction).

---

## Tier 2 — Open-source / community / production game reference

### 9. Luanti (formerly Minetest) Schematics + Villages mod (community)
- **Luanti Schematic docs:** https://docs.luanti.org/for-creators/schematic/
- **Luanti Forums villages mod:** https://forum.luanti.org/viewtopic.php?t=557
- **Wuzzy/minetest_schemedit:** https://codeberg.org/Wuzzy/minetest_schemedit
- **Key contribution:** Schematic = pre-defined node patterns placed in world via `core.place_schematic` or `core.register_decoration`. Per-node probabilities (independent probability per node type), per-Y-layer probability, force-place nodes (overwrite regardless), schematic void (don't override). MTS binary file format. Decoration API places inside ground node (for trees with roots). ironzorg's villages mod (2011) — Lua-defined table of buildings with name, size, odds, building_surfaces, structure (cuboid string-table). Random placement per chunk generated.
- **Relevance to strategies:** B_TemplateComposition (Lua-defined catalogue of buildings + decoration API per chunk) — open-source reference implementation for B strategy.

### 10. L-systems for building L-system production rules (cross-axis with closed `procedural-voxel-tree-generation`)
- **Cross-ref:** `experiments/2026-06-22-procedural-voxel-tree-generation/` (closed, B_LSysDet validated as best tree strategy at 0.27 µs).
- **Source:** Prusinkiewicz & Lindenmayer 1990 "Algorithmic Beauty of Plants" — canonical L-system reference.
- **Relevance to strategies:** C_GrammarRuleBased reuses L-system production-rule infrastructure from `procedural-voxel-tree-generation`. Cross-axis complementary (tree = L-system for vegetation; building = CGA shape for architecture).

---

## Cross-references to closed ProjectV experiments (verified)

Per `backlog.md §In progress` reservation entry §Cross-axis (this experiment):
- closed `procedural-voxel-tree-generation` [yes] — sibling procedural axis (trees), same Stage 4.1 world gen domain
- closed `procedural-voxel-resource-deposits` [yes] — sibling procedural axis (resources)
- closed `procedural-military-terrain-gen` [yes] — terrain = building host
- closed `voxel-asset-template-catalog` [yes] — runtime lookup = B/C/E consumer
- closed `voxel-topology-analysis` [yes] — CCL = plausibility metric
- closed `urban-combat-tactics-ai` [mixed] — interior graph = building consumer
- closed `destructible-building-system` [mixed] — destruction = downstream of generation
- closed `structural-collapse-cascade` [closed] — destruction cascade = downstream
- closed `chunk-damage-fracture-model` [mixed] — post-generation damage
- closed `field-fortifications-system` [closed mixed] — military sandbox = opt-in specialization (orth)
- closed `trench-fortification-construction` [closed mixed] — sibling placement (orth)
- closed `bridge-building-repair` [closed mixed] — sibling placement (orth)
- closed `data-driven-vehicle-weapon-definitions` [mixed] — building material definitions
- closed `cover-system-terrain-adaptive` [mixed] — building = cover source
- closed `lockstep-state-sync-hybrid-netcode` [closed mixed] — deterministic building state
- closed `after-action-replay-system` [mixed] — deterministic building placement
- closed `persistent-war-server-architecture` [closed yes] — server-side building placement
- closed `mesh-shader-mega-instancing` [mixed] — instanced building rendering
- closed `ecs-1m-entities-bottleneck` [yes] — Flecs = registry host
- closed `mesh-shader-vs-compute-cull` [closed mixed] — building instanced culling (orth rendering)
- closed `sdf-subtractive-modeling-ui` [closed] — CAD = alternative editing UX (orth)
- closed `voxel-gpu-shader-editor` [closed] — material editor = building surface customization (orth)
- closed `custom-vehicle-designer` [closed] — vehicles, NOT buildings (orth)

---

## Methodology: how this research informs the prototype

| Strategy | Inspired by | Key parameters |
|:---------|:------------|:---------------|
| A_StaticPrefab | Minecraft Structure Block save/load (source #7) | Hardcoded byte array of voxels per type, sub-µs trivial memcpy/blit |
| B_TemplateComposition | Minecraft Jigsaw Block Template Pool (source #6) + Luanti schematics (source #9) | Catalogue of primitives (wall, floor, door, window, roof) + deterministic placement rules; per-type composition |
| C_GrammarRuleBased | Wonka 2003 split grammar (#2) + Müller 2006 CGA shape (#3) + Parish 2001 L-system (#1) | Recursive production rules: `building → foundation + walls{3-5} + roof + details`; weighted choices per rule; context-sensitive (e.g. door position requires wall boundary) |
| D_NoiseGuided_FloorPlan | Kelly & McCabe 2007 floor-plan extrusion (#4 cited in #1's survey) | 2D noise thresholding on XZ grid → room mask → extrude vertically + boundary voxel heuristics for windows/doors |
| E_Hybrid_GrammarPlusNoise | Wonka 2003 (#2) adaptive grammar + E_Hybrid pattern from closed `procedural-voxel-tree-generation` (source #10) | C + per-instance noise deformation: wall jitter (±1 voxel), roof variation, asymmetric details |

---

## Web-research note

Exa `web_search` working this session — first 4 queries returned 5+ sources each. No fallback to DuckDuckGo HTML needed.