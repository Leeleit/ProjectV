# STATUS — day-night-cycle-celestial-mechanics

**Phase:** wrap-up
**Last action:** 2026-06-22 — prototype built, benchmark run, verdict written
**Next tick:** (none)
**Blocker:** нет

---

## Progress log

- 2026-06-22 — opened. Claim per `AGENTS.md §13.1`. Sentinel §13.7 clean.
- 2026-06-22 — web research: 3 Exa queries, 8 primary sources identified.
- 2026-06-22 — C++26 prototype compiled (0 warnings, Clang 22.1.6 `-O3 -march=native`).
- 2026-06-22 — benchmark run: 125,000 measurements, 126-row CSV.
- 2026-06-22 — conclusions written, documents finalized.

---

## Key results

| Strategy                | Mean latency | vs A (baseline) |
|:------------------------|-------------:|----------------:|
| A_NoCycle               |     20.6 ns  |           1.0× |
| B_SimpleSunAngle        |     57.5 ns  |           2.8× |
| C_FullCelestial         |    315.5 ns  |          15.3× |
| D_CelestialPlusStars    |    578.7 ns  |          28.1× |
| E_PhysicalAttenuation   |    432.9 ns  |          21.0× |

**Verdict:** C is best value (315 ns for full Keplerian sun+moon). B (58 ns) is most CPU-efficient for minimum viable. E (433 ns) adds physical twilight but with scene-dependent quality. D (star field) is too expensive in CPU — should be GPU-only.

**Integration:** 3-step migration B → C → E. See README §7 for details.
