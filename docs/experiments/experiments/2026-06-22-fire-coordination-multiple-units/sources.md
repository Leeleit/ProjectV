# Sources — Multi-Unit Fire Coordination & Target Priority

Web-research проведён 2026-06-22. **Exa HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked** per `agent/knowledge.md Part B §9` line 1424 fallback list. Работали direct `webfetch` к canonical URLs (Wikipedia). 7 Tier 1+2 sources verified.

---

## Tier 1 — Canonical academic + production references

### 1. Wikipedia "Behavior selection algorithm" — https://en.wikipedia.org/wiki/Behavior_selection_algorithm
- **Canonical overview of action-selection architectures в game AI**: FSM, HFSM, decision trees, behavior trees, HTN, hierarchical control systems, utility systems, dialogue tree.
- **GameAIPro Chapter 4 reference** (Champandard + Dunstan, "Behavior Selection Algorithms: An Overview") cited.
- **Relevance:** target selection via utility scoring = one of the canonical behavior selection paradigms; my B_PriorityScoreWeighted = utility AI application.
- **Date accessed:** 2026-06-22.

### 2. Wikipedia "Utility system" — https://en.wikipedia.org/wiki/Utility_system
- **Direct evidence:** "Using numbers, formulas, and scores to rate the relative benefit of possible actions, one can assign utilities to each action. A behavior can then be selected based on which one scores the highest 'utility'."
- **Production precedent:** The Sims (2000, foundational), The Sims 3 (2009, Richard Evans + Boltzmann distribution), ArenaNet IAUS (Dave Mark + Mike Lewis, GDC 2015).
- **Direct reference to focus fire / target priority:** Bill Merrill GameAIPro Ch.10 "Building Utility Decisions into Your Existing Behavior Tree" (gameaipro.com PDF) — direct precedent for utility-based engagement scoring.
- **Relevance:** B_PriorityScoreWeighted = utility-based target priority; production-validated.
- **Date accessed:** 2026-06-22.

### 3. Wikipedia "Hierarchical task network" — https://en.wikipedia.org/wiki/Hierarchical_task_network
- **HTN planning** as canonical approach for RTS AI.
- **Production systems:** SHOP2 (University of Maryland), NOAH, Nonlin, O-Plan, UMCP, PANDA, HTNPlan-P.
- **Relevance:** HTN upstream of fire-coordination — engagement choice как composite task внутри squad HTN per closed `2026-06-21-combined-arms-coordination-ai` [mixed] (C_Hierarchical_2Tier). My E_AdaptiveDoctrine = HTN-like adaptive mode switcher.
- **Date accessed:** 2026-06-22.

### 4. Wikipedia "Supreme Commander" — https://en.wikipedia.org/wiki/Supreme_Commander_(video_game)
- **Multi-core AI dispatch** ("When detecting a multi-core processor, the game assigns a specific task, such as AI calculations, to each core") — direct precedent for parallel engagement selection.
- **Hard AI variants** (Horde / Tech / Balanced / Supreme) — doctrine-aware selection precedent for E_AdaptiveDoctrine.
- **Formations** ("tankiest units at the front, ranged units at the rear") — relevant for engagement range / role-based scoring, parallel to my B_PriorityScoreWeighted (role + proximity).
- **Strategic zoom + 80km maps + 1000 units per player** — scale reference for engagement-assignment overhead budget.
- **Date accessed:** 2026-06-22.

### 5. Wikipedia "Wargame: European Escalation" — https://en.wikipedia.org/wiki/Wargame:_European_Escalation
- **Eugen Systems** developer (RTS Cold War real-time tactics, IrisZoom engine).
- **Military doctrine per country / faction** ("Each country has its own arsenal of units, reflecting their military doctrine") — direct precedent for doctrine-aware target priority в моей E_AdaptiveDoctrine.
- **361 historical units** recreated — engagement variety → priority scoring importance.
- **Relevance:** military sandbox canon reference for cold-war RTS target priority + doctrine selection.
- **Date accessed:** 2026-06-22.

### 6. Wikipedia "WARNO" — https://en.wikipedia.org/wiki/Warno_(video_game)
- **2024 spiritual successor to Wargame** by Eugen Systems.
- **10v10 multiplayer scale** — engagement coordination must work at scale.
- **"Some basic unit functions can be assigned to the AI, but a majority of actions are done by the player"** — current production has manual target priority, автоматизация — opportunity.
- **Battlegroups with specializations** (infantry / combined arms) — doctrine concept для E_AdaptiveDoctrine.
- **Relevance:** active RTS market reference для engagement-assignment architecture.
- **Date accessed:** 2026-06-22.

### 7. Wikipedia "Artificial intelligence in video games" — https://en.wikipedia.org/wiki/Artificial_intelligence_in_video_games
- **Combat AI overview** including hunting, survival instinct, monster infighting (id Software Doom 1993 precedent).
- **"AI often consists of a half-dozen rules of thumb, or heuristics, that are just enough to give a good gameplay experience"** — confirms utility/heuristic scoring approach в production game AI.
- **Relevance:** validation that engagement-assignment via heuristic scoring is canonical pattern.
- **Date accessed:** 2026-06-22.

---

## Tier 2 — Supplementary

### 8. Wikipedia "Target selection" — https://en.wikipedia.org/wiki/Target_selection
- **Neurology context** (axon target selection, chemoaffinity hypothesis Roger Sperry 1960s) — not directly applicable to game AI, but relevant to "target selection" как cross-domain concept.
- **Date accessed:** 2026-06-22 (less directly relevant, included for terminology cross-check).

---

## Web-research limitations

- **Exa HTTP 429 persistent** per `agent/knowledge.md Part B §9` fallback list (no work this session).
- **DuckDuckGo HTML endpoint CAPTCHA blocked** (CAPTCHA challenge received 2026-06-22).
- **Direct webfetch к Wikipedia** = working fallback per `agent/knowledge.md Part B §9` line 1424 (searx / duckduckgo / brave / bing / google / startpage + direct URLs).
- **arXiv, GDC Vault, GameAIPro PDFs not retrieved** this session (would require additional fallback chain).
- **Total sources verified:** 7 Tier 1 + 1 Tier 2 = 8 unique canonical URLs.

## Cross-refs

- `backlog.md §In progress` — reservation record.
- [`hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X dev host, актуально на 2026-06-21).
- `agent/knowledge.md §30.4` — 3-step migration precedent.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- Closed experiments per §1:
  - `2026-06-21-combined-arms-coordination-ai` [mixed, C_Hierarchical_2Tier] = upstream
  - `2026-06-21-suppression-mechanics` [mixed] = D_SuppressionFocus consumer
  - `2026-06-21-hierarchical-tactical-ai-btree` [mixed] = EngagementDecision action node
  - `2026-06-21-cover-system-terrain-adaptive` [mixed] = cover score input
  - `2026-06-21-recon-intel-fog-of-war` [yes] = intel visibility gates selection
  - `2026-06-21-radars-detection-system-simulation` [yes] = radar-locked bonus input
  - `2026-06-21-flanking-maneuver-ai` [closed] = post-arrival target selection
  - `2026-06-21-group-formation-maneuver-axis` [closed mixed] = post-positioning engagement
  - `2026-06-21-lockstep-state-sync-hybrid-netcode` [mixed] = determinism requirement