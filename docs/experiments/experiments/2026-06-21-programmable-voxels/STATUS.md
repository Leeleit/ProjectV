# STATUS — 2026-06-21-programmable-voxels

**Current phase:** Phase 0-5 done (reservation + web-research + README + analytical model + closure sync).
**Status:** `concluded-verdict-mixed`.

## Timeline

- **2026-06-21** — Reservation per `AGENTS.md §13.1`: `research/backlog.md §Open` → `§In progress`. Anti-duplicate sentinel clean per `AGENTS.md §13.7` (`rg "wasm|programmable.*voxel|script"` over `INDEX.md` + `experiments/` = 0 dedicated experiments). `INDEX.md §5 Active experiments` updated. `README.md` + `STATUS.md` created in `experiments/2026-06-21-programmable-voxels/`.

## Phases

| Phase | Description | Status |
|:------|:------------|:-------|
| 0 | Reservation + INDEX.md + backlog sync | ✅ |
| 1 | Web-research (DuckDuckGo + webfetch, 20+ sources) | ✅ |
| 2 | README.md §1-9 filled | ✅ |
| 3 | Analytical prototype `programmable_voxels_bench.cpp` | ⬜ deferred (analytical only, no deps on dev host) |
| 4 | Build + run prototype | ⬜ blocked (wasmtime/LuaJIT/libtcc not installed) |
| 5 | Close: verdict + sources.md + INDEX.md §6 sync | ⬜ |

## Blocker

`libwasmtime`, `libluajit`, `libtcc` not installed on dev host. CPU-only analytical model from published benchmarks — no real runtime embedding benchmark possible this session.

## Verdict

`mixed` — per-runtime tradeoff matrix: WASM for security-critical mods, LuaJIT for iteration speed, TinyCC for zero-dep developer tools. Right architecture is multi-runtime.

## Closure sync

- [x] `research/backlog.md §In progress` → `§Closed` (updated per §13.5).
- [x] `INDEX.md §5 Active` → `§6 Recent closed` (entry added).
- [x] `README.md` Status: `in-progress` → `concluded-verdict-mixed`.
- [x] `sources.md` complete (30 sources, 3 tiers).
- [x] Prototype: analytical only (no deps on dev host). No build/run possible this session.
