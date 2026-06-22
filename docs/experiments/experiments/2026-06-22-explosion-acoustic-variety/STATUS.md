# STATUS — explosion-acoustic-variety

**Phase:** closed
**Last action:** 2026-06-22 — prototype built + benchmark completed (125k measurements) + results analyzed + verdict=concluded-verdict-mixed
**Next tick:** N/A (closed)
**Blocker:** нет

---

## Progress log

- 2026-06-22 — opened. Sentinel §13.7 clean. Phase 0 init complete (folder, README, STATUS, backlog §Open→§In progress, INDEX §5 entry).
- 2026-06-22 — Phase 1 web research: Cook PhISEM (1996) + Friedlander waveform + Wikipedia explosion references.
- 2026-06-22 — Phase 2 prototype: `explosion_bench.cpp` built green (1 cosmetic warning). 5 strategies × 5 types × 5 distances × 5 seeds × 1000 iter = 125,000 measurements.
- 2026-06-22 — Closed. Verdict=mixed. C recommended default. INDEX + backlog synced.

---

## Notes

Fresh axis: first dedicated per-explosion-type procedural acoustics in 166+ closed experiments. Cross-cuts: closed `ballistic-crack-thump` [mixed] + closed `procedural-voxel-material-audio` [yes] + closed `procedural-engine-sound` [mixed] + closed `explosion-crater-terrain-deformation` [yes] + closed `chunk-damage-fracture-model` [mixed].

**Key results:**
- C_MultiLayerSynthesis: 0.67 µs, -0.50 dB PSNR vs ref, 589-6833 Hz centroid range → RECOMMENDED DEFAULT
- E_AdaptiveHybrid: 1.27 µs (1.9× C), -0.35 dB PSNR → hero-only opt-in
- D_PhysicallyModeled: 0.48 µs, -2.68 dB PSNR, 4-76 Hz centroid → NOT recommended standalone
