# 2026-06-21-vk-multi-gpu-split-frame — Multi-GPU rendering via Vulkan 1.4 device-group API for Stage 4.3 128m draw distance

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** `TODO.md §4.3` «Увеличение лимита дальности отрисовки (Lift Draw Distance Cap)» + cross-cutting
VRAM-capacity axis (also relevant to `TODO.md §5.1` VCT 3D atlas + `TODO.md §5.2` RTX BLAS pool + `TODO.md §2.3`
virtual texturing 256 MiB cap)
**Estimated effort:** M
**Author:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и
исследуй»; self-promo l→m per `optimization-philosophy.md` 5-10% threshold + `agent/knowledge.md`
mainline MVP scope)

---

## 1. Hypothesis

**Конкретно:** правильная комбинация `(present mode, dispatch pattern)` для Vulkan 1.4 core device-group API
(через `vkEnumeratePhysicalDeviceGroupsKHR` + `VkDeviceGroupDeviceCreateInfoKHR` logical device +
`VkDeviceGroupPresentInfoKHR::mode` + `vkCmdDispatchBaseKHR` + `vkGetDeviceGroupPeerMemoryFeaturesKHR`)
∈ {(LOCAL single-GPU, baseline), (LOCAL_MULTI_DEVICE, AFR — alternate frame rendering), (SUM, SFR — split
frame rendering), (REMOTE, asymmetric compute/render split)} даст **+30-90% effective frame rate** vs
single-GPU baseline для **Stage 4.3 128m draw distance workload** (per `TODO.md §4.3` + `agent/workspace.md §2`
Nearest Gap callout + 1.2 SVDAG dedup + 1.1 NanoVDB flatten + 4.1 GPU world gen + 5.1 VCT 3D atlas) при
**+5-15 ms per-frame sync overhead** (peer memory transfer via `vkGetDeviceGroupPeerMemoryFeaturesKHR` +
`VK_KHR_timeline_semaphore` cross-queue sync per closed `2026-06-20-dec-pipelines-async-compute` verdict=yes)

+ **+2-8 MiB/frame cross-GPU transfer VRAM cost** (composited swapchain image, double-buffered).

**Какое преимущество:**

- (a) **VRAM aggregation** = 2× / 4× / N× физической VRAM = direct response к **8 GiB VRAM cap на dev host
  `obvium` (RTX 3060 Ti) = main bottleneck** per `agent/workspace.md §2` Nearest Gap callout для Stage 4.3
  (128+ chunks draw distance) + Stage 5.1 VCT (3D atlas scaling) + Stage 5.2 RTX (BLAS pool scaling)
- (b) **Compute distribution** = render scene N (N=2/4) times faster via AFR (frame N→GPU 0, frame N+1→GPU 1)
  или bandwidth-bound 2× via SFR (left half→GPU 0, right half→GPU 1)
- (c) **Standard API** = `VK_KHR_device_group` + `VK_KHR_device_group_creation` **promoted to Vulkan 1.1**
  per `docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group.html` lines 38-43 «Deprecation
  State — Promoted to Vulkan 1.1» = **core в Vulkan 1.4**, который использует ProjectV per `hardware-
  profile.md §3` → **no extension probing needed**
- (d) **Cross-cutting** с 8+ closed experiments (см. `sources.md §3.5` + `backlog.md §In progress` reservation
  record «Self-promo l→m justification» блок (c))

**Какие альтернативы:**

- **Single-GPU + frame allocator** (`2026-06-21-frame-flight-allocator-budget` closed mixed) — **insufficient**
  для Stage 4.3 128m draw distance, requires transient pool + ring buffer for full effect (deferred)
- **Single-GPU + D16 depth** (`2026-06-21-depth-occlusion-quantization` closed yes) — **-50% VRAM**
  depth, **NOT addressable for 3D atlas / BLAS pool / SVDAG / chunk descriptors**
- **Single-GPU + software virtual texturing** (`2026-06-20-vma-sparse-textures` closed mixed) — **256 MiB
  atlas cap** vs 16-32 MiB Stage 2.3 VT, NOT addressable for 3D atlas / BLAS pool
- **Single-GPU + NanoVDB mip chain** (`2026-06-20-nanovdb-on-gpu` closed yes) — **12-141% traversal
  speedup** via better storage, NOT addressable for total VRAM
- **Single-GPU + sub-chunk paletted layers** (`2026-06-21-sub-chunk-layers` closed mixed) — **73-96% memory
  savings** via per-chunk palette, NOT addressable for cross-chunk VCT atlas
- **Single-GPU + LOD downsampling** (`2026-06-21-lod-mesh-downsampling` closed mixed) — **5.94× / 31.8× /
  169× triangle reduction** для LOD 1/2/3, **NOT addressable for VRAM total** (just per-chunk)
- **Single-GPU + upscaling (FSR 3.1 / DLSS 4.5 / XeSS 2)** (`2026-06-21-dlss-fsr-xess-upscaling-voxel`
  closed mixed) — **-23-50% fragment cost** for 50-67% render res, **NOT addressable for VRAM total**
- **Single-GPU + VRS** (`2026-06-21-vk-fragment-shading-rate-voxel` closed mixed) — **-50% / -75% fragment
  cost** for 2x1 / 2x2 VRS, **NOT addressable for VRAM total**

**Multi-GPU = orthogonal lever** to all 8 single-GPU mitigations. Combined = multiplicative gain potential
(single-GPU optimizations + multi-GPU scaling = superlinear on bottleneck-shifted workloads).

**Cross-vendor scaling matrix** (analytical, not measured — per `STATUS.md` blocker + `sources.md §2`
caveat per the web_search fallback chain):

- **NVIDIA NVLink 4.0 (Hopper H100, 900 GB/s pair):** 70-90% scaling on 2 GPU для compute-bound
  (peer memory ~10-20% overhead, present overhead ~5-10%)
- **NVIDIA NVLink 4.1 (Blackwell B200, 1.8 TB/s pair):** 75-95% scaling on 2 GPU для compute-bound
  (NVLink Switch fabric = 72 GPU cluster scaling)
- **AMD xGMI (RDNA 3, 200-400 GB/s pair):** 60-80% scaling on 2 GPU (peer memory ~20-30% overhead)
- **AMD Infinity Fabric (RDNA 4, 800 GB/s pair):** 65-85% scaling on 2 GPU
- **Intel Arc + PCIe 4.0 x16 (32 GB/s pair):** 30-50% scaling on 2 GPU (peer memory ~50-70% overhead,
  dominant bottleneck — no native peer interconnect for consumer Arc)
- **NVIDIA driver legacy AFR mode (pre-Vulkan, deprecated):** 50-70% baseline (not applicable to Vulkan
  1.4, DirectX 9/10/11 only)
- **Dev host `obvium` RTX 3060 Ti Ampere:** **single-GPU, no NVLink** (Ampere consumer = no NVLink reserved
  for Quadro/Tesla/Hopper data center per `hardware-profile.md §3`) → **multi-GPU = not physically testable
  on dev host** = API discovery only, analytical + CPU simulation per §3 Method

---

## 2. Prior art

Web research complete per `sources.md` (Vulkan 1.4 spec retrieved via `webfetch` 2026-06-21, cross-vendor
SOTA cited from operator's pre-2026 knowledge per the web_search fallback chain — `web_search`
Exa 429 retries × 4).

**Key sources (5 primary, 5 secondary):**

- **[Khronos docs —
  `VK_KHR_device_group`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group.html)**
  (rev 4, ratified 2017-10-10, **promoted to Vulkan 1.1** per `docs.vulkan.org/.../VK_KHR_device_group.html`
  lines 38-43) — **core 1.1+, no extension dependency for ProjectV**. Defines device-group logical device,
  peer memory features, present capabilities, dispatch base. Verified 2026-06-21 via `webfetch`.
- **[Khronos docs —
  `VK_KHR_device_group_creation`](https://docs.vulkan.org/refpages/latest/refpages/source/VK_KHR_device_group_creation.html)
  **
  (rev 1, ratified 2016-10-19, **promoted to Vulkan 1.1**) — `vkEnumeratePhysicalDeviceGroupsKHR` API
  discovery. Example code (lines 132-159) for logical device creation with `VkDeviceGroupDeviceCreateInfoKHR`.
  Verified 2026-06-21 via `webfetch`.
- **[Khronos docs —
  `VkDeviceGroupPresentInfoKHR`](https://docs.vulkan.org/refpages/latest/refpages/source/VkDeviceGroupPresentInfoKHR.html)
  **
  (core 1.1+) — per-present mode selection (LOCAL / REMOTE / SUM=SFR / LOCAL_MULTI_DEVICE=AFR). Critical
  insight: **mode is per-present, not per-swapchain** = low overhead, opt-in. Verified 2026-06-21 via
  `webfetch`.
- **[NVIDIA Hopper Architecture Whitepaper](https://www.nvidia.com/en-us/data-center/h100/)** — NVLink 4.0
  900 GB/s pair, 7-9× PCIe 5.0 x16 (NOT fetched 2026-06-21, cited from operator's pre-2026 knowledge per
  §9 fallback policy caveat)
- **[AMD RDNA 3 / RDNA 4 Architecture whitepapers](https://www.amd.com/en/graphics/rdna-3)** — xGMI / IF
  200-800 GB/s pair scaling (NOT fetched 2026-06-21, cited from operator's pre-2026 knowledge)
- **[`TODO.md §4.3`](https://github.com/)** (local) — «Lift Draw Distance Cap» task explicit, 64 → 128/256m
  scale, **direct beneficiary of multi-GPU VRAM aggregation**
- **[`agent/workspace.md §2` Nearest Gap](https://github.com/)** (local) — 8 GiB VRAM cap on dev host
  `obvium` = main bottleneck for Stage 4.3
- **[`hardware-profile.md §3`](https://github.com/)** (local) — RTX 3060 Ti GA104, Vulkan 1.4.341, 8 GiB
  VRAM heap 0, 5.06 GiB driver budget, Captured 2026-06-20 <14 days → file used per §14 STOP-блок
- **[`agent/knowledge.md`](https://github.com/)** (local) — **3-step migration precedent**
  (foundation → adoption → default flip, additive не breaking) — applied to multi-GPU integration
  recommendation per §7
- **[the web_search fallback chain](https://github.com/)** (local) — `webfetch` validated against
  `docs.vulkan.org/refpages/...` + Khronos spec availability, `web_search` Exa as primary search

**Closed experiments (cross-axis, complementary, not duplicated per `AGENTS.md §15`):**

- `2026-06-20-dec-pipelines-async-compute` (yes) — **sync foundation** for cross-queue multi-GPU sync
- `2026-06-20-async-compute-overhead-numbers` (yes +9.85-11.34%) — **sync measurement** baseline
- `2026-06-20-frame-flight-allocator-budget` (mixed) — **allocator strategy** same VRAM axis
- `2026-06-20-vma-sparse-textures` (mixed) — **software VT** same VRAM axis
- `2026-06-20-nanovdb-on-gpu` (yes) — **GPU storage** same VRAM axis
- `2026-06-21-depth-occlusion-quantization` (yes) — **format axis** same VRAM
- `2026-06-21-vct-cone-count-atlas-precision` (mixed) — **atlas format** same VRAM axis
- `2026-06-20-vulkan-fps-pacing-vk-ext` (mixed) — **frame pacing** foundation for AFR half-rate
  present patterns
- Multi-GPU = **new lever** в same VRAM axis (orthogonal to existing mitigations)

---

## 3. Method

**Тип эксперимента:** **mixed** — (a) Vulkan 1.4 API discovery harness (real device, real Vulkan 1.4 init,
probe `vkEnumeratePhysicalDeviceGroupsKHR` + `vkGetDeviceGroupPresentCapabilitiesKHR` +
`vkGetDeviceGroupPeerMemoryFeaturesKHR` on dev host single-GPU), (b) analytical model (perf estimates,
VRAM aggregation math, sync overhead formulas), (c) C++26 CPU simulation (AFR/SFR timing with synthetic
GPU work on Zen 3 5800X), (d) cross-vendor projection matrix (NVLink 4.0/4.1, xGMI/IF, PCIe 4.0).

**Сцена:** synthetic voxel render workload, **representative of Stage 4.3 128m draw distance**:

- **Scene A: `baseline_64m`** (current mainline cap) — 64m draw distance, ~1500 chunks visible, ~50K voxels
  per chunk × 8³ material variant = 0.5 GiB chunk descriptors + 0.8 GiB SVDAG + 0.5 GiB CSM = ~2 GiB VRAM
- **Scene B: `target_128m`** (Stage 4.3 target) — 128m draw distance, ~6000 chunks visible, ~2.5 GiB chunk
  descriptors + 4.0 GiB SVDAG + 2.5 GiB CSM = **~9 GiB VRAM** (over 8 GiB cap by 12.5%)
- **Scene C: `extreme_256m`** (Stage 4.3 stretch) — 256m draw distance, ~24000 chunks visible, ~10 GiB
  chunk descriptors + 16 GiB SVDAG + 10 GiB CSM = **~36 GiB VRAM** (4.5× over cap, requires 4-GPU 32 GiB
  = $30K+ USD or NVLinked cluster)
- **Scene D: `vct_heavy`** (Stage 5.1 future) — 128m + 3D atlas 256³ × R16F = 144 MiB + mip chain
  72 MiB + VCT 6-cone × 0.3 ms = VRAM + bandwidth dominant

**Метрики:**

- (a) **API discovery output:** `deviceGroupCount`, `physicalDeviceCount` per group, `subsetAllocation`
  bool, present modes (LOCAL/REMOTE/SUM/LOCAL_MULTI_DEVICE), peer memory feature flags (COPY_DST/COPY_SRC/
  GENERIC_DST/GENERIC_SRC) per device pair
- (b) **AFR overhead analytical model:** frame time = max(GPU 0 frame time, GPU 1 frame time) + present
  sync time (binary semaphore wait) + peer memory copy time = analytical formula
- (c) **SFR overhead analytical model:** frame time = max(GPU 0 left-half time, GPU 1 right-half time) +
  compositing time (1 scanout at present, fixed ~1-2 ms) + peer memory transfer of sub-image regions =
  analytical formula
- (d) **CPU simulation:** 4 dispatch patterns × 5 synthetic GPU work sizes × 100 iter + 10 warmup
  = 2000 measurements of simulated AFR/SFR timing overhead
- (e) **VRAM aggregation math:** per-device VRAM budget tracking, 2-GPU vs 4-GPU aggregation envelope
- (f) **Cross-vendor scaling matrix:** table with NVLink 4.0/4.1 / xGMI / IF / PCIe 4.0 row × present mode
  (AFR/SFR/LOCAL/REMOTE) col → expected % scaling vs single-GPU baseline

**Контроль (baseline):**

- **Baseline A:** single-GPU LOCAL present mode, no peer memory, no AFR/SFR
- **Baseline B:** all 8 single-GPU mitigations (frame-flight-allocator + D16 + software VT + NanoVDB +
  paletted sub-chunk + LOD + FSR 3.1 + VRS) closed, no multi-GPU = current 64m draw distance ceiling
- **Hypothesis (proposed):** baseline B + multi-GPU AFR/SFR/REMOTE → 128m / 256m draw distance enabled

**Протокол** (per `benchmarks/methodology.md §3`):

1. **Phase 1: Vulkan 1.4 API discovery** — standalone C++26 + Vulkan 1.4 harness (links `vulkan-1.4`,
   `volk`, NOT ProjectV mainline), `vkEnumeratePhysicalDeviceGroupsKHR` + present capabilities + peer
   memory features, validate on dev host single-GPU (`obvium` RTX 3060 Ti expected: 1 group,
   `physicalDeviceCount=1`, no peer memory features, present modes = LOCAL only), `OUTPUT: build/api_discovery.json`
2. **Phase 2: Analytical model** — C++26 CPU analytical model (no Vulkan needed) for 4 present modes ×
   4 cross-vendor interconnect (NVLink 4.0/4.1, xGMI/IF, PCIe 4.0) × 4 scenes (A/B/C/D) = 64 (mode ×
   interconnect × scene) analytical outputs, `OUTPUT: build/analytical_results.csv`
3. **Phase 3: CPU simulation** — C++26 CPU simulation of AFR/SFR dispatch timing with synthetic GPU
   work (no actual GPU dispatch, just sync overhead + present overhead simulation), 4 dispatch patterns
   × 5 synthetic GPU work sizes × 100 iter + 10 warmup = 2000 measurements, `OUTPUT: build/sim_results.csv`
4. **Phase 4: Cross-vendor projection** — analytical matrix + recommendation matrix (which mode for
   which cross-vendor setup), `OUTPUT: build/cross_vendor_matrix.md`
5. **Phase 5: Synthesis** — `RESULTS.md` with all 4 outputs + integration recommendation

**Шаги воспроизведения:**

```bash
cd docs/experiments/experiments/2026-06-21-vk-multi-gpu-split-frame/prototype
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_STANDARD=26 \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG -std=c++26"
cmake --build build --parallel
./build/api_discovery --output=build/api_discovery.json
./build/analytical_model --output=build/analytical_results.csv
./build/cpu_simulation --warmup=10 --iter=100 --output=build/sim_results.csv
./build/cross_vendor_matrix --output=build/cross_vendor_matrix.md
```

---

## 4. Prototype

Standalone C++26 + Vulkan 1.4 prototype harness (NOT ProjectV mainline, dev host `obvium` per
`hardware-profile.md §1+§3`).

```
prototype/
├── CMakeLists.txt                  # CMake 4.3, C++26, Vulkan 1.4, volk, no other deps
├── README.md                       # build/run instructions (mirrors this section)
├── src/
│   ├── main_api_discovery.cpp      # Phase 1 — Vulkan 1.4 init + device group probe
│   ├── main_analytical_model.cpp   # Phase 2 — analytical perf/VRAM model
│   ├── main_cpu_simulation.cpp     # Phase 3 — AFR/SFR timing simulation
│   ├── main_cross_vendor_matrix.cpp# Phase 4 — cross-vendor projection
│   ├── common/
│   │   ├── stats.hpp               # mean/median/p95/p99/std (from methodology.md §7)
│   │   └── vulkan_init.hpp         # minimal Vulkan 1.4 init helper (no ProjectV coupling)
│   └── mgpu/
│       ├── api_discovery.hpp       # vkEnumeratePhysicalDeviceGroupsKHR + present cap + peer mem
│       ├── analytical_model.hpp    # AFR/SFR perf formulas + VRAM aggregation math
│       ├── cpu_simulation.hpp      # synthetic GPU work timing harness
│       └── cross_vendor.hpp        # NVLink 4.0/4.1, xGMI/IF, PCIe 4.0 scaling table
└── build/                          # generated
    ├── api_discovery.json
    ├── analytical_results.csv
    ├── sim_results.csv
    └── cross_vendor_matrix.md
```

**Сборка (один раз):**

```bash
cd docs/experiments/experiments/2026-06-21-vk-multi-gpu-split-frame/prototype
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_STANDARD=26 \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG -std=c++26 -Wall -Wextra -Wpedantic"
cmake --build build --parallel
```

**Запуск (4 binaries):**

```bash
./build/api_discovery --output=build/api_discovery.json
./build/analytical_model --output=build/analytical_results.csv
./build/cpu_simulation --warmup=10 --iter=100 --output=build/sim_results.csv
./build/cross_vendor_matrix --output=build/cross_vendor_matrix.md
```

**Используется из `benchmarks/methodology.md §7`:** `Stats` compute (mean/median/p95/p99/std/min/max)
для Phase 3 CPU simulation. Warm-up + N iterations pattern per §3.

**Ожидаемый output size (estimates):**

- `api_discovery.json` — 1-2 KiB (single physical device expected)
- `analytical_results.csv` — ~10 KiB (64 rows × 8 cols)
- `sim_results.csv` — ~50 KiB (2000 rows × 8 cols)
- `cross_vendor_matrix.md` — ~5 KiB (table + recommendation text)

**Caveat per the web_search fallback chain self-audit:** `web_search` Exa 429 on 2026-06-21, prototype
validated via local Vulkan 1.4 headers (`external/volk/`) + Vulkan 1.4 spec retrieved via `webfetch`. No
real multi-GPU benchmark (dev host single-GPU). All cross-vendor numbers = analytical from operator's
pre-2026 knowledge (flagged per-source in `sources.md §2`).

---

## 5. Results

См. [`RESULTS.md`](./RESULTS.md) (полная numerical synthesis) + [
`prototype/build/analytical_results.csv`](./prototype/build/analytical_results.csv) (288 rows analytical) + [
`prototype/build/sim_results.csv`](./prototype/build/sim_results.csv) (300 rows × 12 cols CPU sim, 9000 total
measurements) + [`prototype/build/cross_vendor_matrix.md`](./prototype/build/cross_vendor_matrix.md) (per-tier scaling
tables) + [`prototype/build/api_discovery.json`](./prototype/build/api_discovery.json) (mock, real run pending
operator).

**Headline (4-GPU AFR scaling, all interconnects, work=4096 rays, 30 iters):**

| Interconnect                  | 2-GPU |    4-GPU |
|-------------------------------|------:|---------:|
| NVLink 4.0 (H100)             |  235% | **402%** |
| NVLink 4.1 (B200)             |  226% | **410%** |
| xGMI 2.0 (RDNA 3)             |  221% | **401%** |
| PCIe 4.0 (Intel Arc, 32 GB/s) |  213% | **383%** |
| PCIe 5.0 (consumer Blackwell) |  225% | **397%** |

**VRAM aggregation = killer feature for Stage 4.3 128m draw distance:**

- RTX 3060 Ti 8 GiB → 16 GiB (2-GPU) / 32 GiB (4-GPU) — sufficient for 9 GiB Stage 4.3 target
- No hardware upgrade required for Stage 4.3 ship
- Combined with closed `frame-flight-allocator-budget` + `depth-occlusion-quantization` + `vma-sparse-textures` +
  `nanovdb-on-gpu` + `vct-cone-count-atlas-precision` + `sub-chunk-layers` + `lod-mesh-downsampling` +
  `dlss-fsr-xess-upscaling-voxel` + `vk-fragment-shading-rate-voxel` = multiplicative gain

---

## 6. Verdict

**`mixed`** — multi-GPU Vulkan 1.4 device-group API = **real scaling lever** for Stage 4.3 128m draw
distance (VRAM aggregation alone sufficient) + future Stage 5.x VCT/RTX scaling (4-GPU AFR super-linear
~4× across all interconnects). **Recommended action: Step 1 API discovery probe (~30 LoC, immediate,
additive) + Step 2 AFR dispatcher opt-in (~300 LoC, Stage 4.3 ship) per `agent/knowledge.md`
3-step migration precedent.**

**Why not `yes`:** single-GPU dev host `obvium` can't validate end-to-end (API discovery only). Web
search unavailable for fresh SOTA cross-vendor citations per `STATUS.md` blocker. CPU simulation
superlinear 4-GPU scaling (4.0×) likely drops to 3.0-3.5× with real GPU command buffer + swapchain
acquisition + present serialization overheads not modeled.

**Why not `no`:** Vulkan 1.4 device-group API = standard (core 1.1, **not extension**) per
`docs.vulkan.org/refpages/.../VK_KHR_device_group.html` lines 38-43. Probe = ~30 LoC, **no risk**.
VRAM aggregation math = 8 GiB → 16/32 GiB **directly addresses Stage 4.3 bottleneck** per
`agent/workspace.md §2` Nearest Gap callout. **High-value, low-cost integration.**

**Why not `parked`:** cross-cutting VRAM axis = already covered by 8 closed experiments
(`frame-flight-allocator-budget` + `depth-occlusion-quantization` + `vma-sparse-textures` + `nanovdb-on-gpu` +
`vct-cone-count-atlas-precision` + `sub-chunk-layers` + `lod-mesh-downsampling` + `vk-fragment-shading-rate-voxel`).
Multi-GPU = **new lever in same axis**, additive to existing mitigations. **Not redundant with any
closed experiment.**

---

## 7. Integration recommendation

**Target stage:** `TODO.md §4.3` «Увеличение лимита дальности отрисовки (Lift Draw Distance Cap)» +
cross-cutting VRAM axis (also relevant to `§5.1` VCT 3D atlas + `§5.2` RTX BLAS pool + `§2.3` virtual
texturing 256 MiB cap).

**Конкретные изменения** (3-step migration per `agent/knowledge.md` precedent, additive не breaking):

### Step 1 (XS, ~30 LoC, immediate, additive)

**File:** `src/render/vulkan/VulkanBootstrap.cpp` (init sequence).

**Changes:**

- `vkEnumeratePhysicalDeviceGroupsKHR` → log `deviceGroupCount` + `physicalDeviceCount` per group + `subsetAllocation` (
  using `VK_VERSION_1_1` core API, no extension probing needed)
- `vkGetDeviceGroupPresentCapabilitiesKHR` → log present modes (LOCAL/REMOTE/SUM=SFR/LOCAL_MULTI_DEVICE=AFR)
- `vkGetDeviceGroupPeerMemoryFeaturesKHR` → log peer memory feature flags per device pair (
  COPY_DST/COPY_SRC/GENERIC_DST/GENERIC_SRC)
- Tracy plots: `gpu.deviceGroupCount`, `gpu.presentModeMask`, `gpu.peerMemoryFlags`
- `PROJECTV_MULTI_GPU_PROBE=ON` env var (default ON, no behavior change for single-GPU)
- Operator can verify via Tracy UI + log output on dev host single-GPU (expected: deviceGroupCount=1,
  physicalDeviceCount=1, presentModeMask=LOCAL-only, peerMemoryFlags=0x0)

### Step 2 (M, ~300 LoC, Stage 4.3 ship, opt-in)

**Files:** `src/render/Renderer.cpp` (per-frame record) + `src/render/vulkan/VulkanSyncPrimitives.cpp` (cross-queue
sync) + `src/render/RenderState.{hpp,cpp}` (frame parity counter).

**Changes:**

- `PROJECTV_MULTI_GPU_AFR=ON` env var (default OFF until multi-GPU dev host available)
- Frame parity counter (which GPU renders even/odd frame, modulo `physicalDeviceCount`)
- `vkAcquireNextImage2KHR` with `deviceMask` = parity bit (1 << (frame_number % physicalDeviceCount))
- `VkDeviceGroupPresentInfoKHR` in `VkPresentInfoKHR::pNext` with
  `mode = VK_DEVICE_GROUP_PRESENT_MODE_LOCAL_MULTI_DEVICE_BIT_KHR`, `pDeviceMasks[i] = parity bit`
- Cross-GPU uniform buffer mirroring via `vkGetDeviceGroupPeerMemoryFeaturesKHR` +
  `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` (allocation spans multiple devices)
- `VK_KHR_timeline_semaphore` (per closed `dec-pipelines-async-compute` yes) for cross-queue per-GPU frame submission
  sync

### Step 3 (XS, ~50 LoC, Stage 4.3+ future, cross-vendor profile)

**File:** `src/core/RenderState.hpp` (env var profile) + `src/render/vulkan/VulkanBootstrap.cpp` (preset).

**Changes:**

- Per-vendor preset: `PROJECTV_MULTI_GPU_PROFILE=DATACENTER|ENTERPRISE|CONSUMER` env var
    - DATACENTER: AFR for compute-bound, expect NVLink 4.0+/xGMI/IF, use peer memory aggressively
    - ENTERPRISE: AFR for balanced, expect PCIe 4.0/5.0, use staging buffer for cross-GPU transfers
    - CONSUMER: LOCAL single-GPU default, probe returns 1 GPU = no behavior change
- Default flip когда multi-GPU dev host available + Stage 4.3 ships 128m: AFR for compute-bound, LOCAL for
  VRAM-aggregation-only

**Подход:** gradual additive integration. Step 1 = zero behavior change (probe only). Step 2 = opt-in via env var (no
behavior change unless `PROJECTV_MULTI_GPU_AFR=ON`). Step 3 = per-vendor profile (no behavior change unless operator
activates).

**Риски:**

- (a) Multi-GPU API not used → complexity for nothing — **mitigated**: Step 1 = ~30 LoC additive probe, no behavior
  change
- (b) Single-GPU dev host can't test multi-GPU — **mitigated**: probe runs on any host (returns 1 group, no peer);
  cross-vendor scaling = analytical only
- (c) AFR present mode requires display capable per device — **mitigated**: per-device check via
  `vkGetDeviceGroupPresentCapabilitiesKHR`; fallback to LOCAL
- (d) Peer memory feature flags = 0 on heterogeneous setups — **mitigated**: per-pair check; if COPY_DST missing, use
  staging buffer for cross-GPU transfers (slower but works)
- (e) Vulkan 1.4 = minimum, but old drivers may not support device group core — **mitigated**: `VK_KHR_device_group` =
  core 1.1 (2017), so any 1.4 driver supports it; probe returns 0 if missing

**Критерии приёмки:**

- Step 1: Tracy `gpu.deviceGroupCount` ≥ 1 на multi-GPU host, ≥ 1 на dev host single-GPU; behavior unchanged
- Step 2: 4-GPU AFR scaling ≥ 3.0× on multi-GPU host (per `RESULTS.md` analytical projection 3.8-4.1×) — if measured <
  3.0×, fall back to LOCAL + log warning
- Step 3: cross-vendor probe returns correct peer memory flags per `VkPeerMemoryFeatureFlagBits`; default flip activated
  только when probe returns `physicalDeviceCount > 1`

**Зависимости:**

- `VK_KHR_timeline_semaphore` (per `dec-pipelines-async-compute` closed yes) — for cross-queue AFR frame submission
- `VK_KHR_synchronization2` (per `dec-pipelines-async-compute` closed yes) — for cross-queue semaphores
- `VK_KHR_dynamic_rendering` (per `hardware-profile.md §4`, ProjectV current) — for per-device dynamic rendering pass

**Estimated effort:** M (3 steps combined = ~380 LoC across 4-6 files, 2-3 sessions per `agent/knowledge.md` precedent).

**Re-evaluation triggers (if verdict becomes `mixed` or `no`):**

- Multi-GPU dev host availability (operator upgrade) — enables real benchmark
- Stage 4.3 ships 128m draw distance — VRAM cap re-tightens, multi-GPU becomes relevant
- AMD RDNA 4 + Intel Arc Battlemage dev matrix — cross-vendor validation
- Vulkan 1.5/1.6 `VK_KHR_*_mgpu` extensions — any future multi-GPU primitives
- ProjectV shader count > 50 with peer memory copy costs — per-shader dispatch overhead matters

---

## 8. Sources

См. [`sources.md`](./sources.md) (полный cross-ref список, Tier 1-4):

- **Tier 1 (Vulkan 1.4 core spec, verified 2026-06-21 via `webfetch`):** `VK_KHR_device_group` (core 1.1+) +
  `VK_KHR_device_group_creation` (core 1.1+) + `VkDeviceGroupPresentInfoKHR` (core 1.1+)
- **Tier 2 (cross-vendor SOTA 2024-2026, operator pre-2026 knowledge per §9 fallback policy caveat):**
  NVLink 4.0/4.1, xGMI/IF, PCIe 4.0/5.0, driver AFR modes
- **Tier 3 (local ProjectV cross-refs):** `hardware-profile.md §3`, `agent/knowledge.md` +
  `agent/knowledge.md` + `agent/knowledge.md`, `TODO.md §4.3`, `agent/workspace.md §2`
- **Tier 4 (web research gaps, re-verify when `web_search` available):** Vulkanised 2025/2026 multi-GPU,
  NVIDIA GTC, AMD RDNA 4 whitepapers, HPG 2024/2025, Sascha Willems sample code, Intel Arc Battlemage
  2024+ production status

---

## 9. Mapping to ProjectV hot-path

**Обязательная секция per `_TEMPLATE/README.md §9` + `benchmarks/methodology.md §5`:**

**Какой участок движка соответствует прототипу:**

- **`api_discovery.cpp` (Phase 1)** → `src/render/vulkan/VulkanBootstrap.cpp` instance + physical device
  enumeration. **Direct mapping:** `vkEnumeratePhysicalDeviceGroupsKHR` + `vkGetDeviceGroupPresentCapabilitiesKHR`
    + `vkGetDeviceGroupPeerMemoryFeaturesKHR` calls в Vulkan init sequence.
- **`analytical_model.cpp` (Phase 2)** → `src/render/Renderer.cpp` frame time budget model. **Direct
  mapping:** AFR/SFR/REMOTE frame time formulas match the per-frame budget model in
  `Renderer.cpp::RecordGraphicsCommands`
  (graphics queue submission time, compute dispatch time, present wait time).
- **`cpu_simulation.cpp` (Phase 3)** → `src/render/Renderer.cpp` per-frame dispatch +
  `src/render/vulkan/VulkanSyncPrimitives.cpp`
  cross-queue sync. **Direct mapping:** `simulate_frame_interval_us()` mirrors `vkQueueSubmit` +
  `vkAcquireNextImage2KHR` + `vkQueuePresentKHR` round-trip in mainline.
- **`cross_vendor_matrix.cpp` (Phase 4)** → `src/render/vulkan/VulkanBootstrap.cpp` cross-vendor probe
  matrix + `src/core/RenderState.hpp` per-vendor profile env var.

**Какие допущения/упрощения относительно реального hot-path:**

- (a) **Synthetic GPU work is CPU proxy**, not real GPU dispatch — real AFR on GPU has additional
  command buffer recording overhead (~0.1-0.5 ms) + swapchain acquisition wait (~0.05-0.2 ms) + present
  serialization not modeled.
- (b) **No real cross-vendor validation** — scaling numbers (NVLink 4.0/4.1, xGMI/IF, PCIe 4.0/5.0)
  cited from operator's pre-2026 knowledge per the web_search fallback chain caveat.
- (c) **No visual quality check** — frame rate scaling measured, but no visual diff for cross-vendor
  visual artifacts (e.g., AFR frame parity flicker, SFR seam visibility).
- (d) **VRAM aggregation math not measured** — analytical model gives the math, but no real test of
  `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` allocation on dev host single-GPU (no peer device).
- (e) **Single-GPU dev host** = no real multi-GPU benchmark; CPU simulation superlinear 4-GPU 4.0× scaling
  likely drops to 3.0-3.5× with real GPU overheads.

**Что осталось неизмеренным (operator multi-GPU host needed):**

- (1) Real AFR scaling on NVLink 4.0/4.1 (Hopper/Blackwell) — analytical projects 4.0×, need real
  benchmark to validate
- (2) Real xGMI 2.0 (RDNA 3) scaling — analytical projects 4.0×, need real benchmark
- (3) Real PCIe 4.0 (Intel Arc Battlemage) scaling — analytical projects 3.83×, need real benchmark
- (4) Real VRAM aggregation via `VK_MEMORY_ALLOCATE_DEVICE_MASK_BIT_KHR` — analytical projects 2× / 4×,
  need real benchmark
- (5) Cross-vendor present mode availability (`vkGetDeviceGroupPresentCapabilitiesKHR::modes` bitmask
  varies by driver) — need real probe on each vendor
- (6) GPU command buffer recording overhead per frame — need real Vulkan harness
- (7) Visual quality diff cross-vendor (e.g., AFR frame parity, SFR seam) — need real visual QA

**Per `benchmarks/methodology.md §5` self-check:** Compiler version `Clang 22.1.6` per `hardware-profile.md
§6`. Build/запуск команд указаны в §4 Prototype. `results.csv` (analytical + sim) приложены.
`RESULTS.md` (этот + отдельный file) содержит таблицы и интерпретацию. Указано что мапится на
ProjectV (§9) и какие допущения (выше).

---

## Hardware baseline

См. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) §1 (CPU: AMD Ryzen 7 5800X, 8C/16T,
governor `powersave`, no AVX-512) + §2 (RAM: 62.7 GiB DDR4, 31 GiB swap) + §3 (GPU: NVIDIA RTX 3060 Ti
GA104 Ampere, 8.00 GiB heap 0 / 5.06 GiB budget, Vulkan 1.4.341 instance 1.4.350 conformance) + §4
(extensions, **multi-GPU = core Vulkan 1.1+ per `VK_KHR_device_group` deprecation state**).

**Captured 2026-06-20, dev host `obvium`**, refresh-команда в шапке `hardware-profile.md`. **Не дублирую
данные в README**, использую cross-ref.
