# Fluid CA settled-support attempt (2026-07-21) — FAILED

**Status:** abandoned / archived after operator runtime rejection.

**Symptom (operator):** side-breach stream worse than before — portions / dual
creeks not fixed; further “settled-support + column settle” made behaviour
worse in VoxelLab.

**What this folder is:** snapshot of the working-tree attempt before any
rollback. Not authoritative. Do not revive without a new design pass.

## Contents

| Path                              | What                                             |
|-----------------------------------|--------------------------------------------------|
| `src/VoxelWorldFluid.cpp`         | Driver                                           |
| `src/VoxelWorldFluidBodies.cpp`   | Basin/T/chains engine (settled-support + settle) |
| `src/VoxelWorldFluidInternal.hpp` | Shared internals                                 |
| `tests/FluidCATests.cpp`          | N1–N10 suite (unit green ≠ runtime OK)           |
| `tests/FluidCAGpuTests.cpp`       | GPU gate tests                                   |
| `docs/*`                          | Specs/plans for column-hole + hydrostatic chains |
| `tmp/*`                           | Patch/debug scripts used during the attempt      |
| `working-tree-vs-HEAD.patch`      | Diff of tracked fluid/agent files vs HEAD        |
| `related-voxel-ecs-vs-HEAD.patch` | Related VoxelWorld/ECS diffs vs HEAD             |

## Root cause (post-mortem, short)

Unit tests passed after bottom-of-excess sources + column settle; runtime still
wrong. Prior agent had already stacked heuristics (exit-first, streamMark,
fed-skip, octant rotation). Settled-support fixed N10 in CI but broke operator
visual contract. Stopped per operator.

## Next step (operator decision)

- Leave working tree as-is, or
- Explicitly order rollback of fluid paths to HEAD / last known good commit.
