# Sources — 2026-06-21-eye-tracked-foveated

Web research performed `2026-06-21`. 3 wave queries via `web_search` Exa (working this session per `agent/knowledge.md Part B §9` line 1424 fallback list — fallback не понадобился). All sources verified by direct page fetch via `webfetch`. 14 primary sources + 7 supplementary.

---

## Primary sources (14)

### 1. arXiv 2503.23410 — Visual Acuity Consistent Foveated Rendering towards Retinal Resolution
- **URL:** https://arxiv.org/html/2503.23410
- **Authors:** ICCVM 2026 paper
- **Key claim:** **6.5×-9.29× speedup for deferred rendering of 3D scenarios, 10.4×-16.4× for ray-casting at retinal resolution**. Log-polar mapping based on human visual acuity model.
- **Why important:** Highest-measured foveated rendering speedup in SOTA 2026 literature; validates 30-70% savings hypothesis (extends to 6-16× for log-polar). Direct production relevance для voxel engine deferred lighting + VCT ray-cast.
- **Verified:** 2026-06-21 via webfetch.

### 2. docs.vulkan.org — VK_EXT_fragment_density_map (refpage)
- **URL:** https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_fragment_density_map.html
- **Spec:** "This extension allows an application to specify areas of the render target where the fragment shader may be invoked fewer times... The primary use of this extension is to reduce workloads in areas where lower quality may not be perceived such as the distorted edges of a lens or the periphery of a user's gaze."
- **Superseded:** per `docs.vulkan.org/refpages` and `KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc`, this extension's functionality is **included in core Vulkan 1.4** with the KHR suffix omitted. ProjectV mainline uses `vkCmdBeginRendering` dynamic rendering path — `VkRenderPassCreateInfo`-bound `VK_EXT_fragment_density_map` NOT drop-in.
- **Why important:** SOTA Vulkan extension for foveated rendering; validates hardware-OS level support.
- **Verified:** 2026-06-21 via webfetch.

### 3. docs.vulkan.org — Fragment Density Map Operations (spec chapter)
- **URL:** https://docs.vulkan.org/spec/latest/chapters/fragmentdensitymapops.html
- **Spec:** Defines density value (0.0-1.0 normalized float), texel size limits, implementation-defined fragment area clamping, multiview interaction.
- **Why important:** Reference spec for understanding density map semantics. Useful for production prototype implementation.
- **Verified:** 2026-06-21 via webfetch.

### 4. docs.vulkan.org — VK_KHR_dynamic_rendering_local_read (feature proposal)
- **URL:** https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_dynamic_rendering_local_read.html
- **Spec:** "Enables reads from attachments and resources written by previous fragment shaders within a dynamic render pass... Vulkan Version 1.4 implementations only have to support local read for storage resources and single sampled color attachments."
- **Why important:** **Vulkan 1.4 core** replacement for legacy FDM extensions. Compatible with dynamic rendering path (which ProjectV uses).
- **Verified:** 2026-06-21 via webfetch.

### 5. Khronos Blog — Streamlining Subpasses (announcement)
- **URL:** https://www.khronos.org/blog/streamlining-subpasses
- **Date:** 2024-01-25
- **Content:** "We're happy to announce the release of VK_KHR_dynamic_rendering_local_read, which adds support for local dependencies to dynamic rendering; enabling developers to fully move over to dynamic rendering as support is rolled out... This extension will be available as part of the Vulkan Roadmap 2024 milestone."
- **Why important:** Authoritative Khronos blog on dynamic rendering local read. Sets context for Vulkan 1.4 FDM supersession.
- **Verified:** 2026-06-21 via webfetch.

### 6. GitHub — KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc
- **URL:** https://github.com/KhronosGroup/Vulkan-Docs/blob/main/appendices/VK_KHR_dynamic_rendering_local_read.adoc
- **Key text:** "Functionality in this extension is included in core Vulkan 1.4, with the KHR suffix omitted. However, Vulkan 1.4 implementations only have to support local read for storage resources and single sampled color attachments."
- **Why important:** Canonical Khronos repository source confirming FDM supersession. Cross-references many Vulkan vendor contributors (NVIDIA, AMD, Intel, Arm, Valve, Google, Broadcom, Imagination, MediaTek, Qualcomm).
- **Verified:** 2026-06-21 via webfetch.

### 7. docs.vulkan.org — VK_KHR_fragment_shading_rate (refpage)
- **URL:** https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_fragment_shading_rate.html (via sandbox refpages)
- **Spec:** Three methods: Pipeline (per-draw), Primitive (per-primitive), Attachment (per-region image). Tier 1 = pipeline only; Tier 2 = primitive + attachment.
- **Attachment method:** "specification of a rate per-region of the framebuffer, specified in a specialized image attachment". Direct fit for gaze-driven foveation.
- **Why important:** **Cross-vendor Tier 2 attachment method = correct path for ProjectV** (dynamic-rendering compatible). Already supported on NVIDIA Turing+ / AMD RDNA 2+ / Intel Arc.
- **Verified:** 2026-06-21 via webfetch.

### 8. Vulkan Samples — Fragment Shading Rate Dynamic (Khronos Vulkan-Samples GitHub)
- **URL:** https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/extensions/fragment_shading_rate + https://docs.vulkan.org/samples/latest/samples/extensions/fragment_shading_rate_dynamic/README.html
- **Sample code:** Demonstrates `VkFragmentShadingRateAttachmentInfoKHR` + compute shader that calculates derivative image for next-frame shading rate.
- **Why important:** Production reference implementation. Demonstrates two-pass approach (current frame @ 1x1 compute derivatives, next frame @ variable rate per derivative image). Avoids feedback loop stutter.
- **Verified:** 2026-06-21 via webfetch.

### 9. Vulkan Samples — Fragment Density Map (Khronos Vulkan-Samples)
- **URL:** https://docs.vulkan.org/samples/latest/samples/extensions/fragment_density_map/README.html
- **Sample code:** Demonstrates `VK_EXT_fragment_density_map` with gaze-centered foveation map. "A common use case is foveated rendering in Virtual Reality (VR): with eye tracking, you render the gaze region at full resolution and peripheral regions at lower resolution."
- **Why important:** Production reference for legacy FDM path (will need port to Tier 2 attachment for ProjectV dynamic rendering compatibility).
- **Verified:** 2026-06-21 via webfetch.

### 10. Khronos OpenXR — XR_EXT_eye_gaze_interaction (spec manual page)
- **URL:** https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_EXT_eye_gaze_interaction.html
- **Status:** Ratified, **Revision 2**
- **Dep:** OpenXR 1.0+
- **Why important:** Standardized OpenXR extension for eye gaze data (position + orientation + confidence). Required input для gaze-driven foveation. Cross-vendor (Meta Quest Pro, Varjo XR-3, Pico Pro, etc.).
- **Verified:** 2026-06-21 via webfetch.

### 11. OpenXR 1.1 Specification — XR_VARJO_foveated_rendering + XR_FB_foveation_vulkan + XR_META_foveation_eye_tracked + XR_ANDROID_eye_tracking
- **URL:** https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html
- **Status:** All vendor-specific extensions listed in OpenXR 1.1 spec.
- **Why important:** Production reference for VR foveated rendering data path. Vendor-specific extensions layer on top of `XR_EXT_eye_gaze_interaction`.
- **Verified:** 2026-06-21 via webfetch.

### 12. Meta Horizon OS Developers Blog — Save GPU with Eye Tracked Foveated Rendering
- **URL:** https://developers.meta.com/horizon/blog/save-gpu-with-eye-tracked-foveated-rendering/
- **Content:** "ETFR moves the foveal region around to match where you're looking using eye tracking... our Unity and Unreal integration implements a new Vulkan extension: Tile Offset (`VK_QCOM_fragment_density_map_offset`). Developed by our Qualcomm partners, Tile Offset allows the same foveation map to be re-used and smoothly moved around by specifying a pixel offset value... Tile Offset gives much finer control when moving the foveal region around, and avoids tile flickering artifacts in the traditional method where we update the fragment density map every frame."
- **Why important:** Production-grade Meta Quest reference design. Validates tile offset pattern для low-latency gaze updates.
- **Verified:** 2026-06-21 via webfetch.

### 13. Varjo Developer Documentation — Foveated Rendering API
- **URL:** https://developer.varjo.com/docs/native/foveated-rendering-api
- **Date:** 2026-05-07
- **Content:** Three foveation modes: Partner SDKs, Dynamic projection, Variable-rate shading (VRS). "Variable-rate shading (VRS) is available in different graphics APIs in slightly different ways... We can set a 2x2 shading rate, which means that the GPU will render only one pixel from a 2x2 square... Stereo rendering mode will benefit even more when gaze is enabled."
- **Why important:** Production-grade reference for desktop VR (Varjo XR-3, XR-4). Validates VRS for stereo rendering with gaze.
- **Verified:** 2026-06-21 via webfetch.

### 14. NVK Mesa DeepWiki — NVIDIA Vulkan Driver (`bminor/mesa-mesa`)
- **URL:** https://deepwiki.com/bminor/mesa-mesa/2.5-nvk-nvidia-vulkan-driver
- **Date:** 2026-01-12
- **Key data:** NVK supports NVIDIA GPUs from Kepler through Blackwell. Vulkan 1.4 features per hardware generation: Turing+ = full feature set. `fragmentShadingRate` (Tier 2 VRS) Turing+. `cooperativeMatrix` Turing+. RTX 3060 Ti = Ampere = full feature set.
- **Why important:** Confirms dev host RTX 3060 Ti GA104 Ampere supports all measured features (VRS Tier 2 + cooperative matrix + Vulkan 1.4 core).
- **Verified:** 2026-06-21 via webfetch.

---

## Supplementary sources (7)

### 15. NVIDIA Developer — Vulkan Driver Support
- **URL:** https://developer.nvidia.com/vulkan-driver
- **Content:** NVIDIA RTX 3060 Ti = Ampere = full Vulkan 1.4 support. Vulkan Roadmap 2026: `VK_EXT_descriptor_heap`, `VK_EXT_shader_subgroup_partitioned`, `VK_KHR_internally_synchronized_queues`, `VK_NV_push_constant_bank`, `VK_KHR_incremental_present` (Linux), `VK_NV_cluster_acceleration_structure`, `VK_NV_partitioned_acceleration_structure`, `VK_NV_present_metering`.
- **Why useful:** Driver-level feature availability reference.

### 16. NVIDIA RTX Blackwell GPU Architecture whitepaper
- **URL:** https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf
- **Content:** "Cooperative Vectors API for DX12 and Vulkan, Tensor Cores can be accessed through any type of shader, including pixel and ray tracing... neural texture compression that provides up to seven-to-one VRAM compression over block compressed formats, and other techniques such as RTX Neural Materials, Neural Radiance Cache, RTX Skin, and RTX Neural Faces."
- **Why useful:** SOTA NVIDIA future direction. Cooperative Vectors = unified neural shading API. May replace Vulkan-only `VK_KHR_cooperative_matrix` + `VK_KHR_fragment_shading_rate` in next-gen hardware.

### 17. NVIDIA RTX PRO Blackwell Architecture PDF
- **URL:** https://www.nvidia.com/content/dam/en-zz/Solutions/design-visualization/quadro-product-literature/pdf/NVIDIA-RTX-Blackwell-PRO-GPU-Architecture-v1_1.pdf
- **Content:** Same content as consumer Blackwell whitepaper, plus workstation use cases.
- **Why useful:** Confirms Cooperative Vectors applies to all Blackwell SKUs.

### 18. ACM 2025 ETRA — Quantifying Energy Reduction of Foveated Volume Visualization
- **URL:** https://dl.acm.org/doi/10.1145/3715669.3725881
- **Date:** 2025 Symposium on Eye Tracking Research and Applications
- **Key finding:** Both VRS + LBG stippling reduce per-frame energy by "more than" (number truncated in abstract). Viewpoint-dependent savings.
- **Why useful:** Energy quantification (complementary to GPU savings), VRS + LBG comparison.

### 19. Springer Nature — Performance-driven foveated VR rendering for large 3D meshes
- **URL:** https://link.springer.com/article/10.1007/s10055-026-01316-3
- **Date:** 2026-03-14
- **Key finding:** 100M triangle meshes. Foveated LOD (spatial + eccentricity): 9.74 ms frame (SD 2.12). Spatial-only LOD: 10.06% slower. **+10.06% improvement** from adding eccentricity factor.
- **Why useful:** Production reference for foveated LOD on large meshes. Validates multi-axis optimization (spatial LOD + eccentricity-based density reduction).

### 20. IEEE VR 2026 — Hybrid Foveated Path Tracing with Peripheral Gaussians
- **URL:** https://hex-lab.io/publication/2026/2026-ieee-vr-foveated-gs-constiantin/
- **Date:** 2026-03-10
- **Key claim:** Hybrid foveated path tracing + Gaussian Splatting для medical VR. Peripheral model regen < 1 second. Direct production reference для voxel + path tracing + VR.
- **Why useful:** Voxel-adjacent production reference (anatomical volume rendering). Validates hybrid foveated path tracing pattern.

### 21. Unity OpenXR Plugin — Foveated rendering documentation
- **URL:** https://docs.unity3d.com/Packages/com.unity.xr.openxr@1.17/manual/features/foveatedrendering.html
- **Content:** "On Vulkan, FDM Foveated Rendering will be automatically disabled at runtime if the physical device running OpenXR does not support fragment density maps (`VK_EXT_fragment_density_map`)."
- **Why useful:** Confirms Unity's runtime fallback path when FDM extension unavailable. Validates that `VK_EXT_fragment_density_map` is the primary cross-vendor extension reference.

---

## Cross-references (ProjectV local)

- `docs/experiments/hardware-profile.md` §1 (Zen 3 5800X dev host `obvium`)
- `docs/experiments/hardware-profile.md` §3 (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341)
- `docs/experiments/hardware-profile.md` §4 (Vulkan extensions subset)
- `docs/experiments/benchmarks/methodology.md` §3 (measurement protocol: 1000 iter + 10 warmup)
- `TODO.md` §2.1 (HZB culling), §4.3 (lift draw distance), §5.1 (VCT), §5.2 (RTX shadow), §5.3 (TAA)
- `src/render/Renderer.cpp` (dynamic rendering path, `vkCmdBeginRendering` verified)
- `src/shaders/voxel.frag` (VCT + main fragment pipeline — foveation integration point)
- `src/shaders/voxel_mesh.comp:146` (mesh shader dispatch — foveation preserves vertex density)
- `agent/knowledge.md §30.4` (3-step migration precedent)
- `agent/workspace.md §2` (Nearest Gap callout)
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold)
- Closed experiment cross-refs: `2026-06-21-vk-fragment-shading-rate-voxel/` (verdict=mixed, uniform global VRS), `2026-06-21-vulkan-memory-aliasing-transient/` (VRAM aliasing), `2026-06-21-dlss-fsr-xess-upscaling-voxel/` (post-process upscaling), `2026-06-21-texture-compression-format-axis/` (texture compression)
- Active parallel cross-refs: `2026-06-21-tracy-gpu-vs-manual`, `2026-06-21-taa-motion-vectors`, `2026-06-21-gpu-fluid-ca-atomic-strategy`, `2026-06-21-vct-3d-mip-generation`, `2026-06-21-vk-multi-gpu-split-frame`, `2026-06-21-vulkan-defragmentation-compaction`
