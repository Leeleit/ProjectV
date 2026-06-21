# STATUS — 2026-06-21-vulkan-fps-pacing-wayland-prototype

**Phase:** concluded-verdict-yes
**Last action:** 2026-06-21 — measured Wayland prototype completed (5 modes × 3 scenarios × 5 seeds ×
100 frames + 5 warmup = 7,500 main measurements, dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02
+ Wayland session + Vulkan 1.4.341). Single-pass sync per `AGENTS.md §13.5`: `backlog.md §In progress
→ §Closed`, `INDEX.md §5 Active → §6 Recent closed` (next sync pass), old `2026-06-20-vulkan-fps-pacing-vk-ext/STATUS.md`
already updated with supersede notation 2026-06-21.
**Blocker:** нет.
**Verdict:** **`yes`** — measured Wayland prototype validates hypothesis:
- **Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`)** gives **93-99% frame interval reduction** vs Mode A
  (busy-wait FIFO baseline) для cpu_bound (192 us vs 17,066 us) + gpu_bound (1,117 us vs 17,111 us)
  + jitter (1,119 us vs 17,114 us) scenarios. **Best low-latency mode.**
- **Mode D (`VK_EXT_present_timing` + `desiredPresentTime`)** gives **41-93% P99 variance reduction** vs
  Mode A. Std-dev = 47-77 us для cpu_bound/gpu_bound vs Mode A 427-902 us = **~10-15× tighter**.
  **Best low-variance mode для vsync-locked pacing.**
- **Mode C (`VK_KHR_present_wait2`)** similar to Mode D без explicit timing.
- **Mode E (D + B combined)** similar to Mode D, slightly higher std-dev.
- **Mode A (busy-wait FIFO)** worst (rate-limited by vsync, high variance).

**CPU present overhead:** Mode B = 44 us mean (lowest), Mode D = 76 us mean, Mode A = 81 us mean.
**Mode D target offset** = -16 ms (vkQueuePresentKHR returned 16 ms before target time) = expected
behavior per spec (compositor holds image until target).

**Headline numbers** (detailed в `RESULTS.md`):

| Mode | Scenario    | Mean (us) | P99 (us) | Std (us) | vs A baseline    |
|:-----|:------------|----------:|---------:|---------:|:-----------------|
| A    | cpu_bound   | 17,066    | 17,771   |    903   | reference (60 Hz) |
| **B**  | cpu_bound   |    **192** |    **427** |    **59** | **-98.9% mean, -97.6% P99** |
| **D**  | cpu_bound   | 10,311    | 10,477   |    **47** | -39.6% mean, **-41.0% P99** |
| **D**  | gpu_bound   | 11,212    | 11,473   |    **78** | -34.5% mean, -34.6% P99 |

**Replaces / supersedes:** `2026-06-20-vulkan-fps-pacing-vk-ext/` (closed `2026-06-20` mixed,
analytical-only + Wayland measurement gap self-identified). New experiment fills measurement gap +
adds `VK_KHR_present_mode_fifo_latest_ready` lever (ratified 2025-03-18, after old capture).
**Hardware-profile.md §4 updated 2026-06-21** with new extension row.

**Caveats:** (a) single GPU vendor validated at first (NVIDIA RTX 3060 Ti, dev host); cross-vendor
deferred to mainline (AMD Mesa RADV + Intel ANV via Mesa 26.1+ Jan 2026); (b) synthetic scenarios
representative not exhaustive; real ProjectV workload validation required; (c) VRR display
behavior out of scope (assumes fixed refresh 60 Hz); (d) Mode B drops frames when CPU+GPU faster
than refresh — Mode D recommended if vsync must be respected; (e) Wayland compositor jitter surface
— gain ожидаемо меньше, чем direct-display per Mesa 26.2 benchmark; (f) CPU prototype only, no real
ProjectV workload coupling; (g) `low_latency_layer` Mesa no-op issue per Korthos 2026-04-27 —
manual implementation рекомендуется; (h) ProjectV input-to-photon latency currently unknown
(TracyPlot не имеет explicit "input latency" tracker — follow-up post-MVP).
**Next tick:** нет (concluded). Re-evaluation trigger: Stage 3.1 GPU Fluid CA pipeline integration
→ mainline prototype + measured ProjectV workload variance; or Stage 5.2 RTX shadow spikes where
frame-pacing matters for input consistency.
