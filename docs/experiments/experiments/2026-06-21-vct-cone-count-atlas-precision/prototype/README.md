// SPDX-License-Identifier: MIT
// vct_cone_atlas_bench — standalone Vulkan 1.4 compute prototype.
//
// Hypothesis: 6 cones × R16G16B16A16_SFLOAT = sweet spot (per Crassin 2011 GIVoxels §5
// 5-6 diffuse cones baseline + Panteleev 2014 R16F + OGRE 2019 R8 banding risk).
//
// Build:
//   cmake -S . -B build -G Ninja
//   cmake --build build
//
// Run:
//   ./build/vct_cone_atlas_bench
//   # → results.csv (12 measurements: 9 measured configs + 3 references)
//
// Hardware: dev host `obvium` per docs/experiments/hardware-profile.md §3
// (NVIDIA RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, 8 GiB VRAM).
//
// Synthetic scene: a 128³ voxel grid containing
//   - ground plane at y=64 (gray, alpha=1)
//   - sky above (white, alpha=1)
//   - radial boulder at center (brown, alpha=1)
//   - empty space (alpha=0)
//
// Top-down view at 32×32 sample grid for PSNR measurement.
//
// Output:
//   results.csv: format, cones, mean_ms, p95_ms, psnr_vs_1024_ref
//
// See README.md (parent dir) for full design + interpretation.
