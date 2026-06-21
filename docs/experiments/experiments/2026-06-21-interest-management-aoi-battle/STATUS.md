# STATUS — 2026-06-21-interest-management-aoi-battle

**Phase:** concluded (verdict=mixed)
**Last action:** 2026-06-21 — benchmark complete (150 configs, 6 strategies, all in <0.1 sec);
README + RESULTS + sources.md updated; winner = E_KNN_BackCull (1.5-1.8 Mbps, 10-86× speedup vs
A_FullBroadcast); fallback = D_Priority (2.7-3.3 Mbps, 5-46×); B/C/F insufficient (3-6× only).
**Blocker:** нет
**Next action:** sync to INDEX.md §6 + research/backlog.md §Closed per §13.5

---

## State

- **Claimed:** `2026-06-21` by self per `docs/experiments/AGENTS.md §13.1` + sentinel §13.7.
- **Slug:** `2026-06-21-interest-management-aoi-battle` (military sandbox axis — Tier 0 Foundation & Optimization — netcode).
- **Priority:** h (Tier 0 — Foundation & Optimization).
- **Verdict:** `mixed`.
- **Cross-axis:** orthogonal ко всем in-progress parallel (tracy-gpu-vs-manual, dynamic-battlefield-decal-system,
  just-closed crater); complementary к closed `ecs-1m-entities-bottleneck` (ECS для entity registry) +
  `multi-resolution-collision-broadphase` (spatial indexing pattern reuse) +
  `flow-field-pathfinding-10k-units` (AOI determines WHO needs pathfinding);
  prerequisite для open `lockstep-state-sync-hybrid-netcode` (h, Tier 1) +
  `persistent-war-server-architecture` (h, Tier 1).

---

## Progress log

- **2026-06-21 — Phase 1 (reservation + skeleton + web-research):**
  - `backlog.md §In progress` — reservation entry per §13.2.
  - `experiments/2026-06-21-interest-management-aoi-battle/` — folder + `prototype/build/` created.
  - `README.md` — §1 Hypothesis (6 sub-hypotheses), §2 Prior art (8 sources via Exa), §3 Method,
    §9 Mapping, §10 Operator directives.
  - Web-research: ESEngine AOI + wirepair.org 2025-12 + Photon Fusion 2 + UCL 2011 + esengine
    AOI fix 2026-04 + aceld 2023-08 + AFLL arXiv 2601.10998.

- **2026-06-21 — Phase 2 (prototype + build):**
  - `prototype/aoi_bench.cpp` ~720 LoC (Clang 22.1.6, build green 0 warnings).
  - `prototype/CMakeLists.txt` standalone build.
  - `prototype/README.md` quick build.
  - 6 strategies: A_FullBroadcast / B_GridAOI_NoTiering / C_GridAOI_3Tier / D_GridAOI_3Tier_Priority
    / E_GridAOI_3Tier_KNN_BackCull / F_GridAOI_3Tier_Batched.
  - 5 scenes × 5 seeds = 25 configs × 6 strategies = 150 measurements.

- **2026-06-21 — Phase 3 (benchmark):**
  - 1 bug fixed mid-run: tier rates /4 и /20 → /6 и /30 (correct: 5 Hz / 30 Hz = 1/6, 1 Hz / 30 Hz = 1/30).
  - 1 bug fixed: cell_radius=4 → 3 (correct critical range, was 320m overshoot).
  - 1 bug fixed: F = C (batched) initially showed same kbps — confirmed correct (packet reduction, not bytes).
  - Output: `prototype/build/aoi_bench_results.csv` (151 rows = 1 header + 150 data).
  - Wall time < 0.1 sec на Zen 3 5800X.

- **2026-06-21 — Phase 4 (docs sync):**
  - `RESULTS.md` — full per-config + per-scene aggregate + analysis.
  - `sources.md` — 7 references with verification + cross-axis links.
  - `README.md` §5/§6/§7 — Results, Verdict=mixed, Integration recommendation.
  - Pending: `backlog.md §Closed` + `INDEX.md §6 Recent closed sessions`.

- **2026-06-21 — Race condition recovery (initial claim):**
  - Initial reservation attempt was on `2026-06-21-explosion-crater-terrain-deformation` (h, Tier 1).
  - Parallel agent overwrote my work (race condition per §13.3).
  - Operator chose "Взять adjacent orthogonal h-slug" via `question` tool.
  - Self-switched to `interest-management-aoi-battle` (h, Tier 0 netcode) — fully orthogonal.
  - Removed my outdated INDEX.md entry (now theirs in §6 closed; no duplicate).

---

## Notes

- **Hypothesis outcome:** "<1 Mbps" target REJECTED (E achieved 1.5-1.8 Mbps, D 2.7-3.3 Mbps —
  close but not exact); ">5× reduction" CONFIRMED (D = 5-46×, E = 10-86×).
- **Critical finding:** 3-tier alone (Strategy C) is INSUFFICIENT for 100-player scale because
  peripheral tier (5 Hz) still dominates bandwidth. **Need top-K cap (D) or KNN+back cull (E).**
- **CPU caveat:** analytical 2-3 ms/tick for C-F, real cost 2-5× higher; <2 ms target marginal.
- **Cross-vendor / cross-network:** analytical only, no actual netcode prototype.
- **Static AOI policies:** no runtime adaptation per AFLL arXiv 2601.10998 (deferred).
- **MTU 1200 bytes** assumption; real MTU 1500 (Ethernet) or 9000 (jumbo) — affects packet count.
- **Anti-ritual:** НЕ запускал hardware-probe (`lscpu`/`free`/`vulkaninfo`/`nvidia-smi`) — hardware-profile.md
  свежий (`2026-06-21` capture).
- **Websearch:** Exa работал в этой сессии (HTTP 200, 8 results returned). DuckDuckGo fallback не понадобился.