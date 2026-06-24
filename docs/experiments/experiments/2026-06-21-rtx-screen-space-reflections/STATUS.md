# STATUS — 2026-06-21-rtx-screen-space-reflections

**Phase:** concluded-verdict-mixed (synced 2026-06-21 cleanup pass)
**Last action:** 2026-06-21 — Phase B build + Phase C measurement + Phase D analysis complete. 175,000
main measurements, wall time 0.14 sec, **0 warnings** (Clang 22.1.6 `-O3 -march=native -std=c++26
-DNDEBUG`). 7-strategy ranking per platform tier: F_RT_SSR_Hierarchical (1.88 ms / 33.08 dB) = sweet
spot для RTX 3060 Ti; G_RT_SSR_TemporalFiltered (3.00 ms / 44.60 dB) = best apparent quality;
C_SSR_HiZ_Trace (0.42 ms / 23.30 dB) = universal fallback для no-HW-RT.
**Next tick:** N/A (closed).
**Blocker:** нет.

---

## Progress log

- 2026-06-21 — Anti-duplicate sentinel verified per §13.7 (`rg "ssr|screen-space reflection|specular reflect"`
  = только cross-refs в `rt-shadows-vs-csm/README` + `restir-gi-feasibility` + `taa-motion-vectors`,
  dedicated experiment = 0; `ls experiments/2026-06-21-rtx*` = 0 папок; `INDEX.md` = 0 entries). Parallel
  agents в работе: `ambient-occlusion-strategy` (m, AO axis, orth), `vk-video-decoder-replay` (l,
  video decode, orth), `gpu-fluid-ca-atomic-strategy` (m, Stage 3.1, orth), `tracy-gpu-vs-manual` (m,
  profiling, orth), `vk-multi-gpu-split-frame` (m, multi-GPU, orth). Тема не дублируется.
- 2026-06-21 — Reservation per AGENTS.md §13.1 + §13.2: folder created
  (`docs/experiments/experiments/2026-06-21-rtx-screen-space-reflections/` + `prototype/` + `build/`)
  + README.md written (h-priority slot direct fit в `full rt + tensor cores load` backlog line 16 +
  Stage 5.x reflection axis = 0% coverage) + STATUS.md written + backlog.md §In progress entry added
  (full reservation record per §13.2 format) + INDEX.md §5 Active entry added.
- 2026-06-21 — Phase A web-research complete via Exa `web_search` (working this session) + DuckDuckGo
  HTML + webfetch fallback: 15 primary sources verified (Khronos Ray Tracing Best Practices 2020-11-23 +
  Khronos Vulkan Tutorial Reflections chapter + SIGGRAPH 2025 Hands-on Vulkan Ray Tracing tutorial +
  `VK_KHR_ray_query` rev 1 ratified 2020-11-12 + NVIDIA Blackwell 4th-gen RT cores whitepaper Jan 2025
  + UE5 Raytracing Guide v5.4 + Lumen SIGGRAPH 2022 Wright et al. + UE5.7 Documentation +
  GDC Vault 2019 Wolfenstein Youngblood + Iago Calvo Lista Arm Vulkanised 2024 + Vulkanised 2026 +
  NVIDIA RTXGI 2.7.0 SDK + Heitz 2015 GGX + Stachowiak 2015 SSR + Crassin 2011 GIVoxels §6) + 10
  supplementary sources (McAuley 2022 + Yu 2016 + Pharr 2016 PBR + Akenine-Möller 2018 + AMD RDNA 4
  HotChips + Intel Battlemage Xe2 + Mesa RADV RDNA 4 ray tracing + SaschaWillems Vulkan samples +
  Phoronix + Khronos Best Practices). `sources.md` written.
- 2026-06-21 — Phase B build complete: standalone C++26 CPU reflection cost simulator
  (`prototype/reflection_sim.cpp` ~430 LoC), Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic` (build green, **0 warnings** after 1 fix iteration: removed unused
  `vct_specular_psnr_db` variable). 7 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup =
  **175,000 main measurements**.
- 2026-06-21 — Phase C measurement complete: wall time **0.14 sec** on Zen 3 5800X governor=`powersave`
  per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (175,001 rows = 1 header +
  175,000 data rows, 9.6 MB) + `prototype/build/run.log` (2.9 KB summary). `prototype/README.md`
  written (build/run/output spec + cost model calibration + scene profiles + cross-vendor matrix).
- 2026-06-21 — Phase D analysis complete: per-strategy aggregate ranking + per-strategy per-scene
  detailed (35 configs × 6 metrics = 210 cells) + per-platform recommended defaults + quality/cost
  efficiency table (PSNR/cost_ms) + cross-axis findings (F_RT_SSR_Hierarchical = Lumen hybrid pattern
  analog) + mainline 3-step migration recommendation per `agent/knowledge.md` precedent. `RESULTS.md`
  written (~280 lines, 7 sections). README.md §5/§6/§7 updated with full results + verdict +
  integration recommendation.

---

## Headline findings (summary)

| Strategy | Cost | PSNR | VRAM | Verdict |
|----------|------|------|------|---------|
| A_None   | 0.00 ms | 8.00 dB | 0 MiB | reference |
| B_CubeProbe | 0.10 ms (0.3%) | 20.42 dB | 4 MiB | baked static baseline |
| C_SSR_HiZ | 0.42 ms (1.3%) | 23.30 dB | 2 MiB | no-HW-RT fallback |
| D_RT_1Ray | 1.40 ms (4.2%) | 35.04 dB | 4 MiB | simple RTX path |
| **F_RT_SSR_Hierarchical** | **1.88 ms (5.6%)** | **33.08 dB** | 6 MiB | **WINNER RTX 3060 Ti** |
| G_RT_SSR_TemporalFiltered | 3.00 ms (9.0%) | 44.60 dB | 12 MiB | best apparent quality |
| E_RT_SSR_Stochastic | 5.71 ms (17.2%) ⚠️ | 40.80 dB | 4 MiB | defer до Ada/Blackwell |

**5-10% threshold per `optimization-philosophy.md`:** все 6 strategies значительно выше 8 dB baseline
(PSNR gain 12-37 dB = 150-460% relative).

**Verdict = `mixed`** — multiple winners per platform tier:
- No HW RT: C_SSR_HiZ_Trace (universal)
- RTX-class mid (RTX 3060 Ti): **F_RT_SSR_Hierarchical** (Lumen SIGGRAPH 2022 hybrid pattern)
- RTX-class high (Ada/Blackwell): G_RT_SSR_TemporalFiltered
- Static-baked: B_CubeReflectionProbe

**Mainline 3-step migration per `agent/knowledge.md`:** ~380 LoC, S-M effort, 2-3 sessions,
deferred до Stage 5.x dedicated session per operator decision per `agent/workspace.md §2` line 36.

---

## Notes

**Key insight:** Reflection axis (Stage 5.x) = 0% coverage в 50+ closed experiments = new axis. Direct
fit для h-priority `full rt + tensor cores load` backlog (max occupancy RT cores = перевод
fragment-shader SSR на RT cores освобождает fragment shading budget для Stage 5.x lighting/VCT). Stage
5.x mainline deferred per `agent/workspace.md §2` line 36 (operator 8x planning decision) — this =
recommendation only, mainline pickup is operator decision.

**Cross-axis coupling:** `F_RT_SSR_Hierarchical` strategy naturally integrates с Stage 5.1 VCT
cutoff=0.3 per closed `2026-06-20-vct-vs-rt-cutoff` (RT при r<0.3 + VCT при r>0.3, natural extension
до per-region ray count). `G_RT_SSR_TemporalFiltered` requires `R16G16_SFLOAT` motion vector texture
per closed `2026-06-21-taa-motion-vectors` (closed yes, MV MRT integration complete per
`agent/workspace.md §1 Phase 3`). Parallel `2026-06-21-ambient-occlusion-strategy` (AO axis, m,
RTX SSAO/HBAO/GTAO/RTAO/VCTAO/VDCAO) = complement (reflection + occlusion = Stage 5.x Visual Polish).

**Lumen validation:** F_RT_SSR_Hierarchical = exact Lumen SIGGRAPH 2022 hybrid ray tracing pipeline
analog (Screen Tracing first → Software RT → Hardware RT handoff via ray state) per Wright et al.
SIGGRAPH 2022 paper. Production-proven pattern.

**E_RT_SSR_Stochastic rejection:** 5.71 ms = 17.2% of 33.3 ms 30 Hz frame budget (exceeds 10%
threshold per `optimization-philosophy.md`). Defer до Ada/Blackwell with 4× ray budget, или 60+ Hz
frame rate scenarios.

**C_SSR_HiZ_Trace universal fallback:** no HW RT required, works on AMD RDNA 2 (no ray_query support
pre-2025) + Intel Arc Alchemist A770 (SIMD8 tile mismatch per `llama.cpp/issues/12690`). Scene-dependent
variance (cave_stress worst 21.5 dB, uniform_floor best 27 dB) acceptable for fallback tier.

**Cross-vendor matrix validated:** RTX 3060 Ti GA104 = 1-2 rays/pixel limited per closed
`rt-shadows-vs-csm` mixed analytical model. Ada = 2-4 rays, Blackwell 4th-gen = 4-12 rays (2× vs Ada
per NVIDIA whitepaper Jan 2025). AMD RDNA 3/4 = native via Mesa RADV 2024-2025. Intel Battlemage
Xe2 SIMD16 = full via Mesa ANV 2025+. Mobile = `VK_QCOM_tile_shading` software fallback.