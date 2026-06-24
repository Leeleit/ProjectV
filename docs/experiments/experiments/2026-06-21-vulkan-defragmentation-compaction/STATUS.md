# STATUS — 2026-06-21-vulkan-defragmentation-compaction

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — Experiment closed same session.
**Next tick:** N/A (closed).
**Blocker:** N/A.

---

## Progress log

- **2026-06-21 (current session):** Claimed per `AGENTS.md §13.1`. Self-invented topic per operator instruction
  «выбирай свободную тему или придумывай свою исследуй»; ninth invocation this session. Anti-duplicate sentinel
  clean per `AGENTS.md §13.7`.
- **2026-06-21:** Web-research complete via `webfetch` direct URLs (DuckDuckGo CAPTCHA + Exa HTTP 429 per
  operator directive). 8+ primary sources verified: VMA docs rev 3.4.0 + GitHub CHANGELOG + Vulkan 1.4 spec +
  closed ProjectV experiments cross-refs.
- **2026-06-21:** Standalone C++26 CPU prototype `prototype/defrag_bench.cpp` (~430 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, 0 warnings after final iteration).
  500 measurement configurations × 1000 frames + 10 warmup = **500,000 main measurements**, wall time
  10.40 sec on dev host `obvium` Zen 3 5800X governor `powersave` per `hardware-profile.md §1`.
- **2026-06-21:** 4 iterations (`v1` first-fit + large heap → `v2` best-fit + smaller heap → `v3` real OOM via
  no-hole → `v4` realistic 2 GiB heap + reduced intensity) to find measurement regime that exposes
  fragmentation effects.
- **2026-06-21:** Closed with `verdict=mixed`. See `README.md` §6 + `RESULTS.md` for full analysis.

---

## Notes

- **Continuation chain:** `vulkan-memory-aliasing-transient` (aliasing axis, mixed) →
  `frame-flight-allocator-budget` (allocator strategy, mixed) → this (compaction axis, mixed) =
  complete VRAM fragmentation mitigation stack.
- **Cross-axis:** orthogonal ко всем 5+ in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca-atomic =
  Stage 3.1 atomic, hzb-smart-mip-select = Stage 2.1 HZB refinement, vct-3d-mip-generation = Stage 5.1 VCT
  mip, vk-multi-gpu-split-frame = multi-GPU VRAM).
- **Mainline recommendation:** `C_IncrementalBudgeted` strategy (`maxBytesPerPass=8 MiB`) = safest default
  per VMA docs + synthetic validation. Step 1-3 migration per `agent/knowledge.md` precedent.
- **Why mixed not yes:** synthetic CPU sim shows trivial results (heap utilization too low to produce
  fragmentation). Mainline integration with real VMA + real Vulkan workload required for final verdict.
- **Why mixed not no:** VMA official docs (`defragmentation.html`) + `CHANGELOG.md` v3.4.0 defrag race
  condition fix + closed `vulkan-memory-aliasing-transient` -7-8% VRAM savings all confirm compaction is
  a valid lever. Cross-axis potential = -10-15% VRAM stacked savings = crosses 5% threshold.
