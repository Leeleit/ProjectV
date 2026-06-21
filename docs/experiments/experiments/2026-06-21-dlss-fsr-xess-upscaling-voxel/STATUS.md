# 2026-06-21-dlss-fsr-xess-upscaling-voxel — STATUS

**Phase:** E (concluded-verdict-mixed)
**Started:** 2026-06-21
**Closed:** 2026-06-21 (single session, ~2h)
**Verdict:** `mixed` — FSR 3.1 = best cost-benefit (3.7-23% savings, PSNR 39 dB); DLSS 4.5 + XeSS 2 XMX = real GPU measurements required (analytical model conservative for Tensor Core / XMX); FSR 4 = NOT usable on Vulkan per `mypcbottleneck 2026-06-04`; DirectSR = defer to Vulkan core promotion; Frame Generation = out of scope.
**Last action:** Prototype built (clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic, 0 warnings) + 288 measurements (4 upscalers × 4 presets × 3 extents × 2 scenes × 3 seeds) + RESULTS.md + cross-vendor matrix + 3-step migration recommendation per `agent/knowledge.md §30.4` precedent.
**Blocker:** нет.
**Next tick:** None — closed per `AGENTS.md §6` DoD.
**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §3 (RTX 3060 Ti GA104, 8 GiB VRAM, Vulkan 1.4.341, NVIDIA 610.43.02).
