# STATUS — 2026-06-21-cloudscape-rendering

## Current phase

**Phase 0 (reservation):** DONE `2026-06-21`
- Anti-duplicate sentinel clean per §13.7
- Claimed per §13.1: backlog.md §Open → §In progress, INDEX.md §5 updated, folder created

**Phase 1 (context & web-research):** DONE `2026-06-21`
- Exa `web_search` working (3 waves, 15+ primary sources verified)
- Key findings: elliahu RTX 3060 clouds = 3.008 ms, RTX 4080 = 0.755 ms; Nubis PS4 = 2 ms; Frostbite = scalable 30-60 FPS
- Sources documented in `sources.md`

**Phase 2 (C++26 CPU prototype):** DONE `2026-06-21`
- `prototype/cloud_sim.cpp` ~180 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green 0 warnings)
- 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = 125,000 measurements

**Phase 3 (measurements):** DONE `2026-06-21`
- `prototype/build/results.csv` (125,001 rows)
- Analysis: B_SingleLayerRayMarch = 2.172 ms / 23.99 dB / 4.20 MiB (recommended default)
- E_RTXRayMarchCloud = 1.769 ms / 27.19 dB (fastest quality, RTX-dependent)

**Phase 4 (results & verdict):** DONE `2026-06-21`
- Verdict: `mixed` per platform tier
- No single winner — B for universal, E for RTX, C for quality opt-in

**Phase 5 (integration recommendation):** DONE `2026-06-21`
- 3-step migration ~430 LoC, M effort, 2-3 sessions
- Default `PROJECTV_CLOUDS=SINGLE_LAYER` (B strategy)
- Scene-adaptive gate `PROJECTV_CLOUDS_MIN_SKY_VISIBILITY=0.15`
- Deferred до Stage 5.x dedicated session

**Close-out sync:** PENDING (INDEX.md §6 + backlog.md §Closed + AGENTS.md §10 DoD check)
