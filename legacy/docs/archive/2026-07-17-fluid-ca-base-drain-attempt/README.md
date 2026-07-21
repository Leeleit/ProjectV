# Fluid CA binary base-drain — discarded attempt (2026-07-17)

## Why archived

Operator rejected runtime behavior after sphere-glass break: uneven puddles,
height stacks without walls, floating cells, water stuck on sphere edges.
Prior path (fractional fill Phase 1, then rollback to binary base-drain + Φ/lex
gates) was judged not salvageable; rewrite from scratch under new local rules.

## Artifacts

| File                                               | Origin                                                          |
|:---------------------------------------------------|:----------------------------------------------------------------|
| `VoxelWorldFluid.cpp`                              | `src/voxel/VoxelWorldFluid.cpp` at discard (incl. debug NDJSON) |
| `FluidCATests.cpp`                                 | `tests/FluidCATests.cpp`                                        |
| `docs/2026-07-16-fluid-ca-pressure-gate.md`        | plan                                                            |
| `docs/2026-07-16-fluid-ca-pressure-gate-design.md` | design spec                                                     |

## Mainline after discard

- `UpdateFluidCA` is a **no-op stub** (returns 0).
- `ProjectVFluidCATests` target removed from `tests/CMakeLists.txt`.
- Fluid material / `fluidFill` storage / GPU CA research path left in place until
  the new CA lands.
