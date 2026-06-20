# STATUS — 2026-06-20-simd-procedural-noise

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-20 — closed per §13.5. 8 configs benchmarked (2 variants × 2 dims × 2 kernels).
All AVX2 vs scalar bit-identical (`rel_err = 0.00e+00`). Speedup ceiling 1.5-1.83× on Zen 3 (not 4-7×
literature) due to LLVM SLP auto-vectorization of scalar loop to 4 lanes.
**Blocker:** нет.
**Next tick:** N/A (closed). Re-evaluation triggers: (a) Stage 5.1 VCT (indirect lighting noise A/B
test), (b) AVX-512 hardware arrival (Zen 5 / Intel Arrow Lake), (c) ISPC toolchain adoption (separate
follow-up backlog item).