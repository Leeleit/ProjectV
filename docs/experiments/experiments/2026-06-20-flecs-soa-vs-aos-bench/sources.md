# Sources — flecs-soa-vs-aos-bench

Web-research выполнен `2026-06-20` через Exa per `docs/experiments/AGENTS.md §4`. Все источники верифицированы по
году/автору/контексту перед цитированием в `README.md §2`.

## Primary sources (cited in README §2)

1. **Mertens, Sander — "Building an ECS #3: Storage in Pictures"** (Medium, 2024-09-14)
    - <https://ajmmertens.medium.com/building-an-ecs-storage-in-pictures-642b8bfd6e04>
    - Автор = создатель Flecs. **Direct validation** of our hypothesis: Flecs default = archetype/SoA storage.
    - Diagram shows: archetype table = entity-id array + sorted component-id array + per-component columns.
    - AoS alternative discussed but not recommended for hot loops.

2. **SanderMertens/flecs — "Flecs v4.1 is out!"** (Medium, 2025-06-29)
    - <https://ajmmertens.medium.com/flecs-4-1-is-out-fab4f32e36f6>
    - Performance improvements: get/get_mut 5×, ref_get 5×, cached query iteration 2-4×, pipelines 2×, world creation
      1.4-2.5×.
    - Direct evidence that SoA-based archetype storage scales.

3. **abeimler/ecs_benchmark** (GitHub)
    - <https://github.com/abeimler/ecs_benchmark>
    - Comprehensive cross-ECS benchmark: EnTT, Ginseng, mustache, Flecs, pico_ecs, gaia-ecs (SoA variant).
    - Update 1M entities × 7 systems: Flecs 19ms, gaia-ecs SoA 31ms.
    - Update 2M: Flecs 42ms (better scaling than gaia-ecs SoA 69ms).

4. **SAC 2026 paper (boyang.cs.uwm.edu) — "Performance Evaluation of ECS Architecture in Tower Defense Simulation"**
    - <https://boyang.cs.uwm.edu/publication/sac2026_ECS.pdf>
    - Empirical: SoA-PAR > SoA > AoS > OOP for Tower Defense.
    - Parallel SoA sustains 10× more objects vs OOP.

5. **Uprt Dev — "AoS vs SoA"**
    - <https://uprt.dev/posts/ecs/5/>
    - **Critical caveat**: SoA wins for 1-2 components, AoS/hybrid wins for 5+ components.
    - Our measurement: SoA wins for all 3 workloads (1.44-3.86×) including `cull` (4 fields read).
    - **Reason for partial contradiction**: our fields are larger (vec3=12B, AABB=24B) → AoS cache waste bigger.

6. **Sagar — "C++ Performance: OOP vs Cache-Friendly SoA (ECS Benchmarked)"** (Medium, 2026-04-02)
    - <https://medium.com/@sagar.necindia/optimize-cpp-performance-ecs-soa-vs-aos-02612ef58797>
    - **Quantitative baseline**: ECS (SoA) 5.67× faster than OOP (vtbl) for 1M entities × 100 frames.
    - Cache misses: OOP 29.7% → ECS 2.9% (10× reduction).
    - IPC: 0.71 → 3.12 (4.4× improvement).
    - Scaling: 10K=3.5×, 100K=5.4×, 1M=5.7×, 5M=5.9×, 10M=6.0×.

7. **Bevy PR #14049 — "Opportunistically use dense iteration for archetypal iteration"** (2024-06-27)
    - <https://github.com/bevyengine/bevy/pull/14049>
    - Dense iteration gives ~2× win in specific scenarios, no regression in other tests.
    - Maintainer explanation: dense iteration → compiler auto SIMD optimizations → up to 4× faster.

8. **AMD EPYC 7003 Series Microarchitecture Overview** (Zen 3 reference, official AMD docs)
    - <https://docs.amd.com/api/khub/documents/cdbcpYJAub6P1i3lB2DRJg/content>
    - Zen 3 cache spec: L1d 32 KiB 8-way, L2 512 KiB unified, L3 32 MiB shared per CCX (8 cores), cache line 64 B.
    - Matches `docs/experiments/hardware-profile.md §1` exactly.

## Background sources (not cited in README §2, but used for context)

- **Soren Saket — "Data-Oriented Design: Journey to 1.000.000 Particles Part 2"** (Medium, 2025-01-20)
    - <https://medium.com/low-level/data-oriented-design-journey-to-1-000-000-particles-part-2-cache-2767ac86cb48>
    - Practical walkthrough of AoS → SoA optimization. Cache theory primer.

- **Nomad Game Engine Part 4.3 — AoS vs SoA** (Savas, Medium, 2017-02-28)
    - <https://medium.com/@savas/nomad-game-engine-part-4-3-aos-vs-soa-storage-5bec879aa38c>
    - Early benchmark: AoS wastes cache lines (3 components per 64 B line), SoA = 16 floats per 64 B line.
    - Quantitative: 10,000 entities × health regeneration → AoS 3300 cache lines vs SoA 1250 cache lines (2.6×
      reduction).

- **TUDelft paper — SoA for AST nodes** (TU Delft Repository)
    - <https://repository.tudelft.nl/file/File_5748302e-41df-4214-b489-32eca09e39bf>
    - SoA 5.6× average speedup type-checking phase, 3.5×-7.1× cross-hardware.
    - Confirms SoA advantage generalizes beyond ECS to any data-structure traversal.

- **Astra ECS (T3mps, 2025-07-24)**
    - <https://github.com/T3mps/Astra>
    - Modern C++20 ECS with SIMD (SSE2/SSE4.2/AVX2/NEON auto-detect), 16 KB chunks per archetype, ~1.05 ns/entity
      ForEach.

- **DevelopersIO — "Game Development Data Layout Strategy: Making Updates 3 Times Faster by Switching from Node-Based to
  SoA"** (2026-02-22)
    - <https://dev.classmethod.jp/en/articles/game-dev-soa-data-layout-strategy/>
    - Godot 4.6 benchmark: update 3.3× faster with SoA, overall FPS 1.2× (drawing pipeline bottleneck).

- **arXiv 2512.07841 — "Impact of Data-Oriented and Object-Oriented Design on Performance and Cache Utilization with AI
  Algorithms in Multi-Threaded CPUs"**
    - <https://arxiv.org/html/2512.07841v1>
    - DOD consistently outperforms OOD in cache misses + execution time, especially multi-threaded.

- **Flecs GitHub — Memory Management**
    - <https://deepwiki.com/SanderMertens/flecs/2.6-memory-management>
    - "Tables store entity data using a Structure-of-Arrays (SoA) layout for cache-friendly iteration."

- **Flecs GitHub — Architecture**
    - <https://deepwiki.com/SanderMertens/flecs/1.1-architecture>
    - "Flecs uses an archetype-based storage model where entities with identical component sets are stored together in
      tables. ... Columnar Storage: Components stored in separate arrays (Structure of Arrays pattern)."

- **Tainted Coders — "Bevy Archetypes"**
    - <https://taintedcoders.com/bevy/archetypes>
    - Archetypes in Bevy: entities with similar component composition in contiguous memory.
    - Vectorized operations on contiguous arrays = SIMD-friendly.

- **Tainted Coders — "Bevy ECS"**
    - <https://taintedcoders.com/bevy/ecs>
    - Cache line theory, archetypes explanation, Bevy implementation.

- **Flow Render Engine — "C++ Data-Oriented Design (DOD) vs OOP Benchmark"**
    - <https://www.flowrenderengine.com/cpp-data-oriented-design-vs-oop-benchmark.html>
    - DOD scales almost linearly with entity count, OOP exhibits superlinear growth.
    - SIMD (SSE/AVX) provides up to 5× speedup in DOD layouts.

## ProjectV internal cross-refs (not duplicated, only referenced)

- `agent/knowledge.md` A9 — Voxel storage `std::vector<uint8_t>` (AoS byte-per-voxel) without SoA material
  distribution.
- `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` — SoA vs AoS philosophy, mermaid diagram showing 3-5×
  speedup analytical claim.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — "if perf gain < 5-10%, choose simple"
  measurement-driven decision rule.
- `agent/workspace.md §1` Phase 5 — ECS migration in progress (`VoxelInteractionTickSystem`,
  `BenchmarkAutomationTickSystem`, `LookDevCaptureTickSystem`).
- `agent/workspace.md §1` Phase 6 — more Flecs ECS systems (`FluidCAGpuTickSystem` per pending Stage 3.1 GPU CA).
- `TODO.md §6.1` — Flecs ECS migration (incremental).
- `TODO.md §3.1` — GPU Fluid CA reversal will use ECS bookkeeping.
- `TODO.md §3.2` — Incremental Jolt will use per-chunk body lifecycle in ECS.
- `TODO.md §5.1` — VCT voxelize bookkeeping will be Flecs system.
- `external/flecs/` v4.1.5 — ECS framework in ProjectV, supports chunk-component SoA storage.
- `docs/experiments/hardware-profile.md §1` — Zen 3 5800X cache hierarchy (matches AMD EPYC 7003 docs).
