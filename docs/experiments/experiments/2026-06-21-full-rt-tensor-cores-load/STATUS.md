# 2026-06-21-full-rt-tensor-cores-load — STATUS

**Phase:** concluded-verdict-mixed (Phase 4 complete — closure sync pending).
**Last action:** Phase 0-3 done (reservation + web-research + prototype + measurements + RESULTS + README + sources).
Phase 4 (closure sync: INDEX §5→§6 + backlog → §Closed per §13.5) pending this entry.
**Blocker:** нет.
**Date next tick:** this session.

---

## Phase log

- **2026-06-21 Phase 0** — reservation created in `research/backlog.md §In progress` per §13.1/13.2; folder +
  `prototype/` scaffolding created; STATUS.md written.
- **2026-06-21 Phase 1** — web-research via `webfetch` DuckDuckGo fallback (Exa HTTP 429 persistent per operator
  directive); 14 fetches total (4 DuckDuckGo HTML + 10 direct primary fetches); **33 sources verified** (30 Tier 1
  primary + 6 Tier 2 closed-experiment cross-refs + 4 Tier 3 architecture/theory).
- **2026-06-21 Phase 2** — standalone C++26 CPU cycle-budget harness written `prototype/cycle_budget.cpp` (~620 LoC,
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green, 0 warnings**); 14
  candidates (8 RT + 6 Tensor) × 7 workloads × 5 seeds × 1000 iter + 10 warmup = **490 configs × 1000 iter =
  490,000 main measurements**, wall time **31 ms** на dev host `obvium` Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Two build iterations: (1) initial sm_count=30 bug fixed to sm_count=38 (RTX 3060 Ti
  GA104-200 = 38 SMs); (2) tensor practical efficiency 50% → 30% per Jeff Bolz NVIDIA blog benchmark reference.
- **2026-06-21 Phase 3** — results analysis + ranked recommendation list + cross-vendor matrix. **Key findings:**
  6 RT candidates cross 5% threshold (1.60-6.25× speedup) + 2 RT anti-patterns (RT_GISurfelVisibility + RT_HBAO_8RayHemi
  show 0.40× speedup — RT 2.5× SLOWER than generic при low op-per-ray count); 4 Tensor candidates recommended
  (77-307× peak); 2 Tensor anti-patterns (BRF_LUT_Interp memory-bound, SmallMLP_PostEffect too small).
- **2026-06-21 Phase 4 (this entry)** — README.md + RESULTS.md + sources.md written. Closure sync pending.

## Closure sync (per §13.5)

Phase 4 actions (next):
1. `docs/experiments/INDEX.md §5 Active experiments` → §6 Recent closed (insert per template).
2. `docs/experiments/research/backlog.md §In progress` → §Closed (insert per template + mark `Closed this session`).

## Anti-duplicate perimeter (per §13.7) — verified

- **NOT** implementing VCT temporal denoise via cooperative_matrix (parallel `vct-temporal-denoise-tensor-core`).
- **NOT** implementing SSR via RTX ray query (parallel `rtx-screen-space-reflections`).
- **NOT** SOTA-GI path-tracing survey (closed `restir-gi-feasibility`).
- **NOT** VCT/RT roughness cutoff policy (closed `vct-vs-rt-cutoff`).
- **NOT** RTX shadow strategy (closed `rt-shadows-vs-csm`).
- **THIS** = cross-cutting inventory + cycle-budget + ranked recommendations + anti-pattern discovery. **Verdict=mixed** per
  operator §Open line 16 l-priority + «parked» tone.

## Top-3 mainline recommendations

1. **`RT_MeshletCulling`** — 6.25× speedup + +0.5 PSNR, 310 LoC, Stage 2.1/2.2 meshlet cull replacement.
2. **`Tensor_VCT_TemporalDenoise`** — 307× peak (~100-150× realistic) + +2.5 PSNR, parallel agent covers impl.
3. **`RT_SoftShadow_RRQSS`** — 1.60× + +2.0 PSNR (highest quality gain), 280 LoC, Stage 5.2 local-light shadows.

## Anti-patterns (DO NOT ADOPT) — saves ~550 LoC + 6 MiB VRAM

1. **`RT_GISurfelVisibility`** — 0.40× speedup (RT slower than generic).
2. **`RT_HBAO_8RayHemi`** — 0.40× speedup (RT slower than generic).
3. **`Tensor_BRF_LUT_Interp`** — 48× peak but PSNR=0 (texture memory-bound).
4. **`Tensor_SmallMLP_PostEffect`** — 38× peak but PSNR=0 + 550 LoC for no gain.