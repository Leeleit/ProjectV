# STATUS — nanovdb-on-gpu

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-20 — experiment complete. All 5 scenes measured on both SVDAG-on-64-tree
and NanoVDB-aligned layouts, both CPU-side (byte-exact, memory, ray-march proxy) and GPU-side
(standalone Vulkan compute, Mrays/s, byte-exact correctness verification).
**Next tick:** закрыто
**Blocker:** нет

---

## Progress log

- 2026-06-20 — topic claimed per `docs/experiments/AGENTS.md §13.1`. Anti-duplicate sentinel clean.
  `backlog.md` §Open → §In progress. Folder + `STATUS.md` created.
- 2026-06-20 — web research (Exa, 4 batch queries, 26 sources total). Key findings: OpenVDB 13.0.0
  (Nov 2025) lowered NanoVDB mutation barrier, fVDB (NVIDIA ACM 2024) validates NanoVDB+HDDA
  as GPU traversal SOTA, Aokana (arxiv 2505.02017, May 2025) per-chunk SVDAG identical to
  ProjectV design, Mathijs PG 2024 SVDAG-on-GPU editing 5× faster than CPU HashDAG, NanoVDB PR
  #2220 fused accessor 1.4×-2.6× speedup.
- 2026-06-20 — CPU prototype v1 written. Initial bug: used chunkSize=32 (per previous experiment)
  but mainline actually uses chunkSize=8 (per VoxelWorld.hpp:78). Rewrote with mainline constants.
- 2026-06-20 — CPU prototype v2: byte-exact on all 5 scenes (verify_mismatches=0). NanoVDB-aligned
  uses ~50% less memory than SVDAG-on-64-tree. CPU ray-march latency within noise (no clear
  winner — CPU not reliable proxy for GPU).
- 2026-06-20 — GPU prototype v1: standalone Vulkan compute with 2 kernels. Initial bugs:
  shader `uint64_t` required `#extension GL_EXT_shader_explicit_arithmetic_types`, push constant
  used `layout(set=0, binding=3)` instead of `layout(push_constant)`. Both fixed.
- 2026-06-20 — GPU prototype v2: byte-exact on ALL 5 scenes, BOTH kernels
  (verify_mismatches=0). **NanoVDB-aligned wins on 4/5 scenes by 12-141%, ties on solid_8
  (0.6% within noise).** GPU memory: NanoVDB-aligned uses 57-75% less VRAM.
- 2026-06-20 — README.md full fill (9 sections). Verdict `yes`. Recommendation: hybrid strategy
  — keep CPU-side SVDAG (proven by 2026-06-20-svdag-vs-vdb-memory-throughput), but flatten chunks
  into NanoVDB-aligned transient SSBO for Stage 5.1 VCT cone-march + 3 fragment-shader DDA traces
  in voxel.frag per TODO.md §6.2.2.
- 2026-06-20 — backlog.md moved from §In progress → §Closed. INDEX.md §5 Active → §6
  Recent closed. Single-pass per §13.5 sync-обязательство.

## Notes

- **Standalone prototype в `prototype/`:**
    - `cpu_bugfix.cpp` (~720 lines, C++26, no external deps beyond STL). Builds with
      `clang++ -std=c++26 -O3 -march=native -DNDEBUG`. Runs in <1 second.
    - `gpu_traversal.cpp` (~750 lines, C++26, requires Vulkan SDK + libvulkan).
      Builds with `clang++ ... -lvulkan`. Runs in ~5 seconds.
    - `svdag64_spv.h`, `nanovdb_spv.h` — pre-compiled SPIR-V (via glslc) for the 2 compute kernels.
- **GPU host**: NVIDIA RTX 3060 Ti (GA104 Ampere), Vulkan 1.4.350 (driver 610.43.02),
  `subgroupSize=32`. Per `hardware-profile.md §3`.
- **Measured results vs literature:**
    - NanoVDB-aligned ~1200 Mrays/s (this experiment). NanoVDB 2020 paper: 5-44× speedup over CPU TBB,
      but didn't report absolute Mrays/s for our exact traversal pattern.
    - SVDAG-on-64-tree ~500-1300 Mrays/s (this experiment, scene-dependent). dubiousconst282 2024:
      182 Mrays/s CPU, Werner VMV 2024: 108 FPS path-tracing 113 GB volume on consumer GPU.
    - Our absolute numbers are higher than literature CPU numbers (consistent with GPU vs CPU ratio)
      but lower than full HDDA-optimized NanoVDB kernels (we didn't implement warp ballot early-out).
- **Verdict `yes` is based on:**
    - Byte-exact correctness on all 5 scenes × 2 kernels = 10 measurements, verify_mismatches=0.
    - 4/5 scenes cross 5% threshold (per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
    - Maximum advantage 141% (sparse_random_8), well above threshold.
    - GPU memory reduction 57-75% across all scenes.
- **Caveat:** Single GPU vendor (NVIDIA Ampere) measured. Cross-vendor (AMD RDNA, Intel Arc) per
  literature only — Stage 5.1 mainline should re-test on dev matrix.
- **Caveat:** HDDA-specific optimizations (warp ballot early-out, ReadAccessor caching) NOT
  implemented in this prototype's first-iteration kernels. Adding these would likely give
  additional 10-30% per NanoVDB PR #2220 reference numbers.
- **Closes measurement gap from `2026-06-20-svdag-vs-vdb-memory-throughput` §3 line 157**
  «Не реализовывал GPU traversal». GPU side fully measured now.
- **Continuation chain:**
    - `2026-06-20-sparse-64-tree-alternatives` → structural analysis.
    - `2026-06-20-svdag-vs-vdb-memory-throughput` → CPU-side memory + latency.
    - `2026-06-20-nanovdb-on-gpu` (this) → GPU-side traversal + memory.
      Three orthogonal angles of Stage 1.1/1.2 storage analysis, all closed within same week.
