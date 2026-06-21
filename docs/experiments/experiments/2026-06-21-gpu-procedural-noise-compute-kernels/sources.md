# Sources — gpu-procedural-noise-compute-kernels

Verified during research session `2026-06-21`. Sources grouped by tier
(primary = direct measurement reference, secondary = production validation,
tertiary = supporting theory).

---

## Primary (verified — direct measurement or implementation reference)

1. **Schneider et al., 2019 — *Implementing Noise with Hash functions for GPUs***
   `https://arxiv.org/abs/1903.12270`
   Perlin/Float 3D = **77 ALU instructions**, Perlin/Integer = 134,
   Perlin/Jenkins = 905, Perlin/Murmur = 257, Perlin/FNV1 = 473.
   Direct baseline для instruction count comparison. Important: shows pure-ALU
   Perlin = 77 inst, texture LUT = 53 inst (per GPU Gems 2 Ch 26).

2. **NVIDIA, 2005 — *GPU Gems 2, Chapter 26: Implementing Improved Perlin Noise***
   `https://developer.nvidia.com/gpugems/gpugems2/part-iii-high-quality-rendering/chapter-26-implementing-improved-perlin-noise`
   Reference textured LUT Perlin = **53 PS2.0 instructions / 9 texture lookups**.
   Pre-Direct3D 10 baseline. Cross-validates Schneider's pure-ALU count.

3. **atywuen/bitangent_noise — `Develop/SimplexNoise.hlsl`**
   `https://github.com/atyuwen/bitangent_noise/blob/main/Develop/SimplexNoise.hlsl`
   Simplex 3D = **~71 instruction slots** (HLSL reference impl).
   Uses PCG3D hash + permutation. Direct port from Gustavson's reference GLSL.
   Comment in code: «Approximately 71 instruction slots used».

4. **KdotJPG/OpenSimplex2 — `glsl/OpenSimplex2.glsl` + `glsl/OpenSimplex2S.glsl`**
   `https://github.com/KdotJPG/OpenSimplex2`
   OpenSimplex2 3D = ~similar cost to Simplex (~71 inst), BCC lattice, no skew transform.
   OpenSimplex2S = 8-point variant (recommended для ridged noise per README).
   License: CC0 (required attribution per §4(a)).
   673 GitHub stars, 8 language ports (Java/C#/Rust/C++/C/HLSL/GLSL/Makefile).
   Last push 2024-01 — actively maintained through 2024.

5. **Auburn/FastNoiseLite — `README.md`**
   `https://github.com/Auburn/FastNoiseLite/blob/master/README.md`
   CPU benchmark table (Intel 7820X @ 4.9 GHz, clang-cl 10 /O2):
   - 3D Value: 64.13 M/s (scalar)
   - 3D Perlin: 47.93 M/s (scalar)
   - 3D Simplex: 36.83 M/s (scalar)
   - 3D Cellular: 12.49 M/s (scalar)
   - 2D variants: ~2× faster than 3D
   - FastNoise2 AVX2: Perlin 261.10 M/s, Simplex 268.44 M/s — 5.5× faster than scalar
   CPU-side data, GPU extrapolation per algorithm complexity.

6. **NVIDIA Nsight Compute Profiling Guide**
   `https://docs.nvidia.com/nsight-compute/ProfilingGuide/index.html`
   Occupancy guidance: «A thread group with 32 threads or fewer will be limited to
   half occupancy. Increasing to 64 threads per CTA will relieve this issue».
   64 threads = sweet spot для pure compute kernels на Ampere / Ada / Blackwell.

7. **JCGT 2022 — Olano, *Modified Noise for Evaluation on Graphics Hardware***
   `https://jcgt.org/published/0011/01/02/paper-lowres.pdf`
   NVIDIA GTX 1660 measurements: 3D noise = «three times longer to compute than 2D»
   per Olano. Modern shader compiler DCE анализ: 17% speedup если disable tiling,
   10-18% если disable rotation. Validates that **compiler optimizations matter
   more than algorithm choice** для modern GPU.

8. **Khronos Forums — *Compute Shader poor write performance***
   `https://community.khronos.org/t/compute-shader-poor-write-performance/105940`
   Confirms imageStore/SSBO write dominates total compute time. Removing write
   puts compute to «doing nothing» speed. Direct validation of memory-bound hypothesis.

---

## Secondary (production reference implementations)

9. **paulrobello/voxel-world — `README.md`**
   `https://github.com/paulrobello/voxel-world` (2026-02)
   Vulkan compute voxel engine, **17 biomes × 5D climate noise × Perlin** для world gen.
   Production reference для compute-shader voxel world gen pattern. Rust 1.94.1+ +
   Vulkan compute + SVT-64 brick distance fields + chunk LOD 5 levels.
   Validates compute-only approach для terrain gen (no CPU-side chunk gen).

10. **arXiv 2505.02017 — Aokana et al., 2025-05**
    `https://arxiv.org/abs/2505.02017` (*A GPU-Driven Voxel Rendering Framework for Open
    World Games*)
    Multiple shallow SVDAGs, chunk selection + tile selection + DAG ray-march in compute.
    Validates compute-only voxel pipeline. Mentions 64-bit vis-buffer (per
    `vis-buffer-for-voxels` README cross-ref).

11. **AdityaGupta1/mega-minecraft — `README.md`**
    `https://github.com/AdityaGupta1/mega-minecraft` (2025-10)
    Minecraft + OptiX 8.0 path tracing + CUDA terrain gen. **5D FBM noise parameters**
    (temperature, humidity, continentality, erosion, weirdness) — direct Stage 4.1
    multi-channel noise pattern reference. Per Machado 2019 "Procedural Generation of
    Volumetric Data for Terrain".

12. **russellocean/pebble-rs — `README.md`**
    `https://github.com/russellocean/pebble-rs` (2025-11)
    WGPU compute voxel raytracer (DDA traversal), Perlin via noise-rs, infinite terrain.
    Validates cross-API (Vulkan/Metal/DX12 via WGPU) compute world gen.

13. **Yunasawa — YNL Vozel Devlog #2.2**
    `https://yunasawa.itch.io/ynl-vozel/devlog/1035890/devlog-22-minecraft-like-biome-generation`
    (2025-09)
    «5 core parameters (Temperature, Humidity, Continentality, Erosion, Strangeness),
    each generated with FBM noise in the range -1 to 1. By combining these parameters,
    every biome is defined by thresholds». Direct reference для Minecraft 1.18+ biome
    generation pattern (relevant для ProjectV Stage 4.1 multi-channel noise).

14. **Yunasawa — YNL Vozel Devlog #2.1**
    `https://yunasawa.itch.io/ynl-vozel/devlog/1035740/devlog-21-procedural-noise-in-action`
    (2025-09)
    «Procedural Noise in Action» — implementation notes для voxel procedural noise.
    «Burst-compatible biome proceducer» (Devlog #2.3) — Burst/Unity DOTS validation.

---

## Tertiary (supporting theory & cross-vendor considerations)

15. **Vulkanised 2024 — Devon McKee, *GPU Atomic Performance Modeling with Microbenchmarks***
    `https://vulkan.org/userpages/09.events/vulkanised-2024/vulkanised-2024-devon-mckee.pdf`
    Atomic RMW cost varies by access pattern, padding, thread distribution. Informative
    для SSBO write contention analysis (но не directly applicable к noise kernel —
    per-voxel evaluations are independent).

16. **CompilerSutra — *Register Pressure on GPU: Why Kernels Fail***
    `https://www.compilersutra.com/docs/compilers/techblog/register-pressure-on-gpu/`
    «If register demand gets too high, the GPU keeps fewer threads resident. Lower
    occupancy makes latency harder to hide. If the compiler cannot keep values in
    registers, it spills them to slower memory». Theoretical framework для analyzing
    noise kernel register pressure (OpenSimplex2 = 4 corner gradients = ~16-20 live
    registers, well below 32-register/wave floor).

17. **StackOverflow — *Register pressure in Compute Shader* (David Kuri)**
    `https://computergraphics.stackexchange.com/questions/4307/register-pressure-in-compute-shader`
    Empirical case study: GCN Fiji ray tracer reduced SGPR 85→64 = +13% perf via
    reduced register pressure. Cross-validation that register pressure matters для
    occupancy-bound kernels.

18. **NVIDIA Developer Blog — *Optimizing GPU Utilization with Nsight Compute 2021.3***
    `https://developer.nvidia.com/blog/optimizing-gpu-utilization-with-nsight-compute-2021-3/`
    Occupancy Calculator methodology + roofline analysis. Reference для follow-up
    Nsight Compute profiling (not done в этом experiment — extension opportunity).

19. **Storage Image and Texel Buffers — Vulkan Documentation Project**
    `https://github.khronos.org/Vulkan-Site/guide/latest/storage_image_and_texel_buffers.html`
    «Prefer Render Passes Over Compute for Image Processing: On tile-based renderers,
    operations within a render pass can often be more efficient than compute shaders
    using storage images.» Note: ProjectV is desktop (Ampere/RDNA = IMR), so SSBO
    preferred over storage image для world gen.

20. **Jimbly/3dnoise-test — `README.md`**
    `https://github.com/Jimbly/3dnoise-test` (2020-09)
    JavaScript performance comparison (1M random samples): simplex-noise-2d 99.4 ms,
    perlin-noise-3d 105.1 ms, simplex-noise-3d 125.2 ms, open-simplex-noise-3d 148.4 ms.
    Cross-validates «Simplex slightly slower than Perlin for 3D» (different hash pattern).
    Note: 1 star, single-author — low confidence tier, included для cross-validation only.

---

## ProjectV (cross-refs)

21. **`TODO.md §4.1`** — GPU Noise & World Gen (target stage).
22. **`src/voxel/VoxelWorld.hpp:85`** — chunkSize = 8 default (workload definition).
23. **`src/voxel/SceneConfig.cpp:78`** — chunkSize = 8 config default.
24. **`src/shaders/voxel_mesh.comp:146`** — chunkSize via push constants (dispatch pattern).
25. **`agent/workspace.md §1 Phase 1`** — world_gen.comp skeleton exists (foundation ready).
26. **`docs/experiments/experiments/2026-06-20-simd-procedural-noise/`** — closed CPU-side
    AVX2 vs scalar orthogonal experiment.
27. **`docs/experiments/experiments/2026-06-20-dec-pipelines-async-compute/`** — async-compute
    foundation (world gen spike isolation).
28. **`docs/experiments/experiments/2026-06-20-nanovdb-on-gpu/`** — GPU voxel SSBO format
    для write target.
29. **`docs/experiments/hardware-profile.md`** §3 (RTX 3060 Ti GA104) + §6 (Clang 22.1.6,
    glslc 2026.2) — dev host baseline.
30. **`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`** — 5-10% threshold
    definition (которую все 5 variants НЕ пересекают = main reason для verdict=mixed).

---

## Verification notes

- All sources accessed via Exa web_search + direct URL fetch в сессии `2026-06-21`.
- GitHub stars / dates verified at time of research (commit hashes not extracted — would
  require archive.org snapshots).
- arXiv preprints verified via ar5iv.labs.arxiv.org HTML rendering (stable URL).
- Cross-vendor data (AMD RDNA, Intel Arc) NOT validated — flagged as limitation §5.5.
- Spectral quality claims = literature-consensus, не измерены в этом experiment (FFT
  framework not built — extension opportunity).
