# STATUS — 2026-06-20-vct-vs-rt-cutoff

**Phase:** concluded-verdict-mixed (closed)
**Last action:** 2026-06-20 — Closed. sources.md complete (31 external sources + 16 cross-refs).
backlog.md §In progress → §Closed sync per §13.5. INDEX.md §5 → §6 sync pending.
**Blocker:** нет.
**Next tick:** INDEX.md §5 → §6 sync.
**Verdict (refined):** `mixed` — гипотеза частично подтверждена, но recommended cutoff = **0.3** (не
0.3–0.5 диапазон), потому что analytical cost model + literature сходятся на 0.2–0.4 как реальный sweet
spot. Cross-vendor threshold adjustment (Blackwell → 0.4, RDNA 2 → 0.2) recommended.

---

## Action log

- `2026-06-20` — claim per `docs/experiments/AGENTS.md §13.1`. Anti-duplicate sentinel clean (only
  simd-noise + meshing-algo + async-compute-overhead параллельно, no vct-vs-rt claim).
- `2026-06-20` — Web-research complete: 3 batch queries через Exa, ~30 sources identified (Crassin 2011
  GIVoxels, NVIDIA VXGI, NVIDIA GTC 2012 slides, OGRE 2019 hybrid blog, Lumen SIGGRAPH 2022, Journey to
  Lumen blog 2022, Akenine-Möller JCGT 2021, Wiche & Kuri JCGT 2020, RTXGI 2.0 SDK, RTXDI 3.0 SDK,
  Erlich et al. Eurographics 2024, NVIDIA Blackwell architecture whitepaper, AMD RDNA 4 deep dive,
  Intel Battlemage Xe2, Minecraft RTX 2021, Franke Delta VCT 2014, etc.).
- `2026-06-20` — Analytical cost model drafted: VCT diffuse constant 1.0×, VCT specular grows from 1.0×
  (rough) to 40× (mirror), RTX specular 1.0–8.0× (1-8 rays). Crossover at roughness 0.3 (VCT 2.5× = RTX
  1-ray).
- `2026-06-20` — Cross-vendor HW RT perf matrix compiled: NVIDIA Ampere 4 tri/cycle baseline → Ada
  same + more units → Blackwell 8/cycle (2× gain). AMD RDNA 2/3 1 tri/cycle → RDNA 4 2/cycle (2× gain).
  Intel Alchemist 1 tri/cycle → Battlemage 2/cycle (2× gain). Cross-vendor convergence at RDNA 4 /
  Battlemage.
- `2026-06-20` — Industry validation matrix: OGRE 2019 cutoff 0.02 (precision cliff), Lumen 2022 rejected
  pure VCT, Minecraft RTX 2-LOD via Akenine-Möller math, Aokana 2025 pure SVDAG VCT, ProjectV =
  recommended hybrid.
- `2026-06-20` — README.md drafted (9 sections, all template requirements met per
  `docs/experiments/AGENTS.md §7`).
- `2026-06-20` — backlog.md §Open → §In progress sync per §13.5 (reservation record per §13.2).
- `2026-06-20` — INDEX.md §5 update per §13.5.

## Decision pending

- Mainline review of integration recommendation (Stage 5.1 + 5.2 combined L effort).
- Threshold runtime-tunability decision (`PROJECTV_VCT_RTX_CUTOFF=0.3` env var vs shader constant).
- Cross-vendor threshold table source (NVIDIA/AMD/Intel vendor extensions vs runtime
  `vkGetPhysicalDeviceAccelerationStructurePropertiesKHR` probe).

## Future work (out of scope this experiment)

- `restir-gi-feasibility` (backlog, m, Stage 5.1/5.2) — separate experiment для ReSTIR DI/GI/PT.
- DDGI / SHaRC / NRC integration (post-Stage 5, requires HW RT mature).
- Lumen-style surface cache (out of ProjectV SVO scope per `decisions.md §1.2`).
- ProjectV-specific VCT leak measurement (mainline prototype required).
