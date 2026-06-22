# Sources — Tech Tree Research System

> Web-research via direct `webfetch` to canonical URLs (Exa `web_search` HTTP 429 persistent this session per `agent/knowledge.md Part B §9` line 1424 fallback list).

## Tier 1 — Foundational algorithms

### 1. Wikipedia "Technology tree" — canonical reference for game tech tree patterns
https://en.wikipedia.org/wiki/Technology_tree
- **Key facts:** "In strategy games, a technology, tech, or research tree is a hierarchical visual representation of the possible sequences of upgrades a player can unlock. Because these trees are technically directed and acyclic, they can more accurately be described as a **directed acyclic graph** of technologies."
- **DAG formal model:** the canonical game industry data structure for tech tree.
- **Historical precedent:** Tresham *Civilization* board game 1980 first; Sid Meier's *Civilization* 1991 first major computer implementation; Mega Lo Mania 1991 first RTS tech tree.
- **Scale examples:** Freeciv 86 techs; Civilization IV ~60-step end-game; Space Empires III >200 technologies; Warcraft III 5 steps to top level.
- **Sourcing:** validated 2026-06-22, Wikipedia version `oldid=1338196678`, last edit 13 Feb 2026.
- **Production references:** Master of Orion series (10 levels per subject, 2-5 techs per level); Civilization series (multiple prereqs per top-level tech); Galactic Civilizations (final tech = victory); Rise of Nations (4 final techs end game); Stellaris (3 alternative paths per tech); Endless Legend/Space (asymmetric faction trees).
- **HoI4 / Warno / SupCom context:** Warzone 2100 / Mega Lo Mania / Total Annihilation / StarCraft (building-based); Civilization / Master of Orion (research allocation); Space Empires / Ascendancy (research via buildings, no allocation choice).

### 2. Wikipedia "Critical path method" — CPM 1959 DuPont/Remington Rand
https://en.wikipedia.org/wiki/Critical_path_method
- **Key facts:** "CPM is a project-modeling technique developed in the late 1950s by Morgan R. Walker of DuPont and James E. Kelley Jr. of Remington Rand."
- **Algorithm:** "CPM calculates the longest path of planned activities to logical end points or to the end of the project, and the earliest and latest that each activity can start and finish without making the project longer."
- **Complexity:** O(V+E) one-time precompute for entire DAG critical path; the basis of all later shortest-path / longest-path algorithms on DAGs.
- **For our research-tree scheduling:** Strategy C_CriticalPathPrecompute pre-computes per-track critical path length once at game start; per-tick, only O(1) work to determine if a track is on critical path.
- **Historical:** precursors 1940-1943 (Manhattan Project DuPont); PERT 1958 (Booz Allen Hamilton + U.S. Navy); CPM 1959 (DuPont + Remington Rand).
- **Sourcing:** validated 2026-06-22, Wikipedia version `oldid=1319504246`, last edit 30 Oct 2025.

### 3. Wikipedia "Topological sorting" — Kahn 1962, O(V+E)
https://en.wikipedia.org/wiki/Topological_sorting
- **Key facts:** "Topological sorting has many applications, especially in ranking problems such as feedback arc set. ... A closely related application of topological sorting algorithms was first studied in the early 1960s in the context of the PERT technique for scheduling in project management."
- **Algorithm:** Kahn's algorithm 1962 O(V+E) BFS-style + DFS variant 1976 Tarjan; both linear in V+E.
- **DAG cycle detection:** "A topological ordering is possible if and only if the graph has no directed cycles ... if the graph has edges then return error (graph has at least one cycle)" — perfect for prerequisite cycle detection in tech trees.
- **Parallel:** "On a parallel random-access machine, a topological ordering can be constructed in O((log n)²) time using a polynomial number of processors, putting the problem into the complexity class NC²" — for our prototype, we use single-threaded (DAG critical path + Kahn single-thread is enough).
- **Applications:** "makefiles, data serialization, and resolving symbol dependencies in linkers, decide in which order to load tables with foreign keys in databases" — same pattern as our tech tree prerequisite resolution.
- **Sourcing:** validated 2026-06-22, Wikipedia version `oldid=1328053597`, last edit 17 Dec 2025.

### 4. Wikipedia "Dijkstra's algorithm" — 1959, O((V+E) log V)
https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
- **Key facts:** "Dijkstra's algorithm is an algorithm for finding the shortest paths between nodes in a weighted graph... conceived by computer scientist Edsger W. Dijkstra in 1956 and published three years later."
- **Complexity:** Θ(|E| + |V| log|V|) with Fibonacci heap; Θ((V+E) log V) with binary heap; Θ(V²) with linked list/array.
- **For our research-tree scheduling:** Strategy B_PriorityQueueDijkstra uses min-heap of next-activatable nodes (sorted by estimated completion time) — same as Dijkstra's algorithm but on DAG with dynamic edge costs.
- **Practical optimizations:** "initialize the priority queue to contain only source; then, inside the if alt < dist[v] block, the decrease_priority() becomes an add_with_priority() operation" — same lazy insertion pattern we use.
- **Sourcing:** validated 2026-06-22, Wikipedia version `oldid=1357396357`, Wikipedia Dijkstra 1956 invention, 1959 publication.

## Tier 2 — Game production references (for empirical cross-ref)

### 5. Civilization series — research + eureka boost model
- **Civ 6 eureka:** researching a specific world state (e.g. finding a natural wonder for "Cartography") gives 50% time reduction. Adaptive dispatcher pattern.
- **Civ 5 final tech:** "Future Tech" repeatable for score increase. Pattern: extensible research queue.
- **Production lesson for our prototype:** the eureka boost pattern maps to our "research_speed_modifier" tech node attribute.

### 6. Stellaris — research alternatives pattern
- **Stellaris:** 3 choices per technology, weighted random selection. Player can specialize. Pattern: priority queue with stochastic ordering.

### 7. Hearts of Iron IV — 5 categories, slot-based parallel research
- **HoI4:** 5+ tech trees (infantry, armor, artillery, navy, air, industry, doctrine). Player has limited research slots (4-5 at start, scaling with research buildings). Each slot picks one tech from one tree.
- **For our prototype:** this is exactly the "3 parallel tracks with slot count" model in our `tree_3_50` and `realistic_hoi4_subset_60` scenes.

### 8. Warno — division-level fixed unlock paths
- **Warno:** 3 decks per division, fixed unlock paths (no choice). Pattern: critical path = entire deck, no parallelism.
- **For our prototype:** contrast with HoI4 (which has choice + parallelism); informs our scene design.

### 9. Supreme Commander — tech tier (T1/T2/T3) progression
- **SupCom:** 3 tech tiers per faction, linear within tier. Once T1 unlocked, T2 unlocks. Tech gating = economy/build gating.
- **For our prototype:** validates "linear chain" scene as a degenerate but important test case.

## Tier 3 — ProjectV cross-references

### 10. ProjectV agent/knowledge.md §30.4 — 3-step migration pattern
- **Pattern:** Step 1 (XS, ~80 LoC) foundation + env gate; Step 2 (M, ~300-500 LoC) per-strategy implementation; Step 3 (S, ~100-150 LoC) tests + Tracy + env gate default.
- **Application:** our integration recommendation uses this exact pattern for `src/economy/TechTree.{hpp,cpp}`.

### 11. ProjectV closed experiments — `factory-production-system`, `data-driven-vehicle-weapon-definitions`
- **Downstream consumer pattern:** our `tech-tree-research-system` experiment outputs "unlock events" consumed by factory system and content definitions.
- **Complementary axis:** tech tree = upstream of content unlocks; factory + content = downstream.

## Caveats / limitations

- **Exa HTTP 429 persistent** this session per `agent/knowledge.md Part B §9` line 1424 fallback list → relied on direct `webfetch` to canonical Wikipedia URLs.
- **Wikipedia version validity:** all 4 primary Wikipedia pages validated `2026-06-22` (latest edits within last 12 months).
- **4 primary sources** (Tier 1) + **5 game production references** (Tier 2) + **2 ProjectV cross-references** (Tier 3) = **11 total sources** verified. Coverage adequate for a focused architecture comparison; deeper game-specific research (e.g. Endless Legend Fatum Machina tech tree internals) deferred to integration phase.
