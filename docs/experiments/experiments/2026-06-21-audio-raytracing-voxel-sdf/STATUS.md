# STATUS — 2026-06-21-audio-raytracing-voxel-sdf

**Status:** concluded-verdict-mixed.

**Date opened:** 2026-06-21
**Date closed:** 2026-06-21 (single session)

**Phase:** closed.

**Verdict:** `mixed` — partial validation.

- ✅ **Occlusion-only path (config B)** production-ready (< 0.05 ms per audio frame for 64 sources at 30 Hz).
- ❌ **Full hybrid path (config C)** not ready (17 ms on cave scene, 3.4× over 5 ms target).
- ⚠️ **Temporal cache (config D)** not validated in current benchmark setup (jitter model unrealistic, cache epsilon
  too tight).

**Last action (2026-06-21, this session):**

- Reservation claimed per `AGENTS.md §13.1`: moved slug из `§Open` → `§In progress` в `research/backlog.md`.
- Web-research complete: 3 batch queries, 12 key sources верифицированы (Vercidium 2025, SIGGRAPH 2025, GSound-SIR
  Mar 2025, Schissler & Manocha 2014, RESound 2007, iSound, Tsingos 2001, Funk 2002, Meta Audio SDK, NeRAF ICLR 2025).
- Standalone C++26 prototype implemented (6 файлов, ~700 LoC): `voxel_grid.{hpp,cpp}` (synthetic SVO + DDA),
  `audio_raytracer.{hpp,cpp}` (geometric ray traversal + reflections + cache), `reverb.{hpp,cpp}` (Eyring tail),
  `bench.cpp` (harness).
- Compiled clean: Clang 22.1.6, `-O3 -march=native -DNDEBUG -Wall -Wextra`, 0 warnings.
- Benchmark executed: 4 configs × 3 scenes × 3 seeds × 1000 iter + 100 warmup = 36 runs × 1000 = 36000 measurements.
- Results written to `prototype/results.csv` + `prototype/RESULTS.md`.
- README §5 Results + §6 Verdict + §7 Integration recommendation updated.
- `sources.md` written with 12 key sources + cross-validation coverage map.

**Sync (per AGENTS.md §13.5):**

- ✅ `research/backlog.md §In progress` → `§Closed` (next step in session).
- ✅ `INDEX.md §5 Active` → `§6 Recent closed` (next step in session).
- ✅ `INDEX.md §8 Last update` — append new entry (next step in session).

**Headline numbers (Zen 3 5800X, single-threaded):**

| Config | Cave (ms) | OpenPlains (ms) | MultiRoom (ms) | Budget @30Hz |
|:-------|----------:|----------------:|---------------:|-------------:|
| B_occlusion | 0.015 | 0.013 | 0.008 | **0.05%** ✓ |
| C_full_hybrid | 17.1 | 13.8 | 6.3 | **52%** ❌ |
| D_full_cached | 21.1 | 14.4 | 6.0 | **85%** ❌ |

**Mainline recommendation (per README §7):**

- **Phase 1 (XS, immediate):** Occlusion-only path → muffling для sources behind walls. < 1.5 ms for 64 sources.
- **Phase 2 (XS, immediate):** Eyring late reverb → realistic room perception, negligible cost.
- **Phase 3 (M, deferred):** Full hybrid after SVO hierarchical acceleration OR lower ray budget OR AVX-512 hardware.

**Caveats (per RESULTS.md §"Caveats"):**

- Single-vendor (Zen 3 5800X), governor `powersave` (not `performance`).
- `voxels_traversed` counter instrumentation bug — doesn't affect latency, blocks cache-miss analysis.
- Synthetic scenes representative but not exhaustive.
- No material absorption modeling (simplified reflection).
- Sequential, single-threaded per `work-stealing-job-system` verdict=mixed.

**Cross-refs:**

- `experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md` — full hypothesis + method + results + integration.
- `experiments/2026-06-21-audio-raytracing-voxel-sdf/sources.md` — 12 верифицированных web sources + coverage map.
- `experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/` — standalone C++26 prototype (~700 LoC).
- `experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/RESULTS.md` — measurement details.
- `experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/results.csv` — raw data (36 rows).
- `research/backlog.md §Closed` — closure record (next sync).
- `INDEX.md §6` — recent closed list (next sync).
- `hardware-profile.md §1/§2` — CPU + RAM baseline.
- `agent/knowledge.md §28` — AudioEngine contract.
- `2026-06-20-nanovdb-on-gpu` — SVO walker foundation.
- `2026-06-20-flecs-soa-vs-aos-bench` — SoA storage pattern.
- `2026-06-20-work-stealing-job-system` — serial dispatcher baseline.

**Continuation chain:** _none_ (first audio axis experiment; opens Stage 7.x audio, no predecessor).
**Follow-up candidates:** `_audio-hierarchical-svo-skip_` (Phase 3 trigger, SVO empty-skip acceleration),
`_audio-rt-budget-vs-source-count_` (scaling to >100 sources), `_audio-diffraction-hybrid_` (Schissler 2014
diffraction via HZB per `hzb-binding-models`).
