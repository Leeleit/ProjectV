# STATUS — 2026-06-21-soft-body-physics-debris

**Phase:** closed
**Last action:** 2026-06-21 — closed verdict=`yes` (D_XPBD recommended default).
**Next tick:** нет (closed). Re-evaluation triggers в README.md §6.
**Blocker:** нет

---

## Progress log

- 2026-06-21 — §13.1 claim per `AGENTS.md §13.1`. Sentinel §13.7 clean.
- 2026-06-21 — Phase 0 init: `experiments/2026-06-21-soft-body-physics-debris/{README.md, STATUS.md}` + `prototype/build/` структура готова. Selected as **adjacent h-priority** (Tier 1 Physics, military sandbox axis) — fresh axis среди 100+ closed experiments (все closed Physics = rigid body 6-DOF; soft body = orthogonal).
- 2026-06-21 — Phase 1 web-research complete: 3 primary academic (Müller 2007 PBD, Macklin 2016 XPBD, Bouaziz 2014 PD) + 3 production OSS (Pies, XPBD-Cloth-Simulation, imstk) + 13 cross-references в ProjectV closed experiments. 7+ primary sources verified через direct webfetch + DuckDuckGo HTML (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9` line 1424 fallback list).
- 2026-06-21 — Phase 2-3 prototype complete: `prototype/soft_body_debris_bench.cpp` ~750 LoC (5 strategies, 5 scenes, 3 panel sizes, 5 seeds) + `prototype/CMakeLists.txt` (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`). Build green **0 warnings** after 1 fix iteration (Vec3 `operator-=` + `operator/` missing).
- 2026-06-21 — Phase 4 benchmark run: 5 × 5 × 3 × 5 = **375 configs × 1000 iter + 10 warmup = 375,000 main measurements**, wall time **6.18 sec** на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (376 rows = 1 header + 375 data, 36 KB).
- 2026-06-21 — Phase 5 analysis complete: `RESULTS.md` написан. Headline: D_XPBD = universal recommended default (21.77 µs/panel/tick at 64-vert = 1.96% of 30 Hz budget для 30 panels, 63% reduction worst-case stretch vs C_PBD on tearing scenarios).
- 2026-06-21 — **Closed verdict=`yes` same session ~3h.** Mainline 3-step migration ~450 LoC, M effort, 2-3 sessions, **deferred** до Stage 6+ military sandbox activation per `agent/workspace.md §2` line 36 operator 8x planning decision.

---

## Notes

- **Soft body = fresh axis** в 100+ closed experiments; все closed Physics = rigid body 6-DOF.
- **Hypothesis validated:** < 50 µs/panel at 64-vert (измерено 21.77 µs) + 30 panels < 5% of 30 Hz budget (1.96%).
- **D_XPBD win:** 63% reduction worst-case stretch vs C_PBD on tearing_localized. Cross-reference: this is the same compliance term that makes XPBD production-grade in PhysX 4/5 / Unreal Chaos / Pixar Presto.
- **E_ProjectiveDynamics** analytical proxy (no Cholesky global step) — real PD = 5-10× slower per Bouaziz 2014. Not recommended.
- **Borderline:** 30 panels × 64-vert = 1.96% of 30 Hz (above 1.5% ideal but within 5% threshold). For 50+ panel scenarios, recommend D at 64-vert max + LOD1/2 fallback to A_RigidProxy.
- **Deferred to Stage 6+:** GPU compute port (closed `dec-pipelines-async-compute` [yes] provides foundation), self-collision, aerodynamic drag coupling, tear criteria, full Projective Dynamics Cholesky global step.
- **§13.5 sync complete:** `backlog.md` §Open → §In progress → §Closed; `INDEX.md` §5 Active → §6 Recent closed.

---

## Cross-references

- Web sources: см. [sources.md](./sources.md) (3 primary academic + 3 production OSS + 13 ProjectV cross-refs).
- Code: см. [prototype/soft_body_debris_bench.cpp](./prototype/soft_body_debris_bench.cpp) (~750 LoC) + [prototype/CMakeLists.txt](./prototype/CMakeLists.txt) + [prototype/build/results.csv](./prototype/build/results.csv) (375 configs).
- Results: см. [RESULTS.md](./RESULTS.md) для полной таблицы.
