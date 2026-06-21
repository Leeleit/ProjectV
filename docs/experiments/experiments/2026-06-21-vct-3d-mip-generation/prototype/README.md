# Prototype: `mip_bench`

Standalone C++26 CPU benchmark for **3D mip chain generation** algorithm choice (VCT atlas use case).
**Not** ProjectV mainline — per `docs/experiments/AGENTS.md §2` scope discipline.

## Что внутри

- **4 downsample algorithms:**
  - `A_2x2x2_Box` — 8-sample arithmetic mean (baseline, current mainline assumed per
    `2026-06-21-vct-cone-count-atlas-precision` §3.2)
  - `B_4tap_Smooth` — 4-tap diagonal smoothstep (NVIDIA HZB practice analog)
  - `C_8tap_3DGaussian` — 8-tap 3D Gaussian weighted (σ=0.5 voxel, Crassin 2011 §3.2 cone-tapered filter basis)
  - `D_Blit3D_perAxis` — 3 sequential per-axis 2D blits (CPU analog of `vkCmdBlitImage` chain)
- **4 synthetic 3D voxel atlas scenes** (per `vct-cone-count-atlas-precision` §3 precedent):
  - `uniform_sky` — homogeneous fill
  - `uniform_floor` — bottom/top hemisphere
  - `cave_stress` — central sphere of bright color (worst-case light leaking)
  - `mixed_biome` — multi-frequency sine-wave noise (Minecraft-style)
- **2 atlas sizes:** 64³, 128³ (Stage 5.1 working size 128³ per `vct-cone-count-atlas-precision` §5)
- **3 target mip levels:** 1 (inner), 3 (mid), 5 (outer)
- **5 seeds:** 1, 7, 42, 1234, 31337
- **N=100 iterations + 10 warmup** per `benchmarks/methodology.md §3`

**Total:** 4 × 4 × 2 × 3 × 5 × 100 = **48,000 main measurements**.

## Сборка

```bash
cd docs/experiments/experiments/2026-06-21-vct-3d-mip-generation/prototype
mkdir -p build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

**Требования:** Clang 22.x (per `agent/knowledge.md §17`), CMake 3.20+, Ninja.

## Запуск

```bash
./mip_bench
```

Вывод: `build/results.csv` (480 rows = 1 header + 480 measurement rows).

## Что измеряет

`results.csv` schema:

| Column          | Type    | Description                                            |
|:----------------|:--------|:-------------------------------------------------------|
| `alg`           | string  | Algorithm name (`A_2x2x2_Box` / `B_4tap_Smooth` / ...) |
| `scene`         | string  | Scene name (`uniform_sky` / `uniform_floor` / ...)     |
| `atlas_size`    | int     | Atlas edge length (64 / 128)                           |
| `mip_level`     | int     | Target mip level for PSNR (1 / 3 / 5)                  |
| `seed`          | uint    | PRNG seed (1 / 7 / 42 / 1234 / 31337)                  |
| `psnr_db`       | float   | Mean PSNR vs analytical 3D Gaussian reference (dB)     |
| `perf_mean_ms`  | float   | Mean wall time per mip chain refresh (ms)              |
| `perf_p95_ms`   | float   | p95 wall time per mip chain refresh (ms)               |
| `perf_p99_ms`   | float   | p99 wall time per mip chain refresh (ms)               |
| `perf_std_ms`   | float   | stddev wall time per mip chain refresh (ms)            |
| `n`             | int     | Sample count (= 100 iterations)                       |

## ProjectV mapping

- `mip_chain refresh` = per-frame mip chain generation cost for VCT 3D atlas (Stage 5.1).
- In mainline, this would be `voxelize_mipgen.comp` compute pass after `voxelize.comp` writes mip 0,
  before `voxel.frag` cone-march reads mips 0-7.
- **CPU prototype vs GPU mainline:** no Vulkan dispatch, no GPU timing. Per `benchmarks/methodology.md §5`,
  result maps to "algorithm choice" axis, not "raw GPU speed". GPU `vkCmdBlitImage` cost = follow-up
  (deferred до Stage 5.1 integration milestone).

## Известные ограничения

- CPU prototype, не GPU dispatch (3D blit pattern D = simulated via per-axis CPU loops, real
  `vkCmdBlitImage` GPU timing is out of scope).
- Synthetic 3D voxel atlas (not real ProjectV chunk content; per-scene representative).
- Analytical 3D Gaussian low-pass reference (ideal, not real ground truth).
- Mutations (per-chunk rebuild on voxel edit) out of scope (Stage 5.1 DoD does not require).
- Crassin 2011 cone-tapered anisotropic filter (direction-weighted) = follow-up, NOT in this prototype.
- 4D temporal VCT (closed `2026-06-21-taa-motion-vectors` follow-up candidate) = out of scope.

## Hardware baseline

См. [`docs/experiments/hardware-profile.md`](../../../hardware-profile.md) §1 — dev host
`obvium` (Zen 3 5800X, governor=`powersave`, no AVX-512). Captured `2026-06-20`, <14 дней,
**no probe needed** per `AGENTS.md §14`.
