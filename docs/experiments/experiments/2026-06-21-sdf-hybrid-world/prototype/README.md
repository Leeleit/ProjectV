# sdf_hybrid_bench — SDF overlay benchmark

Standalone C++26 CPU prototype for `2026-06-21-sdf-hybrid-world` experiment.
Per `docs/experiments/AGENTS.md §1` + `benchmarks/methodology.md §3`.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
```

Or with raw clang:

```bash
clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic \
    sdf_overlay.cpp scenes.cpp vct_cone_march.cpp physics_normals.cpp bench.cpp \
    -o sdf_hybrid_bench
```

Toolchain per `docs/experiments/hardware-profile.md §6`:
- Clang 22.1.6 (ProjectV mainline baseline)
- libstdc++ 16.1.1 (GCC build, `GLIBCXX_3.4.35`)
- LLD linker

## Run

Single configuration:

```bash
./build/sdf_hybrid_bench --scene cave_stress --seed 42 \
    --encoding B_R8_1byte --build J_JFA_GPU --term T_Hybrid \
    --cones 6 --iters 1000
```

Output (human-readable summary):

```
[Summary]
  Scene:      cave_stress (seed 42)
  Encoding:   B_R8_1byte
  Build:      J_JFA_GPU
  Term:       T_Hybrid
  Cones:      6 (reference: 1024)
  Iters:      1000
  SDF build:  4.231 µs (std 0.521, min 3.412, max 5.123)
  Cone march: 0.082 µs (std 0.011, min 0.062, max 0.103)
  VRAM:       512 bytes/chunk
  Irradiance: 0.4123 (ref 0.4201) → PSNR 18.34 dB
  Normal err: voxel 24.32° / SDF 4.12° (lower = smoother)
```

CSV mode (append to file):

```bash
./build/sdf_hybrid_bench --scene cave_stress --seed 42 \
    --encoding B_R8_1byte --build J_JFA_GPU --term T_Hybrid \
    --cones 6 --iters 1000 --csv results.csv
```

CSV header:
```
scene,seed,encoding,build,term,cones,iters,sdf_build_us,sdf_std_us,march_us,march_std_us,vram_bytes,irradiance,ref_irradiance,psnr,voxel_normal_err,sdf_normal_err
```

## Sweep

For full experiment matrix (~3 × 4 × 3 × 5 × 5 = 900 measurements per vct-cone-count precedent × 2.5 ≈ 2 250 measurements), use a shell loop:

```bash
for scene in uniform_air uniform_floor forest_floor cave_stress mixed_biome; do
  for seed in 1 7 42 1234 31337; do
    for encoding in A_None B_R8_1byte C_R8_4quant D_RLE_NoneSparse; do
      for build in J_JFA_GPU K_BruteForce_BFS L_AdaptiveMultiRes; do
        for term in T_VoxelDiscrete T_SDFSmooth T_Hybrid; do
          ./build/sdf_hybrid_bench --scene $scene --seed $seed \
            --encoding $encoding --build $build --term $term \
            --cones 6 --iters 1000 --csv results.csv
        done
      done
    done
  done
done
```

Expected runtime: ~30 min on Zen 3 5800X (per `hardware-profile.md §1`).
Expected total measurements: 5 × 5 × 4 × 3 × 3 = 900.

## Key measurement axes (per `2026-06-21-sdf-hybrid-world/README.md §3`)

1. **SDF encoding:** `A_None` / `B_R8_1byte` / `C_R8_4quant` / `D_RLE_NoneSparse`
2. **Build algorithm:** `J_JFA_GPU` (Rong 2006) / `K_BruteForce_BFS` (baseline) / `L_AdaptiveMultiRes`
3. **VCT termination:** `T_VoxelDiscrete` / `T_SDFSmooth` / `T_Hybrid`

## Metrics

- **Build cost:** µs/chunk (mean + std + min + max over N=1000 iters)
- **March cost:** µs/cone (mean + std, aggregated over N=1000 fragments × M cones)
- **VRAM:** bytes/chunk per encoding
- **VCT quality:** PSNR vs 1024-cone reference (per `vct-cone-count-atlas-precision` baseline)
- **Physics normal:** angular error vs analytical (8 standard contact points)

## Dependencies

None. Pure C++26 + standard library. No GPU, no Vulkan, no mainline ProjectV coupling.
