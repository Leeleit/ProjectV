# Fluid CA Column-Hole-Displace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:
> executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the `UpdateFluidCA` no-op stub with the binary column+hole+displace+elevated sidewalk CA from the
design spec.

**Architecture:** Single CPU pass in `VoxelWorldFluid.cpp`: copy fluid AABB into `next`/`claimed`, run four phases (
FALL → BASE_HOLE → BASE_DISPLACE → ELEVATED_SIDEWALK), commit Air↔Fluid swaps. No Φ, no GPU path changes.

**Tech Stack:** C++26, existing `VoxelWorld` / `SetVoxelMaterial` / `GetVoxelMaterial`, Catch-free `TestContext`
harness (`projectv_test_utils.hpp`), CMake test target `ProjectVFluidCATests`.

**Spec:** `docs/superpowers/specs/2026-07-17-fluid-ca-column-hole-displace-design.md`

## Global Constraints

- Work only on `main` (no feature branches unless operator orders otherwise).
- Do not commit unless operator explicitly asks.
- `fluidFill` stays 0/255 bookkeeping only; CA logic ignores fill fractions.
- `PROJECTV_FLUID_CA_GPU` remains default OFF; do not change GPU CA semantics in this plan.
- Inline comments: one trailing line on the same line as the code (AGENTS.md §5.7).
- Debug build is the perf/correctness gate when measuring; do not cite Release as excuse.
- After code lands: update `agent/knowledge.md` §8 and `agent/workspace.md` (same change set / later operator-ordered
  commit).

## File map

| File                                                                        | Role                                                  |
|:----------------------------------------------------------------------------|:------------------------------------------------------|
| `docs/superpowers/specs/2026-07-17-fluid-ca-column-hole-displace-design.md` | Algorithm source of truth                             |
| `src/voxel/VoxelWorldFluid.cpp`                                             | `UpdateFluidCA` implementation + private helpers      |
| `src/voxel/VoxelWorldInternal.hpp`                                          | Only if a shared helper declaration is required       |
| `src/voxel/VoxelWorld.hpp`                                                  | Keep `uint32_t UpdateFluidCA(VoxelWorld &)` signature |
| `tests/FluidCATests.cpp`                                                    | Unit anchors (new file; archive is reference only)    |
| `tests/CMakeLists.txt`                                                      | Restore `ProjectVFluidCATests` target                 |
| `agent/knowledge.md`                                                        | §8 status: CA live again                              |
| `agent/workspace.md`                                                        | Now-line for fluid CA rewrite                         |

Reference (read-only): `legacy/docs/archive/2026-07-17-fluid-ca-base-drain-attempt/` — patterns for test world setup, *
*not** CA logic to copy.

---

### Task 1: Test harness + fall + mass

**Files:**

- Create: `tests/FluidCATests.cpp`
- Modify: `tests/CMakeLists.txt` (restore target near former FluidCA comment ~line 419)
- Modify: `src/voxel/VoxelWorldFluid.cpp` (minimal FALL only until later tasks)

**Interfaces:**

- Consumes: `Make`-style world setup like archive `MakeFluidCATestWorld`; `SetVoxelMaterial` / `GetVoxelMaterial` /
  `UpdateFluidCA` / `BuildFlatVoxelSnapshot`
- Produces: `ProjectVFluidCATests` runnable via ctest; FALL moves one cell down per tick

- [ ] **Step 1: Add CMake target**

Mirror `ProjectVVoxelWorldAsciiTests` pattern: executable `ProjectVFluidCATests` with `FluidCATests.cpp` + needed
`src/voxel/*.cpp` / links (`projectv_build_options`, volk, fmt, flecs, Jolt, SDL3, VMA, glm, json as Ascii tests).
`add_test` + label `unit`.

- [ ] **Step 2: Write failing tests (fall + mass)**

```cpp
void TestFluidCASingleCellFallsOneCellPerTick(TestContext &context);
void TestFluidCAMassPreservedEachTick(TestContext &context);
```

World 8³, Fluid at `{4,5,4}` → after one `UpdateFluidCA`: Air there, Fluid at `{4,4,4}`, `moved==1`. Second test:
random-ish stack, 20 ticks, `CountFluid` constant.

- [ ] **Step 3: Run tests — expect FAIL**

```
ctest -R ProjectVFluidCATests --output-on-failure
```

Expected: link OK, assertions fail (`moved==0` stub).

- [ ] **Step 4: Implement Phase 1 FALL only in `UpdateFluidCA`**

AABB from `fluidCAAabbMin` / `MaxExclusive` (expand Y down by 1). `next` buffer + `claimed`. Bottom-up scan; Air below →
swap; commit via `SetVoxelMaterial` (or direct write consistent with existing fluid AABB updates). Return `moved`.

- [ ] **Step 5: Run tests — expect PASS for Task 1 tests**

```
ctest -R ProjectVFluidCATests --output-on-failure
```

---

### Task 2: Hole predicate + BASE_HOLE (height-1 and DrainBase)

**Files:**

- Modify: `src/voxel/VoxelWorldFluid.cpp`
- Modify: `tests/FluidCATests.cpp`

**Interfaces:**

- Consumes: SIDE_ORDER and hole rules from spec §4–§5
- Produces: Phase 2 after FALL; `DrainBase` for H≥2; height-1 `MoveCell`

- [ ] **Step 1: Write failing tests**

```cpp
void TestFluidCAHeight1WalksIntoSameLevelAir(TestContext &context);
void TestFluidCAColumnBaseDrainsIntoSideHole(TestContext &context);
void TestFluidCANoClimbOntoNeighborTop(TestContext &context);
```

- Floor at y=0 Solid/Floor material; Fluid height-1 beside Air on floor → walks into Air within few ticks.
- Column H=3 at x=2 on floor, Air at (3,0) on floor → after ticks H decreases, mass constant.
- Fluid at (2,0) and neighbor column top empty at (3,1) must **not** climb: after tick, (3,1) stays Air if only climb
  would fill it.

- [ ] **Step 2: Run — FAIL**

- [ ] **Step 3: Implement helpers + Phase 2**

Private in `VoxelWorldFluid.cpp`:

```cpp
static constexpr Int2 kSideOrder[8] = {
	{0, -1}, {1, 0}, {0, 1}, {-1, 0},
	{1, -1}, {1, 1}, {-1, 1}, {-1, -1},
};

bool IsHole(...);           // spec §4 on next/claimed
int ColumnTopY(...);        // from base y upward while Fluid in next
bool TryBaseHoleMove(...);  // MoveCell or DrainBase
```

Wire Phase 2 scan after FALL.

- [ ] **Step 4: Run Task 1+2 tests — PASS**

---

### Task 3: BASE_DISPLACE

**Files:**

- Modify: `src/voxel/VoxelWorldFluid.cpp`
- Modify: `tests/FluidCATests.cpp`

**Interfaces:**

- Consumes: Phase 2 helpers; asks only height-1 neighbor bases
- Produces: Phase 3 per spec §6

- [ ] **Step 1: Write failing test**

```cpp
void TestFluidCACenterTowerDisplacesEdgeOnOpenFloor(TestContext &context);
```

3×3 Fluid on y=0 floor (world larger, Air outside), plus Fluid at center y=1. Settle ≤ 64 ticks: `maxH==1`, mass==10.

- [ ] **Step 2: Run — FAIL** (tower stuck without Phase 3/edge walk alone may be slow; assert maxH==1)

- [ ] **Step 3: Implement Phase 3**

For unclaimed base with H≥2: `TryBaseHoleMove`; else iterate `kSideOrder`, try height-1 neighbor `TryHoleMove`, then
`DrainBase` into vacated cell if still a valid hole.

- [ ] **Step 4: Run tests — PASS**

---

### Task 4: ELEVATED_SIDEWALK

**Files:**

- Modify: `src/voxel/VoxelWorldFluid.cpp`
- Modify: `tests/FluidCATests.cpp`

**Interfaces:**

- Consumes: hole predicate with `restY = elevated.y`
- Produces: Phase 4; top moves sideways when bottom cannot free floor slots

- [ ] **Step 1: Write failing test**

```cpp
void TestFluidCAElevatedSidewalkWhenFloorBlocked(TestContext &context);
```

3×3 Fluid on floor, walls (Solid) sealing all floor-adjacent Air outside the puddle, plus center Fluid at y=1. After ≤
16 ticks: center y=1 is Air **or** Fluid moved to a neighboring (nx,1,nz) over the puddle; mass==10; some cell at y=1
still Fluid (cannot flatten).

- [ ] **Step 2: Run — FAIL**

- [ ] **Step 3: Implement Phase 4**

Scan elevated unclaimed cells; `TryHoleMove` as single cell (never DrainBase).

- [ ] **Step 4: Run all FluidCA tests — PASS**

---

### Task 5: Crater, determinism, sphere smoke

**Files:**

- Modify: `tests/FluidCATests.cpp`
- Modify: `src/voxel/VoxelWorldFluid.cpp` only if bugs found

- [ ] **Step 1: Add tests**

```cpp
void TestFluidCACraterDepth2DrainsToFloor(TestContext &context);
void TestFluidCADeterministicReplay(TestContext &context);
void TestFluidCAUnsupportedStepRestYMinus1(TestContext &context);
```

- Rim Fluid + pit depth 2 → mass on bottom within N ticks.
- Same world two independent runs → identical material grids after N ticks.
- Fluid at y=2, Air at y=1 and y=0 beside → eventually reaches y=0 (M8).

- [ ] **Step 2: Fix until PASS**

- [ ] **Step 3: Optional sphere fixture** if cheap to build from preset/snapshot; else document manual ASCII check via
  `VoxelWorldAscii` / tick log. Must not tunnel Solid.

---

### Task 6: Docs sync

**Files:**

- Modify: `agent/knowledge.md` §8
- Modify: `agent/workspace.md` §1 Now

- [ ] **Step 1: Replace §8 discarded/stub language** with: binary column-hole-displace CA live; link design spec; list
  phases; note GPU still OFF.

- [ ] **Step 2: Workspace Now-line** for 2026-07-17 rewrite complete + test target green.

- [ ] **Step 3: Operator-ordered commit only** — suggest message:

```
feat(voxel): restore binary fluid CA with displace and elevated sidewalk

Replace UpdateFluidCA stub with fall/base-hole/displace/elevated phases.
Anchors in ProjectVFluidCATests; no Φ or placeYOnColumn.
```

---

## Spec coverage checklist

| Spec section                  | Task                                    |
|:------------------------------|:----------------------------------------|
| Phase 1 FALL                  | Task 1                                  |
| Hole predicate + M4/M5/M8     | Task 2                                  |
| Phase 2 BASE_HOLE + DrainBase | Task 2                                  |
| Phase 3 DISPLACE              | Task 3                                  |
| Phase 4 ELEVATED_SIDEWALK     | Task 4                                  |
| Crater / determinism / M8     | Task 5                                  |
| knowledge/workspace           | Task 6                                  |
| Anti-oscillation optional     | Out of scope unless Test shows period-2 |

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-17-fluid-ca-column-hole-displace.md`.

**Do not start code until operator approves the design spec.**

After approval, two execution options:

1. **Subagent-Driven (recommended)** — fresh subagent per task, review between tasks
2. **Inline Execution** — execute tasks in this session with checkpoints

Which approach?
