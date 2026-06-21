# Sources — 2026-06-21-mesh-shader-mega-instancing

> Web research verification per `AGENTS.md §4`. 15+ primary + 7 supplementary sources.

---

## Tier 1 — Primary sources (production evidence + canonical patterns)

### 1. GameDev.net blog 2024-08-10 — "Insane draw call reduction with mesh shaders in Vulkan"

- **URL:** https://gamedev.net/blogs/entry/2293837-insane-draw-call-reduction-with-mesh-shaders-in-vulkan
- **Author:** anonymous, production pet-project benchmark
- **Why primary:** **specific numbers** (0.3-0.5ms → 0.02-0.03ms = 10-15× speedup), specific
  scene (6740 instances + 3k visible, 6k draw calls → 15 DispatchMesh calls = 400× reduction in
  draw call count).
- **Key finding:** "In my case it cuts the execution time as 1/10. I didn't even make these
  DispatchMesh() calls in parallel".
- **Pitfall noted:** "When I tested the mesh shader on AMD integrated graphics recently, I find
  previous early-return method causes TDR" → AMD driver needs sorted material groups for
  stability.
- **Author recommendations:** 126 threads/vertices (closest multiple of 3 to 128), 42 indices
  per meshlet, instance sorted by material group (Lowest SV_GroupID = closest instance).

### 2. XRReady/multi-mesh v0.3.0 2026-03-29

- **URL:** https://github.com/XRReady/multi-mesh
- **Author:** XRReady (Godot 4 educational project)
- **Why primary:** **1M entity swarm at 14% GPU utilization** via compute culling + atomic
  counter + stream compaction + indirect draws. Direct validation of mega-instancing pattern.
- **Key results table:**
  - Test A: 1 Swarm of 1,000,000 instances (v0.3.0) = **14% Utilization**
  - Test B: 1 Swarm of 1,000,000 instances (v0.4.0-multi-swarm) = **52% Utilization**
  - Test C: 1,000 Swarms of 1,000 instances (v0.4.0-multi-swarm) = **60% Utilization**
- **3-pass pattern (canonical):**
  1. Pass 1 (Physics & Culling): 1M entities, pack visible transforms into Mega-Buffer
  2. Pass 2 (Command Writer): writes 1,000 distinct indirect draw commands
  3. Pass 3 (Data Distributor): `rd.buffer_copy` to slice Mega-Buffers into per-RID buffers
- **Ultimate fix (v0.4.0):** Pure `RenderingDevice` Multi-Draw Indirect + single draw call, bind
  Mega-Buffers directly to custom vertex shader as storage buffer, `rd.draw_command_indirect()`.

### 3. jglrxavpok 2024-05-13 — "Recreating Nanite: Mesh shader time"

- **URL:** https://blog.jglrxavpok.eu/2024/05/13/recreating-nanite-mesh-shader-time.html
- **Author:** jglrxavpok (Nanite recreation project, Vulkan 1.3)
- **Why primary:** Detailed walkthrough of task+mesh shader pipeline, VkPhysicalDeviceMeshShaderFeaturesEXT
  setup, 8.65ms GPU time analysis (half a frame just for visibility buffer).
- **Pipeline setup checklist:**
  1. Vulkan 1.3
  2. `VK_EXT_MESH_SHADER_EXTENSION_NAME` device extension
  3. `VkPhysicalDeviceMeshShaderFeaturesEXT` with `taskShader` and `meshShader` = VK_TRUE in
     `VkDeviceCreateInfo::pNext`
  4. `VK_SHADER_STAGE_MESH_BIT_EXT` and `VK_SHADER_STAGE_TASK_BIT_EXT` stage flags
  5. **Don't fill** `vertexInput`, `inputAssembly`, `vertexBindingDescriptions`, `vertexAttributes`
     for mesh shader pipelines
- **Key optimization:** workgroup size in both mesh and task shaders must be increased to fully
  use wavefront — "I needed to increase the maximum workgroup size of both mesh and task shaders,
  in order to have more work done by wavefront".
- **Caveat:** "MeshDrawTasksEXT took way longer than it should" — likely CPU-visible buffer write
  or atomicAdd overhead. Putting atomic behind flag = "major performance improvement".
- **Pipeline change:** "I had to change the architecture which makes more sense (at least to me)":
  - CPU: Prepare draw call with number of
  - GPU: The task → goes over → emits → ...
  - Achieved: Bistro scene at ~80 FPS (was 8.65ms = 115 FPS with bottleneck; now 80 FPS = 12.5ms).

### 4. chaoticbob 2024-01-26 — "Mesh Shading Part 3: Instancing"

- **URL:** https://chaoticbob.github.io/2024/01/26/mesh-shading-part-3.html
- **Author:** chaoticbob (mesh shading series author)
- **Why primary:** Canonical instancing-via-amplification example, validates math:
  - `threadGroupCountX = ((meshletCount * instanceCount) / 32) + 1`
  - Example: meshletCount=241, instanceCount=200 → threadGroupCountX = 1507 (1507×32=48224
    threads ≥ 48200 meshlets).
- **Key insight:** "In Mesh shaders, the IA stage is no longer used, and the mesh shader produces
  primitives directly, allowing more flexibility and performance".
- **GLSL pattern:** `taskPayloadSharedEXT` for inter-thread communication inside amplification
  shader workgroup.

### 5. AMD GPUOpen — "Mesh Shaders in AMD RDNA 3 Architecture" GDC 2024

- **URL:** https://gpuopen.com/download/GDC2024_Mesh_Shaders_in_AMD_RDNA_3_Architecture.pdf
  (YouTube: https://www.youtube.com/watch?v=qDwdMDsPfpI)
- **Author:** AMD GPU Architecture team (RDNA 3)
- **Why primary:** **AMD official guidance** for amplification shader culling. "An amplification
  shader is called per mesh to select dynamically the LOD and possibly cull".
- **Key recommendations:**
  - "Process at least 32 or 64 elements, e.g., meshlets, per amplification shader thread group"
  - Use `WavePrefixCountBits` and `WaveActiveCountBits` wave intrinsics on RDNA
  - "AS thread groups should launch at least 32 MS thread groups" (otherwise gaps in MS events)
  - "MAX_TRIANGLES and MAX_VERTICES should be set as low as possible"
  - "An occupancy of ~25% can be enough to reach the triangle throughput limit"
- **Architecture insight:** "MS calls are executed in the same order as the amplification shader
  thread groups were launched. Required to comply with the specified rasterization order".
- **Meshlet culling pattern (AMD GLSL):**
  ```glsl
  [NumThreads(64, 1, 1)]
  void AmplificationShader(uint dtid : SV_DispatchThreadID, uint gtid : SV_GroupThreadID) {
      uint meshletCount = 0;
      // ... cull + WavePrefixCountBits
      DispatchMesh(meshletCount, 1, 1);
  }
  ```

### 6. AMD GPUOpen — "Meshlet compression"

- **URL:** https://gpuopen.com/learn/mesh_shaders/mesh_shaders-meshlet_compression/
- **Author:** AMD GPUOpen (Coburg University of Applied Sciences collaboration)
- **Why primary:** Detailed culling benchmarks + cone culling pattern.
- **Key results:** "Direct3D12 implementation renders to a 500 by 500 pixel framebuffer using Phong
  Shading. As can be seen, our baseline mesh shader pipeline outperforms the vertex pipeline"
  (linear scaling). Cone culling improves performance up to 2× for typical scenes (every second
  meshlet faces away from camera).

### 7. Vulkanised 2023 — "Mesh shading best practices"

- **URL:** https://vulkan.org/user/pages/09.events/vulkanised-2023/vulkanised_mesh_best_practices_2023.02.09-1.pdf
- **Author:** Vulkanised 2023 conference (Khronos)
- **Why primary:** **Cross-vendor performance comparison**:
  - "NVidia: small workgroups, more vertices/primitives per invocation"
  - "AMD: large workgroups, 1 vertex/primitive per invocation"
  - "Use a compile-time loop to match both!"
- **Additional best practices:**
  - "Use `SetMeshOutputsEXT` to allocate output arrays"
  - "Write to output arrays"
  - "Follow driver preferences!"
  - "Avoids fixed-func bottlenecks (IA, tess)"
  - "MS allows per-primitive outputs"
- **Caveat:** "If you disregard best practices... a driver-optimized VS is going to perform
  better than your MS."

### 8. nvpro-samples/gl_vk_meshlet_cadscene

- **URL:** https://github.com/nvpro-samples/gl_vk_meshlet_cadscene
- **Author:** NVIDIA nvpro-samples (official reference)
- **Why primary:** **VK_NV_mesh_shader vs VK_EXT_mesh_shader perf delta** on NVIDIA. "The
  drivers with `EXT_mesh_shader` may not be as fast as `NV_mesh_shader`". "The lack of read &
  write access to outputs in `EXT_mesh_shader` makes the per-primitive culling using shared
  memory slower than the equivalent in `NV_mesh_shader` not requiring shared memory".
- **Workaround:** `drawmeshlet_ext_scull.mesh.glsl` avoids shared memory, only works for meshlets
  with vertex/primitive counts of 32 or 64.
- **Pitfall:** At time of release, Vulkan SDK shaderc may not support `GL_EXT_mesh_shader`
  extension — need to build shaderc yourself.

### 9. NVIDIA Blackwell GPU Architecture 2025

- **URL:** https://images.nvidia.com/aem-dam/Solutions/geforce/blackwell/nvidia-rtx-blackwell-gpu-architecture.pdf
- **Author:** NVIDIA
- **Why primary:** **RTX 50 (Blackwell) Mega Geometry feature**: "RTX Mega Geometry which allows
  you to have an uncompromised solution where you can have a full-fat Nanite mesh, fully
  path-traced, with no rasterization involved. This is done with a new API which quickly,
  effectively and efficiently compresses these clusters over time".
- **Key Blackwell changes:**
  - 4th-gen RT cores (Triangle Cluster Intersection Engine replacing Triangle Intersection)
  - 2× ray triangle intersection rate vs Ada
  - Neural Shaders + Cooperative Vectors API for DX12 + Vulkan
  - 2× INT32 throughput (unified FP32/INT32 in all shader cores)
  - 2× Shader Execution Reordering (SER) throughput
- **Mega Geometry dependency:** "Mega Geometry is what you get when mesh shaders and RT cores
  converge" — directly validates mesh shader + RT convergence direction.
- **Zorah demo:** "RTX Mega Geometry with half a billion triangles per scene".

### 10. KhronosGroup Vulkan-Samples — multi_draw_indirect

- **URL:** https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/performance/multi_draw_indirect/README.adoc
- **Author:** KhronosGroup (official sample)
- **Why primary:** Canonical indirect drawing sample with 3 modes:
  1. CPU-only (VkDrawIndexedIndirectCommand array written on CPU)
  2. GPU (compute shader generates draw commands, toggles instance count 0/1 for visibility)
  3. GPU with buffer device address (using `buffer_reference`)
- **Use case:** 16×16 grid test scene, frustum culling toggles bounding sphere visibility.
- **Key insight (3rd mode):** "buffer_reference allows culling of the next frame to occur prior
  to completion of rendering of the current frame with minimal overhead".

### 11. Vulkan Guide — "Mesh Rendering"

- **URL:** https://www.vkguide.dev/docs/gpudriven/mesh_rendering/
- **Author:** vkguide.dev (Vulkan community)
- **Why primary:** Practical indirect-rendering pattern with 3-layer indirection:
  1. `flat_batches[]`: individual non-instanced draws sorted by sort key
  2. `batches[]`: array of `DrawIndirect` data, each covering range of flat_batches
  3. `multibatches[]`: another level of indirection for combining batches
- **Two indirect buffers pattern:**
  - `clearIndirectBuffer`: CPU-writeable, all instanceCount=0 (used to reset)
  - `drawIndirectBuffer`: GPU-side, actual used for rendering
- **Culling compute shader:** writes visible instance count + new indirect commands.

### 12. DEV.to Michael Sacco 2026-05-13 — "How I Render 100,000 Unity Objects With One Draw Call"

- **URL:** https://dev.to/michael_sacco_0d4c96f7eaf/how-i-render-100000-unity-objects-with-one-draw-call-38ms-to-04ms-2jhh
- **Author:** Michael Sacco
- **Why primary:** **38ms → 0.4ms = 95× speedup** at 100k instances; specific timing table:
  - Default (no batching): CPU 35ms + GPU 3ms = 38ms
  - Static batching: CPU 28ms + GPU 3ms = 31ms
  - GPU instancing: CPU 12ms + GPU 3ms = 15ms
  - Indirect rendering: CPU 0.1ms + GPU 0.3ms = 0.4ms
- **Compute culling pattern:**
  ```hlsl
  [numthreads(64, 1, 1)]
  void CSMain(uint3 id : SV_DispatchThreadID) {
      if (id.x >= _InstanceCount) return;
      float4 pos = _AllPositions[id.x];
      if (IsInFrustum(pos)) {
          uint idx;
          InterlockedAdd(_ArgsBuffer[1], 1, idx);
          _VisiblePositions[idx] = pos;
      }
  }
  ```
- **Result:** 100k → 8k rendered (camera angle dependent), cull in microseconds.

### 13. Vulkan Validation Layer Issue #9263 — false VUID-vkCmdDrawMeshTasksEXT-None-08607

- **URL:** https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/9263
- **Author:** darkestofbreads (filed 2025-01-19)
- **Why primary:** **Pitfall identified**: when using Shader Objects with mesh shaders, you must
  bind `VK_NULL_HANDLE` to `VK_SHADER_STAGE_VERTEX_BIT` for mesh-draw-only pipelines, otherwise
  VVL reports false VUID-vkCmdDrawMeshTasksEXT-None-08607.
- **Spec clarification:** "If the shaderObject feature is enabled, either a valid pipeline must be
  bound... or a valid combination of valid and `VK_NULL_HANDLE` shader objects must be bound to
  every supported shader stage corresponding to the pipeline bind point used for this command".
- **Practical fix (PR #9280):** "Provide a better error message".

### 14. KhronosGroup Vulkan-ValidationLayers PR #4524 — Add validation for VK_EXT_mesh_shader

- **URL:** https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/4524
- **Author:** Ru7w1k
- **Why primary:** Comprehensive VUID list for `VK_EXT_mesh_shader` integration; useful for
  ProjectV graceful fallback validation.
- **VUIDs tracked:**
  - VUID-vkCmdDrawMeshTasksEXT-stage-06480
  - VUID-vkCmdDrawMeshTasksEXT-MeshEXT-07087
  - VUID-vkCmdDrawMeshTasksIndirectEXT-drawCount-07088/07089/07090
  - VUID-vkCmdDrawMeshTasksEXT-TaskEXT-07322/07323/07324/07325
  - VUID-RuntimeSpirv-MeshEXT-07291/07292/07293/07294
  - VUID-RuntimeSpirv-TaskEXT-07291/07292/07293/07294
  - +10 more validation tests

### 15. AMD GPUOpen — "Work Graphs mesh nodes" 2024

- **URL:** https://gpuopen.com/learn/work_graphs_mesh_nodes/work_graphs_mesh_nodes-intro/
- **Author:** AMD (Direct3D 12 mesh nodes in work graphs)
- **Why primary:** **Future direction** — work graph mesh nodes turn work graph into "an
  amplification shader on steroids" with switching between multiple mesh shader PSOs in same
  work graph. "draw calls inside of a work graph turn from a sneak peek into the future to a
  feature you can try out today".
- **ProjectV relevance:** Out of scope (DX12-only, no Vulkan equivalent yet), but informs future
  integration direction.

---

## Tier 2 — Supplementary sources (production reference patterns)

### 16. Vulkan Foliage 2024-02-24 — "Vulkan Foliage rendering using GPU Instancing"

- **URL:** https://thegeeko.me/blog/foliage-rendering
- **Why supplementary:** Specific GPU numbers: 6.7M grass blades at 1080p, 4.7ms compute + 8ms
  draw on RX 5600 XT RADV. Validates massive-instance scale.

### 17. proceduralpixels "Custom GPU-Driven rendering in Unity"

- **URL:** https://www.proceduralpixels.com/blog/custom-gpu-driven-rendering-in-unity
- **Why supplementary:** Detailed 5-step pipeline: ground plane + painting → instance generation
  → indirect draw → frustum culling → indirect draw with culling. Compute prep shader that writes
  group counts into GPU buffer, cull dispatch reads args, vertex shader binds `cullingBuffer` and
  uses `SV_InstanceID` to index culled list.

### 18. ellioman/Indirect-Rendering-With-Compute-Shaders 2017

- **URL:** https://github.com/ellioman/Indirect-Rendering-With-Compute-Shaders
- **Why supplementary:** Canonical Unity3D reference for compute-based indirect rendering with
  frustum + HiZ occlusion + LOD. Tested on Metal + D3D11.
- **Caveat:** "On my Macbook pro (mid 2014) sorting takes approx 50% of the GPU time" — sorting
  cost is non-trivial at scale.

### 19. Unity Scripting API: Graphics.RenderMeshIndirect

- **URL:** https://docs.unity3d.com/6000.5/Documentation/ScriptReference/Graphics.RenderMeshIndirect.html
- **Why supplementary:** Official Unity API for `RenderMeshIndirect` with `IndirectDrawIndexedArgs`
  struct. Validates that `RenderParams.worldBounds` defines bounds to cull and sort geometry as
  a single entity.

### 20. eldnach/indirect-rendering 2023-12-24

- **URL:** https://github.com/eldnach/indirect-rendering
- **Why supplementary:** Unity URP procedural (indirect) Scriptable Render Pass; compute shader
  procedurally transforms vertices and mesh instances; final instance-based culling dispatch
  per-frame. Optional heightmap, wind deformation, camera-based repulsion.

### 21. Themaister/Granite (background reference)

- **URL:** https://github.com/themaister/granite
- **Why supplementary:** Vulkan render graph implementation, automatic layout transitions, async
  compute queue usage. Predecessor of `themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/`.

### 22. AMD GPUOpen — "From vertex shader to mesh shader"

- **URL:** https://gpuopen.com/learn/mesh_shaders/mesh_shaders-from_vertex_shader_to_mesh_shader/
- **Why supplementary:** Introductory overview of mesh shader pipeline; explains how amplification
  shaders skip the input assembler stage and gives complete control over which primitives to export.

---

## Cross-vendor matrix (synthesis from sources)

| Vendor | Generation | Mesh shader | Best workgroup size | Output pattern | Source |
|:-------|:-----------|:------------|:--------------------|:---------------|:-------|
| NVIDIA | Turing (RTX 20) | `VK_NV_mesh_shader` (2018) | small | custom | [1, 8] |
| NVIDIA | Ampere (RTX 30) | `VK_NV_mesh_shader` + `VK_EXT_mesh_shader` (2020) | small | custom | [7, 8] |
| NVIDIA | Ada (RTX 40) | `VK_EXT_mesh_shader` | small, more vertices/primitives | custom | [7] |
| NVIDIA | Blackwell (RTX 50) | `VK_EXT_mesh_shader` + Mega Geometry | small | cluster export | [9] |
| AMD | RDNA 2 (RX 6000) | `VK_EXT_mesh_shader` (limited) | large, 1 vertex/prim | per-thread | [7] |
| AMD | RDNA 3 (RX 7000) | `VK_EXT_mesh_shader` full | large, 1 vertex/prim | per-thread + wave intrinsics | [5] |
| AMD | RDNA 4 (RX 8000) | `VK_EXT_mesh_shader` + Work Graphs mesh nodes | TBD | TBD | [15] |
| Intel | Arc Alchemist (A) | `VK_EXT_mesh_shader` (limited) | TBD | TBD | (no source) |
| Intel | Arc Battlemage (B) | `VK_EXT_mesh_shader` (improved) | TBD | TBD | (no source) |
| Qualcomm | Adreno 7xx | TBD (mobile tier) | TBD | TBD | (no source) |
| Arm | Mali G715+ | Limited (`VK_EXT_mesh_shader` opt-in) | TBD | TBD | (no source) |

**Implication for ProjectV cross-vendor strategy:** compile-time loop pattern (per Vulkanised
2023) accommodates NVIDIA small + AMD large workgroup preferences. RTX 3060 Ti (Ampere) =
`VK_EXT_mesh_shader` available per `hardware-profile.md §4`.
