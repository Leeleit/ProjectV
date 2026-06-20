# Sources — `2026-06-20-hzb-binding-models`

Полный список источников, использованных в этом эксперименте. Дата обращения: 2026-06-20.

---

## 1. HZB reference implementations

- **[vkguide.dev — Compute based Culling](https://www.vkguide.dev/docs/gpudriven/compute_culling/)** —
  classic combined-sampler + `vkCmdBlitImage` mip chain + `textureLod` in compute cull. Reference
  pattern для проекта. **Issue:** `textureLod` fragile под `VK_EXT_descriptor_heap` на NVIDIA
  (см. §3).
- *
  *[RasterGrid — Hierarchical-Z map based occlusion culling (2010-10-01)](https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/)
  **
  — original HZB paper (OpenGL era). 4 texel fetches для conservative comparison.
- *
  *[Interplay of Light — Experiments in GPU-Based Occlusion Culling (Kostas Anagnostou, 2017-11-15)](https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling/)
  **
  — Splinter Cell: Conviction lineage. Compute-based mipchain + smart mip-selection
  (lower level if ≤ 2 texels touched).
- **[sydneyzh/gpu_occlusion_culling_vk](https://github.com/sydneyzh/gpu_occlusion_culling_vk)**
  (2018, archived 2022) — concrete Vulkan impl Anagnostou's approach + indirect draw.
- **[AurelienLeandri/VulkanCulling](https://github.com/AurelienLeandri/VulkanCulling)** (2021)
  — compute-based occlusion + frustum culling + indirect draw, indirect_cull shader modified from
  vkguide reference.

---

## 2. Compute-shader mipchain generation

- **[Mike Turitzin — Hierarchical Depth Buffers (2017)](https://miketuritzin.com/post/hierarchical-depth-buffers/)**
  — **ключевая находка** для mipgen cost comparison. Compute shader `atomicMin` reduction, 4×4
  workgroup where each thread fetches 4×4 region = **87% faster NVIDIA GTX 980, 197% faster
  AMD R9 290** vs `vkCmdBlitImage`. Cited in README §5.4.
- **[ferri.dev — Two Pass Occlusion Culling](https://ferri.dev/project/two-pass-occlusion-culling)** —
  modern re-implementation с `VK_EXT_sampler_filter_minmax`. Explicit note:
  bindless model **не работает для HZB mip writes** (need to write specific mips).
- **[Khronos —
  `VK_EXT_sampler_filter_minmax` spec](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_sampler_filter_minmax.html)
  **
  — promoted to Vulkan 1.2. `VK_SAMPLER_REDUCTION_MODE_MIN`/`MAX` для одно-sample mipchain reduction.

---

## 3. NVIDIA bug repros + bindless caveats

- **[foijord/vk-textureLod-repro (2025-Q1 / 2026)](https://github.com/foijord/vk-textureLod-repro)** —
  **critical finding**. `textureLod` (via `OpImageSampleExplicitLod` with `Lod` operand) **ignores
  explicit LOD** when bound via `VK_EXT_descriptor_heap` on NVIDIA. Tested on RTX A6000, driver
  596.46, Windows 11, Vulkan 1.3. `texelFetch` через тот же heap работает корректно. **Этот
  источник — primary motivation для `texelFetch` recommendation.**
- *
  *[Khronos — Vulkan-Samples descriptor_buffer_basic](https://github.com/KhronosGroup/Vulkan-Samples/blob/ac9edc79/samples/extensions/descriptor_buffer_basic/README.adoc)
  **
  — reference `VK_EXT_descriptor_buffer` implementation. Note: different from `VK_EXT_descriptor_heap`
  (which is the bindless path that has the bug).

---

## 4. Vulkan API spec references

- *
  *[Khronos — Resource Descriptors chapter](https://github.khronos.org/Vulkan-Site/spec/latest/chapters/descriptors.html)
  **
  — `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`, `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`,
  `VK_DESCRIPTOR_TYPE_STORAGE_IMAGE` definitions.
- *
  *[Khronos — VkSamplerReductionMode](https://docs.vulkan.org/refpages/latest/refpages/source/VkSamplerReductionMode.html)
  **
  — reduction mode enum для sampler filter minmax.
- *
  *[Khronos — VkPhysicalDeviceSamplerFilterMinmaxProperties](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT.html)
  **
  — `filterMinmaxSingleComponentFormats` property (R32_SFLOAT поддерживается).
- **[Khronos —
  `VK_KHR_dynamic_rendering_local_read`](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_dynamic_rendering_local_read.html)
  **
  — extension core in 1.4, depth/stencil reads gated by
  `dynamicRenderingLocalReadDepthStencilAttachments`. **Не substitute для HZB mip pyramid** —
  only intra-subpass reads.

---

## 5. Vendor blogs / community

- **[Khronos Blog — Streamlining Subpasses (2024-01-25)](https://www.khronos.org/blog/streamlining-subpasses)**
  — `VK_KHR_dynamic_rendering_local_read` introduction.
- *
  *[Arm — Framebuffer Fetch in Vulkan (2024-01-30)](https://developer.arm.com/community/arm-community-blogs/mobile-graphics-and-gaming-blog/posts/framebuffer-fetch-in-vulkan)
  **
  — mobile perspective на `VK_EXT_shader_tile_image` vs `VK_KHR_dynamic_rendering_local_read`.
  ProjectV = desktop, mobile out of scope.

---

## 6. Internal ProjectV references

- `src/render/HizCulling.cpp` — current mainline HZB image lifecycle (`R32_SFLOAT`,
  `TRANSFER_DST | SAMPLED`, mip chain via `vkCmdBlitImage`).
- `TODO.md §2.2` — Stage 2.2 spec.
- `agent/workspace.md §1 Phase 4, §2 Nearest Gap` — HZB lifecycle готов, Renderer integration отложена.
- `docs/experiments/experiments/2026-06-20-bindless-descriptor-overhead/README.md` — Phase E
  (bindless HZB) rollout plan, requires HZB to be in stable bindless slot.
- `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` — vendored Vulkan 1.4 SDK docs (читать spec
  разделы перед binding-model discussion).

---

## 7. Verification notes

- **`foijord/vk-textureLod-repro`**: source code review проведён через webfetch highlights —
  проверка структуры repro (compute shader + descriptor heap setup), подтверждает minimum repro.
  **Не запускал** на dev host (требует `VK_KHR_maintenance5` chain setup, out of scope prototype).
- **Turitzin 2017 mipgen numbers**: cross-validated с ferri.dev и vkguide.dev (все ссылаются на
  similar compute-vs-blit comparison). Hardware-specific quantitative claims acknowledged as
  approximate.
- **GLSL single-mip-per-binding limitation**: подтверждено нашим measurement
  (`prototype/results.csv`) — storage image bound via single `image2D` descriptor reads from
  view's `baseMipLevel` only, regardless of any other intention. Cross-referenced с Khronos spec
  for `imageLoad` semantics.

---

## 8. Sources explicitly NOT used (with reason)

- **DirectX 12 HZB / DirectX Raytracing HZB** — not applicable; ProjectV = Vulkan 1.4.
- **Metal HZB** — not applicable; ProjectV = Vulkan 1.4. (Cited at a high level for cross-API
  confirmation in Interplay of Light 2017, no direct reuse.)
- **`VK_NV_ray_tracing_invocation_reorder`** — N/A; HZB cull doesn't use RT.
- **TBDR-specific HZB optimization (Mali / Adreno)** — N/A; ProjectV = desktop discrete GPU
  scope per `hardware-profile.md §3`.
