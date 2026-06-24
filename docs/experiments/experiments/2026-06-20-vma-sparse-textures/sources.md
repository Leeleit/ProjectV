# Sources — `2026-06-20-vma-sparse-textures`

Полный список reference'ов для README §2 (Prior art). Все ссылки верифицированы по году/автору/контексту
в рамках эксперимента `2026-06-20`. Cross-refs к mainline документам (TODO.md, AGENTS.md, knowledge.md)
в README §9.

---

## S1. Software Virtual Texturing (production patterns)

### S1.1 shlomnissan "How Virtual Textures Really Work" (2026-02-04)

- **URL:** https://www.shlom.dev/articles/how-virtual-textures-work/
- **Author:** Shlomi Nissan (independent systems engineer; Biohub scientific viz background).
- **Дата:** 2026-02-04 (fresh — current year 2026 per env).
- **Контекст:** Companion article к working prototype (`shlomnissan/virtual-textures`). Прямой
  reference для ProjectV software VT implementation. Covers: page table encoding (R32Uint),
  feedback pass (reduced-res offscreen), page manager (LRU eviction), Atlas allocation policy,
  hardware sparse vs software trade-off.
- **Ключевые цитаты:**
  - *"Modern GPUs support virtualized texture addressing directly in hardware through sparse
    textures... Sparse textures provide efficient address translation and managed page tables but
    they avoid defining policy."*
  - *"Most modern engines implement their own virtual texturing systems on top of available
    hardware features. Engines prioritize explicit control over residency, feedback, eviction,
    and cross-platform determinism, treating hardware support as a mechanism rather than a
    complete solution."*
  - *"In practice, virtual texturing is effective only in narrow, data-dominated scenarios
    where texture size vastly exceeds GPU memory. For the majority of real-time workloads
    traditional textures offer better performance and faster iteration."*
- **Применимость:** PRIMARY reference для Step 1-3 mainline integration (page table pattern,
  shader function `SampleVirtualTexture`, feedback encoding).

### S1.2 shlomnissan/virtual-textures (GitHub, 2026)

- **URL:** https://github.com/shlomnissan/virtual-textures
- **Author:** Shlomi Nissan.
- **Дата:** 2026 (early year, fresh).
- **Контекст:** Working prototype implementing VT **without hardware sparse textures** — confirms
  software VT sufficient для практики. Built alongside the article.
- **Architecture:** VGLX-based Vulkan prototype, page table + physical atlas + feedback pass +
  CPU page cache + bilinear page padding. Minimal reference implementation.
- **Применимость:** Code skeleton reference для `PageManager` class structure.

### S1.3 Unreal Engine 5.7 Streaming Virtual Texturing

- **URL:** https://dev.epicgames.com/documentation/unreal-engine/streaming-virtual-texturing-in-unreal-engine
- **Author:** Epic Games (UE team).
- **Дата:** 2024-2025 (current shipping engine version per UE 5.7 release).
- **Контекст:** Production-grade VT in UE 5.7. Runtime Virtual Textures (RVT) per
  `URuntimeVirtualTextureComponent`. Material sampling через `RuntimeVirtualTextureSample` node.
- **Pattern:** Page table + atlas + feedback, no sparse HW used directly. UE VT runs on top of
  regular texture array.
- **Применимость:** Production validation that software VT pattern scales to shipping AAA engine.

### S1.4 Nanite GDC 2024 — Graham Wihlidal (Epic Games)

- **URL:** https://media.gdcvault.com/gdc2024/Slides/GDC+slide+presentations/Nanite+GPU+Driven+Materials.pdf
- **Author:** Graham Wihlidal (graphics engineer, Epic Games).
- **Дата:** GDC 2024 (Mar 2024, ~2 years ago — still SOTA per UE 5.4 release cycle).
- **Контекст:** Nanite GPU-driven materials presentation. Confirms UE VT = software layer,
  sparse HW = optional accelerator (not used in shipping UE).
- **Ключевая цитата:** *"We only transform the calculations that affect texture sampling, and we
  already absorbed a lot of this cost prior to Nanite because our virtual texturing system
  already does SampleGrad."*
- **Применимость:** Validates software VT = production path, even with full Nanite integration.

### S1.5 bgfx examples/40-svt (Branimir Karadzic)

- **URL:** https://github.com/bkaradzic/bgfx/blob/9849fe96/examples/40-svt/vt.h
- **Author:** Branimir Karadzic (bgfx author).
- **Дата:** 2018+ (active bgfx branch 2024-2026).
- **Контекст:** Production reference implementation в bgfx engine. Classes: `VirtualTexture`,
  `PageTable`, `TextureAtlas`, `PageCache`, `PageLoader`. TileDataFile streaming format.
- **Применимость:** C++ API skeleton для mainline `VirtualTexture` / `PageTable` /
  `TextureAtlas` classes.

### S1.6 Nathan Gauër "Sparse Virtual Textures" (2022-04-27)

- **URL:** https://studiopixl.com/2022-04-27/sparse-virtual-textures
- **Author:** Nathan Gauër (graphics programmer, StudioPixl).
- **Дата:** 2022-04-27 (~4 years ago, but educational — pattern stable).
- **Контекст:** Educational implementation page-fault simulation via feedback pass. Covers
  mip chains, eviction, jitter for coverage.
- **Применимость:** Conceptual reference for feedback encoding (page index + mip level).

### S1.7 SaschaWillems/Vulkan `texturesparseresidency`

- **URL:** https://github.com/SaschaWillems/Vulkan/blob/master/examples/texturesparseresidency/texturesparseresidency.cpp
- **Author:** Sascha Willems (independent graphics engineer; prolific Vulkan example author).
- **Дата:** 2018+ (active, current master).
- **Контекст:** Vulkan-specific example демонстрирующий `VkSparseImageMemoryBind` + page
  management + mip-level switching. Reference для hardware sparse на Vulkan.
- **Применимость:** Code skeleton reference для Step 4 (hardware sparse path), validation
  point для VMA sparse binding.

---

## S2. Hardware Sparse Texture SOTA 2024-2026

### S2.1 foijord/SparseTexture (GitHub, 2025-02)

- **URL:** https://github.com/foijord/SparseTexture
- **Author:** foijord (independent, contact via GitHub).
- **Дата:** 2025-02-14 (last push, ~4 months ago — fresh, no major driver changes since).
- **Контекст:** Empirical benchmark `vkQueueBindSparse` latency across NVIDIA / AMD / Intel.
  Critical negative finding для NVIDIA: **global blocking, slow scaling, 1 TiB address limit**.
- **Ключевые findings:**
  - NVIDIA `vkQueueBindSparse` blocks globally (thread + process). "Even threaded fence-wait
    ineffective on NVIDIA."
  - NVIDIA 1 TiB sparse address space limit (vs AMD 256 TiB, Intel 16 TiB).
  - Bind latency grows non-linearly with image coverage.
- **Применимость:** Direct input to R5 risk (HW sparse not recommended on NVIDIA for runtime
  streaming) + vendor matrix.

### S2.2 NVIDIA Developer Forums "Sparse texture binding is painfully slow" (2023-07)

- **URL:** https://forums.developer.nvidia.com/t/sparse-texture-binding-is-painfully-slow/259105
- **Author:** antoinerichermoz (community member, A4000 owner).
- **Дата:** 2023-07-07 (orig post), NVIDIA team response 2023-09-12, no public fix timeline.
- **Контекст:** Empirical confirmation NVIDIA A4000: 1000 pages = multiple **seconds**, single
  CPU thread at 100%. Bind cost proportional to both requested binds AND already-bound pages
  across all sparse textures.
- **Status:** NVIDIA Vulkan engineers acknowledged, but no public timeline для fix.
- **Применимость:** Cross-validation of foijord 2025 finding, older hardware (Ampere A4000 ≈
  RTX 3060 Ti tier).

### S2.3 `VK_NV_extended_sparse_address_space` (rev 1, 2023-10-03)

- **URL:** https://github.khronos.org/Vulkan-Site/refpages/latest/refpages/source/VK_NV_extended_sparse_address_space.html
- **Author:** Russell Chou + Christoph Kubisch + Eric Werness + Jeff Bolz (NVIDIA).
- **Дата:** 2023-10-03 (rev 1, not ratified).
- **Контекст:** NVIDIA-specific workaround для 1 TiB sparse address limit. Allows larger virtual
  sparse address space для limited usage set.
- **Status (2026-06):** Available in NVIDIA r535+ driver. Recent NVIDIA r580 (2025-Q2) release
  notes: "Improve image creation speed when using VK_NV_extended_sparse_address_space" per
  https://developer.nvidia.com/vulkan-driver.
- **Применимость:** NVIDIA-only, not cross-vendor — не подходит для default path.

### S2.4 VMA 3.4.0 CHANGELOG (2026-06-05)

- **URL:** https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases/tag/v3.4.0
- **Author:** jlacroixAMD (AMD).
- **Дата:** 2026-06-05 (release tag published; ProjectV vendors this version per
  `external/VulkanMemoryAllocator/CHANGELOG.md`).
- **Контекст:** Latest VMA release. Notable для experiment:
  - **NEW:** `vmaCreateDedicatedImage` / `vmaAllocateDedicatedMemory` with extra `pMemoryAllocateNext`
    for external memory.
  - **NEW:** `VmaAllocationCreateInfo::minAlignment` (#523).
  - **NEW:** `VMA_VERSION` macro (#507).
  - **Sparse convenience functions (`vmaAllocateMemoryPages`, `vmaFreeMemoryPages`)** — **already
    present from 2.x**, не новые. Confirmed per CHANGELOG history.
- **Применимость:** Direct VMA API reference для Step 1-3 implementation. Validate `VMA 3.4.0`
  supports all planned operations.

### S2.5 `VK_EXT_pageable_device_local_memory` (rev 1, 2021)

- **URL:** https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_pageable_device_local_memory.html
- **Author:** Piers Daniell (NVIDIA).
- **Дата:** 2021-08-24 (rev 1).
- **Контекст:** OS-level memory virtualization. When enabled, OS may transparently page
  device-local allocations to host-local. Plus `vkSetDeviceMemoryPriorityEXT` for runtime
  priority adjustment.
- **Ключевая цитата:** *"When this extension is exposed by the Vulkan implementation it indicates
  to the application that the operating system implements pageable device-local memory and the
  application should adjust its memory allocation strategy accordingly."*
- **Применимость:** Complementary, не replacement для VT — решает OS-level memory pressure, не
  page-level residency.

### S2.6 `VK_EXT_memory_decompression` (rev 1, ratified 2025-01-23)

- **URL:** https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_memory_decompression.html
- **Author:** Vikram Kushwaha + Daniel Koch + Jeff Bolz + Christoph Kubisch (NVIDIA) + Spencer Fricke (LunarG).
- **Дата:** 2025-01-23 (rev 1, ratified).
- **Контекст:** GPU memory-to-memory decompression via `vkCmdDecompressMemoryEXT`. GDeflate 1.0
  algorithm. Designed для compressed asset streaming (NVMe → GPU).
- **Status (2026-06):** NVIDIA full (r555+), AMD в progress, Intel not yet.
- **Применимость:** Complementary enhancement — reduces PCIe bandwidth 2-3× для atlas content
  upload. Optional, не standalone VT solution.

### S2.7 KhronosGroup/Vulkan-Guide `sparse_resources.adoc`

- **URL:** https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/sparse_resources.adoc
- **Author:** Khronos Vulkan Working Group.
- **Дата:** 2024-2025 (active Vulkan-Guide repo).
- **Контекст:** Authoritative guide. Covers bind patterns, metadata + mip tail separation,
  multi-aspect handling, VkSparseMemoryBind structure usage.
- **Применимость:** Reference для Step 4 (hardware sparse path) implementation details.

### S2.8 Khronos Vulkan-Samples `sparse_image`

- **URL:** https://github.khronos.org/Vulkan-Site/samples/latest/samples/extensions/sparse_image/README.html
- **Author:** Khronos (Vulkan-Samples repo).
- **Дата:** 2024+ (active Vulkan-Samples).
- **Контекст:** Official sample с `VK_KHR_sparse_*` + `shaderResourceResidency` feature, mip
  streaming, defragmentation. Direct Vulkan sparse reference impl.
- **Применимость:** Validation skeleton для Step 4.

---

## S3. ProjectV internal cross-refs

### S3.1 `TODO.md` §2.3 (Виртуальное текурование вокселей)

- **Файл:** `/home/le1t/Projects/ProjectV/TODO.md:189-207`.
- **Контекст:** Stage 2.3 explicit задача. Target: "256 МБ cap текстур даже на гигантских сценах".
- **Key files per TODO:** `src/render/SceneResources.{hpp,cpp}`, `src/shaders/voxel.frag`.
- **Применимость:** Source of truth для integration target.

### S3.2 `agent/knowledge.md` (VoxelMaterialVisual SSBO)

- **Файл:** `/home/le1t/Projects/ProjectV/agent/knowledge.md`.
- **Контекст:** Current mainline material = small fixed SSBO
  (`VoxelMaterialVisual` × `kVoxelMaterialCount`), bound as `set=0, binding=2` в `voxel.frag`.
- **Применимость:** What gets replaced/augmented by VT (Step 2).

### S3.3 `bindless-descriptor-overhead` (2026-06-20, verdict=mixed)

- **Файл:** `docs/experiments/experiments/2026-06-20-bindless-descriptor-overhead/README.md`.
- **Контекст:** Closed same session. Phase D explicitly deferred до Stage 2.3 landing.
- **Применимость:** Required prerequisite для Step 1 (page table = bindless sampled image).

### S3.4 `dec-pipelines-async-compute` (2026-06-20, verdict=yes)

- **Файл:** `docs/experiments/experiments/2026-06-20-dec-pipelines-async-compute/README.md`.
- **Контекст:** Sync foundation pattern (timeline semaphores + sync2 + async-compute queues).
- **Применимость:** Optional dependency для async feedback decode (Step 3).

### S3.5 `legacy/docs/libraries/vulkan/performance.md` (lines 350-389)

- **Файл:** `/home/le1t/Projects/ProjectV/legacy/docs/libraries/vulkan/performance.md`.
- **Контекст:** Образец sparse binding кода через VMA 3.x
  (`VMA_ALLOCATION_CREATE_SPARSE_BINDING_BIT` + `VkSparseMemoryBind`).
- **Применимость:** Reference impl для Step 4 (hardware sparse path).

### S3.6 `legacy/docs/libraries/vulkan/troubleshooting.md` (line 274)

- **Файл:** `/home/le1t/Projects/ProjectV/legacy/docs/libraries/vulkan/troubleshooting.md`.
- **Контекст:** "Используйте sparse textures для миров >4GB" — already documented pattern.
- **Применимость:** Validates VT is known direction, current experiment provides concrete pattern.

---

## S4. Anti-cite / context setters

### S4.1 Why not `VK_KHR_sparse_*` direct for ProjectV Stage 2.3

Per `foijord 2025` + `NVIDIA forum 2023`: NVIDIA `vkQueueBindSparse` blocking global. AMD
performance degrades. Intel OK but limited coverage. **Not recommended для runtime world streaming**
на mainstream hardware — only для static mega-atlas (load once, never re-bind).

### S4.2 Why not `VK_EXT_pageable_device_local_memory` alone

OS-level paging решает **memory pressure** (multiple apps competing для device-local), не
**virtual texturing** (texture size >> memory). Complementary, не replacement.

### S4.3 Why not dense single-atlas (current pattern)

Per `TODO.md §2.3` explicit DoD: "Объем используемой видеопамяти под текстуры не превышает заданного
лимита в 256 МБ даже на гигантских сценах". Dense single-atlas = same as 256 MiB cap для
*all* materials = inflexible. Software VT = explicit working set control.

### S4.4 Cross-engine VT pattern consensus

| Engine | Pattern | Feedback mechanism | Atlas type | Source |
|:-------|:--------|:-------------------|:-----------|:-------|
| **id Tech 5** (MegaTexture) | Full virtual, every surface | Yes (Carmack 2008) | MegaTexture single | `shlomnissan 2026` historical ref |
| **UE 5.7 RVT** | Volumes + indirect draw | Yes (RuntimeVirtualTexture) | Texture array | `Epic 2024-2025` |
| **Nanite (UE 5.4+)** | Integrated with virtual geometry | Yes (SampleGrad) | Texture array | `Wihlidal GDC 2024` |
| **bgfx 40-svt** | Atlas + page table | Yes | Texture 2D | `Karadzic 2018+` |
| **Frostbite** | Integrated with virtual geometry | Yes | Texture 2D | `Andersson 2017` (general Frostbite) |
| **ProjectV (proposed)** | Atlas + page table | Yes | Texture 2D (16-32 MiB) | This experiment |

Consensus: software VT with feedback = cross-engine default. Hardware sparse = optional accelerator.
