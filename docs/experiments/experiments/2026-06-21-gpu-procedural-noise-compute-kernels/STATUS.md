# STATUS — 2026-06-21-gpu-procedural-noise-compute-kernels

**Phase:** concluded-verdict-mixed
**Last action:** 2026-06-21 — closed per §13.5. Standalone Vulkan 1.4 compute prototype ran 5 variants
× 1000 iters × 3 runs на RTX 3060 Ti. All 5 variants в пределах 2.9% mean (0.0272-0.0280 ms).
OPENSIMPLEX2 == SIMPLEX == PERLIN == VALUE < WORLEY. Stage 4.1 budget (50 µs/chunk) — 8× headroom
single octave, 1.9× FBM 4 octaves, 0.63× multi-channel FBM. Memory-bound (SSBO write = 65% of peak
bandwidth). Noise algorithm НЕ bottleneck для chunkSize=8 pattern.
**Blocker:** нет.
**Next tick:** N/A (closed). Mainline integration path documented в `README.md §7`.
Re-evaluation triggers: (a) AMD RDNA / Intel Arc validation (cross-vendor не измерено),
(b) FBM 4+ octaves + multi-channel прототип для verify budget fit, (c) Nsight Compute
register/occupancy follow-up, (d) `dxc-vs-glslc-toolchain` verdict влияет на wave intrinsics
availability (SM 6.x) — может open up дальнейшие ALU optimizations.
