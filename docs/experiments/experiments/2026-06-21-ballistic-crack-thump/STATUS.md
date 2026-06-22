# STATUS — 2026-06-21-ballistic-crack-thump

**Last update:** 2026-06-21
**Phase:** 5 of 5 (closed)
**Blocker:** нет
**Verdict:** `mixed` per strategy; `yes` for the architecture class (audio event generation with physics-based timing)

---

## Phase progress

- [x] **Phase 0: Claim** — `research/backlog.md` updated (§Open → §In progress), sentinel §13.7 clean,
  `INDEX.md §5` updated.
- [x] **Phase 1: Web-research** — 6 Tier 1 sources verified via direct `webfetch` (Exa HTTP 429 + DuckDuckGo
  CAPTCHA blocked per `agent/knowledge.md Part B §9`). Wikipedia "Sonic boom" + "Muzzle blast" + "Doppler
  effect" + "Gunshot" + "Speed of sound" + miniaudio manual. См. `sources.md`.
- [x] **Phase 2: Prototype skeleton** — `prototype/{ballistic_audio_bench.cpp, audio_strategies.hpp,
  scenes.hpp, stats.hpp, CMakeLists.txt}`. ~430 LoC C++26.
- [x] **Phase 3: Build + run** — Clang 22.1.6, `prototype/build/ballistic_audio_bench` (32240 bytes),
  0 warnings. 5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**, wall time <1 sec.
  Output: `results.csv` (8.3 MB, 125,001 rows) + `summary_means.csv` (2.1 KB, 26 rows).
- [x] **Phase 4: Analysis** — `RESULTS.md` + `README.md` updated with full analysis, verdict, integration
  recommendation.
- [x] **Phase 5: Sync** — `backlog.md` §Closed + `INDEX.md` §6 + `backlog_closed.md` + remove from §5.

---

## Verdict per strategy

- **A_NoAudio** = `yes` (baseline null)
- **B_SimpleSample** = **`no`** REJECTED (physically incorrect — delay = 0, both at t=0)
- **C_PhysicsBasedCrackThump** = `yes` ⭐ UNIVERSAL RECOMMENDED DEFAULT
- **D_DopplerShifted** = `yes` (opt-in for higher realism)
- **E_PhysicallyModeledSynthesis** = `yes` (opt-in for high quality, occasional tail outliers)

---

## Notes

- CPU-only analytical prototype (audio = mostly CPU per ProjectV mainline per `agent/workspace.md §1`).
- No Vulkan dispatch (audio = miniaudio backend, not GPU).
- Exa `web_search` HTTP 429 persistent per `agent/knowledge.md Part B §9`; primary = direct `webfetch`.
- DuckDuckGo HTML endpoint CAPTCHA blocked per `agent/knowledge.md Part B §9`; primary = direct `webfetch`
  to canonical sources.
- Dev host: Zen 3 5800X + 8C/16T + governor=`powersave` per `hardware-profile.md §1`.
- Wall-clock budget: <1 sec for 125K measurements (analytical only).
- 6 Tier 1 sources verified via `webfetch` to Wikipedia + miniaudio manual.
- Crack-before-thump verified for rifle_100m: t_crack_ms = -186.7 ms (negative = crack before thump,
  canonical "crack-thump" effect when listener is to the side of trajectory).

---

## Per AGENTS.md §13.5 sync obligations

- [x] `INDEX.md` updated (Active → Recent closed)
- [x] `backlog.md` updated (§In progress → §Closed)
- [x] `backlog_closed.md` updated
- [x] Remove from `INDEX.md §5 Active`
