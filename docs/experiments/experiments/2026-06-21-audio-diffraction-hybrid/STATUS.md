# 2026-06-21-audio-diffraction-hybrid — STATUS

**Status:** concluded-verdict-mixed (closed `2026-06-21` same session)
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Owner:** self (operator instruction `2026-06-21`)

---

## Current phase

**All phases complete (single session, ~1.5h):**

- [x] **Phase A — infrastructure** (per `AGENTS.md §13.1`):
  - [x] Anti-duplicate sentinel clean per `AGENTS.md §13.7`.
  - [x] `research/backlog.md §In progress` reservation record created.
  - [x] `experiments/2026-06-21-audio-diffraction-hybrid/{README.md, STATUS.md}` created.
  - [x] `INDEX.md §5 Active experiments` entry created.

- [x] **Phase B — web research**:
  - [x] 4 batches, ~30 results, 16 primary + 7 secondary sources verified.
  - [x] `sources.md` written (full bibliography).
  - [x] **Critical correction during verification:** Tsingos 2001 ≠ Tsingos 2007 (UTD vs depth-mip, two different papers by same first author).

- [x] **Phase C — prototype implementation**:
  - [x] `voxel_grid.hpp` (~350 LoC) — synthetic voxel scenes + DDA ray traversal + edge finding.
  - [x] `audio_path.hpp` (~30 LoC) — common interface.
  - [x] `diffraction.hpp` (~180 LoC) — 3 strategies.
  - [x] `bench.cpp` (~150 LoC) — measurement harness per `benchmarks/methodology.md §3`.
  - [x] `Makefile` + `prototype/README.md` + `prototype/RESULTS.md`.

- [x] **Phase D — measurements**:
  - [x] 3 strategies × 3 scenes × 3 seeds × 100 iter × 16 sources = **14,400 invocations**.
  - [x] Output: `results.csv` (28 rows) + `RESULTS.md` (full analysis).
  - [x] Build green, 0 warnings, 0 errors.

- [x] **Phase E — close**:
  - [x] Main `README.md` §5, §6, §7, §8 fully written (Results, Verdict, Integration recommendation, Sources).
  - [x] `INDEX.md §5` → §6 sync per `AGENTS.md §13.5`.
  - [x] `research/backlog.md §In progress` → §Closed sync per `AGENTS.md §13.5`.
  - [x] `STATUS.md` updated to `concluded-verdict-mixed`.

---

## Verdict (one-line)

**`mixed`** — **C_Tsingos production-ready** (0.5-0.6% audio budget, +1.2-1.4 dB recovery per Tsingos 2007 spec); **B_Schissler deferred** (5-16% budget, 0 dB в first-order UTD prototype — second-order required для full +2-4 dB).

---

## Mainline recommendation (one-line)

Integrate **C_Tsingos (Phase 1.5)** immediately as drop-in addition поверх closed `audio-raytracing-voxel-sdf` Phase 1+2. **XS effort** (~150 LoC per `agent/knowledge.md` 3-step migration). Defer **B_Schissler (Phase 1.6)** до second-order UTD implementation (Chandak 2008 / Cao 2021) или Zen 5+ AVX-512 hardware availability.

---

## Cross-refs

- Main [`README.md`](./README.md) — full experiment documentation.
- [`sources.md`](./sources.md) — 16 primary + 7 secondary sources verified.
- [`prototype/README.md`](./prototype/README.md) — build + run instructions.
- [`prototype/RESULTS.md`](./prototype/RESULTS.md) — per-strategy × per-scene × per-seed tables + analysis.
- `prototype/results.csv` (28 rows) — machine-readable measurements.
- `INDEX.md §6 Recent closed sessions` — registry entry.
- `research/backlog.md §Closed` — backlog closure.
