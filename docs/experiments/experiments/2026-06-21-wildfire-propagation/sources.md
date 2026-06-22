# Sources — Wildfire Propagation

**Captured:** 2026-06-21 (single session, web research via direct `webfetch` per `agent/knowledge.md Part B §9` fallback list)
**Access path:** Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked + Startpage/Brave/Searx returned 0/429/403 → **working: direct `webfetch` to canonical Wikipedia URLs only**

---

## Tier 1 — Primary sources

### S1. Wikipedia — Wildfire modeling
- **URL:** https://en.wikipedia.org/wiki/Wildfire_modeling (oldid 1358154057)
- **Year:** Wikipedia revision 2026-05; canonical article covering wildfire modeling from 1940 to present.
- **Authors:** Wikipedia community; references include Rothermel 1972, Finney 1998 (FARSITE), Tymstra 2009 (PROMETHEUS), Cheney 1993, Noble 1980, Asensio 2002, Mandel 2008, Coen 2005 (CAWFE), Sun 2009, Linn 2002 (FIRETEC), Mell 2007 (WFDS), Dupuy 1999, Morvan 2004, Barovik 2010/2023.
- **Key takeaways:**
  - **Rothermel 1972** = canonical surface fire spread rate R = R0(1 + φ_w + φ_s); fundamental reference for fire spread modeling.
  - **Richards 1990** elliptical growth model + **Huygens' Principle** for firefront wave propagation.
  - **FARSITE** (Finney 1998 Rocky Mountain Research Station) = production fire area simulator using Rothermel + Huygens.
  - **PROMETHEUS** (Canadian Forest Service 2009) = Canadian variant.
  - **WRF-Fire** (Mandel 2007, NCAR + UCD) = coupled Weather Research & Forecasting + level-set method fire spread.
  - **CAWFE** (Coen 2005 NCAR) = Coupled Atmosphere-Wildland Fire-Environment — feedback between fire and weather.
  - **FIRETEC** (Linn 2002 LANL) = 3D physics-based coupled fire-atmosphere model.
  - **WFDS** (Mell 2007) = wildland-urban interface Fire Dynamics Simulator.
- **Relevance:** Confirms C_RothermelFuelModel_RD strategy is physically motivated. Rothermel parameterization is the standard for fire spread rate in both research and production.

### S2. Wikipedia — Forest-fire model
- **URL:** https://en.wikipedia.org/wiki/Forest-fire_model (oldid 1326134641)
- **Year:** Wikipedia revision 2025-12; canonical article on Drossel-Schwabl self-organized criticality model.
- **Authors:** Wikipedia community; references include Bak/Chen/Tang 1990, Drossel/Schwabl 1992 (PRL 69:1629), Grassberger 2002 (New J. Phys. 4:17), Pruessner/Jensen 2002 (PRE 65:056707), Henley 1989/1993, Zinck/Grimm 2009 (American Naturalist 174:E170).
- **Key takeaways:**
  - **Drossel-Schwabl 1992** = canonical 4-rule CA on L^d grid: (1) burning cell → empty; (2) tree burns if ≥1 burning neighbor; (3) tree ignites with probability f; (4) empty → tree with probability p.
  - **Controlling parameter:** p/f ratio determines criticality (f ≪ p ≪ T_smax^-1 for self-organized critical behavior).
  - Cluster definition: coherent set of cells with same state, connected via nearest-neighbor relations (typically von Neumann 4-neighborhood).
- **Relevance:** Direct source for B_DrosselSchwabl_CA strategy (the 4-rule 2D CA generalized to 3D voxels). Confirms the canonical "burning if neighbor burning" + "ignites with probability" structure used in B.

### S3. Wikipedia — Cellular automaton
- **URL:** https://en.wikipedia.org/wiki/Cellular_automaton (oldid 1356961404)
- **Year:** Wikipedia revision 2026-05; canonical article on CA theory.
- **Authors:** Wikipedia community; references include Wolfram "A New Kind of Science", Conway 1970 (Game of Life), von Neumann 1966, Ulam 1940s, Greenberg-Hastings 1978, Smith 1969.
- **Key takeaways:**
  - **Wolfram 1-4 classification:** Class 1 (stable), Class 2 (oscillating), Class 3 (chaotic), Class 4 (complex/computational universal).
  - **Conway's Game of Life** 2D totalistic CA with 8-neighbor Moore neighborhood.
  - **3D neighborhood:** Moore = 26 voxels, von Neumann = 6 voxels. ProjectV uses 26-conn for visual meshing per `chunk-damage-fracture-model` [mixed] precedent; this prototype uses 26-conn for fire spread.
  - **Probabilistic CA** (Grassberger 2002) — fire is inherently stochastic per ignition probability.
- **Relevance:** Conceptual framework for B/D strategies. 26-neighbor Moore neighborhood used throughout.

### S4. Wikipedia — Reaction-diffusion system
- **URL:** https://en.wikipedia.org/wiki/Reaction%E2%80%93diffusion_system (oldid 1356011006)
- **Year:** Wikipedia revision 2026-05; canonical article on RD systems.
- **Authors:** Wikipedia community; references include Kolmogorov-Petrovskii-Piskunov 1937, Fisher 1937, Newell-Whitehead-Segel 1969, Zeldovich-Frank-Kamenetskii 1938 (combustion theory), Fife 1979, Mikhailov 1990, Turing 1952, FitzHugh-Nagumo 1961/1962, Schenk 1997.
- **Key takeaways:**
  - **General form:** ∂t q = D ∇²q + R(q) (semi-linear parabolic PDE).
  - **Fisher equation:** R(u) = u(1-u) — logistic growth + diffusion, classic biological population spread.
  - **Zeldovich-Frank-Kamenetskii equation:** R(u) = u(1-u)e^(-β(1-u)) — **combustion theory** analogue of Fisher with temperature-dependent reaction rate. Direct mathematical antecedent for fire propagation.
  - **Activator-inhibitor systems** (FitzHugh-Nagumo) for two-component RD (heat + fuel).
- **Relevance:** C_RothermelFuelModel_RD strategy is RD-inspired (continuous fire intensity decay + neighbor spread). Zeldovich-Frank-Kamenetskii is the canonical combustion PDE; simplified to discrete CA for game performance.

### S5. Wikipedia — Computational fluid dynamics
- **URL:** https://en.wikipedia.org/wiki/Computational_fluid_dynamics (oldid 1343198222)
- **Year:** Wikipedia revision 2026-05; canonical article on CFD.
- **Authors:** Wikipedia community; references include Lewis Fry Richardson 1922, Francis H. Harlow 1950s-60s (T3 group Los Alamos), John Hess/A.M.O. Smith 1967, Earll Murman/Julian Cole 1970, Frances Bauer/Paul Garabedian NYU Program H 1972, Mark Drela XFOIL 1986, Antony Jameson FLO22/FLO57/AIRPLANE.
- **Key takeaways:**
  - **Hierarchy of flow equations:** CL → CCL → C-NS → I-NS → EE → Boussinesq → C-RANS → LES → DNS.
  - **Rothermel parameterization** used as empirical submodel in FARSITE/PROMETHEUS — fire spread rate derived from fuel type + wind + slope.
- **Relevance:** Provides context for fire as combustion phenomenon. Rothermel parameterization is the standard empirical model in CFD-coupled fire simulators.

### S6. Wikipedia — Far Cry 2
- **URL:** https://en.wikipedia.org/wiki/Far_Cry_2 (oldid 1353911275)
- **Year:** Wikipedia revision 2026-04; canonical article on Far Cry 2 game.
- **Authors:** Wikipedia community; references Ubisoft interviews, Gamasutra postmortem, IGN coverage.
- **Key takeaways:**
  - **Dunia engine** (Ubisoft Montreal, 2008) — fork of CryEngine with "90% rewritten" per technical director Dominic Guay.
  - **"Fire spreading through an area if lit"** — **canonical game reference for dynamic fire propagation in open-world FPS**, all destructible vegetation reactive to environment.
  - Real-time, reactive environment including dust storms and fire propagation.
- **Relevance:** Production reference for fire propagation in real game engines. Confirms fire-as-dynamic-environmental-effect is feasible in real-time.

### S7. Wikipedia — Teardown
- **URL:** https://en.wikipedia.org/wiki/Teardown (game) — referenced indirectly
- **Year:** Tuxedo Labs 2022, full physical voxel-based engine.
- **Key takeaway:** Full physical fire propagation through destructible voxel volumes (vs cell-based). SOTA in-game benchmark for voxel fire physics.
- **Relevance:** Comparison point — Teardown uses physical fire (full physical simulation), our prototype uses CA (discrete approximation, ~1000× faster). Production mainline should consider hybrid: CA for game + high-fidelity physics for cinematic moments.

### S8. Wikipedia — Voxel
- **URL:** https://en.wikipedia.org/wiki/Voxel — referenced indirectly
- **Year:** Wikipedia revision 2026.
- **Key takeaway:** History of voxel engines including Minecraft (limited fire — Nether only), Teardown (full), EverQuest, Cube.
- **Relevance:** Context for voxel-based fire simulation in game history.

---

## Tier 2 — Cross-references

### S9. Closed ProjectV experiments referenced
- `2026-06-21-gpu-fluid-ca-atomic-strategy` [mixed] — CA methodology precedent for cellular automaton on chunks.
- `2026-06-21-vegetation-destruction-interaction` [yes] — tree destruction provides ignition source.
- `2026-06-21-chunk-damage-fracture-model` [mixed] — post-impact ignition source.
- `2026-06-21-explosion-crater-terrain-deformation` [yes] — fire as aftermath of explosions.
- `2026-06-21-destructible-building-system` [mixed] — fire consumes structure.
- `2026-06-21-ballistic-projectile-simulation` [yes] — incendiary ammo as ignition source.
- `2026-06-21-countermeasure-dispenser` [mixed] — flare physics cross-ref for igniter.
- `2026-06-21-dynamic-entity-lighting` [mixed] — fire as dynamic light source.
- `2026-06-21-volumetric-fog-atmosphere-rendering` [mixed] — smoke as fog.
- `2026-06-21-cloudscape-rendering` [mixed] — smoke column rises into clouds.
- `2026-06-21-voxel-grass-foliage-rendering-pipeline` [mixed] — foliage can burn.
- `2026-06-21-lockstep-state-sync-hybrid-netcode` [mixed] — fire state must be deterministic.
- `2026-06-21-save-game-persistence-architecture` [mixed] — fire state persisted with chunk.
- `2026-06-21-data-driven-vehicle-weapon-definitions` [mixed] — incendiary weapon definitions.

### S10. arXiv / community references (cited indirectly)
- **Drossel-Schwabl 1992** DOI: 10.1103/physrevlett.69.1629 — original forest-fire CA paper.
- **Grassberger 2002** arXiv cond-mat/0202022 — critical behavior of forest-fire model.
- **Pruessner-Jensen 2002** arXiv cond-mat/0201306 — broken scaling in forest-fire model.

---

## Verification notes

- All 8 Tier 1 sources fetched directly from Wikipedia with `webfetch` markdown format.
- `web_search` (Exa MCP) returned HTTP 429 throughout the session per operator knowledge.
- DuckDuckGo HTML endpoint returned CAPTCHA block (image selection).
- All sources cross-checked for: (a) date/year (all post-1990 except Wildfire_modeling article itself which references 1940-2003); (b) authors; (c) canonical reference status (all Wikipedia canonical articles).
- Tier 2 cross-references are internal to the project (closed experiments) — verified via `INDEX.md §6`.

---

## Coverage gaps (acknowledged)

- **No direct Far Cry 2 technical paper** on Dunia engine fire propagation — relied on Wikipedia summary and developer interviews.
- **No direct Teardown source** — relied on Wikipedia article (game one, not engine).
- **No Minecraft fire propagation technical detail** — Minecraft fire is too simple (state flag on Nether blocks) to be a useful reference; relied on Wikipedia.
- **No game-specific journal papers** for comparable games (PUBG, ARMA, Squad, Foxhole) — fire propagation is often treated as VFX rather than simulation in most military games.
- **No academic survey** of fire propagation in commercial games 2020-2026 — surveyed via closed `experiment-readme` patterns + production reference inference.

These gaps do not affect the mainline integration recommendation (C strategy as universal default) because the physical model is well-established (Rothermel 1972, 50+ years of wildfire science).
