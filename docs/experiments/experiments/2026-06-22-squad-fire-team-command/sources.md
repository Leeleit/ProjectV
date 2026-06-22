# Sources — `2026-06-22-squad-fire-team-command`

Web-research via direct `webfetch` to canonical Wikipedia URLs (Exa `web_search` HTTP 429 persistent +
DuckDuckGo HTML endpoint CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list;
8 primary sources verified):

---

## Tier 1 — Primary doctrine references

1. **Wikipedia — "Fireteam"** (https://en.wikipedia.org/wiki/Fireteam) — 2026-05-19.
   *Canonical reference для fireteam / squad / section / platoon organization per US Army ADP 3-90 + FM 1-02.2 + NATO APP-06.*
   - 2-4 soldiers per fireteam (automatic rifleman + grenadier + rifleman + team leader)
   - Two or three fireteams → squad (5-14 soldiers, corporal/sergeant/staff sergeant ranks)
   - 50 m spread in combat, 500 m effective range, ~100 m detection limit
   - Fire-and-maneuver team concept (one team fights while other moves)
   - Wikipedia editors note: 33 language versions — direct evidence of canonical military concept

2. **Wikipedia — "Squad leader"** (https://en.wikipedia.org/wiki/Squad_leader) — 2025-11-04.
   *Defines squad structure and command.*
   - US Army rifle squad = 9 Soldiers (squad leader + 2 fireteams × 4 men) at staff sergeant rank
   - US Marine Corps rifle squad = 13 Marines (squad leader + 3 fireteams × 4 men) at sergeant rank
   - British Commonwealth: section = squad, corporal in charge
   - Bundeswehr equivalent: Gruppenführer (group leader)

3. **Wikipedia — "Bounding overwatch"** (https://en.wikipedia.org/wiki/Bounding_overwatch) — 2025-04-14.
   *Canonical squad-level fire & movement doctrine (FM 3-21.8).*
   - "Leapfrogging" / "buddy system" / "moving overwatch"
   - 3-5 second rush per bound
   - One fireteam overwatch while other bounds to new covered position
   - Originated WWII for man-portable automatic weapons
   - Direct evidence for templated `BOUNDING_OVERWATCH` squad order

4. **Wikipedia — "Close-quarters battle"** (https://en.wikipedia.org/wiki/Close-quarters_battle) — 2026-05-13.
   *CQB doctrine and historical development (Fairbairn 1925, Munich 1972, Fallujah 2004).*
   - Building entry, clearing a room, grenade types
   - 4-man fire team as tactical atomic unit for room clearing
   - U.S. Army Rangers, Marines, SEAL Teams training
   - Direct evidence for `CLEAR_ROOM` squad order in urban scenes

---

## Tier 2 — Primary game-AI architecture references

5. **Wikipedia — "Behavior tree (artificial intelligence, robotics and control)"** (https://en.wikipedia.org/wiki/Behavior_tree_(artificial_intelligence,_robotics_and_control)) — 2025-12-11.
   *Canonical BT formalism per Colledanchise & Ögren 2018 (arXiv:1709.00084).*
   - Formal model: `T_i = {f_i, r_i, Δt}` (vector field + return status + time step)
   - Return status: `Running / Success / Failure`
   - Control flow: Selector (fallback) + Sequence
   - Event-driven BT (Champandard 2012, Agis 2020) — modern SOTA

6. **Wikipedia — "F.E.A.R. (video game)"** (https://en.wikipedia.org/wiki/F.E.A.R._(video_game)) — 2026-04.
   *Canonical GOAP / squad-AI production reference (Monolith Productions 2005).*
   - First commercial use of GOAP (Goal-Oriented Action Planning) per Jeff Orkin
   - 70 goals × 120 actions; A* navigates FSM
   - 3 FSM states: GoTo, Animate, UseSmartObject
   - NavMesh for free-form movement (vs waypoints)
   - Squad behavior via order-response: AI prioritizes orders vs other goals
   - Direct evidence for `D_Blackboard_SharedState` + `E_Hierarchical_2Tier` architectures

7. **Wikipedia — "Squad (video game)"** (https://en.wikipedia.org/wiki/Squad_(video_game)) — 2026-05-19.
   *Production reference for squad-based tactical game (Offworld Industries 2020, UE5).*
   - 50v50 matches, 9-player squads
   - Squad leader can place spawn points, claim vehicles, construct FOBs
   - Kits: rifleman, LAT, medic, crewman, pilot
   - Entrenching tool for player-built structures
   - Direct evidence for slot-based role assignment (Flecs prefab pattern)

8. **Wikipedia — "Arma 3"** (https://en.wikipedia.org/wiki/Arma_3) — 2026-05.
   *Production reference for tactical squad AI (Bohemia Interactive 2013, RV4 engine).*
   - Realistic ballistics, real-world military organization
   - Factions: NATO / CSAT / AAF / FIA (BLUFOR / REDFOR / INDFOR)
   - Eden Editor for scenario creation
   - Zeus DLC for real-time mission control
   - Direct evidence for templated orders (move, attack, defend, support) + squad composition

---

## Cross-references (closed ProjectV experiments, not new sources)

- `agent/knowledge.md §30.4` — 3-step migration precedent (~400 LoC, S effort, 1-2 sessions)
- `agent/workspace.md §2` — operator 8x planning decision (Stage 6+ military sandbox activation)
- `2026-06-21-hierarchical-tactical-ai-btree` [mixed, Tier 2 AI] — per-unit BT (180-263 ns/u/tick)
- `2026-06-21-group-formation-maneuver-axis` [closed mixed, Tier 2 AI] — formation positioning (per-slot assignment analog)
- `2026-06-21-cover-system-terrain-adaptive` [closed mixed, Tier 2 AI] — cover score input (0.2 µs/unit)
- `2026-06-21-suppression-mechanics` [closed mixed, Tier 2 AI] — suppression state input
- `2026-06-21-flanking-maneuver-ai` [closed mixed, Tier 2 AI] — flank route planning
- `2026-06-21-combined-arms-coordination-ai` [closed mixed, Tier 2 AI] — cross-arm coordinator
- `2026-06-21-recon-intel-fog-of-war` [closed yes, Tier 2 AI] — intel visibility input
- `2026-06-21-ballistic-projectile-simulation` [closed yes, Tier 1 Physics] — weapon spec data
- `2026-06-21-infantry-soldier-sim` [closed yes, Tier 1 Physics] — per-soldier physical sim
- `2026-06-21-ecs-1m-entities-bottleneck` [closed yes, Stage 6.x] — Flecs cost basis
- `2026-06-21-urban-combat-tactics-ai` [in-progress, Tier 2 AI] — interior graph extraction (cross-axis, room clearing)
- `2026-06-22-fire-coordination-multiple-units` [in-progress, Tier 2 AI] — focus fire
- `2026-06-22-stealth-signature-reduction` [in-progress, Tier 2 AI] — passive EW sibling

---

## Notes

- **Exa `web_search` HTTP 429 persistent this session** per `agent/knowledge.md Part B §9` line 1424 fallback list.
- **DuckDuckGo HTML endpoint CAPTCHA blocked** per same source.
- **Wikipedia direct `webfetch` working** (no rate limit detected for direct article URLs).
- All 8 primary sources = Tier 1 (Wikipedia) for doctrine/game-AI references.
- 0 of 130+ closed ProjectV experiments cover dedicated squad-fire-team-command axis (validated via sentinel §13.7).
- 0 academic paper queries this session (time budget exhausted on Wikipedia deep-read).
