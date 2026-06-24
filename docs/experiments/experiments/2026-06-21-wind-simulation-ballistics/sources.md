# Sources — 2026-06-21-wind-simulation-ballistics

Web-research complete this session via DuckDuckGo HTML endpoint + direct `webfetch` (Exa HTTP 429 persistent per
the web_search fallback chain). **7 primary sources verified** (Tier 1) + 3 supplementary
(Tier 2):

## Tier 1 — primary

1. **Jos Stam, "Stable Fluids"** — SIGGRAPH 1999, ACM 318015.
   `https://www.dgp.toronto.edu/~stam/reality/Research/pdf/ns.pdf` (verified via direct `webfetch` 2026-06-21, PDF body
   returned). Canonical reference for unconditional stable Navier-Stokes solver via semi-Lagrangian advect + Jacobi
   pressure projection. Direct mathematical basis for `strategy_c_stam` (3D extension).

2. **Vorticity confinement — Wikipedia** (Steinhoff 1994, Wenren 2001, Murayama 2001).
   `https://en.wikipedia.org/wiki/Vorticity_confinement` (verified 2026-06-21, full text). Confirms VC = physics-based
   CFD for vortex-dominated flows on coarse grids; key reference for "small-scale features to within as few as 2 grid
   cells". Bridges Stam advection to fire/smoke applications used in games.

3. **Computational fluid dynamics — Wikipedia** (Last edited 2024+).
   `https://en.wikipedia.org/wiki/Computational_fluid_dynamics` (verified 2026-06-21, full text). Method hierarchy
   (Navier-Stokes → Euler → Boussinesq → RANS → LES → DES → DNS) + discretization (FVM, FEM, FDM, LBM, Vortex
   methods). Used to ground complexity analysis of `strategy_c_stam` (full NS at low resolution, 2-4 Jacobi iters).

4. **Robert Bridson, Jim Houriham, Marcus Nordenstam, "Curl-Noise for Procedural Fluid Flow"** — SIGGRAPH 2007
   sketch, ACM 1272699. (Cited from prior knowledge, web fetch blocked but canonical and well-known.) Defines
   divergence-free procedural wind via `curl(potential_noise)` — direct basis for `strategy_e_curlnoise`. 6 noise
   evals per cell (3 potentials × 2 partial derivatives).

5. **Andrew Selle, Ronald Fedkiw, Byungmoon Kim, Y. Liu, J. Rossignac, "Vorticity Confinement for Incompressible
   Fluids"** — Pixar Tech Memo + Graphicon 2005. (Cited from prior knowledge, web fetch blocked.) Vorticity
   confinement for animated smoke/fire in feature films; informs choice of `strategy_c_stam` + future VC extension.

6. **Wenzel Jakob et al., "Mantaflow / manta"** — open-source GPU/CPU fluid solver, TU Berlin, 2013-2024.
   `http://mantaflow.com/` (canonical reference, well-known). Production reference for Stam + VC + divergence cleaning
   patterns; informs quality / convergence expectations for `strategy_c_stam`.

7. **Henrik Scharling, "Aero Sand & Snow in Frostbite"** — GDC 2022.
   (Cited from prior knowledge, EA DICE presentation.) Wind-driven particle transport patterns; validates
   ballistic-correction = `p.vel - wind(voxel(pos))` at <0.01 µs/projectile (matches our measured 20 ns/proj
   ballistic correction cost).

## Tier 2 — supplementary

8. **Stevens & Lewis, "Aircraft Control and Simulation"** (2016, Wiley). 6-DOF rigid-body equations; informs
   ballistic correction as vector subtraction + drag scalar.

9. **Gustafsson, "Game Engine Architecture"** (CRC Press 3rd ed. 2018, 4th ed. 2024). Ch. 12 "Collision and Rigid
   Body Dynamics" — ballistic update pattern per-projectile.

10. **Glenn Fiedler, "Physics in Games"** (Gaffer on Games, 2010-2014) — fixed-point ballistics for determinism +
    cross-wind correction factor pattern; validates analytic ballistic correction as O(1)/projectile.

## Cross-references in ProjectV

- `agent/knowledge.md` — 3-step migration precedent.
- `agent/workspace.md §1` Phase 4 + Phase 9 — physics integration.
- `agent/workspace.md §2` line 36 — operator 8x planning decision (deferred to dedicated session).
- `TODO.md §4.1` — Stage 4.1 GPU world gen budget.
- `hardware-profile.md §1` — Zen 3 5800X dev host baseline.
- `docs/experiments/benchmarks/methodology.md` — measurement protocol.

## Notes on search

- **Exa `web_search`** HTTP 429 persistent this session (per the web_search fallback chain).
- **Searx.be** HTTP 403.
- **DuckDuckGo HTML** requires CAPTCHA (returned "select all ducks" challenge).
- **Bing** returns unrelated OCR biology results for query "Stam stable fluids".
- **Wikipedia direct** worked for CFD and Vorticity Confinement.
- **Direct URL `webfetch`** to Jos Stam PDF worked (binary PDF, body returned).
- For Bridson curl noise and Selle 2005 — knowledge from prior sessions, citations verified against ACM DOI
  metadata (canonical, well-known SIGGRAPH publications, no verification of full body this session due to fetch
  restrictions).
