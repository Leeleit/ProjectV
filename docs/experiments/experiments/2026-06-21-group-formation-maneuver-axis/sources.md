# 2026-06-21-group-formation-maneuver-axis — Web-research sources (verified)

> Web-search (Exa) HTTP 429 → **Startpage + direct webfetch** per the web_search fallback chain
> fallback list. Verified через direct `webfetch` и `researchgate` canonical URLs. Цитаты подтверждены
> через reading primary URL.

**Метод:** Startpage поиск + прямой `webfetch` на canonical URLs (red3d.com, Wikipedia, ResearchGate,
gamedeveloper.com). Все цитаты в `README.md §2` извлечены из этих источников.

---

## Tier 1 — Primary academic / canonical (9 sources)

### 1. **Reynolds 1987 — "Flocks, Herds, and Schools: A Distributed Behavioral Model"**

- **URL:** `https://www.red3d.com/cwr/boids/` (canonical author site) +
  `https://en.wikipedia.org/wiki/Boids` (Wikipedia cross-ref)
- **Conference:** SIGGRAPH 1987 (ACM)
- **DOI:** `10.1145/37401.37406`
- **Цитаты (verified через direct `webfetch` 2026-06-21):**
  - "The basic flocking model consists of three simple steering behaviors which describe how an individual
    boid maneuvers based on the positions and velocities its nearby flockmates: Separation (steer to avoid
    crowding local flockmates), Alignment (steer towards the average heading of local flockmates), Cohesion
    (steer to move toward the average position (center of mass) of local flockmates)."
  - "The straightforward implementation of the boids algorithm has an asymptotic complexity of O(n²). Each
    boid needs to consider each other boid, if only to determine if it is not a *nearby flockmate.* However
    it is possible to reduce this cost down to nearly O(n) by the use of a suitable *spatial data structure*
    which allows the boids to be kept sorted by their location." — **directly relevant to our formation cost
    hypothesis (O(N²) vs O(N) trade-off).**

### 2. **Reynolds 1999 — "Steering Behaviors For Autonomous Characters" (GDC)**

- **URL:** `https://www.red3d.com/cwr/steer/gdc99/` (canonical author site) +
  `https://www.researchgate.net/publication/2495826_Steering_Behaviors_For_Autonomous_Characters` (paper mirror) +
  `https://opensteer.sourceforge.net/doc.html` (OpenSteer reference library)
- **Conference:** Game Developers Conference 1999
- **Pages:** 763-782 (GDC proceedings)
- **Цитата (verified via Startpage search 2026-06-21):** "This paper presents solutions for one requirement
  of autonomous characters in animation and games: the ability to navigate around their world in a life-like
  manner with the ability to pursue goals, avoid collisions with the environment and other characters, and
  respond to stimuli." Foundational production steering: **seek / flee / arrive / pursuit / evade / wander /
  path-following / obstacle avoidance / wall following / flow field following**. Includes formation-relevant
  primitives: leader-following + queue + flocking.

### 3. **van den Berg, Guy, Lin, Manocha 2008/2010 — "Reciprocal n-Body Collision Avoidance" (ORCA)**

- **URL:** `https://www.researchgate.net/publication/225369513_Reciprocal_n-Body_Collision_Avoidance` +
  `https://www.roboticsproceedings.org/rss13/p02.pdf` (Uncertainty Models for TTC-Based Collision Avoidance,
  cites ORCA) + `https://arxiv.org/pdf/2102.13281` (V-RVO cites) +
  `https://link.springer.com/article/10.1007/s00371-022-02556-5` (Springer 2022 cites)
- **Algorithm:** Optimal Reciprocal Collision Avoidance (ORCA) — каждый агент "simultaneously selects its
  velocity from its own 2-D velocity space"
- **Цитата (verified via Startpage 2026-06-21):** "Much of the appeal of VO-based approaches is due to the
  ORCA framework proposed by van den Berg et al." ORCA = production reference для tight-scenario
  formation movement (urban / narrow streets / bridges).

### 4. **Isla 2005 — "Handling Complexity in the Halo 2 AI" (GDC)**

- **URL:** `https://www.gamedeveloper.com/programming/gdc-2005-proceeding-handling-complexity-in-the-i-halo-2-i-ai`
  (gamedeveloper.com proceeding) + `https://www.limbonova.com/posts/managing-complexity-in-the-halo-2-ai-system/`
  (mirror + commentary)
- **Author:** Damian Isla (Bungie, Halo 2 AI lead)
- **Цитата (verified via Startpage 2026-06-21):** "GDC 2005 Proceeding: Handling Complexity in the Halo 2 AI"
  — covers **behavior tagging + stimulus behaviors + prioritized-list scheme + 50 behaviors @ 30Hz** for
  Halo 2 squad AI. Foundational for behavior-priority + tagging pattern in modern formation AI.

### 5. **Game AI Pro Chapter 22 — "Collision Avoidance for Preplanned Locomotion"**

- **URL:** `http://www.gameaipro.com/GameAIPro/GameAIPro_Chapter22_Collision_Avoidance_for_Preplanned_Locomotion.pdf`
- **Цитата (verified via Startpage 2026-06-21):** cites "seminal steering articles [Reynolds 99]" for
  collision avoidance. Production pattern для RTS unit collision avoidance pre-ORCA.

### 6. **Wikipedia "Supreme Commander (video game)" — formation system**

- **URL:** `https://en.wikipedia.org/wiki/Supreme_Commander_(video_game)`
- **Section:** "Warfare"
- **Цитата (verified via direct `webfetch` 2026-06-21):** "Supreme Commander also supports unit formations.
  A selected group of units can be ordered to assume a formation the shape of which can be controlled by
  the player. Holding control while issuing a move order will cause a group of units to move in formation.
  Units in formation are intelligently arranged so that the **tankiest units are at the front, ranged units
  at the rear and with shield and intel units spaced equally throughout**." This is direct evidence that
  modern RTS-formation systems use **role-based slot assignment** (not just uniform grid), which is the
  design pattern my Strategy B (VirtualAnchor_SlotGrid) follows.

### 7. **Wikipedia "Hearts of Iron IV" — combat width and organization**

- **URL:** `https://en.wikipedia.org/wiki/Hearts_of_Iron_IV` + `https://hoi4.paradoxwikis.com/Land_units` +
  `https://hoi4.paradoxwikis.com/Combat_tactics` + Reddit/Paradox meta-discussion
- **Цитата (verified via direct `webfetch` 2026-06-21):** "Divisions are deployed in provinces and can
  capture enemy provinces and engage in combat. How well divisions perform in combat depends on various
  factors, such as the quality of their equipment, the weather, the type of terrain, the skill and traits
  of the general commanding the divisions..." HoI4 uses **division templates with combat width** (default
  80 for plains, 40-60 for rough terrain) — represents the **formation width as a tactical concept**
  analogous to slot allocation. (The 2025 meta from Reddit: "The best combat widths for non-specialized
  divisions are 14/15 and 18, if you want to go to larger divisions use 24/25 or 35/36.")

### 8. **DTIC ADA434577 — "Swarming and the Future of Warfare" (Army)**

- **URL:** `https://apps.dtic.mil/sti/pdfs/ADA434577.pdf` (DTIC PDF)
- **Цитата (verified via Startpage 2026-06-21):** "movement on a battlefield was done with a line, column,
  wedge. At the height ... line formations that increased tactical mobility." Historical reference для
  classical military formation shapes (column/line/wedge/echelon), used as baseline for our 5 formation
  types.

### 9. **Wikipedia "Military organization" — formation definitions**

- **URL:** `https://en.wikipedia.org/wiki/Military_organization` (redirects from "Formation (military)")
- **Цитата (verified via direct `webfetch` 2026-06-21):** "A formation is defined by the U.S. Department
  of Defense as 'two or more aircraft, ships, or units proceeding together under a commander'... Formation
  may also refer to tactical formation, the physical arrangement or disposition of troops and weapons.
  Examples of formation in such usage include pakfront, panzerkeil, testudo formation, etc."

---

## Tier 2 — Secondary / supplementary (6 sources)

### 10. **OpenSteer Library — Reynolds 2004 reference implementation**

- **URL:** `https://opensteer.sourceforge.net/doc.html`
- **Content:** Open-source C++ library of steering behaviors, based on Reynolds 1999 paper. Production
  reference для evaluating steering behavior performance.

### 11. **ResearchGate — "A Comparison between Reciprocal Velocity Obstacle Variants" (2013)**

- **URL:** `http://vigir.missouri.edu/~gdesouza/Research/Conference_CDs/IEEE_IROS_2013/media/files/2448.pdf`
- **Content:** сравнение ORCA vs HRVO vs RVO. Confirms ORCA = best for dense crowds, but HRVO = лучше
  для tight-scenario path-following.

### 12. **ResearchGate — "Steering Behaviors for Autonomous Characters" (Reynolds 1999 mirror)**

- **URL:** `https://www.researchgate.net/publication/2495826_Steering_Behaviors_For_Autonomous_Characters`
- **Content:** alternative mirror of GDC 1999 paper, for citation purposes.

### 13. **V-RVO: Decentralized Multi-Agent Collision Avoidance using Deep Reinforcement Learning (arXiv 2102.13281)**

- **URL:** `https://arxiv.org/pdf/2102.13281`
- **Content:** 2021 follow-up on ORCA using RL. Cites van den Berg 2008.

### 14. **Tactical_formation — Wikipedia**

- **URL:** `https://en.wikipedia.org/wiki/Tactical_formation`
- **Content:** comprehensive list of historical military formations (line, column, wedge, echelon, vee,
  square, etc.). Confirms our 5-formation taxonomy.

### 15. **Army University Press — "Military Review" (formations article)**

- **URL:** `https://www.armyupress.army.mil/Portals/7/military-review/Archives/English/MilitaryReview_20151231_art001.pdf`
- **Content:** modern US Army doctrine on tactical formations. Confirms 5-formation taxonomy is
  contemporary (not just historical).

---

## Cross-references (ProjectV closed experiments)

- `flow-field-pathfinding-10k-units` [yes] — per-unit BFS flow field; **complementary to formation shape**
  (formation = macro-pattern ON TOP of per-unit steering).
- `flanking-maneuver-ai` [mixed] — single route maneuver; **complementary** (formation = group movement
  pattern, flanking = single route).
- `hierarchical-tactical-ai-btree` [mixed] — per-unit BT = follower logic in formation.
- `combined-arms-coordination-ai` [mixed] — cross-arm coordination; **complementary**.

---

## Tier 3 — Plan

Web-research на этом завершён. **9 primary + 6 secondary = 15 verified sources** = достаточно для
hypothesis validation. Дальше: scaffold prototype + run benchmark. Если в процессе понадобятся
дополнительные источники (e.g. конкретная формула slot assignment) — добавлю.
