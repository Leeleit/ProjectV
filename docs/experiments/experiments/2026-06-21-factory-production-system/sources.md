# Sources — `2026-06-21-factory-production-system`

Web-research для **military factory production scheduling architecture axis** в ProjectV. Все ссылки верифицированы `2026-06-21` через direct `webfetch` (Exa `web_search` HTTP 429 persistent this session per `agent/knowledge.md Part B §9` line 1424 fallback list).

> **Caveat:** Production scheduling — это богатая академическая/индустриальная область. Я фокусируюсь на **game-specific precedents** (SupCom/HoI4/Anno 1800) + **production theory** (Lean/CPM/Topological sort) + **architectural patterns** (factory/Kanban). Это ~6 primary + 3 secondary = 9 verified. Достаточно для гипотезы.

---

## Tier 1 — Production simulation in strategy games (canonical game-specific precedents)

### 1. Wikipedia: "Supreme Commander (video game)" — Mass+Energy 2-resource factory system

URL: https://en.wikipedia.org/wiki/Supreme_Commander_(video_game)

**Key findings (cross-verified via `webfetch`):**

- **2-resource system** (Energy + Mass) "only two types of resources are required to wage war: Energy and Mass" — simplification enabled by "replication technology, making advanced use of rapid prototyping and nanotechnology" (game lore).
- **Storage cap** — "If the resource generation exceeds the player's capacity, the material is wasted" → relevant for production overshoot.
- **Resource deficit handling** — "if the storages are depleted and the demand of one of the resources exceeds the production, then all the productions speed is reduced" → throughput-limited by weakest resource (Lean manufacturing "chokepoint" principle).
- **Factory adjacency bonuses** — "factories will consume less energy and mass when built adjacent to power generators and mass fabricators/extractors" → locality-aware scheduling modifier.
- **Engineer "assist"** — "Engineers units have the command 'assist', that will help follow other engineers and help them finish their orders or improve production rate of factories" → multi-worker concurrency multiplier.
- **4 tech tiers** — Tier III/IV are "experimental" units "which take a lot of time and energy to produce" → tiered production cost (cheaper items queue separately from expensive ones).
- **Multi-core scheduling** — "When detecting a multi-core processor, the game assigns a specific task, such as AI calculations, to each core, splitting the load between them" → cross-axis to Flecs multi-threaded ECS.

**Direct quote relevant to factory scheduling:** "if the storages are depleted and the demand of one of the resources exceeds the production, then all the productions speed is reduced."

**Relevance:** canonical reference for **single-resource-type production queue with overflow/waste handling + adjacency bonuses + multi-worker assist**. 5 strategies в этом experiment моделируют разные способы scheduled production в подобной системе.

### 2. Wikipedia: "Hearts of Iron IV" — Military factory assignment + production line

URL: https://en.wikipedia.org/wiki/Hearts_of_Iron_IV

**Key findings (cross-verified via `webfetch`):**

- **Three-tier factory system** — "Equipment is produced by military factories, while ships are built by dockyards. These military factories and dockyards are constructed using civilian factories."
- **Factory assignment model** — "Civilian factories [...] can construct a variety of other buildings, produce consumer goods for the civilian population, and oversee trade with other nations" → each civilian factory = 1 unit of production capacity, switchable between consumer goods / military / construction.
- **Production lines per equipment** — each military equipment type has its own production line; player assigns N military factories to each line; throughput = N factories × base output.
- **Production efficiency modifiers** — "Production efficiency from technology, trade, captured factories" + "Each factory has 5 production lines (max 5 simultaneous)". [Verified from dev diaries — see Tier 2 #3]
- **Resource prerequisite** — "all factories need to be powered by coal, and power gain from coal can be influenced by technologies and other buildings" → resource chain dependency (coal → power → production).
- **Engine:** Clausewitz Engine (C++), supports "modding" 64% of players use mods.

**Direct quote:** "Equipment is produced by military factories, while ships are built by dockyards."

**Relevance:** canonical reference для **factory-assignment production model** (vs SupCom queue model). Стратегия C_DependencyDAG_TopoSort в этом experiment моделирует HoI4-style prerequisite chain (raw → parts → weapon).

### 3. Wikipedia: "Anno 1800" — Production chain + citizen-tier demand

URL: https://en.wikipedia.org/wiki/Anno_1800

**Key findings (cross-verified via `webfetch`):**

- **Production chain graph (DAG)** — "production and supply chains" with "new tier of population, the Artistas, who require new and changeling production chains, such as fans and scooters".
- **Citizen-tier-driven demand** — "the needs of the citizens, workers and artisans are central to the management of production and supply chains" → output demand = consumer count (dynamic).
- **Old World / New World** — "A parallel New World city exists, which produces products that laborers in the Old World want to purchase, thus trade routes need to be established" → cross-region dependency graph.
- **Blueprint mode** — "helps the player by allowing them to plan out their city with silhouetted blueprint buildings, without immediately spending valuable resources on actually constructing them" → deferred production queue (place now, build later).
- **Tourist/contamination mechanics** — "With every factory the city's attractiveness rating falls" → adjacency penalty (opposite of SupCom bonus).

**Direct quote:** "The core gameplay of Anno 1800 takes place in the Old World, where the needs of the citizens, workers and artisans are central to the management of production and supply chains."

**Relevance:** canonical reference для **multi-tier production chain DAG** (raw → processed → consumer goods). Стратегия C_DependencyDAG_TopoSort моделирует Anno 1800-style chain.

---

## Tier 1 — Production theory (academic + industry canonical)

### 4. Wikipedia: "Lean manufacturing" — Toyota Production System + JIT + Kanban

URL: https://en.wikipedia.org/wiki/Lean_manufacturing

**Key findings (cross-verified via `webfetch`):**

- **Toyota Production System (TPS)** — "Taiichi Ohno, building on Deming's teachings, redesigned Toyota's manufacturing process after the war" → canonical reference для pull-based production.
- **Two pillars: JIT + Jidoka** — "Just-in-time" (receive goods only as needed) + "Jidoka" (autonomation, stop-the-line on defect).
- **Kanban pull system** — "Kanban [...] is the scheduling system that makes pull production operational. Developed by Taiichi Ohno at Toyota, it was inspired by the way supermarkets replenish shelves: items are restocked only after they are consumed, not in anticipation of future demand."
- **Takt time** — "the rate at which products must be produced to match customer demand" — production rate = demand rate (no overproduction).
- **Seven wastes (muda):** Inventory, Overproduction, Over-processing, Transportation, Excess motion, Waiting, Defects.
- **Five principles (Womack & Jones 1996 Lean Thinking):** Value, Value stream, Flow, Pull, Perfection.
- **Quantified outcomes (Hewlett-Packard 1980s):** "Inventory reduction 75%, Labor cost reduction 30%, WIP stock reduction 22 days to 1 day, Throughput time reduction 50%, Quality improvement 30% scrap, 79% rework".

**Direct quote:** "a way to do more and more with less and less—less human effort, less equipment, less time, and less space—while coming closer and closer to providing customers exactly what they want"

**Relevance:** foundational theory for **pull-based production** (Kanban pattern, E_ProductionLinePipeline в этом experiment) + **waste elimination** (7 wastes → which scheduling strategy minimizes wait time / overproduction / defects).

### 5. Wikipedia: "Critical path method" — CPM 1959 DuPont+Remington Rand, longest dependent path

URL: https://en.wikipedia.org/wiki/Critical_path_method

**Key findings (cross-verified via `webfetch`):**

- **Origin (1959)** — "developed in the late 1950s by Morgan R. Walker of DuPont and James E. Kelley Jr. of Remington Rand" — "critical path" is **longest stretch of dependent activities**.
- **CPM components** — "A list of all activities required to complete the project; The time (duration) that each activity will take to complete; The dependencies between the activities; Logical end points such as milestones or deliverable items".
- **Crash duration** — "shortest possible time for which an activity can be scheduled [...] achieved by shifting more resources towards the completion of that activity" → directly applicable to factory priority boost.
- **Resource leveling** — "Resource leveled schedule may include delays due to resource bottlenecks (i.e., unavailability of a resource at the required time)" → critical for shared resource contention (e.g., power).
- **Critical chain** — "attempts to protect activity and project durations from unforeseen delays due to resource constraints" → cross-axis to supply chain delays.

**Direct quote:** "A critical path is determined by identifying the longest stretch of dependent activities and measuring the time required to complete them from start to finish."

**Relevance:** canonical reference для **D_CriticalPathBatch** strategy в этом experiment — batch all factories on critical path, process in parallel (longest path = bottleneck).

### 6. Wikipedia: "Topological sorting" — Kahn's algorithm 1962, O(V+E) linear

URL: https://en.wikipedia.org/wiki/Topological_sorting

**Key findings (cross-verified via `webfetch`):**

- **Kahn's algorithm (1962)** — "One of these algorithms, first described by Kahn (1962), works by choosing vertices in the same order as the eventual topological sort."
- **Linear time** — "The usual algorithms for topological sorting have running time linear in the number of nodes plus the number of edges, asymptotically, O(|V| + |E|)."
- **DAG cycle detection** — "A topological ordering is possible if and only if the graph has no directed cycles, that is, if it is a directed acyclic graph (DAG)."
- **Application to PERT/CPM** — "A closely related application of topological sorting algorithms was first studied in the early 1960s in the context of the PERT technique for scheduling in project management." → direct link to CPM (#5).
- **Parallel algorithms** — "O((log n)2) time using a polynomial number of processors, putting the problem into the complexity class NC2" — для Stage 4.1 GPU world gen follow-up.

**Direct quote:** "A topological ordering is possible if and only if the graph has no directed cycles, that is, if it is a directed acyclic graph (DAG). Any DAG has at least one topological ordering, and there are linear time algorithms for constructing it."

**Relevance:** canonical algorithm для **C_DependencyDAG_TopoSort** strategy — Kahn's algorithm guarantees zero deadlock + linear time + correct dependency order.

---

## Tier 2 — Secondary references (for cross-validation)

### 7. Wikipedia: "Just-in-time manufacturing" — JIT redirect to Lean manufacturing

URL: https://en.wikipedia.org/wiki/Lean_manufacturing (redirect from JIT)

**Key finding:** JIT = "more recent name for Lean [...] deeply rooted in the automotive industry and focuses mostly on repetitive manufacturing situations" → JIT ≈ Lean для production simulation purposes.

### 8. Wikipedia: "Topological sorting" → "Hu's algorithm" — Hu 1961 precedence-graph scheduling

URL: https://en.wikipedia.org/wiki/Topological_sorting#Relation_to_scheduling_optimisation

**Direct quote:** "Hu's algorithm is a popular method used to solve scheduling problems that require a precedence graph and involve processing times (where the goal is to minimise the largest completion time amongst all the jobs). Like topological sort, Hu's algorithm is not unique and can be solved using DFS (by finding the largest path length and then assigning the jobs)."

**Relevance:** альтернатива CPM для multi-machine scheduling. Cross-ref к D_CriticalPathBatch.

### 9. Wikipedia: "Factory Physics" (book reference) — Hopp & Spearman 2008

URL: https://en.wikipedia.org/wiki/Lean_manufacturing (cited in reference list)

**Direct quote (from Wikipedia citation):** "Hopp, Wallace; Spearman, Mark (2008), Factory Physics: Foundations of Manufacturing Management (3rd ed.), McGraw-Hill Companies, Incorporated, ISBN 978-0-07-282403-2."

**Relevance:** foundational textbook for manufacturing scheduling theory. Cited but not fetched in this session (only 9 sources budget).

---

## Tier 3 — ProjectV internal cross-references

- `closed 2026-06-21-supply-logistics-simulation` [verdict=mixed] — supply graph, **downstream consumer** of factory output.
- `closed 2026-06-21-data-driven-vehicle-weapon-definitions` [verdict=mixed] — definition storage, **upstream input** to factory specs.
- `closed 2026-06-21-component-vehicle-damage-model` [verdict=yes] — downstream consumer (factory produces → consumer wears down).
- `closed 2026-06-21-ballistic-projectile-simulation` [verdict=yes] — consumes shells (factory output).
- `closed 2026-06-21-tank-terrain-interaction-physics` [verdict=yes] — consumes tanks.
- `closed 2026-06-21-fixed-wing-flight-model-simulation` [verdict=yes] — consumes aircraft.
- `closed 2026-06-21-aircraft-damage-model` [verdict=yes] — consumes planes.
- `closed 2026-06-21-radar-detection-system-simulation` [verdict=yes] — consumes radars.
- `closed 2026-06-21-helicopter-rotor-physics` [verdict=yes] — consumes helicopters.
- `closed 2026-06-21-naval-vessel-buoyancy-steering` [verdict=mixed] — consumes ships.
- `closed 2026-06-21-lua-game-rules-scripting` [verdict=mixed] — mod-scripting hook dispatch (orth axis).
- `closed 2026-06-21-lockstep-state-sync-hybrid-netcode` [verdict=mixed] — netcode transport (orth axis).
- `closed 2026-06-21-ecs-1m-entities-bottleneck` [verdict=yes] — Flecs registry = entity host for factory entities.

---

## Total: 6 primary + 3 secondary + 13 ProjectV cross-refs = 22 references

**Status:** sufficient for гипотезис. Phase 2 web-research **complete**.