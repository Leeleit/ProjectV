# STATUS — 2026-06-22-irst-thermal-imaging-detection

**Phase:** closed
**Last action:** 2026-06-22 — Phase 4 (writeup + close)
**Next tick:** N/A (concluded)
**Blocker:** нет

---

## Progress log

- **2026-06-22 — Phase 0 (claim):** §13.7 sentinel clean. Created `experiments/2026-06-22-irst-thermal-imaging-detection/{README.md,STATUS.md,prototype/}` + added to `backlog.md §In progress` + `INDEX.md §5 Active`.
- **2026-06-22 — Phase 1 (web-research):** Direct `webfetch` to 4 Wikipedia primary + 2 cross-refs (Exa 429 + DuckDuckGo CAPTCHA blocked). 6 sources verified, см. `sources.md`.
- **2026-06-22 — Phase 2 (prototype):** `prototype/irst_bench.cpp` 585 LoC C++26 CPU. Build green 0 warnings 0 errors on first attempt (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`).
- **2026-06-22 — Phase 3 (benchmark):** 5 strategies × 5 scenes × 5 seeds × 1010 iter × view_count = **7,025,000 main measurements** + warmup. Wall time 2.34 sec. Bit-exact reproducible across runs.
- **2026-06-22 — Phase 4 (writeup + close):** `RESULTS.md` + `README.md` finalized. `backlog.md §In progress` → `§Closed` sync (per §13.5). `INDEX.md §5 Active` → `§6 Recent closed` row added.

**Final verdict:** `mixed` per strategy, `yes` for the architecture class (IRST/FLIR detection as third
detection axis orthogonal to radar). Per-strategy: A=NO (unrealistic), B=mixed, **C=YES ⭐ universal default**,
D=mixed, **E=YES ⭐ high-fidelity opt-in**.

**3-clause hypothesis validation:**
- ✅ H1 cost: E = 224 ns/target × 1000 = 0.224 ms/frame = 0.67% of 30 Hz budget. 1700× headroom.
- ❌ H2 fidelity ladder REJECTED: detection rate does NOT monotonically increase A→E. A is unrealistically
  optimistic (1.00); C-E give realistic detection (0.20-1.00) with failure modes. "More physics = more realistic
  failure modes" (the price of truth).
- ✅ H3 passive stealth: IRST undetectable by RWR; net tactical value positive in sensor-fusion pipeline.

---

## Notes

- **Self-invented topic per operator instruction `2026-06-22`** «выбирай свободную тему или придумывай свою исследуй».
- **First dedicated passive IRST/FLIR thermal-imaging detection axis в 140+ closed experiments.**
- **Cross-axis orth** to all 3 in-progress parallel (`medical-evacuation-chain` Tier 2 AI + `surface-micro-detail`
  Stage 5.x polish + `indirect-fire-artillery-fdc` Tier 1 Phys+2 AI).
- **Cross-axis complementary** to closed `radar-detection-system-simulation` [yes, radio sibling] +
  `stealth-signature-reduction` [yes, IR signature source] + `electronic-warfare-jamming` [mixed, comms denial] +
  `aircraft-damage-model` [yes, IR signature post-damage] + `component-vehicle-damage-model` [yes, IR signature
  per-component] + `ballistic-projectile-simulation` [yes, projectile launch IR] + `fixed-wing-flight-model-simulation`
  [yes, afterburner IR] + `helicopter-rotor-physics` [yes, exhaust IR] + `recon-intel-fog-of-war` [yes, intel fusion input].
- **Key insight:** "More physics ≠ more detections" — the cost of physical realism is more failure modes (clutter
  masking, sun glint, atmospheric extinction). A is the "optimistic lie" (1.0 always), E is the "honest truth"
  (0.20-0.90 with confidence interval). The right answer is C (best cost/accuracy balance) by default, E for
  high-fidelity.
- **Integration recommendation:** 3-step ~730 LoC, S-M effort, per `agent/knowledge.md` precedent. Defer to
  dedicated session per `agent/workspace.md §2` operator 8x planning decision. Default `PROJECTV_IRST_STRATEGY=C`,
  opt-in `E`.
- **Caveat:** CPU-only analytical model with 2-band LOWTRAN approximation. Real mainline integration would need
  MODTRAN per-band lookup + real FLIR vendor NETD specs + tracking/association.
