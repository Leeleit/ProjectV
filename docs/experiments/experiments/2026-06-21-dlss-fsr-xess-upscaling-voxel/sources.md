# Sources — 2026-06-21-dlss-fsr-xess-upscaling-voxel

Все ссылки верифицированы (2024-2026). Per `AGENTS.md §5.3` + `docs/experiments/AGENTS.md §4` обязательность web-search.

## Primary sources (15+, 2024-2026)

1. **StraySpark, "DLSS 4, FSR 4, and XeSS 2: The Complete UE5.7 Super Resolution Integration Guide"** — https://www.strayspark.studio/blog/dlss-fsr-xess-super-resolution-ue57-guide — published 2026-03-25. UE 5.7 production integration reference; benchmarks: FSR 4 Balanced +69% FPS, FSR 3.1 Balanced +100%+ FPS, XeSS 2 Quality +53% FPS; FSR 4 = RDNA 4-only; FSR 3.1 = automatic fallback для older hardware + non-RDNA 4; DirectSR = Microsoft unified API; vendor decision matrix (NVIDIA → DLSS Balanced, AMD RDNA 4 → FSR 4 Balanced, AMD RDNA 2/3 → FSR 3.1 Balanced, Intel Arc → XeSS 2 Balanced, others → FSR 3.1 Quality).

2. **NVIDIA/DLSS GitHub releases** — https://github.com/NVIDIA/DLSS/releases — DLSS SDK 310.6.0 (Mar 2026): "Added DLSS Frame Generation 5x and 6x Modes", "DLSS Transformer Model is officially out of Beta", "Bug Fixes & Stability Improvements". Streamline SDK = Vulkan-compatible integration path.

3. **NVIDIA Technical Blog, "NVIDIA DLSS 4.5 Delivers Super Resolution Upgrades and New Dynamic Multi Frame Generation"** — https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/ — published 2026-01-14 (CES 2026). 2nd-gen transformer model = 5× more compute, 6x Multi Frame Generation mode, Dynamic MFG.

4. **Wccftech, "AMD FidelityFX SDK v1.1 Now With FSR 3.1 Support"** — https://wccftech.com/amd-fidelityfx-sdk-v1-1-fsr-3-1-support-enhanced-upscaling-quality-decoupled-frame-generation-dlss-xess/ — published 2024-07-09. FSR 3.1 Vulkan support explicit; Decoupled Frame-Gen (DX12-only); Brixelizer GI alternative (RDNA matrix accelerator tested on RX 7900 XTX).

5. **RigPulse Editorial, "DLSS 4 vs FSR 4 vs XeSS 2 in 2026: Real-World Differences"** — https://rigpulse.ai/blog/dlss-4-vs-fsr-4-vs-xess-2 — published 2026-03-29. Buyer guide: DLSS 4 best for NVIDIA + RT + 4K; FSR 4 best for RDNA 4; XeSS 2 best for Intel Arc; FSR 3.1 = broad fallback.

6. **TechSpot, "DLSS vs FSR vs XeSS Support Across 650+ Games"** — https://www.techspot.com/article/3093-dlss-vs-fsr-vs-xess-game-support/ — published 2026-03-12. Industry adoption metrics: ~90% pre-2025 games support some form; FSR 4 = requires FSR 3.1 signed DLL integration; AMD driver upgrade = DX12-only.

7. **mypcbottleneck.com, "FSR 4-Supported Cards, Games, How to Enable It and How It Compares to DLSS 4"** — https://mypcbottleneck.com/fsr-4/ — published 2026-06-04. **CRITICAL FINDING: "Vulkan API games and games using non-standard FSR integration methods are not compatible with the FSR 4 Upgrade feature"** = FSR 4 driver upgrade = DX12-only; FSR 3.1 = universal Vulkan fallback; 90+ games support FSR 4 (mostly via driver upgrade, not native integration).

8. **Wccftech, "NVIDIA DLSS 4.5 SDK Now Available, Enabling Devs To Integrate Ray Reconstruction, Dynamic Frame Gen, & More In Their Games"** — https://wccftech.com/nvidia-dlss-4-5-sdk-now-available-enabling-devs-to-integrate-dynamic-frame-gen-more-in-their-games/ — published 2026-04-21. DLSS 4.5 Streamline SDK release; Dynamic Multi Frame Generation = auto frame multiplier for monitor refresh; Atomic Heart DLSS 4.5 example.

9. **optiscaler/OptiScaler GitHub** — https://github.com/optiscaler/OptiScaler — open-source cross-API hub. Enables DLSS replacement с XeSS/FSR in games with DLSS support; Vulkan support per README; FSR4 officially RDNA4 only; DX11/DX12/Vulkan cross-API.

10. **GamerHardware.org, "FSR vs DLSS vs XeSS: Complete Upscaling Guide (2026)"** — https://gamerhardware.org/fsr-vs-dlss-vs-xess-upscaling/ — published 2026-03-29. Compatibility matrix per GPU family: DLSS 4 Multi Frame Gen = RTX 50 only; FSR 3.1 = most universal; XeSS Frame Gen = Intel Arc only.

11. **NVIDIA GeForce news, "DLSS 4 Multi Frame Generation Out Now"** — https://www.nvidia.com/en-us/geforce/news/gfecnt/20251/dlss-4-multi-frame-generation-out-now/ — published Jan 2025. DLSS 4 launch context: transformer model architecture; MFG 3x; new Frame Generation model for RTX 40 + RTX 50 with reduced VRAM.

12. **NVIDIA/DLSS commit 9a6b48a, "DLSS 310.4.0 SDK"** — https://github.com/NVIDIA/DLSS/commit/9a6b48a79d5ae41bf1481d0c83d73859ec481bd2 — published 2025-08-25. SDK changelog: Ray Reconstruction transformer-based preset; Linearized Depth Parameter for FG; 5x/6x/8x modes.

13. **Wccftech, "DLSS vs FSR vs XeSS Explained: AI Upscaling, Frame Generation & Ray Reconstruction Compared"** — https://wccftech.com/roundup/nvidia-dlss-vs-amd-fsr-vs-intel-xess-everything-you-need-to-know/ — published 2026-03-19. Comprehensive comparison table: DLSS 4 (2025), FSR 4, XeSS 2 features + GPU compatibility matrix.

14. **Khronos Vulkan Documentation** — https://docs.vulkan.org/spec/latest/appendices/versions.html — `VK_KHR_fragment_shading_rate` verified **NOT in Vulkan 1.4 core** (remains device extension in 1.4); Vulkan 1.4.350 = current.

15. **StraySpark 2026-03-25 (article 1, "DirectSR" section)** — UE 5.7 DirectSR integration; Vulkan = beta status; defer to core promotion.

## Secondary sources (context, 2024-2026)

- NVIDIA DLSS SDK v310.3.0 release tag (Jun 2025) — Transformer model beta, 97.2 MB Linux demo / 84.8 MB Windows demo.
- NVIDIA RTX Neural Texture Compression SDK (Mar 2026 update) — complementary AI tech.
- AMD FSR 4 / Redstone marketing materials (RDNA 4 launch) — ML-based upscaler on dedicated AI accelerators.
- Intel XeSS 2.0 SDK release notes (Dec 2024) — XeSS-FG + XeLL latency reduction.
- OptiScaler Wiki (FSR4 compatibility list) — known supported games per AMD driver version.
- OptiScaler Wiki (Unreal Engine tweaks) — UE-specific integration notes for FSR upscaler replacement.

## Cross-references в existing mainline (per `agent/knowledge.md`)

- `TODO.md §4.3` — Stage 4.3 lift draw distance (Nearest Gap)
- `agent/workspace.md §2` — Nearest Gap callout
- `src/render/TaaRenderTargets.{hpp,cpp}` — TAA pipeline = integration point
- `src/render/Taa.cpp` — TAA Halton jitter = upscaling-aware input
- `agent/knowledge.md §30.4` — 3-step migration precedent
- `2026-06-21-taa-motion-vectors` — motion vector MRT (R16G16_SFLOAT) = upscaling standard input
- `2026-06-20-bindless-descriptor-overhead` Phase D — bindless = required for cross-vendor upscaling
- `2026-06-21-depth-occlusion-quantization` — VRAM-budget axis (cross-cutting)
- `2026-06-21-vk-fragment-shading-rate-voxel` — VRS cost axis (complementary)
- `2026-06-20-restir-gi-feasibility` — DLSS Ray Reconstruction relevance
- `2026-06-20-vct-vs-rt-cutoff` — Stage 5.1 VCT cost → upscaling directly reduces
- `2026-06-21-lod-mesh-downsampling` — LOD geometry reduction = orthogonal cost axis
- `2026-06-21-gpu-procedural-noise-compute-kernels` — world gen async = upscaling overlap candidate
- `2026-06-20-dec-pipelines-async-compute` — async compute = upscaling async pass candidate
- `2026-06-20-nanovdb-on-gpu` — VCT cone-march cost = primary upscaling target

## Coverage gap rationale

**Render-target post-process upscaling axis** = **0 of 30+ closed experiments covered this** (per `INDEX.md §6 Recent closed sessions` + `research/backlog.md §Closed`). Coverage map:

- Stage 0: toolchain (dxc-vs-glslc) — closed (different axis)
- Stage 1.x: storage (svdag, nanovdb) — closed (different axis)
- Stage 2.1: HZB cull — closed (different axis)
- Stage 2.2: depth quantization — closed (different axis)
- Stage 2.3: sparse VT — closed (different axis)
- Stage 3.1: GPU Fluid CA atomic — in-progress (different axis)
- Stage 4.1: GPU noise + WFC + sub-chunk-layers — closed (different axes)
- Stage 4.2: LOD — closed (different axis)
- Stage 5.1: VCT cutoff + cone count — closed + in-progress (different axes)
- Stage 5.2: RTX shadows — closed (different axis)
- Stage 5.3: TAA motion vectors — closed (different axis — but motion vector MRT = upscaling input)
- Stage 6.x: ECS + PIMPL + frame-flight — closed (different axes)
- Profiling: tracy-gpu-vs-manual — in-progress (different axis)
- Audio: raytracing + diffraction — closed + in-progress (different axis)
- Clustered lighting, FXAA, vis-buffer, mass-lights — closed (different axes)
- **THIS EXPERIMENT = render-target post-process upscaling axis = NEW**
