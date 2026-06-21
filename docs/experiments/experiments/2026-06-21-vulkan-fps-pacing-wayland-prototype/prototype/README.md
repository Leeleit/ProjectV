# Prototype: Vulkan 1.4 + SDL3 Frame Pacing Harness

Standalone Vulkan 1.4 + SDL3 binary. NOT ProjectV mainline. Tests 5 frame-pacing modes on dev host.

## Build

```bash
cd docs/experiments/experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/prototype

# Compile shaders (one-time)
glslc -fshader-stage=vert triangle.vert -o triangle.vert.spv
glslc -fshader-stage=frag triangle.frag -o triangle.frag.spv

# Generate SPIR-V headers
xxd -i triangle.vert.spv > triangle.vert.spv.h
xxd -i triangle.frag.spv > triangle.frag.spv.h

# Build
mkdir -p build && cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel 4
```

## Run

```bash
cd build
./frame_pacing_bench
```

Output: `results.csv` (7,501 rows = 1 header + 5 modes × 3 scenarios × 5 seeds × 100 frames).

## Modes

- **A (busy-wait FIFO):** `vkQueuePresentKHR(FIFO)` + `vkWaitForFences` polling with 9 ms timeout.
- **B (FIFO_LATEST_READY):** `vkQueuePresentKHR(VK_PRESENT_MODE_FIFO_LATEST_READY_KHR)`.
- **C (present_wait2):** FIFO + `VkPresentIdKHR` + `vkWaitForPresent2KHR`.
- **D (present_timing):** FIFO + `VkPresentTimingInfoEXT::targetTime` (Mode D).
- **E (D + B):** `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` + `VkPresentTimingInfoEXT::targetTime`.

## Scenarios

- **cpu_bound:** CPU sleep 100 us per frame.
- **gpu_bound:** CPU sleep 1000 us per frame (simulates ~5 ms GPU work at powersave governor).
- **jitter:** Alternating 500 us / 1500 us per frame.

## Output columns

`mode,scenario,seed,frame_id,cpu_acquire_us,cpu_present_us,gpu_sim_us,frame_interval_us,target_offset_us,target_time_ns`

## Files

- `main.cpp` — prototype harness (~600 LoC).
- `triangle.vert/frag` — GLSL source for SPIR-V compilation.
- `triangle.{vert,frag}.spv` — compiled SPIR-V binaries.
- `triangle.{vert,frag}.spv.h` — embedded SPIR-V byte arrays (`xxd -i`).
- `CMakeLists.txt` — CMake build config (finds SDL3 + Vulkan via system).
- `build/` — generated build dir (gitignored in mainline).
- `build/results.csv` — measurement output (7,501 rows, 7,500 main measurements).
