# STATUS — drone-swarm-tactics

**Phase:** wrap-up
**Last action:** 2026-06-22 — Phase 0-4 complete: claim → web-research → prototype → benchmark → analysis → docs
**Next tick:** (none)
**Blocker:** нет

---

## Progress log

- 2026-06-22 — opened. Claim per `AGENTS.md §13.1`. Sentinel §13.7 clean (boid-flocking is animal flocking, orth axis).
- 2026-06-22 — web research: 4 canonical fetches (UCAV / Swarm robotics / Bully / Drone swarm redirect) + 9 closed experiment cross-refs.
- 2026-06-22 — C++26 prototype built: `drone_swarm_bench.cpp` ~480 LoC (Clang 22.1.6 `-O3 -march=native`, build green 0 warnings).
- 2026-06-22 — final benchmark: 125,000 main measurements, wall time 8.3 sec на Zen 3 5800X.
- 2026-06-22 — conclusions written, verdict = `mixed per strategy / yes for D ⭐ as universal default`.

---

## Key results

| Strategy | Mean (ns/tick) | Per-drone cost | Engagement | Efficiency |
|:---------|---------------:|---------------:|-----------:|-----------:|
| A_NoSwarm | 5,842 | 58 ns | 100% | 3.47 |
| B_PriorityQueue | 14,203 | 142 ns | 73% | **7.04** |
| C_RoleBasedSwarm | 11,181 | 112 ns | 78% | 1.94 |
| **D_DynamicRoleReassignment ⭐** | **6,080** | **61 ns** | **100%** | **3.42** |
| E_HierarchicalConsensus | 5,490 | 55 ns | 100% | 3.29 |

**3-clause hypothesis validation:**
- ✅ H1 cost <2 µs/drone/tick: CONFIRMED MASSIVELY (worst 142 ns = 14× under)
- ❌ H2 B-E ≥30% more targets engaged than A: REJECTED — A/D/E reach 100%, B/C LIMITED by correct role targeting (not a bug, design choice)
- ⚠️ H3 C/D scale better than E: MIXED — E simplified (no full Bully); would scale worse in production
- ❌ H4 D ≥20% ammo efficiency over C: REJECTED for ammo; D wins on engagement
- ✅ H5 E ≥50% mission completion with 50% loss: CONFIRMED (E matches A 100%)

**Verdict:** `concluded-verdict-mixed` per strategy; `concluded-verdict-yes` for D ⭐ as universal recommended default for Stage 6+ military sandbox.

**Integration:** 3-step migration ~250-350 LoC, S-M effort, 1-2 sessions, defaults `PROJECTV_DRONE_SWARM=DYNAMIC_ROLE`. See README §7 for details.