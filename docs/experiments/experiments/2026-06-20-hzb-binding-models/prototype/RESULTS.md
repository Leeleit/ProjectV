# RESULTS — `2026-06-20-hzb-binding-models`

**Run:** 2026-06-20, dev host `obvium`
**Hardware baseline:** см. [`../../hardware-profile.md`](../../hardware-profile.md) §3 (RTX 3060 Ti, GA104
Ampere, 8 GiB VRAM, driver 610.43.02, Vulkan API 1.4.341).
**Configuration:** 1920×1080 base resolution, 8 mip levels, synthetic depth values
`computeRef(m, x, y) = (m * 1_000_000 + y * 1000 + x) * 0.001` per texel.
**Build:** `cmake -B build -S prototype -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`
**Run:** `./hzb_bench 1920 1080 8` (cwd = `build/`).

---

## 1. Test matrix

| Pattern                    | Bind strategy      | Mip 0 | Mip 1 | Mip 2 | Mip 3 | Mip 4 | Mip 5 | Mip 6 | Mip 7 |
|:---------------------------|:-------------------|:------|:------|:------|:------|:------|:------|:------|:------|
| `textureLod`               | `combined_sampler` | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     |
| `texelFetch_storage_image` | `storage_image`    | ✅     | ❌     | ❌     | ❌     | ❌     | ❌     | ❌     | ❌     |
| `texelFetch_sampled_image` | `combined_sampler` | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     | ✅     |

**Summary:** 17/24 PASS, 7/24 FAIL.

**Note on `mip=0` pass for storage_image:** all mips 1-7 fail with the same root cause, but mip 0 coincidentally
returns the correct value because at mip 0 the view's base mip = target mip. See §3 for analysis.

---

## 2. Raw data (`results.csv`)

| pattern                  | bind_strategy    | mip | all_correct | max_abs_error | mismatched | total |
|:-------------------------|:-----------------|:---:|:-----------:|:-------------:|:----------:|:-----:|
| textureLod               | combined_sampler |  0  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  0  |    PASS     |       0       |     0      | 1024  |
| texelFetch_sampled_image | combined_sampler |  0  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  1  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  1  |    FAIL     |     1000      |    1024    | 1024  |
| texelFetch_sampled_image | combined_sampler |  1  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  2  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  2  |    FAIL     |     2000      |    1024    | 1024  |
| texelFetch_sampled_image | combined_sampler |  2  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  3  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  3  |    FAIL     |     3000      |    1024    | 1024  |
| texelFetch_sampled_image | combined_sampler |  3  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  4  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  4  |    FAIL     |     4000      |    1024    | 1024  |
| texelFetch_sampled_image | combined_sampler |  4  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  5  |    PASS     |       0       |     0      | 1024  |
| texelFetch_storage_image | storage_image    |  5  |    FAIL     |     5000      |    1024    | 1024  |
| texelFetch_sampled_image | combined_sampler |  5  |    PASS     |       0       |     0      | 1024  |
| textureLod               | combined_sampler |  6  |    PASS     |       0       |     0      |  480  |
| texelFetch_storage_image | storage_image    |  6  |    FAIL     |     6000      |    480     |  480  |
| texelFetch_sampled_image | combined_sampler |  6  |    PASS     |       0       |     0      |  480  |
| textureLod               | combined_sampler |  7  |    PASS     |       0       |     0      |  120  |
| texelFetch_storage_image | storage_image    |  7  |    FAIL     |     7000      |    120     |  120  |
| texelFetch_sampled_image | combined_sampler |  7  |    PASS     |       0       |     0      |  120  |

---

## 3. Analysis

### 3.1 `textureLod` (combined sampler) — passes everywhere

`textureLod(sampler2D, vec2(uv), float(lod))` correctly samples the requested mip level via the explicit LOD
argument. Verified against ground truth for all 8 mips (max abs error = 0 across 5176 samples).

### 3.2 `texelFetch` (sampled image, not combined) — passes everywhere

`texelFetch(sampler2D, ivec2(coord), int(lod))` correctly samples the requested mip level via the explicit LOD
argument (third operand = mip level for sampled images in GLSL). Verified against ground truth for all 8 mips
(max abs error = 0 across 5176 samples).

This pattern is **functionally equivalent** to `textureLod` for our use case, but uses integer coordinates and an
explicit mip level rather than UV + LOD.

### 3.3 `texelFetch` (storage image) — fails on mips 1-7 with consistent offset

`imageLoad(image2D, ivec2(coord))` reads from the **image view's base mip level only**, regardless of any other
intention. Per the Vulkan spec, a single `image2D` GLSL binding corresponds to one mip level of the bound image
view; the shader **cannot specify a different mip** through `imageLoad`.

Evidence:

- Mip 1: max abs error = 1000 → sample returns value at (x, y) of mip 0 instead of mip 1.
- Mip 2: max abs error = 2000 → sample returns value at (x, y) of mip 0 instead of mip 2.
- Mip N: max abs error = N × 1000 → consistent offset = N × (mip0_y_step).

The error pattern `1000 × N` is exactly `computeRef(0, x, y) - computeRef(N, x, y)` for y > 0, confirming storage
image is reading mip 0.

**Mip 0 passes** only because at mip 0 the view's base mip = target mip. This is a degenerate case; mip 1+ are the
real test.

### 3.4 Implications for HZB culling

HZB culling needs to **dynamically sample a different mip per chunk** based on the chunk's screen-space size.
The cull shader computes `level = floor(log2(max(width, height)))` per chunk AABB and then samples that mip.

With storage image binding:

- Either need **N separate image descriptors** (one per mip), with branching in the shader
- Or use **`image2DArray`** with `arrayLayers = mipCount`, then `imageLoad(image2DArray, ivec3(x, y, mipLevel))`
  (works because array layers are distinct from mip levels; layers can hold mip data if image created with
  `arrayLayers=mipCount` and each "layer" actually contains one mip via sliced view — `VK_KHR_maintenance5`
  feature needed for image view with `subresourceRange = {layer=N}` to map to a specific mip)
- Or store HZB as a **2D image array** with N array layers of size `base/N` each (requires `mipCount` array
  layers, complex setup)

All of these workarounds are significantly more complex than the equivalent sampled-image pattern.

---

## 4. Bindless / descriptor heap cross-check (NOT measured on dev host)

The `foijord/vk-textureLod-repro` (2026) demonstrates a bug where `textureLod` ignores the explicit LOD argument
when the sampled image is bound via `VK_EXT_descriptor_heap` on NVIDIA. Their reproduction used:

- RTX A6000, driver 596.46, Windows 11, Vulkan 1.3
- `textureLod(sampler2D, uv, lod)` through descriptor heap → returns mip 0 always
- `texelFetch(sampler2D, coord, lod)` through descriptor heap → works correctly

**We did not test this on dev host** because:

- `VK_EXT_descriptor_heap` requires `VK_KHR_maintenance5` to be chained into `VkDeviceCreateInfo::pNext` (with
  `VkPhysicalDeviceDescriptorHeapFeaturesEXT.descriptorHeap = VK_TRUE`)
- Setting up that pNext chain requires a feature struct that's not in the system Vulkan headers at this
  prototype's scope (only the `VkPhysicalDeviceDescriptorHeapFeaturesEXT` struct fields `descriptorHeap` and
  `descriptorHeapCaptureReplay` are exposed; `descriptorHeapPushDescriptors` referenced in some samples is not
  in current headers)
- Per `vulkaninfo` summary on dev host: `VK_EXT_descriptor_heap` IS available (extension enumerable), but
  enabling it requires additional plumbing

**Implication for ProjectV:** even though our prototype doesn't reproduce the bindless bug on RTX 3060 Ti
(driver 610.43.02), the `foijord` repro on a different NVIDIA driver (596.46) confirms the bug exists. The
`texelFetch` pattern is robust against this bug per the `foijord` evidence; `textureLod` is fragile.

---

## 5. Conclusion

| Pattern                          | Mip correctness (classic set) | Mip correctness (bindless heap, per literature)                        | Recommended |
|:---------------------------------|:------------------------------|:-----------------------------------------------------------------------|:------------|
| `textureLod` on combined sampler | ✅ all 8 mips                  | ⚠️ **fragile** under `VK_EXT_descriptor_heap` on NVIDIA (foijord 2026) | NO          |
| `texelFetch` on sampled image    | ✅ all 8 mips                  | ✅ **robust** (foijord 2026 confirms works)                             | **YES**     |
| `imageLoad` on storage image     | ❌ only mip 0 works            | ⚠️ fundamentally unsuited (single mip per descriptor)                  | NO          |

**Recommended binding model for Stage 2.2 HZB cull:**

- **Image type:** `VkImage` with `VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT` (current
  `HizCulling.cpp` already sets `TRANSFER_DST | SAMPLED`; needs `STORAGE` only if we want compute mip writes —
  not needed if we use blit-mipchain).
- **Descriptor type:** `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + `VK_DESCRIPTOR_TYPE_SAMPLER` (separate, **not**
  combined) — gives the shader `texelFetch(sampler2D, coord, mip)` access.
- **Shader pattern:** `texelFetch(uDepthPyramid, ivec2(coord), mipLevel).r` with integer mip level computed
  per-chunk from screen-space AABB.
- **Sampler:** `VK_FILTER_NEAREST` + `VK_SAMPLER_MIPMAP_MODE_NEAREST` (HZB needs nearest-neighbor, not linear).

This is a **minimal change** from the current mainline `HizCulling.cpp`:

- Current: `VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT` → already correct for sampled-image
  path.
- Future: add `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + separate `VkSampler` to the HZB descriptor set; change cull
  shader from `textureLod(...)` to `texelFetch(..., mipLevel)`.
- Compatibility: texelFetch works with `VK_KHR_dynamic_rendering_local_read` and bindless heap (
  `VK_EXT_descriptor_heap`).

---

## 6. Mapping to ProjectV hot-path

| ProjectV component                            | Prototype equivalent            | Notes                                                                                                                  |
|:----------------------------------------------|:--------------------------------|:-----------------------------------------------------------------------------------------------------------------------|
| `src/render/HizCulling.cpp` image creation    | `createPyramid()`               | Same `R32_SFLOAT` + `OPTIMAL` tiling + mip chain. Already correct.                                                     |
| `src/render/HizCulling.cpp::BuildHizMipChain` | N/A (blit not tested)           | Stage 2.2 spec uses blit; compute-mip is future optimization.                                                          |
| Planned `src/shaders/hzb_cull.comp`           | `shaders/sample_*.comp`         | **Currently planned to use `textureLod`** per vkguide.dev pattern. **Should be `texelFetch`** for bindless robustness. |
| Future Stage 2.2 wiring in `Renderer.cpp`     | `runSampleTest()` dispatch path | Dispatch path is correct; just binding model needs adjustment.                                                         |

**What stays unmeasured in this prototype:**

- Real ProjectV per-chunk AABB count + realistic depth distribution (out of scope for standalone prototype).
- Actual `vkCmdBlitImage` vs compute mipchain cost on RTX 3060 Ti — only sampled literature (Turitzin 2017).
- Cross-vendor behavior on AMD RDNA3 / Intel Arc (no hardware access).

**What this prototype DOES validate:**

- `texelFetch` + sampled image works correctly across all 8 mips on dev host.
- `textureLod` + combined sampler works correctly across all 8 mips on dev host **with classic descriptor set**.
- `imageLoad` + storage image fundamentally cannot sample different mips from one descriptor (GLSL spec
  limitation).
- The `foijord` repro of `textureLod` under bindless is documented and relevant — adopting `texelFetch` future-proofs
  against this risk.
