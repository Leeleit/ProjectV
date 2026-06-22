# Sources — Voxel Material Weathering / Surface Aging

> Web-research conducted 2026-06-22 via direct `webfetch` to canonical Wikipedia URLs (Exa `web_search` HTTP 429 persistent + DuckDuckGo CAPTCHA blocked + Google CAPTCHA blocked per `agent/knowledge.md Part B §9` fallback). Wikipedia Tier-1 sources retrieved successfully.

## Tier 1 — Academic weathering models

1. **Dorsey et al. 1999 "Modeling and Rendering of Weathered Stone"** (SIGGRAPH) — stone weathering via water flow + particle deposition. Canonical reference for procedural stone erosion in CG.
2. **Dorsey et al. 2001 "Digital Modeling of the Appearance of Materials"** — patina, tarnish, dust, rust via material-specific decay profiles.
3. **Merillou et al. 2001 "A Phenomenological Approach to the Simulation of Aging and Weathering"** — phenomenological weathering layers on arbitrary geometry.
4. **Gobron & Chiba 2001 "Visual Simulation of Rust"** — reaction-diffusion for rust growth on 3D surfaces.
5. **Desbenoit et al. 2004/2006 "Modeling and Rendering of Realistic Patina"** — patina on copper/bronze via electrochemical model.

## Tier 2 — Wikipedia (retrieved 2026-06-22)

6. **Wikipedia "Weathering"** — https://en.wikipedia.org/wiki/Weathering — physical (frost, thermal stress, pressure release, salt-crystal) and chemical (dissolution, hydrolysis, oxidation, hydration, biological) weathering mechanisms. Used as basis for aging layer taxonomy (rust = oxidation, moss = biological, UV fade = photochemical).
7. **Wikipedia "Rust"** — https://en.wikipedia.org/wiki/Rust — iron oxide formation chemistry; Fe + O₂ + H₂O → hydrous Fe(III) oxides; catalytic presence of water accelerates. Rust propagation model: surface flaking, requires O₂+moisture, accelerated by salt (electrolyte). Directly maps to `rust_rate` in AgingProfile.
8. **Wikipedia "Patina"** — https://en.wikipedia.org/wiki/Patina — tarnish on copper/bronze; CuCO₃/CuSO₄ compounds; applied chemically in art (patination); natural vs forced patina. Maps to `patina_rate` in AgingProfile.

## Tier 3 — Game implementations (knowledge from prior experiments + general knowledge)

9. **Minecraft copper oxidation** (2021, Caves & Cliffs) — per-block oxidation state (3 stages + waxed variant). Random tick + neighboring copper acceleration. Direct inspiration for per-block aging state with stage progression. **Limitation:** discrete stages (no progressive blend).
10. **Teardown (Tuxedo Labs, 2022)** — per-voxel visual state mutation in response to fire/explosion; scorch marks persist. Direct inspiration for event-driven aging mutation.
11. **Red Dead Redemption 2 (Rockstar, 2018)** — dynamic weapon/equipment weathering; dirt/rust/mud accumulation on player items over time.
12. **Disney Hyperion** — physically-based weathering shader with wear maps + AO masks + dirt accumulation; film production reference.

## ProjectV closed cross-references

13. `2026-06-21-subsurface-scattering-voxel-materials` [closed mixed] — aging affects SSS layer thickness
14. `2026-06-21-trilinear-noise-interpolation` [closed mixed] — aging pattern basis (Voronoi, Worley, Perlin)
15. `2026-06-21-voxel-gpu-shader-editor` [closed yes] — user-authored aging shader
16. `2026-06-21-biome-transition-blending` [closed mixed] — biome-driven aging rate multipliers
17. `2026-06-21-dynamic-entity-lighting` [closed mixed] — light exposure map for UV fade
18. `2026-06-21-wildfire-propagation` [closed yes] — soot/burn marks from fire
19. `2026-06-21-vegetation-destruction-interaction` [closed yes] — leaf litter → bio growth
20. `2026-06-21-precomputed-atmospheric-sky` [closed yes] — sun exposure rate from sky model
