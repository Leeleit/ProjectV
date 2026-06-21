# Build & Run — vct_temporal_denoise_sim

**Build** (single file, no external deps beyond C++26 stdlib):
```bash
cd /home/le1t/Projects/ProjectV/docs/experiments/experiments/2026-06-21-vct-temporal-denoise-tensor-core/prototype
mkdir -p build
clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
    -o build/vct_temporal_denoise_sim vct_temporal_denoise_sim.cpp
```

Expected build output: clean (0 warnings) per `benchmarks/methodology.md §8` self-check.

**Run**:
```bash
cd build
./vct_temporal_denoise_sim \
    --scenes 5 --seeds 3 --frames 50 --warmup 5 \
    --output results.csv
```

**Output**: `build/results.csv` with 125 rows = 5 strategies × 5 scenes × 5 seeds.
Columns: `strategy,scene,seed,n_frames,psnr_mean_db,psnr_std_db,psnr_min_db,psnr_max_db,build_us`.

**Analysis**: see `../RESULTS.md` for headline findings.

**Design notes:**
- **Simplified radiance model:** per-pixel radiance = sum over N cones of (cone.direction ·
  voxel.color) + per-cone Gaussian noise + temporal jitter. **NOT full 3D voxel traversal.**
  Denoise algorithm correctness is independent of voxel physics; only per-frame radiance
  noise characteristics matter.
- **480×270 resolution** = 1/8 of 1080p, representative for voxel scenes.
- **32³ voxel grid** per `2026-06-21-sub-chunk-layers` precedent for direct comparability.
- **5 procedural scenes** (uniform_floor / forest_floor / cave_stress / mixed_biome /
  uniform_air) per same precedent.
- **6 diffuse + 1 specular cones** per `TODO.md §5.1` current mainline.
- **1024-cone brute force reference** = ground truth.
- **Noise std = 0.15** per-cone radiance (calibrated to produce visible per-frame variance
  with 6 cones, similar to Crassin 2011 + Panteleev 2014 measurements).
- **Temporal jitter amp = 0.02** simulates voxel mipmap aliasing temporal correlation.
- **D_TemporalReprojectCooperativeMatrix alpha = 0.2** (higher than C's 0.1 since
  cooperative matmul SNR improves by sqrt(16) = 4× per tile — equivalent to 4× more samples).
- **E_TemporalReprojectSVGF** implements 3-pass Schied 2017 algorithm: temporal accumulation
  + variance estimation + 3×3 bilateral filter.

**Wall time estimate:** ~5-15 seconds on Zen 3 5800X for 125 measurements × 100 frames each.
