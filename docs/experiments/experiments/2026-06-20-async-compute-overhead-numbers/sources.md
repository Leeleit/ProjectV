# Sources — 2026-06-20-async-compute-overhead-numbers

Web-research per `docs/experiments/AGENTS.md §4` (Exa + verification). Captured `2026-06-20`.

---

## Primary references (measured numbers)

### [NVIDIA Nsight Graphics 2025.5 — Async Compute / GPU Trace](https://docs.nvidia.com/nsight-graphics/2025.5/UserGuide/gpu-trace-system-architecture.html)

NVIDIA official docs (2025-05+). Confirms:

- "On Ampere, you can also dispatch concurrent compute workloads by dispatching it on both the DIRECT and ASYNC_COMPUTE
  queue."
- ASYNC_COMPUTE queue row in GPU Trace = visual proof of async execution.
- "Compute will only run simultaneously with graphics if submitted on from an ASYNC_COMPUTE queue. This can disambiguate
  the SM Occupancy row."

**Key takeaway:** ASYNC_COMPUTE queue is **a real Vulkan queue family**, not just a hint. RTX 3060 Ti (Ampere GA104) has
family 2 = COMPUTE-only queue, exactly per `vulkaninfo` probe `2026-06-20`.

---

### [NVIDIA Advanced API Performance: Async Compute and Overlap (2021-10-22)](https://developer.nvidia.com/blog/advanced-api-performance-async-compute-and-overlap/)

Vladimir Bondarev (NVIDIA DevTech). Quantitative guidance:

- "**Overlap compute workloads with other compute workloads. This scenario is very efficient on NVIDIA Ampere
  Architecture GPUs.**"
- Specific overlap pairs (Table 1):
    - **Math-limited compute** + **Shadow map rasterization** = good (graphics-pipe dominated, compute math-bound).
    - **Ray tracing** + **Denoising** = good (RT = math-limited, denoising = math-limited, separate phases).
    - **DLSS (Tensor)** + **Build Acceleration Structure** = good (Tensor vs FP/ALU different paths).
    - **Build Acceleration Structure** + **G-Buffer / shadow maps** = good (BLAS = FP/ALU, G-buffer = multiple units).
- "**SM Idle % without conflicting high throughput units is almost always a guaranteed improvement.**"

**Key takeaway:** Concrete overlap matrix for ProjectV-relevant scenarios. RTX shadows (Stage 5.2) explicitly listed as
good async-candidate.

---

### [KhronosGroup Vulkan-Samples — Timeline Semaphore](https://github.com/KhronosGroup/Vulkan-Samples/blob/main/samples/extensions/timeline_semaphore/README.adoc)

Official Khronos sample. Reference implementation pattern:

- 2 `VkQueues` — async compute + main graphics.
- 2 timeline semaphores for RAW (compute → graphics) + WAR (graphics → compute).
- Dedicated worker threads per queue; only sync via timeline semaphores, no host-side fence.
- Forward progress throttled by main thread (timeline values).
- **WSI swapchain does NOT support timeline semaphores** — must use binary semaphores for acquire/present.
- Out-of-order submission requires multi-queue implementation (single-queue impls deadlock).

**Key takeaway:** Canonical pattern для нашего prototype — 2 timeline semaphores + 2 worker threads pattern.

---

### [nvpro-samples/vk_timeline_semaphore](https://github.com/nvpro-samples/vk_timeline_semaphore)

NVIDIA's own production-quality sample (RTX 3090 baseline, release build, Ubuntu 18.04).

- **Measured: 230 FPS with async compute vs 200 FPS without = +15% frame rate** (mcubes workload).
- Vulkan timeline semaphore + dedicated async-compute queue.
- Concrete code (`timeline_semaphore_main.cpp`):
    - `VkTimelineSemaphoreSubmitInfo` with separate wait/signal values.
    - Per-batch `timelineValue` tracking (McubesChunk tracks when drawn, signals value N+1 when compute reuses).
    - Pipeline barrier (`VkMemoryBarrier`) for memory dependency (cache flush) — timeline semaphore only handles
      execution order.

**Key takeaway:** **+15%** measured on NVIDIA RTX 3090 for mcubes workload = direct reference для нашего 5-8% estimate (
cross-validation). mcubes = compute-heavy geometry processing (similar to ProjectV voxel pipeline).

---

### [Vulkan Documentation Project — Async Compute Sample](https://docs.vulkan.org/samples/latest/samples/performance/async_compute/README.html)

Khronos official sample. Quantitative:

- "**21.8 ms vs. 22.9 ms = ~5% gain**" for specific FRAGMENT → COMPUTE → FRAGMENT pipeline (when fragment cycles close
  to GPU cycles = no starvation).
- "performance does not scale immensely here, and we shouldn't expect that either" — important caveat.
- TBDR (mobile) vs IMR (desktop) distinction:
    - TBDR: pipeline bubble if FRAGMENT → COMPUTE → FRAGMENT in single queue; need 2 queues with different priorities.
    - IMR (desktop): async compute is more straightforward.
- "**Use multiple Vulkan queues if there is any FRAGMENT → COMPUTE workload happening.**"

**Key takeaway:** 5% measured reference для typical desktop workload. Aligns с ProjectV 5-8% threshold per
`dec-pipelines-async-compute`.

---

### [AMD GPUOpen — Leveraging Asynchronous Queues](https://gpuopen.com/learn/concurrent-execution-asynchronous-queues/)

AMD official guidance:

- "**Compute shaders which make heavy use of LDS and ALU are usually good candidates for the asynchronous compute queue.
  **" → ProjectV Stage 3.1 Fluid CA (heavy ALU, LDS shared mem for active chunk list per `agent/knowledge.md §30.4`) =
  textbook async candidate.
- "**Depth only rendering passes are usually good candidates to have some compute tasks run next to it.**" → Stage 2.2
  HZB cull (depth→HZB blit + compute cull) = good overlap pair.
- "**A common solution for efficient asynchronous compute usage can be to overlap the post processing of frame N with
  shadow map rendering of frame N+1.**" → Stage 5.1 VCT post-process + Stage 2.2 HZB for next frame.
- "**Porting as much of the frame to compute will result in more flexibility**" — supports ProjectV Stage 2.1 compute
  cull path.
- "**Make sure each command list is big enough**" — small async dispatches (<1 µs) hurt more than help.

**Key takeaway:** Specific guidance for ProjectV passes. **Heavy compute + depth-only render** pattern explicit.

---

### [Diligent Engine Tutorial 23: Command Queues](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial23_CommandQueues)

Diligent Engine (AMD-aligned DX12/Vulkan abstraction). Measured:

- **Desktop GPUs: as much as 2× performance improvement** with async compute + double buffering (terrain height/normal
  map gen).
- **High-end mobile GPUs: 1.5× improvement** with double buffering (scene drawing overlap).
- **Low-end mobile: no advantage** — explicitly noted.
- Frame latency tradeoff: "Action games and VR applications require low frame latency and overlapping with the previous
  frame may add extra latency."

**Key takeaway:** Up to **2× speedup** measured on desktop. Critical caveat: **frame latency** increases (need to track,
not just frame time). ProjectV is interactive voxel MVP, not VR — frame latency tradeoff acceptable.

---

### [Unity Issue Tracker UUM-109659 (2025-06-17)](https://issuetracker.unity3d.com/issues/compute-and-graphics-queues-are-not-run-asynchronously-when-asynchronous-compute-shaders-are-enabled-in-dx12)

Unity DX12 bug tracker. Status: "By Design" (not bug, intentional fallback).

- "Compute and graphics queues are not run asynchronously when asynchronous compute shaders are enabled in DX12."
- Reproducible on Unity 6000.0.51f1+ on Windows 11.
- **Vulkan = different story** (more flexible async compute per Khronos async_compute sample).

**Key takeaway:** DX12 doesn't guarantee async; **Vulkan does** (when dedicated compute queue family is exposed).
ProjectV uses Vulkan, so this DX12 bug doesn't apply.

---

## Secondary references

### [Aokana: GPU-Driven Voxel Rendering (arXiv 2505.02017, May 2025)](https://arxiv.org/html/2505.02017v1)

Cross-ref from `nanovdb-on-gpu`. **All passes in compute shaders**: chunk selection, tile selection, ray marching, build
Hi-Z. Single queue — but for 60 FPS voxel sim "headroom is enormous" so async not pursued.

**Key takeaway:** Compute-heavy voxel pipelines are viable without async on RTX class hardware, but for Stage 4.3 (128+
chunks) async becomes useful.

---

## Cross-references to ProjectV docs

- `2026-06-20-dec-pipelines-async-compute` (closed verdict=yes) — literature review foundation.
    - §2.2 vendor matrix (NVIDIA / AMD / Intel).
    - §2.3 production engine usage (Nanite, Lumen, Timberdoodle).
    - §2.4 DEC / persistent compute (AMDX deferred).
    - §2.5 queue priority / queue count.
- `agent/knowledge.md §30.4` — GPU Fluid CA contract (ping-pong + atomicOr + active chunk list).
- `TODO.md §2.2 / §3.1 / §4.1 / §5.2` — ProjectV compute pass stages.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold.
- `docs/experiments/hardware-profile.md` §3 (RTX 3060 Ti queue family 2) + §4 (sync2/timeline/global_priority all core).
- `docs/experiments/benchmarks/methodology.md` — measurement protocol.

---

## Quantitative reference summary (для Integration recommendation)

| Source                                     | GPU             | Workload                 | Speedup      | Notes                  |
|:-------------------------------------------|:----------------|:-------------------------|:-------------|:-----------------------|
| nvpro-samples vk_timeline_semaphore        | NVIDIA RTX 3090 | mcubes geometry          | **+15%**     | Release build, mcubes  |
| Vulkan async_compute sample (Khronos)      | generic desktop | FRAG→COMP→FRAG           | **~5%**      | Specific pipeline case |
| DiligentEngine Tutorial 23                 | desktop GPU     | terrain height/normal    | **up to 2×** | Double buffering       |
| DiligentEngine Tutorial 23                 | high-end mobile | terrain height/normal    | **1.5×**     | Double buffering       |
| `dec-pipelines-async-compute` (literature) | NVIDIA + AMD    | mixed compute + graphics | **5-8%**     | Expected, не measured  |

**Spread: 5% → 200%.** Lower bound = conservative Khronos sample. Upper bound = DiligentEngine with double-buffering
latency tradeoff. ProjectV target: ≥ 5% on typical ProjectV workload (VoxelLab 24³ chunks) per
`legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

---

## Verification notes

- All URLs verified accessible `2026-06-20`.
- Khronos `VkTimelineSemaphoreSubmitInfo` spec link:
  `https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkSemaphoreTypeCreateInfo.html` (verified in
  nvpro-samples code).
- `VK_KHR_synchronization2` core in Vulkan 1.3 per Khronos registry (2020-12-03 ratified by Tobias Hector). Verified via
  `vulkaninfo --summary` on dev host showing Vulkan 1.4.341.
- `VK_KHR_timeline_semaphore` core in Vulkan 1.2 (2019-06-12 ratified by Faith Ekstrand).
- `VK_KHR_global_priority` core in Vulkan 1.4 (2022).
- All three = core on dev host RTX 3060 Ti Vulkan 1.4.341 (per `vulkaninfo` probe).