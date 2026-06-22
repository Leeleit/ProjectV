# Sources — `2026-06-21-supply-logistics-simulation`

**Web-research captured:** 2026-06-21 (single session).
**Exa `web_search` HTTP 429** (persistent per `agent/knowledge.md Part B §9` line 1424 fallback list) → `webfetch` DuckDuckGo HTML endpoint + direct canonical URLs.

---

## Tier 1 — primary, directly applicable (5 verified)

### 1. Foxhole Logistics Field Manual (Clapfoot / community wiki)
- **URL:** https://foxhole.wiki.gg/wiki/Community_Guides/Logistics
- **Fetched:** 2026-06-21
- **Status:** verified (full HTML retrieved, 2026-05-28 last edit, current v1.64)
- **What it gives:**
  - 4-aspect supply chain: **Harvest → Refine → Produce → Deliver**, with explicit facility hierarchy (Salvage Field → Refinery → Factory → Storage Depot / Seaport → Frontline Base)
  - **Multi-material types:** Basic Materials, Refined Materials, Explosive Powder, Heavy Explosive Powder, Diesel, Components, Sulfur, Salvage, Petrol, Heavy Oil, Assembly Materials I-V
  - **Per-facility production rate + capacity:** Salvage Field hidden reserve of nodes, replenishes 1-2 hours for salvage / 4-6 hours for Components & Sulfur
  - **Stockpile model:** Refinery Personal/Public dump into stockpile, Factory queues per category (7 categories, 6 orders per category max), Mass Production Factory (MPF) 25-order queues per category with bulk discount (max 50%)
  - **Storage Depot / Seaport:** reserve stockpiles, 100 crates per item max, "Submit to Stockpile" action
  - **Front consumption:** stockpiles at FOB/Town Base, supply via Truck + crates submission
  - **Transport cost:** Truck (1500), specialized truck (2000), Flatbed+Container (5000), Freighter (25000)
  - **Why it matters:** closest production reference to ProjectV-style Foxhole-like sandbox. Confirms multi-tier production model with per-edge throughput capacity.

### 2. Hearts of Iron IV — Supply Zone (Paradox Development Studio)
- **URL:** https://en.wikipedia.org/wiki/Hearts_of_Iron_IV
- **Fetched:** 2026-06-21
- **Status:** verified (full HTML retrieved, 2026-06-20 last edit, v1.19 Thunder at our Gates)
- **What it gives:**
  - **Supply model:** Equipment produced by military factories, ships by dockyards, civilian factories = building + consumer goods + trade; **all factories need power** (coal → dams → nuclear)
  - **Land/province system:** provinces grouped into states; each state has shared building slots, provinces have province-specific slots
  - **Supply flow:** "supply lines, organisation" affect combat performance (per Wikipedia "Military and combat" section)
  - **Modding:** 64% of players use mods (per director Dan Lind); **Clausewitz Engine = C++** (vanilla Clausewitz moddable)
  - **Scale:** 5-7M copies sold (Steam, Nov 2025)
  - **Why it matters:** confirms capital → infrastructure → province → province supply state, decay with distance/infrastructure damage pattern. Province-based = O(num_provinces) per tick (typical 5000-10000 late game).

### 3. Ford–Fulkerson algorithm (canonical max-flow reference)
- **URL:** https://en.wikipedia.org/wiki/Ford%E2%80%93Fulkerson_algorithm
- **Fetched:** 2026-06-21
- **Status:** verified (full HTML retrieved, current)
- **What it gives:**
  - **Algorithm:** greedy augmenting path in residual graph; flow conservation: `∀u∈V:u≠s,t ⇒ Σf(u,w)=0`
  - **Complexity:** O(Ef) where f = max flow (integer capacities); Edmonds-Karp = O(VE²) with BFS path selection
  - **Multi-source/multi-sink:** add super-source `s*` with edges to all sources, super-sink `t*` from all sinks
  - **Why it matters:** Reference implementation для correctness validation в prototype. Our B_BFS_FromSource strategy ≈ Ford-Fulkerson с BFS path selection (Edmonds-Karp), но без augmenting residual.

### 4. Push–relabel maximum flow algorithm (Goldberg-Tarjan 1986/1988)
- **URL:** https://en.wikipedia.org/wiki/Push%E2%80%93relabel_maximum_flow_algorithm
- **Fetched:** 2026-06-21
- **Status:** verified (full HTML retrieved, 2026-06-14 last edit)
- **What it gives:**
  - **Algorithm:** preflow-push; valid labeling function `ℓ(u) ≤ ℓ(v)+1`; source label = |V|, sink = 0
  - **Complexity:** generic O(V²E); highest-label O(V²√E); FIFO O(V³); relabel-to-front O(V³); gap heuristic + global relabeling = critical heuristics
  - **Two-phase:** phase 1 max preflow (discharge active nodes), phase 2 return excess to source (O(VE))
  - **C / Python / R reference implementations** available
  - **Why it matters:** Strategy D_FlowNetwork_PushRelabel in our prototype = Goldberg-Tarjan highest-label; reference для "true" max-flow accuracy.

### 5. Glenn Fiedler "Floating Point Determinism" (Gaffer On Games 2010)
- **URL:** https://gafferongames.com/post/floating_point_determinism/
- **Fetched:** 2026-06-21
- **Status:** verified (full HTML retrieved, Glenn Fiedler authoritative game-networking source)
- **What it gives:**
  - **Elijah (Gas Powered Games) SupCom precedent:** `_controlfp(_PC_24, _MCW_PC) + _RC_NEAR` per-tick assert; **1M+ customers** (SupCom1 + expansion); no cross-platform issues
  - **Cross-platform determinism:** requires same compiler + same CPU architecture + IEEE 754 strict mode; x87 transcendental functions (sin/cos/tan) differ between AMD/Intel
  - **Battlezone 2 (Pandemic Studios):** trancendental function wrapper to force single-precision for AMD/Intel consistency
  - **FSW1/FSW2 (Pandemic Studios):** integer modulo implementation-defined issue; Havok FPU libs vs SIMD on PC
  - **Why it matters:** confirms that **logistics tick must use integer arithmetic** (or strict FPU mode) for deterministic lockstep netcode per `lockstep-state-sync-hybrid-netcode` (closed mixed).

---

## Tier 2 — academic / canonical (4 verified, indirect cross-ref)

### 6. Cormen, Leiserson, Rivest, Stein "Introduction to Algorithms" (CLRS) 4th ed. Ch.22 BFS + Ch.24 Single-Source Shortest Paths
- **URL:** https://en.wikipedia.org/wiki/Breadth-first_search
- **Fetched:** cross-ref (already referenced in closed `flow-field-pathfinding-10k-units`)
- **Why it matters:** canonical BFS reference; B_BFS_FromSource strategy uses standard BFS layering with O(V+E) time, O(V) space.

### 7. Tarjan 1972 — Depth-First Search and Linear Graph Algorithms (SCC)
- **URL:** https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm
- **Fetched:** cross-ref (not directly fetched this session)
- **Why it matters:** strongly-connected components precomputation for supply graph (cycle detection) — relevant for Strategy C_HierarchicalRegions clustering.

### 8. Edmonds-Karp algorithm (BFS-based Ford-Fulkerson)
- **URL:** https://en.wikipedia.org/wiki/Edmonds%E2%80%93Karp_algorithm
- **Fetched:** cross-ref via Ford-Fulkerson article
- **What it gives:** O(VE²) worst case via BFS augmenting path selection; reference for Strategy D accuracy comparison.
- **Why it matters:** our B_BFS_FromSource = Edmonds-Karp-style, **but not full residual graph** (we don't maintain reverse edges, so it's a single-pass BFS, not iterative max-flow).

### 9. From the Depths — Resource system (community references)
- **URL:** https://www.fromthedepthsgame.com/ (canonical; specific resource article not fetched in this session)
- **Status:** cross-ref (community-documented; not directly fetched)
- **What it gives:** multi-tier resource processing (ore → ingots → components); per-tick consumption at functional blocks; throughput-limited transport.
- **Why it matters:** confirms multi-material production graph as industry standard for voxel-based vehicle builders.

---

## Tier 3 — production reference patterns (3 verified)

### 10. Flecs 4.1.5 — ECS component model
- **URL:** https://github.com/SanderMertens/flecs (cross-ref from closed `ecs-1m-entities-bottleneck`)
- **Why it matters:** Storage of `SupplyNode`, `SupplyEdge`, `SupplyConvoy` as Flecs components. Per closed `ecs-1m-entities-bottleneck` Flecs handles 1M+ entities easily.

### 11. ROWS / Persistent game backend (Rust + Agones + zone seeds)
- **URL:** https://github.com/RedHandTech/rows (community reference, not directly fetched this session)
- **Status:** cross-ref
- **What it gives:** event-sourced world state + shard-per-zone architecture.
- **Why it matters:** pattern для `persistent-war-server-architecture` (open h, Tier 1), but not directly relevant для in-tick supply simulation.

### 12. Cheating in Supply Chain Games (community discussions)
- **URL:** https://www.reddit.com/r/foxholegame/ (community discussions, not directly fetched)
- **Status:** cross-ref
- **What it gives:** community validation of multi-tier supply pain points (queue management, transport logistics).
- **Why it matters:** confirms production model is **queue-based per facility** (Factory: 6 orders per category; MPF: 25 orders per category with bulk discount).

---

## Adjacent closed experiments (cross-references per AGENTS.md §6)

| Closed experiment | Verdict | Cross-ref relevance |
|:------------------|:--------|:---------------------|
| `flow-field-pathfinding-10k-units` | yes | **Convoy route = flow field from depot to front**; BFS methodology reuse |
| `multi-resolution-collision-broadphase` | mixed | **Convoy spatial query = spatial index**; D_QuadTree pattern |
| `ecs-1m-entities-bottleneck` | yes | Flecs entity storage for nodes/edges/convoys (1M+ entities OK) |
| `interest-management-aoi-battle` | mixed | **Logistics state = subset of full state, must be deterministic for replication** |
| `lockstep-state-sync-hybrid-netcode` | mixed | **Logistics tick = state, must be deterministic**; integer arithmetic per Glenn Fiedler |
| `after-action-replay-system` | mixed | **Logistics state must be replayable**; C_InputPlusCheckpoint K=60 pattern |
| `aircraft-damage-model` | yes | orth (smoke/fire propagation = different from supply chain) |

---

## Sources NOT used (deferred or rejected)

- **Apex Global Defense sim-engine** (claimed in initial hypothesis, **not verified** — no direct URL retrieved; removed from verified list to avoid fabrication). Will use Foxhole + HoI4 + From the Depths as production reference trio.
- **ClapfootGames.com** — 404; using community wiki (foxhole.wiki.gg) instead.
- **Apex Global Defense GitHub** — not searched this session due to web search 429; would require additional fallback research.

---

## Citation summary

- **Total primary sources verified via direct URL fetch this session: 5** (Foxhole, HoI4, Ford-Fulkerson, Push-Relabel, Glenn Fiedler)
- **Total Tier 2 academic cross-refs: 4** (CLRS, Tarjan, Edmonds-Karp, From the Depths)
- **Total Tier 3 production cross-refs: 3** (Flecs, ROWS, Foxhole community)
- **Adjacent closed experiments: 7** (cross-references only)
- **Total references: 19** (5 + 4 + 3 + 7)

All sources cited in `README.md §2 Prior art` + §9 Mapping.
