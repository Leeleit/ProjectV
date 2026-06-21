# STATUS — voxel-hydraulic-erosion

**Phase:** wrap-up
**Last action:** 2026-06-21 — closed, verdict=mixed
**Next tick:** по запросу оператора (Stage 4.1 GPU world gen activation)
**Blocker:** нет

---

## Progress log

- 2026-06-21 — opened, web-research complete (15+ sources)
- 2026-06-21 — standalone C++26 CPU prototype built (260 LoC, Clang 22.1.6, build green 2 warnings)
- 2026-06-21 — 125 main measurements (5 strategies × 5 scenes × 5 seeds)
- 2026-06-21 — closed, verdict=mixed

---

## Notes

- GPU pipe model at 11.7 µs/iter is 40× faster than CPU.
- CPU particle droplet at 3.5 µs/iter is faster than pipe model but produces different erosion character.
- Slope method not applicable at default threshold for procedural terrain.
- Cross-ref: closed `gpu-fluid-ca-atomic-strategy` (shared GPU compute pattern for fluid-like simulation).
