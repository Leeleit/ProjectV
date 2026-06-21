# `2026-06-21-vk-fragment-shading-rate-voxel` — STATUS

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Verdict:** `mixed` — global VRS savings validated (50%/75%), hybrid savings **falsified for sparse voxel
scenes** (4-6% coverage → 0% savings via per-region classifier)
**Stage link:** independent (cross-cutting Stage 5.x lighting cost optimization, **follow-up axis** после полного
closure lighting-strategy-axis `2026-06-20`: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
`rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed)
**Estimated effort:** M (~770 LoC standalone C++26 prototype + web-research + analytical cross-vendor projection)
**Author:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»)

---

## Phase

**Claimed** `2026-06-21` per `AGENTS.md §13.1` (slug chosen, sentinel clean, `INDEX.md §5` updated).
**Closed** `2026-06-21` per `AGENTS.md §13.5` single-pass sync.

---

## Last action

`2026-06-21` — closure single-pass sync:
- `README.md` finalized with §1-9 (hypothesis + prior art + method + prototype + results + verdict + integration
  recommendation + sources + mapping).
- `STATUS.md` updated to `concluded-verdict-mixed`.
- `prototype/vrs_voxel_sim.cpp` (~770 LoC, Clang 22.1.6 -O3 -march=native -std=c++26, 0 warnings) built + 100 iter
  benchmark executed on dev host `obvium` Zen 3 5800X + governor `powersave`.
- `prototype/results.csv` (60 rows × 12 cols) + `prototype/RESULTS.md` (detailed analysis) generated.
- `sources.md` (10 primary sources verified) created.
- `backlog.md §In progress` → `§Closed` + `INDEX.md §5` → `§6 Recent closed` per §13.5.
- `backlog.md §Open`: `meshing-algo-comparison` stale duplicate fixed per §13.5 (same pattern as
  `async-compute-overhead-numbers` sync-fix r1).

---

## Hardware baseline

Cross-ref: [`hardware-profile.md`](../hardware-profile.md) §1 (AMD Ryzen 7 5800X Zen 3 dev host `obvium`,
governor `powersave`, 62.7 GiB DDR4) + §3 (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02, dev host
Tier 2 VRS validated per NVIDIA driver 460+ baseline).

**⚠️ VRS extension support not yet captured в `hardware-profile.md §4`** — operator action: verify с
`vulkaninfo --summary | grep -i shading_rate` и append к §4 per `AGENTS.md §14` "Edge cases: ✅ Нужны данные,
которых в файле нет → probe + дополнить файл новой секцией".

---

## Cross-axis continuity

**Closed same-session `2026-06-21`:** dxc-vs-glslc-toolchain + gpu-procedural-noise + frame-flight-allocator +
audio-raytracing + sub-chunk-layers + this = **6 orthogonal axes closed** сегодня.

**Closed `2026-06-20` (lighting cluster, full closure):** vct-vs-rt-cutoff (mixed) + clustered-forward-mass-lights
(yes) + rt-shadows-vs-csm (mixed) + restir-gi-feasibility (mixed) — **this experiment = follow-up axis** для уже
закрытого lighting cluster (cost-side optimization, не strategy-side).

**In-progress parallel сессии `2026-06-21`:** tracy-gpu + wfc-procedural + taa-motion-vectors + gpu-fluid-ca-atomic +
audio-diffraction-hybrid + lod-mesh-downsampling = **5-6 orthogonal axes** still in progress.

**VRS cross-axis risks/follow-ups:**

1. **TAA feedback loop** (per NVIDIA NAS GDC 2019: 3-4 frames transition latency) — cross-axis risk с
   in-progress `2026-06-21-taa-motion-vectors`. Separate experiment needed если VRS + TAA combined.
2. **Stage 4.3 draw distance lift** — если voxel coverage per frame > 30%, hybrid classifier may start working.
3. **VR / foveation** (`eye-tracked-foveated` backlog l-priority) — VRS image = direct feed для gaze-driven VRS.
   Future Stage 7+.
4. **GPU prototype** — CPU analytical model sufficient для hypothesis check, но реальные GPU fragment shader
   timings + visual quality (PSNR/SSIM) + cross-vendor measurement = deferred до Stage 5.x integration milestone.

**Not in scope (out of session scope):** VRS + VR + foveation + cross-vendor GPU measurement + TAA feedback loop.
Future experiments to be claimed per `backlog.md §Open` (no new entries created — this experiment is closed).

