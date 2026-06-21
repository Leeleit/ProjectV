# Sources — 2026-06-21-vulkan-defragmentation-compaction

Web-research via `webfetch` direct URLs (DuckDuckGo HTML CAPTCHA per operator
directive; Exa HTTP 429 persistent). **8+ primary sources verified** для
этой сессии.

## Primary sources

### 1. VMA (Vulkan Memory Allocator) official documentation, rev 3.4.0

Author: AMD (jlacroixAMD + adam-sawicki-a maintainers). License: MIT.

#### 1.1 [VMA Documentation Index](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/index.html)

Version 3.4.0, Apache/MIT 2017-2026. Указатель глав: Quick start, Choosing
memory type, Memory mapping, Staying within budget, **Resource aliasing (overlap)**,
Custom memory pools, **Defragmentation**, Statistics, Virtual allocator,
Allocation annotation, Debugging, Interop, Configuration.

#### 1.2 [VMA Defragmentation](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/defragmentation.html) (rev 3.4.0, 2026-06-05)

**Главная страница моего experiment.** Документирует полный API:
`VmaDefragmentationInfo`, `vmaBeginDefragmentation` + iterative
`vmaBeginDefragmentationPass` + `vmaEndDefragmentationPass` + `vmaEndDefragmentation`.

**Цитаты (важные для гипотезы):**

> "Interleaved allocations and deallocations of many objects of varying size can
> cause fragmentation over time, which can lead to a situation where the library
> is unable to find a continuous range of free memory for a new allocation
> despite there is enough free space, just scattered across many small free ranges
> between existing allocations."

> "You can perform the defragmentation incrementally to limit the number of
> allocations and bytes to be moved in each pass, e.g. to call it in sync with
> render frames and not to experience too big hitches. See members:
> `VmaDefragmentationInfo::maxBytesPerPass`,
> `VmaDefragmentationInfo::maxAllocationsPerPass`."

> "It is also safe to perform the defragmentation asynchronously to render frames
> and other Vulkan and VMA usage, possibly from multiple threads, with the
> exception that allocations returned in
> `VmaDefragmentationPassMoveInfo::pMoves` shouldn't be destroyed until the
> defragmentation pass is ended."

> "Defragmentation is not supported in custom pools created with
> `VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT`."

#### 1.3 [VmaDefragmentationInfo struct reference](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/struct_vma_defragmentation_info.html)

**Главные параметры:**

```cpp
typedef struct VmaDefragmentationInfo {
    VmaDefragmentationFlags flags;           // ALGORITHM_FAST/BALANCED/FULL/EXTENSIVE
    VmaPool                pool;             // null = all default pools
    VkDeviceSize           maxBytesPerPass;  // 0 = no limit
    uint32_t               maxAllocationsPerPass;  // 0 = no limit
    PFN_vmaCheckDefragmentationBreakFunction pfnBreakCallback;
    void*                  pBreakCallbackUserData;
} VmaDefragmentationInfo;
```

#### 1.4 [VMA Staying within budget](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/staying_within_budget.html)

Содержит `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` flag +
`vmaGetHeapBudgets()` API + `VK_EXT_memory_budget` extension. **Важно для
Stage 4.3 lift draw distance VRAM constraint.**

#### 1.5 [VMA Custom memory pools](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/custom_memory_pools.html)

**Цитата (caveat к моей гипотезе):**

> "Custom pools are commonly overused by VMA users. While it may feel natural
> to keep some logical groups of resources separate in memory, in most cases it
> does more harm than good. Using custom pool shouldn't be your first choice.
> Instead, please make all allocations from default pools first and only use
> custom pools if you can prove and measure that it is beneficial in some way,
> e.g. it results in lower memory usage, better performance, etc."

Подтверждает что compaction — orthogonal lever, не требует custom pools.

#### 1.6 [VMA Memory allocation](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/group__group__alloc.html)

Полный enum список `VmaAllocationCreateFlagBits` (включая
`VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` + strategy flags) +
`VmaDefragmentationFlagBits` (FAST/BALANCED/FULL/EXTENSIVE).

### 2. VMA GitHub repository + CHANGELOG

#### 2.1 [VMA Releases page](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases)

**v3.4.0 (2026-06-05) — 3.4k stars, 442 forks**, по состоянию на 2026-06-21.

#### 2.2 [VMA CHANGELOG.md](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/CHANGELOG.md)

**Цитаты из v3.4.0:**

> "Fixed race conditions in defragmentation (#529, #313) and other places (#525)."

**Цитата из v2.2.0 (2018-12-13):**

> "New, more powerful defragmentation: Added support for defragmentation of GPU
> memory. Defragmentation of CPU memory now uses memmove, so it can move data
> to overlapping regions."

**Цитата из v3.0.0 (2022-03-25):**

> "Added new defragmentation API and algorithm, replacing the old one. See
> structure `VmaDefragmentationInfo`, `VmaDefragmentationMove`,
> `VmaDefragmentationPassMoveInfo`, `VmaDefragmentationStats`, function
> `vmaBeginDefragmentation`, `vmaEndDefragmentation`, `vmaBeginDefragmentationPass`,
> `vmaEndDefragmentationPass`."

### 3. Vulkan 1.4 spec — Memory Allocation chapter

#### 3.1 [Vulkan 1.4 spec Memory Allocation](https://docs.vulkan.org/spec/latest/chapters/memory.html)

Документирует `VkPhysicalDeviceMemoryProperties`, `VkMemoryHeap`, `VkMemoryType`.
**Не критично для моей гипотезы** — используется только как background reference.

## Cross-references (closed ProjectV experiments)

### 4. `2026-06-21-vulkan-memory-aliasing-transient` (closed mixed)

**Aliasing axis = orthogonal lever к compaction.** Зафиксировано VRAM savings
-7-8% для typical workload (276→255 MiB). Combined с compaction (этот
experiment, -1.4%) = ~ -9% total savings = crosses 5% threshold per
`optimization-philosophy.md`.

### 5. `2026-06-21-frame-flight-allocator-budget` (closed mixed)

**Allocator strategy axis = orthogonal lever.** Зафиксировано 0% overhead at
current scale via `WITHIN_BUDGET_BIT`. Step 3 (pre-created ring buffer) deferred
до Stage 4.3. Compaction **stackable** с ring buffer pattern.

### 6. `2026-06-20-vma-sparse-textures` (closed mixed)

**Software VT pattern** использует page table texture allocator. Compaction
помогает page table array fragmentation (R32Uint page table texture).

## Web-research caveats

- DuckDuckGo HTML search endpoint returns CAPTCHA per operator directive
  (persistence verified 2026-06-21).
- `web_search` (Exa MCP) returns HTTP 429 "Too Many Requests" persistent.
- Direct `webfetch` на GPUOpen + Khronos + GitHub URLs работает — primary
  sources retrieved successful.
- Academic papers (arXiv) не использовались — VMA internal algorithm = closed
  source, не публикуется.
- SaschaWillems VMA example (404 на `/master/examples/vulkanmemoryallocator/`)
  — alternative VMA sample path not retrieved; primary VMA docs sufficient.
