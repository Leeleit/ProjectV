# 2026-06-21-electronic-warfare-jamming — STATUS

**Status:** `concluded-verdict-mixed`
**Phase:** 0 → 6 (closed)

## Phase log

- **Phase 0** ✅ `2026-06-21` ~1 min — claim via §13.1 (slug clean: no parallel `experiments/2026-06-21-electronic-warfare*` per sentinel §13.7; no active `EW|jammer` cross-refs).
- **Phase 1** ✅ `2026-06-21` ~10 min — web-research via `webfetch` (Exa HTTP 429 + DDG CAPTCHA blocked; 6 Tier-1 Wikipedia sources verified: "Electronic warfare" + "Radar jamming and deception" + "DRFM" + "RGPO" + "Krasukha" + "Radio jamming" — retrieved 2026-06-21). Solid J/S equation + DRFM/RGPO mechanics + modern ECCMs (frequency-agile + AESA + LPI).
- **Phase 2** ✅ `2026-06-21` — README.md skeleton (8 sections per `_TEMPLATE/README.md`).
- **Phase 3** ✅ `2026-06-21` — C++26 CPU prototype `ew_bench.cpp` (~430 LoC, build green 0 warnings after 3 fix iterations: chrono header add + `radar_locked` `[[maybe_unused]]` + work-unit `[[maybe_unused]]` + removed unused `linear_to_db`).
- **Phase 4** ✅ `2026-06-21` — build (`clang++ -std=c++26 -O3 -march=native`) + run, **0.27 sec wall time** для 125,000 main + 1,250 warmup. Output `prototype/build/results.csv` (125,001 rows, 9.6 MB) + `prototype/build/summary_means.csv` (26 rows) + `prototype/run.log` (131 lines).
- **Phase 5** ✅ `2026-06-21` — RESULTS.md + sources.md final write-up. Per-(strategy, scene) means computed via Python. Cross-refs to 7 closed experiments.
- **Phase 6** ✅ `2026-06-21` — sync INDEX.md §6 + backlog.md §Closed per §13.5.

## Last action

`2026-06-21` ~10 sec ago — README.md sections 5/6/7/9 filled with closed findings (B/D/E ⭐, C REJECTED for modern radars). Backlog.md moved from §In progress to §Closed via §13.5 sync.

## Blocker

Нет. Experiment closed.

## Final verdict

`mixed` (per-strategy: B/D/E ⭐, C REJECTED, A baseline).
- 5-10% threshold massively crossed (85% radar detection reduction; 2-9% comms denial; 0-3M false targets).
- Wall time 93-910 ns mean = 0.0003-0.0027% of 30 Hz frame budget. Hypothesis CONFIRMED.
- Modern ECCMs (frequency-agile + AESA + LPI) neutralize C_DirectedSpot.
- E_HybridBarrageDeception ⭐ as universal recommended default.

## Next tick

N/A. Experiment closed. Sync to INDEX.md + backlog.md done.
