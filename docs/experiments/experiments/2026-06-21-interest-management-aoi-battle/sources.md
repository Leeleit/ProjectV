# Sources — 2026-06-21-interest-management-aoi-battle

Verified references from web research (8 primary sources, all Exa `web_search` 2026-06-21):

## Tier 1 — Primary references

1. **ESEngine AOI (Area of Interest)** — production-grade grid-based AOI manager.
   <https://esengine.cn/en/modules/spatial/aoi/>
   - Author: ESEngine team
   - Date: 2025-2026 (live docs)
   - **Key contribution:** canonical grid-AOI reference. `cellSize` = 1-2x average view range.
     `addObserver(player, position, {viewRange, observable})` API. Blueprint nodes:
     `GetEntitiesInView`, `CanSee`, `OnEntityEnterView`, `OnEntityExitView`.
   - **Used for:** cell-size heuristic (1-2x view range), observer entity registry pattern.

2. **Netcode optimizations for MMORPGs** — production postmortem.
   <https://wirepair.org/2025/12/20/netcode-optimizations-for-mmorpgs/>
   - Author: wirepair.org blog
   - Date: 2025-12-20
   - **Key contribution:** concrete bandwidth numbers for MMOs. 60-70 bytes per player update,
     7-10 packets/player/tick × 100 players = 800 packets/tick × 30 Hz = 24,000 pkt/s = 1.344 Mbps
     per player = 434 TiB/month. Distance tiering: 50 m / 150 m / 250 m zones. Jolt sensor pattern
     for AoI (3x3 multi-sensor, 126 m chunks).
   - **Used for:** per-player bandwidth baseline, packet batching justification, distance tiering
     inspiration.

3. **Photon Fusion 2 — Interest Management** — production netcode reference.
   <https://doc.photonengine.com/fusion/current/manual/advanced/interest-management>
   - Author: Photon Engine documentation
   - Date: 2025-2026 (live docs, Fusion 2)
   - **Key contribution:** `Send Priority` per-object priority value determines send frequency
     (high-priority close = 20 Hz, low-priority far = 1 Hz). AOI modes: Area / Global / Explicit.
     `AddPlayerAreaOfInterest()` API. `AutoAOIOverride` for nested objects.
   - **Used for:** per-object priority queue pattern (Strategy D), 3-tier frequency structure.

## Tier 2 — Supporting references

4. **Spatial Interest Management in Networked Games** — academic paper.
   <https://www.ee.ucl.ac.uk/lcs/previous/LCS2011/LCS1121.pdf>
   - Author: John Mitchell
   - Date: 2011 (UCL LCS)
   - **Key contribution:** VELVET (Variable AOI based on K Nearest Neighbours). Cross-layer
     optimization (Application + Session). Occlusion as Interest metric.
   - **Used for:** KNN-based variable AOI radius (Strategy E).

5. **esengine AOI fix commit c5adfff** — production optimization.
   <https://github.com/esengine/esengine/commit/c5adfff0fc50a55b00835f2f193dbd9aebff2940>
   - Author: esengine/esengine
   - Date: 2026-04-09
   - **Key contribution:** original `_updateObserversOfEntity` iterated ALL observers (O(N²)).
     Fix uses spatial grid to limit to `cellRadius` cells around moved entity (O(K) where K = 9
     cells × avg_observers_per_cell).
   - **Used for:** validates 9-grid lookup as production standard, not theoretical.

6. **MMO Online Game AOI Algorithm** — Chinese MMO dev post.
   <https://dev.to/aceld/11-mmo-online-game-aoi-algorithm-l7d>
   - Author: aceld
   - Date: 2023-08-17
   - **Key contribution:** canonical 9-grid pattern. `GetGIDByPos(x, y)` →
     `GetSurroundGridsByGid(gid)` → aggregate entities from 9 cells. AOI entry/exit events drive
     network sync.
   - **Used for:** 9-grid algorithm reference.

7. **AFLL: Real-time Load Stabilization for MMO Game Servers Based on Circular Causality Learning** —
   academic paper.
   <https://arxiv.org/html/2601.10998>
   - Authors: AFLL research group
   - Date: 2026 (arXiv 2601.10998)
   - **Key contribution:** learning-based load balancing. 3 limitations of static AOI:
     (1) unrecognized circular causality, (2) static policy, (3) no runtime adaptation.
   - **Used for:** modern frontier (deferred — out of scope single session).

## Cross-axis (closed experiments referenced)

- `2026-06-21-ecs-1m-entities-bottleneck` (verdict=yes) — Flecs 0.5 ns/ent iteration,
  supports 1M+ ents. AOI uses Flecs entity registry.
- `2026-06-21-multi-resolution-collision-broadphase` (verdict=mixed) — D_QuadTree winner
  at 10k bodies. AOI grid = same spatial indexing pattern at smaller scale.
- `2026-06-21-flow-field-pathfinding-10k-units` (verdict=yes) — C_FlowField_BFS.
  AOI reduces pathfinding scope by 50×.
- `2026-06-21-dynamic-battlefield-decal-system` (Phase 4/4) — AOI needed for decal replication
  scope (only send decals to players near them).

## Open `lockstep-state-sync-hybrid-netcode` (h, not started)

- AOI is foundation for state-sync bandwidth reduction. This experiment validates
  grid AOI + 3-tier + top-K as default approach.

## Caveats

- All sources verified during this session (2026-06-21) via Exa `web_search` (HTTP 200, 8 results).
- DuckDuckGo HTML fallback not needed this session.
- No URL shorteners, all direct canonical URLs.
