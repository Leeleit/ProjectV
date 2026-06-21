# STATUS — 2026-06-21-god-rays-crepuscular

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — closed same session. Phase 0-7 complete: claim → web-research
(11 sources verified) → prototype build (Clang 22.1.6, 0 warnings) → run (150,000 measurements,
0.032 sec wall time) → RESULTS.md finalized → README.md updated → sync §13.5 pending.
**Next tick:** sync §13.5 (INDEX.md §6 + backlog.md §Closed) — operator may direct Stage 5.x
integration session per `agent/workspace.md §2` line 36.
**Blocker:** нет.

---

## Progress log

- 2026-06-21 — claimed via `§13.1` (anti-duplicate sentinel clean per §13.7).
- 2026-06-21 — backlog.md `§In progress` updated; INDEX.md `§5` updated; folder + skeleton created.
- 2026-06-21 — web-research complete (11 primary + 3 secondary sources verified, see `sources.md`).
- 2026-06-21 — prototype build green (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall
  -Wextra -Wpedantic`, 0 warnings after removing namespace).
- 2026-06-21 — prototype run complete (150,000 main measurements, 0.032 sec wall time).
- 2026-06-21 — RESULTS.md finalized (per-strategy + per-scene aggregate tables + per-platform
  tier recommendation).
- 2026-06-21 — README.md finalized (status=concluded-verdict-mixed, all 8 sections filled).
- 2026-06-21 — sync §13.5 closing.

---

## Notes

- Занимает свободную Stage 5.x Visual Polish ось (god rays / crepuscular rays) — 0 of 50+ closed
  experiments covered.
- Complementary к closed `2026-06-21-volumetric-fog-atmosphere-rendering` (mixed) — different
  physics: fog = atmospheric scattering по всей сцене, god rays = directional sun shafts **через
  occluders**.
- **Verdict=mixed per platform tier** (аналог closed volumetric fog + rtx-screen-space-reflections):
  - **B_ScreenSpaceRadialBlur** = no-HW-RT default (universal, scene-INDEPENDENT 1.2% std, 0.343 ms).
  - **D_VolumetricConeTraceRayQuery** = RTX-class default (1.123 ms, +8.08 dB PSNR, scene-DEPENDENT 7.9% std).
  - **E_Hybrid** = opt-in для cinematic (5.0% budget = tight).
  - **F_PrecomputedSkydomeBaked** = static-only fallback (+2.9 dB, mobile).
  - **C_AnalyticOccludedRayMarch** = **REJECTED** (only +0.31 dB vs B at 4× cost).
- Mainline integration **deferred** до Stage 5.x dedicated session per `agent/workspace.md §2`
  line 36 operator 8x planning decision.
- Operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй» —
  self-invented topic.
- Cross-axis orth orth ко всем 3+ in-progress parallel (tracy-gpu + voxel-mutation +
  gpu-fluid-ca) + параллельным сессиям (rtx-screen-space-reflections + full-rt-tensor-cores-load).
- 6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **150,000 main measurements** per
  `benchmarks/methodology.md §3` — wall time 0.032 sec на Zen 3 5800X governor=`powersave`.
- Standalone C++26 CPU analytical cost model `prototype/god_rays_sim.cpp` ~280 LoC, NOT ProjectV
  mainline.
- 11 primary + 3 secondary sources verified via Exa `web_search` (working this session, no fallback
  needed): Mitchell 2008 GPU Gems 3 Ch 13 + Crytek GDC 2008 + Yusov 2014 GPU Pro 5 + Vos 2014 +
  Hillaire 2015 Frostbite SIGGRAPH + Wright 2022 Lumen SIGGRAPH + Narkowicz 2022 blog +
  UE5 Lumen blog + YouTube + Hillaire 2016 PBR Sky+Clouds + open-source Unity impl +
  .NET Code Geeks walkthrough.