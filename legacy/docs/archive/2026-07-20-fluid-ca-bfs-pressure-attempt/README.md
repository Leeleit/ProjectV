# Fluid CA BFS-pressure attempt — discarded (2026-07-20)

## Why archived

Uncommitted rewrite (BFS pressure drain + gap-fill tension, 19/19 unit tests green).
Runtime tick log 2026-07-20 (`build/windows-clang-debug/bin/voxel-ascii-tick.log`,
952 ticks): sphere drains non-uniformly across layers (y10=19 left while y9 69→41),
base teleports to distant Air via `findNearestAirViaFluid`, 17 cells hover over an
air gap at settle (y2–y3 empty), one voxel jitters at ticks 3025–3037, no settle.
Operator contract 2026-07-20: layer-ordered uniform drain, no teleport, no hover,
no jitter — requires the hydrostatic-target model instead.

## Artifacts

| File                  | Origin                                                          |
|:----------------------|:----------------------------------------------------------------|
| `VoxelWorldFluid.cpp` | `src/voxel/VoxelWorldFluid.cpp` working-tree version at discard |
| `FluidCATests.cpp`    | `tests/FluidCATests.cpp` at discard                             |

## Mainline after discard

`UpdateFluidCA` rewritten per
`docs/superpowers/specs/2026-07-20-fluid-ca-hydrostatic-chains-design.md`
(hydrostatic target T per connected body + gradient-guided chain moves).
