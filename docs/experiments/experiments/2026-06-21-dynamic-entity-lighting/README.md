# 2026-06-21-dynamic-entity-lighting — Dynamic Entity Lighting

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** TODO.md §5.x (Visual Polish)
**Estimated effort:** S (250-350 LoC, 1-2 sessions)
**Author:** agents (self)

---

## 1. Hypothesis

Динамический свет от энтити (игрок с факелом/glowstone, мобы с light-emitting айтемами) для ProjectV.

**Hypothesis:** CPU-cost of dynamic entity lighting scales O(N_entities); on RTX 3060 Ti <5 dynamic sources stay within 1% frame budget; smooth interpolation on entity movement costs ~0.01 ms/light; incremental BFS from closed `incremental-light-propagation` directly applicable.

**Альтернативы:**
- OptiFine DynamicLights: per-tick lightmap injection в существующий lightmap buffer + chunk rebuild
- LambDynamicLights: lightmap coordinate injection via mixin + distance-based falloff (range 7.75 blocks)
- Shader-based (GPU injection): entity data → SSBO, GPU computes per-pixel falloff

---

## 2. Prior art

Web-research complete (3 Exa searches, 15+ sources verified):

- **OptiFine DynamicLights** (sp614x, 2012-2026) — Original implementation: `DynamicLights.java` + `DynamicLight.java`.
  Entity-as-light-source: player items, dropped items, mobs (blaze L10, magma cube L8/L13). Range = 7.5 blocks
  (MAX_DIST_SQ = 56.25). Fast mode: rate-limited to 500ms updates. Chunk rebuild bottleneck.

- **LambDynamicLights** (LambdAurora, 2018-2026) — Modern alternative with 3 methods:
  - Lightmap coordinate injection (TAIL → `getLightmapCoordinates`)
  - Chunk light provider injection
  - Shader-based (best perf)
  Light formula: `luminance * (1 - sqrt(distance) / range)`, range = 7.75 blocks. Rate-limited to 50ms updates.
  **Key insight:** chunk rebuilding is the critical performance bottleneck, not BFS CPU time.

- **Starlight** (PaperMC, 2021-2024) — Rewritten vanilla light engine: 12× fewer updates for increases,
  44× fewer for decreases. Uses `Long2ByteMap` instead of `NibbleArray`.

- **Minecraft light engine** — `LightEngine.java`: BFS propagation with `enqueueIncrease`/`enqueueDecrease`,
  `doLightUpdates(int budget)`. `ThreadedLevelLightEngine` adds task-based scheduling.
  `ChunkLightProvider.addLightSource(BlockPos, int level)`.

- **Other mods:** Lumi (2.5× perf vs vanilla), Dynamic Lights datapack (server-side light blocks).

---

## 3. Method

- **Тип:** Standalone C++26 CPU prototype + benchmark.
- **Сцена:** 5 synthetic voxel scenes (uniform_floor, forest_floor, cave_stress, mixed_biome, uniform_air)
  на grid 32×32×32 (8×4×8 chunks, chunkSize=8). 5 seeds (1, 7, 42, 1234, 31337).
- **Entity counts:** 1, 3, 5, 10, 20 (simulated continuous movement via sinusoidal path).
- **Стратегии:**
  - **A_None:** No dynamic lights — baseline.
  - **B_FullBFS:** Full BFS propagation from all entity positions every frame — PSNR reference.
  - **C_BudgetBFS:** Budget-limited BFS (queue entry budget = 2048) — incremental-light-propagation method.
  - **D_RateLimited:** Full BFS every 3rd frame (throttled to ~33% frequency) — OptiFine/Lamb style.
  - **E_GPUInjection:** Shader-based — CPU only writes entity data to SSBO (simulated memcpy),
    GPU does per-pixel distance-falloff (not timed in CPU measurement).
- **Метрики:** wall_time_us (CPU cost), PSNR vs B_FullBFS as reference, chunk rebuild count estimate.
- **Протокол:** 100 iter + 10 warmup per config = 62,500 main measurements.

---

## 4. Prototype

`prototype/dynamic_light_bench.cpp` ~600 LoC, GCC 16.1.1 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`.
Build green, 4 cosmetic warnings.

```bash
cd prototype && cmake -B build && cmake --build build && ./build/dynamic_light_bench
```

Output: `prototype/build/results.csv` (62,501 rows = header + 62,500 measurements).

---

## 5. Results

| Strategy | #src | Time(us) mean | PSNR(dB) | Chunks | Speedup vs B |
|:---------|:----|:-------------|:---------|:-------|:-------------|
| A_None   | 1   | 0.023        | 33.26    | 0.0    | 719× |
| A_None   | 5   | 0.023        | 24.62    | 0.0    | 3,524× |
| A_None   | 20  | 0.024        | 18.63    | 0.0    | 12,182× |
| B_FullBFS| 1   | 16.30        | 100.00   | 256.0  | 1× (ref) |
| B_FullBFS| 5   | 80.66        | 100.00   | 256.0  | 1× |
| B_FullBFS| 20  | 296.55       | 100.00   | 256.0  | 1× |
| C_BudgetBFS| 1 | 16.07        | 92.68    | 256.0  | 1.01× |
| C_BudgetBFS| 5 | 30.84        | 43.56    | 256.0  | 2.62× |
| C_BudgetBFS|20 | 46.71        | 28.65    | 256.0  | 6.35× |
| D_RateLimited|1| 6.01         | 55.87    | 18.6   | 2.71× |
| D_RateLimited|5| 31.48        | 50.22    | 91.1   | 2.56× |
| D_RateLimited|20| 104.21      | 46.29    | 367.3  | 2.85× |
| E_GPUInjection|1| 0.054       | 38.52    | 0.0    | 301× |
| E_GPUInjection|5| 0.166       | 30.24    | 0.0    | 486× |
| E_GPUInjection|20| 0.355      | 24.63    | 0.0    | 834× |

**Наблюдения:**

- **B_FullBFS** scales O(N_entities × voxels): 16→297 µs (1→20 entities), ≈15 µs/entity.
- **C_BudgetBFS** cost sub-linear: 16→47 µs for 1→20 entities (budget shared across all sources).
  Quality degrades when budget exhausted: 92.68 dB at 1 src → 28.65 dB at 20 src.
- **D_RateLimited** stable PSNR across entity counts: 55.87→46.29 dB (vs 92.68→28.65 for C).
  Cost scales linearly but 3× cheaper than B (every 3rd frame).
- **E_GPUInjection** CPU cost negligible (0.05-0.36 µs = SSBO write). PSNR lower (30.68 dB mean)
  because distance-falloff ≠ BFS propagation — physically smoother but differs from Minecraft-blocky light.
- **Cave_stress scene** had highest variance: opaque blocks limit BFS reach, throttled strategies show
  larger PSNR variation.

---

## 6. Verdict

`concluded-verdict-mixed` — hypothesis **partially validated**:

- **CPU cost < 1% budget:** YES — even B_FullBFS at 20 sources (297 µs) is ~0.9% of 33.3 ms frame at 30 Hz.
  E_GPUInjection is essentially free (0.36 µs at 20 sources).
- **Smooth interpolation cost:** NOT independently measurable — merged into BFS cost.
  For E_GPUInjection, interpolation is free (GPU computed per-pixel).
- **Incremental BFS applicability:** VALIDATED — C_BudgetBFS reduces CPU cost 2-6× with quality
  dependent on entity count.

**Mixed because:** quality-cost tradeoff is scene-dependent; E_GPUInjection offers best CPU profile
but lower PSNR vs BFS methods; choice depends on target hardware and quality requirements.

---

## 7. Integration recommendation

- **Target stage:** TODO.md §5.x Visual Polish.
- **Recommended strategy:** **E_GPUInjection (shader-based)** for minimal CPU overhead +
  **D_RateLimited fallback** for no-shader-path configs.
- **Implementation (3-step, ~320 LoC):**
  - Step 1 (XS, ~50 LoC): `EntityLightController` + SSBO for entity light data +
    `PROJECTV_ENTITY_LIGHTS=NONE|BFS|SHADER` env gate.
  - Step 2 (S, ~150 LoC): Shader integration in `voxel.frag` — sample entity light SSBO,
    compute per-fragment distance-falloff, blend with block lightmap.
  - Step 3 (XS, ~30 LoC): BFS fallback (C or D) for non-shader paths + Tracy plot.
- **Риски:** Shader-based method requires SSBO binding + fragment shader modification;
  PSNR gap vs BFS may be noticeable; entity count limit needed (< 64).
- **Зависимости:** None — independent post-process effect.

---

## 8. Sources

- sp614x, OptiFine DynamicLights source (`DynamicLights.java`, `DynamicLight.java`)
- LambdAurora, LambDynamicLights HOW_DOES_IT_WORK.md (1.16/1.17/1.21/1.21.11)
- LambdAurora, LambDynamicLights API.md, entity.html docs
- Gray R., How LambDynamicLights Integrates with Minecraft's Engine (lambdynamiclights.com, 2025)
- PaperMC, Starlight TECHNICAL_DETAILS.md (Fabric branch)
- optifine.readthedocs.io, Dynamic Lights configuration
- Modrinth, Dynamic Lights datapack (CreeperMeyT)
- Minecraft mappings: `LightEngine`, `ChunkLightProvider`, `ThreadedLevelLightEngine`

---

## 9. Mapping to ProjectV hot-path

Прототип моделирует CPU-side cost dynamic lighting. Реальный pipeline:
1. **Entity System** (ECS) собирает light-emitting entities → `EntityLightController`
2. **CPU:** для BFS-стратегий — инкрементальный BFS в LightSolver (как closed `incremental-light-propagation`)
3. **GPU:** для Shader-стратегии — SSBO с entity data → fragment shader читает и вычисляет falloff
4. **Lightmap:** результаты накладываются на block lightmap перед shading

**НЕ измерено:** GPU dispatch overhead, shader instruction cost, SSBO read bandwidth,
cross-vendor performance (AMD/Intel), driver overhead.

**Hardware baseline:** [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (Zen 3 5800X) + §3 (RTX 3060 Ti, 8 GiB VRAM).
