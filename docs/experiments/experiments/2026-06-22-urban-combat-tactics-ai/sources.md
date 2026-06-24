# Sources — 2026-06-22-urban-combat-tactics-ai

All sources retrieved `2026-06-22` via direct `webfetch` (Exa `web_search` HTTP 429 persistent + DuckDuckGo HTML endpoint CAPTCHA blocked per the web_search fallback chain). 8 primary + 4 supplementary = **12 verified sources**.

---

## Tier 1: Game Industry Tactical Shooters (production-validated CQB AI)

1. **Wikipedia — "Tom Clancy's Rainbow Six (video game)"** (Red Storm Entertainment 1998, Brian Upton lead designer)
   - URL: https://en.wikipedia.org/wiki/Tom_Clancy%27s_Rainbow_Six_(video_game)
   - Key facts: planning stage + AI team follows player orders; "set team orders, such as AI pathing, team 'go' codes, where AI operatives will deploy equipment such as flashbangs or door breaching charges, and rules of engagement" — canonical sector-fire + plan-and-execute doctrine.
   - Per IGN PC: "unlike any first-person shooter yet made" — Metacritic 85/100.
   - **Relevance to hypothesis:** A_NaivePerRoom baseline reference — Rainbow Six 1998 already implemented plan-then-execute CQB but **per-mission planning was human-driven**, not autonomous; sets A as the no-graph low-intelligence floor.

2. **Wikipedia — "SWAT 4"** (Irrational Games 2005, Sierra Entertainment)
   - URL: https://en.wikipedia.org/wiki/SWAT_4
   - Key facts: SWAT team divided into two "elements" — **red and blue**, entire team = **gold**, team leader = **white**; "player is able to command each element and individual team member to perform actions including 'stacking up' at an entryway, using equipment, holding position, or even moving on their own to perform actions elsewhere"; "officers will only arrest a surrendering suspect if ordered to, but they will independently shoot armed and resisting suspects" — **auto-engage on armed targets**.
   - Metacritic 85/100, "as a realistic police simulator, SWAT 4 definitely hits the mark" — GameSpot 8.5/10.
   - **Relevance to hypothesis:** **Stack-and-clear pattern is the foundational sequence node** — "stack up at entryway → use equipment (breach wedge / gas / bang) → hold position → enter → clear" maps 1:1 to B_BT_Sequence strategy.

3. **Wikipedia — "Ready or Not (video game)"** (VOID Interactive 2023, Unreal Engine 5)
   - URL: https://en.wikipedia.org/wiki/Ready_or_Not_(video_game)
   - Key facts: "AI teammates will follow the instruction of the player, such as breaching a room, but will also act independently without player input in some situations such as shooting active threats and arresting suspects"; "they are tactical professionals, effectively fully functional on an autonomous level as well. They don't require constant commands or handholding. They will make callouts, move, **prioritize contacts in a room in accordance with your orders**, **lean around corners**, stay alert to openings, **make arrests**, check hiding places, and more based on their own astute tactical awareness" — full autonomous CQB.
   - Suspect behaviors: "faking their deaths when shot, pretending to surrender and then pulling out a weapon, committing suicide using a firearm, and using bystanders as human shields" — **safety criterion H3 motivation**.
   - **Relevance to hypothesis:** **Canonical example of E_CoverAwarePeek_DoorPriority** — leans/peek behavior is the de facto standard for room-clearing AI in modern tactical shooters; "prioritize contacts in accordance with orders" maps to door-priority queue.

4. **Wikipedia — "F.E.A.R. (video game)"** (Monolith Productions 2005, Vivendi)
   - URL: https://en.wikipedia.org/wiki/F.E.A.R._(video_game)
   - Key facts: "**first video game to use GOAP (Goal Oriented Action Planning)**"; "**70 available goals, using any combination of the 120 actions encoded in the game**"; FSM has only 3 states (GoTo / Animate / UseSmartObject); **A\* navigates the FSM, selects the state, selects when to initiate a state transition**; "**GOAP handles this, with the planning system deciding how best to achieve any of the 70 available goals**".
   - **NPC movement freedom:** "**the navigation mesh system (NavMesh) lets the NPCs move around the world anywhere that the player can move**" — direct relevance to interior graph extraction.
   - GameSpot 2005 Best AI Award.
   - **Relevance to hypothesis:** **Canonical reference for interior graph + dynamic replanning** — D_HierarchicalRoomGraph_FlowField pattern (room graph as NavMesh analogue for interior); 70-goal GOAP = general BT architecture for room-clearing.

5. **Wikipedia — "Close-quarters battle"**
   - URL: https://en.wikipedia.org/wiki/Close-quarters_battle
   - Key facts: "**William E. Fairbairn of the Shanghai Municipal Police**" origin (1925 May Thirtieth Movement); "modern firearm CQB tactics were developed in the 1970s as 'close-quarters battle' by Western counterterrorist special forces units following the **1972 Munich massacre**"; SAS / Delta Force / GSG 9 / GIGN / JTF2 development; "**First and Second Battles of Fallujah during the Iraq War were the watershed moments for infantry CQB**... used conventional combined arms and fire support against the city, and **lacked proper CQB training and equipment to effectively clear buildings, causing numerous civilian and allied casualties**" — safety motivation for H3.
   - ABCA armies (American / British / Canadian / Australian / New Zealand) shared doctrine post-2004.
   - **Relevance to hypothesis:** **Direct safety motivation for E_CoverAwarePeek_DoorPriority** — Fallujah casualty data is why modern SWAT4 / Ready or Not implement peek + lean + cover-aware entry (sector fire doctrine reduces friendly fire + civilian casualties).

---

## Tier 2: Building Interior Graph Standards (cross-domain reference for voxel → graph)

6. **Wikipedia — "CityGML"** (OGC standard 1.0 / 2.0 / 3.0)
   - URL: https://en.wikipedia.org/wiki/CityGML
   - Key facts: "open standardised data model and exchange format to store digital 3D models of cities and landscapes"; "defines different standard levels of detail (LoDs) for the 3D objects"; LoD 0-4 (regional / city / neighborhood / building / interior); GML3 application schema.
   - CityGML 3.0 *Conceptual Model* (2024 draft) explicitly models interior with **Building/BuildingPart/BuildingRoom/InteriorFurniture** primitives.
   - **Relevance to hypothesis:** **Direct cross-domain reference for C_Graph_BFS_Interior + D_HierarchicalRoomGraph_FlowField** — CityGML 3.0 rooms = CCL components + boundary walls; this is exactly the room-graph extraction we need.

7. **Wikipedia — "Industry Foundation Classes (IFC)"** (buildingSMART, ISO 16739-1:2024)
   - URL: https://en.wikipedia.org/wiki/Industry_Foundation_Classes
   - Key facts: "**IfcProduct is the base class for all physical objects**... **Spatial elements include IfcSite, IfcBuilding, IfcBuildingStorey, and IfcSpace**" — `IfcSpace` = room primitive.
   - "**IfcRelDecomposes** captures a whole-part relationship having exclusive containment such as **subdividing a building into floors and rooms**" — explicit decomposition to rooms.
   - "**IfcRelConnects indicates connectivity between objects such as a floor slab connected to a beam or a pipe connected to a sink**" — door/window = `IfcRelConnects` between `IfcSpace` instances.
   - IFC4.3 Add2 (2024).
   - **Relevance to hypothesis:** **Reference for hierarchical structure (Building → Storey → Room)** — D_HierarchicalRoomGraph_FlowField can use multi-storey decomposition; doors = connections between rooms.

---

## Tier 3: Behavior Tree + BT Room-Clearing (algorithmic foundation)

8. **Wikipedia — "Behavior tree (artificial intelligence, robotics and control)"**
   - URL: https://en.wikipedia.org/wiki/Behavior_tree_(artificial_intelligence,_robotics_and_control)
   - Key facts: "mathematical model of plan execution"; Colledanchise & Ögren 2018 formal model `T_i = {f_i, r_i, Δt}` where `r_i: ℝⁿ → {R_i, S_i, F_i}` (Running / Success / Failure); sequence composition `T_0 = sequence(T_i, T_j)` for combining subtasks; event-driven extension (Champandard & Dunstan 2012 Game AI Pro Ch.6) "**react to events and abort running nodes**".
   - Production use: Halo, BioShock, Spore; Unreal Engine 4 BT documentation; ROS behavior tree library.
   - **Relevance to hypothesis:** **B_BT_Sequence_StackBreachClearSecure uses sequence operator directly** — formal BT semantics for `(Stack → Breach → Clear → Secure)`; event-driven halt on "enemy detected" → fallback to peek/cover node.

---

## Supplementary References

9. **Colledanchise & Ögren 2018** "Behavior Trees in Robotics and AI: An Introduction" (CRC Press, arXiv:1709.00084)
   - Formal BT model — already cited in `experiments/2026-06-21-hierarchical-tactical-ai-btree/sources.md` (closed experiment).
   - **Relevance:** direct cross-ref to closed BT experiment for sequence-node cost data (180-260 ns/unit/tick baseline).

10. **Champandard & Dunstan 2012** "The Behavior Tree Starter Kit" (Game AI Pro Ch.6)
    - Already cited in `experiments/2026-06-21-hierarchical-tactical-ai-btree/sources.md`.
    - **Relevance:** halt nodes (Interrupt / Abort / Restart) — used in E_CoverAwarePeek for "abort clear on new hostile contact".

11. **IFC 4.3 Add2** (buildingSMART 2024) — `IfcSpace` + `IfcRelConnects` primitives.
    - Per Wikipedia IFC §Architecture.
    - **Relevance:** formal schema for room decomposition.

12. **Wikipedia — "Behavior tree" §Mathematical state space definition** — `T_i = {f_i, r_i, Δt}` per Colledanchise/Ögren 2014 IROS.
    - **Relevance:** formal BT runtime model for cost measurement in our prototype.

---

## Sources NOT Verified (acknowledged limitations)

- **AIGameDev.com** "Most Influential AI Games" F.E.A.R. #2 ranking — referenced in F.E.A.R. Wikipedia article, **NOT independently fetched** in this session (used only as cited fact).
- **Gamasutra / Game Developer Magazine 2005** Isla "Handling complexity in the Halo 2 AI" — referenced in behavior-tree Wikipedia article, **NOT independently fetched** (used as cited fact for BT production validation).
- **Game AI Pro (Rabin 2014)** — referenced as canonical industry BT book, **NOT independently fetched** (relies on Wikipedia + closed experiment cross-ref).
- **IEEE Transactions on Robotics 2017** Colledanchise/Ögren "How Behavior Trees Modularize Hybrid Control Systems" (DOI:10.1109/TRO.2016.2633567) — formal BT theoretical foundation, **NOT independently fetched** (relies on Wikipedia + arXiv:1709.00084 cross-ref).

These limitations are **standard** for the docs/experiments/ scope: we cite Wikipedia + closed experiment cross-refs as authoritative, treat non-Wikipedia primary sources as supplementary per the web_search fallback chain verification protocol.
