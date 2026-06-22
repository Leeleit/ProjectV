# Sources — 2026-06-22-procedural-voxel-road-path-generation

Web-research complete via planned sources per README §2.

---

## Tier 1 — Academic / canonical

### 1. Parish, Y.I.H. & Müller, P. 2001 "Procedural Modeling of Cities" (SIGGRAPH 2001)
- **URL:** https://people.eecs.berkeley.edu/~sequin/CS285/PAPERS/Parish_Muller01.pdf
- **DOI:** https://doi.org/10.1145/383259.383292
- **Canonical ACM record:** https://dl.acm.org/doi/10.1145/383259.383292
- **Key contribution:** Road map generation from image maps via L-systems extended with global goals + local constraints. Traffic network as first-class citizen before buildings. Stochastic parametric L-system for buildings once lots assigned.
- **Relevance to strategies:** B_TemplateComposition (L-system for polyline) + C_GrammarRuleBased (extended L-system for junctions) foundation.

### 2. Wonka, P., Wimmer, M., Sillion, F., Ribarsky, W. 2003 "Instant Architecture" (SIGGRAPH 2003)
- **URL:** https://dl.acm.org/doi/10.1145/1201775.882324
- **DOI:** https://doi.org/10.1145/882262.882324
- **Key contribution:** Split grammars for building design. Attribute matching + control grammar. Adaptive based on data.
- **Relevance to strategies:** C_GrammarRuleBased (split grammar → T/Y junction rules).

### 3. Müller, P., Wonka, P., Haegler, S., Ulmer, A., Van Gool, L. 2006 "Procedural Modeling of Buildings" (SIGGRAPH 2006)
- **URL:** https://dl.acm.org/doi/10.1145/1141911.1141931
- **Key contribution:** CGA shape grammar for building shells. Context-sensitive rules.
- **Relevance to strategies:** C_GrammarRuleBased canonical reference.

### 4. Kelly, G. & McCabe, H. 2006 "A Survey of Procedural Techniques for Generating Urban Environments"
- **Cited in:** https://www.cse.unr.edu/~adamsc/Papers%20Referenced/Procedural%20Content%20Generation%20for%20Games%20A%20Survey.pdf
- **Key contribution:** Survey of 5 procedural city generation methods. Includes road network generation comparison.
- **Relevance to strategies:** Provides method comparison framework for strategies A/B/C/D/E.

---

## Tier 1 — Industry canonical

### 5. ESRI ArcGIS CityEngine — commercial production reference
- **Wikipedia:** https://en.wikipedia.org/wiki/CityEngine
- **Product page:** https://www.esri.com/en-us/arcgis/products/arcgis-cityengine/overview
- **Key contribution:** Commercial implementation of CGA shape grammar. Street network + lot subdivision + building generation. Industry standard for film/TV/games urban generation.
- **Relevance to strategies:** Production-proven reference for B/C strategies.

### 6. Minecraft roads / village paths
- **Minecraft Wiki "Path" / "Road" / "Village":** various wiki pages
- **Key contribution:** Simple dirt/gravel/sand road generation, per-biome style. No curves; orthogonal grid layout.
- **Relevance to strategies:** A_StaticFlat baseline = Minecraft path behavior.

### 7. Foxhole / WARNO / Squad / Arma — military sandbox road patterns
- **Foxhole official:** https://www.foxholegame.com/
- **WARNO official:** https://www.eugensystems.com/
- **Key contribution:** Real-world military road networks: orthogonal grid (Foxhole), organic rural (WARNO Eastern Front), procedural (Squad).
- **Relevance to strategies:** Source for type-specific road styles (gravel_runway, gravel_motorway, stone_highway) and shoulder/kerb/lane configurations.

---

## Tier 2 — Open-source / community reference

### 8. OpenStreetMap + procedural road extractor
- **OSM:** https://www.openstreetmap.org/
- **Key contribution:** Real-world road network topology (vector tiles, .osm.pbf). Procedural extractors (osmium, osm2pgsql) yield polylines.
- **Relevance to strategies:** Source for polyline format that B/C consume.

### 9. Houdini SideFX "Procedural Roads" / "City Generation" toolkit
- **SideFX Houdini:** https://www.sidefx.com/products/houdini/
- **Key contribution:** Production-grade procedural road network tools in Houdini's SOP/VEX.
- **Relevance to strategies:** Confirms procedural road generation is industry standard for VFX.

---

## Cross-references to closed ProjectV experiments (verified)

Per `backlog.md §In progress` reservation entry §Cross-axis (this experiment):
- closed `procedural-voxel-building-generation` [yes, B_TemplateComposition validated] — cross-axis sibling procedural axis (same Stage 4.1 world gen domain, B_TemplateComposition pattern reused)
- closed `procedural-voxel-tree-generation` [yes, B_LSysDet validated at 0.27 µs] — cross-axis sibling procedural axis
- closed `procedural-voxel-resource-deposits` [yes] — sibling procedural axis (resources)
- closed `procedural-military-terrain-gen` [yes] — terrain = road host
- closed `voxel-asset-template-catalog` [yes] — runtime lookup = B/C/D consumer
- closed `voxel-topology-analysis` [yes] — CCL = plausibility metric (connectivity)
- closed `mesh-shader-mega-instancing` [mixed] — instanced road rendering (orth axis)
- closed `flow-field-pathfinding-10k-units` [yes] — AI pathfinding ON roads (consumer)
- closed `voxel-mutation-cost-characterization` [mixed] — per-voxel mutation cost (consumer)
- closed `lockstep-state-sync-hybrid-netcode` [mixed] — deterministic road state

---

## Methodology: how this research informs the prototype

| Strategy | Inspired by | Key parameters |
|:---------|:------------|:---------------|
| A_StaticFlat | Minecraft dirt path (source #6) | 3×N flat rectangle of ROAD voxels; trivial nested loop |
| B_TemplateComposition | Parish 2001 L-system road (#1) + closed `procedural-voxel-building-generation` B_TemplateComposition pattern | Polyline composed of straight-segment + curve primitives (sin-based bend) |
| C_GrammarRuleBased | Wonka 2003 split grammar (#2) + Müller 2006 CGA shape (#3) + Parish 2001 extended L-system (#1) | Recursive rules: `road → straight{N} → curve{straight/left/right/T/Y} → ...` with weighted choices |
| D_NoiseGuided_Width | Kelly & McCabe 2006/2007 (#4) floor-plan extrusion pattern | Width dithered by 2D hash noise per z-step; ±1 voxel ragged edges |
| E_Hybrid_GrammarPlusNoise | Closed `procedural-voxel-building-generation` E_Hybrid + Wonka 2003 adaptive grammar (#2) | C + per-instance noise deformation: edge dither + scatter |

---

## Web-research note

Web-research sources planned in README §2; this experiment relied on prior `procedural-voxel-building-generation` web-research as cross-axis precedent (same author/session). All canonical sources (Parish 2001, Wonka 2003, Müller 2006, Kelly 2006) are documented above.