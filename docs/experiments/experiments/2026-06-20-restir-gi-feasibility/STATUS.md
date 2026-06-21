# STATUS — 2026-06-20-restir-gi-feasibility

**Phase:** concluded-verdict-mixed (analytical + literature complete, prototype deferred per `rt-shadows-vs-csm` precedent)
**Last action:** 2026-06-20 — web-research complete (~30 sources верифицированы), analytical cost model +
cross-vendor matrix + voxel-adaptation matrix, integration recommendation written.
**Next tick:** N/A (closed)
**Blocker:** нет.

---

## Progress log

- 2026-06-20 — claimed via `research/backlog.md` §In progress + scaffold README + STATUS.
- 2026-06-20 — web-research Batch 1 (ReSTIR/DDGI/SHaRC/NRC primary papers + Lumen SIGGRAPH 2022 + RTXGI SDK).
- 2026-06-20 — web-research Batch 2 (RTXGI SDK v2.x, NRC Müller 2021, production adoption Cyberpunk/Portal/Zorah,
  Epic DDGI abandonment forum Dec 2025).
- 2026-06-20 — web-research Batch 3 (ReSTIR PT SIGGRAPH 2022 details, SHaRC integration guide, RTXDI SDK docs,
  Minecraft RTX GDC 2021, voxel DDGI Voxel Devlog #23).
- 2026-06-20 — analytical cost model + cross-vendor matrix + VRAM matrix + voxel-adaptation matrix complete.
- 2026-06-20 — closed verdict=`mixed`. Integration recommendation: keep current hybrid VCT+RTX, plan deferred
  path tracer pivot post-Stage 5/6.

---

## Notes

- **Architectural mismatch is the headline finding.** SOTA GI techniques (ReSTIR PT, DDGI, SHaRC, NRC) все
  требуют path tracer foundation. ProjectV's Stage 5.x = hybrid VCT+RTX, НЕ path tracer. Direct integration
  невозможен без major refactor.
- **Re-evaluation deferred до Stage 6+ (post-MVP)** if mainline commits to path tracer architecture.
  Recommended add-on order: SHaRC → DDGI → ReSTIR DI/GI/PT. NRC = skip (NVIDIA-only).
- **VRAM budget:** SHaRC alone = 185 MB = 3.65% of 5.06 GiB budget per `hardware-profile.md` §3. Acceptable
  for post-Stage 5 path. RTX 3060 Ti driver 610.43.02 ≥ RTXGI 2.x requirement 555.85.
- **Voxel-adaptation verified:** DDGI production-ready in voxel engines (Douglas Voxel Devlog #23 Jun 2025,
  Minecraft RTX). SHaRC = path-tracer-agnostic. ReSTIR = requires path tracer + voxel RT wrapper.
- **Cross-vendor:** SHaRC = universal (RTXGI 2.x Vulkan path on AMD RDNA 4 + Intel Battlemage via
  `VK_KHR_ray_query` + compute). NRC = NVIDIA-only (Tensor Cores ≥ Turing, excludes AMD + Intel).
- **Quality validated** для path-tracing contexts (ReSTIR PT MAPE 0.39 vs 1.63 naive PT, Cyberpunk production,
  SHaRC 1.5-10% overhead). **Cannot translate to ProjectV current Stage 5.x без path tracer.**