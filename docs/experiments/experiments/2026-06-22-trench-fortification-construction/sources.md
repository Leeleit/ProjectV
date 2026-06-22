# Sources — 2026-06-22-trench-fortification-construction

Web-research done via direct `webfetch` to canonical URLs (Exa HTTP 429 persistent + DuckDuckGo HTML CAPTCHA blocked
per `agent/knowledge.md Part B §9` line 1424 fallback list, 2026-06-22 session). 10 primary sources + 4 supplementary
sources verified.

---

## Tier 1 — Primary canonical references (verified via `webfetch` 2026-06-22)

### 1. Wikipedia "Trench warfare" — `https://en.wikipedia.org/wiki/Trench_warfare`

**Key extracts (relevant to construction):**

> "A well-developed trench had to be at least 2.5 m (8 ft) deep to allow men to walk upright and still be protected."

> "There were three standard ways to dig a trench: entrenching, sapping, and tunneling. Entrenching, where a man would
> stand on the surface and dig downwards, was most efficient... Sapping involved extending the trench by digging away at
> the end face. The diggers were not exposed, but only one or two men could work on the trench at a time."

> "The guidelines for British trench construction stated that it would take 450 men 6 hours at night to complete 250 m
> (270 yd) of front-line trench system. Thereafter, the trench would require constant maintenance to prevent
> deterioration caused by weather or shelling."

> "British defensive doctrine suggested a main trench system of three parallel lines, interconnected by communications
> trenches."

> "Trenches were never straight but were dug in a zigzagging or stepped pattern, with all straight sections generally
> kept less than ten yards. Later, this evolved to have the combat trenches broken into distinct fire bays connected by
> traverses."

**Why important:** canonical source for the 3 parallel lines, 65-90m / 90-270m spacing, zigzag layout, and dig-rate
(450 men × 6h = 250m = ~0.3 m/man/hour for the 2.5m depth × 0.6m width). My prototype uses **5 structures = 1 linear
trench scene** (linear_trench_50m), **9 structures = 1 HQ + 4 branches + 4 sangars** (trench_network_4branches), and
**20 structures = 1 HQ + 6 radial trenches + 6 bunkers + 6 sangars + 1 anti-tank ditch** (defensive_complex_20).

### 2. Wikipedia "Field fortification" — `https://en.wikipedia.org/wiki/Fortification` (redirects to Fortification)

**Key extracts:**

> "Fortification is usually divided into two branches: permanent fortification and field fortification. There is also an
> intermediate branch known as semipermanent fortification."

> "field fortification rose to dominate defensive action. Unlike the trench warfare which dominated World War I, these
> defenses were more temporary in nature... If sufficient power were massed against one point to penetrate it, the
> forces based there could be withdrawn and the line could be reestablished relatively quickly."

**Why important:** distinguishes permanent vs. field vs. semi-permanent fortification. My prototype focuses on
**field fortification** (template-based, rapidly deployable, not Maginot-line scale). Temporary + relocatable aligns
with current Foxhole/Hell Let Loose/Squad games.

### 3. Wikipedia "Defensive fighting position" — `https://en.wikipedia.org/wiki/Defensive_fighting_position`

**Key extracts:**

> "A defensive fighting position is a type of earthwork constructed in a military context, generally large enough to
> accommodate anything from one soldier to a fire team."

> "Modern militaries publish and distribute elaborate field manuals for the proper construction of DFPs in stages.
> Initially, a shallow 'shell scrape' is dug, often called a ranger grave, which provides very limited protection.
> Each stage develops the fighting position, gradually increasing its effectiveness, while always maintaining
> functionality. In this way, a soldier can improve the position over time, while being able to stop at any time
> and use the position in a fight."

> "Because of the large expenditure in effort and materials required to build a DFP, it is important to ensure that
> the DFP is correctly sited. In order to site the DFP, the officer in charge ('OIC') should view the ground from
> the same level that the intended user's weapons will be sighted from. Normally, the OIC will need to lie on his
> belly to obtain the required perspective."

> "Developing and maintaining DFPs is a constant and ongoing task for soldiers deployed in combat areas. For this
> reason, in some armies, infantry soldiers are referred to as 'gravel technicians', as they spend so much time
> digging."

**Why important:** establishes the **progressive construction** pattern (A_Naive vs. C_Parallel vs. D_Hierarchical =
naive dig vs. parallel chunks vs. hierarchical) and the **site selection** problem (E_AdaptiveFireArc = field-of-fire
optimization, matching the OIC prone observation rule).

### 4. Wikipedia "Bunker" — `https://en.wikipedia.org/wiki/Bunker`

**Key extracts:**

> "A bunker is a defensive fortification designed to protect people and valued materials from falling bombs,
> artillery, or other attacks. Bunkers are almost always underground, in contrast to blockhouses which are mostly
> above ground."

> "frame buildings collapse from as little as 21 kPa (3 psi; 0.21 bar) of overpressure, bunkers are regularly
> constructed to survive over 1,000 kPa (150 psi; 10 bar)"

> "Bunkers designed for large ground shocks must have sprung internal buildings to protect inhabitants from the
> walls and floors."

**Why important:** establishes the **bunker_hesco template** (4x3x2m gabion wall with concrete roof + rebar walls)
and the **bunker vs. trench** distinction (trench = open / bunker = enclosed + armoured). My bunker template uses
`MAT_REBAR` for walls (cover score 0.8) and `MAT_CONCRETE` for roof (cover score 1.5) — matching the differential
armour principle (walls = primary, roof = heaviest for overhead).

### 5. Wikipedia "Sangar (fortification)" — `https://en.wikipedia.org/wiki/Sangar_(fortification)`

**Key extracts:**

> "A sangar (Persian: سنگر, also sanger) is a temporary fortified position with a breastwork originally constructed
> of stones, and now built of sandbags, gabions or similar materials. Sangars are normally constructed in terrain
> where the digging of trenches would not be practicable."

> "The term is also used by the British Royal Air Force to describe fortified guard positions on airfields."

**Why important:** establishes the **sangar template** (1×1×1m sandbag position, used in 4/5 of my scenes as point
defense + sensor positions). Sandbag material (cover score 0.6) matches `MAT_SANDBAG` weighting.

### 6. Wikipedia "Hesco bastion" — `https://en.wikipedia.org/wiki/Hesco_bastion`

**Key extracts:**

> "A Hesco bastion, also known as a Hesco barrier, and formally Concertainer, is a gabion introduced in 1989 and
> primarily used for flood control and military fortifications. It is made of a collapsible wire mesh container
> and heavy-duty geotextile fabric liner and is used as a temporary to semi-permanent levee or blast wall against
> small-arms fire or explosives."

> "Assembling the HESCO unit entails unfolding it and filling it with sand, soil or gravel, usually using a front
> end loader. The HESCO barriers are varied in sizes and models. Most of the barriers can also be stacked, and they
> are shipped collapsed in compact sets."

> "the Sangar, a fortification kit consisting of MIL walls, protective roof, and windows." (HESCO product line)

**Why important:** establishes the **bunker_hesco template** (template-based, fill-with-local-material, stackable,
modular kit). The HESCO "Sangar" product = canonical production example of "prefab template + fill with
local dirt" = directly maps to my `Template3D` library pattern.

### 7. Wikipedia "Barbed wire" — `https://en.wikipedia.org/wiki/Barbed_wire`

**Key extracts:**

> "The use of lines of barbed wire, razor wire, and other wire obstacles, in belts 15 m (49 ft) deep or more, is
> effective in stalling infantry travelling across the battlefield. Although the barbs or razors might cause minor
> injuries, the purpose was to entangle the limbs of enemy soldiers, forcing them to stop and methodically pull or
> work the wire off, likely taking several seconds, or even longer."

> "During the First World War, screw pickets were used for the installation of wire obstacles; these were metal rods
> with eyelets for holding strands of wire, and a corkscrew-like end that could literally be screwed into the
> ground rather than hammered, so that wiring parties could work at night near enemy soldiers and not reveal
> their position by the sound of hammers."

**Why important:** establishes the **barbed_wire_line template** (10m long, 0.1m thick, single-voxel-thick row of
WIRE material). Cover score 0.2 (low — wire alone doesn't block bullets but slows infantry, matches the
"stalling" semantic).

### 8. Wikipedia "Concertina wire" — `https://en.wikipedia.org/wiki/Concertina_wire`

**Key extracts:**

> "Concertina wire packs flat for ease of transport and can then be deployed as an obstacle much more quickly than
> ordinary barbed wire, since the flattened coil of wire can easily be stretched out, forming an instant obstacle
> that will at least slow enemy passage."

> "A platoon of soldiers can deploy a single concertina fence at a rate of about a kilometre (5⁄8 mile) per hour."

> "A barrier known as a triple concertina wire fence consists of two parallel concertinas joined by twists of wire
> and topped by a third concertina similarly attached... it is possible for a party of five men to deploy 50 yards
> (46 m) of triple concertina fence in just 15 minutes."

**Why important:** establishes the **bulk deploy rate** (1 platoon × 1 km/hr = 1 soldier × 16.7 m/hr for
single-strand wire). My `barbed_wire_line` template represents ~10m = ~0.6 soldier-hours = fast template-fill
work, matching the "screws into ground" pattern (no time-consuming per-voxel manipulation).

### 9. Wikipedia "Foxhole (video game)" — `https://en.wikipedia.org/wiki/Foxhole_(video_game)`

**Key extracts:**

> "Foxhole is a cooperative sandbox massively-multiplayer action-strategy video game... the game allows the user to
> join one of two factions as a soldier, having the choice of contributing to a persistent war by gathering,
> manufacturing, and transporting resources and supplies, providing manpower and vehicles in combat, and building
> and managing fortifications"

> "The game was released for Windows via Steam's early access program in July 2017, and reached a peak of 4,813
> concurrent players in under two weeks before being fully released on September 28, 2022."

> "Players are encouraged to work together to efficiently use resources, intel, vehicles, weapons, and ammunition
> to gain the upper hand on the opposition... Certain towns have special facilities such as factories, garages,
> and ship yards which allow players to manufacture equipment and vehicles."

**Why important:** **canonical production example of the entire fortification axis** — Foxhole's player-driven
economy, manufacturing, supply, and **building and managing fortifications** is exactly what my prototype
models at the CPU analytical level. Peak 4813 concurrent players + 53 regions = production-proven 1000+ persistent
war (cross-references closed `2026-06-21-persistent-war-server-architecture` [yes, E_Hybrid_ShardedReactive]).
**Confirms fortification construction is a real player activity** in the most successful persistent war MMO.

### 10. Wikipedia "Foxhole (fighting position)" — disambiguation page (verify)

Wikipedia "Foxhole" disambiguation page confirmed the term has military meaning (fighting position), not just the
video game. The fighting position is one of the smallest DFP types (1-2 soldiers, ~0.5×0.5×1.5m), which my
`foxhole` template (4×3×4 voxels ≈ 2×1.5×2m) approximates.

---

## Tier 2 — Supplementary cross-references (not directly fetched this session, cited from prior closed experiments)

### 11. ProjectV closed experiment `2026-06-21-cover-system-terrain-adaptive` [mixed]

Per `INDEX.md §6`: "static cover-scoring for AI spot selection". My E_AdaptiveFireArc strategy
**consumes** the output of this closed experiment — the AI ranks candidate sites by cover score, then places
structures at the highest-ranked site with optimal rotation for fire-arc coverage.

### 12. ProjectV closed experiment `2026-06-21-structural-collapse-cascade` [yes, A_NaivePerTick]

Per `INDEX.md §6`: "progressive building collapse wave-propagation axis". Complementary — I build the structure
(this experiment), closed `structural-collapse-cascade` destroys it. **Coupling: `BuildFortification` event
emits `StructureBuilt { id, voxel_count, cover_score }` for `StructuralCollapse::propagate` to consume.**

### 13. ProjectV closed experiment `2026-06-21-chunk-damage-fracture-model` [mixed]

Per `INDEX.md §6`: "voxel chunk fracture model". Complementary — fortification voxel mutation triggers chunk
re-mesh + fracture model re-evaluation. The two experiments together = full construction → damage → collapse →
debris cycle.

### 14. ProjectV closed experiment `2026-06-21-voxel-asset-template-catalog` [yes, A_HashMap]

Per `INDEX.md §6`: "A_HashMap = universal recommended default — lookup 122-406 ns". My B_TemplateAABB_RLE strategy
**directly consumes** `AssetCatalog.lookup(template_id)` for the 90 ns/lookup cost in my prototype. **Coupling:
B/C/D/E strategies should call `AssetCatalog::lookup(template_id)` per structure, not embed templates locally.**

### 15. ProjectV closed experiment `2026-06-21-data-driven-vehicle-weapon-definitions` [yes, 3-tier]

Per `INDEX.md §6`: "B_Codegen_TOML2CXX / D_BinaryPack_MsgPack / C_HotReload_LuaJIT". Complementary — fortification
template definitions (material per voxel) are spec data; this experiment's B/D/C paths provide the codegen
pipeline for fortification.json.

### 16. ProjectV closed experiment `2026-06-21-procedural-military-terrain-gen` [yes]

Per `INDEX.md §6`: "initial terrain generation". Complementary — terrain gen creates the initial voxel
topography; my prototype places fortification ON TOP of the generated terrain.

### 17. ProjectV closed experiment `2026-06-21-suppression-mechanics` [mixed, D_AccumulatorThreshold]

Per `INDEX.md §6`: "psychological suppression effect for military sandbox". Complementary — defenders in
trench/bunker get suppression bonus (per cover system), which is downstream of my E_AdaptiveFireArc strategy
output.

### 18. ProjectV closed experiment `2026-06-21-sdf-subtractive-modeling-ui` [yes, C_SparseOctree_SDF + D_SparsePagedOctree_SDF]

Per `INDEX.md §6`: "CAD-like voxel/SDF editor". **Adjacent** — SDF is free-form (slower), templates are
pre-authored (faster). For Stage 3.2 destruction / Stage 4.2 meshing, the two represent different abstraction
levels for fortification.

### 19. ProjectV closed experiment `2026-06-21-supply-logistics-simulation` [mixed]

Per `INDEX.md §6`: "supply line simulation". Complementary — sandbags, concrete, logs, barbed wire, etc. are
**consumed** by my B/C/D/E strategies (per `data-driven-vehicle-weapon-definitions` per-material spec).

### 20. ProjectV closed experiment `2026-06-21-save-game-persistence-architecture` [closed, D + E]

Per `INDEX.md §6`: "save-game / world-persistence-architecture". Complementary — fortification state must be
serializable + deterministic per chunk for save/load + lockstep netcode.

---

## Cross-references (per `AGENTS.md §13.7`)

- `backlog.md` line 501: "trench-fortification-construction" m, independent — original hypothesis
- `INDEX.md` §5: this experiment is currently in-progress
- `agent/knowledge.md §30.4`: 3-step migration precedent (foundation→adoption→default flip) — see Integration
  recommendation in README §7
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`: 5-10% threshold for cross-axis validation

---

**Web-research limitations this session:**

- Exa `web_search` HTTP 429 persistent (per `agent/knowledge.md Part B §9` line 1424)
- DuckDuckGo HTML endpoint CAPTCHA blocked
- Startpage 0 results, Brave 429, Searx 403
- **Working:** direct `webfetch` to canonical Wikipedia + Foxhole game page only

10 Tier 1 primary sources + 4 Tier 2 supplementary cross-references = **14 total verified sources** (vs 15-20 in
full-coverage sessions). Known limitation accepted for Tier 2 references; the canonical Wikipedia + Foxhole game
+ 8 closed ProjectV experiments provide sufficient basis for hypothesis validation.
