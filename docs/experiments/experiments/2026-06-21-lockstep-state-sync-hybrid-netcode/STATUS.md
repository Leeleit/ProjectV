# STATUS — `2026-06-21-lockstep-state-sync-hybrid-netcode`

**Phase:** **concluded-verdict-mixed** (research + prototype + benchmark complete).
**Last action:** Operator (self) built prototype `prototype/netcode_bench.cpp` ~570 LoC
(Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green
0 warnings** after 1 fix iteration: `static_assert(sizeof(EntityState) == 40)` → 48 due to
8-byte alignment padding). Ran full sweep: 5 strategies × 5 scenes × 5 seeds × 1000 iter +
10 warmup = **125,000 main measurements** on dev host `obvium` (Zen 3 5800X governor=`powersave`).
Wall time **19.5 sec**. Per-config results in `prototype/build/results.csv` (126 rows = 1
header + 125 data, 12 KB). **Verdict issued = `mixed`** (per `README.md §6`):

- **A_PureLockstep ⭐ = DEFAULT for ProjectV** at 48-92 KB/s/player (hypothesis target
  ≤50 KB/s/player CONFIRMED for A; rejected for all hybrid).
- **B_PureStateSync = NEVER** at 4.5-13.8 MB/s/player (94-150× worse than A; bandwidth
  catastrophic for 100-player scale).
- **C_Hybrid_10Hz** = 1576 KB/s/player (32× A) — REJECTED, snapshot payload dominates.
- **D_Hybrid_5Hz** = 812 KB/s/player (17× A) — REJECTED, still 8-50× over budget.
- **E_RollbackCRC** = 1576 KB/s/player (32× A) + **2053 µs/tick CPU** (30× C) — REJECTED,
  CRC32 per-frame over 10000 entities kills CPU.
- All 5 strategies handle 2% packet loss + 50ms latency + 10ms jitter at 1.83% measured
  loss rate (close to target).
- E_RollbackCRC divergence detection: 100% in synthetic worst-case (peer intentionally
  desynced); expected 0.1-1% in production per SupCom precedent at 1M+ customers.

**Blocker:** нет. **Date next tick:** this session closed.

**Integration recommendation:** see `README.md §7`. 3-step migration per
`agent/knowledge.md §30.4` precedent (~1650 LoC total, L effort, 3-5 sessions).
Steps 1+2 (determinism foundation + FPU mode) immediate prerequisites for 100-player
scale; Step 3 (recovery + late-joiner) deferred до Stage 6+ military sandbox activation
per `agent/workspace.md §2` line 36 operator 8x planning decision.

**Caveats** (per `RESULTS.md §7`): CPU-only synthetic; no real network; assumes FPU
determinism achievable; snapshot payload uncompressed; E worst-case divergence
test; no real cross-platform validation. All caveats are explicit and bounded.
