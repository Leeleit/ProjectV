# Prototype — `vma_sparse_bench`

Standalone Vulkan 1.4 + VMA 3.4.0 + volk harness for `2026-06-20-vma-sparse-textures`.
**НЕ собирается в mainline** — это изолированный прототип в зоне `docs/experiments/`.

## Что меряет

3 варианта реализации virtual texturing (per `README.md §3 Method`):

- **dense_atlas** — single dense 4 MiB atlas (current mainline pattern, no VT). Метрики:
  `create_atlas_us`, `peak_vram_mib`.
- **sparse_atlas** — `VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT` 4 MiB atlas + 64-page bind test.
  Метрики: `bind_64pages_us`, `peak_vram_mib`. **Requires** NVIDIA sparse residency support
  (RTX 3060 Ti Ampere = full per `hardware-profile.md`).
- **software_vt** — texture atlas (4 MiB) + R32Uint page table texture (16 KiB) + CPU LRU page
  manager. Метрики: `page_miss_us`, `peak_vram_mib`.

## Сборка

```bash
# Standalone build (NOT mainline cmake)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -std=c++26"
cmake --build build -j
```

**Dependencies** (all vendored in `external/`):

- Vulkan SDK headers (vendored в `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` или system `/usr/include/vulkan/`)
- VMA 3.4.0 (`external/VulkanMemoryAllocator/include/vk_mem_alloc.h`)
- volk (optional — VMA has `vmaImportVulkanFunctionsFromVolk` direct API per `VulkanBootstrap.cpp:814`)
- C++26 compiler (Clang 22.1.6 per `hardware-profile.md §6`)

## Запуск

```bash
# Dev host (RTX 3060 Ti, headless — no swapchain)
./build/vma_sparse_bench --variant=dense --iters=1000
./build/vma_sparse_bench --variant=sparse --iters=1000
./build/vma_sparse_bench --variant=software-vt --iters=1000
./build/vma_sparse_bench --variant=all --iters=1000

# Рекомендуется pinning CPU (single-thread workload, dev host 8C/16T)
taskset -c 2 ./build/vma_sparse_bench --variant=all --iters=1000
```

## Output

- `results.csv` — machine-readable (variant,metric,mean,median,p95,p99,stddev,min,max,n)
- stdout — human-readable summary table

## Caveats (per-prototype)

- **No swapchain, no present path.** Per `async-compute-overhead-numbers` precedent — pure
  resource cost measurement, accurate per-operation latency.
- **Synthetic workload** — page-miss pattern из `std::mt19937(42)` uniform random, not real
  chunk mutation events. Real pattern likely more spatially-coherent (player movement).
- **Single GPU vendor validated** (RTX 3060 Ti). AMD RDNA 4 + Intel Battlemage matrix is
  analytical projection per `dec-pipelines-async-compute` vendor matrix.
- **Sparse bind path is reference skeleton** — `BindSparsePages` validates API surface
  (`vkQueueBindSparse` + fence-wait) but does not produce valid image bind info structure
  (mip-level + subresource range details omitted). Operator should review `SaschaWillems/Vulkan
  texturesparseresidency` for production-grade sparse bind.
- **No driver overhead measurement** — bind latency measured wall-clock (CPU + GPU).
- **No validation layer** — pass `VK_LAYER_KHRONOS_validation` via env var if needed for debug.

## Files

- `vma_sparse_bench.hpp` — `Context` + `Stats` + utility structs (134 lines).
- `main.cpp` — 3 variant implementations + benchmark harness + CSV writer (~620 lines).
- `README.md` (this file) — build/run instructions + caveats.
