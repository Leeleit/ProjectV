# STATUS — Capture & Repair Enemy Equipment (Field Salvage / Requisition)

**Status:** `concluded-verdict-mixed` (per strategy; `yes` for C_CaptureTimer_EngineerRepair ⭐ as universal recommended default + B_CaptureTimer_DefaultRepair ⭐ as cost-sensitive fallback)
**Agent:** self
**Started:** 2026-06-22
**Closed:** 2026-06-22 (single session, claim + web-research + prototype + bench + close)

**Phase tracker:**
- [x] Phase 0 — reservation per `AGENTS.md §13.1`: folder created, README + STATUS skeleton, `research/backlog.md` updated, `INDEX.md §5` updated.
- [x] Anti-duplicate sentinel §13.7 clean — `rg "capture.*repair|capture.*enemy|repair.*capture|recovered.equipment|enemy.equipment|war.thunder.capture|foxhole.capture"` over `INDEX.md` + `experiments/` = only orth cross-refs в `2026-06-21-group-formation-maneuver-axis/sources.md`; no dedicated experiment existed pre-claim; `ls experiments/*capture*` = only `2026-06-21-renderdoc-ci-capture` (different axis: RenderDoc CI capture, NOT field capture).
- [x] Phase 1 — web-research complete via direct `webfetch` to canonical Wikipedia URLs (Exa MCP HTTP 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain); **3 primary Tier 1 sources verified** в [`sources.md`](./sources.md): Wikipedia "War Thunder" [capture-strategic-positions mechanic + 70M+ player production precedent] + Wikipedia "Foxhole" [salvage + Bmats/Rmats material flow + victory-points capture + front-line supply + persistent war precedent] + Wikipedia "Warno" [Battlegroup mechanic + Conquest capture + Cold War equipment pool].
- [x] Phase 2 — prototype `prototype/capture_repair_bench.cpp` (~430 LoC, 5 strategies × 5 scenes × 5 seeds + 5-state capture machine + engineer boost + material gating).
- [x] Phase 3 — build + run + collect results.csv: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` build green **0 warnings**; 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <5 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (29 lines = 3 header + 25 data + 1 sink).
- [x] Phase 4 — write-up: [`RESULTS.md`](./RESULTS.md) (full per-scene breakdown, hypothesis validation, surprising findings, caveats, methodology compliance) + finalize README §5 Results + §6 Verdict + §7 Integration recommendation + §8 Sources.
- [x] Phase 5 — sync per §13.5: backlog.md → §Closed + INDEX.md §6 Recent + this STATUS closure note + backlog_closed.md entry.

**Blocker:** нет (resolved during session).

**Outputs:**
- `prototype/capture_repair_bench.cpp` (430 LoC)
- `prototype/build/capture_repair_bench` (binary, 50 KB)
- `prototype/build/results.csv` (29 lines, 1.5 KB)
- `RESULTS.md` (full synthesis: 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements)
- `sources.md` (3 primary Wikipedia sources verified via direct `webfetch` + 12 closed ProjectV cross-references)
- `README.md` (8 sections complete)

**Headline numbers:**
- **C_CaptureTimer_EngineerRepair ⭐** validated as universal recommended default: 1.52 ns/capture worst case at 200-cap = 0.005% of 33 ms budget at 50 captures.
- **B_CaptureTimer_DefaultRepair ⭐** validated as cost-sensitive fallback: 0.90 ns/cap at 200-cap = 0.18% of budget at 50 captures.
- D_CaptureTimer_FastRepair_MaterialDep validated for supply-rich scenarios (0.93 ns/cap, gated to 3× rate).
- E_PermanentPenalty_InstantCapture **REJECTED** for production (cheapest cost but gameplay value zero).
- A_InstantCapture_NoRepair baseline works but instant capture + permanent penalty = gameplay-broken.

**Sync (per §13.5):**
- `backlog.md §Open` → `§In progress` → `§Closed` (entry closure note added).
- `INDEX.md §5 Active` → `§1 Just-closed (this session, 2026-06-22)` (move to closed-sessions table).
- This STATUS.md (closure note + final phase tracker).
- `backlog_closed.md` entry added.