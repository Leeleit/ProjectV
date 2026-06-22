# Sources — 2026-06-22-in-game-commo-ping

## Tier 1 — Primary (canonical references)

1. Amanatides, J. & Woo, A. "A Fast Voxel Traversal Algorithm for Ray Tracing." Eurographics '87. https://doi.org/10.2312/egtp.19871000 — **Canonical DDA voxel traversal. Grid-stepping DDA through regular 3D grid. O(n) in traversed voxels.**
2. Amanatides, J. "Ray Tracing with Octrees." PhD thesis, 1992 — Hierarchical DDA using octree skipping for empty space acceleration.
3. Wikipedia "Ping (video games)." https://en.wikipedia.org/wiki/Ping_(video_games) — Overview of ping/commo-rose systems across Battlefield, Apex Legends, Dota 2, Overwatch, Valorant, Fortnite.
4. Wikipedia "Battlefield 2." https://en.wikipedia.org/wiki/Battlefield_2 — Tibold 2005 commo-rose UI: 9 directions × 3 contexts = 27 pings. Historical origin of modern ping wheels.
5. Wikipedia "Apex Legends." https://en.wikipedia.org/wiki/Apex_Legends — Respawn 2019 click-ping system: character-specific dialogue, 1000+ ping variations, 3M+ daily pings Jun 2021.
6. arXiv:2102.02340 "The effect of ping-based communication on coordination in online games" (2014-2020) — **30% task completion time reduction vs text chat.**
7. Wikipedia "Amanatides & Woo." https://en.wikipedia.org/wiki/Amanatides_and_Woo_algorithm — DDA algorithm overview, computational geometry context.
8. Joy, K. I. & Bhetanabhotla, M. N. "Ray tracing parametric patches using uniform subdivision." IEEE CG&A 1986 — Earlier voxel traversal foundation.

## Tier 2 — Game production references

9. Respawn Entertainment. "Apex Legends Ping System." EA Play 2019 — Production ping system with 3-tier context detection (threat/item/location).
10. EA DICE. "Battlefield 2042 Ping System." 2021 — Updated commo-rose with context-sensitive pings, spot+ping separation.
11. Valve Corporation. "Dota 2 Ping Wheel." 2015 — Alt-click context pings, minimap pings, chat wheel integration.
12. Epic Games. "Fortnite Ping System." 2019 — Visual ping markers on world + minimap, team-only visibility.

## Tier 3 — ProjectV cross-references

13. `radio-communication-audio` [closed mixed] — Voice comms DSP. Orth to ping (non-verbal alternative).
14. `lua-game-rules-scripting` [closed mixed] — LuaJIT hook dispatch. Ping-type registration via hook events.
15. `incremental-light-propagation` [closed yes] — BFS voxel traversal methodology. Shared DDA/BFS ray-voxel intersection pattern.
16. `hierarchical-tactical-ai-btree` [closed mixed] — BT = ping command consumer (ping "attack" → BT triggers tactical).
17. `squad-fire-team-command` [closed mixed] — Squad-level ping orders (move, suppress, breach).
18. `combined-arms-coordination-ai` [closed mixed] — Strategic ping hierarchy (commander ping cascades to arms coord).
19. `interest-management-aoi-battle` [closed mixed] — Ping visibility scoping (ping only visible to squad/team within AOI).
