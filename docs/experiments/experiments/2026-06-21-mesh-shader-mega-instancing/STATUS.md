# STATUS — 2026-06-21-mesh-shader-mega-instancing

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — closed same session, integration recommendation written
**Next tick:** Stage 6+ military sandbox activation
**Blocker:** нет (defer to Stage 6+)

---

## Progress log

- 2026-06-21 — открыт; reservation record в `backlog.md` §In progress; sentinel clean per §13.7.
  Topic choice rationale: h-priority Tier 0 Foundation per `backlog.md` military sandbox axis;
  orth ко всем 50+ closed experiments (closed `2026-06-20-mesh-shader-vs-compute-cull` [mixed] = cull strategy,
  **not** mega-instancing 10k+ юнитов; `bindless-descriptor-overhead` [closed mixed] = descriptor strategy;
  `eye-tracked-foveated` [mixed] = per-region fragment density; etc.). Stage 2.1 mesh shader pipeline
  для per-chunk voxel mesh generation **already closed** per `TODO.md §2.1` 2026-06-21 session 5e11993 +
  8x V A — это **отдельная ось** для **animated 10k+ юнитов** (military sandbox = RTT / Supreme Commander
  like army rendering), не для chunk rendering.
- 2026-06-21 — Web-research complete via Exa `web_search` (working this session);
  **15+ primary + 7 supplementary sources verified** (GameDev.net 2024-08-10, XRReady/multi-mesh
  2026-03-29, jglrxavpok 2024-05-13, chaoticbob 2024-01-26, AMD GDC 2024 RDNA 3, Vulkanised 2023,
  nvpro-samples/gl_vk_meshlet_cadscene, NVIDIA Blackwell 2025, DEV.to Michael Sacco 2026-05-13,
  Vulkan Guide, KhronosGroup Vulkan-Samples, Vulkan Validation Layer Issue #9263, VVL PR #4524,
  AMD GPUOpen Meshlet compression, AMD GPUOpen Work Graphs mesh nodes 2024, AMD GPUOpen
  "From vertex shader to mesh shader", Vulkan Foliage 2024, proceduralpixels, ellioman,
  Unity RenderMeshIndirect, eldnach, Themaister Granite).
- 2026-06-21 — Standalone C++26 CPU analytical prototype `prototype/mesh_shader_sim.cpp`
  (5 strategies × 5 scenes × 5 seeds × 1000 iter = **125,000 main measurements**).
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`
  (build green, **0 warnings**). Wall time 0.107 sec на dev host `obvium` Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`.
- 2026-06-21 — Headline: **C_AmplificationShaderOnly = universal winner** (62-544× speedup
  vs A_TraditionalDrawIndexed across 1k-1M instances, well above 5-10% threshold per
  `optimization-philosophy.md`). B_ComputeCull strong 2nd (40-95× speedup but 5-8× more cull
  cost than C). D/E NOT recommended (D 7× only, E animation-broken + 2 GiB VRAM at 1M).
  C at 200k instances ≈ 16 ms = safe within 30 Hz budget. 1M instances at 64 ms = 3.85× over
  budget (defer to Stage 6+ post-MVP scale-up).
- 2026-06-21 — Integration recommendation: 3-step migration ~550 LoC, M effort, gated to
  Stage 6+ military sandbox activation per operator 8x planning decision. Cross-vendor
  matrix portable per Vulkanised 2023 compile-time loop + AMD GDC 2024 wave intrinsics.
