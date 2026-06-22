# STATUS — 2026-06-22-voxel-water-flow-ca

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-22 — Phase 2 prototype built + benchmarked; Phase 3 verdict written
**Blocker:** нет

---

## Progress log

- 2026-06-22 — Claimed per §13.7 sentinel clean. §13.1 protocol: backlog updated, INDEX.md §5 updated, folder + README.md + STATUS.md created.
- 2026-06-22 — Phase 1 web-research: 15+ sources fetched (Wikipedia, Minecraft, Dwarf Fortress, w-shadow.com CA, GitHub voxel-world/gpu-voxel-sim, academic LGA/LBM).
- 2026-06-22 — Phase 2 prototype: `water_ca_bench.cpp` (900 LoC C++26). 5 strategies × 5 scenes × 5 seeds × (10 warmup + 1000 main) iterations = 125,000 benchmarks. Build fixed (negative local coord bug → Euclidean modulo fix). ASan-verified clean. Optimized binary run successful.
- 2026-06-22 — Phase 3 results: Performance hypothesis ✅ (<1 µs/chunk/tick = 13-21× under <10 µs target). Behavioral quality ⚠️ (utility scores 0.002-0.250; S5 fire extinguish fails). Verdict: mixed. Integration recommendation written (~800 LoC plan for Stage 4.1/6+).

---

## Notes

- **Key finding:** 3D CA water at 0.5 µs/chunk/tick is trivially cheap. Mainline can afford 100 Hz tick rate on thousands of chunks with <5 ms total CPU time.
- **Key limitation:** Water propagation speed (WATER_MAX_SPEED=0.1) limits per-tick flow to 0.1 voxel. Need higher speed or sub-stepping for responsive gameplay.
- **B_SimpleHeightCA broken** — heightmap stub doesn't initialize; effectively no-op. Not representative of real 2D shallow-water approaches.
- Cross-axes: orth to `gpu-fluid-ca-atomic-strategy`, `water-surface-rendering`, `wildfire-propagation`, `voxel-hydraulic-erosion`. Complementary to `weather-svo-metafield`.
- Operator: `2026-06-22` — self-invented topic per «выбирай свободную тему или придумывай свою исследуй».
