# STATUS — `2026-06-21-deferred-translucent-sorting`

**Phase:** **concluded-verdict-mixed** (research + prototype + measurement complete).
**Last action:** Built standalone C++26 CPU prototype `prototype/translucent_sort_bench.cpp` ~510 LoC
(Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 errors**).
Ran full sweep: 6 strategies × 5 scenes × 5 seeds × 5 rotation profiles = **~575 configs × 1000 frames**
(plus warmup) = **~575,000 frame measurements** on dev host `obvium` (Zen 3 5800X governor=`powersave`
per `hardware-profile.md §1`). Per-config results in `build/results.csv`.

**Headline findings:**
- Sort time cost **negligible** across all strategies (~0.6 µs mean) — `std::sort` on 5-236 entries
  is tiny relative to 33.3 ms frame budget.
- Deferred strategies degrade PSNR: Every4 → 36.11 dB, Every8 → 35.13 dB, Every16 → 34.54 dB
  (vs 45.00 dB per-frame baseline). **~10 dB drop** at 8-frame interval.
- DistanceAdaptive matches Every8 (35.13 dB) — slow rotation dominates.
- PerChunk stays at 44.26 dB (within-chunk ordering preserved, only cross-chunk errors).
- **Real cost in mainline is NOT sort time — it's draw call reordering + state change cost**
  (not measurable in CPU prototype).

**Verdict issued = `mixed`:**
- `yes` for the principle: deferred sorting every 8 frames (VoxelCore pattern) is viable
  for low-camera-velocity scenes; 35 dB PSNR is acceptable for many translucent scenarios.
- `no` for CPU sort time savings: the sort itself is negligible (<0.001% of frame budget).
  The actual savings come from GPU draw call amortization.
- DistanceAdaptive adds complexity without meaningful quality improvement over fixed 8-frame.
- PerChunk is a cheap (~35% faster sort) but introduces cross-chunk ordering errors.

**Blocker:** нет.
**Next:** mainline 3-step migration per README §7 (Step 1 ~50 LoC sort manager, Step 2 ~120 LoC
per-frame sort + env gate, Step 3 default flip). Estimated mainline effort: S (~170 LoC, 1-2 sessions).
**Cross-axis:** orthogonal к closed `2026-06-21-taa-motion-vectors` (temporal AA),
`2026-06-21-volumetric-fog-atmosphere-rendering` (fog pass), `2026-06-21-god-rays-crepuscular` (god rays).
