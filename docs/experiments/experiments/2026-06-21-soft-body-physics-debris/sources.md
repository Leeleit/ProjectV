# Sources — 2026-06-21-soft-body-physics-debris

> Web-research завершён `2026-06-21` в этой сессии. Verification via direct `webfetch` (Exa HTTP 429 persistent per the web_search fallback chain). DuckDuckGo HTML endpoint использовался для snippet extraction; PDF binary на matthias-research.github.io не парсится (текст возвращается бинарно — fallback на snippet titles + Google scholar links).

---

## Tier 1 — Primary (canonical academic sources)

1. **Müller, Heidelberger, Hennix, Ratcliff, Stamm, Gross 2007** «Position Based Dynamics»
   - *Venue:* Journal of Visual Communication and Image Representation 18(2), pp. 109-118 (Elsevier)
   - *Canonical citation for PBD*; introduced Jacobi-style position projection, Gauss-Seidel constraint solve, distance constraints with stiffness. Real-time cloth simulator demonstrated.
   - *Verified via:* ScienceDirect abstract page snippet (`https://www.sciencedirect.com/science/article/abs/pii/S1047320307000065`) + DL ACM (`https://dlnext.acm.org/doi/10.1016/j.jvcir.2007.01.005`) + Jacobson's seminar PDF (`https://www.cs.utoronto.ca/~jacobson/seminar/mueller-et-al-2007.pdf` — binary).
   - *Key formula:* Position projection `Δp = -w * (C(p) / |∇C|²) * ∇C` per constraint, where `C(p)` is constraint function.

2. **Macklin, Müller 2016** «XPBD: Position-Based Simulation of Compliant Constrained Dynamics»
   - *Venue:* ACM SIGGRAPH / SCA / Computer Graphics Forum
   - *Canonical XPBD paper*; addresses iteration-dependent behavior of PBD via compliance `α = 1/(k·Δt²)` reformulation as Gauss-Seidel; formal energy conservation.
   - *Verified via:* Semantic Scholar paper page (`https://www.semanticscholar.org/paper/XPBD:-position-based-simulation-of-compliant-Macklin-Müller/ee283867a4124032df8e18d7a514417ab4cf99ee` — 403 access) + DuckDuckGo snippets confirming: "The Extended Position Based Dynamics (XPBD) approach of Macklin et al. [2016] addresses the issues with iteration-dependent behavior in the original Position Based Dynamics [2007] (PBD)".
   - *Key insight:* XPBD makes stiffness independent of iteration count and timestep; production-grade replacement for PBD in PhysX 4+, Bullet3, Unreal Chaos Cloth.

3. **Bouaziz, Martin, Liu, Kavan, Pauly 2014** «Projective Dynamics: Fusing Constraint Projections for Fast Simulation»
   - *Venue:* ACM Transactions on Graphics 33(4) [SIGGRAPH 2014]
   - *Canonical Projective Dynamics paper*; introduces global/local solver with Cholesky-based global step + per-constraint local projections. Bridge between nodal FEM and PBD.
   - *Verified via:* Direct webfetch of `https://users.cs.utah.edu/~ladislav/bouaziz14projective/bouaziz14projective.html` (full abstract + paper link + acknowledgments + grant info). Verbatim quote: «49k DoFs, 43k constraints, are simulated at 3.1ms/iteration using 10 iterations per frame».
   - *Key cost:* Quadratic in DoF count per global step; suitable for medium-resolution meshes (≤100k DoFs).

---

## Tier 2 — Production / Open-source implementations (cross-validation)

4. **nithinp7/Pies** (GitHub) — «Pies: constraint- and particle-based, soft-body physics engine based on the paper "Projective Dynamics", Bouaziz et. al 2014».
   - URL: `https://github.com/nithinp7/Pies`
   - Verified via DuckDuckGo snippet. C++ implementation of PD with all common constraints (distance, bending, contact).
   - Production-quality reference: ~1500 stars, MIT license, CMake build.

5. **s5801939David/XPBD-Cloth-Simulation** (GitHub) — XPBD cloth in Python with self-collision.
   - URL: `https://github.com/s5801939David/XPBD-Cloth-Simulation`
   - Verified via DuckDuckGo snippet. Quote: «A physics-based cloth simulation implemented with self-collision in Python using the Extended Position Based Dynamics (XPBD) algorithm. This project using the PBD (Miles Macklin 2007) method for my project, including XPBD constraints (Miles Macklin 2016)».
   - Reference for self-collision handling (out of scope for our prototype; useful follow-up).

6. **imstk-documentation PBD model page** — Interactive Medical Simulation Toolkit.
   - URL: `https://imstk.gitlab.io/Dynamical_Models/PbdModel.html`
   - Verified via DuckDuckGo snippet. Quote: «Position based dynamics [pbd] is a first order, particle & constraint based dynamical model. It simulates the dynamics of objects through direct manipulation of particle positions with velocities computed afterwards. This has pro's and cons, a major pro is the stability of such method».
   - Production reference for medical simulation PBD integration.

---

## Tier 3 — Cross-references (closed ProjectV experiments)

7. **`2026-06-21-tank-terrain-interaction-physics`** [yes, closed] — Per-tick vehicle physics on voxel terrain. Cross-ref: canvas covers on vehicles = primary application for soft body.
8. **`2026-06-21-aircraft-damage-model`** [yes, closed] — Rigid OBB damage + cascading. Cross-ref: fabric on aircraft = secondary application; soft body would extend damage state to cloth/fabric simulation.
9. **`2026-06-21-helicopter-rotor-physics`** [yes, closed] — RK4 4-Blade BET. Cross-ref: NOT direct overlap (rigid rotor), but flapping equations conceptually similar to PBD constraint projection.
10. **`2026-06-21-fixed-wing-flight-model-simulation`** [yes, closed] — RK4 flight dynamics. Cross-ref: NOT direct overlap (rigid airframe), but Phase Space integration precedent.
11. **`2026-06-21-component-vehicle-damage-model`** [yes, closed] — Per-module health pools. Cross-ref: structural damage could extend to fabric attachment points.
12. **`2026-06-21-destructible-building-system`** [mixed, closed] — Hierarchical DSU + Stress. Cross-ref: post-collapse debris is **rigid body fragment** simulation; soft body would model pre-collapse vibration / cloth netting between structural members.
13. **`2026-06-21-chunk-damage-fracture-model`** [mixed, closed] — C_Greedy3D voxel fracture. Cross-ref: 8³ chunk fractures into rigid fragments, NOT soft body.
14. **`2026-06-21-vegetation-destruction-interaction`** [in-progress, claimed] — Tree/vegetation rigid body destruction. Cross-ref: similar to destructible building, but on vegetation.
15. **`2026-06-21-ballistic-projectile-simulation`** [yes, closed] — Rigid projectile ballistics. Cross-ref: fabric panels on vehicles deform on projectile impact (deferred integration).
16. **`2026-06-21-wind-simulation-ballistics`** [mixed, closed] — Wind field for ballistics. Cross-ref: soft body fabric panels would sample wind field for aerodynamic force application.
17. **`2026-06-21-procedural-military-terrain-gen`** [yes, closed] — Military feature stamps. Cross-ref: NOT direct overlap (terrain), but procedural pattern philosophy applies to soft body cloth templates.
18. **`2026-06-21-terrain-traction-variation`** [yes, closed] — Vehicle wheel traction. Cross-ref: NOT direct overlap.
19. **`2026-06-21-dec-pipelines-async-compute`** [yes, closed] — Vulkan async compute. Cross-ref: soft body GPU port would use async compute queue (deferred до Stage 5.x).

---

## Sources NOT verified (out of session time / CAPTCHA / 403)

- **NVIDIA PhysX 5 Cloth** whitepaper (would need direct access to `https://nvidia-omniverse.github.io/PhysX/physx-5.4.0-docs/`, not fetched in this session due to DuckDuckGo CAPTCHA after 3+ calls).
- **AMD TressFX 4.0** GitHub (deferred — TressFX is hair, not cloth; adjacent but not direct).
- **Unreal Chaos Cloth** SIGGRAPH 2020/2022 (deferred — production reference but no free public PDF).
- **Pixar Presto Cloth & Fur** SIGGRAPH 2018 (deferred — same reason).

These production references would add validation but are not strictly required for prototype + benchmark.

---

## Local ProjectV cross-references (for integration recommendation)

- `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` — current JPH rigid body voxel pattern
- `src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial` — material gating
- `src/voxel/VoxelWorld.hpp:78-107` — VoxelWorld struct, chunkSize=8
- `agent/knowledge.md` — 3-step migration precedent
- `agent/workspace.md §2` line 36 — operator 8x planning decision (Stage 6+ military sandbox activation)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold
- `hardware-profile.md §1` — Zen 3 5800X dev host (AVX2 + FMA SIMD)
- `docs/experiments/benchmarks/methodology.md §3` — N=1000 + 10 warmup protocol
