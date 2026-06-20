# Prototype — simd-procedural-noise

## Build

```bash
cd docs/experiments/experiments/2026-06-20-simd-procedural-noise/prototype
clang++ -std=c++26 -O3 -march=native -DNDEBUG bench.cpp -o /tmp/bench_simd_noise
```

**Требования к host:**

- Clang 22.1.x (текущий ProjectV baseline per `agent/knowledge.md §17`).
- CPU с AVX2 + FMA (Haswell+ Intel / Ryzen+ AMD). Dev host = Ryzen 5800X (Zen 3) — ✅.

## Run

```bash
/tmp/bench_simd_noise
```

**Выход:**

- stdout: human-readable summary (mean / p95 / p99 / throughput per kernel) + correctness check.
- `../results.csv`: machine-readable per-config stats (8 rows: 2 variants × 2 dims × 2 kernels).
- `../RESULTS.md` (внешний): полный анализ.

## Что внутри

Один файл `bench.cpp` содержит **8 конфигураций** (2 variants × 2 dims × 2 kernels):

### Variants

1. **Spec** — faithful Ken Perlin improved noise (2002), smootherstep fade curve, 256-byte
   permutation table, 8 gradient vectors для 2D, 12 для 3D. Canonical algorithm.
2. **SIMD-hash** — splitmix32 integer hash + 16-entry gradient table. SIMD-friendly variant
   (no perm table, no scalar extraction in hot path). Slightly different noise distribution
   (no permutation bijection) but C¹ continuous, visually similar.

### Kernels

- **Scalar** — reference impl for each variant. LLVM SLP vectorizer при `-O3 -march=native`
  auto-vectorizes hot loop to ~4 lanes (verified via asm: ~108 `vmulps`/`vfmadd` instructions).
- **AVX2/FMA** — 8 samples parallel via `__m256` lanes. `__attribute__((target("avx2,fma")))`
  для cross-`march` compatibility. Hash extraction = pure SIMD integer ops (splitmix32)
  или scalar (spec variant) + `i32gather` for gradient lookup.

### Harness

- `Bench()` — warm-up 10 iter + 1000 замеров + Stats (mean / median / p95 / p99 / std / min / max).
- `PinToCore(0)` — `sched_setaffinity` на core 0 для низкого jitter.
- Deterministic seed (`0xC0FFEE`) — reproducible output.

## Измеренные результаты (single-threaded, 5800X, governor `powersave`)

| Variant | Dim | Kernel | Throughput |     Speedup      |
|:--------|:---:|:-------|-----------:|:----------------:|
| spec    | 2D  | scalar |  216.6 M/s |        —         |
| spec    | 2D  | avx2   |  246.3 M/s |    **1.14×**     |
| spec    | 3D  | scalar |  117.9 M/s |        —         |
| spec    | 3D  | avx2   |   72.8 M/s | **0.62×** (loss) |
| simd    | 2D  | scalar |  134.2 M/s |        —         |
| simd    | 2D  | avx2   |  244.9 M/s |    **1.83×**     |
| simd    | 3D  | scalar |   74.1 M/s |        —         |
| simd    | 3D  | avx2   |  112.0 M/s |    **1.51×**     |

**Гипотеза (≥ 4×) NOT confirmed on Zen 3 AVX2.** Theoretical max = ~2× due to scalar
auto-vec to 4 lanes. To reach literature 5-7×, need ISPC toolchain or AVX-512 hardware.

**Correctness:** all 4 AVX2 vs scalar = bit-identical (`rel_err = 0.00e+00`).

Полный анализ: `../RESULTS.md`. Integration recommendation: `../README.md §7`.

## Что НЕ внутри

- Multi-threading (1 thread per kernel — sequential throughput).
- AVX-512 / AVX-VNNI path (нет на dev host).
- FBM (fractal Brownian motion) multi-octave — out of scope (измеряется одна octave).
- PSHUFB-based hash (faster than gather but more complex; FastNoise2 uses this).
- AVX-512 / Arm NEON / RISC-V V cross-platform portability.

## Sanity check

```bash
# Verify AVX2 path produces same noise as scalar (within bit-exact):
/tmp/bench_simd_noise 2>&1 | grep "rel_err"
# Expected: rel_err = 0.00e+00 for all 4 (variant × dim) pairs.
```

## Cross-vendor portability

Per `agent/knowledge.md §10` + `§17` — ProjectV baseline = x86-64 Linux + clang. Текущий прототип:

- ✅ AVX2/FMA path tested на Zen 3 (Ryzen 5800X).
- ⚠️ Не тестировалось на Intel Haswell/Skylake/Alder Lake — но тот же baseline AVX2 ISA (must work).
- ❌ Не тестировалось на Arm (Apple Silicon, AWS Graviton) — нет NEON path. Production-grade
  cross-vendor требует либо `std::experimental::simd` (но см. README §2 bug #176670), либо
  ISPC (но добавляет toolchain overhead per README §7 integration recommendation).