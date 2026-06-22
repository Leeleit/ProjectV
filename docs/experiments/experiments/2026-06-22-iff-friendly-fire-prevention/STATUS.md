# STATUS — iff-friendly-fire-prevention

**Phase:** wrap-up
**Last action:** 2026-06-22 — Phase 0-4 complete: claim → web-research → prototype → benchmark → analysis → docs
**Next tick:** (none — final experiment of session)
**Blocker:** нет

---

## Progress log

- 2026-06-22 — opened. Claim per `AGENTS.md §13.1`. Sentinel §13.7 clean (cross-refs only).
- 2026-06-22 — web research: 2 canonical Wikipedia fetches (IFF Mark XII + Friendly fire statistics).
- 2026-06-22 — C++26 prototype built: `iff_bench.cpp` ~340 LoC (Clang 22.1.6 `-O3 -march=native`, build green 0 errors, 4 cosmetic warnings about unused params/functions).
- 2026-06-22 — final benchmark: 125,000 main measurements, wall time 4.1 sec на Zen 3 5800X.
- 2026-06-22 — conclusions written, verdict = `mixed per strategy / yes for B ⭐ as universal default`.

---

## Key results

| Strategy | Mean (ns/decision) | Fratricide (mean per scene) | Purity |
|:---------|-------------------:|----------------------------:|-------:|
| A_NoIFF | 414.0 | 100.0 | 31% |
| **B_TransponderOnly ⭐** | **526.7** | **21.6** | **76%** |
| C_VisualOnly | 426.7 | 126.4 | 35% |
| D_ROE_HoldAll | 186.2 | 0.0 | 0% (over-tuned) |
| E_HybridMultimodal | 169.7 | 0.0 | 0% (over-tuned) |

**3-clause hypothesis validation:**
- ✅ H1 cost <5 µs/tick: CONFIRMED MASSIVELY (worst 527 ns = 10× under)
- ⚠️ H2 fratricide reduction ≥80%: MIXED — B = -78% (just under), D/E = -100% but at cost of engagement
- ✅ H3 B fails >30% comm loss: CONFIRMED (76% → 56% purity)
- ❌ H4 D reduces engagement ~20%: REJECTED — D reduces 100% (over-tuned)
- ⚠️ H5 E ≥95% fratricide reduction + <10% engagement loss: PARTIAL — 100% fratricide but >10% engagement loss

**Verdict:** `concluded-verdict-mixed` per strategy; `concluded-verdict-yes` for B ⭐ as universal recommended default for Stage 6+ military sandbox.

**Integration:** 3-step migration ~200-300 LoC, S-M effort, 1-2 sessions, defaults `PROJECTV_IFF=TRANSPONDER`. See README §7 for details.

---

## Session summary

3 experiments closed in this cycle session (operator instruction «автономно в режиме цикла исследуй свободные темы»):

1. `2026-06-22-time-of-day-tactical-gameplay-effects` — Tier 2/4 cross-cut, C ⭐ as default at 878 ns/tick.
2. `2026-06-22-drone-swarm-tactics` — Tier 1/2, D ⭐ as default at 6,080 ns/tick.
3. `2026-06-22-iff-friendly-fire-prevention` — Tier 1/2, B ⭐ as default at 527 ns/decision (FINAL).

Total: **375,000 main measurements**, 3 verdicts=`mixed with strategy-specific yes` (universal-recommended-default pattern), 3 integration recommendations ready for mainline Stage 6+ military sandbox activation.