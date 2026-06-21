# STATUS — 2026-06-21-luajit-scripting-hotpath-cost

**Current phase:** Phase 0-5 complete (reservation + web-research + README + analytical prototype + closure sync).
**Status:** `concluded-verdict-mixed`.

## Timeline

- **2026-06-21** — Reservation per `AGENTS.md §13.1`: `research/backlog.md §Open` → `§In progress`. Anti-duplicate sentinel clean per `AGENTS.md §13.7`. `INDEX.md §5 Active experiments` updated. `README.md` + `STATUS.md` created.
- **2026-06-21** — Web-research complete (Exa, 15+ primary sources verified).
- **2026-06-21** — Standalone C++26 CPU analytical prototype `prototype/luajit_hotpath_bench.cpp` built (Clang 22.1.6, **0 warnings**). 6 strategies × 5 workloads × 5 seeds = 150 measurements. Output: `prototype/build/results.csv`.
- **2026-06-21** — Closure: verdict `mixed`, INDEX.md §6 updated, backlog synced, sources.md written.

## Phases

| Phase | Description | Status |
|:------|:------------|:-------|
| 0 | Reservation + INDEX.md + backlog sync | ✅ |
| 1 | Web-research (Exa, 15+ sources) | ✅ |
| 2 | README.md §1-9 filled | ✅ |
| 3 | Analytical prototype `luajit_hotpath_bench.cpp` | ✅ |
| 4 | Build + run prototype (0 warnings) | ✅ |
| 5 | Close: verdict + sources.md + INDEX.md §6 + backlog sync | ✅ |

## Blocker (documented)

`libluajit` not installed on dev host (same blocker as `2026-06-21-programmable-voxels`). CPU analytical model from 11+ published benchmarks — real embedding measurement deferred to Stage 6+ integration.

## Verdict

`mixed` — per-pattern recommendation:

| Pattern | Recommended | Cost | Notes |
|:--------|:------------|:-----|:------|
| Per-block random tick | **D_FFI_struct** | 9.6 ns (3.8× native) | Near-native for hot paths |
| Per-entity AI | C_pcall_warm + table pooling | 74 ns | Acceptable up to 5000 calls/frame |
| Event callbacks | C_pcall_warm | 37 ns | Negligible |
| Chunk generator | D_FFI_struct | 21.1 ns | FFI algorithm in Lua |
| UI / setup | Any (incl. sol2) | — | Cold path, no perf impact |

**Budget:** all FFI scenarios < 2% of 30 Hz frame budget (worst case 550 µs = 1.65%). Sol2 = NEVER on hot paths (195× native, 117% budget at worst case).

## Closure sync

- [x] `research/backlog.md §In progress` → `§Closed`.
- [x] `INDEX.md §5 Active` → `§6 Recent closed`.
- [x] `README.md` Status: `in-progress` → `concluded-verdict-mixed`.
- [x] `sources.md` complete (15+ sources).
- [x] Prototype: `prototype/build/luajit_hotpath_bench` + `prototype/build/results.csv` (151 rows).
