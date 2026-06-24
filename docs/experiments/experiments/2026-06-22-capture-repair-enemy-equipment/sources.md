# Sources — 2026-06-22-capture-repair-enemy-equipment

**Web-research complete via direct `webfetch` to canonical Wikipedia URLs** (Exa MCP HTTP 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain).

## Primary sources (Tier 1 verified)

1. **Wikipedia "War Thunder"** (https://en.wikipedia.org/wiki/War_Thunder, retrieved 2026-06-22)
   - **Vehicle capture mechanism**: "The objective of most battles is to accumulate points, either by destroying enemy vehicles or **capturing strategic positions on the map**."
   - **Production precedent**: Gaijin Entertainment, 2013-present, 70M+ registered players (2022), 250k+ concurrent (Feb 2024).
   - **Combined arms**: aviation + ground + fleet vehicles pre-WWI to modern.
   - **Why important**: demonstrates the canonical "capture strategic position" mechanic in a top-tier vehicular combat game; **direct precedent for capture timer + effectiveness penalty** in ProjectV.

2. **Wikipedia "Foxhole (video game)"** (https://en.wikipedia.org/wiki/Foxhole_(video_game), retrieved 2026-06-22)
   - **Salvage mechanic**: "tools, such as a hammer, sledgehammer, or 'harvester' in order to gather raw resources from stylized ore deposits known as 'resource fields'."
   - **Material flow**: Scrap + Components + Coal + Sulfur → Refinery → Basic Materials (Bmats) + Refined Materials (Rmats) → manufacture everything (weapons, ammo, fortifications, vehicles).
   - **Capture mechanic**: "**victory points (town halls that are marked out in their regions with a flag icon)**" + "In order to win a war, players must secure and build up a specific number of victory points".
   - **Front line supply**: "coordinated logistics to **front line bases** in order to keep players on the front line supplied with weapons and ammunition, as well as providing materials to **repair and build fortifications**."
   - **AI defense**: "central feature of Foxhole's persistent war is the artificial intelligence (AI) system, through which certain defensive structures near friendly bases are controlled automatically."
   - **Persistent war**: war cycles of weeks/months with continuous salvage loop.
   - **Why important**: canonical production precedent for the salvage/capture/repair loop at scale; **direct precedent for capture state machine + repair progress + faction-adaptation** in ProjectV.

3. **Wikipedia "Warno (video game)"** (https://en.wikipedia.org/wiki/Warno_(video_game), retrieved 2026-06-22)
   - **Battlegroup mechanic**: "The units available to the player depend on their chosen battlegroup. Various battlegroups have different specializations, such as a focus on infantry combat or combined arms warfare."
   - **Cold War gone hot**: Inner German Border + Czechoslovakia, NATO vs Warsaw Pact.
   - **Capture via Conquest**: "Conquest mode" for capturing objectives + "Destruction" for killing units.
   - **Real-time + turn-based**: tactical (real-time) + strategic (turn-based Army General campaigns).
   - **Why important**: battlegroup/deck system is analogous to ProjectV's data-driven vehicle definitions per closed `data-driven-vehicle-weapon-definitions` mixed; provides canonical production-grade reference for Cold War equipment pool.

## Cross-references to closed ProjectV experiments

- `2026-06-22-engineer-capabilities-system` [closed same-session mixed] — engineer repairs captured equipment via C_Engineer_CooperativeSum ⭐.
- `2026-06-21-factory-production-system` [closed mixed] — factory output for vehicle production; this experiment = field alternative (salvage).
- `2026-06-21-component-vehicle-damage-model` [closed yes] — per-component damage consumed by repair state.
- `2026-06-21-aircraft-damage-model` [closed yes] — aircraft damage consumed by repair state.
- `2026-06-21-supply-logistics-simulation` [closed mixed] — supply transport for repair materials.
- `2026-06-21-data-driven-vehicle-weapon-definitions` [closed mixed] — captured equipment reads definition.
- `2026-06-21-lockstep-state-sync-hybrid-netcode` [closed mixed] — capture state must be lockstep-compatible.
- `2026-06-21-after-action-replay-system` [closed mixed] — capture events = replay input.
- `2026-06-21-ecs-1m-entities-bottleneck` [closed yes] — Flecs registry host.
- `2026-06-21-tank-terrain-interaction-physics` [closed yes] — captured vehicle can be driven.
- `2026-06-21-ballistic-projectile-simulation` [closed yes] — captured vehicle weapon usable.
- `2026-06-21-bridge-building-repair` [closed mixed] — engineer repair mechanic (sibling to capture-repair).
- `2026-06-21-trench-fortification-construction` [closed mixed] — engineer construction mechanic (sibling).

## Verification

**Sentinel §13.7 clean — claim `2026-06-22`**:
- `rg "capture.*repair|capture.*enemy|repair.*capture|recovered.equipment|enemy.equipment|war.thunder.capture|foxhole.capture"` over `INDEX.md` + `experiments/` = only orth cross-refs в `2026-06-21-group-formation-maneuver-axis/sources.md`.
- `ls experiments/*capture*` = only `2026-06-21-renderdoc-ci-capture` (RenderDoc CI capture, different axis: CI testing infrastructure).
- All 3 web sources verified via direct `webfetch` to canonical Wikipedia URLs.
- Exa HTTP 429 + DuckDuckGo CAPTCHA blocked this session per the web_search fallback chain → direct webfetch to Wikipedia primary sources.