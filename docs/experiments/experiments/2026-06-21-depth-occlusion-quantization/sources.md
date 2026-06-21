# Sources — `2026-06-21-depth-occlusion-quantization`

Полный список источников, верифицированных в `README.md §2` + дополнительные cross-refs.

## A. Vulkan format specification (authoritative)

1. **KhronosGroup/Vulkan-Guide** — `chapters/depth.adoc` (2024+, f4745cfa commit).
   <https://github.com/KhronosGroup/Vulkan-Guide/blob/f4745cfa/chapters/depth.adoc>
   **Why:** format support matrix — `D16_UNORM` required для sampled/blit; `D32_SFLOAT` required для attachment; `X8_D24_UNORM_PACK32` + `D24_UNORM_S8_UINT` optional. Foundation для hypothesis #1 (D32 → D16).

2. **KhronosGroup/Vulkan-LoaderAndValidationLayers** — `layers/vk_format_utils.cpp` (2024+).
   <https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/layers/vk_format_utils.cpp>
   **Why:** texel block size reference table — concrete byte sizes: `D16_UNORM=2`, `X8_D24_UNORM_PACK32=4`, `D32_SFLOAT=4`, `D24_UNORM_S8_UINT=4`, `D32_SFLOAT_S8_UINT=8`. **Critically:** `X8_D24_UNORM_PACK32` is **4 bytes per texel** (pack32 = 32-bit word), NOT a 3-byte compression.

3. **KhronosGroup/Vulkan-Utility-Libraries** — `vku_format_utils.h` (2024+).
   <https://github.com/KhronosGroup/Vulkan-Utility-Libraries/blob/main/include/vulkan/utility/vk_format_utils.h>
   **Why:** texel block size + depth size helpers. Cross-check: `VKU_FORMAT_COMPATIBILITY_CLASS_D24` = 4 bytes, 1 texel.

4. **vkdoc.net** — `chapters/formats`.
   <https://vkdoc.net/chapters/formats>
   **Why:** clear table — "D24 Block size 4 byte, 1x1x1 block extent, 1 texel/block" для `X8_D24_UNORM_PACK32`. **Disproves** my initial hypothesis that `X8_D24_UNORM_PACK32` saves 25% VRAM (it doesn't — same 4 bytes/texel as D32).

## B. Depth precision research (SOTA 2010–2026)

5. **Nathan Reed "Visualizing Depth Precision"** (NVIDIA Technical Blog, 2021-10-21).
   <https://developer.nvidia.com/blog/visualizing-depth-precision/>
   **Why:** **Foundational reverse-Z analysis.** Quote: "Reversed-Z with a float depth buffer gives a zero error rate in this test. ...just use a floating-point depth buffer with reversed-Z! And if you can't use a floating-point depth buffer, you should still use reversed-Z."
   **Methodology:** 32-bit float + reversed-Z = best; 24-bit integer = "as good as any of the other integer options" с reversed-Z; 16-bit float = worst.

6. **MJP "Attack of the depth buffer"** (2010-03-23).
   <https://therealmjp.github.io/posts/attack-of-the-depth-buffer/>
   **Why:** SOTA-baseline 2010 test of all depth formats. Quote: "16-bit float... easily the worst format out of everything I tested. ...don't use this!"; 24-bit = "this isn't terrible, and a lot of people have shipped awesome-looking games with this format". **Caveat:** 2010 era, no reverse-Z in tests — modern analysis (Reed 2021) supersedes.

7. **Upchurch & Desbrun "Tightening the Precision of Perspective Rendering"** (Caltech 2012, PDF).
   <https://www.geometry.caltech.edu/pubs/UD12.pdf>
   **Why:** infinite projection theoretical analysis. Quote: "infinite projection is a more precise general purpose projection and the finite projection is only useful when the depth range ratio is very small or extremely high precision is needed at the near plane."

8. **DLR "Comparison of Depth Buffer Techniques for Large and Detailed 3D Scenes"**.
   <https://elib.dlr.de/187280/1/Comparison%20of%20Depth%20Buffer%20Techniques%20for%20Large%20and%20Detailed%203D%20Scenes.pdf>
   **Why:** comparison of standard / reversed / reversed-infinite projection + float / fixed-point. Quote: "reversed and reversed infinite projections with 32-bit floating-point depth buffers... best overall distribution of precision from all tested methods."

9. **Zero Radiance "Quantitative Analysis of Z-Buffer Precision"** (2020-08-24).
   <https://zero-radiance.github.io/post/z-buffer/>
   **Why:** analytical depth precision analysis + infinite far plane precision. Quote: "reversed floating-point Z-buffer is an approximation of a logarithmic depth buffer."

10. **ImgTec "Optimal Depth Buffer Usage for Large-scale Games"**.
    <https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/optimal-depth-buffer-usage-for-large-scale-games.html>
    **Why:** `GL_EXT_clip_control` reverse-Z recommendation: "With this method, the usual `D24S8` format may be enough for most games. For even more precision, use a `D32F` with a separate stencil buffer." Direct cross-vendor reverse-Z implementation reference.

## C. D16 known artifacts

11. **doitsujin/dxvk PR #5564** (2026-03-25).
    <https://github.com/doitsujin/dxvk/pull/5564>
    **Why:** **D16 shadow map banding/moiré artifacts** на Vulkan. Quote: "Vulkan's `VK_FORMAT_D16_UNORM` provides exactly 16 bits of depth precision. D3D11 and OpenGL drivers likely use higher internal precision for D16 depth buffers, as the OpenGL spec explicitly allows implementations to use higher bitdepth than requested for internal formats."
    **Caveat:** DXVK promotes D16 to D32 при `D3D11_BIND_DEPTH_STENCIL` + `D3D11_BIND_SHADER_RESOURCE` (shadow map + PCF). Direct validation of D16 = insufficient precision для shadow map PCF.

12. **GPUOpen-LibrariesAndSDKs/FidelityFX-SDK Issue #90** (2024-08-30).
    <https://github.com/gpuopen-librariesandsdks/fidelityfx-sdk/issues/90>
    **Why:** D32_SFLOAT_S8_UINT depth buffer incompatibility с FidelityFX FSR3.1 Vulkan backend. Indirect: cross-vendor depth format preference = D32_SFLOAT (industry standard).

13. **neo-veldrid issue #58 "Split color and depth pixel formats"** (2024+).
    <https://github.com/jhm-ciberman/neo-veldrid/issues/58>
    **Why:** cross-API depth format support matrix: `D16_UNORM` / `D32_Float` = universal (Desktop + Metal + WebGPU); `D24_UNorm_S8_UInt` = limited на Metal (absent on Apple GPUs).

## D. HZB mip chain (foundational)

14. **Arm "Occlusion Culling with Hierarchical-Z"** (Arm Software, OpenGL ES SDK, 2023+).
    <https://arm-software.github.io/opengl-es-sdk-for-android/occlusion_culling.html>
    **Why:** HZB mip chain pattern reference, `textureGather` 4-tap quad fetch for max reduction. Compute shader implementation.

15. **RasterGrid "Hierarchical-Z map based occlusion culling"** (2010-10-01).
    <https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/>
    **Why:** HZB construction "takes less than 0.2 milliseconds and the actual culling comes at almost no cost". SOTA reference для occlusion culling cost.

16. **Mike Turitzin "Hierarchical Depth Buffers"** (2023).
    <https://miketuritzin.com/post/hierarchical-depth-buffers/>
    **Why:** HZB mip chain construction, `atomicMin` + 4x4 workgroup pattern. Modern GLSL compute shader example.

17. **Luc Momber "Two-Pass Hierarchical Z-Buffer Occlusion Culling"** (Medium 2025-04-01).
    <https://medium.com/@Lucmomber/two-pass-hierarchical-z-buffer-occlusion-culling-93171c5a9808>
    **Why:** современный HZB pattern + reprojection + visibility buffer. Direct reference для ProjectV HZB cull shader.

18. **Vkguide.dev "Compute based Culling"** (2024+).
    <https://www.vkguide.dev/docs/gpudriven/compute_culling/>
    **Why:** `textureLod(depthPyramid, ...)` mip-level selection via `log2(max(width, height))`. **Note:** this guide uses `textureLod` (not `texelFetch`) per `foijord/vk-textureLod-repro` 2026 bug — bindless-unsafe per `hzb-binding-models` (closed verdict=mixed).

19. **Mesa "Hierarchical Depth (HiZ)"** (latest documentation).
    <https://docs.mesa3d.org/isl/hiz.html>
    **Why:** HW HZB implementation details per vendor (Intel Sandy Bridge + ISL layout). Notes: "hierarchical depth buffer does not support the LOD field, it is assumed by hardware to be zero" — important для mip-level interaction with HZB.

## E. Conservative rasterization (alternative to HZB)

20. **VkPhysicalDeviceConservativeRasterizationPropertiesEXT** (Vulkan Spec, 2024+).
    <https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceConservativeRasterizationPropertiesEXT.html>
    **Why:** extension spec — `conservativeRasterizationPostDepthCoverage` (SPV_KHR_post_depth_coverage), `primitiveUnderestimation`, `fullyCoveredFragmentShaderInputVariable` (SPV_EXT_fragment_fully_covered).

21. **natillum dP "Efficient GPU-based occlusion culling via early-Z and indirect dispatch"** (2024+).
    <https://natillum.com/en/article/27/efficient-gpu-based-occlusion-culling-via-early-z-and-indirect-dispatch>
    **Why:** **conservative rasterization tier limitations** (Maxwell 2.0 = tier 1 only) + post-depth-coverage pattern + early-Z. Direct alternative to HZB mip chain.

22. **Bittner J. "Hierarchical Raster Occlusion Culling"** (CGF 39(2) 2020).
    <https://onlinelibrary.wiley.com/doi/10.1111/cgf.142649>
    **Why:** "scalable online occlusion culling algorithm, which significantly improves the previous raster occlusion culling using object-level bounding volume hierarchy". Production reference.

23. **NVIDIA GPU Gems 2 Ch 6 "Hardware Occlusion Queries Made Useful"** (2005, Bittner/Wimmer).
    <https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-6-hardware-occlusion-queries-made-useful>
    **Why:** foundational CHC++ algorithm. Modern replacement = HZB mip chain (ProjectV current path).

## F. Tile-based occlusion culling (alternative, parked)

24. **Fyrox "Tile-based Occlusion Culling"** (2024).
    <https://fyrox.rs/blog/post/tile-based-occlusion-culling/>
    **Why:** R32UI bitmask per tile pattern, 32 objects per pixel + readback collapse. Alternative to HZB, but readback cost = CPU stall. Parked для follow-up.

## G. TBR + Vulkan best practices (cross-cutting)

25. **Vulkan-Guide "Tile Based Rendering (TBR) Best Practices"** (2024+).
    <https://docs.vulkan.org/guide/latest/tile_based_rendering_best_practices.html>
    **Why:** `VK_ATTACHMENT_LOAD_OP_CLEAR` vs `vkCmdClearAttachments`, `VK_KHR_dynamic_rendering_local_read`, HSR + early-Z hardware depth culling, subpass merging. Cross-cutting для depth/occlusion.

26. **Vulkanised 2023 Mesh Shading Best Practices**.
    <https://vulkan.org/user/pages/09.events/vulkanised-2023/vulkanised_mesh_best_practices_2023.02.09-1.pdf>
    **Why:** task shader per-meshlet culling pattern + vendor preferences (NVIDIA + AMD RDNA). Alternative to HZB для mesh shader path.

27. **Vulkan samples "Mesh Shader Culling"** (2024+).
    <https://docs.vulkan.org/samples/latest/samples/extensions/mesh_shader_culling/README.html>
    **Why:** task+mesh cull pattern. Cross-reference для `mesh-shader-vs-compute-cull` (closed verdict=mixed).

## H. Vulkan Tutorial + reference (baseline)

28. **Vulkan Tutorial "Depth buffering"** (Overvoorde).
    <https://vulkan-tutorial.com/Depth_buffering>
    **Why:** baseline reference для `findDepthFormat()` pattern. Direct cross-reference для ProjectV `src/render/SceneResources.cpp`.

29. **Khronos Vulkan Hardware Database** (Sascha Willems).
    <http://www.vulkan.gpuinfo.org/listpropertiesextensions.php>
    **Why:** extension coverage statistics — `VK_KHR_depth_stencil_resolve` (36.32% coverage), `VK_KHR_maintenance5` (17.09% coverage с `depthStencilSwizzleOneSupport`). Cross-vendor cross-check.

## I. ProjectV cross-references

30. **ProjectV codebase** — `src/render/Renderer.cpp:290-297` (standard-Z clear), `src/render/HizCulling.{hpp,cpp}`, `src/shaders/hzb_cull.comp`, `src/render/SceneResources.cpp`, `src/render/ShadowProjection.cpp:13` (kShadowDepthPadding).
    <https://github.com/.../ProjectV> (local: `/home/le1t/Projects/ProjectV/`)

31. **`agent/knowledge.md §30.4`** — 3-step migration precedent (XS foundation / S integration / M default flip).

32. **`hardware-profile.md §3+§4`** — dev host `obvium` (RTX 3060 Ti GA104 Ampere, 8 GiB VRAM, Vulkan 1.4.341, NVIDIA 610.43.02) + relevant extensions.

33. **`TODO.md §2.1+§2.2`** — HZB cull + mesh shader feature-flagged path.

34. **Closed experiments**: `2026-06-20-hzb-binding-models` (verdict=mixed, texelFetch pattern), `2026-06-20-bindless-descriptor-overhead` (verdict=mixed, Phase A shadow cascade VRAM), `2026-06-20-frame-flight-allocator-budget` (verdict=mixed, VRAM budget = 5.06 GiB на 8 GiB hardware), `2026-06-20-mesh-shader-vs-compute-cull` (verdict=mixed, Pattern A compute cull = default).

## J. Why this experiment is novel (not duplicate)

- **hzb-binding-models** (closed): pattern decision (texelFetch vs textureLod), **NOT format**. Does not address D32 vs D16.
- **frame-flight-allocator-budget** (closed): allocator strategy (transient ring buffer), **NOT depth format**. VRAM budget context only.
- **bindless-descriptor-overhead** (closed): descriptor model (bindless vs traditional+dynamic-offset), **NOT depth format**. Shadow cascade depth-bound = motivation only.
- **mesh-shader-vs-compute-cull** (closed): compute cull vs mesh shader, **NOT depth format**. Alternative rendering path, not format optimization.
- **vulkan-fps-pacing-vk-ext** (closed): frame-pacing via present_timing, **NOT depth**. Cross-cutting timing, not depth precision.

**Novel axis:** **depth format precision vs VRAM** (D32 vs D16 vs D24 packed vs mixed HZB). Uncovered in 24+ closed experiments of `2026-06-20` + `2026-06-21`.

---
