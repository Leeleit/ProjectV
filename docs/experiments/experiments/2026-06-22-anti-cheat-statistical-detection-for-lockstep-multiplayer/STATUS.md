# STATUS — 2026-06-22-anti-cheat-statistical-detection-for-lockstep-multiplayer

**Phase:** closed (concluded-verdict-mixed)
**Last action:** 2026-06-22 — single-session close (~2.5h total). Build green, results.csv populated, README + RESULTS + sources.md finalized.
**Next tick:** нет (closed). Defer mainline integration до Stage 6+ military sandbox activation per `agent/workspace.md §2`.
**Blocker:** нет.

---

## Progress log

- 2026-06-22 — opened. Self-invented per operator instruction «выбирай свободную тему или придумывай свою исследуй».
- 2026-06-22 — §13.7 sentinel verified clean. Claim filed in `research/backlog.md §In progress` + `INDEX.md §5 Active`.
- 2026-06-22 — Phase 1: web research complete (9 sources verified via direct `webfetch` to Wikipedia + arXiv; Exa 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain).
- 2026-06-22 — Phase 2: `sources.md` written (Tier 1: 5 academic, Tier 2: 3 production, Tier 3: 1 ProjectV cross-ref).
- 2026-06-22 — Phase 3: `prototype/anticheat_bench.cpp` written (~600 LoC, standalone C++26 CPU harness with 5 strategies × 5 scenes × 5 seeds = 125 measurements).
- 2026-06-22 — Phase 4: build green 0 warnings 0 errors (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`); full benchmark run, `build/results.csv` 126 rows, wall time ~100 sec.
- 2026-06-22 — Phase 5: `RESULTS.md` written with per-strategy × per-scene breakdown + per-strategy aggregate + analysis + caveats.
- 2026-06-22 — Phase 6: README hypothesis + results + verdict + integration recommendation written. §Closed sync in progress.

---

## Notes

- **Verdict:** `mixed` per strategy / `no` for the strict hypothesis (no single strategy meets 85% TPR + 1% FPR).
- **Architecture verdict:** `yes` (server-side statistical detection in lockstep multiplayer is architecturally sound even though single-strategy targets not met).
- **Recommended production default:** `BD` hybrid (B + D) = up to 40% TPR, ≤5% FPR.
- **Headline finding:** Replay-based detection (D) is the **strongest** signal against adversarial cheaters (70% TPR on S5) because deterministic state divergence can't be faked; CUSUM-based detection (C) catastrophically fails on autocorrelated data (100% FPR) per Wikipedia "Statistical process control" §"Mathematics of control charts" warning.
- **Cross-axis:** orth ко всем ~16 in-progress parallel; complementary к closed `lockstep-state-sync-hybrid-netcode` [mixed] + `persistent-war-server-architecture` [yes, E_Hybrid_ShardedReactive ⭐] + `after-action-replay-system` [mixed] + `multi-resolution-collision-broadphase` [mixed] + `ecs-1m-entities-bottleneck` [yes] + Tier 2 gameplay cheat surfaces (ballistic + radar + vehicle damage + factory production).
- **Web-research limitations:** Exa 429 + DuckDuckGo CAPTCHA blocked this session per the web_search fallback chain; 9 sources verified via direct `webfetch` to canonical URLs (Wikipedia + arXiv); Tier 1: 5, Tier 2: 3, Tier 3: 1 entry spanning 5 closed ProjectV experiments.
- **Single-session methodology:** claim + web-research + prototype + build + bench + analysis + close = ~2.5h.
- **Deferral note:** mainline integration deferred до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision; B + D hybrid (recommended default) is ready-to-implement when Stage 6 starts.
- **Follow-up experiment candidates (not claimed, informational):**
  1. **ML training data collection** — collect labeled (cheater vs legit) player telemetry from a live dev-server; needed for E strategy to be useful.
  2. **CUSUM detrending engineering** — implement proper detrend + reset logic for C; potentially useful for slow-drift cheats.
  3. **Cross-server cheat detection** — aggregate baseline statistics across multiple realms (per-player cheat score normalized by fleet distribution).
  4. **Client-side hash validation** — for DMA-card hardware cheats (out of scope for server-side, requires client-side integrity check).
