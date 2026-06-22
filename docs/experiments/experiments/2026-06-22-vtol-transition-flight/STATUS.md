# STATUS — 2026-06-22-vtol-transition-flight

**Status:** concluded-verdict-mixed (per strategy; `yes` for **C_BlendedTransition ⭐** as universal recommended default; **E_PhysicsCoupledTiltRotor** as safety-critical opt-in for engine-out / corridor-edge cases; `no` for D as default; A/B = baselines only)
**Phase:** Phase 5 (closed)
**Started:** 2026-06-22
**Closed:** 2026-06-22
**Agent:** self

**Last action:** Closed experiment per §13.5. Wrote README.md (8 sections) + RESULTS.md (per-config table) + sources.md (8 primary + 3 supplementary). Prototype builds clean (0 warnings, 0 errors), 125,000 main measurements in 0.094 sec wall time.
**Blocker:** нет.

**Hypothesis validation:**
- H1: All strategies < 0.03 ms (30,000 ns) / craft per tick — **CONFIRMED MASSIVELY** (max mean 442.7 ns = 68× headroom).
- H2: C_BlendedTransition handles 30+ sec transition smoothly — **CONFIRMED** (100% plausible, 11% scene-spread = most uniform).
- H3: D_BlendWithCrossover better than C for moment crossover — **REJECTED** (1.8× cost, no measurable quality benefit).
- H4: E_PhysicsCoupledTiltRotor justified for engine-out — **CONFIRMED** (442.7 ns still < 0.0015% of 30 Hz).
- H5: A/B single-regime baselines — **CONFIRMED** (both < 150 ns, but cannot handle transition).

**Headline (mean ns/tick):** A=110.1, B=120.7, C=132.6 ⭐, D=237.8, E=442.7.

**Integration recommendation:** ~3 LoC mainline default (port `aero_blended_transition` from prototype) + ~120 LoC E opt-in for safety-critical cases. Default `PROJECTV_VTOL_AERO=BLENDED`. **Deferred** до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8× planning decision.

**Cross-axis:** orth to all 18 in-progress parallel на `2026-06-22`; complementary to closed `fixed-wing-flight-model-simulation` [yes, Tier 1 forward-flight] + `helicopter-rotor-physics` [yes, Tier 1 hover] + `tank-terrain-interaction-physics` [yes] + `naval-vessel-buoyancy-steering` [mixed] + `ballistic-projectile-simulation` [yes] + `soft-body-physics-debris` [yes] + `wind-simulation-ballistics` [mixed, crosswind] + `aircraft-damage-model` [yes, engine-out] + `boid-flocking-steering-axis` [yes, V-22 formation] + `group-formation-maneuver-axis` [yes, platoon V-22] + `mesh-shader-mega-instancing` [mixed, rendering] + `dec-pipelines-async-compute` [yes, async aero].

**New axis:** first dedicated VTOL/STOVL transition flight dynamics axis в 138+ closed experiments; opens Stage 6+ military sandbox для AV-8B Harrier / V-22 Osprey / F-35B / F-35C / AW609 / custom tiltrotor craft.
