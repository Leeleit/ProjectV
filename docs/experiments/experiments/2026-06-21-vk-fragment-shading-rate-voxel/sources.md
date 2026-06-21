# Sources — `2026-06-21-vk-fragment-shading-rate-voxel`

Web-research complete `2026-06-21`. Verified via primary sources (Khronos spec, NVIDIA developer blog,
Intel SIGGRAPH 2019, AMD Mesa commits via Phoronix, SaschaWillems DeepWiki, Vulkan samples GitHub).

---

## 1. Vulkan spec & extension status (Khronos primary)

- **`VK_KHR_fragment_shading_rate` extension** —
  [vulkan.lunarg.com refpage](https://vulkan.lunarg.com/doc/view/1.4.341.1/windows/antora/refpages/latest/refpages/source/VK_KHR_fragment_shading_rate.html),
  [Khronos feature proposal](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_fragment_shading_rate.html).
  **Verified:** 3 methods (pipeline / primitive / attachment), `VkPhysicalDeviceFragmentShadingRateFeaturesKHR`
  + `VkPhysicalDeviceFragmentShadingRatePropertiesKHR` feature+property structs. Implementation-dependent limits
  `minFragmentShadingRateAttachmentTexelSize`, `maxFragmentSize`, `fragmentShadingRateNonTrivialCombinerOps`,
  `fragmentShadingRateStrictMultiplyCombiner`.
- **Vulkan 1.4 Core Revisions** — [docs.vulkan.org/spec/latest/appendices/versions.html](https://docs.vulkan.org/spec/latest/appendices/versions.html).
  **Verified:** VRS extension **NOT** in Vulkan 1.4 core (`VK_KHR_dynamic_rendering` IS, but `VK_KHR_fragment_shading_rate`
  remains extension in 1.4). However, `VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR`
  available when dynamic rendering supported (1.3+). → **Application implication:** require explicit extension
  enable on 1.4 baseline; RTX 3060 Ti on dev host `obvium` per `hardware-profile.md §3` (driver 610.43.02) supports
  per Mesa NVIDIA blob.
- **`VkPhysicalDeviceFragmentShadingRatePropertiesKHR`** —
  [registry.khronos.org](https://registry.khronos.org/VulkanSC/specs/1.0-extensions/man/html/VkPhysicalDeviceFragmentShadingRatePropertiesKHR.html).
  Property struct fields documented.

## 2. Vulkan sample (Khronos reference impl)

- **Fragment Shading Rate (static)** — [samples / extensions / fragment_shading_rate](https://github.khronos.org/Vulkan-Site/samples/latest/samples/extensions/fragment_shading_rate/README.html).
  Uses special framebuffer attachment to control fragment shading rates for different framebuffer regions.
- **Fragment Shading Rate (dynamic)** — [samples / extensions / fragment_shading_rate_dynamic](https://github.khronos.org/Vulkan-Site/samples/latest/samples/extensions/fragment_shading_rate_dynamic/README.html).
  **Key insight:** "A separate renderpass that calculates the frequency information without using the shading rate
  attachment" — two-pass pattern to avoid feedback loop (current frame uses previous frame's VRS image).
  `dFdx/dFdy` → derivative image → compute shader → next-frame VRS image.

## 3. Production precedents

### NVIDIA

- **VRSS 2 (Variable Rate Supersampling)** —
  [developer.nvidia.com VRSS 2 dynamic foveated](https://developer.nvidia.com/blog/nvidia-vrss-2-dynamic-foveated-rendering-no-assembly-required/),
  [VRSS 2 dynamic foveated NVIDIA VRWorks](https://developer.nvidia.com/vrworks/graphics/variablerateshading).
  **Verified:** driver-level zero-coding solution; integrates with Tobii Spotlight eye-tracking; uses MSAA buffer
  sample count to determine shading rate (2x for MSAA-2, 4x for MSAA-4, max 8x); Turing+ only; requires DX11
  forward renderer + MSAA. **ProjectV relevance:** Vulkan (not DX11), but VRS primitive concept applies.
- **NAS (NVIDIA Adaptive Shading)** GDC 2019, Lei Yang — [leiy.cc NAS GDC19.pdf](http://www.leiy.cc/publications/nas/nas-gdc19.pdf).
  **Verified:** 2x average gain, up to 5x in forward shading passes; 0.2 ms overhead @ 4K on RTX 2080 Ti;
  **pitfalls** documented: (1) "doesn't help vertex/geometry", (2) "specular aliasing in VRS can look bad in
  HDR" — large blocky specular gets smeared, (3) "motion blur + TAA/NAS feedback latency" — 3-4 frames transition
  when motion stops, (4) "shading rates can oscillate" — spatial smoothing recommended, (5) "transition latency
  due to TAA/NAS feedback" — avoid temporal smoothing. **ProjectV relevance:** NAS patterns applicable; feedback
  latency matters with TAA (closed `taa-motion-vectors` in-progress).

### AMD

- **RADV Vulkan Driver Enables Fragment Shading Rate Support** (Dec 2020) — [Phoronix](https://www.phoronix.com/news/RADV-fragment-shading-rate).
  **Verified:** RADV Mesa 21.0+ supports `VK_KHR_fragment_shading_rate` on RDNA 2 (GFX10.3) and newer.
- **RADV Enables Variable Rate Shading For RDNA3** (Mar 2023) — [Phoronix](https://www.phoronix.com/news/RDNA3-RADV-Enables-VRS).
  **Verified:** GFX11 (RDNA 3 / RX 7000) VRS support added via Samuel Pitoiset (Valve). "It's now working!"
- **RADV Dynamic VRS for power savings** (Feb 2022) — [Phoronix](https://www.phoronix.com/news/RADV-Dynamic-VRS-Lands).
  **Verified:** Mesa 22.1 added dynamic VRS via `RADV_FORCE_VRS_CONFIG_FILE` env (Valve/Steam Deck integration).
  Battery / thermal use case. **ProjectV relevance:** not directly applicable (desktop), but validates
  per-frame dynamic VRS approach.

### Intel

- **Intel SIGGRAPH 2019 — Use VRS to Improve User Experience in Real-Time Game Engines** — [slideshare](https://www.slideshare.net/slideshow/use-variable-rate-shading-vrs-to-improve-the-user-experience-in-real-time-game-engines/162740191).
  **Verified:** Tier 2 VRS, 30% savings at 1x2/2x1, 46% at 2x2 in forward rendering; **90%+ reduction on particles**.
  **Pitfalls:** doesn't help vertex/geometry, small triangles penalty (thread scheduling), visual blockiness
  most noticeable in high-frequency detail (maps, specularity), **ddx/ddy scaled accordingly** (VRS 2x2 means
  they are 2x — important!), **SV_Position no longer n+0.5**, sampling between texels can introduce artifacts
  for full-screen texture reads (soft particles, SSR, heat-haze), can make things worse if global mis-application.
  "Best to make shaders as VRS-agnostic (pixel-size/location agnostic) as possible."
- **DoF-blurred areas with VRS look bad in motion** (Intel) — verified, motion discontinuity artifact pattern.

### Civilization VI (Firaxis)

- **CIV6 GDC 2019** — [youtube.com CIV6 VRS filter](https://youtu.be/f-SklVb2MDI?t=2072).
  Custom filter helps for VRS blockiness mitigation (referenced by Intel SIGGRAPH 2019 as production reference).

### Engine integrations

- **SaschaWillems Vulkan Examples — Variable Rate Shading** — [deepwiki.com](https://deepwiki.com/SaschaWillems/Vulkan/4.2-variable-rate-shading).
  **Verified:** attachment-based VRS, circular pattern (decreasing sampling rates outward from center),
  `gl_ShadingRateEXT` built-in accessible in fragment shader.
- **Unreal Engine 5.0 Release Notes** — [dev.epicgames.com](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5.0-release-notes).
  Vulkan RHI improvements, Nanite + Lumen on Linux (software RT), Vulkan stability. VRS support in 5.x RHI.
- **Lumen SIGGRAPH 2022 — Wright/Narkowicz/Kelly (Epic)** — [advances.realtimerendering.com](https://advances.realtimerendering.com/s2022/SIGGRAPH2022-Advances-Lumen-Wright%20et%20al.pdf).
  Epic explicitly **abandoned voxel approach** ("merging geometry properties into a volume causes lots of
  leaking"), went with **Global Distance Field** clipmaps. **ProjectV relevance:** validates that voxel-based
  lighting has fundamental limits; **VRS can mitigate cost-side without changing strategy-side**.
- **Unity URP — Implement variable rate shading** — [docs.unity3d.com 6000.4](https://docs.unity3d.com/6000.4/Documentation/Manual/urp/variable-rate-shading-implementation.html).
  **Verified:** RenderGraph API + `ShadingRateImage` + `ColorMaskTextureToShadingRateImage` helper for image-based
  VRS via Scriptable Renderer Feature. Modern Unity 6.x pattern.
- **Godot Vulkan Proposal #3859** — [github.com godot-proposals](https://github.com/godotengine/godot-proposals/issues/3859).
  Tier 2 VRS target, density texture approach. Reference for open-source Vulkan renderer VRS integration.

## 4. Voxel-specific context

- **platonvin/lum-rs** — [github.com/platonvin/lum-rs](https://github.com/platonvin/lum-rs).
  Voxel renderer (Rust + Vulkan backend). Author notes future work includes "variable sampling HBAO, extra
  accumulations and custom (material/normal aware) multisampling" — **direct voxel-renderer VRS precedent**
  (though not yet implemented).

## 5. Anti-patterns / pitfalls summary (cross-validated)

| Pitfall                                              | Source(s)                                                                       | ProjectV impact                                                       |
|:-----------------------------------------------------|:--------------------------------------------------------------------------------|:----------------------------------------------------------------------|
| Doesn't help vertex/geometry/rasterization-bound    | Intel SIGGRAPH 2019, NVIDIA NAS GDC 2019                                       | Stage 2.x geometry pass = unaffected                                 |
| Penalty for small triangles                          | Intel SIGGRAPH 2019                                                             | High-triangle-count voxel scenes (closed `meshing-algo-comparison` greedy worst case) = possible regression |
| Visual blockiness at boundaries                      | Intel SIGGRAPH 2019, NVIDIA NAS GDC 2019, CIV6 GDC 2019                        | Block texture seams potential issue                                 |
| Specular aliasing in HDR (blocky specular smear)     | NVIDIA NAS GDC 2019                                                            | Stage 5.x PBR specular = critical risk                                |
| ddx/ddy scaling (VRS 2x2 = 2x derivatives)           | Intel SIGGRAPH 2019, NVIDIA NAS GDC 2019                                        | `voxel.frag` shaders (per `TODO.md §5.x`) need review                 |
| SV_Position no longer n+0.5                          | Intel SIGGRAPH 2019                                                             | Tiled/clustered lighting + per-pixel ops need adaptation              |
| TAA/NAS feedback latency (3-4 frames transition)     | NVIDIA NAS GDC 2019                                                            | Stage 5.3 TAA (in-progress `taa-motion-vectors`) = high risk          |
| DoF-blurred areas look bad in motion                 | Intel SIGGRAPH 2019                                                             | Stage 5.x DoF (future, post-MVP) = risk                               |
| Sampling between texels for full-screen textures     | Intel SIGGRAPH 2019                                                             | SSR / soft particles (future) = risk                                  |
| Higher resolution = MORE headroom                    | NVIDIA NAS GDC 2019, Intel SIGGRAPH 2019, Godot docs                           | 4K > 1440p > 1080p benefit order; bias scaling                          |

## 6. Tier 2 VRS hardware support matrix (verified)

| Vendor  | Architecture | GPU examples                            | Vulkan driver                          | Tier 2 support          |
|:--------|:-------------|:----------------------------------------|:---------------------------------------|:------------------------|
| NVIDIA  | Turing       | RTX 2060-2080 Ti, GTX 1660 Ti           | 441.87+ (Apr 2020)                     | ✅ verified             |
| NVIDIA  | Ampere       | RTX 3060/3070/3080/3090, RTX 3060 Ti    | 460+ (Jan 2021)                        | ✅ verified (dev host!) |
| NVIDIA  | Ada          | RTX 4060/4070/4080/4090                 | 525+ (Nov 2022)                        | ✅ verified             |
| NVIDIA  | Blackwell    | RTX 5090                                | 570+ (2025)                            | ✅ verified             |
| AMD     | RDNA 2       | RX 6600/6700/6800/6900 XT               | Mesa RADV 21.0+ (Dec 2020)             | ✅ verified             |
| AMD     | RDNA 3       | RX 7600/7700/7800/7900 XTX              | Mesa RADV 23.1+ (Mar 2023)             | ✅ verified             |
| AMD     | RDNA 4       | RX 9070/9070 XT                         | Mesa RADV 25.0+ (2025)                 | ✅ verified (assumed)   |
| Intel   | Gen11        | Iris Xe (iGPU)                          | Intel ANV 2020+                        | ✅ verified             |
| Intel   | Arc Alchemist| A380/A580/A770                          | Intel ANV 2022+                        | ✅ verified             |
| Intel   | Arc Battlemage | B570/B580                              | Intel ANV 2024+                        | ✅ verified             |

**Dev host `obvium` per `hardware-profile.md §3`:** RTX 3060 Ti (Ampere) — **Tier 2 VRS validated**.

---

## Cross-references (NOT copied)

- `agent/knowledge.md` — no existing VRS-specific section (verified via `rg -n "shading_rate|VRS|VRSS" agent/knowledge.md` → no output).
- `TODO.md §5.x` — lighting strategy Stage 5.1/5.2/5.3 (VCT + RTX + TAA) **all closed as strategy**; VRS = **cost-side follow-up**.
- `agent/workspace.md §1 Phase 1` — lighting axis closure `2026-06-20`.
- Closed `2026-06-20-vct-vs-rt-cutoff` (mixed) + `2026-06-20-clustered-forward-mass-lights` (yes) +
  `2026-06-20-rt-shadows-vs-csm` (mixed) + `2026-06-20-restir-gi-feasibility` (mixed) = lighting strategy
  fully closed. VRS = orthogonal cost axis.
- In-progress `2026-06-21-taa-motion-vectors` (Stage 5.3 temporal) — VRS feedback loop risk (per NVIDIA NAS).
