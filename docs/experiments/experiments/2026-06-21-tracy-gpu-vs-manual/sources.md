# Sources — `2026-06-21-tracy-gpu-vs-manual`

Полный список верифицированных источников (15 primary, 5 supplementary). Все ссылки проверены
на дату `2026-06-21`.

---

## Primary (15) — Tracy + Vulkan profiling foundation

### Tracy project core

1. **Tracy Profiler manual** — [`github.com/wolfpld/tracy/blob/master/manual/tracy.md`](https://github.com/wolfpld/tracy/blob/master/manual/tracy.md)
   — SOTA authoritative. Подтверждено overhead per zone: 2.25 ns per CPU zone (start+end events);
   10-50 ns typical empty zone; 200-500 ns с callstack depth=16. Section 1.7 (Performance Impact)
   даёт per-zone calibration. Использовано для analytical lower bound §5.

2. **Tracy release notes (NEWS)** — [`github.com/wolfpld/tracy/blob/master/NEWS`](https://github.com/wolfpld/tracy/blob/master/NEWS)
   — verified timeline: v0.10 Aug 2024 → v0.11.1 Aug 2024 → v0.12 May 2025 (Metal/CUDA GPU) →
   v0.13 Nov 2025 (LLM integration) → v0.13.1 Dec 2025 → **vx.xx.x 2026-xx-xx** (host query
   reset для Vulkan traces, **removed queue delay calibration** как «served no real purpose»).
   ProjectV вендорит vx.xx.x per `external/tracy/`. Used для version-specific behavior
   validation.

3. **Tracy `public/tracy/TracyVulkan.hpp`** (vendored at
   `/home/le1t/Projects/ProjectV/external/tracy/public/tracy/TracyVulkan.hpp`) —
   Implementation reference. Подтверждены API names: `tracy::CreateVkContext` (multiple overloads:
   4-arg non-calibrated, 6-arg calibrated, 7-arg dynamic-load with `calibrated` bool, 4-arg
   host-calibrated), `tracy::DestroyVkContext`, `TracyVkCollect(ctx, cmdbuf)` macro (NOT
   `tracy::CollectVkQuery`), `TracyVkZone` macros. Line 671-689 — все overloads.

4. **TracyDeepWiki Performance Considerations** —
   [`deepwiki.com/wolfpld/tracy/5.4-performance-considerations`](https://deepwiki.com/wolfpld/tracy/5.4-performance-considerations)
   — Подтверждено: «Lock-free queuing, no heap allocations on critical path, fast timestamp
   capture, zone cost calibration». Used для analytical model verification.

5. **Tracy Issue #663** — [`github.com/wolfpld/tracy/issues/663`](https://github.com/wolfpld/tracy/issues/663)
   — **CRITICAL**. At 120 FPS `vkGetCalibratedTimestampsEXT` cost grows to 20+ ms over time,
   causing validation errors VUID-vkCmdWriteTimestamp-None-00830 «query not reset». Hypothesis
   from wolfpld: ring buffer overflow at high frequency zones без своевременного `TracyVkCollect`.
   Workaround: more frequent collect. Used для long-run drift test design.

6. **Tracy Issue #227** — [`github.com/wolfpld/tracy/issues/227`](https://github.com/wolfpld/tracy/issues/227)
   — Intel Mesa timestamps 36-bit precision → rollover every 69 sec at full clock; AMD GPU
   power-saving shutdowns reset timestamps → workaround: `manual` profile via sysfs. Cross-vendor
   reliability matrix used для §9 mapping.

7. **Tracy Issue #1212** — [`github.com/wolfpld/tracy/issues/1212`](https://github.com/wolfpld/tracy/issues/1212)
   — gaps 200-500 ns между zones из-за callstack depth; с `TRACY_NO_CALLSTACK` → 30-50 ns.
   Used для analytical calibration baseline.

8. **Tracy Issue #1301** — [`github.com/wolfpld/tracy/issues/1301`](https://github.com/wolfpld/tracy/issues/1301)
   — C API for non-threaded GPU contexts (open Mar 2026). Implies current C API only supports
   threaded (immediate) GPU contexts, не modern graphics APIs. Used для §6 verdict.

9. **Tracy Issue #1319 (Mar 2026)** — [`github.com/wolfpld/tracy/issues/1319`](https://github.com/wolfpld/tracy/issues/1319)
   — `m_refTimeGpu` global → per-context migration (open). Текущая версия: delta-encoding GPU
   timestamps хаотичен при multiple contexts. Implication для multi-context Tracy overhead.
   Used для §7 recommendation.

10. **Tracy PR #642 (Oct 2023)** — [`github.com/wolfpld/tracy/pull/642`](https://github.com/wolfpld/tracy/pull/642)
    — Defer GPU context creation from C API (YaLTeR). Workaround для AMD timestamp resets +
    on-demand connection issues. Used для cross-vendor considerations.

11. **Tracy PR #9252 (Jan 2025)** — [`github.com/KhronosGroup/Vulkan-ValidationLayers/pull/9252`](https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/9252)
    — Vulkan Validation Layers internal Tracy GPU profiling (arno-lunarg). Worker thread
    scanning query results (NOT main thread). Pattern adopted for production environments.
    Used для §7 hybrid strategy rationale.

12. **Bevy + Tracy GPU support PR #18490** — [`github.com/bevyengine/bevy/pull/18490`](https://github.com/bevyengine/bevy/pull/18490)
    — `wgpu-profiler` → `RenderDiagnosticsPlugin` → Tracy GPU timeline row labeled `RenderQueue`.
    Bevy docs [`docs/profiling.md`](https://cocalc.com/github/bevyengine/bevy/blob/main/docs/profiling.md):
    «Tracy can be used to coarsely measure GPU performance. Dynamic clock speeds → look at MTPC
    column, not single frame». Production reference architecture.

### Vulkan core

13. **Khronos `VK_KHR_calibrated_timestamps`** —
    [`vulkan.lunarg.com/doc/view/1.4.341.1/.../vkGetCalibratedTimestampsKHR.html`](https://vulkan.lunarg.com/doc/view/1.4.341.1/windows/antora/refpages/latest/refpages/source/vkGetCalibratedTimestampsKHR.html)
    — **core в Vulkan 1.4**; `VK_EXT_calibrated_timestamps` rev 2 (2018-10-04, **NOT ratified**)
    → promoted to KHR. Used для §3 Method (config B = calibrated path).

14. **Khronos `vkResetQueryPool` host-side** —
    [`docs.vulkan.org/refpages/latest/refpages/source/vkResetQueryPool.html`](https://docs.vulkan.org/refpages/latest/refpages/source/vkResetQueryPool.html)
    — **core в Vulkan 1.2** через promotion из `VK_EXT_host_query_reset` rev 1 (2019-03-12).
    Требует `hostQueryReset` feature. ProjectV dev host (Vulkan 1.4.350) имеет в core. Used
    для §3 Method (manual config C = host-side reset, no GPU submit dependency).

15. **Khronos Vulkan-Samples `samples/api/timestamp_queries`** —
    [`github.com/KhronosGroup/Vulkan-Samples/samples/api/timestamp_queries`](https://github.com/KhronosGroup/Vulkan-Samples/tree/main/samples/api/timestamp_queries)
    — reference implementation с `VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT` +
    `VK_QUERY_RESULT_WITH_AVAILABILITY_BIT` polling pattern. Used для §3 Method manual
    timestamp collection.

---

## Supplementary (5) — vendor tools + cross-references

16. **NVIDIA DriveOS Vulkan-SC perf tuning** —
    [`developer.nvidia.com/docs/drive/drive-os/7.0.3/.../vulkan_sc_performance_tuning.html`](https://developer.nvidia.com/docs/drive/drive-os/7.0.3/public/drive-os-linux-sdk/embedded-software-components/DRIVE_AGX_SoC/Graphics_Programming/Vulkan_SC_Guidance/vulkan_sc_performance_tuning.html)
    — **«`VK_QUERY_RESULT_WAIT_BIT` defines execution dependency → polling CPU loop in the
    driver → use `VkFence` instead»**. Прямая рекомендация против WAIT_BIT в hot-path.
    Used для §7 recommendation (manual path must use availability bit, not WAIT_BIT).

17. **AMD Radeon GPU Profiler 2.6 (Nov 2025)** — [`gpuopen.com/rgp/`](https://gpuopen.com/rgp/)
    — RDNA 4 (RX 9060), memory-related counters, dynamic VGPR. Cross-vendor comparison tool.
    Used для §9 cross-vendor expectations.

18. **Bevy profiling docs** —
    [`cocalc.com/github/bevyengine/bevy/blob/main/docs/profiling.md`](https://cocalc.com/github/bevyengine/bevy/blob/main/docs/profiling.md)
    — Production guide: «Note that while RenderDoc is a great debugging tool, it is not a
    profiler, and should not be used for this purpose». Vendor tools (Nsight Graphics for
    NVIDIA, RGP for AMD) vs Tracy. Used для §2 prior art context.

19. **TracyD3D12.hpp** — [`github.com/wolfpld/tracy/blob/master/public/tracy/TracyD3D12.hpp`](https://github.com/wolfpld/tracy/blob/master/public/tracy/TracyD3D12.hpp)
    — Reference implementation for queue-based context: query heap + readback buffer +
    payload queue + recalibration via `GetClockCalibration`. Mirrors Vulkan pattern. Used
    для analytical model.

20. **TracyCUDA.hpp** — [`github.com/wolfpld/tracy/blob/master/public/tracy/TracyCUDA.hpp`](https://github.com/wolfpld/tracy/blob/master/public/tracy/TracyCUDA.hpp)
    — Alternative GPU context creation: CUPTI callbacks + `cuptiDeviceGetTimestamp` for
    per-device timestamp alignment. Cross-API reference. Used для analytical model.

---

## Cross-references внутри ProjectV

- `agent/knowledge.md` — build / verification contract (Tracy instrumentation rules).
- `src/debug/ProfilingGpu.hpp:54-159` — ProjectV current Tracy GPU integration
  (`TryCreateCalibratedTracyGpuContext`).
- `src/render/vulkan/VulkanInit.cpp:21-110` — `CreateTracyGpuContext` +
  `TryCreateCalibratedTracyGpuContext` flow.
- `dec-pipelines-async-compute` (closed 2026-06-20, verdict=yes) — async-compute foundation;
  предпосылка для multi-context Tracy overhead.
- `vulkan-fps-pacing-vk-ext` (closed 2026-06-20, verdict=mixed) — frame budget context; SOTA
  pattern для p99 frame stability.
- `bindless-descriptor-overhead` (closed 2026-06-20, verdict=mixed) Phase E — RTX TLAS bindless
  + Tracy GPU async-compute profiling interaction.
- `clustered-forward-mass-lights` (closed 2026-06-20, verdict=yes) — top-3 hot-path includes
  `voxel.frag` с cluster grid lookup.
- `hardware-profile.md §3, §4` — RTX 3060 Ti dev host + `VK_KHR_calibrated_timestamps` core
  в Vulkan 1.4.350.
- `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` — 5-10% threshold для
  «if perf gain < 5%, choose simple».
- `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` — Vulkan 1.4 vendor docs (reference для
  `vkCmdWriteTimestamp` + `vkCmdResetQueryPool` + `vkGetCalibratedTimestampsKHR`).
