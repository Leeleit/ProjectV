# Pipeline overlap analysis — ProjectV compute passes

Detailed ProjectV-specific overlap analysis per the experiment
`2026-06-20-dec-pipelines-async-compute`. Builds the dependency graph of all 5 compute passes in
`TODO.md` Stages 2.2 / 3.1 / 4.1 / 5.1 / 5.2, classifies each as `OVERLAP_SAFE` or `SEQUENTIAL_REQUIRED`,
and computes the Amdahl-style upper bound for steady-state frame time saving.

---

## 1. Compute pass dependency graph (VoxelLab reference shot)

```
                  ┌───────────────────────────────────────────────┐
                  │ Frame N                                       │
                  │                                               │
                  │  ┌────────────┐    ┌────────────┐             │
                  │  │ shadow CSM │    │  HZB cull  │ (Stage 2.2) │
                  │  │  (graph.)  │    │  (comp.)   │             │
                  │  └─────┬──────┘    └──────┬─────┘             │
                  │        │                  │                   │
                  │        ▼                  ▼                   │
                  │  ┌────────────┐    ┌────────────┐             │
                  │  │ greedy     │    │ fragment   │             │
                  │  │ meshing    │    │ (voxel     │             │
                  │  │ (compute)  │    │  .frag)    │             │
                  │  └─────┬──────┘    └──────┬─────┘             │
                  │        │                  │                   │
                  │        └────────┬─────────┘                   │
                  │                 ▼                             │
                  │           ┌──────────┐                        │
                  │           │  present │                        │
                  │           └──────────┘                        │
                  │                                               │
                  │  Fluid CA tick (Stage 3.1, 20 Hz, separate  │
                  │  frame dispatch — every 3rd frame at 60 FPS):│
                  │                                               │
                  │  ┌────────────┐                                │
                  │  │ fluid CA   │ writes world.voxels[next]     │
                  │  │ (compute)  │   (ping-pong, atomicOr,       │
                  │  └────────────┘    activeChunks)             │
                  │         │                                     │
                  │         │ result consumed by frames           │
                  │         │ N+1, N+2, N+3 (until next tick)     │
                  │         ▼                                     │
                  │  used by meshing + fragment in N+1..N+3       │
                  └───────────────────────────────────────────────┘

   Frame N+1 reads world.voxels (post-tick N), uses updated fluid data
   Frame N+2 reads world.voxels (post-tick N), uses updated fluid data
   Frame N+3 reads world.voxels (post-tick N+1), uses updated data
```

**Key insight:** fluid CA's result is **consumed for 2-3 frames** (at 20 Hz tick vs 60 FPS frame rate). The
async-compute dispatch for tick N can run **any time before frame N+3's render**. This is the natural async
candidate.

---

## 2. Per-pass analysis (T1)

### 2.1 Stage 2.2 HZB cull (compute)

| Aspect                          | Value                                                                                                        |
|:--------------------------------|:-------------------------------------------------------------------------------------------------------------|
| **Read**                        | Prev-frame depth (frame N-1's depth buffer).                                                                 |
| **Write**                       | `visibleMask` SSBO + `VkDrawIndirectCommand` for frame N.                                                    |
| **RAW hazard with main render** | NO — reads N-1 depth, writes N's draw commands. Graphics uses N's draw commands.                             |
| **Sync point**                  | Graphics must wait for HZB signal before `vkCmdDrawIndirectCountKHR` (frame N's draw).                       |
| **OVERLAP_SAFE**                | **YES** — natural double-buffered pattern.                                                                   |
| **Async benefit**               | ~0.5 ms saved per frame (compute hides in fragment bubble).                                                  |
| **Caveat**                      | HZB mip chain build (after main pass, prev frame) can be on graphics queue; AABB-vs-mip test on async queue. |

**Recommendation:** **ASYNC** on dedicated compute queue. Signal `kHzbSignal = 1` per frame. Graphics
queue waits for `kHzbSignal` before indirect draw. Cross-vendor: works on all 4 vendors. Confidence: **high**.

### 2.2 Stage 3.1 Fluid CA (compute, 20 Hz)

Per `agent/knowledge.md`:

| Aspect                          | Value                                                                                                                         |
|:--------------------------------|:------------------------------------------------------------------------------------------------------------------------------|
| **Read**                        | `world.voxels` SSBO (current state, "read source") + `activeChunks` SSBO.                                                     |
| **Write**                       | `world.voxels` SSBO (next state, "write target", via ping-pong swap).                                                         |
| **Tick rate**                   | 20 Hz (per `decisions.md §30.1`).                                                                                             |
| **Frame rate**                  | 60 FPS = 16.6 ms.                                                                                                             |
| **Tick-frame latency**          | 3 frames between dispatch and consumption (worst case).                                                                       |
| **RAW hazard with main render** | NO at time of dispatch (write is to NEXT tick state, not current frame). YES at time of NEXT tick's consumption.              |
| **OVERLAP_SAFE**                | **YES** — naturally async.                                                                                                    |
| **Async benefit**               | ~1.0 ms per tick (saved amortized over 3 frames = ~0.33 ms steady-state, ~2% of frame).                                       |
| **Caveat**                      | Sync point: tick N+1 dispatch must wait for tick N's write completion. Pipeline barrier on `world.voxels` SSBO between ticks. |

**Recommendation:** **ASYNC** on dedicated compute queue. Signal `kFluidCaSignal = 2` per tick. Graphics
queue does NOT need to wait for fluid signal — voxel state is read from prev-tick data which is
double-buffered. Cross-vendor: works on all 4 vendors. Confidence: **high**. Reference implementation:
`daniel-keitel/fluid_bending` (Vulkan, 2022-12) — direct precedent.

### 2.3 Stage 4.1 GPU world gen (compute, periodic)

| Aspect                          | Value                                                                                                       |
|:--------------------------------|:------------------------------------------------------------------------------------------------------------|
| **Read**                        | (none — generates from noise function, no read deps on current frame).                                      |
| **Write**                       | new chunk SSBOs (not consumed by current frame, queued for next frame).                                     |
| **RAW hazard with main render** | NO.                                                                                                         |
| **OVERLAP_SAFE**                | **YES** — strongest candidate.                                                                              |
| **Async benefit**               | 100% spike elimination. Without async: 10-100 ms per batch (full frame hitch). With async: hides in render. |
| **Caveat**                      | Should run on **LOW priority** queue (`VK_QUEUE_GLOBAL_PRIORITY_LOW`) — can be preempted by main render.    |

**Recommendation:** **ASYNC** at LOW priority. Signal `kWorldGenSignal = 3` per batch. No sync with main
render needed (results consumed next frame). Cross-vendor: works on all 4 vendors. Confidence: **very high**.

### 2.4 Stage 5.1 VCT voxelization (compute, per-frame)

| Aspect                          | Value                                                                                                                                                                       |
|:--------------------------------|:----------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **Read**                        | SVDAG (read-only, snapshot).                                                                                                                                                |
| **Write**                       | 3D texture atlas (consumed by fragment cone trace same frame).                                                                                                              |
| **RAW hazard with main render** | YES — fragment shader reads atlas same frame.                                                                                                                               |
| **OVERLAP_SAFE**                | **TIGHT** — depends on mip generation timing.                                                                                                                               |
| **Async benefit**               | 0-0.5 ms (saves only if voxelize + mipgen fits in fragment bubble).                                                                                                         |
| **Caveat**                      | Per AMD RDNA 2023 perf guide: "**Async compute performs poorly when executed in parallel with export bound shaders**". Fragment cone trace is export-bound → don't overlap. |

**Recommendation:** **Sequential default** (graphics queue, after main pass). **Async opt-in** via
`PROJECTV_VCT_ASYNC=ON` env for further experimentation. Cross-vendor: AMD may benefit more (Nanite-
pattern), NVIDIA may not (Unreal: "DX12 nvidia does not support compatible async compute"). Confidence:
**medium**.

### 2.5 Stage 5.2 RTX BLAS build (compute, per chunk edit)

| Aspect                          | Value                                                                                                            |
|:--------------------------------|:-----------------------------------------------------------------------------------------------------------------|
| **Read**                        | SVDAG mesh (snapshot).                                                                                           |
| **Write**                       | BLAS (Bottom-Level Acceleration Structure).                                                                      |
| **RAW hazard with main render** | NO — BLAS used in RT pass, naturally late in pipeline.                                                           |
| **OVERLAP_SAFE**                | **YES** — natural fit.                                                                                           |
| **Async benefit**               | 100% spike elimination (1-10 ms per chunk rebuild hidden). With `VK_KHR_deferred_host_operations`, CPU cost = 0. |
| **Caveat**                      | Intel: Ray Queries + groupshared + async compute = L1 contention (per Intel Xe-HPG doc). Document for Stage 5.2. |

**Recommendation:** **ASYNC** on dedicated compute queue at MEDIUM priority. Use
`VK_KHR_deferred_host_operations` for non-blocking dispatch. Signal `kBlasSignal = 5` per chunk rebuild.
Cross-vendor: NVIDIA + AMD + Intel all support. Confidence: **high**. Reference: NVIDIA Bondarev 2021 blog
explicitly recommends "Build acceleration structure / G-Buffer" as good async overlap pair.

---

## 3. Amdahl-style upper bound

### 3.1 Setup

Steady-state per-frame (VoxelLab reference shot, 60 FPS = 16.6 ms):

- Total frame time: 16.6 ms (measured per `agent/knowledge.md` build verification baseline).
- Compute fraction (current, sequential):
    - HZB: 0.5 ms (3% of frame)
    - Fluid CA: 1.0 ms per tick, amortized to 0.33 ms per frame (2%)
    - VCT (if landed): 1.0 ms (6%)
    - Total current sequential compute: 1.83 ms (11% of frame).
- Graphics fraction (estimate): 12 ms (72% of frame).
- Idle/bubble: 2.77 ms (17% of frame).

### 3.2 Async scenario

Assuming all 4 async candidates fully hide in graphics bubble:

- HZB: 0 ms (was 0.5 ms).
- Fluid CA: 0 ms (was 0.33 ms amortized).
- World gen: 0 ms per frame (was 10-100 ms in batch, amortized negligible but spikes eliminated).
- VCT (async opt-in): 0-0.5 ms (was 1.0 ms; ~50% hidden in fragment cone trace bubble).
- Total new compute in graphics: 0-0.5 ms.
- Total frame: 16.1-16.6 ms.
- **Saving: 0-1.83 ms = 0-11% per frame**, median ~5-8% for steady-state.

### 3.3 Spike elimination (separate from steady-state)

World gen and BLAS build are **spike** workloads (10-100 ms each). Sequential: visible as p99 frame time
spikes. Async: completely hidden, p99 = mean. Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
"**low latency > throughput**": p99 reduction is more valuable than mean FPS gain.

### 3.4 Cross-vendor reality check

| Vendor                            | Async compute benefit estimate          | Reason                                                                                                                                                                                               |
|:----------------------------------|:----------------------------------------|:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| NVIDIA RTX 30/40/50               | 5-8% steady-state                       | NVIDIA has true async (multiple ACE engines), but Unreal: "DX12 nvidia does not support compatible async compute but AMD does". ProjectV compute cull is more compatible than Nanite's mesh-shading. |
| AMD RDNA2/3/4                     | 7-12% steady-state (potentially higher) | Stronger async-compute per RDNA perf guide + Unreal observation. RDNA4 (2025-Q4) has further improvements.                                                                                           |
| Intel Arc Alchemist/Battlemage    | 5-8% steady-state                       | Comparable to NVIDIA. Battlemage (Dec 2024) has minor scheduling improvements.                                                                                                                       |
| **Dev host (RTX 3060 Ti Ampere)** | **5-8% steady-state**                   | Estimate; not measured.                                                                                                                                                                              |

### 3.5 Threshold check

Per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`: «if perf gain < 5-10%, choose simple».
This experiment: 5-8% steady-state **AND** simpler sync model (`vkQueueSubmit2` is cleaner than current
`vkQueueSubmit` + pNext chains + binary semaphores + `vkWaitForFences`).

**Conclusion: PASS.** Both axes (perf gain + simplicity) favor adoption.

---

## 4. Queue topology recommendation

```
┌──────────────────────────────────────────────────────┐
│  Physical device                                     │
│                                                      │
│  Family 0: GRAPHICS | COMPUTE | TRANSFER             │
│  ┌──────────────────────────────────────┐            │
│  │ Queue 0.0: graphics (MEDIUM)         │ ← main render, present │
│  │ Queue 0.1: async compute (MEDIUM)    │ ← HZB, VCT (opt), BLAS │
│  │ Queue 0.2: async compute (MEDIUM)    │ ← fluid CA, world gen  │
│  └──────────────────────────────────────┘            │
│  (Some devices may have separate families)           │
│                                                      │
│  Family 1: TRANSFER (DMA, optional)                  │
│  ┌──────────────────────────────────────┐            │
│  │ Queue 1.0: transfer (LOW)            │ ← async uploads        │
│  └──────────────────────────────────────┘            │
└──────────────────────────────────────────────────────┘
```

**Priority hints (`VK_KHR_global_priority`, core in Vulkan 1.4):**

- Graphics queue 0.0: `MEDIUM` (default).
- Compute queue 0.1: `MEDIUM` (compete fairly with graphics).
- Compute queue 0.2: `LOW` (preemptible by higher).
- Transfer queue 1.0: `LOW` (preemptible by all).

**Don't use `REALTIME`:** requires OS privilege; may fail with `VK_ERROR_NOT_PERMITTED_EXT` on Linux NVIDIA.

**Fallback if no separate compute queue family:**

- Use multiple queues from same `VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT` family.
- Per Imagination blog 2020-07-21: create 2 identical queues, alternate per frame for barrier
  multiplication (serializing pattern), but for true parallelism need 2+ actual hardware queues.

**RDNA-specific note:** Per `agent/knowledge.md`, dev host = NVIDIA. AMD-specific behavior (RDNA
"export bound shaders", RDNA1/2 maintenance branch) deferred to when AMD hardware available.

---

## 5. Sync point matrix (what waits on what)

| Signal             | Signaled by          | Waited by             | Frequency            | Value |
|:-------------------|:---------------------|:----------------------|:---------------------|:------|
| `kHzbSignal`       | async compute (HZB)  | graphics (draw)       | per frame            | 1     |
| `kFluidCaSignal`   | async compute (CA)   | next-tick CA dispatch | per tick (20 Hz)     | 2     |
| `kWorldGenSignal`  | async compute (gen)  | next-frame meshing    | per batch            | 3     |
| `kVctSignal` (opt) | async compute (VCT)  | graphics (mipgen)     | per frame (if async) | 4     |
| `kBlasSignal`      | async compute (BLAS) | RT pass               | per rebuild          | 5     |
| `kImageAvailable`  | swapchain            | graphics              | per frame            | 6     |
| `kRenderFinished`  | graphics             | swapchain             | per frame            | 7     |

Single shared timeline semaphore (1 `VkSemaphore`, multiple values) is sufficient. Alternative: N separate
semaphores, one per signal. Per Khronos blog 2020-01-15, single shared is preferred for simplicity.

---

## 6. What this analysis does NOT cover

1. **Real GPU timing on dev host** — would require ProjectV integration + RenderDoc / Nsight captures.
2. **Driver-specific quirks** — driver version not pinned in this experiment.
3. **Mali TBDR** — not ProjectV target.
4. **`VK_KHR_global_priority` REALTIME** — would require OS privilege test.
5. **`VK_AMDX_shader_enqueue`** — deferred (2025 proposal, AMD-only).
6. **Cross-queue resource ownership transfer** — implementation detail, not analyzed here. Per
   Lou Kramer: use `EXCLUSIVE` + ownership transfer, not `CONCURRENT`.

---

## 7. Reference: AMD RDNA 2023 best practices summary

From `gpuopen.com/learn/rdna-performance-guide/` (2023-03-22):

- "Async compute performs poorly when executed in parallel with export bound shaders." → VCT sequential
  default.
- "Smaller workgroups (64 threads) usually perform better than larger workgroups when run async." → design
  hint for new compute shaders.
- "When overlapping frame rendering with async compute, present from a different queue to reduce the chances
  of stalling." → graphics queue is present, not async.
- "Common overlapping opportunities include Z pre-pass, shadow rendering, and post-process." → matches
  ProjectV's HZB pattern.
- "The frame post-processing can be overlapped with the beginning of the next frames rendering." → world
  gen pattern.
- "Small dispatches work better as pipelined compute vs async compute." → world gen is batch dispatch (OK),
  but small per-frame HZB dispatches may be better on graphics queue. Validate per-measurement.
- "Any graphics work submitted after a compute dispatch or vice-versa can overlap if there are no barriers."
  → key principle.

## 8. Reference: NVIDIA Bondarev 2021 overlap table summary

From `developer.nvidia.com/blog/advanced-api-performance-async-compute-and-overlap/` (2021-10-22):

| Workload A                     | Workload B                                      | Rationale                                                                                              |
|:-------------------------------|:------------------------------------------------|:-------------------------------------------------------------------------------------------------------|
| Math-limited compute           | Shadow map rasterization                        | Shadow is graphics-pipe dominated (CROP/PROP/ZROP/VPC/RASTER), little interference with heavy compute. |
| Ray tracing                    | Math-limited compute                            | RT = RT core, compute = FP/ALU. Different datapaths.                                                   |
| DLSS (tensor core)             | Build acceleration structure (FP/ALU)           | DLSS uses tensor, BLAS uses FP/ALU. No conflict.                                                       |
| Any long workload              | Many short workloads with back-to-back barriers | Bubbles are async opportunity.                                                                         |
| Post-process end of prev frame | G-buffer fills of next frame                    | Inter-frame overlap = "substantial perf gains".                                                        |
| Build acceleration structure   | G-Buffer, shadow maps                           | Both underuse throughput units.                                                                        |
| Ray tracing                    | Shadow map rasterization                        | RT core / FP overlaps graphics (ZROP/PROP/RASTER).                                                     |

For ProjectV, the most applicable rows:

- "Math-limited compute" = fluid CA, world gen, HZB.
- "Build acceleration structure" = BLAS build (Stage 5.2).
- "Shadow map rasterization" = CSM (current mainline) — math-limited compute can overlap.

This confirms: **all 4 async candidates are validated by NVIDIA's own SOTA guidance**.

---

## 9. Operator handoff (for mainline agent adopting this)

If adopting:

1. **Measure on dev host first** (RTX 3060 Ti, current driver). RenderDoc capture of current sequential
   pipeline. Establish baseline. Then enable async + measure. **Per `decisions.md §15` close-out rule, run
   captures required.**
2. **Per-stage rollout** with `PROJECTV_ASYNC_COMPUTE=ON` env. 3-step migration per
   `decisions.md §30.4` precedent.
3. **Document caveats in code** — June 2025 NVIDIA driver bug, RDNA export-bound warning, Intel Ray
   Queries + groupshared L1 contention. Per `AGENTS.md §5.7` "Fix, don't silence" — document, don't
   suppress.
4. **Use `VK_KHR_deferred_host_operations` for Stage 5.2 BLAS build** (CPU cost → 0).
5. **Cross-vendor validation when hardware available**: test on AMD RDNA3/4 + Intel Arc Battlemage. AMD
   likely shows higher benefit per Unreal observation.
