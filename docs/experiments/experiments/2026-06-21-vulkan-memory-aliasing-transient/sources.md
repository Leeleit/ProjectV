# Sources — 2026-06-21-vulkan-memory-aliasing-transient

> Web-research complete Phase A via `webfetch` + DuckDuckGo HTML endpoint (Exa HTTP 429 persistent
> per operator directive 2026-06-21). Все источники верифицированы через прямой fetch; дата
> проверки 2026-06-21.

---

## A. Canonical references (primary, 2017-2026)

### A.1 Industry SOTA — Render Graph

1. **Yuriy O'Donnell 2017 — «FrameGraph: Extensible Rendering Architecture in Frostbite»**
   (GDC 2017 talk)
   - URL: <https://www.gdcvault.com/play/1024612/FrameGraph-Extensible-Rendering-Architecture-in-Frostbite>
     (mirror: <https://sandbox.gdcvault.com/play/1024045/>)
   - **Key contribution:** canonical paper for transient resource aliasing + DAG-based barrier
     batching + automatic lifetime analysis. **Frostbite engine** (EA DICE).
   - **Production evidence:** "FG能提升引擎的扩展性，简化async compute，自动的ESRAM别名管理，节省了大量显存"
     (zhuanlan.zhihu.com/p/36522188 summary of GDC17 talk).
   - **Verified:** 2026-06-21 via webfetch.

2. **Themaister 2017 — «Render graphs and Vulkan — a deep dive»**
   - URL: <https://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/>
   - **Key contribution:** Granite Engine open-source reference implementation. Subpass dependencies
     + transient attachments pattern.
   - **Verified:** 2026-06-21 via webfetch.

3. **Themaister 2019 — «A tour of Granite's Vulkan backend — Part 2»**
   - URL: <https://themaister.net/blog/2019/04/17/a-tour-of-granites-vulkan-backend-part-2/>
   - **Key contribution:** transient command pool + transient buffer pattern, `ONE_TIME_SUBMIT_BIT`
     + `TRANSIENT_BIT` on the pool.
   - **Verified:** 2026-06-21 via webfetch.

4. **AMD GPUOpen — Render Pipeline Shaders SDK (RPS)**
   - URL: <https://gpuopen.com/rps/>
   - **Key contribution:** production-quality render graph compiler for explicit APIs (D3D12 +
     Vulkan) with "generally optimal resource barrier and memory aliasing scheduler. Its
     compiler-like architecture is designed to be both controllable and extensible."
   - **Verified:** 2026-06-21 via webfetch.

5. **Khronos Vulkan Tutorial — «Engine Architecture: Rendering Pipeline»**
   - URL: <https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Engine_Architecture/05_rendering_pipeline.html>
   - **Key contribution:** official Vulkan 1.4 declarative render graph pattern: "The registration
     interface enables declarative rendergraph construction where developers specify what they
     want to achieve rather than how to achieve it."
   - **Verified:** 2026-06-21 via webfetch.

### A.2 Vulkan spec + VMA

6. **KhronosGroup/Vulkan-Docs — `chapters/resources.adoc` (2026-06-05)**
   - URL: <https://github.com/KhronosGroup/Vulkan-Docs/blob/main/chapters/resources.adoc>
   - **Key contribution:** `VK_IMAGE_CREATE_ALIAS_BIT` + Vulkan Memory Aliasing spec (§11.8).
   - **Verified:** 2026-06-21 via webfetch.

7. **Vulkan Documentation Project — VkImageCreateFlagBits (3)**
   - URL: <https://docs.vulkan.org/refpages/latest/refpages/source/VkImageCreateFlagBits.html>
   - **Key contribution:** "VK_IMAGE_CREATE_ALIAS_BIT specifies that two images created with the
     same creation parameters and aliased to the same memory can interpret the contents of the
     memory consistently with each other, subject to the rules described in the Memory Aliasing
     section."
   - **Verified:** 2026-06-21 via webfetch.

8. **GPUOpen — Vulkan Memory Allocator «Resource aliasing (overlap)»**
   - URL: <https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/resource_aliasing.html>
   - **Key contribution:** official VMA guidance: "Memory Aliasing of Vulkan specification or
     VK_IMAGE_CREATE_ALIAS_BIT flag. You can create more complex layout where different images
     and buffers are bound at different offsets inside one large allocation."
   - **Verified:** 2026-06-21 via webfetch.

### A.3 Academic

9. **WSCG 2023 — «A Resource Allocation Algorithm for a History-Aware Frame Graph»**
   (WSCG 2023 journal paper E71)
   - URL: <http://wscg.zcu.cz/WSCG2023/journal/E71-full.pdf>
   - **Key contribution:** academic validation of history-aware reuse для resource history read
     patterns. "A history of a resource is defined for a particular frame to be the final contents
     of such a resource at the end of the previous frame. When organizing a graphical application
     using a frame rendering graph approach, it makes sense to implement automatic serving of
     resource history read."
   - **Verified:** 2026-06-21 via webfetch.

### A.4 Modern (2025-2026) implementation

10. **dev.to p3ngu1nzz — «Advanced Vulkan Rendering: Building a Modern Frame Graph and Memory
    Management System»** (2025-10-06)
    - URL: <https://dev.to/p3ngu1nzz/advanced-vulkan-rendering-building-a-modern-frame-graph-and-memory-management-system-15kn>
    - **Key contribution:** modern VMA + DAG + aliasing integration. VMA integration for
      "high-performance, low-fragmentation GPU memory management."
    - **Verified:** 2026-06-21 via webfetch.

11. **dev.to p3ngu1nzz — «Inside 3 Weeks of Vulkan Engine Dev: Render Graphs, Descriptors,
    Deterministic Frame Pacing»** (2025-10-18)
    - URL: <https://dev.to/p3ngu1nzz/inside-3-weeks-of-vulkan-engine-dev-render-graphs-descriptors-deterministic-frame-pacing-2nb2>
    - **Key contribution:** "We integrated VMA (Vulkan Memory Allocator) to provide controlled
      suballocation, budget enforcement, and incremental defragmentation. The RenderGraph
      (DAG-based) now records resource lifetimes and enables aliasing for transient attachments."
    - **Verified:** 2026-06-21 via webfetch.

---

## B. Secondary references

12. **Tony Adriansen 2025 — «Building a Vulkan Render Graph»**
    - URL: <https://tadriansen.dev/2025-04-21-building-a-vulkan-render-graph/>
    - **Key contribution:** practical tutorial, "textures currently render graph owned/generated,
      but user textures could be possible in future future-proofed for auto resource aliasing".
    - **Verified:** 2026-06-21 via webfetch.

13. **Liam Tyler 2024 — «A Poor Man's Render Graph»**
    - URL: <https://liamtyler.github.io/posts/task_graph/>
    - **Key contribution:** minimal render graph implementation, "Render Graphs, or 'Task Graphs,'
      rapidly became the standard in the industry ever since Yuriy O'Donnell's 2017 GDC
      presentation on Frostbite's FrameGraph."
    - **Verified:** 2026-06-21 via webfetch.

14. **Pikachuxxxx/Razix issue #399 — «Transient Resources Management Architecture : Vulkan»**
    (2025-02-11)
    - URL: <https://github.com/Pikachuxxxx/Razix/issues/399>
    - **Key contribution:** Razix RZFrameGraph will implement "transient resources aliasing
      with RZFrameGraph using dedicated GPU resource pools similar to ResourcePools we have."
    - **Verified:** 2026-06-21 via webfetch.

15. **cosmiclearn.com — «Vulkan Frame Graph»**
    - URL: <https://www.cosmiclearn.com/vulkan/frame-graph.php>
    - **Key contribution:** "The deferred realization of physical resources unlocks the most
      devastatingly powerful optimization available to modern rendering engines: physical memory
      aliasing."
    - **Verified:** 2026-06-21 via webfetch.

16. **Raikiri/LegitEngine — rendergraph-based graphical framework**
    - URL: <https://github.com/Raikiri/LegitEngine>
    - **Key contribution:** open-source render graph framework reference.
    - **Verified:** 2026-06-21 via webfetch.

17. **Pyrogenesis — Resource aliasing (overlap)** (Wildfire Games open-source engine)
    - URL: <https://docs.wildfiregames.com/pyrogenesis/resource_aliasing.html>
    - **Key contribution:** "Memory Aliasing" of Vulkan specification or VK_IMAGE_CREATE_ALIAS_BIT
      flag pattern in 0 A.D. engine.
    - **Verified:** 2026-06-21 via webfetch.

18. **Riccardo Loggini 2021 — «Render Graphs»**
    - URL: <https://logins.github.io/graphics/2021/05/31/RenderGraphs.html>
    - **Key contribution:** "Transient Resource System Since all the resources created within the
      RenderGraph are meant to last for a specific time span within a single frame, there is a
      high potential for memory re-use. These resources, owned by the render graph and lasting a
      maximum of one frame, are also called transient resources."
    - **Verified:** 2026-06-21 via webfetch.

---

## C. Cross-refs to existing ProjectV research (per §4 protocol)

Per `docs/experiments/AGENTS.md §3` sources-of-truth:

- **Код ProjectV** (абсолютный приоритет):
    - `src/render/Renderer.cpp:81-110` — `TransitionImage` helper (manual barrier pattern exemplar).
    - `src/render/Renderer.cpp:507-536` — manual `vkCmdPipelineBarrier2` batch for
      `RecordVoxelMeshingCommands`.
    - `src/render/SceneResources.cpp:600-700` — 22 separate `vmaCreateImage`/`vmaCreateBuffer`
      allocations per frame.
    - `src/render/vulkan/VulkanBootstrap.cpp:844-862` — VMA allocator creation.
- **Корневой `AGENTS.md` §5.3** — web search obligation (Exa 429 fallback documented here).
- **`agent/knowledge.md`** (cross-refs only, не copy).
- **`TODO.md`** Stage 2-5 (multi-pass growth makes this axis critical).
- **`agent/workspace.md`** §2 Nearest Gap (no render-pipeline-architecture callout → confirms gap).

## D. Coverage assessment

**Axis coverage check (no duplication in INDEX §6):**

- ✅ Storage axis: covered by `nanovdb-on-gpu`, `vma-sparse-textures`, `svdag-vs-vdb-memory-throughput`.
- ✅ Allocator axis: covered by `frame-flight-allocator-budget` (VMA pool + budget bits).
- ✅ Format axis: covered by `depth-occlusion-quantization`, `vct-cone-count-atlas-precision`.
- ❌ **Render graph / aliasing axis: NOT covered** (gap = green light per §13.7).
