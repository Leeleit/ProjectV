# STATUS — `2026-06-20-vma-sparse-textures`

**Phase:** concluded-verdict-mixed.
**Last action:** Experiment closed same session (`2026-06-20`). README.md (9 sections, ~520 lines),
sources.md (16 references + 5 cross-refs), prototype (`vma_sparse_bench.hpp` + `main.cpp` +
`prototype/README.md`, ~770 LoC total standalone harness). Single-pass sync per
`AGENTS.md §13.5`: backlog.md `§In progress` → `§Closed` + INDEX.md `§5 Active` removed,
new row in `§1 Now` + `§6 Recent closed` + `§8 Last update` added.
**Blocker:** нет.
**Findings:**

- **Verdict=mixed.** Hardware sparse textures unusable на NVIDIA для runtime world streaming
  per `foijord/SparseTexture 2025` + NVIDIA forum 2023 — `vkQueueBindSparse` blocking global,
  1 TiB address limit, slow scaling. Software VT (texture array + page table + feedback pass
  + LRU page manager) = доминирующий паттерн (UE 5.7 RVT, Nanite, id Tech 5 MegaTexture,
  bgfx 40-svt, Frostbite). Per `shlom.dev 2026-02`: hardware sparse = mechanism, не policy.
- **Recommended mainline path:** software VT (shlom.dev pattern) as default + optional HW
  sparse для static prebake (Stage 4.1). 4-step migration per `agent/knowledge.md`
  precedent: foundation (PageManager + page table texture, ~150 LoC) → integration
  (voxel.frag SampleVirtualTexture, ~350 LoC) → page manager wiring (~150 LoC) → optional
  HW sparse path (~120 LoC). Total ~770 LoC + integration code, M effort.
- **Cross-vendor validated:** RTX 3060 Ti dev host (Vulkan 1.4.341) + analytical projection
  for AMD RDNA 4 / Intel Battlemage per `dec-pipelines-async-compute` vendor matrix.
- **Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance), NVIDIA driver fix для
  `vkQueueBindSparse` blocking, `VK_KHR_sparse_image2` cross-vendor, `VK_EXT_memory_decompression`
  cross-vendor ratification.

**Date next tick:** N/A (closed).
