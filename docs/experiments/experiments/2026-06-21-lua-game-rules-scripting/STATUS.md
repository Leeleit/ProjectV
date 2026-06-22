# 2026-06-21-lua-game-rules-scripting

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** independent (modding infrastructure for Stage 6+ sandbox)
**Estimated effort:** S (1.5h single session)
**Author:** agent (self, per operator instruction «выбирай свободную тему или придумывай свою и исследуй»)

---

## Phase

Phase 4 — closed.

## Last action

`2026-06-21 21:35 UTC` — experiment closed. **Standalone C++26 CPU prototype `prototype/hook_bench.cpp`**
(~870 LoC, Clang 22.1.6, build green **0 warnings**, wall time 0.50 sec).
5 strategies × 5 scenes × 5 seeds × (Add + 1000×Run + Remove) = **375,000 main measurements**
output to `prototype/build/results.csv` (376 rows, 25 KB).

**Verdict: `mixed`** (per strategy; `yes` for the architecture class itself).
- **A (NaiveLinkedList, GMod baseline) = universal recommended default** — production-validated
  by 10+ years of GMod. 39.8-60.1 ns Run across all scenes. 526 ns mean Remove.
- **E (IndexedByEventHash) = equally valid** — 39.8-59.4 ns Run. Slightly better in large_modded.
- **B (ArrayOfHandlers) = REJECTED** — 8265 ns Run for large_modded = 138× slower than A.
  Catastrophic O(N) scan through ALL hooks per dispatch.

**Hypothesis CONFIRMED** — per-tick hook dispatch <0.5 ms target met with **83× headroom**
across all non-B strategies.

## Blocker

None.

## Cross-references

- `README.md` — 8 sections per `_TEMPLATE/README.md`, full hypothesis / method / results
- `RESULTS.md` — human-readable summary tables, per-strategy analysis, caveats
- `sources.md` — 10 verified web sources (Exa 429 → direct canonical URLs fallback)
- `prototype/hook_bench.cpp` (~870 LoC) + `prototype/build/hook_bench` binary +
  `prototype/build/results.csv` (376 rows)
- `agent/knowledge.md Part B §9` — web fallbacks used
- `hardware-profile.md §1` — Zen 3 5800X dev host `obvium`
- `benchmarks/methodology.md §3` — measurement protocol applied
- `_TEMPLATE/README.md` — 8-section format
- Cross-axis: `2026-06-21-programmable-voxels` (closed mixed, deeper multi-runtime survey),
  `2026-06-21-luajit-scripting-hotpath-cost` (closed mixed, raw call cost),
  `2026-06-21-after-action-replay-system` (closed mixed, replay hook events),
  `2026-06-21-lockstep-state-sync-hybrid-netcode` (closed mixed, server-authoritative hooks)

## Integration recommendation

Per README.md §7: defer до Stage 6+ military sandbox activation per `agent/workspace.md §2`
operator 8x planning decision. **Universal recommended default: A_NaiveLinkedList** (GMod pattern),
NOT B_ArrayOfHandlers. New module `src/scripting/HookSystem.{hpp,cpp}` ~750 LoC total (foundation +
LuaJIT binding via sol2 banned on hot path → raw `lua_pushcfunction`, per closed
`2026-06-21-luajit-scripting-hotpath-cost` mixed verdict). Integration tests + Tracy plot. M effort,
2-3 sessions.

## Files in this folder (final)

- `README.md` (8 sections)
- `STATUS.md` (this file)
- `sources.md` (10 verified citations)
- `RESULTS.md` (human-readable summary + per-strategy analysis)
- `prototype/hook_bench.cpp` (~870 LoC)
- `prototype/build/hook_bench` (compiled binary)
- `prototype/build/results.csv` (376 rows, 25 KB)