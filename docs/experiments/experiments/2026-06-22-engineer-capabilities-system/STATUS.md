# STATUS — Engineer Capabilities System (Foxhole-Style Engineer Class)

**Status:** `concluded-verdict-mixed` (per strategy; `yes` for C_Engineer_CooperativeSum ⭐ as universal recommended default + B_Engineer_SingleClaim ⭐ as cost-sensitive fallback)
**Agent:** self
**Started:** 2026-06-22
**Closed:** 2026-06-22 (single session, claim + web-research + prototype + bench + close)

**Phase tracker:**
- [x] Phase 0 — reservation per `AGENTS.md §13.1`: folder created, README + STATUS skeleton, `research/backlog.md` updated, `INDEX.md §5` updated.
- [x] Anti-duplicate sentinel §13.7 clean — `rg "engineer.capabilities|engineer.class|engineer.role|engineer.kit|foxhole.engineer|sapper"` over `INDEX.md` + `experiments/` = only orth cross-refs в `2026-06-22-bridge-building-repair/README.md` mentioning "engineer" as downstream; no dedicated experiment existed pre-claim; `ls experiments/*engineer*` = ENOENT pre-claim.
- [x] Phase 1 — web-research complete via direct `webfetch` to canonical Wikipedia URLs (Exa MCP HTTP 429 + DuckDuckGo CAPTCHA blocked per `agent/knowledge.md Part B §9`); **3 primary Tier 1 sources verified** в [`sources.md`](./sources.md): Wikipedia "Combat engineer" + Wikipedia "Military engineering" + Wikipedia "Sapper" (8-nation-specific usage including Israel palas profession code + France sapeur-pompier + US Sapper Leader Course 28-day + Royal Engineers).
- [x] Phase 2 — prototype `prototype/engineer_capabilities_bench.cpp` (~425 LoC, 5 strategies × 5 scenes × 5 seeds + engineer state machine + cooperative sum + per-op pool + LLM placeholder).
- [x] Phase 3 — build + run + collect results.csv: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` build green **1 cosmetic warning** on unused `kDt` constant; 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall time <5 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Output `prototype/build/results.csv` (29 lines = 3 header + 25 data + 1 sink).
- [x] Phase 4 — write-up: [`RESULTS.md`](./RESULTS.md) (full per-scene breakdown, hypothesis validation, surprising findings, caveats, methodology compliance) + finalize README §5 Results + §6 Verdict + §7 Integration recommendation + §8 Sources.
- [x] Phase 5 — sync per §13.5: backlog.md → §Closed + INDEX.md §6 Recent + this STATUS closure note + backlog_closed.md entry.

**Blocker:** нет (resolved during session).

**Outputs:**
- `prototype/engineer_capabilities_bench.cpp` (425 LoC)
- `prototype/build/engineer_capabilities_bench` (binary, 50 KB)
- `prototype/build/results.csv` (29 lines, 1.5 KB)
- `RESULTS.md` (full synthesis: 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 main measurements, per-scene breakdown, hypothesis validation, surprising findings, caveats, methodology compliance)
- `sources.md` (3 primary Wikipedia sources verified via direct `webfetch` + 13 closed ProjectV cross-references)
- `README.md` (8 sections complete with §5 Results, §6 Verdict, §7 Integration recommendation, §8 Sources)

**Headline numbers:**
- **C_Engineer_CooperativeSum ⭐** validated as universal recommended default: 566 ns/engineer/tick worst case (mega_battle_256e) = 0.17% of 33 ms budget at 100 engineers.
- **B_Engineer_SingleClaim ⭐** validated as cost-sensitive fallback: 424 ns/engineer/tick worst case = 0.13% of budget at 100 engineers.
- D_PerOpPool **REJECTED** (17% overhead at 128-eng scale; over-engineered).
- E_LLMDriven **REJECTED** for per-tick game logic (analytical proxy hides real LLM cost).
- A_PlainWorker baseline works but loses 2-3× speed boost for engineer class.

**Sync (per §13.5):**
- `backlog.md §Open` → `§Closed` (entry claim `[x]` with closure note + reservation record kept for reference).
- `INDEX.md §5 Active` → `§6 Recent closed` (move to closed-sessions table).
- This STATUS.md (closure note + final phase tracker).
- `backlog_closed.md` entry added (cross-ref to README/RESULTS/sources/prototype).