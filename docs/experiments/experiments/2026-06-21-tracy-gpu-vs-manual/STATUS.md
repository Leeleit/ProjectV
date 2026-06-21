# STATUS — `2026-06-21-tracy-gpu-vs-manual`

**Phase:** **concluded-verdict-mixed** (research + prototype + measurement complete).
**Last action:** Operator (self) built prototype `prototype/{bench.cpp,CMakeLists.txt,scripts/run_all.sh}`
(after `add_compile_definitions(TRACY_VK_USE_SYMBOL_TABLE)` for Vulkan 1.4 KHR-promoted
function symbol resolution + `TracyVkZoneTransient` for dynamic string names +
`TracyVkCollect` reordering vs command buffer recording state). Ran full sweep:
12 configs (4 × 3 workloads) × 1000 frames + 3 drift configs × 10000 frames = **~42,000
measurements** on dev host `obvium` (RTX 3060 Ti, driver 610.43.02, Vulkan 1.4.341,
AMD Ryzen 7 5800X governor `powersave`, `taskset -c 2`). Per-config results captured
in `prototype/build/results.csv` + 3 drift CSVs (A/B/D at 15 passes, per-1K-window
mean). **Verdict issued = `mixed`** (per `README.md §6`):
- B (Tracy GPU all): +13.7% / +11.8% / +2.8% overhead at 3/8/15 passes — **`no` for
  ≤8, `yes` for ≥15** (above 5% threshold для low-pass).
- C (manual only): within ±5% — **`yes`**.
- D (hybrid): +8.7% / −1.2% / +3.0% overhead at 3/8/15 — **`yes` для ≥8, `mixed` для
  ≤3**. **Best balance для ProjectV Stage 5.x (15+ passes)**.
- No Issue #663 manifest в 10K drift test (B drift = −0.1%, D drift = +3.6%, well below
  +20% alert threshold; A baseline = −7.8% system noise).
- Measured per-zone overhead **1.5-10 µs** (HIGHER than analytical 5-15 ns projection)
  — Tracy has significant per-frame calibration + collect cost.
- p99 variance 2× higher для B vs A (1.45-1.93ms vs 0.68-1.33ms at 3-8 passes).

**Blocker:** нет (build/run complete в `prototype/build/`, no further operator action).
**Next:** mainline 3-step migration per `README.md §7` (Step 1 foundation ~50 LoC, Step 2
per-pass opt-in ~100 LoC, Step 3 default flip). Estimated mainline effort: S (~150 LoC,
2-3 sessions, low risk).
**Date next tick:** this session closed.
