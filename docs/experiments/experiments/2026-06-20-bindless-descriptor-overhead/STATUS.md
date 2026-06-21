# STATUS — bindless-descriptor-overhead

**Phase:** concluded-verdict-mixed (recommendation complete)
**Last action:** 2026-06-20 — research complete. `README.md` заполнен всеми 9 секциями (Hypothesis, Prior art, Method,
Prototype, Results, Verdict, Integration recommendation, Sources, Mapping to ProjectV hot-path).
`prototype/bindless_layout_sketch.cpp` (C++26, standalone CPU-side analytical model, ~280 lines) +
`prototype/RESULTS.md` (255 lines captured output). `backlog.md` + `INDEX.md` synced per §13.5.
**Next tick:** по запросу оператора (закрыто)
**Blocker:** нет

---

## Progress log

- 2026-06-20 — открыт. Прочитан `INDEX.md`, `backlog.md`, `sparse-64-tree-alternatives/README.md` (Stage 1.x
  settled — sparse 64-tree verdict=yes), `mesh-shader-vs-compute-cull/README.md` (Stage 2.1 verdict=mixed
  — compute cull default, mesh shader optional). Прочитан `TODO.md` §2.x + `knowledge.md §4`/`§15`/
  `§25`/`§30.4`. Captured host env: AMD Ryzen 7 5800X, clang 22.1.6.
- 2026-06-20 — topic claim per §13.1 (anti-duplicate sentinel §13.7 clean). `backlog.md` + `INDEX.md`
  updated in single-pass (per §13.5 sync-обязательство).
- 2026-06-20 — survey: bindless refs в `src/` — только `VkDescriptorSetLayoutBinding` в 4 pipeline
  files. Никаких `VK_EXT_descriptor_indexing` / `VK_KHR_acceleration_structure` / `bindless`
  не используется. Baseline = 23 bindings across 4 pipelines, ~10-14 `vkCmdBindDescriptorSets`
  per frame.
- 2026-06-20 — web research (8 batch queries, ~30 results). Verified key SOTA claims: NVIDIA Advanced
  API blog 2023 (bindless preference, 1M/2K limits), Khronos spec (descriptor_indexing + push_descriptor +
  descriptor_buffer + Vulkan 1.4 promotion), XDC 2025 (per-vendor HW descriptor costs: NV 32B/32B +
  emulated buffer = 5 indirections, AMD 32B/16B + HW buffer, Intel 64B/16B + Gfx12.5+ dual mode,
  Arm v9+ 32 set bindings HW), Samsung Traha 2024 (3.5ms / 220 calls saved = +5 FPS), Arm Mali
  sample (38% frame time saved by caching), Doom Eternal 1M descriptor bindless + GPU-AV requirement.
- 2026-06-20 — prototype compiled clean with `clang++ 22.1.6 -std=c++26 -O3 -march=native -DNDEBUG
  -Wall -Wextra -Wpedantic`, zero warnings. Output captured в `prototype/RESULTS.md` (255 lines).
- 2026-06-20 — verdict `mixed`. Pure bindless НЕ рекомендуется для ProjectV (cost savings <0.2%
  frame budget, 8× validation overhead, GPU memory bandwidth trade-off). Hybrid strategy =
  bindless для stable resources (material table, Sparse64Node pool, HZB mip, virtual texture
  page table) + traditional+dynamic-offset для transient SSBOs (PackedFace, indirect, motion) +
  push descriptors для small per-draw transient (shadow cascade params). Defer `VK_EXT_descriptor_buffer`
  до NVIDIA native support (current emulation = 5 indirections in VKD3D-Proton).
- 2026-06-20 — `README.md` записан (570 lines, all 9 sections). Integration recommendation с
  5-phase rollout plan (Phase A push shadow cascade → Phase B bindless material table → Phase C
  bindless SVDAG → Phase D bindless virtual texture → Phase E bindless RTX TLAS). Estimated
  effort per phase: XS / S / S / M / M.
- 2026-06-20 — `backlog.md` slot moved `§In progress` → `§Closed` (this entry). `INDEX.md §1/§2/
  §5/§6/§8` updated в single-pass per §13.5.

---

## Notes

- **Pure bindless** для ProjectV = overkill. Текущий CPU cost (≈25 µs / 0.15% frame) **far below**
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Premature
  optimization risk если пойти в full bindless сейчас.
- **Hybrid** — right answer для Stage 2.x. Push descriptors (Phase A) — immediate small win,
  validate infrastructure. Bindless для stable resources (Phases B-E) — incremental rollout.
- **Traha (Samsung 2024)** — strongest direct validation что descriptor update overhead is real
  (3.5ms / 220 calls на mobile MMO). Это показывает что **descriptor update может** быть bottleneck
  при high-frequency-update сценах — но ProjectV current workload не достигает этого масштаба.
- **NVIDIA descriptor buffer emulation penalty** (5 indirections per XDC 2025) — основная причина
  deferring `VK_EXT_descriptor_buffer`. Если NVIDIA adds native HW support (RTX 50-series или
  позже), reconsider.
- **Arm Mali bindless note:** «Adreno 660 performed poorly with bindless» (AsEn benchmarks).
  Mobile is not auto-bindless. ProjectV desktop-first, но `PROJECTV_*_ANDROID` flags (если будут)
  должны учитывать mobile-specific bindless behavior.
- **Validation strategy:** bindless + `PARTIALLY_BOUND` + `UPDATE_AFTER_BIND` требует GPU-AV
  (validation layers can't CPU-check). Debug builds = opt-in `VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT`;
  release preset per `agent/knowledge.md §4` = `PROJECTV_ENABLE_VALIDATION=OFF`. Practical impact:
  bindless migration is debug-slowdown-safe, release-no-impact.
- **3-step migration precedent** per `decisions.md §30.4` (`fluid-ca-reversal` contract) applicable
  here: (a) additive `PROJECTV_BINDLESS_DESCRIPTOR_STRATEGY=phase-a` env, both paths run in
  parallel; (b) flip default per phase; (c) delete legacy path. Same migration pattern.
- **Next experiment candidates** (per `backlog.md §Open` + `INDEX.md §2 Nearest Gap`):
  `hzb-binding-models` (m, Stage 2.2), `dec-pipelines-async-compute` (m, independent),
  `cache-oblivious-chunk-tree` (m, independent), `sub-chunk-layers` (m, independent),
  `wfc-procedural-worlds` (m, independent). `nanovdb-on-gpu` (m, independent) — adjacent к
  in-progress `svdag-vs-vdb-memory-throughput`, escalate per §13.3 если racing.
