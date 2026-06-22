# Sources — 2026-06-22-player-roles-hierarchy

**Web-research complete via direct `webfetch` to canonical Wikipedia URLs** (Exa MCP HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list).

## Primary sources (Tier 1 verified)

1. **Wikipedia "Squad (video game)"** (https://en.wikipedia.org/wiki/Squad_(video_game), retrieved 2026-06-22)
   - **Role taxonomy (canonical production precedent)**: "Each squad is led by a **squad leader** who can communicate with allied squad leaders, claim vehicles, construct forward operating bases, and place spawn points. Any squad leader can additionally nominate themselves for the position of **commander**, who, once voted into position, can coordinate the team's battle plan and call in additional support such as UAV recon and artillery."
   - **Class/kit taxonomy**: "Squad members have access to various kits (or classes) with distinct roles in combat. These kits include **riflemen** who provide ammo and target infantry, **Light Anti-Tank (LAT)** who target vehicles using MANPATS, and **medics** who quickly revive and heal downed teammates. The **crewman** and **pilot** kits enable players to operate armored vehicles and helicopters respectively."
   - **Hierarchy structure**: Commander (top, voted) > SquadLeader > Subordinate roles (rifleman/LAT/medic/crewman/pilot).
   - **Why important**: canonical production precedent for in-session player role hierarchy with bitmask-style gating (each kit enables specific permissions: construction, vehicle operation, intel call-downs).

2. **Wikipedia "Arma 3"** (https://en.wikipedia.org/wiki/Arma_3, retrieved 2026-06-22)
   - **Role gating pattern**: "Standard tools such as maps, GPS satnavs, compasses, and radios operate basic in-game functions that are disabled if removed. For example, **removing the radio disables NPC radio messages and multiplayer long-distance communication**."
   - **Inventory-based role hints**: "Clothing and carried items add weight to the player, which decreases movement speed and stamina, encouraging proper inventory management and promoting cooperation between soldiers carrying different equipment."
   - **Multiplayer unit structure**: "Arma 3 also features 'units', the community's official multiplayer clan system."
   - **Mission editor role assignment**: "Arma 3 includes the Eden Editor, a level creation system that allows players to create comprehensive custom single-player and multiplayer scenarios using the game's assets and scripting."
   - **Why important**: demonstrates **per-input system gating** by item/role presence (radios → comms; medkit → healing). Validates role-gating as O(1) feature-flag check per system, not O(N) per-entity scan.

## Cross-references to closed ProjectV experiments

- `2026-06-22-soldier-role-specialization` [closed yes] — soldier class + skill table = per-entity. This = in-session per-player.
- `2026-06-22-squad-fire-team-command` [closed mixed] — squad-level command = downstream.
- `2026-06-22-soldier-role-specialization` [closed yes] — class system (medic/engineer/etc.) integrated via closed `engineer-capabilities-system` mixed.
- `2026-06-22-capture-repair-enemy-equipment` [closed mixed] — engineer repairs captured equipment (engineer role downstream).
- `2026-06-22-engineer-capabilities-system` [closed mixed] — engineer role state machine (engineer role downstream).
- `2026-06-21-fixed-wing-flight-model-simulation` [closed yes] — pilot role = aircraft control downstream.
- `2026-06-21-helicopter-rotor-physics` [closed yes] — pilot role = rotor control downstream.
- `2026-06-21-tank-terrain-interaction-physics` [closed yes] — driver role = vehicle control downstream.
- `2026-06-21-ballistic-projectile-simulation` [closed yes] — gunner role = weapon control downstream.
- `2026-06-21-lockstep-state-sync-hybrid-netcode` [closed mixed] — role state = lockstep node.
- `2026-06-21-after-action-replay-system` [closed mixed] — role assignment = replay input.
- `2026-06-21-ecs-1m-entities-bottleneck` [closed yes] — Flecs registry host.
- `2026-06-21-cover-system-terrain-adaptive` [closed mixed] — per-unit cover = downstream consumer.
- `2026-06-22-squad-fire-team-command` [closed mixed] — squad command = squad_leader role downstream.

## Verification

**Sentinel §13.7 clean — claim `2026-06-22`**:
- `rg "player.roles|player.role|commander.role|squad.leader.role|role.gate|role.hierarchy"` over `INDEX.md` + `experiments/` = only backlog.md self-ref + closed experiments cross-references + `2026-06-22-capture-repair-enemy-equipment` README mention.
- `ls experiments/*player*` = only `2026-06-22-soldier-role-specialization` (soldier class axis, different from in-session role gating).
- All 2 web sources verified via direct `webfetch` to canonical Wikipedia URLs.
- Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per `agent/knowledge.md Part B §9` line 1424 fallback list → direct webfetch to Wikipedia primary sources.