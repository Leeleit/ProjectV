# Sources — simd-procedural-noise

Все ссылки accessed `2026-06-20` через `webfetch` (для верификации цитат) или `websearch` (для
initial discovery).

## Primary (cited in README §2)

1. **Intel ISPC official benchmark** — https://ispc.github.io/perf.html
    - Perlin Noise Function: **5.37× speedup** (1 core, AVX2, gcc 4.2.1 baseline scalar C++).
    - Baseline: 4-core Apple iMac Core-i7 3.4GHz.

2. **Auburn/FastNoise2** — https://github.com/Auburn/FastNoise2/
    - 3D Perlin 261.10 M/s (AVX2) vs 47.93 M/s (FastNoise Lite scalar) = **5.45×**.
    - 3D Simplex 268.44 M/s vs 36.83 M/s = **7.29×**.
    - 2D Perlin 624.27 M/s vs 92.83 M/s = **6.73×**.
    - Baseline: Intel 7820X @ 4.9 GHz, clang-cl 10.0.0 -m64 /O2.
    - License: MIT, 1.4k stars.

3. **Auburn/FastNoiseSIMD (legacy)** — https://github.com/Auburn/FastNoiseSIMD/tree/0.7
    - 2016 numbers: Perlin 3D 324 ns (AVX2) / 592 ns (SSE4.1) / 1002 ns (scalar) for 32³ points
      on Intel Xeon Skylake @ 2.0 GHz, Intel 17.0 x64.
    - AVX2 vs scalar: **3.1×** (less than v2 due to less optimized kernels).

4. **Clang 22 Release Notes
   ** — https://rocmdocs.amd.com/projects/llvm-project/en/latest/LLVM/clang/html/ReleaseNotes.html
    - Added `__builtin_masked_load/store/gather/scatter` для conditional memory ops.
    - AVX/AVX512 intrinsics в constexpr contexts.

5. **libstdc++ P1928 std::simd patch v6** — https://gcc.gnu.org/pipermail/gcc-patches/2026-March/711217.html
    - March 2026 patch: P1928 std::simd для C++26.
    - x86-only для начала, значительные отличия от `std::experimental::simd` (template instantiation reduction).

6. **Clang 21+ ABI regression bug #176670** — https://github.com/llvm/llvm-project/issues/176670
    - `simd_of<uint64_t, 4>` parameter pass-by-implicit-pointer regression в Clang 21 для libstdc++
      `std::experimental::simd`.
    - **Open, no milestone, no PR** на 2026-03-13 (verified 2026-06-20).

7. **LLVM Auto-Vectorization docs** — https://llvm.org/docs/Vectorizers.html
    - Loop + SLP vectorizers. Vectorize math intrinsics при наличии `-fveclib` (libmvec / SLEEF / etc).
    - Для custom noise function: indirect table lookup блокирует vectorizer analysis.

8. **TopicTrick C++ SIMD blog** — https://topictrick.com/blog/cpp-simd-intrinsics-optimization
    - 2025-10-20, dot product benchmark:
        - Scalar ~500ns
        - Auto-vectorized ~70ns (**7×**)
        - AVX2 manual ~65ns (**7.7×**)
        - AVX-512 FMA ~35ns (**14×**)
    - Совет: "Always try auto-vectorization before writing intrinsics".

## Secondary (background)

9. **xtensor-stack/xsimd** — https://github.com/xtensor-stack/xsimd
    - C++ SIMD wrappers, BSD-3 license.
    - Production в Firefox, Apache Arrow, Krita, Pythran.
    - v8 = complete rewrite. SSE2/3/4 + AVX/AVX2/AVX512 + FMA3 + NEON + SVE + WASM + VSX + RISC-V + VXE.

10. **VCDevel/std-simd** — https://github.com/VcDevel/std-simd
    - TS implementation of `std::experimental::simd` for GCC.
    - Baseline для ISO/IEC TS 19570:2018 §9.
    - Master last push 2023-03-10 (development moved to libstdc++ for C++26 std::simd).

11. **Hacker News: Expressive Vector Engine / xsimd discussion** — https://news.ycombinator.com/item?id=42603188
    - 2025-01-08, multi-arch SIMD library design philosophy.
    - EVE = compile-time arch selection, xsimd = runtime template param, ISPC = separate toolchain.
    - 95%+ performance of hand-written intrinsics achievable with libraries.

12. **ispc GitHub** — https://github.com/ispc/ispc/tree/refs/heads/main
    - v1.30.0 (2026-02-04). BSD-licensed. x86 SSE2/4 + AVX/AVX2/AVX512 + ARM NEON + Intel GPU.

13. **Pixelant ISPC blog** — https://pixelantgames.com/blog/ispc-the-shaders-of-cpu/
    - 2025-09-17, game dev perspective: ISPC = "CPU shaders", Unreal Engine Chaos uses it.
    - 3-6× on SSE, 5-6× on AVX2.

14. **bhavana.io ISPC + Compute Shaders
    ** — https://bhavana.io/bridging-the-cpu-gpu-divide-experimenting-with-ispc-and-compute-shaders/
    - 2024-12-17, ISPC + threading jump flooding: 8.5s → 5ms.
    - GPU compute still 3.5× faster for that workload.

15. **Hackernoon/Auburn FastNoise2 header (legacy)
    ** — https://github.com/jackmott/FastNoise-SIMD/blob/master/FastNoise/headers/FastNoise.h
    - AVX2 `_mm256_*` wrappers — pattern reference (unions, gather).

## Verifications

- **Web searches performed:** 4 batch queries (Exa + fallbacks):
    1. "AVX2 SIMD Perlin noise performance benchmark 2024 2025 procedural generation" (1/4 success due to rate limit,
       retry succeeded)
    2. "std::experimental::simd Clang 22 GCC 15 maturity AVX2 2025 2026 production" (8 results)
    3. "xsimd vs ISPC SIMD C++ procedural noise game engine performance 2024 2025" (6 results)
    4. "Clang 22 AVX2 intrinsics FMA performance auto-vectorize noise function 2025" (6 results)
- **Webfetches performed:** 3 (ISPC perf page, FastNoise2 GitHub, Clang issue #176670).
- **Internal verifications:**
    - `Splitmix32_scalar` vs `Splitmix32_8` AVX2: bit-identical output на 8 random uint32 inputs (test program
      `/tmp/debug_hash.cpp`).
    - `PerlinSimd2D` scalar vs AVX2: bit-identical accumulator на 1024 sample batch (rel_err=0.00e+00).
    - `PerlinSimd3D` scalar vs AVX2: bit-identical (rel_err=0.00e+00).
    - `Perlin2D` spec scalar vs AVX2: bit-identical.
    - `Perlin3D` spec scalar vs AVX2: bit-identical (после fixing scalar 3D hash bug — пропущенный `perm[AA]` lookup).