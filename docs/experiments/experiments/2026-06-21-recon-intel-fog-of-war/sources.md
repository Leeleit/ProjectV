# Sources — recon-intel-fog-of-war

## Tier 1 — Primary game references

1. **WARNO unit characteristics / detection model** — `namu.wiki` WARNO unit optics + concealment + radar + SIGINT mechanics. Confirms: per-unit visual range + concealment modifier + probability-based spotting + radar separate from visual + SIGINT detects without LOS.
2. **WARNO Data Dictionary (dreamfarer/WARNO-DATA)** — `github.com/dreamfarer/WARNO-DATA`. Confirms fields: `OpticalStrength`, `IdentifyBaseProbability`, `UnitConcealmentBonus (Stealth)`, `HitRollECM`, `PorteeVision` (max visual range), `DetectionTBA` (helicopter detection). Real production parameter names.
3. **Foxhole Map Intelligence Wiki** — `foxhole.wiki.gg/wiki/Map_Intelligence`. Confirms: radio backpack, watch tower, observation bunker intel radius; Scout Uniform 80% detection avoidance; intel fades <10 min; air vs ground intel tiers; rain/weather affects detection.
4. **Foxhole Devblog #34 Map Intelligence** — `foxholegame.com/post/devblog-34`. Original design doc for Foxhole intel: shared faction map, time-sensitive intel, radio backpacks, LUV scouting.
5. **HoI4 Intel system** — `hoi4.paradoxwikis.com/Intel`. Confirms: multi-tier intel (no intel → known province → exact position), radar stations, encryption/decryption bonuses, spy networks. Intel categories: civilian/army/navy/air.
6. **HoI4 Ship detection** — `hoi4.paradoxwikis.com/Sub_detection`. Confirms: surface detection, sub detection, radar station coverage, visibility modifiers, task force detection averaging.

## Tier 2 — Implementation references

7. **RTS Fog of War — Vision Layers** — `rts-fogofwar.netlify.app/vision-layers/`. Reference implementation for multi-layer fog of war (ground/upper floor/flying). 4 channel system (RGBA).
8. **GameDev StackExchange — Fog of War implementation** — `gamedev.stackexchange.com/questions/134040`. Practical architecture: CPU for game logic (low-res, deterministic), GPU for rendering (high-res, smooth). Intel aging: dim 10/tick from 255→50→0 over states.
9. **DesignTheGame — Fog of War Systems** — `designthegame.com/learning/tutorial/the-art-science-fog-war-systems-video-games`. Taxonomy: static vs dynamic fog, line-of-sight, stealth interaction, temporary reveals.
10. **Wayward Strategy — Fixing Stealth in RTS** — `waywardstrategy.com/2023/06/26/fixing-stealth-in-rts`. Stealth/detector binary problem analysis: persistent stealth + detector balance.

## Tier 3 — Technical / academic

11. **RTS Fog of War Unity implementation 2025** — `successknocks.com`. Practical Unity implementation: physics-based LOS, half-res vision textures 1024×1024, 0.1-0.2s update rate for distant units, spatial partitioning.
12. **Subspace Infinity fog-of-war ADR** — `github.com/assofohdz/subspace-infinity`. Server-authoritative visibility: per-channel detection (visual/radar), cloaking/stealth mechanics, interest management via component visibility.
13. **Krzysztof Bziuk — RTS Fog of War asset** — Unity asset with multi-layer vision, modular architecture, detection probability.

## Cross-refs to closed experiments

14. `2026-06-21-flood-fill-visgraph-culling` — visgraph flood-fill on 8³ = 55.8 µs worst case, provides LOS basis for visual detection channel.
15. `2026-06-21-interest-management-aoi-battle` — grid-based AOI tiering (critical 200m @ 20Hz, peripheral 200-500m @ 5Hz, ambient >500m @ 1Hz). Fog of war intel broadcast is consumer of AOI.
16. `2026-06-21-multi-resolution-collision-broadphase` — quad-tree spatial query (0.45 ms at 10k ents) provides spatial acceleration for sensor range queries.
