# Sources — 2026-06-21-vk-multi-gpu-split-frame

> **⚠️ Web search availability caveat (per `agent/knowledge.md Part B §9` self-audit fallback policy):**
> `web_search` (Exa) returned HTTP 429 «Too Many Requests» on `2026-06-21` during initial research per
> `AGENTS.md §4` obligation. **Fallback per §9 = `webfetch` + operator's pre-2026 cross-vendor knowledge +
> Vulkan 1.4 spec retrieved via `docs.vulkan.org/refpages/...` 2026-06-21.** All Vulkan 1.4 API claims are
> sourced from primary Vulkan documentation pages. **Cross-vendor SOTA production numbers (NVLink 4.0, AMD
> xGMI, Intel Arc mGPU 2024-2026) are cited from operator's pre-2026 knowledge, NOT verified via fresh
> web_search on 2026-06-21.** Flagged per-experiment в `STATUS.md` и `README.md §5 Results` caveats.

---

## Tier 1 — Vulkan 1.4 core spec (verified 2026-06-21 via `webfetch`)

### 1.1 [Khronos docs — `VK_KHR_device_group`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group.html) (rev 4, ratified 2017-10-10, **promoted to Vulkan 1.1**)

**Что:** Device extension providing functionality for **logical device composed of multiple physical devices**
(«device group»). Allows: (a) memory allocation across subdevices; (b) memory binding from one subdevice to
resource on another; (c) command buffers where some work executes on arbitrary subset of subdevices;
(d) present swapchain image from one or more subdevices.

**Key new commands (core 1.1+):**
- `vkCmdDispatchBaseKHR` — compute dispatch with arbitrary base group ID per physical device (AFR/SFR work
  distribution)
- `vkCmdSetDeviceMaskKHR` — command buffer device mask (which subdevice executes which command)
- `vkGetDeviceGroupPeerMemoryFeaturesKHR` — **peer memory feature query** (COPY_DST, COPY_SRC, GENERIC_DST,
  GENERIC_SRC) — critical for cross-GPU buffer sharing
- `vkGetDeviceGroupPresentCapabilitiesKHR` — present capabilities per subdevice (LOCAL, REMOTE, SUM,
  LOCAL_MULTI_DEVICE)
- `vkGetDeviceGroupSurfacePresentModesKHR` — surface present modes per device
- `vkGetPhysicalDevicePresentRectanglesKHR` — **SFR rectangle geometry query** (which rectangles each device
  renders, sum-composited at present)
- `vkAcquireNextImage2KHR` — image acquisition with device mask

**Key new enums (core 1.1+):**
- `VkDeviceGroupPresentModeFlagBitsKHR`:
  - **`VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR`** — each element of pDeviceMasks selects which instance
    of swapchain image is presented (per-device local present)
  - **`VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR`** — present on remote device (one device renders, another
    presents; useful for heterogeneous setup)
  - **`VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR`** — **SFR** (split frame rendering) — images are
    component-wise summed at present
  - **`VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR`** — **AFR** (alternate frame rendering) —
    multiple instances of swapchain images are presented (one per device, frames staggered)
- `VkMemoryAllocateFlagBitsKHR`:
  - `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` — per-allocation device mask
- `VkPeerMemoryFeatureFlagBitsKHR`:
  - `VK_PEER_MEMORY_FEATURE_COPY_DST_BIT_KHR` / `COPY_SRC` / `GENERIC_DST` / `GENERIC_SRC` — what peer
    memory operations are allowed
- `VkImageCreateFlagBitsKHR`:
  - `VK_IMAGE_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR` — **SFR image split binding** (per-device sub-image
    in split instance bind regions)
- `VkSwapchainCreateFlagBitsKHR`:
  - `VK_SWAPCHAIN_CREATE_SPLIT_INSTANCE_BIND_REGIONS_BIT_KHR` — **SFR swapchain split instance bind**

**Why important:** **В Vulkan 1.4 — core API, не device extension.** `VK_VERSION_1_1` baseline includes
all `VK_KHR_device_group` types/enums/commands per `docs.vulkan.org/refpages/.../VK_KHR_device_group.html`
lines 38-43 «Deprecation State — Promoted to Vulkan 1.1». **ProjectV uses Vulkan 1.4 per `hardware-profile.md
§3` → multi-GPU = standard API surface, no extension probing needed.**

**Spec quote (lines 38-43):**
> «Deprecation State:
> Promoted to Vulkan 1.1»

**Cross-refs:** `agent/knowledge.md Part B §9` (network validation: `docs.vulkan.org/refpages/...` reachable
200 OK), `hardware-profile.md §3` (Vulkan 1.4.341 instance, 1.4.350 conformance), `TODO.md §4.3` (lift draw
distance — direct VRAM beneficiary).

---

### 1.2 [Khronos docs — `VK_KHR_device_group_creation`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group_creation.html) (rev 1, ratified 2016-10-19, **promoted to Vulkan 1.1**)

**Что:** Instance extension providing **enumeration of device groups** and logical device creation from
group subset. Required for `VK_KHR_device_group`.

**Key new commands (core 1.1+):**
- `vkEnumeratePhysicalDeviceGroupsKHR` — **primary entry point** for multi-GPU discovery. Returns
  `VkPhysicalDeviceGroupPropertiesKHR[]` with `physicalDeviceCount` + `physicalDevices[]` + `subsetAllocation`
  per group. Single-GPU host = 1 group with `physicalDeviceCount = 1`.
- `vkCreateDevice` with `VkDeviceGroupDeviceCreateInfoKHR` — logical device with `physicalDeviceCount > 1`
  → multi-GPU logical device.

**Spec quote (line 38-43):**
> «Deprecation State:
> Promoted to Vulkan 1.1»

**Why important:** Provides the **API discovery** mechanism. **For ProjectV's `VulkanBootstrap.cpp`** =
add `vkEnumeratePhysicalDeviceGroupsKHR` call in init, log `deviceGroupCount` + `physicalDeviceCount` per
group + `subsetAllocation` (whether memory can be allocated on subset of group) + Tracy plot.

**Example code (Khronos spec lines 132-159, retrieved 2026-06-21):**
```c
VkDeviceCreateInfo devCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
// (not shown) fill out devCreateInfo as usual.
uint32_t deviceGroupCount = 0;
VkPhysicalDeviceGroupPropertiesKHR *props = NULL;

// Query the number of device groups
vkEnumeratePhysicalDeviceGroupsKHR(g_vkInstance, &deviceGroupCount, NULL);

// Allocate and initialize structures to query the device groups
props = malloc(deviceGroupCount * sizeof(VkPhysicalDeviceGroupPropertiesKHR));
for (i = 0; i < deviceGroupCount; ++i) {
    props[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_GROUP_PROPERTIES_KHR;
    props[i].pNext = NULL;
}
vkEnumeratePhysicalDeviceGroupsKHR(g_vkInstance, &deviceGroupCount, props);

// If the first device group has more than one physical device, create
// a logical device using all of the physical devices.
VkDeviceGroupDeviceCreateInfoKHR deviceGroupInfo = { VK_STRUCTURE_TYPE_DEVICE_GROUP_DEVICE_CREATE_INFO_KHR };
if (props[0].physicalDeviceCount > 1) {
    deviceGroupInfo.physicalDeviceCount = props[0].physicalDeviceCount;
    deviceGroupInfo.pPhysicalDevices = props[0].physicalDevices;
    devCreateInfo.pNext = &deviceGroupInfo;
}
vkCreateDevice(props[0].physicalDevices[0], &devCreateInfo, NULL, &g_vkDevice);
```

**Cross-refs:** same as 1.1.

---

### 1.3 [Khronos docs — `VkDeviceGroupPresentInfoKHR`](https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceGroupPresentInfoKHR.html) (core 1.1+)

**Что:** Structure passed via `pNext` chain of `VkPresentInfoKHR` to specify **mode and device masks** for
multi-device present. Per-present, not at swapchain creation.

**Structure definition (Khronos spec, retrieved 2026-06-21):**
```c
typedef struct VkDeviceGroupPresentInfoKHR {
    VkStructureType                        sType;
    const void*                            pNext;
    uint32_t                               swapchainCount;
    const uint32_t*                        pDeviceMasks;
    VkDeviceGroupPresentModeFlagBitsKHR    mode;
} VkDeviceGroupPresentInfoKHR;
```

**Mode semantics (verified 2026-06-21):**
- `VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_BIT_KHR` (LOCAL): «each element of pDeviceMasks selects which
  instance of the swapchain image is presented. Each element of pDeviceMasks must have exactly one bit set»
- `VK_DEVICE_GROUP_PRESENT_MODE_REMOTE_BIT_KHR` (REMOTE): «each element of pDeviceMasks selects which
  instance of the swapchain image is presented. Each element of pDeviceMasks must have exactly one bit set,
  and some physical device in the logical device must include that bit in its
  VkDeviceGroupPresentCapabilitiesKHR::presentMask» (one device renders, another presents; for
  heterogeneous setup)
- `VK_DEVICE_GROUP_PRESENT_MODE_SUM_BIT_KHR` (SUM = SFR): «each element of pDeviceMasks selects which
  instances of the swapchain image are component-wise summed and the sum of those images is presented. If
  the sum in any component is outside the representable range, the value of that component is undefined»
  (SFR = split frame, composited at present)
- `VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR` (LOCAL_MULTI_DEVICE = AFR): «each element of
  pDeviceMasks selects which instance(s) of the swapchain images are presented. For each bit set in each
  element of pDeviceMasks, the corresponding physical device must have a presentation engine as reported
  by VkDeviceGroupPresentCapabilitiesKHR» (AFR = alternate frames, multiple devices present staggered
  frames)

**Why important:** Per-present mode selection = **low overhead, opt-in, no swapchain recreation needed**.
ProjectV can probe `vkGetDeviceGroupPresentCapabilitiesKHR` once at startup, then choose mode per-frame
based on workload (e.g., AFR for normal scenes, LOCAL for low-VRAM workload, SUM for bandwidth-bound
VCT/BLAS).

**Cross-refs:** same as 1.1.

---

## Tier 2 — Cross-vendor SOTA 2024-2026 (operator's pre-2026 knowledge, **NOT verified via web_search 2026-06-21**)

> **⚠️ Caveat per §9 fallback policy:** cross-vendor production numbers below cited from operator's pre-2026
> knowledge. **No fresh `web_search` verification 2026-06-21** (Exa 429). Will be flagged in `README.md §5
> Results` caveats. Recommended re-verification when `web_search` becomes available.

### 2.1 NVIDIA NVLink 4.0 / 4.1 (Hopper, Blackwell)

- **NVLink 4.0** (Hopper H100, 2022): 900 GB/s bidirectional per pair (18 links × 50 GB/s/link),
  7-9× **PCIe 5.0 x16** (128 GB/s bidirectional)
- **NVLink 4.1** (Blackwell B200, 2024-2025): 1.8 TB/s bidirectional per pair (18 links × 100 GB/s/link),
  ~14× **PCIe 5.0 x16**; scales to **NVLink Switch** fabric = 72 GPU cluster
- **Multi-GPU scaling** (Hopper/Blackwell NVLink 4.0+): 70-90% on 2 GPU for compute-bound workloads
  (peer memory copy ~10-20% overhead, present overhead ~5-10%), 60-80% on 4 GPU
- **AFR scaling** (NVIDIA driver AFR mode, legacy): 50-70% baseline (no NVLink, PCIe-only)
- **Reference:** [NVIDIA Hopper Architecture Whitepaper](https://www.nvidia.com/en-us/data-center/h100/),
  [NVIDIA Blackwell Architecture Whitepaper](https://www.nvidia.com/en-us/data-center/h200/),
  [NVIDIA NVLink Roadmap](https://www.nvidia.com/en-us/data-center/nvlink/) — **NOT fetched 2026-06-21,
  cited from operator's pre-2026 knowledge**

### 2.2 AMD xGMI / Infinity Fabric (RDNA 3, RDNA 4)

- **xGMI** (RDNA 3, 2022): 200-400 GB/s per link pair, **APU + discrete dual-GPU** (e.g., RX 7900 XTX
  dual-slot) — peer memory band ~5-8× **PCIe 4.0 x16**
- **Infinity Fabric** (RDNA 4, 2024-2025): 800 GB/s per pair (×2 of RDNA 3 xGMI), Apple M-series Ultra
  fabric pattern; **dual-die GPUs** (e.g., dual-GCD Radeon Pro)
- **Multi-GPU scaling** (RDNA 3/4 xGMI/IF): 60-80% on 2 GPU (peer memory ~20-30% overhead, present overhead
  ~10-15%)
- **AFR scaling** (AMD driver AFR mode, legacy): 50-70% baseline (no xGMI, PCIe-only)
- **Reference:** [AMD RDNA 3 Architecture](https://www.amd.com/en/graphics/rdna-3),
  [AMD RDNA 4 Architecture](https://www.amd.com/en/graphics/rdna-4) — **NOT fetched 2026-06-21,
  cited from operator's pre-2026 knowledge**

### 2.3 Intel Arc mGPU (Alchemist, Battlemage)

- **No native peer interconnect** (Resizable BAR / SAM only, no NVLink-equivalent for consumer)
- **PCIe 4.0 x16** (32 GB/s bidirectional) = peer memory bottleneck for 2-GPU setups
- **Multi-GPU scaling** (Intel Arc + PCIe 4.0): 30-50% on 2 GPU (peer memory ~50-70% overhead, dominant
  bottleneck)
- **Reference:** [Intel Arc Architecture](https://www.intel.com/content/www/us/en/products/docs/discrete-
  gpus/arc/overview.html) — **NOT fetched 2026-06-21, cited from operator's pre-2026 knowledge**

### 2.4 Implicit multi-GPU via driver (legacy AFR / "implicit mGPU")

- **NVIDIA driver "Multi-GPU mode"** (pre-2015, deprecated): driver-level AFR dispatch + present,
  no application code required for simple cases. **NOT supported in Vulkan** (only DirectX 9/10/11 legacy
  AFR profile)
- **AMD driver "XDMA mGPU"** (pre-2018, deprecated): same pattern, not Vulkan
- **Vulkan 1.4 = explicit multi-GPU only** (via `VK_KHR_device_group` core). Implicit driver AFR = legacy,
  out of scope for current ProjectV API.

---

## Tier 3 — Local reference (ProjectV repo)

### 3.1 `docs/experiments/hardware-profile.md §3` (Captured 2026-06-20, dev host `obvium`)

- **Device:** NVIDIA GeForce RTX 3060 Ti (GA104, Ampere)
- **VRAM:** 8.00 GiB heap 0, **budget 5.06 GiB** (driver limit)
- **Vulkan API version:** 1.4.341 (instance), 1.4.350 (conformance)
- **Per `hardware-profile.md §4`:** all listed extensions (RTX, mesh shader, dynamic rendering, synchronization2,
  timeline semaphore) are validated. **`VK_KHR_device_group` NOT explicitly listed in §4** but **promoted
  to Vulkan 1.1** per Tier 1.1 — so it's core 1.1+ and available on Vulkan 1.4 device per spec.
- **Per `hardware-profile.md §3`:** RTX 3060 Ti Ampere = peer memory bandwidth limited by **PCIe 4.0 x16**
  (32 GB/s bidirectional, Ampere + consumer motherboard). **No NVLink** on Ampere consumer (NVLink reserved
  for Quadro / Tesla / Hopper data center). For 2-GPU NVLink pair, need **Hopper / Blackwell data center**
  (per Tier 2.1).

### 3.2 `agent/knowledge.md` (cross-refs, not duplicated per `AGENTS.md §15`)

- `Part A §2` — Mainline = reproducible interactive voxel MVP; **multi-GPU = forward-looking scaling, not
  gating current MVP slice**
- `Part A §4` — Build / verification contract (Tracy instrumentation, debug/release presets)
- `Part A §15` — CSM shadow baseline (orthogonal to multi-GPU)
- `Part A §30.4` — **3-step migration precedent** (foundation → adoption → default flip, additive не
  breaking) — applied to multi-GPU recommendation
- `Part B §9` — Self-audit / tool availability; `webfetch` validated against `docs.vulkan.org/refpages/...`

### 3.3 `TODO.md` (Stage 4.3 explicit cross-ref)

- **§4.3 «Увеличение лимита дальности отрисовки (Lift Draw Distance Cap)»** — **direct beneficiary of
  multi-GPU VRAM aggregation**: «Снять ограничение дальности отрисовки в 64 метра
  (`kMainlineVisibleSceneMaxDistance` в `src/app/Camera.cpp`). Масштабировать размер дескрипторов и
  емкость буферов (`sceneFaceCapacity`, `opaqueIndirectBuffer`, `shadowIndirectBuffer`) под целевую
  дальность в 128/256 метров.»
- Multi-GPU = **alternative scaling lever** to scale 128/256m draw distance without 8 GiB → 16/24 GiB
  hardware upgrade (RTX 5090 32 GiB ~$2K USD, H100 80 GiB ~$30K USD)

### 3.4 `agent/workspace.md` (Nearest Gap, 2026-06-21)

- §2 Nearest Gap: «**Stage 4.3 lift draw distance** — VRAM cap = main bottleneck for 128+ chunks draw
  distance. Closed `frame-flight-allocator-budget` (mixed) + `depth-occlusion-quantization` (yes) +
  `vma-sparse-textures` (mixed) + `vct-cone-count-atlas-precision` (mixed) + `nanovdb-on-gpu` (yes) все
  mitigation strategies for single-GPU; **multi-GPU aggregation = new lever** in same axis.»

### 3.5 Closed experiments (cross-axis, not duplicated per `AGENTS.md §15`)

- **`2026-06-20-dec-pipelines-async-compute` (yes)** — **sync foundation** for cross-queue multi-GPU sync
  (`VK_KHR_timeline_semaphore` cross-queue)
- **`2026-06-20-async-compute-overhead-numbers` (yes +9.85-11.34%)** — **sync measurement** for
  cross-queue async (per-frame overhead baseline)
- **`2026-06-20-frame-flight-allocator-budget` (mixed)** — **allocator strategy** same VRAM axis
- **`2026-06-20-vma-sparse-textures` (mixed)** — **software VT** same VRAM axis
- **`2026-06-20-nanovdb-on-gpu` (yes)** — **GPU storage** same VRAM axis
- **`2026-06-21-depth-occlusion-quantization` (yes)** — **format axis** same VRAM (-50% VRAM D32→D16)
- **`2026-06-21-vct-cone-count-atlas-precision` (mixed)** — **atlas format** same VRAM axis
- **`2026-06-20-vulkan-fps-pacing-vk-ext` (mixed)** — **frame pacing** foundation for AFR half-rate
  present patterns (per `VK_KHR_present_id/2` integration)
- **Multi-GPU aggregation = new lever** в same VRAM axis (orthogonal to existing mitigations)

---

## Tier 4 — Web research gaps (per §9 fallback policy, will retry `web_search` 2026-06-21+)

- **NVIDIA Hopper / Blackwell NVLink 4.0/4.1 production benchmarks** (multi-GPU scaling numbers) — not
  fetched 2026-06-21; recommend re-verification
- **AMD RDNA 4 Infinity Fabric production benchmarks** — not fetched 2026-06-21
- **Intel Arc Battlemage mGPU status 2024-2026** — not fetched 2026-06-21 (Intel may have cancelled
  consumer mGPU, or moved to data center only)
- **Vulkanised 2024/2025 multi-GPU presentations** (Khronos events) — not retrieved; URLs 404 or
  moved. Sascha Willems multi-GPU blog post (`saschawillems.de/blog/2024/01/10/vulkan-device-groups/`)
  404; AMD GPUOpen multi-GPU guide (`gpuopen.com/learn/handle-multi-gpu-graphics-apis/`) 404
- **HPG (High Performance Graphics) 2024/2025 papers on multi-GPU rendering** — not retrieved; recommend
  re-verification
- **DirectX 12 multi-GPU (`Explicit Multi-Adapter`) cross-reference** — not retrieved, but DX12 has
  similar API surface (NodeVisibility, NodeIndex, multi-engine); useful for API design comparison

---

## Re-evaluation triggers (when `web_search` becomes available)

1. **Vulkanised 2025/2026 multi-GPU presentations** (Khronos events, free, public) — primary source
2. **NVIDIA GTC 2024-2026 multi-GPU sessions** (developer.nvidia.com/gtc) — Hopper/Blackwell scaling
3. **AMD RDNA 4 / UDNA whitepapers** (developer.amd.com) — Infinity Fabric production numbers
4. **HPG 2024/2025 proceedings** (diglib.eg.org) — academic multi-GPU rendering research
5. **Sascha Willems multi-GPU sample code** (GitHub SaschaWillems/Vulkan) — may exist under different URL
6. **Intel Arc Battlemage 2024+ production status** (intel.com/content/www/us/en/developer) — consumer
   mGPU status check
