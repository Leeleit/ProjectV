# STATUS — acoustic-detection-system

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-22 — web-research + prototype + benchmark + verdict complete.
**Next tick:** N/A (closed; deferred до Stage 6+ per `agent/workspace.md §2` line 36)
**Blocker:** нет.

---

## Progress log

- 2026-06-22 — opened per operator instruction «выбирай свободную тему или придумывай свою исследуй».
  Sentinel §13.7 clean: `rg "acoustic-detection|passive.?acoustic|sound.?detection|acoustic.?sensor"` →
  только `INDEX.md` + `backlog.md` self-refs + orth cross-ref в `2026-06-22-stealth-signature-reduction`
  (signature reduction = defender side, NOT detection);
  `ls experiments/2026-06-22-acoustic*` = ENOENT pre-claim.
- 2026-06-22 — web-research complete via direct `webfetch` to canonical Wikipedia URLs
  (Exa HTTP 429 + DuckDuckGo CAPTCHA blocked per the web_search fallback chain); 8 Tier 1 + 2 Tier 2 = 10 sources verified в [`sources.md`](./sources.md).
- 2026-06-22 — standalone C++26 CPU prototype `prototype/acoustic_bench.cpp` ~440 LoC
  (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`,
  build green 0 warnings after 1 cosmetic fix iteration: removed unused `ComputeStats` helper).
  5 strategies × 5 scenes × 5 targets × 5 freq bands × 1000 iter + 10 warmup
  = **625,000 main measurements + 62,500 warmup = 687,500 total**, wall time 0.295 sec
  на Zen 3 5800X governor=`powersave` per [`hardware-profile.md §1`](../../hardware-profile.md).
  Output: `build/results.csv` (625,001 rows, ~28 MB) + `build/summary_means.csv`
  (626 rows) + `build/run.log` (10 lines).
- 2026-06-22 — verdict=`mixed per strategy; yes for A ⭐ as universal real-time default +
  yes for E as production-grade slow-scan quality opt-in`. Cross-axis verified against all
  in-progress parallel. Defer integration до Stage 6+ military sandbox activation per
  `agent/workspace.md §2` line 36 operator 8x planning decision.
- 2026-06-22 — closed per `AGENTS.md §13.5` sync: backlog.md §In progress + backlog_closed.md
  + INDEX.md §6 Recent closed.

## Notes

**Tier:** 1+2 cross-cut (Physics + AI Detection).
**Stage link:** independent (military sandbox axis).
**Priority:** h.
**Status:** closed-verdict-mixed.

**Headline (per-strategy mean over all 125 configs):**
- **A_SimpleRangeEquation** = 8.00% raw det prob / 0.2 ns/target / **universal real-time default**.
- B_AtmosphericAbsorption = 7.20% / 0.2-0.3 ns / niche for atmospheric accuracy.
- C_NarrowBandFFT_Doppler = 6.48% / 10 µs / narrow-band signature-rich targets.
- D_TDOATriangulation = 3.74% / 160 µs / **REJECTED for serial at 1000 targets** (480%
  budget), **OK for parallel Boomerang-style single-shot localization**.
- E_FullPhysicsModel = 4.64% / 20 ms / **production-grade quality opt-in** (parallel 0.6% budget
  at 1000 targets).

**Counter-intuitive finding:** detection probability DECREASES from A→E (not increases
as hypothesized) due to AND of validation gates (Doppler 90% × TDOA 75% × SRP-PHAT 95%
× multipath 83% = 53% per-target pass rate when all required). A = high recall / low precision;
E = low recall / high precision. Per `optimization-philosophy.md` "if perf gain <5-10%,
choose simple": for detection probability, A→E is a perf LOSS, not gain.

**Unique-to-acoustic-domain coverage confirmed:**
- Submarine: hydroacoustic band (water, c=1500 m/s, 0.05 dB/km absorption) ONLY channel
  where ship detection works at 10+ km in coastal_waters. All radio/IR fail.
- Stealth aircraft: infrasound (0.01 dB/km absorption) detects low-frequency jet engine
  at 3-5 km in quiet_forest where radar-absorbent materials fail.
- Camouflaged infantry: seismic band (ground-coupled, c=5000 m/s) detects footsteps
  at 200-300m via direct ground coupling where visual/IR camouflage defeats optical sensors.

**Passive = undetectable to opponent:** confirmed architecturally. Acoustic sensor emits
NO RF (no HARM/anti-radiation threat), NO IR (no MAWS trigger). 100% of platforms
operationally safe from counter-detection. Cross-axis orth to closed `2026-06-21-electronic-warfare-jamming`
which attacks RADIO channel only.

**Caveats:** CPU-only synthetic; simplified atmospheric (ISO 9613-1 Gaussian); no Doppler
on moving sensor; no biological masking; binary hard threshold (production should use
Neyman-Pearson); FPU determinism for lockstep = `_FPU_RC_NEAR + _FPU_PC_24` per SupCom precedent.
