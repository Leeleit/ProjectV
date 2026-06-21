# STATUS — 2026-06-21-wfc-procedural-worlds

**Phase:** concluded-verdict-mixed (closed 2026-06-21)
**Last action:** 2026-06-21 — experiment closed. README.md fully filled (§1-§9). Results CSV appended. INDEX.md §6 + backlog.md §Closed synced per `AGENTS.md §13.5`.

---

## Timeline

- **2026-06-21 (this session):**
  - Reserved slug per `AGENTS.md §13.1` (anti-duplicate sentinel clean).
  - Moved entry `research/backlog.md §Open` → `§In progress`.
  - Updated `INDEX.md §5 Active experiments`.
  - Created `experiments/2026-06-21-wfc-procedural-worlds/{README.md,STATUS.md,prototype/}`.
  - Web-research: 2 batches, 8+ sources верифицированы (Maxim Gumin 2016, arXiv 2308.07307 N-WFC, Chocomunk cuWFC, s-ol gpWFC, Fennec-hub three-wfc, julzerinos WFC brush, basta WFC, RWTH thesis).
  - Implemented `prototype/{wfc.hpp, tilesets.hpp, bench.cpp, CMakeLists.txt, README.md}` (~440 LoC).
  - Built clean on first attempt (Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG).
  - **Initial benchmark timeout issue** (32³ = exponential blow-up) → added `--max-size` flag + per-solve 1 sec deadline.
  - Re-ran with `--max-size 16 --iters 100 --warmup 5` для cave + biome tilesets.
  - **Results:** 8³ OK (220-235 µs, 50% success, coh=0.67); 16³ exponential (11 ms, 0% success).
  - Closed verdict=`mixed` per §6.

---

## Final outcome

- **Verdict:** `mixed` — perf axis marginal на boost / over-budget на powersave; quality axis wins для discrete structure; scalability catastrophic выше 8³ sub-region.
- **Mainline recommendation:** 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1+2 immediate (XS+S, ~180 LoC) для 8³ sub-region + OpenSimplex2 hybrid; Step 3 deferred (M, ~300 LoC) — N-WFC nested pattern per arXiv 2308.07307 для Stage 4.3+ chunks > 8³.
- **Critical caveat:** governor MUST be `performance` для budget; 50% success rate must be raised через better MRV heuristics.

---

## Open questions (deferred)

- **N-WFC nested pattern viability:** measured analytical only; prototype deferred до Stage 4.3+.
- **GPU-side WFC:** deferred indefinitely (Chocomunk 2020 + s-ol 2018 failed attempts).
- **Success rate improvement via MRV:** not measured в этом experiment.

---

## Cross-refs

- `INDEX.md §6` — entry "concluded-verdict-mixed" line для wfc-procedural-worlds.
- `research/backlog.md §Closed` — entry `[x] 2026-06-21-wfc-procedural-worlds`.
- `agent/workspace.md` — no change required (frontier-research results не влияют на mainline session log напрямую).
- `TODO.md §4.1` — optional cross-ref для Step 3 follow-up trigger (Stage 4.3 lift draw distance).
