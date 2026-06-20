# Sources — 2026-06-20-vis-buffer-for-voxels

> Detailed bibliography for the vis-buffer prototype experiment.
> Each citation has been verified via `webfetch` (year, author, context).
> Inline references are in `README.md §2` (top 10) + §8 (full list).

## Foundational papers

1. **Burns, C. A., Hunt, W. A. (2013)** — The Visibility Buffer: A Cache-Friendly Approach to Deferred Shading.
   *Journal of Computer Graphics Techniques (JCGT)* 2(2):55-69.
   https://jcgt.org/published/0002/02/04/paper.pdf
   **Key claim:** 64 MB vis-buffer vs 398 MB G-buffer at 1080p × 8xMSAA = 6.2× bandwidth win.
   **Verification:** PDF read; abstract explicitly states "four-byte integer that encodes a triangle id and an instance
   id".

2. **Schied, C., Dachsbacher, C. (2015)** — Deferred Attribute Interpolation for Memory-Efficient Deferred Shading.
   *HPG 2015* (Eurographics / ACM SIGGRAPH Symposium on High Performance Graphics).
   http://cg.ivd.kit.edu/publications/2015/dais/DAIS.pdf
   **Approach:** Triangle buffer with sample point + screen-space partial derivatives (alternative to Burns-Hunt).
   **Verification:** PDF read; alternative approach to vis-buffer.

3. **Olsson, O., Billeter, M., Assarsson, U. (2012)** — Clustered Deferred and Forward Shading.
   *HPG 2012: Proceedings of the Conference on High Performance Graphics*, pp 87-96.
   DOI: 10.2312/EGGH/HPG12/087-096
   https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf
   **Key claim:** Up to ~1M lights, scales better than tiled shading for depth discontinuities.
   **Verification:** PDF read; explicit comparison to tiled deferred shading (Tiled Deferred 53 FPS vs Clustered Forward
   161 FPS with 1024 lights + transparent bubbles on GTX 680).

4. **Harada, T., McKee, J., Yang, J. (2012/2017)** — Forward+: Bringing Deferred Lighting to the Next Level.
   *GPU Pro 4*, CRC Press / AK Peters, Chapter 5.
   https://takahiroharada.github.io/forward+/
   https://www.oreilly.com/library/view/gpu-pro-4/9781466567443/chapter-33.html
   **Key claim:** Forward+ outperforms compute-deferred on bandwidth-limited GPUs (mobile / integrated).
   **Verification:** O'Reilly page read; reference impl `GPUOpen-LibrariesAndSDKs/ForwardPlus11` (DX11).

## Production deployments

5. **Karis, B. (2021)** — Nanite: A Deep Dive.
   *SIGGRAPH 2021 Advances in Real-Time Rendering in Games*.
   https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf
   **Key claim:** 64-bit vis-buffer (32-bit depth + 32-bit triangle/cluster ID), atomicMax writes.
   "Sounds crazy? Not as slow as it seems — lots of cache hits, no overdraw or pixel quad inefficiencies."
   **Verification:** PDF read; explicit memory layout (depth:instanceID:triangleID), 128-triangle clusters, DAG cluster
   hierarchy.

6. **Wihlidal, G. (2024)** — Nanite GPU-Driven Materials.
   *GDC 2024 talk*.
   https://media.gdcvault.com/gdc2024/Slides/GDC+slide+presentations/Nanite+GPU+Driven+Materials.pdf
   https://www.unrealengine.com/en-US/blog/take-a-deep-dive-into-nanite-gpu-driven-materials
   **Key claim:** 100% compute shaders (UE 5.4); "shading bins" = 3-phase counting sort (count/reserve/scatter) →
   indirect dispatch per material.
   4015 total shading bins, 3675 empty = empty bin dispatch compaction needed.
   **Verification:** Slides read; 3-phase algorithm described; binning pass uses 64-bit atomics (no UAV barriers).

7. **Andersson, J. (2017)** — Triangle Visibility Buffer (Frostbite Labs).
   *DICE Frostbite GDC slides 2017*.
   https://www.slideshare.net/slideshow/parallel-futures-of-a-game-engine-v20/4345460
   **Key claim:** "Triangle Visibility Buffer offers up to 10x-20x geometry vs Deferred rendering and much higher
   resolution. Aligns better with memory access patterns in modern GPUs compared to Deferred Lighting like Clustered
   Deferred Lighting."
   Battlefield 1 / Mass Effect Andromeda: 4K checkerboard + high-res primitive ID buffers on PS4 Pro.
   **Verification:** Slideshare read; DICE/Frostbite attribution confirmed.

8. **Engel, W. (2018)** — Triangle Visibility Buffer (TVB).
   *Diary of a Graphics Programmer blog post, March 2018*.
   http://diaryofagraphicsprogrammer.blogspot.com/2018/03/triangle-visibility-buffer.html
   **Key claim:** Production implementation at Coherent Labs since Sept 2015, derived from Schied,
   simplified to Burns approach, uses ExecuteIndirect + compute shader.
   **Verification:** Blog post read; full implementation history cited.

9. **ConfettiFX / The Forge (2024)** — Triangle Visibility Buffer 2.0.
   *The Forge v1.57 release, May 2024; I3D 2024 Industry Talk*.
   https://github.com/ConfettiFX/The-Forge/releases/tag/v1.57
   https://www.youtube.com/watch?v=kWLev9CoQdg
   **Key claim:** TVB 1.0 = one draw call; TVB 2.0 = no draws (pure compute, 2 dispatches).
   "TVB 2.0 doesn't use draw calls anymore. Not using draw calls makes the whole code base more consistent and less
   convoluted."
   Platforms: Windows D3D12, PS4/5, XBOX, macOS/iOS (Windows/Linux Vulkan planned).
   **Verification:** Release notes read; I3D talk abstract read.

10. **Cao, J. et al. (2024)** — Seamless Rendering on Mobile (NanoMesh + Visbuffer).
    *SIGGRAPH 2024 Advances in Real-Time Rendering*.
    https://advances.realtimerendering.com/s2024/content/Cao-NanoMesh/AdavanceRealtimeRendering_NanoMesh0810.pdf
    **Key claim:** 32-bit visbuffer for mobile; 7 bits for triangle ID (since cluster-based), rest for cluster ID.
    "Visbuffer provides higher Quad Utilization than forward and deferred pipelines, with just 32 bits of extra
    overhead."
    Avoids 64-bit visbuffer on mobile (lacks atomic64 support, higher bandwidth).
    **Verification:** PDF read; cluster-based rationale, mobile-specific constraints explicit.

11. **Unreal Engine 5.4 release notes (2024)** — Nanite improvements.
    https://www.unrealengine.com/en-US/blog/unreal-engine-5-4-is-now-available
    **Key claim:** Experimental Nanite Tessellation, software VRS via Nanite compute materials, optimized Path Tracer.
    "Substantial performance gains" via software VRS.
    **Verification:** Official blog post read.

## Hardware / API references

12. **KhronosGroup (2024)** — Vulkan Guide: Tile-Based Rendering Best Practices.
    https://github.com/KhronosGroup/Vulkan-Guide/blob/main/chapters/tile_based_rendering_best_practices.adoc
    **Key claim:** "Memory bandwidth is often the most significant performance factor" for mobile GPUs.
    "Pixel formats with smaller bit depths can often allow the hardware to use larger tiles or avoid spilling data to
    external memory."
    Vis-buffer approach explicitly recommended for TBR (tile memory stays on-chip).
    **Verification:** AsciiDoc source read.

13. **Lam, C. (2024)** — Inside Snapdragon 8+ Gen 1's iGPU: Adreno Gets Big.
    https://chipsandcheese.com/p/inside-snapdragon-8-gen-1s-igpu-adreno-gets-big
    **Key claim:** Adreno has HW "Visibility Stream" + "Visibility Stream Compressor" (VSC) with 32 pipes (2× Adreno
    530).
    "Adreno's SPs continue to have tiny 1 KB texture cache and 32 KB of...memory".
    **Verification:** Article read; Adreno-specific HW vis-buffer path described.

14. **jglrxavpok (2023)** — Recreating Nanite: Visibility buffer (Vulkan 1.x impl).
    https://blog.jglrxavpok.eu/2023/11/26/recreating-nanite-visibility-buffer.html
    **Key claim:** R64Uint vis-buffer, atomicMax with depth in high bits, triangle+cluster ID in low bits.
    Vulkan 1.x implementation, ~8 LoC change to convert triangle/cluster to vis-buffer write.
    **Verification:** Blog post read; Vulkan API usage confirmed.

15. **zhing2006 (2024)** — hala-visibility-rendering (open-source impl).
    https://github.com/zhing2006/hala-visibility-rendering
    **Key claim:** Uses Mesh Shader + vis-buffer; 124-triangle meshlet → 7-bit triangle ID, 25-bit meshlet index.
    Classifies pixels from vis-buffer into "shading bins" (one indirect dispatch per bin).
    UE5-inspired material classification approach.
    **Verification:** GitHub readme read.

## Voxel-specific references

16. **SSeanPP (2026)** — VoxelMVP: GPU-driven voxel renderer.
    https://github.com/SSeanPP/VoxelMVP
    **Key claim:** `glMultiDrawElementsIndirectCountARB` + compute frustum culling + binary greedy meshing.
    Single draw call per frame regardless of chunk count.
    65 chunks radius × 17 height = 71825 slots.
    8 bytes/vertex (packed uints).
    **Verification:** GitHub readme read; tech stack confirmed.

17. **cgerikj (2020)** — Binary Greedy Meshing v2.
    https://github.com/cgerikj/binary-greedy-meshing
    **Key claim:** 64×64 array of 64-bit integers (occupancy mask); bitwise operations cull 64 faces at a time.
    8 bytes per quad (6 bit x, 6 bit y, 6 bit z, 6 bit width, 6 bit height + 8 bit voxel type).
    Vertex pulling rendering with single draw call.
    **Verification:** GitHub readme read; algorithm described.

18. **Slater, M. (2018)** — Exile: Voxel Rendering Pipeline.
    https://thenumb.at/Voxel-Meshing-in-Exile/
    **Key claim:** Greedy meshing → 4-vertex per face → instanced quad shader with `gl_VertexID` for vertex unpacking.
    "The vertex shader unpacks the vertex data into floating-point formats, calculates the face normal vector, and
    submits the data to the fragment shader."
    **Verification:** Blog post read; rendering pipeline described.

19. **vkguide.dev (2024)** — High-performance voxel and mesh rendering (Ascendant).
    https://www.vkguide.dev/docs/ascendant/ascendant_geometry/
    **Key claim:** "Voxel engines work quite well with deferred or visibility buffer approaches."
    Recommends vis-buffer for high-triangle voxel scenes.
    "Voxel engines can throw at the screen 1 pixel = 1 voxel or even lower" with increasing draw distance.
    **Verification:** Web page read.

20. **McPhail, C. (2018-2019)** — VisBufferTessellation: Vulkan impl with HW tessellation.
    https://github.com/cammymcp/VisBufferTessellation
    **Key claim:** Vulkan implementation, custom bespoke framework, two pipelines (with/without HW tessellation).
    Cites Burns-Hunt (2013), Schied-Dachsbacher (2015), Engel (2016, 2018).
    21 stars / 4 forks (limited adoption but reference impl).
    **Verification:** GitHub readme read.

## ProjectV internal cross-refs

21. **`src/shaders/voxel.frag`** — current fragment shader (per-material SSBO lookup).
    `MaterialVisualBuffer` binding 2, `PackedChunkDescriptors` binding 1, `PackedChunkVoxelPayload` binding 5.
22. **`src/shaders/voxel.vert`** — vertex shader decoding PackedFace to world position.
    `outMaterialIndex` (location 2, uint, 32-bit) per fragment.
23. **`src/shaders/voxel_shadow.{vert,frag}`** — shadow vertex re-decodes PackedFace (cost!) + fragment just discards
    glass.
24. **`src/voxel/VoxelMaterials.hpp`** — `VoxelMaterialVisual` (64 bytes, std430 layout, kVoxelMaterialCount = 5).
25. **`src/render/Renderer.cpp:540-863`** — `RecordGraphicsCommands` orchestration:
    - `RecordVoxelMeshingCommands` (line 551) — compute cull/mesh
    - `RecordShadowCommands` (line 552) — CSM depth pass for 4 cascades
    - Opaque pass (line 776) — single `vkCmdDrawIndirect`, full inline lighting
    - Transparent pass (line 836)
    - Model pass (line 793) — for non-voxel models
26. **`agent/knowledge.md §25`** — greedy meshing per-axis dispatch rationale.
27. **`agent/knowledge.md §30.4`** — 3-step migration precedent (`vkQueueSubmit2` + timeline semaphores).
28. **`agent/knowledge.md §17`** — Linux baseline (Clang 22.1.6, LLD, libstdc++ 16, Vulkan 1.4.350 SDK).
29. **`TODO.md §5.2`** — RTX shadows feature-flag (where vis-buffer integration would land if re-evaluated).
30. **`docs/experiments/hardware-profile.md §3`** — dev host GPU (RTX 3060 Ti, 8 GiB VRAM, Vulkan 1.4.341).
