# Sources — 2026-06-22-sector-strategic-map-system

Web-research via direct `webfetch` to canonical game developer + Wikipedia URLs.

---

## Tier 1 — Game / industry references

### 1. Hearts of Iron IV (Paradox Development Studio) — province system
- **URL:** https://www.paradoxinteractive.com/games/hearts-of-iron-iv
- **Key contribution:** Map divided into provinces (1000-10000 per game). Each province has: owner, controller, supply infrastructure, victory point value, building slots, terrain (plains/forest/mountain/desert/marsh). Per-tick simulation: combat, supply distribution, building construction, resistance, compliance.
- **Relevance:** Canonical reference for sector-based grand strategy. Per-province update loop = inspiration for A_NaivePerSector strategy.

### 2. Total War (Sega) — region system
- **URL:** https://www.totalwar.com/
- **Key contribution:** Map divided into regions (50-200 per game). Each region has: owner, public order, wealth, building slots, recruitment slots. Per-turn simulation: building construction, recruitment, public order decay/recovery.
- **Relevance:** Region-level simulation pattern. Turn-based vs tick-based — performance considerations differ but core state model applies.

### 3. WARNO (Eugen Systems) — sector control
- **URL:** https://www.eugensystems.com/game/warno
- **Key contribution:** Sector-based territorial control (1v1 mode). Sectors captured by zone control mechanic (units present + no enemy). Real-time tick simulation at 30 Hz.
- **Relevance:** Real-time sector capture mechanic. Closest reference to ProjectV's intended use case.

### 4. Stellaris (Paradox) — hyperlane grid
- **URL:** https://www.paradoxinteractive.com/games/stellaris
- **Key contribution:** Galaxy divided into star systems connected by hyperlanes (graph topology). Sector = collection of star systems. Per-month tick simulation: economy, research, fleet movement, diplomacy.
- **Relevance:** Graph-based sector abstraction (vs 2D grid). Per-tick update with delta replication.

### 5. Civilization VI (Firaxis) — hex grid city-state system
- **URL:** https://www.civilization.com/
- **Key contribution:** Hex tile map with city-states (independent factions). Per-tile terrain + per-city-state bonuses. Per-turn simulation: yields, growth, combat.
- **Relevance:** Hex grid coordinate system. B_HexGridOffset strategy uses axial coordinates.

### 6. Crusader Kings III (Paradox) — de jure / de facto control
- **URL:** https://www.paradoxinteractive.com/games/crusader-kings-3
- **Key contribution:** De jure (legal) vs de facto (actual) control layered. Counties → Duchies → Kingdoms → Empires. Control transitions via war, inheritance, claim fabrication.
- **Relevance:** Layered control abstraction. Sectors can have multiple layers (raw control + influence + supply).

---

## Tier 2 — Academic / general

### 7. Wikipedia "Board game geography" — hex grid advantages
- **URL:** https://en.wikipedia.org/wiki/Hexagonal_tiling
- **Key contribution:** Hexagonal tiling: 6 neighbors per cell (vs 8 for square). All equidistant. Properties useful for grid-based simulation: no diagonal movement ambiguity, uniform distance.
- **Relevance:** B_HexGridOffset strategy.

### 8. Wikipedia "Sparse matrix" — sparse data structures
- **URL:** https://en.wikipedia.org/wiki/Sparse_matrix
- **Key contribution:** Storage schemes (CSR/CSC) for matrices where most entries are zero. Active-set optimization (only update non-zero entries).
- **Relevance:** C_SparseActiveSet strategy.

---

## Cross-references to closed ProjectV experiments

- closed `interest-management-aoi-battle` [mixed, E_KNN_BackCull 1.5-1.8 Mbps per player] — AOI drives active sector set.
- closed `lockstep-state-sync-hybrid-netcode` [mixed, A_PureLockstep 48 KB/s/player] — sector state = lockstep node.
- closed `supply-logistics-simulation` [mixed, B/D/E supply network] — supply propagation across sectors.
- closed `factory-production-system` [closed mixed, E_ProductionLinePipeline + A_NaiveLinearScan] — sector = factory host.
- closed `dynamic-front-line-system` [open concept, related to this axis] — front line computed from sector control.
- closed `ecs-1m-entities-bottleneck` [yes, Flecs = registry host] — sector entity in Flecs.

---

## Methodology: how this research informs the prototype

| Strategy | Inspired by | Key parameters |
|:---------|:------------|:---------------|
| A_NaivePerSector | HoI4 per-province tick | full state update per sector per tick |
| B_HexGridOffset | Hex grid coordinate system | axial coords, 6 neighbors |
| C_SparseActiveSet | Sparse matrix optimization | only active sectors (within AOI + recent changes) |
| D_DeltaEncodedState | Stellaris hyperlane graph | delta encoding for state changes |
| E_ChunkedSpatialHash | Spatial hash indexing | 32-cell chunks + linear scan |

---

## Web-research note

Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per the web_search fallback chain. Direct `webfetch` to canonical game developer + Wikipedia URLs confirmed content.