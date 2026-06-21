# Hardware profile — dev host для ProjectV research

> ## ⛔ Перед ЧТЕНИЕМ этого файла для агентов
>
> **Не запускай hardware-probe (`lscpu`, `free`, `vulkaninfo`, `nvidia-smi`, `dmidecode`, `lshw`, `uname -a`,
> `cat /proc/cpuinfo`) если этот файл существует и дата в шапке <14 дней назад.**
>
> Полный протокол: [`AGENTS.md`](./AGENTS.md) — STOP-блок в начале файла + §14 «Hardware profile reference».
> Краткая версия:
> - Дата в шапке **<14 дней** → использовать файл, **не запускать probe**.
> - Дата **≥14 дней** или файл отсутствует → запустить refresh-команду ниже, обновить данные, обновить дату.
> - Любой probe «для проверки» при свежем файле = **ЗАПРЕЩЁННЫЙ ритуал**.

**Captured:** 2026-06-21 (refresh: добавлен `VK_KHR_present_mode_fifo_latest_ready` row в §4 — ratified 2025-03-18,
поддержка подтверждена на dev host driver 610.43.2.0 per `vulkaninfo 2026-06-21` probe для
`experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/`; остальные секции unchanged от 2026-06-20 capture).
**Hostname:** `obvium`
**Refresh:** `bash -c "$(cat <<'EOF'
  echo '== CPU =='; lscpu | grep -E "Model name|CPU\(s\)|Core\(s\)|Thread\(s\)|L[1-3] cache|MHz|Microcode"; \
  echo '== RAM =='; free -h | head -2; \
  echo '== GPU =='; vulkaninfo --summary 2>/dev/null | grep -E "deviceName|apiVersion|driverVersion|driverName"; \
  echo '== OS =='; uname -sr; head -3 /etc/os-release; \
  echo '== Tools =='; clang++ --version | head -1; ld.lld --version | head -1; cmake --version | head -1; \
  echo '== VRAM =='; nvidia-smi --query-gpu=memory.total,memory.used --format=csv,noheader,nounits
EOF
)"`
**Verify:** когда host меняется (новый GPU / CPU / OS) — обновить + сообщить оператору.

---

## 1. CPU

| Параметр          | Значение                                  |
|:------------------|:------------------------------------------|
| Model             | AMD Ryzen 7 5800X                         |
| Microarchitecture | Zen 3 (family 25, model 33, stepping 2)   |
| Cores / Threads   | 8 / 16                                    |
| Sockets           | 1                                         |
| L1d / L1i         | 256 KiB / 256 KiB (per core, 8 instances) |
| L2                | 4 MiB (per core, 8 instances)             |
| L3                | 32 MiB (shared, 1 instance)               |
| Base / Boost      | 573 MHz / 5007 MHz                        |
| Governor          | `powersave` (driver `amd-pstate-epp`)     |
| NUMA              | 1 node (CPUs 0-15)                        |
| TDP               | 105 W                                     |

**ISA-флаги (selected, релевантные для ProjectV):**

- ✅ **AVX2, FMA, BMI1, BMI2** — Stage 1.x/3.x SIMD для greedy meshing / SVDAG walks.
- ✅ **SSE 4.2, AES-NI, SHA-NI, ADX, VAES, VPCLMULQDQ** — secondary.
- ❌ **AVX-512** — **отсутствует** (Zen 3 не поддерживает). Compiler НЕ должен auto-vectorize в AVX-512. Циклы в
  `voxel_mesh.comp`-like CPU-коде остаются AVX2/FMA cap.
- ❌ **SVE / SVE2** — ARM-only.
- ⚠️ **RDTSCP, INVPCID, CLWB, CLFLUSHOPT** — для cache management + Tracy sampling.

**Риск / заметка:** 5800X = consumer desktop CPU, не server. 8C = меньше чем HEDT/server. Многопоточные workloads (Stage
6.1 ECS, Stage 3.x parallel physics) **должны** измерять scaling per-core carefully.

---

## 2. Memory

| Параметр    | Значение                      |
|:------------|:------------------------------|
| Total       | 62.7 GiB (65759008 kB)        |
| Used / Free | 19 GiB / 3.8 GiB              |
| Buff/cache  | 40 GiB                        |
| Available   | 42 GiB                        |
| Swap        | 31 GiB (использовано 6.6 GiB) |
| zram        | 31.4 GiB                      |

**Speed / type:** ⚠️ не извлечено (нет `dmidecode` без sudo, `lshw` не установлен). По платформе Zen 3 — **DDR4**,
типично 3200-3600 MT/s. Если нужно для memory-bound benchmarks — оператор может предоставить `dmidecode` output.

**Риск / заметка:** `zram` на 31 GiB + swap 31 GiB. Memory pressure активный (`buff/cache = 40 GiB`). Allocation-heavy
workloads (Stage 4.1 batch generation, full-world snapshots) могут использовать swap → latency spike. **TracyPlot
на `swap usage` рекомендуется**.

---

## 3. GPU

| Параметр           | Значение                                              |
|:-------------------|:------------------------------------------------------|
| Device             | NVIDIA GeForce RTX 3060 Ti (GA104, Ampere)            |
| VRAM               | **8.00 GiB** (heap 0), budget 5.06 GiB (driver limit) |
| VRAM in-use        | 2.65 GiB (other apps)                                 |
| Shared system RAM  | 47.03 GiB (heap 1)                                    |
| Driver             | NVIDIA 610.43.02 (`DRIVER_ID_NVIDIA_PROPRIARY`)       |
| Vulkan API version | **1.4.341** (instance 1.4.350, conformance 1.4.3.3)   |
| GPU clock (boost)  | 2100 MHz                                              |
| Memory clock       | 7001 MHz (GDDR6, 14 Gbps effective)                   |

**Vulkan-relevant properties:**

| Limit                             | Value           | Релевантно для                          |
|:----------------------------------|:----------------|:----------------------------------------|
| `subgroupSize`                    | 32              | Stage 2.1 mesh shader (warp-aware code) |
| `maxMeshOutputVertices`           | 256             | Stage 2.1 meshlet upper bound           |
| `maxMeshOutputPrimitives`         | 256             | Stage 2.1 meshlet upper bound           |
| `maxMeshSharedMemorySize`         | 28672 B (28 KB) | Stage 2.1 LDS budget                    |
| `maxTaskWorkGroupTotalCount`      | 4194304         | Stage 2.1 task shader dispatch ceiling  |
| `maxTaskPayloadSize`              | 16384 B (16 KB) | Stage 2.1 task→mesh payload             |
| `maxComputeSharedMemorySize`      | 49152 B (48 KB) | Stage 3.1 GPU Fluid CA LDS              |
| `maxComputeWorkGroupInvocations`  | 1024            | All compute shaders                     |
| `maxPushConstantsSize`            | 256 B           | All shaders                             |
| `minStorageBufferOffsetAlignment` | 16 B            | SSBO layout                             |
| `maxPerStageDescriptor*`          | 1048576         | Bindless (Stage 2.3 virtual texturing)  |
| `maxDescriptorSet*Dynamic`        | 31 total        | Per-set dynamic buffers                 |

**Риск / заметка:** 8 GiB VRAM — small. Stage 1.x SVDAG dedup wins critical (50-100× per `TODO.md §1.2`). Stage 4.3 lift
draw distance (128+ chunks) **может** упереться в VRAM budget.

---

## 4. Vulkan extensions (subset релевантный ProjectV)

Все поддерживаются на dev host (driver 610.43.02):

| Extension                                          | Rev | Стадия ProjectV                   |
|:---------------------------------------------------|:----|:----------------------------------|
| `VK_KHR_acceleration_structure`                    | 13  | **Stage 5.2** RTX BLAS            |
| `VK_KHR_ray_query`                                 | 1   | **Stage 5.2** RTX ray query       |
| `VK_KHR_ray_tracing_pipeline`                      | 1   | **Stage 5.2** (optional)          |
| `VK_KHR_ray_tracing_position_fetch`                | 1   | Stage 5.2 (motion blur)           |
| `VK_KHR_ray_tracing_maintenance1`                  | 1   | Stage 5.2                         |
| `VK_EXT_mesh_shader`                               | 1   | **Stage 2.1** (mesh path)         |
| `VK_NV_mesh_shader`                                | 1   | Stage 2.1 (NVIDIA-native, faster) |
| `VK_KHR_spirv_1_4`                                 | 1   | Stage 2.1 prereq                  |
| `VK_KHR_dynamic_rendering`                         | 1   | **ProjectV current**              |
| `VK_KHR_dynamic_rendering_local_read`              | 1   | Vulkan 1.4 feature                |
| `VK_KHR_synchronization2`                          | 1   | ProjectV current                  |
| `VK_KHR_timeline_semaphore`                        | 2   | ProjectV current                  |
| `VK_KHR_push_descriptor`                           | 2   | ProjectV current                  |
| `VK_KHR_draw_indirect_count`                       | 1   | **Stage 2.2** HZB cull            |
| `VK_KHR_create_renderpass2`                        | 1   | ProjectV current                  |
| `VK_KHR_swapchain` + maintenance1 + mutable_format | —   | ProjectV current                  |
| `VK_KHR_deferred_host_operations`                  | 4   | Stage 5.2 (BLAS build)            |
| `VK_KHR_present_mode_fifo_latest_ready`          | 1   | **Stage 0** frame pacing (added 2026-06-21 per `2026-06-21-vulkan-fps-pacing-wayland-prototype`) |
| `VK_KHR_buffer_device_address` (via features)      | —   | ProjectV current                |
| `VK_EXT_conservative_rasterization`                | 1   | future option                     |
| `VK_EXT_conditional_rendering`                     | 2   | future option                     |
| `VK_EXT_extended_dynamic_state[2,3]`               | 1-2 | ProjectV current                  |
| `VK_EXT_host_image_copy`                           | 1   | Stage 1.3 async streamer          |
| `VK_EXT_shader_atomic_float`                       | 1   | GPU Fluid CA (Stage 3.1)          |
| `VK_EXT_shader_demote_to_helper_invocation`        | 1   | Optional                          |
| `VK_EXT_robustness2`                               | 1   | ProjectV current                  |
| `VK_EXT_vertex_input_dynamic_state`                | 2   | ProjectV current                  |
| `VK_EXT_transform_feedback`                        | 1   | Optional                          |
| `VK_NV_compute_shader_derivatives`                 | 1   | Optional                          |
| `VK_NV_ray_tracing_invocation_reorder`             | 1   | Optional                          |
| `VK_NV_ray_tracing_motion_blur`                    | 1   | Optional                          |

**⚠️ Validation layer:** `VK_LAYER_KHRONOS_validation 1.4.350` — установлен, ProjectV использует.

---

## 5. OS / kernel

| Параметр | Значение                                        |
|:---------|:------------------------------------------------|
| Distro   | Arch Linux (rolling)                            |
| Kernel   | 7.0.12-zen1-1-zen (Zen kernel, PREEMPT_DYNAMIC) |
| glibc    | 16.x (per `GCC 16.1.1` build)                   |

**Zen kernel:** `-zen` patchset добавляет performance tweaks + `-march=native`-friendly defaults. **Полезно** для
release builds, **но** Zen kernel не обязательно = mainline user setup. Cross-host validation требует тестов на
`-stable` и `-lts`.

---

## 6. Toolchain

| Tool             | Version                          | Заметка                                                                                                                                                                                     |
|:-----------------|:---------------------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Clang            | **22.1.6**                       | Per `agent/knowledge.md §17` Linux baseline                                                                                                                                                 |
| LLD              | **22.1.6**                       | `CMAKE_LINKER_TYPE=LLD`                                                                                                                                                                     |
| libstdc++        | 16.1.1 (GCC build)               | Symbol `GLIBCXX_3.4.35` (latest)                                                                                                                                                            |
| CMake            | 4.3.3                            | CMake 4.x                                                                                                                                                                                   |
| SDL3             | 3.4.10                           | Per `agent/knowledge.md §17`                                                                                                                                                                |
| Vulkan loader    | 1.4.350.0                        | `vulkaninfo` summary uses this                                                                                                                                                              |
| glslangValidator | 16.3.0                           | `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` also vendored                                                                                                                                 |
| glslc            | 2026.2 (1.4.350.0)               | Vulkan SDK glslc                                                                                                                                                                            |
| Tracy            | **vendored** в `external/tracy/` | Per `agent/knowledge.md §4`: `linux-clang-debug-tracy-profiler` preset = instrumentation ON, UI binary OFF (upstream nlohmann_json collision). UI build = `tools/tracy-standalone/` script. |

**`vulkaninfo` путь:** `/usr/sbin/vulkaninfo` (system). ProjectV vendors full Vulkan SDK docs в
`legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` per `AGENTS.md §3`.

---

## 7. Storage

| Device    | Size      | Model                | ROTA | Type | Mount   |
|:----------|:----------|:---------------------|:-----|:-----|:--------|
| `nvme0n1` | 931.5 GiB | KINGSTON SNV2S1000G  | 0    | NVMe | `/home` |
| `sda`     | 953.9 GiB | P3-1TB (Crucial)     | 0    | SSD  | (other) |
| `sdb`     | 3.6 TiB   | WDC WD40EFAX-68JH4N1 | 1    | HDD  | (other) |
| `sdc`     | 186.3 GiB | WDC WD2000JS-55NCB1  | 1    | HDD  | (other) |
| `sdd`     | 1.8 TiB   | Expansion (USB)      | 1    | HDD  | (other) |
| `zram0`   | 31.4 GiB  | —                    | 0    | RAM  | swap    |

`/home` = NVMe. ProjectV source tree, build dir, snapshots = NVMe = fast.
`/tmp` = tmpfs 32 GiB = RAM = ultra-fast (per `agent/knowledge.md §17`).
Bulk storage (saves, captures) = HDD = slow. **Stage 1.3 async audio scan, Stage 4.1 batch world gen** — test on NVMe (
representative), validate on HDD (worst-case disk).

---

## 8. Per-stage references

| ProjectV stage / feature             | Hardware data needed                                                                        | File section |
|:-------------------------------------|:--------------------------------------------------------------------------------------------|:-------------|
| **Stage 1.1 Sparse 64-tree**         | CPU L1/L2/L3 cache sizes (cache-line sweet spot)                                            | §1           |
| **Stage 1.2 SVDAG dedup**            | VRAM (8 GiB cap → dedup ratio matters)                                                      | §3           |
| **Stage 1.3 Async audio scan**       | Disk RTT (NVMe vs HDD comparison)                                                           | §7           |
| **Stage 2.1 Mesh shader**            | mesh shader limits (256/256, 28 KB LDS) + `subgroupSize`                                    | §3           |
| **Stage 2.2 HZB cull**               | `draw_indirect_count` ext + VRAM                                                            | §3, §4       |
| **Stage 2.3 Virtual texturing**      | `maxDescriptorSet*` + bindless support                                                      | §3           |
| **Stage 3.1 GPU Fluid CA**           | `maxComputeSharedMemorySize` (48 KB) + atomic_float                                         | §3, §4       |
| **Stage 4.1 GPU world gen**          | CPU cores (8C vs target chunk throughput)                                                   | §1           |
| **Stage 4.3 Lift draw distance**     | VRAM scaling vs chunk count                                                                 | §3           |
| **Stage 5.1 VCT**                    | 3D texture max size + memory heap                                                           | §3           |
| **Stage 5.2 RTX shadows**            | `VK_KHR_acceleration_structure` + `ray_query` + RT cores (RTX 3060 Ti = GA104, 38 RT cores) | §3, §4       |
| **Stage 6.1 Flecs ECS multi-thread** | 8C/16T + `amd-pstate-epp` governor                                                          | §1           |

---

## 9. Cross-refs

- `docs/experiments/AGENTS.md` §15 — где смотреть на этот файл (читать до `lscpu`/`vulkaninfo`/`nvidia-smi`).
- `agent/knowledge.md` §17 — multiplatform baseline (Arch Linux + clang-native + lld + libstdc++).
- `agent/knowledge.md` §4 — build / verification contract (ctest baseline, Tracy instrumentation rules).
- `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` — vendored Vulkan 1.4 SDK docs (read before rg/grep headers).
- `TODO.md` — stage cross-refs (each stage reads specific section above).
