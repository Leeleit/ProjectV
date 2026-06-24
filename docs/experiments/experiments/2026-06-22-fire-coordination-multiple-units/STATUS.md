# 2026-06-22-fire-coordination-multiple-units — STATUS

**Status:** `concluded-verdict-mixed`
**Date opened:** 2026-06-22
**Date closed:** 2026-06-22 (single session, ~4h)

---

## Phase

**All 4 phases complete** (claim + research + prototype + benchmark + verdict).

---

## Last action

- ✅ Claim в `backlog.md` (2026-06-22) per §13.1 + reservation record per §13.2.
- ✅ Anti-duplicate sentinel §13.7 verified.
- ✅ `experiments/2026-06-22-fire-coordination-multiple-units/` папка + README + STATUS + sources + prototype.
- ✅ Web-research: 7 Tier 1 + 1 Tier 2 sources verified via direct `webfetch` to Wikipedia canonical URLs (Exa 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain).
- ✅ Standalone C++26 CPU prototype `prototype/fire_coord_bench.cpp` (~430 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 0 errors / 3 cosmetic warnings).
- ✅ Benchmark complete: 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time ~5-7 min на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`.
- ✅ Output: `prototype/build/results.csv` (126 rows = 1 header + 125 data).
- ✅ Verdict: `mixed per strategy; yes for B_PriorityScoreWeighted ⭐ as recommended default for balanced forces (80% win vs A 60% = +33% relative on balanced_10v10 scene)`.
- ✅ Integration recommendation: 3-step mainline migration per `agent/knowledge.md` precedent, ~530 LoC, S-M effort, **deferred до Stage 6+ military sandbox activation** per `agent/workspace.md §2` line 36 operator 8x planning decision.

---

## Final headline

**H1 (CPU budget <0.1 µs/unit/tick): CONFIRMED MASSIVELY** — все 5 стратегий <50 ns/unit/tick (budget 100 ns).

**H2 (MTTK reduction ≥30%): REJECTED** — A fastest MTTK (17.54s), B/C/D/E slightly slower (+1%, saturated at max_ticks).

**H3 (DPS efficiency ≥40%): REJECTED** — within 4% (0.363-0.376 mean).

**Per-scene win rate** (key differentiator):
- `balanced_10v10`: B/D = **80%**, A/C/E = 60% → **+20pp = +33% relative** (crosses 5-10% threshold per `optimization-philosophy.md`).
- Other scenes: saturated (all 100% or all 0% based on force ratio).

---

## Blocker

Нет.

---

## Hardware baseline

Per `hardware-profile.md §1` (Zen 3 5800X 8C/16T, governor=`powersave`) — данные актуальны на `2026-06-21`, **probe не запускаю** per §14 STOP-блок.

---

## Sync (per §13.5)

Next: обновить `INDEX.md §6 Recent closed` + `backlog.md` §Closed (move from §In progress).

---

## Cross-refs

- [README](./README.md) — full experiment description.
- [sources.md](./sources.md) — verified references.
- [prototype/](./prototype/) — C++26 standalone CPU prototype + build/.
- [`backlog.md`](../../research/backlog.md) §In progress — reservation record (to be moved to §Closed).
- [`INDEX.md`](../../INDEX.md) §5 Active (to be moved to §6 Recent closed).