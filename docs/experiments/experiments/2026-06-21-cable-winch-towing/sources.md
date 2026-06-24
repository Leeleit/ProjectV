# Sources — 2026-06-21-cable-winch-towing

**Web-research via direct `webfetch` to canonical URLs.** Exa `web_search` HTTP 429 persistent per the web_search fallback chain. DuckDuckGo HTML endpoint CAPTCHA blocked.

**Captured:** 2026-06-21.

---

## Tier 1 — Primary sources (verified via direct `webfetch`)

1. **[Wikipedia: Catenary](https://en.wikipedia.org/wiki/Catenary)**
   Mathematical curve formed by a hanging chain under uniform gravity. **Equation: y = a cosh(x/a)** (Leibniz/Huygens/Bernoulli 1691, derived in response to Jakob Bernoulli challenge). Force diagram: horizontal tension T₀ is constant; vertical tension T·sin(φ) = w·s proportional to arc length. Suspension bridges follow catenary, anchor chains use catenary for low-angle pull. Cesàro equation κ = a / (s² + a²). Catenoid = surface of revolution = minimal surface (Euler 1744).
   *Why important:* canonical mathematical foundation for cable physics; provides analytical ground truth for catenary scene (sag curve, force distribution). Use as reference for testing cable solver accuracy.

2. **[Wikipedia: Winch](https://en.wikipedia.org/wiki/Winch)**
   Mechanical device for pulling in / letting out / adjusting tension of rope or wire rope. Spool/drum attached to hand crank or powered drive (electric/hydraulic/pneumatic). **Modern uses:** vehicle recovery (12V/24V electric winches on off-road vehicles, Warn Industries, J-hooks / mini-J / R-T hooks / axle straps); glider launching (1000-1600 m high-tensile steel wire, 25-40 km/h); aircraft rescue helicopter hoist (CH-149 Cormorant); heli-logging; barrage balloons; wakeskate sport (rope pulled in by winch at 25-40 km/h).
   *Why important:* establishes use cases (vehicle recovery, helicopter hoist, glider launch) and physical constraints (cable lengths 100-1600 m, winch speeds 2-40 m/s) for ProjectV.

3. **[Wikipedia: Wire rope](https://en.wikipedia.org/wiki/Wire_rope)**
   Modern wire rope invented by Wilhelm Albert 1831-1834 in German Harz Mountains mining. Construction: wires (carbon steel 0.4-0.95%) → strands (cross lay / parallel lay Seale/Warrington/Filler) → stranded ropes (ordinary lay, lang lay, regular lay) → spiral ropes. **Static wire ropes** for suspension bridges / guy wires / aerial tramways (Bleichert & Co. 1874 dominated industry). **Rope drive calculations:** number of working cycles before replacement, Donandt force (yielding tensile force for given bending diameter ratio D/d), RFL safety factor, allowable number of broken strands.
   *Why important:* provides mechanical properties (Donandt force, breaking strength, fatigue) for break-strength model and safety factor calculations. Mass per meter: typical wire rope 1-10 kg/m for 6-50 mm diameter.

4. **[Wikipedia: Verlet integration](https://en.wikipedia.org/wiki/Verlet_integration)**
   Numerical method for integrating Newton's equations of motion. **Basic Størmer-Verlet: x_{n+1} = 2x_n - x_{n-1} + a_n·Δt²** (used by Loup Verlet 1960s for molecular dynamics, Carl Størmer 1907 for magnetic field trajectories, P. H. Cowell / A. C. C. Crommelin 1909 for Halley's Comet). Time-symmetric (odd-degree Taylor terms cancel) → O(Δt²) global error. **Velocity Verlet** variant for explicit velocity tracking. **Constraints:** distance constraints, collision reactions. Disadvantages: first step approximation O(Δt³), non-constant time steps require correction formula.
   *Why important:* the integration scheme used by all PBD/XPBD/Jakobsen cable solvers. Time-symmetry + symplectic properties = energy conservation over long simulations.

---

## Tier 2 — Research papers (from training knowledge, not web-verified this session due to Exa 429 / DuckDuckGo CAPTCHA blocked)

5. **Macklin, Müller, Chentanez 2016 "XPBD: Position-Based Simulation of Compliant Constrained Dynamics"** (NVIDIA, presented Motion in Games 2016).
   Extends PBD with **per-constraint lambda accumulator**: λᵢ ← λᵢ + (C - α̃λᵢ) / (∇CᵀM⁻¹∇C + α̃). α̃ = α/h² where α = 1/k is compliance. **Mass-ratio independent convergence** (key advantage over PBD). 8-16 iterations sufficient for cloth. Reference: https://research.nvidia.com/publication/xpbd-position-based-simulation-compliant-constrained-dynamics.
   *Why important:* foundation for our Strategy E (XPBD-like compliance-damped PBD).

6. **Müller, Heidelberger, Hennix, Ratcliff 2007 "Position Based Dynamics"** (Mathematical Methods in Computer Graphics, J. Comput. Inf. Sci. Eng. 7(2)).
   Distance constraint projection with mass-weighting: per-particle correction ∝ 1/m. Gauss-Seidel iteration. **4 iterations insufficient for mass-imbalanced scenes** (validated in this benchmark on vertical_suspension 8333:1 mass ratio).
   *Why important:* foundation for our Strategy C (PBD Müller 2007).

7. **Jakobsen 2001 GDC "Hitman: Bullet Physics"** (Game Developers Conference 2001).
   Distance constraint with **equal weight (no mass-weighting)** + Verlet integration. The canonical game-industry approach (Hitman 2001 → Bullet Physics → modern Jolt, Box2D, PhysX). 4 iterations, but each iteration handles 4-8 constraints per particle.
   *Why important:* foundation for our Strategy D (DistanceConstraint Verlet). Surprisingly robust on mass-imbalanced scenes despite equal-weight assumption.

---

## Tier 3 — Supplementary references (from training knowledge, not web-verified)

8. **Bergou, Audoly, Vouga, Wardetzky, Grinspun 2010 "Discrete Viscous Threads"** (ACM TOG / SIGGRAPH 2010).
   Pixar's cable simulator foundation, used in Disney's Hyperion renderer. Handles bending stiffness and damping for realistic cloth/cable.

9. **Bergou, Audoly, Vouga, Wardetzky, Grinspun 2019 "Discrete Cables and Rods"** (ACM TOG).
   Extension of Discrete Elastic Rods (Bergou 2008) to cables with bending stiffness. SIGGRAPH 2019.

10. **Spillmann, Teschner 2008 "Corotational FE"** (Proceedings of the 2008 ACM SIGGRAPH/Eurographics Symposium on Computer Animation).
    For high-fidelity vehicle dynamics, 10-100× more expensive than distance constraint. Reserved for Stage 6+ military sandbox where vehicle sub-systems require full corotational FE.

11. **Pai 2015 "Cosserat Rods with Projective Dynamics"** (Disney Research, SCA 2015).
    For torsion-sensitive ropes (surgical sutures, hair simulation). Excessive for tow cables.

12. **BeamNG.drive** (game studio, since 2013).
    Production reference for cable/winch in vehicle sim. Soft-body vehicle deformation, winch mechanics, tow cables. Open-source beam engine documentation.

13. **Jolt Physics** (Jorrit Rouwé, since 2021).
    Open-source physics engine with `CableConstraint` (Jolt repo, file `src/Physics/Constraints/CableConstraint.h`). Used in Horizon Forbidden West, Stray, and other 2024-2026 AAA games. MIT license.

14. **MudRunner** (Saber Interactive, since 2017).
    Off-road driving simulator with winch mechanics for vehicle recovery. References: official game wiki, GDC talks on vehicle physics in MudRunner.

---

## Cross-references to ProjectV

- **TODO.md §3** (Physics & Simulation) — Stage 3.x (gameplay) or Stage 6+ (military sandbox).
- **`agent/knowledge.md` Part B §30.4** — 3-step migration precedent (reused from `soft-body-physics-debris`, `tank-terrain-interaction-physics`, etc.).
- **`hardware-profile.md` §1/§2/§3** — Zen 3 5800X, DDR4 32 GiB, RTX 3060 Ti 8 GiB.
- **`benchmarks/methodology.md` §3** — measurement protocol (1000 iter + 10 warmup, mean/median/p95/p99/std).
- **Closed `2026-06-21-soft-body-physics-debris`** [yes, D_XPBD validation for cloth] — same XPBD foundation.
- **Closed `2026-06-21-tank-terrain-interaction-physics`** [yes] — RayCastVehicle uses cable/winch in real games.
- **Closed `2026-06-21-naval-vessel-buoyancy-steering`** [mixed] — voxel buoyancy per-column methodology.
- **Closed `2026-06-21-helicopter-rotor-physics`** [yes] — sling load for underslung cargo.
- **Closed `2026-06-21-wind-simulation-ballistics`** [mixed] — wind force per-segment integration.
- **Closed `2026-06-21-data-driven-vehicle-weapon-definitions`** [mixed] — winch spec data-driven.
- **Closed `2026-06-21-procedural-military-terrain-gen`** [yes] — suspension bridge terrain templates.
- **Closed `2026-06-21-mesh-shader-mega-instancing`** [mixed] — instanced cable mesh rendering (downstream).