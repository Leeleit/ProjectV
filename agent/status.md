# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-04-24`

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- Mainline still has the runnable voxel sandbox slice with interaction, control modes, snapshots, lightweight editor, profiling, smoke probes, and the current `walk` controller.
- Latest closed slice: the current lighting baseline is now not only runtime-tunable and runtime-capturable, but also materially closer to the original `10.5.1` contract without washing the scene flat. The sun-shadow path still stays intentionally simple (`2048x2048`, lightweight `3x3` PCF, dedicated all-occluder `shadowIndirectBuffer`, active-chunk shadow fit, authored sun vector flipped only inside the CPU shadow camera, angle-aware receiver bias), yet the direct-light side no longer lives on ad-hoc `spec power + shininess`. `VoxelMaterialVisual` now packs explicit `AO / roughness / metallic / reflectance` data plus transmission tint and fog/emissive/ambient/direct-response hooks, and `voxel.frag` shades the sun with a `GGX + Fresnel-Schlick + Smith` baseline while still honoring authored ambient and diffuse weights so shadow contrast does not disappear into fill light. Real live capture passes still exist on top of that baseline in both opaque-heavy presets: `ChunkGrid` ships with tighter defaults (`depth=0.0010`, `normal=0.0040`, `filter=1.30`) based on `C` captures, while `MeshingStress` now has a reproducible reference shot (`cam -25 19 25`, `look 0.62 -0.48 -0.62`) that produces meaningful diffs for bias candidates even though the tested moderate variants still did not beat the current code baseline clearly enough to justify changing it. The latest warning-cleanup follow-up on top of that baseline also removed the remaining current-source inspection nits in screenshot capture, lighting debug helper contracts, shadow-projection use-sites, descriptor pool constants, and test BMP/debug-HUD helpers; a fresh `problems/tests/` pass then also closed the remaining helper nits in `tests/VoxelWorldTests.cpp` and left only a deliberate file-level `CppDFAUnreachableFunctionCall` suppression for the bespoke single-TU test runner.

## 2. Nearest Gap

- The old P0 process reminders are no longer left open in `TODO.md`: replay-first controller diagnosis, developer-only runtime smoke, and the current warning-cleanup closure are already treated as established baseline, not as unfinished work.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- The next concrete mainline feature gap is no longer "add a shadow debug surface", "stop wasting most of the map on empty padding", "fix missing shadows caused by a flipped sun vector", "replace flat receiver bias with something less brute-force", or "move direct sun off the old Blinn-Phong-like response". The immediate next step inside that gap is to refresh the opaque-heavy `ChunkGrid` / `MeshingStress` capture baselines under the new BRDF/material contract, because the old `FINAL` screenshots predate the direct-light shift; after that, the remaining `10.5.1` foundation holes are still auto exposure / grading and a stronger ambient-environment contract.
- The current limitation is no longer missing off-frustum occluders; it is that the present sun-shadow baseline is opaque-only. Transparent-heavy `VoxelLab` therefore remains a weak baseline for judging shadow readability until the demo-scene gains opaque anchor geometry or a transparent-shadow policy.

## 3. Next Steps

1. Use the current runtime ladder in opaque-heavy presets such as `ChunkGrid` and `MeshingStress` to tune direct-light + shadow bias/coverage/strength now that the shadow fit no longer burns coverage on empty padded space.
2. Keep `MeshingStress` tuning anchored to the fixed reference shot (`cam -25 19 25`, `look 0.62 -0.48 -0.62`) plus `C` captures, so the next sweep compares against one stable case instead of drifting between startup views.
3. Decide whether the demo-scene should gain opaque anchor geometry for readable sun shadows or whether transparent casters deserve a separate shadow/look policy.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
