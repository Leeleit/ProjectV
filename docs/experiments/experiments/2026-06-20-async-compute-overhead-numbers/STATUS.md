# STATUS — 2026-06-20-async-compute-overhead-numbers

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-20 — experiment closed. Verdict `yes` (measured **+9.85% to +11.34% per-frame speedup** on RTX
3060 Ti, depending on run).
**Blocker:** нет.
**Measured numbers:**

- Sequential: 0.771-0.869 ms wall clock / 0.669-0.720 ms GPU total
- Async: 0.695-0.771 ms wall clock / 0.625-0.636 ms GPU total
- **Speedup: 9.85% (run 1) / 11.34% (run 2) — both cross 5% threshold
  from `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by ~2× margin**
- p99 tail latency: -39% (1.917 ms → 1.172 ms)
  **Cross-references:**
  `docs/experiments/experiments/2026-06-20-async-compute-overhead-numbers/{README.md, RESULTS.md, sources.md, prototype/}`.
  **Sync per §13.5:** `backlog.md §In progress → §Closed`, `INDEX.md §5 → §6`, this STATUS.md final.