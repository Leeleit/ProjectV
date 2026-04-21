# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-04-21`

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- Mainline still has the runnable voxel sandbox slice with interaction, control modes, snapshots, lightweight editor, profiling, smoke probes, and the current `walk` controller.
- Latest closed slice: narrow-edge jump replay coverage now matches the real player-visible contract again. The controller only keeps ultra-thin edge support alive under an active jump request, so ordinary walk-off fall timing is back to normal, while the replay fixture still proves the edge jump launches. `build -> tests -> smoke` is green on that state.

## 2. Nearest Gap

- The next controller loop should still stay replay-first; the stacked placed-block wall-climb capture and both boosted creative-flight wedge captures are closed on the current build.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- After this tooling slice, the next practical layer is still gameplay/debug follow-up on top of the current sandbox, not a broad architectural rewrite.

## 3. Next Steps

1. Wait for the next live walk repro on the current build before adding more broad heuristics; prefer another replay capture if runtime behavior still diverges from tests.
2. Pick the next gameplay/debug slice after `walk`: `inspect tools`, simple sandbox interactions, or debug world-mutation helpers.
3. Decide whether headless/self-hosted smoke should exist in CI.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
