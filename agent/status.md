# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-04-21`

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- Mainline still has the runnable voxel sandbox slice with interaction, control modes, snapshots, lightweight editor, profiling, smoke probes, and the current `walk` controller.
- Latest closed slice: inspect/world-mutation tooling is now materially better in the live sandbox (`INSPECT` chunk telemetry, `X` box anchor, `M` material pick), the HUD now splits into basic vs detailed modes (`G`), one-block auto-jump is runtime-toggleable (`J`) with a delay that only arms on the immediate rise, the glass-neighbor meshing regression is closed so opaque blocks stay visible under glass, and the remaining local DFA warnings in `VoxelInteraction.cpp` / `DebugOverlays.cpp` were cleaned by tightening helper contracts. `build -> tests -> smoke` is green on that state.

## 2. Nearest Gap

- The next controller loop should still stay replay-first; the stacked placed-block wall-climb capture, both boosted creative-flight wedge captures, and the current auto-jump delay regression are closed on the current build.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- After this tooling slice, the next practical layer is still `simple sandbox interactions` on top of the current sandbox, not a broad architectural rewrite.

## 3. Next Steps

1. Wait for the next live walk repro on the current build before adding more broad heuristics; prefer another replay capture if runtime behavior still diverges from tests.
2. Pick `simple sandbox interactions` as the next gameplay/debug slice unless a new live controller repro interrupts it.
3. Keep runtime smoke developer-only until there is an explicit reason to add a separate CI/headless path.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
