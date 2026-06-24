# STATUS — 2026-06-21-water-surface-rendering

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session, ~2h)
**Stage link:** independent (cross-cutting Stage 5.x Visual Polish — water surface rendering axis)
**Estimated effort:** S-M (single session achieved)
**Author:** self (per operator instruction «выбирай свободную тему или придумывай свою исследуй» 2026-06-21)

---

## Log

- `2026-06-21` — claimed from backlog per `AGENTS.md §13.1`, moved from `§Open` → `§In progress`. Anti-duplicate sentinel §13.7 confirmed clean: `rg "water|ocean|gerstner|wave.equation|fft.ocean|water.surface"` over `docs/experiments/` returns only cross-references, NO dedicated experiment folder pre-existed.
- `2026-06-21` — Web research complete via DuckDuckGo HTML fallback (Exa `web_search` HTTP 429 persistent per the web_search fallback chain). 15+ primary + secondary sources verified: Tessendorf 2001 + Johanson 2004 + Finch NVIDIA GPU Gems 2 Ch 1 + Timethy Hyman 2026 + WSCG 2025 + deiss/fftocean + Barth Paleologue 2025 + Hanno Malie 2025 + 7 supplementary.
- `2026-06-21` — Standalone C++26 CPU analytical prototype `prototype/water_bench.cpp` 469 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings** after 1 cosmetic fix). 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125 main measurements**, wall time 1.75 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
- `2026-06-21` — Headline (mixed per scene tier): **C_GerstnerWaves = universal default** для non-stormy scenes (0.15 ms total, +3.75 dB mean PSNR over A baseline, well above 5-10% threshold per `optimization-philosophy.md`). **E_ProjectedGridLOD = opt-in** для open-ocean scenes (99.99 dB PSNR within near LOD, 0.65 ms total, high 39 µs CPU cost). **D_FFT_PhillipsSpectrum NOT recommended** for visual water surface (worst PSNR 21.28 dB at 1.7 ms — bilinear interpolation loses high-freq info). **A_FlatStaticMesh sufficient** для calm_lake / voxel_pool (PSNR >30 dB). **B_AnimatedNormalMap_2D strictly dominated** by A (no vertex displacement, identical PSNR, 10× GPU cost).
- `2026-06-21` — Verdict=`mixed` per scene tier: per-scene adaptive dispatcher recommended (A for trivial scenes, C for moderate, E for open-ocean). 3-step mainline migration ~600-800 LoC, deferred до Stage 5.x dedicated session per `agent/workspace.md §2`.
- `2026-06-21` — INDEX.md §6 + backlog.md §Closed synced per `AGENTS.md §13.5`.

---

## Final state

- `prototype/water_bench.cpp` — 469 LoC, **build green 0 warnings**.
- `prototype/water_bench` — compiled binary ~70 KB.
- `prototype/build/results.csv` — 126 rows (1 header + 125 main measurements), 10.5 KB.
- `prototype/build/summary_means.csv` — 26 rows (1 header + 25 strategy×scene means).
- `README.md` — hypothesis + 6 sections + sources.
- `RESULTS.md` — detailed per-strategy × per-scene tables + critical findings.
- `sources.md` — 15+ Tier 1/2/3/4 sources with verbatim quotes + URL + strategy mapping.
- `INDEX.md §6` — entry added.
- `research/backlog.md §Closed` — entry moved from `§In progress`.

**Blocker:** нет.

**Next action:** mainline integration deferred до Stage 5.x dedicated session per `agent/workspace.md §2` operator 8x planning decision.
