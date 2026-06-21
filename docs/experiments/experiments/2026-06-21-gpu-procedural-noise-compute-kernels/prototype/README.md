# Standalone Vulkan 1.4 Compute Benchmark — gpu-procedural-noise-compute-kernels

## Purpose

Measure GPU compute cost of 5 different 3D noise kernels (Value, Perlin, Simplex,
OpenSimplex2, Worley) under ProjectV's chunkSize=8 world-gen pattern, on RTX 3060 Ti
(Ampere, Vulkan 1.4).

Each kernel writes 1 float per voxel into an SSBO for 4096 chunks (8³ × 4096 = 2,097,152
voxel evaluations per dispatch). GPU time measured via `vkCmdWriteTimestamp` top-of-pipe
and bottom-of-pipe.

## Build

```bash
# Compile all 5 SPIR-V variants from one GLSL source.
cd prototype/
glslc -DVARIANT_VALUE        noise_kernels.comp -o noise_value.spv
glslc -DVARIANT_PERLIN       noise_kernels.comp -o noise_perlin.spv
glslc -DVARIANT_SIMPLEX      noise_kernels.comp -o noise_simplex.spv
glslc -DVARIANT_OPENSIMPLEX2 noise_kernels.comp -o noise_opensimplex2.spv
glslc -DVARIANT_WORLEY       noise_kernels.comp -o noise_worley.spv

# Build C++ harness with Vulkan 1.4 headers (system or vendored).
clang++ -std=c++26 -O3 -march=native -DNDEBUG \
    -I/usr/include \
    main.cpp -o gpu_noise_bench \
    -lvulkan -lm -lpthread
```

## Run

```bash
./gpu_noise_bench
```

Output:
- Console: per-variant GPU time stats (mean, median, p95, p99, stddev, min, max)
- File: `results.csv` with `variant,mean_ms` rows

## Design choices

**Why single shader with #define variants?** Keeps the dispatch pattern identical across
variants — only the noise kernel body changes. This matches production: in mainline
`world_gen.comp` you would have one noise function selected at compile time via CMake
option (e.g. `-DPROJECTV_NOISE_KERNEL=OPENSIMPLEX2`). The harness measures the cost of
the kernel itself, not dispatch overhead.

**Why SSBO output, not storage image?** Matches `TODO.md §4.1` plan: "Генератор должен
писать воксели напрямую в глобальный SVDAG/VDB буфер на GPU". SSBO is the canonical
GPU voxel storage layout (per `nanovdb-on-gpu` verdict=yes). Storage image would add
unrelated cost from format conversion and Y-axis tile addressing on AMD.

**Why workgroup size 64?** Per NVIDIA Nsight Compute guidance for Ampere: "A thread
group with 32 threads or fewer will be limited to half occupancy. Increasing to 64
threads per CTA will relieve this issue." 64 = 2 warps = good balance for compute-bound
kernels with shared-memory-free body.

**Why 4096 chunks per dispatch?** Models Stage 4.3 draw distance: 128m × 128m × 64m
visible volume at chunkSize=8 = 16×16×8 = 2048 chunks; we use 4096 = 2× Stage 4.3
target to stress-test throughput.

**Why 1000 iters?** Per `benchmarks/methodology.md §3` default N=1000; we add 10
warmup to ensure shader compile pipeline caches + GPU clock stabilizes.

## Limitations (explicit, for §5 Results honesty)

- Single GPU vendor: NVIDIA Ampere (RTX 3060 Ti GA104). No AMD/Intel data.
- No FBM (multi-octave) — single octave only. Real Stage 4.1 world gen uses 4-8 octaves
  per voxel for terrain detail. Results scale linearly with octave count.
- No biome noise — just heightmap. Stage 4.1 uses 5 noise channels (height, cave, biome).
  Results scale linearly with eval count.
- No integration with SVDAG dedup or NanoVDB SSBO write pattern — pure compute cost only.
- No actual spectral quality measurement (would require FFT + ImageDiff framework).
  Spectral quality claims are literature-cited.
