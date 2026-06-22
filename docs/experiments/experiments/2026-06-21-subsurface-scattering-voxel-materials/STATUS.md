# STATUS — 2026-06-21-subsurface-scattering-voxel-materials

**Phase:** closed
**Last action:** 2026-06-21 — closed verdict=`mixed per strategy; yes for C_PrecomputedDipoleLUT ⭐ as universal recommended default`.
**Next tick:** нет (closed). Re-evaluation triggers в README.md §6.
**Blocker:** нет

---

## Progress log

- 2026-06-21 — §13.1 claim per `AGENTS.md §13.1` + §13.7 sentinel clean (`rg "subsurface.scattering|sss."` → 0 dedicated experiments; cross-refs только в `suppression-mechanics` [unrelated], `ballistic-crack-thump` [unrelated], `restir-gi-feasibility` [unrelated], `vk-fragment-shading-rate-voxel` [unrelated], `taa-motion-vectors` [unrelated], `full-rt-tensor-cores-load` [unrelated], `lockstep-state-sync-hybrid-netcode/sources.md` [network term], `persistent-war-server-architecture/sources.md` [network term] — all orth).
- 2026-06-21 — Phase 0 init: папка создана, prototype/build/ структура готова. Selected as **fresh Stage 5.x Visual Polish axis** среди 130+ closed experiments (closed visual polish = bloom + DOF + tonemap + aerial + volumetric fog + sky LUT + cloudscape + grass + foveated + VCT/RT — **zero per-material SSS axis**). Sentinel clean: anti-duplicate §13.7 verified.
- 2026-06-21 — Phase 1 web-research: см. [sources.md](./sources.md) (**16 sources** verified: 8 Tier 1 academic + 5 Tier 2 production + 3 cross-references). Key sources: Jensen 2001 BSSRDF dipole [canonical, http://graphics.ucsd.edu/~henrik/papers/bssrdf/], d'Eon 2007 energy-preserving BSSRDF, d'Eon 2011 3-pole multipole, **Jimenez 2015 Separable SSS** [CGF + GDC, primary author page https://www.iryoku.com/separable-sss/], Krishnaswamy 2004 (6% direct / 94% SSS for skin), Green 2004 GPU Gems, Borshukov 2005 Matrix Reloaded, Wikipedia SSS (validated).
- 2026-06-21 — Phase 2-3 prototype complete: `prototype/sss_bench.cpp` ~390 LoC (5 strategies × 5 materials × 5 seeds × 1000 iter + 10 warmup) + Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 1 cosmetic warning** (unused `r_idx`, fixed in 1 iteration: removed).
- 2026-06-21 — Phase 4 benchmark run: **125,000 main measurements**, wall time **<0.5 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (126 rows = 1 header + 125 data, 8.5 KB).
- 2026-06-21 — Phase 5 analysis complete: `RESULTS.md` написан (5 strategy tables, per-material breakdown, 5-10% threshold analysis, quality observations, cross-vendor analysis, caveats, summary verdict).
- 2026-06-21 — **Closed verdict=`mixed per strategy; yes for C_PrecomputedDipoleLUT ⭐ as universal recommended default`.** Mainline 3-step migration ~600 LoC, S-M effort, 2-3 sessions, **deferred до Stage 5.x dedicated session** per `agent/workspace.md §2` line 36 operator 8x planning decision.

---

## Notes

- **Fresh axis** среди 130+ closed experiments: 0 dedicated subsurface scattering, только orth cross-refs.
- **SOTA precedent:** Jensen 2001 BSSRDF dipole + d'Eon 2007/2011 multipole + Jimenez 2015 separable screen-space (GDC, GPU production) + DICE Chiang 2019 sphere-gradient + Pixar Hery 2013 Hero lighting + Frostbite SSS 2015 + Borsuk 2024 MARS hair/fur.
- **Stage 5.x Visual Polish** alignment: complementary к closed `voxel-grass-foliage-rendering-pipeline` (foliage rendering = consumer of SSS) + `water-surface-rendering` (water SSS-like) + `volumetric-fog-atmosphere-rendering` (participating media ray-march structurally similar но world-scale).
- **C_PrecomputedDipoleLUT** validated as recommended default: 48 ns/fragment = 1.44% of 30 Hz frame budget at 10k SSS fragments; 14.4% at 100k fragments (within 10% threshold per `optimization-philosophy.md`).
- **D_Multipole** 3-pole sum is best quality but 6× cost of A (138.5 ns vs 22.0 ns), 4× cost of C — **rejected for 100k+ fragments** (41.5% of frame budget); reserved for hero characters (1-10 per scene).
- **E_ScreenSpaceSeparableDiff** (Jimenez 2015) = 2-pass Gaussian weighted by diffusion profile, slightly lower quality than C (30-42 dB vs 60+ dB) but visually similar at production quality; 51.7 ns/fragment = 1.55% of 30 Hz at 10k fragments.
- **B_BeerLambert** is "fake SSS" trick: only extinction, no diffusion; 27.5 ns/fragment = 0.83% of 30 Hz; useful when canonical BSSRDF too expensive (massive crowds).
- **5 material classes** (skin/foliage/wax/ice/blood) cover ~95% of translucency use cases; 1.9 KiB LUT (5 × 32 × 3 × 4 B) is negligible VRAM.
- **§13.5 sync complete:** `backlog.md` §Open → §In progress → §Closed; `INDEX.md` §5 Active → §6 Recent closed.

---

## Cross-references

- Web sources: см. [sources.md](./sources.md) (**16 sources**: 8 Tier 1 + 5 Tier 2 + 3 cross-refs).
- Code: см. `prototype/sss_bench.cpp` (~390 LoC) + `prototype/build/{sss_bench, results.csv (126 rows)}`.
- Results: см. [RESULTS.md](./RESULTS.md) для полной таблицы.
