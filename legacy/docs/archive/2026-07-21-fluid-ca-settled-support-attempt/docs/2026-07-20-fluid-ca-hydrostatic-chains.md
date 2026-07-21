# Fluid CA Hydrostatic-Chains Implementation Plan — AS BUILT

> **Status:** implemented 2026-07-20. This document records what was actually built;
> the algorithm source of truth is `docs/superpowers/specs/2026-07-20-fluid-ca-hydrostatic-chains-design.md`.

**Goal:** Replace the BFS-pressure Fluid CA with the hydrostatic-target + gradient-chain CA.

**As-built architecture:** `src/voxel/VoxelWorldFluid.cpp` (driver: idle fast-path, READ, run-shift FALL, COMMIT) +
`src/voxel/VoxelWorldFluidBodies.cpp` (engine: 6-connected bodies, target T, distance field D, chains) +
`src/voxel/VoxelWorldFluidInternal.hpp` (engine declaration).

**Spec:** `docs/superpowers/specs/2026-07-20-fluid-ca-hydrostatic-chains-design.md`

## Global Constraints (as applied)

- `main` only; no commits without operator order.
- `fluidFill` 0/255 storage, untouched by CA logic; `PROJECTV_FLUID_CA_GPU` stays OFF; `fluid_ca.comp` untouched.
- Fixed scan orders; no position/tick hashing; files ≤ 600 lines (hence the driver/engine split).

## Tasks (as executed)

### Task 1: Archive the BFS-pressure attempt ✅

`legacy/docs/archive/2026-07-20-fluid-ca-bfs-pressure-attempt/` — working-tree `VoxelWorldFluid.cpp` +
`FluidCATests.cpp` + README with the runtime-failure evidence.

### Task 2: Hydrostatic engine rewrite ✅

- `UpdateFluidCA` rewritten as a thin driver; engine `ProcessFluidBodyChains` in a new TU.
- **FALL** changed to contiguous run shift (whole run descends 1 cell/tick — stream never shreds; fixes the operator's "
  air gap between drop and puddle").
- **Target T** per body: reachability (lateral-if-supported, fall traces marking fall-entry cells) + fill in
  `(y, dist, z, x)` order, body cells at dist 0 (same-layer stickiness).
- **Distance field D** to nearest T cell over body + reachable Air (reverse legal moves: fall-into,
  slide-into-supported, live fall nodes).
- **Chains** K=8/body/tick, path ≤64 through grounded cells only (free fall = FALL only, no teleport): sources =
  grounded top-layer cells (stride rotation for area-uniform descent); destinations = adjacent Air with finite D at
  same-or-lower y, guard `(Σy, ΣD) strictly decreases`; **bay fallback** (fillable Air with ≥3 body neighbours, source
  with ≤1).
- **Grounded restriction** (cells supported down to Solid/bottom): chains never accelerate falling water.
- Tests: 13 kept, `Height1WalksIntoPitOnly` → `Height1StepsDownOffPlatform`, `CraterDepth2DrainsToFloor` moved to
  contract level (mass on bottom — pit or outside rim).

### Task 3: New anchors N1–N4 ✅

N1 StrictLayeredDrain (per-layer per-tick prefix rule), N2 ContinuityNoTeleport (6-adjacent growth, ≤K/tick), N3
StreamContinuity (contiguous stream column), N4 SphereBreakEndToEnd (side-hole drain to rim, no hover, settle). 24/24
green.

### Task 4: Idle fast-path + DoD ✅

- `VoxelWorld::{fluidEditSerial,lastFluidCaSerial}`: bump on Air↔Fluid transitions in `SetVoxelMaterial` and on
  `RebuildVoxelWorldDerivedState`; early-out in `UpdateFluidCA`; own-commit serial handled via `serialAtEntry`.
- Test N5 IdleFastPath. Pre-existing build break fixed out of scope: `tests/MeshShaderTests.cpp` structured bindings
  updated for 3-field `MeshCullPushConstants`.
- Full build + full ctest + runtime ASCII log verification on the sphere scene.

### Task 5: Docs sync ✅

Spec rewritten to the final gradient model; `agent/knowledge.md` §8 replaced; `agent/workspace.md` Now-line.

## Commit message suggestion (operator orders the commit)

```
feat(voxel): rewrite fluid CA as hydrostatic target + gradient chains

Per-body bottom-up target T with distance field; top-layer-only chain
sources with (Σy, ΣD) guards make oscillation impossible by construction.
Run-shift FALL keeps streams contiguous; idle fast-path frees settled water.
Refs: docs/superpowers/specs/2026-07-20-fluid-ca-hydrostatic-chains-design.md
```
