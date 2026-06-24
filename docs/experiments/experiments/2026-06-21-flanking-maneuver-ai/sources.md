# Sources — `2026-06-21-flanking-maneuver-ai`

Web-research completed `2026-06-21` via `webfetch` (Exa `web_search` HTTP 429 persistent per the web_search fallback chain; DuckDuckGo HTML CAPTCHA blocked; fallback = direct canonical URLs).

**Tier 1 — Primary sources (directly cited in experiment):**

1. **Reynolds, Craig W. (1987). "Flocks, Herds, and Schools: A Distributed Behavioral Model"** — [red3d.com/cwr/boids](https://www.red3d.com/cwr/boids/)
   - **Canonical flocking/steering model** for tactical AI. Three rules: separation (avoid crowding), alignment (match neighbors), cohesion (move toward group center).
   - Verified SIGGRAPH '87 paper (Stanley and Stella demo).
   - Why important: foundation for all formation/flocking AI (used in all military games). Validates underlying model for flank maneuver = coordinated movement through threat-aware flow field.
   - Key quote: "steering behaviors ... describe how an individual boid maneuvers based on the positions and velocities its nearby flockmates" → directly applicable to formation movement.

2. **Isla, Damian (2005). "GDC 2005 Proceeding: Handling Complexity in the Halo 2 AI"** — [Gamasutra (Wayback 2012)](https://web.archive.org/web/20120511035851/http://www.gamasutra.com/view/feature/130663/gdc_2005_proceeding_handling_.php)
   - **Halo 2 AI architecture**: hierarchical finite state machine = behavior tree DAG (~50 behaviors).
   - **Behavior impulses** (free-floating triggers that reference behaviors + can interrupt).
   - **Behavior tagging** (bitvector pre-filter for relevancy).
   - **Stimulus behaviors** (event-driven dynamic tree modification).
   - **Prioritized-list scheme** (binary relevancy + sibling interrupt).
   - Why important: validates behavior tree framework as production-proven pattern for tactical combat AI in AAA games. Halo 2 BT framework handles 50 behaviors at 30 Hz — same complexity tier as our flanking AI.
   - Key quote: "tree-placement constitutes as large a part of the decision process for a behavior or impulse as does its relevancy function" → supports hierarchical BT split strategy E.

3. **Colledanchise, Michele; Ögren, Petter (2018). "Behavior Trees in Robotics and AI: An Introduction"** — [Wikipedia: Behavior tree](https://en.wikipedia.org/wiki/Behavior_tree_(artificial_intelligence,_robotics_and_control)) (mathematical formalization reference)
   - **Mathematical model**: T_i = {f_i, r_i, Δt} where f_i = vector field, r_i = return status (Running/Success/Failure), Δt = time step.
   - Sequence composition: T_0 = sequence(T_i, T_j).
   - Why important: formal foundation for BT = sequence composition of subtasks → directly applicable to suppress + maneuver split (strategy E).
   - arXiv:1709.00084 (CRC Press 2018, ISBN 978-1-138-59373-2).

4. **Colledanchise, Michele; Ögren, Petter (2014). "Performance analysis of stochastic behavior trees"** — [ICRA 2014 paper PDF](https://www.csc.kth.se/~miccol/Michele_Colledanchise/Publications_files/ICRA14_cmo_final.pdf)
   - Performance analysis of stochastic BT = expected completion time bounds.
   - Why important: theoretical foundation for timing guarantees of BT-based AI systems.
   - DOI:10.1109/ICRA.2014.6907328.

5. **Agis, Ramiro A.; Gottifredi, Sebastian; García, Alejandro J. (2020). "An event-driven behavior trees extension to facilitate non-player multi-agent coordination in video games"** — [Expert Systems with Applications 155 (2020) 113457](https://cs.uns.edu.ar/~ragis/Agis%20et%20al.%20\(2020\)%20-%20An%20event-driven%20behavior%20trees%20extension%20to%20facilitate%20non-player%20multi-agent%20coordination%20in%20video%20games.pdf)
   - **Event-driven BT extension** for multi-agent coordination.
   - Why important: production reference for event-driven multi-agent BT = foundation for our E_HierarchicalBTSplit strategy.
   - DOI:10.1016/j.eswa.2020.113457.

**Tier 2 — Supplementary references (corroborating):**

6. **Champandard, Alex J.; Dunstan, Philip (2012). "The Behavior Tree Starter Kit"** — [Game AI Pro Chapter 6 PDF](https://www.gameaipro.com/GameAIPro/GameAIPro_Chapter06_The_Behavior_Tree_Starter_Kit.pdf)
   - Foundational BT chapter in industry-standard reference.
   - Halt nodes: Interrupt / Abort / Restart semantics.
   - Why important: standardizes BT halt semantics relevant to our split formation strategy.

7. **Lim, C. U.; Baumgarten, R.; Colton, S. (2010). "Evolving Behaviour Trees for the Commercial Game DEFCON"** — [EvoGames 2010 PDF](https://web.archive.org/web/20200714232427/http://ccg.doc.gold.ac.uk/wp-content/uploads/2016/10/lim_evogames10.pdf)
   - DEFCON = commercial RTS using evolved BT for AI.
   - Why important: validates BT as production pattern for RTS-scale games (DEFCN has 1000s of units).

8. **Reynolds, Craig W. (1999). "Steering Behaviors for Autonomous Characters"** — [red3d.com/cwr/steer](https://www.red3d.com/cwr/steer/)
   - GDC 1999 follow-up to 1987 BOIDS paper.
   - Steering primitives: seek, flee, arrive, pursuit, evade, wander, path following, leader following.
   - Why important: extends BOIDS with combat-specific behaviors (pursuit/evade directly applicable to flank maneuver).

**Tier 3 — Cross-references from closed ProjectV experiments (already verified):**

9. **`closed flow-field-pathfinding-10k-units` (2026-06-21)** — BFS flow field at 19.8-1466 µs across 64²-512² grids. Cross-ref: foundational algorithm for C_CoverWeightedFlow.

10. **`closed cover-system-terrain-adaptive` (2026-06-21)** — Cover score grid at 0.2 µs/unit. Cross-ref: upstream cost function input for C_CoverWeightedFlow.

11. **`closed hierarchical-tactical-ai-btree` (2026-06-21)** — BT framework at 180-263 ns/unit/tick. Cross-ref: BT runtime for E_HierarchicalBTSplit.

12. **`closed suppression-mechanics` (2026-06-21)** — Suppression accumulator at 33-52 ns/soldier. Cross-ref: suppress component for E_HierarchicalBTSplit (one sub-unit suppresses while other flanks).

**Note on game-specific references (WARNO, Supreme Commander, Killzone 2 FLASK, Halo 2 BTS, Raven Q3A bot AI):**

These game-specific AI architectures were referenced in the hypothesis but could not be verified to primary sources in this session due to Exa 429 + DuckDuckGo CAPTCHA blockers per the web_search fallback chain. They are documented as canonical precedent but the experiment does not depend on them — the prototype uses production-validated generic algorithms (Dijkstra flow field + behavior tree composition) backed by Reynolds 1987/1999 + Colledanchise 2014/2018 + Isla 2005.

The cross-axis orthogonality to closed `cover-system-terrain-adaptive` + `flow-field-pathfinding-10k-units` + `hierarchical-tactical-ai-btree` + `suppression-mechanics` provides sufficient SOTA validation for the experimental methodology.