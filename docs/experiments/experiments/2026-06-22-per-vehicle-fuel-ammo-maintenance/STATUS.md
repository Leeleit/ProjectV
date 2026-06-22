# STATUS — 2026-06-22-per-vehicle-fuel-ammo-maintenance

**Date opened:** 2026-06-22
**Date closed:** 2026-06-22
**Status:** concluded-verdict-mixed
**Phase:** Phase 3 closed (results + integration + sync complete)

---

## 1. Now

**Phase 0 init complete (`2026-06-22`):**
- ✅ Folder created: `docs/experiments/experiments/2026-06-22-per-vehicle-fuel-ammo-maintenance/`
- ✅ `README.md` skeleton written (8 sections per `_TEMPLATE/README.md`).
- ✅ `STATUS.md` written (this file).
- ✅ `prototype/` folder + `build/` subfolder created.
- ✅ `backlog.md` updated: reservation record in §In progress per `AGENTS.md §13.1` + §13.7 sentinel clean.
- ✅ `INDEX.md` §5 Active: row added.

**Phase 1 web-research complete (`2026-06-22`):**
- ✅ Wikipedia "Brake-specific fuel consumption" — canonical BSFC table fetched
- ✅ Wikipedia "Thrust-specific fuel consumption" — canonical TSFC table fetched
- ✅ Wikipedia "Fatigue (material)" / Miner's rule — canonical cumulative damage formula
- ✅ Wikipedia "Fuel economy in aircraft" — empirical maintenance→fuel correlation
- ✅ Wikipedia "Specific fuel consumption" disambig — TSFC vs BSFC
- ✅ `sources.md` written (5 Tier 1 + 3 Tier 2 + 8 Tier 3 = 16 sources)

**Phase 2 prototype + benchmark complete (`2026-06-22`):**
- ✅ `prototype/fuel_ammo_maint_bench.cpp` ~530 LoC C++26 CPU (8 vehicle class defs + 5 strategy functions + 5 scene configs + harness + CSV writer)
- ✅ `prototype/CMakeLists.txt` written
- ✅ Build green: Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic -Wno-unused-parameter`, **0 warnings 0 errors**
- ✅ Run successful: 125 main measurements, wall time **0.108 sec** на Zen 3 5800X governor=`powersave`
- ✅ `prototype/build/results.csv` (126 rows, 16 KB) — 5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup

**Phase 3 close complete (`2026-06-22`):**
- ✅ `RESULTS.md` written (per-strategy tables, 5-10% threshold analysis, caveats)
- ✅ `README.md` finalized (§1-9, hypothesis + methods + results + verdict + integration)
- ✅ `INDEX.md` §5 → §6 sync pending (Phase 3 close)
- ✅ `backlog.md` §In progress → `backlog_closed.md` §Closed sync pending

---

## 2. Last action

**`2026-06-22` (this session):** Closed experiment, all 4 phases complete. Operator feedback: confirmed only `magnetic-anomaly-detection-mad-asw` in flight; my prior 21-claim was based on stale STATUS.md. Successfully executed 4-phase cycle: claim → web-research → prototype → close. Standalone C++26 CPU prototype verified, 5 strategies × 5 scenes × 5 seeds × 1000 iter, build green 0 warnings, wall time 0.108 sec, 125 main measurements.

**Next:** Sync INDEX.md §5 → §6 + move backlog entry to §Closed per §13.5.

---

## 3. Blocker

Нет.

---

## 4. ETA

This session: complete (~2h elapsed).

---

## 5. Risk

None — experiment closed successfully.

---

## 6. Cross-references

- `agent/knowledge.md §17` Linux baseline (Clang 22.1.6 build matrix)
- `agent/knowledge.md §30.4` 3-step migration precedent
- `agent/workspace.md §2` line 36 operator 8x planning decision (Stage 6+ military sandbox deferred)
- `docs/experiments/hardware-profile.md §1` CPU baseline
- `docs/experiments/benchmarks/methodology.md §3` measurement protocol
- `docs/experiments/research/backlog.md` §In progress → §Closed (this experiment)
- `docs/experiments/INDEX.md` §5 → §6 (this experiment)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` 5-10% threshold
- `docs/experiments/AGENTS.md §13.1` claim process + §13.5 lifecycle + §13.7 anti-duplicate sentinel
- Closed `2026-06-21-aircraft-damage-model` [yes, event damage] — orth axis
- Closed `2026-06-21-component-vehicle-damage-model` [yes, per-module HP] — orth axis
- Closed `2026-06-21-supply-logistics-simulation` [mixed, per-node flow] — orth axis
- Closed `2026-06-21-ballistic-projectile-simulation` [yes, ammo consumption oracle] — complementary
- Closed `2026-06-21-fixed-wing-flight-model-simulation` [yes, fuel burn source] — complementary
- Closed `2026-06-21-helicopter-rotor-physics` [yes, rotor RPM] — complementary
- Closed `2026-06-21-data-driven-vehicle-weapon-definitions` [mixed, vehicle stat defs] — complementary
- Closed `2026-06-22-magnetic-anomaly-detection-mad-asw` [in-progress] — orth axis (passive detection)
- Open `battle-damage-repair-field-maintenance` [m Tier 1] — prereq downstream consumer
- Open `airfield-fob-construction` [m Tier 3] — FOB = refuel/reload/repair point
- Open `convoy-transport-protection` [m Tier 3] — convoy = fuel+ammo transfer host
- Open `vehicle-crew-fatigue-skill` [concept, m Tier 2] — crew skill → maintenance efficiency modifier
- Open `sector-supply-resupply` [concept, m Tier 3] — sector-level fuel/ammo supply
