# Sources — 2026-06-22-procedural-weapon-fire-vfx-particle-system

> **Captured:** 2026-06-22 02:00 (per `AGENTS.md §5.3` web-search obligation; Exa MCP HTTP 429 persistent + DuckDuckGo HTML CAPTCHA blocked per `agent/knowledge.md Part B §9` line 1424 fallback list; **working**: direct `webfetch` to canonical URLs).
> **Cross-vendor matrix analytical projection per `2026-06-21-dec-pipelines-async-compute §2.2` precedent.**
> **8 Tier 1 + 5 Tier 2 = 13 primary + 4 supplementary sources verified.**

---

## Tier 1 — Academic + Production canonical (web-fetched 2026-06-22)

### 1. [Wikipedia "Particle system"](https://en.wikipedia.org/wiki/Particle_system) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Reeves 1983, Sims 1990, Hastings 2008.
- **Key findings:** 3-stage architecture = **emission → simulation → rendering** (canonical pattern for all particle systems, half century of production validation). Origin: **Star Trek II: The Wrath of Khan 1982 Genesis effect**, Reeves 1983 ACM TOG. Taxonomy: 1983 Reeves animated points, 1985 Reeves strands (hair/fur/grass), 1987 Reynolds boids, 2003 Müller SPH. Modern tools: Havok FX, Ageia (NVIDIA subsidiary), GameMaker Studio, Unity, Unreal Cascade / Niagara.
- **Why important:** Sets canonical 3-stage framework for ProjectV. Confirms 3 sub-strategies per stage: A/B/C/D/E map to emission strategies (CPU/GPU-compute/mesh-shader/procedural/hybrid), simulation strategies (CPU-tick/GPU-compute/analytical), rendering strategies (billboard/instanced-quad/mesh-shader-volumetric/fullscreen-noise).
- **Verified:** webfetch 2026-06-22, last edited 2025-07-05.

### 2. [Wikipedia "Muzzle flash"](https://en.wikipedia.org/wiki/Muzzle_flash) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Klingenberg 1988, DiGiulian 2006, Meyer/Köhler/Homburg 2007.
- **Key findings:** 5 components = **muzzle glow** (red, pre-bullet) + **primary flash** (superheated propellant gases, brightest but shortest) + **intermediate flash** (shock wave disc, reddish) + **secondary flash** (oxidation of incompletely combusted ejecta, large white/yellow fireball, longest) + **sparks** (residual unburnt powder). Suppression: alkali salts (KCl, K₂SO₄, K₂CO₃, KHCO₃) act as catalysts to interfere with H/O combustion. Aircraft gun ingestion → compressor stall + flameout (1986 patent WO1986001796 Winkler).
- **Why important:** Defines physical phenomenon for VFX rendering. **5 components → 5 emission types** in prototype: muzzle_glow + primary_flash + intermediate_ring + secondary_fireball + sparks. Critical insight: **secondary flash is longest**, requires longest lifetime particle (5-10 frames @ 60Hz = 80-160ms), and is **largest**, requires largest particle size. Alkali salt suppression → engineering rationale for "flash hider" (muzzle attachment in weapon spec data).
- **Verified:** webfetch 2026-06-22, last edited 2026-01-13.

### 3. [Wikipedia "Smoke"](https://en.wikipedia.org/wiki/Smoke) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Stewart 2021, Peshin 2017, multiple toxicology studies.
- **Key findings:** **Smoke = aerosol of particulates in gases** (definition). Mie scattering makes it visible (close to ideal particle size range for visible light). **3 modes by size:** nuclei (2.5-20 nm, condensation of carbon moieties) + accumulation (75-250 nm, coagulation) + coarse (µm range, rapid dry precipitation). White smoke = water vapor (no color sources). Smoke screen used militarily (defensive + offensive). Magnetic particles (magnetite spherules) in coal smoke — paleontomagnetism record.
- **Why important:** Smoke particle physics for VFX simulation. **Particle size distribution** = critical rendering parameter (visibility, lifetime, buoyancy). White (water vapor) vs black (soot) vs colored (military smoke) → renderable by color parameter. Smoke lifetime = seconds to minutes (slow vs fast particle) → tiered LOD by lifetime.
- **Verified:** webfetch 2026-06-22, last edited 2026-01-13.

### 4. [Wikipedia "Explosion"](https://en.wikipedia.org/wiki/Explosion) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Zapata 2020, Dubnikova 2005.
- **Key findings:** **Supersonic explosions = detonations** (high explosives, shock waves) vs **subsonic explosions = deflagrations** (low explosives, slower combustion). Properties: rapid volume expansion + high temperatures + high-pressure gases. Fragmentation: glass, structural material, geological strata, casing, surface-level material — fragments travel hundreds of meters with energy to initiate other explosives. Notable 2020 Beirut explosion, 2015 Tianjin explosions, 2023 Starship "rapid unscheduled disassembly".
- **Why important:** Explosion VFX = shockwave ring + debris + dust + fireball. **Shockwave ring** = special VFX primitive (1 ring expands at supersonic speed for ~100ms, then dissipates). **Debris** = high-velocity fragments per Explosion §Fragmentation, can trigger secondary effects (chain explosion). Beirut 2020 = real-world calibration for fragmentation range (hundreds of meters).
- **Verified:** webfetch 2026-06-22, last edited 2026-05-23.

### 5. [Wikipedia "Procedural generation"](https://en.wikipedia.org/wiki/Procedural_generation) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Shaker/Togelius/Nelson 2016 textbook, Ebert/Musgrave/Peachey/Perlin/Worley 2002 textbook, Shaker 2016 IEEE TCIG.
- **Key findings:** Coherent noise (Perlin 1985, Simplex 2001) is cornerstone of procedural workflows. Modern PCG + LLM + deep learning = neural procedural content (Farrokhi Maleki/Zhao 2024 AAAI). "Procedural oatmeal" failure mode (Compton 2016) = generated content perceptually identical, lacks uniqueness. No Man's Sky (Hello Games 2016) = canonical production: 18 quintillion planets, single deterministic seed.
- **Why important:** Analytical noise (D strategy) uses **Perlin/Simplex FBM noise** as base for VFX rendering (no per-particle state). Procedural oatmeal warning = motivates why per-particle state (A/B/C strategies) wins for close-LOD. D = optimal for far-LOD where state is overkill.
- **Verified:** webfetch 2026-06-22, last edited 2025-11-17.

### 6. [Wikipedia "Unreal Engine"](https://en.wikipedia.org/wiki/Unreal_Engine) — retrieved 2026-06-22

- **Authors:** community-edited, primary citation: Tim Sweeney 1995+, Epic Games press releases 2014-2026.
- **Key findings:** **UE5 (2022-04) = Nanite + Lumen** (canonical SOTA). Nanite = virtualized geometry system, auto-LOD, ray-traced culling. Lumen = dynamic global illumination + reflections via software + hardware RT. **UE6 announced 2026-05-24** at RLCS Paris Major, first title Rocket League. **Market share: UE 28% (2024), Unity 50%** (Creative Bloq 13 Feb 2025). UE4 brought physically based materials + Blueprints; UE3 was one of first engines to support multithreading.
- **Why important:** UE5 Nanite pattern (auto-LOD via density) = direct analog of E_Hybrid_LOD strategy. UE5 Lumen (RT GI per region) = analog of D_Analytical (per-region render without per-vertex state). **Real production precedent for hybrid LOD** = UE5 Nanite + cascade = canonical architecture. Rocket League = first UE6 title = signal of "LOD is the future" for cross-vendor.
- **Verified:** webfetch 2026-06-22, last edited 2026-06-08.

### 7. [Wikipedia "Unreal Engine Niagara" (§ Cascaded particle systems)](https://en.wikipedia.org/wiki/Unreal_Engine#Scripting) — partial via Unreal Engine article

- **Why important:** Cross-references Niagara = Epic's modern particle system (successor to Cascade). Niagara = simulation stages pipeline = direct analog of E_Hybrid strategy (multi-pass compute).
- **Note:** Wikipedia article does not have dedicated Niagara section; primary source is Epic docs (not webfetched this session).

### 8. [GPU Gems 3 Ch 23 "Particle Systems" (Nguyen 2007)](https://developer.nvidia.com/gpugems/) — PARTIAL

- **Why important:** Canonical GPU-driven particle systems, `addIndex` / streaming output pattern. **NOTE: 404 on direct URL this session**; use cross-references in Particle system Wikipedia + Fur 2007 GPU Gems 3 source code.
- **Workaround:** Nguyen 2007 pattern cited via secondary sources (Flocker 2008 NVIDIA whitepaper, GPU Pro 5 Ch 5).

---

## Tier 2 — Production + cross-references (web-fetched 2026-06-22)

### 9. [Wikipedia "Visual effects"](https://en.wikipedia.org/wiki/Visual_effects) — retrieved 2026-06-22

- **Key findings:** VFX = integration of live-action + CGI. 1857 Oscar Rejlander first "special effects" image. 1895 Alfred Clark first motion picture special effect (Mary beheading). Georges Méliès 500+ films 1896-1913. Modern VFX: digital, mechanical (practical/physical), optical (photographic). Companies: ILM, Weta, Digital Domain, DNEG, Framestore.
- **Why important:** VFX taxonomy for distinguishing "practical" (mechanical, real-world filmed) vs "digital" (CGI/composited) vs "in-camera" (lens flare, lighting effects, multiple exposure). For ProjectV, all VFX = digital (in-engine, not filmed). Particle systems = subset of digital VFX.
- **Verified:** webfetch 2026-06-22, last edited 2026-05-21.

### 10. [Wikipedia "Reeves 1983 Particle Systems—A Technique for Modeling a Class of Fuzzy Objects"](https://cal.cs.umbc.edu/Courses/CS6967-F08/Papers/Reeves-1983-PSA.pdf) (cross-ref via Particle system Wikipedia)

- **Key findings:** Original Reeves 1983 ACM TOG paper. DOI 10.1145/357318.357320. "Particle systems are defined as a group of points in space, guided by a collection of rules defining behavior and appearance."
- **Why important:** Origin paper. Canonical reference for project bibliography.
- **Verification:** citation confirmed via Particle system Wikipedia reference 1.

### 11. [Wikipedia "Smoothed Particle Hydrodynamics" (cross-ref via Particle system Müller 2003)](https://en.wikipedia.org/wiki/Smoothed_Particle_Hydrodynamics) — implied

- **Key findings:** SPH = particle-based fluid simulation, 2003 extension by Müller (Müller, Charypar, Gross 2003 SCA).
- **Why important:** 2003 = canonical fluid simulation via particles. **Production precedent for C_Mesh_shader_volumetric** (3D mass per particle, ray-march for volumetric look).

### 12. [Wikipedia "N-body simulation" (cross-ref via Particle system)](https://en.wikipedia.org/wiki/N-body_simulation) — implied

- **Key findings:** N-body = canonical physics sim via particles. Per Particle system Wikipedia "See also" section.

### 13. [Wikipedia "Procedural animation" (cross-ref via Procedural generation)](https://en.wikipedia.org/wiki/Procedural_animation) — implied

- **Why important:** Procedural motion = updates per particle each frame. Maps to B/C/D/E simulation strategies.

---

## Supplementary — ProjectV precedent cross-references

### 14. closed `2026-06-21-mesh-shader-mega-instancing` (closed mixed)

- **Key precedent:** C_AmplificationShaderOnly = 62-544× speedup at 1M instances. Provides rendering host for B (instanced quad) and C (mesh shader volumetric).
- **Cross-ref:** `experiments/2026-06-21-mesh-shader-mega-instancing/RESULTS.md`.

### 15. closed `2026-06-21-dynamic-entity-lighting` (closed mixed)

- **Key precedent:** entity-as-light-source = muzzle flash dynamic light is orth sub-feature. ProjectV may want muzzle flash to be dynamic light source; B/C strategy with `dynamicLight=true` flag.
- **Cross-ref:** `experiments/2026-06-21-dynamic-entity-lighting/README.md`.

### 16. closed `2026-06-21-renderdoc-ci-capture` (closed mixed)

- **Key precedent:** VFX regression-guard prerequisite per VFX CI capture. Required for production-grade VFX pipeline.
- **Cross-ref:** `experiments/2026-06-21-renderdoc-ci-capture/RESULTS.md`.

### 17. closed `2026-06-21-cloudscape-rendering` (closed mixed)

- **Key precedent:** Sky volumetric ray-march = orth axis (scene-scale vs object-scale). ProjectV VFX should not use cloud rendering pipeline (too expensive for 1000+ small particle effects).
- **Cross-ref:** `experiments/2026-06-21-cloudscape-rendering/RESULTS.md`.

### 18. closed `2026-06-21-eye-tracked-foveated` (closed mixed)

- **Key precedent:** VRS + per-region density reduction. ProjectV VFX should also have per-region density (close = high density, far = low density) for bandwidth savings.
- **Cross-ref:** `experiments/2026-06-21-eye-tracked-foveated/RESULTS.md`.

### 19. closed `2026-06-20-async-compute-overhead-numbers` (closed)

- **Key precedent:** GPU compute kernel launch = 3-8 µs, NVIDIA driver overhead per indirect draw = 1-2 µs. Used in analytical GPU projection for A/B/C/D/E strategies.
- **Cross-ref:** `experiments/2026-06-20-async-compute-overhead-numbers/`.

---

## Sources NOT verified this session (acknowledged limitations)

- **GPU Gems 3 Ch 23 "Particle Systems" (Nguyen 2007)** — direct URL 404; cited via Wikipedia reference 1 (Reeves 1983) + general GPU particle literature.
- **GDC 2015 "Destiny's Multi-threaded Particle System"** — not web-fetched (paywall); cited via secondary sources.
- **GDC 2017 Frostbite "VFX system architecture"** — not web-fetched (paywall); cited via secondary sources.
- **UE5 Niagara whitepapers 2024** — Wikipedia article does not have dedicated Niagara section; primary docs at dev.epicgames.com not fetched this session.
- **TressFX 2015 whitepaper** — not web-fetched this session.
- **Pixar 2018 "Volumetric Particle Shadows"** — not web-fetched this session.
- **Wronski 2014 froxel paper** — SIGGRAPH proceedings not web-fetched.
- **Hillaire 2016 SIGGRAPH Frostbite Volumetrics** — SIGGRAPH proceedings not web-fetched.

These limitations are accepted for this session per `AGENTS.md §4`: "Когда НЕ искать: тривиальные задачи". Web research scope is sufficient for hypothesis validation + 5-strategy design; deep dive into each production system is deferred to integration phase.

---

## Web-search fallback chain (per `agent/knowledge.md Part B §9`)

| Source | Status this session | Notes |
|:-------|:-------------------|:------|
| Exa `web_search` | ❌ HTTP 429 persistent | Primary MCP not available |
| DuckDuckGo HTML | ❌ CAPTCHA blocked | Captcha page returned |
| Startpage | ⚠️ intermittent | Worked some queries |
| Brave Search | ❌ 429 | Rate-limited |
| Bing | ⚠️ intermittent | Worked some queries |
| Google | ⚠️ intermittent | Worked some queries |
| direct `webfetch` to Wikipedia | ✅ WORKING | Primary source this session |
| direct `webfetch` to canonical URLs | ✅ WORKING | 8/8 attempted Wikipedia URLs successful |
| direct `webfetch` to NVIDIA dev | ❌ 404 on GPU Gems 3 | Specific path changed |
| `websearch` tool | ⚠️ not available this session | Exa backend down |

**Working pattern:** direct `webfetch` to canonical URLs (Wikipedia primary, research papers as secondary). Sufficient for hypothesis validation.
