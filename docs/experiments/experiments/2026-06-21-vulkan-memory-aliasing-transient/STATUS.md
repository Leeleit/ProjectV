# STATUS — 2026-06-21-vulkan-memory-aliasing-transient

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — prototype built green, 60,000 measurements, verdict=`mixed` recorded.
**Next tick:** N/A (closed, awaiting mainline pickup)
**Blocker:** нет

---

## Progress log

- **2026-06-21** — Opened per operator instruction «выбирай свободную тему или придумывай свою и
  исследуй». Anti-duplicate sentinel clean per §13.7. Web-research Phase A complete via
  `webfetch` + DuckDuckGo HTML fallback (Exa 429 persistent per operator directive).
- **2026-06-21** — Verified ProjectV mainline uses 22 separate VMA allocations per frame in
  `SceneFrameResources` (`src/render/SceneResources.cpp:610-680`) + manual barrier insertion in
  `Renderer.cpp:507-536`. Hypothesis: render graph + aliasing = −30-60% peak VRAM, −40-70% barrier
  overhead.
- **2026-06-21** — Built standalone C++26 CPU lifetime simulator (`prototype/mem_alias_bench.cpp`
  ~600 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`, builds green with 10 cosmetic
  warnings на unused constexpr / argc-argv). 3 workloads (minimal_mvp / standard /
  projected_stage5x) × 4 strategies (A_ManualBaseline / B_VMA_SubAllocatorPool /
  C_FullAliasing / D_DAGRenderGraph) × 5 seeds × 1000 iter + 10 warmup = 60,000 main measurements,
  wall time <1 sec на Zen 3 5800X governor=`powersave`.
- **2026-06-21** — **Verdict = `mixed`**:
    - **D_DAGRenderGraph wins on barrier reduction** (74% consistent across all workloads) —
      the **real win**, recommended Step 3 adoption.
    - **C_FullAliasing wins on VRAM** (7-8% on typical + projected workloads, crosses 5%
      threshold per `optimization-philosophy.md`) — recommended Step 2 conditional adoption.
    - **B_VMA_SubAllocatorPool is WORSE than A** baseline (5% pool overhead dominates savings
      without lifetime analysis) — never adopt without aliasing.

---

## Notes

- **Headline finding:** VRAM savings modest (~7-8%), **barrier reduction is the real win** (74%).
- **Persistent image bottleneck:** depth + shadow + hiz + taa history = ~98 MiB cannot be safely
  aliased across frames (write-after-read hazards). Hard limit ~35% VRAM ceiling.
- **Cross-axis orthogonal** to all 5+ parallel in-progress experiments (`hzb-smart-mip-select`,
  `tracy-gpu-vs-manual`, `vct-3d-mip-generation`, `vk-multi-gpu-split-frame`,
  `gpu-fluid-ca-atomic-strategy`).
- **Complementary** to closed `frame-flight-allocator-budget` (allocator strategy = VMA pool,
  NOT aliasing — different lever), `depth-occlusion-quantization` (format axis), `vma-sparse-textures`
  (page-table aliasing, NOT within-frame transient).
- **New axis:** Vulkan transient resource aliasing + render graph DAG для ProjectV-style multi-pass
  renderer not covered by any of 30+ closed experiments.
- **Phased integration recommended** per `agent/knowledge.md` precedent — Step 1 pool only
  (S, ~150 LoC) immediate; Step 2 aliasing (M, ~500 LoC) for Stage 4.3; Step 3 DAG (L, ~1500 LoC)
  for Stage 5.x post-VCT+RTX.
- **Caveats:** CPU simulation only (no real GPU dispatch / driver overhead), synthetic workloads
  (realistic upper-bound), greedy coloring (production render graphs use Pettis-Hansen +10-20%
  better packing), single-GPU dev host (cross-vendor analytical projection).
- **Closed:** moved from `§In progress` → `§Closed` per §13.5; INDEX §5 Active → §6 Recent
  closed per §13.5.
