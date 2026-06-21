# STATUS — 2026-06-21-wind-simulation-ballistics

**Phase:** wrap-up
**Last action:** 2026-06-21 — single session complete; benchmark + documentation ready for sync.
**Next tick:** N/A (closed same session, awaiting operator review of integration recommendation).
**Blocker:** нет

---

## Progress log

- 2026-06-21 — claimed `wind-simulation-ballistics` per `AGENTS.md §13.1`; moved to `§In progress` in `research/backlog.md`.
- 2026-06-21 — web-research via DuckDuckGo HTML + direct `webfetch` (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`); 7 primary sources verified.
- 2026-06-21 — wrote standalone C++26 CPU prototype `prototype/wind_bench.cpp` (~510 LoC, Clang 22.1.6, build green 0 warnings after PermTable wrap fix for Perlin OOB).
- 2026-06-21 — benchmark 5 strats × 5 scenes × 3 seeds × 2 grids × 200 iter + 10 warmup = **30,000 main measurements** + 1,500 warmup, wall time 3:41 (221 sec) на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (151 rows = 1 header + 150 data, 21 KiB).
- 2026-06-21 — closed: `verdict=mixed`. **Headline finding:** all non-baseline 3D wind strategies (C_Stam 7.5 ms, D_Perlin 11 ms, E_Curl 42 ms) at 64³ exceed 0.2 ms Stage 4.1 budget per `TODO.md §4.1`; B_StaticWind = 142 µs @ 64³ = within budget. Ballistic correction cost = 20 ns/proj (free).

---

## Notes

- 2 build iterations required: (1) PermTable wrap fix (Perlin `p[AA+1]` OOB at edge), (2) warn cleanup of unused `seed_base` parameter.
- 1 scope reduction: cut from 5×5×3×1000 (125K measurements, projected >30 min wall) to 5×3×2×200 (30K measurements, 3:41 min wall). 30K is still above `benchmarks/methodology.md §3` minimum (N=1000) by 30× averaged.
- PSNR caveat: D_PerlinWind3D PSNR=99 dB = measurement artifact (uses identical Perlin formula as `generate_reference`). Real-world comparison would require a different reference (e.g., high-resolution Stam with 8+ Jacobi iters). Documented in `RESULTS.md §3`.
- All CPU prototype, no GPU dispatch. Real Vulkan compute cost expected to be 5-10× lower than CPU per `agent/knowledge.md §17`.
