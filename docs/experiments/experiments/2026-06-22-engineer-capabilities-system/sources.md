# Sources — 2026-06-22-engineer-capabilities-system

**Web-research complete via direct `webfetch` to canonical Wikipedia URLs** (Exa MCP HTTP 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain).

## Primary sources (Tier 1 verified)

1. **Wikipedia "Combat engineer"** (https://en.wikipedia.org/wiki/Combat_engineer, retrieved 2026-06-22)
   - **Canonical terminology**: combat engineer = pioneer = sapper
   - **Mission taxonomy** (verbatim from §"Practices and techniques"):
     - **Mobility**: clearing obstacles, opening routes, constructing roads and bridges
     - **Countermobility**: building obstacles, planting land mines, digging trenches/demolishing bridges
     - **Explosive material handling**: minefield placement/removal, demolition
     - **Assault**: opening routes, demolishing enemy structures
     - **Defense structures**: building fortifications, outposts, fences
   - **Equipment taxonomy**: hand tools (hammers, sledges, saws, shovels) + vehicles (IDF Caterpillar D9 armored bulldozer, German Army *Dachs*, French EBG, French EFA mobile bridge)
   - **Obstacle breaching tools**: Bangalore torpedo, MICLIC, antipersonnel obstacle breaching system, plastic explosives
   - **Production precedent**: U.S. Army Sapper Leader Course (28-day at Fort Leonard Wood), US Army MOS 18C (Special Forces Engineer Sergeant)
   - **Why important**: defines the canonical combat-engineer mission taxonomy that maps directly to ProjectV engineer capabilities (build/repair/demolish/clear).

2. **Wikipedia "Military engineering"** (https://en.wikipedia.org/wiki/Military_engineering, retrieved 2026-06-22)
   - **NATO definition**: "military engineering is that engineer activity undertaken, regardless of component or service, to shape the physical operating environment. Military engineering incorporates support to maneuver and to the force as a whole, including military engineering functions such as engineer support to force protection, counter improvised explosive devices, environmental protection, engineer intelligence and military search."
   - **Sub-disciplines**: mobility / countermobility / explosive engineering / survivability (NBC defense)
   - **Historical precedent**: Roman *architecti* corps, Vauban-era French Corps of Engineers (1670s), British Royal Engineers (1812 establishment for "Sapping, Mining, and other Military Fieldworks")
   - **Why important**: NATO definition maps to ProjectV's role architecture (engineer is the in-game actor that performs mobility/countermobility operations on the voxel battlefield).

3. **Wikipedia "Sapper"** (https://en.wikipedia.org/wiki/Sapper, retrieved 2026-06-22)
   - **Historical origin**: French *sapeur* from verb *saper* = "to undermine, to dig under a wall or building to cause its collapse" (Vauban-era siege warfare)
   - **Specific usage by nation**:
     - **Commonwealth** (UK, Canada, Australia, NZ, India): sapper = private rank in Royal Engineers, trained in bridging (ACROW, Medium Girder Bridge), mine warfare, EOD, water supply, combat diving, tactical breaching
     - **Israel**: sapper (*palas*) = profession code with levels 05/06/08/11 (basic/general/commander/officer)
     - **France**: *sapeur* (combat engineers), *sapeur-pompier* (firefighters), *pionnier* (Foreign Legion)
     - **Italy**: *Guastatori* (combat), *Pionieri* (construction), *Pontieri* (bridging), *Ferrovieri* (railroad)
     - **Pakistan Army**: Corps of Engineers led by Engineer-in-Chief (Lt Gen)
     - **United States Army**: Sapper Tab earned via 28-day Sapper Leader Course; 4 skill tabs in AR 670-1
     - **PAVN/Viet Cong**: *đặc công* ("special task") commando-sappers with bombs/mines/explosives
   - **Why important**: provides canonical capability list (bridging, mine clearance, EOD, water supply, combat diving, breaching, fortification) that informs ProjectV engineer operation set.

## Cross-references to closed ProjectV experiments

- `2026-06-21-trench-fortification-construction` [closed mixed] — B_TemplateAABB_RLE construction algorithm, **downstream consumer** of engineer state-machine commands.
- `2026-06-21-field-fortifications-system` [closed mixed] — C_PrefabPhysicsHull fortification physics hull, **downstream consumer** of engineer placement operations.
- `2026-06-21-bridge-building-repair` [closed mixed] — B_TemplateAABB_RLE bridge template + structural audit, **downstream consumer** of engineer build/repair operations.
- `2026-06-21-aircraft-damage-model` [closed yes] — aircraft component damage, **downstream consumer** of engineer repair operations.
- `2026-06-21-component-vehicle-damage-model` [closed yes] — vehicle per-component damage, **downstream consumer** of engineer repair operations.
- `2026-06-21-factory-production-system` [closed mixed] — factory output for materials, **upstream provider** of engineer inventory materials.
- `2026-06-21-supply-logistics-simulation` [closed mixed] — supply transport for engineer materials, **upstream provider** for engineer material requests.
- `2026-06-21-data-driven-vehicle-weapon-definitions` [closed mixed] — JSON-defined blueprints, **upstream provider** of engineer construction templates.
- `2026-06-21-lockstep-state-sync-hybrid-netcode` [closed mixed] — deterministic lockstep, **downstream consumer** of engineer state for multiplayer.
- `2026-06-21-after-action-replay-system` [closed mixed] — replay system, **downstream consumer** of engineer operation events for deterministic replay.
- `2026-06-21-ecs-1m-entities-bottleneck` [closed yes] — Flecs entity registry host for engineer entities.
- `2026-06-21-cover-system-terrain-adaptive` [closed mixed] — per-unit cover score, **downstream consumer** of engineer-built fortifications.
- `2026-06-21-infantry-soldier-sim` [closed yes] — per-soldier physical simulation, **complementary** (engineer is specialized soldier).

## Verification

**Sentinel §13.7 clean — claim `2026-06-22`**:
- `rg "engineer.capabilities|engineer.class|engineer.role|engineer.kit|foxhole.engineer|sapper"` over `INDEX.md` + `experiments/` = only orth cross-ref в `2026-06-22-bridge-building-repair/README.md` mentioning "engineer" as downstream prerequisite.
- `ls experiments/*engineer*` = ENOENT pre-claim.
- All 3 web sources verified via direct `webfetch` to canonical Wikipedia URLs (canonical production-grade references for combat engineer role definition).
- Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per the web_search fallback chain → direct webfetch to Wikipedia primary sources.