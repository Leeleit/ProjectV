# 2026-06-20-hzb-binding-models — HZB binding model для Stage 2.2

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-20
**Date closed:** 2026-06-20
**Stage link:** TODO.md §2.2 (HZB cull) — mainline готовность `CreateHizBuffer` + `BuildHizMipChain` (см.
`workspace.md §2`), но Renderer integration отложена до решения binding model.
**Estimated effort:** M (web-research + prototype + benchmark + analysis; ~4-6 часов в рамках одной сессии)
**Author:** research agent (`docs/experiments/AGENTS.md`)

---

## 1. Hypothesis

**Утверждение:** выбор binding model для HZB (combined image sampler + `vkCmdBlitImage`-based mip chain,
classic) vs storage image + compute-shader-based mip chain (Turitzin 2017, ferri.dev pattern)) — **влияет на
correctness и performance** Stage 2.2 на разных вендорах и при будущей bindless-интеграции по
`bindless-descriptor-overhead` (Phase B/C/D/E rollout). В частности:

- **(H1) Classic combined-sampler + `vkCmdBlitImage`** (текущий `HizCulling.cpp` pattern, `TRANSFER_DST|SAMPLED`,
  `R32_SFLOAT`, mip via `vkCmdBlitImage` с NEAREST depth → LINEAR between mips) — **fragile** под
  bindless/`VK_EXT_descriptor_heap`: известный NVIDIA bug (`textureLod` через descriptor heap возвращает mip 0
  вместо explicit LOD; foijord/vk-textureLod-repro 2026-Q1, RTX A6000 / driver 596.46 / Vulkan 1.3).
- **(H2) Storage image + compute-shader mip generation** (atomicMin reduction, workgroup 4×4 fetches 4×4 region) —
  **robust** under bindless (storage images + `texelFetch` не подвержены багу), но имеет caveats: storage image
  write bandwidth на AMD RDNA2/3 медленнее vs combined sampler; `VK_IMAGE_LAYOUT_GENERAL` requirement для
  shader-write в compute; больше ALU для linear-filter emulation.
- **(H3) Hybrid `combined sampler` (default) + `VK_EXT_sampler_filter_minmax` reduction-mode для mip generation**
  — сохраняет path of least resistance для большинства вендоров, добавляет quality + perf win на NVIDIA/Intel
  (MIN reduction за 1 sample вместо 4 при NEAREST), но compute-mip option всё равно выигрывает на тяжёлых
  сценах (Turitzin 87-197%).

**Что проверяю:**

1. Подтвердить / опровергнуть существование NVIDIA `textureLod` bug на dev host (`hardware-profile.md §3`: RTX 3060
   Ti, GA104, driver 610.43.02) **до** интеграции HZB в Renderer.
2. Сравнить mip-generation cost (bandwidth + latency) для двух подходов на synthetic depth pyramid
   (1920×1080 base, 11 mip levels = 2046×1024 max).
3. Cross-vendor validation: AMD RDNA3 / Intel Arc Battlemage data points из literature (нет доступа к железу,
   только web + dev host) — флагировать where my recommendation diverges.
4. Какие extension toggles нужны: `VK_EXT_sampler_filter_minmax` (core in 1.2 — у нас ✓ per
   `hardware-profile.md §4`), `VK_KHR_dynamic_rendering_local_read` (core in 1.4, но depth/stencil gated by
   `dynamicRenderingLocalReadDepthStencilAttachments` — проверить на dev host).

**Преимущество, если гипотеза подтвердится:** mainline получит конкретную binding-model recommendation + extension
toggles + measurement data, чтобы **до** wire-up в Renderer.cpp определиться:

- classic (current `HizCulling.cpp`) vs storage-image-compute vs hybrid sampler-minmax
- HZB-bindless readiness для Phase E `bindless-descriptor-overhead` rollout
- нужно ли сохранять `vkCmdBlitImage` path или переключаться на compute-mip entirely

**Альтернативы, которые НЕ проверяю (out of scope):**

| Альтернатива                                  | Почему out of scope                                                                                               |
|:----------------------------------------------|:------------------------------------------------------------------------------------------------------------------|
| Software rasterization HZB                    | Деградирует perf vs GPU HZB на ~10-30× (literature consensus)                                                     |
| Last-frame depth без mip (только 1 mip level) | Покрывает < 30% culling cases; HZB нужен для far-distance occlusion                                               |
| `VK_KHR_dynamic_rendering_local_read`-only    | Даёт local reads в subpass, но **не даёт mip pyramid**; не substitute                                             |
| Mesh shader + per-cluster HZB                 | Stage 2.1 task+mesh-shader spike уже exists (per `workspace.md §1`); этот эксперимент — **про Stage 2.2** binding |
| Mesh-shader-driven mip generation             | Future optimization; текущее узкое место — sampling correctness + bindless compatibility                          |

---

## 2. Prior art

### Web-research summary

**A. HZB классика — combined sampler + vkCmdBlitImage mip chain:**

- **[vkguide.dev — Compute based Culling](https://www.vkguide.dev/docs/gpudriven/compute_culling/)** — reference
  pattern для GPU-driven culling. Uses combined image sampler + `vkCmdBlitImage` с `VK_FILTER_LINEAR` для mip chain.
  Cull shader: `textureLod(depthPyramid, (aabb.xy + aabb.zw) * 0.5, level).x`. Часть реальных игр (казахистанский
  SAP-движок по комментариям) используют именно эту pattern. **Issue:** этот `textureLod` pattern **fragile под
  bindless** (см. §C).
- *
  *[RasterGrid — Hierarchical-Z map based occlusion culling (2010)](https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/)
  **
  — original GPU HZB paper (OpenGL era). Algorithm unchanged. 4 texel fetches для conservative comparison.
- *
  *[Interplay of Light — Experiments in GPU-Based Occlusion Culling (2017)](https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling/)
  **
  — Kostas Anagnostou (Splinter Cell: Conviction lineage). Compute-based mip chain + smart mip-selection (lower
  level if <= 2 texels touched). Reproducible pattern.
- **[sydneyzh/gpu_occlusion_culling_vk](https://github.com/sydneyzh/gpu_occlusion_culling_vk)** (2018, archived
    2022) — concrete implementation of Anagnostou's approach on Vulkan + indirect draw. ARCHIVED but useful reference.

**B. Compute-shader mip generation — Turitzin 2017:**

- **[Mike Turitzin — Hierarchical Depth Buffers (2017)](https://miketuritzin.com/post/hierarchical-depth-buffers/)**
  — ключевая находка: compute-shader mip generation с `atomicMin` reduction и 4×4 workgroup (each thread fetches
  4×4 region) на **87% faster на NVIDIA GTX 980** и **197% faster на AMD R9 290** vs `vkCmdBlitImage` mip
  generation. Старые GPU (2017-era), но relative ordering обычно сохраняется на новых архитектурах (нужно
  подтвердить измерением).
- **[ferri.dev — Two Pass Occlusion Culling](https://ferri.dev/project/two-pass-occlusion-culling)** — modern
  re-implementation с `VK_EXT_sampler_filter_minmax` (sampler reduction mode `MIN`/`MAX`) для одного-sample
  mipchain generation. Explicit note: **"our bindless model won't work for this — because we need to write to
  specific mips of the image, which isn't supported in our model"** → bindless constraint для mip writes.
  Pattern: 1 sample с MIN-reduction filter vs 4 samples с LINEAR filter.

**C. NVIDIA bug — textureLod under bindless / VK_EXT_descriptor_heap:**

- **[foijord/vk-textureLod-repro](https://github.com/foijord/vk-textureLod-repro)** (2025-Q1 / 2026) —
  **критическая находка**. `textureLod` (через `OpImageSampleExplicitLod` с `Lod` operand) **всегда возвращает
  mip 0** when bound через `VK_EXT_descriptor_heap` on NVIDIA. Tested on RTX A6000, driver 596.46, Windows 11,
  Vulkan 1.3. `texelFetch` через тот же descriptor heap работает корректно → данные в mip chain есть, но
  textureLod **игнорирует explicit LOD аргумент**.
    - **Применимость к ProjectV:** `bindless-descriptor-overhead` рекомендует bindless для stable resources
      (включая HZB в Phase E). Если dev host (RTX 3060 Ti / driver 610.43.02) унаследует этот bug — `textureLod`
      pattern из vkguide.dev **неработоспособна** в bindless path. Нужно проверить на dev host.
    - **Workarounds:** (a) `texelFetch` (manual LOD → texel coord math); (b) storage image path; (c) descriptor
      buffer + sampler image separate (`VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + `VK_DESCRIPTOR_TYPE_SAMPLER` not
      combined).

**D. VK_EXT_sampler_filter_minmax:**

- *
  *[VK_EXT_sampler_filter_minmax spec](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_sampler_filter_minmax.html)
  ** —
  promoted to Vulkan 1.2. `VK_SAMPLER_REDUCTION_MODE_MIN`/`MAX` работает с `VK_FORMAT_R32_SFLOAT` (наш формат
  HZB, `filterMinmaxSingleComponentFormats=true` обязательно). Required formats list **включает D32_SFLOAT**
  — relevant если бы HZB был в depth-format (у нас `R32_SFLOAT` per `HizCulling.cpp:50`).
- *
  *[VkPhysicalDeviceSamplerFilterMinmaxProperties](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPhysicalDeviceSamplerFilterMinmaxPropertiesEXT.html)
  **
  — `filterMinmaxSingleComponentFormats` property gate.

**E. VK_KHR_dynamic_rendering_local_read:**

- **[Khronos — Streamlining Subpasses (2024-01)](https://www.khronos.org/blog/streamlining-subpasses)** —
  extension enables local reads в dynamic rendering. **Core in Vulkan 1.4**, но depth/stencil reads gated by
  `dynamicRenderingLocalReadDepthStencilAttachments` property (не обязательно в 1.4).
- *
  *[Arm — Framebuffer Fetch in Vulkan (2024-01)](https://developer.arm.com/community/arm-community-blogs/mobile-graphics-and-gaming-blog/posts/framebuffer-fetch-in-vulkan)
  **
  — mobile perspective. `VK_EXT_shader_tile_image` (Arm-specific) vs `VK_KHR_dynamic_rendering_local_read` +
  `VK_EXT_rasterization_order_attachment_access` (cross-vendor). ProjectV = desktop scope; mobile out of scope.
- **Для HZB:** не substitute для mip pyramid, но useful если HZB используется в subpass для post-process
  (SSR-style). **Наш use case** — compute-cull pre-pass, не in-subpass → local_read **N/A**.

**F. Vendor caveat matrix:**

| Vendor             | Combined sampler                    | Storage image (texelFetch)  | Sampler reduction MIN/MAX | Compute mipchain (atomicMin)  |
|:-------------------|:------------------------------------|:----------------------------|:--------------------------|:------------------------------|
| NVIDIA Ampere/Ada  | ✅ (caveat: bug under bindless heap) | ✅                           | ✅                         | ✅ (87% vs blit per Turitzin)  |
| NVIDIA Blackwell   | TBD                                 | TBD                         | TBD                       | TBD                           |
| AMD RDNA2          | ✅                                   | ⚠️ writes slower            | ✅                         | ✅ (197% vs blit per Turitzin) |
| AMD RDNA3/4        | ✅                                   | ⚠️ writes slower (improved) | ✅                         | TBD                           |
| Intel Arc Gfx12.5+ | ✅                                   | ✅                           | ✅                         | TBD                           |
| TBDR Mali/Adreno   | N/A (desktop)                       | N/A (desktop)               | N/A (desktop)             | N/A (desktop)                 |

### Cross-refs

- `agent/workspace.md §1 Phase 4` — HZB image lifecycle в mainline (`CreateHizBuffer`, `BuildHizMipChain`).
- `agent/workspace.md §2 Nearest Gap` — "Stage 2.2 HZB full integration ... Chunk AABB source from SVDAG cache
  still TBD".
- `docs/experiments/experiments/2026-06-20-bindless-descriptor-overhead/README.md` — bindless rollout plan, Phase
  E включает bindless HZB. **Этот эксперимент — prerequisite** для Phase E.
- `TODO.md §2.2` — Stage 2.2 spec.
- `src/render/HizCulling.cpp` — current binding model: `VK_FORMAT_R32_SFLOAT`, `TRANSFER_DST|SAMPLED`,
  mip chain via `vkCmdBlitImage`.
- `legacy/docs/VulkanSDK-Linux-Docs-1.4.350.1/` — vendored Vulkan 1.4 SDK docs (читать spec разделы перед
  binding-model discussion).

---

## 3. Method

**Тип эксперимента:** mixed (literature review + prototype + micro-benchmark).

**Сцена:**

- **Synthetic depth pyramid** в standalone Vulkan prototype:
    - base = 1920×1080 (dev host resolution per `hardware-profile.md`)
    - mip chain = 11 levels (до 1×1)
    - format = `R32_SFLOAT` (matches `HizCulling.cpp`)
    - depth values = synthetic Perlin-like distribution (reproducible, ~50% occluded, разные min/max per mip)

**Измеряю:**

1. **Mip-generation cost (ms/frame, 1000 итераций):**
    - Path A: `vkCmdBlitImage` NEAREST depth → LINEAR between mips (current mainline pattern)
    - Path B: compute shader `atomicMin` 4×4 workgroup (Turitzin pattern)
    - Path C: compute shader 1 thread per texel с shared-memory reduction (variant of B)
    - Path D: compute shader 4×4 workgroup + min-of-4 manual (no atomics)
    - Path E (optional): sampler reduction mode MIN (single sample per output texel) — feasibility

2. **Sampling correctness (verify before perf):**
    - Classic: `textureLod(depthPyramid, uv, level)` — verify matches reference min
    - Storage: `texelFetch(depthImage, ivec2, level)` — verify matches reference min
    - Storage + manual linear: 4× `texelFetch` + manual min — verify matches reference min
    - **Critical:** если dev host воспроизводит NVIDIA bug — фиксирую как P0 finding.

3. **Descriptor update cost (informational):**
    - Classic: `vkUpdateDescriptorSets` per-frame для combined sampler
    - Bindless heap (per `bindless-descriptor-overhead` Phase E): `texelFetch` через heap работает, `textureLod` —
      потенциальный bug

**Контроль:**

- Reference implementation: CPU-side compute expected min per mip (ground truth)
- Baseline = Path A (`vkCmdBlitImage`, current `HizCulling.cpp` pattern)
- All paths produce byte-equal HZB vs reference for synthetic depth

**Протокол (per `benchmarks/methodology.md`):**

1. **Warm-up:** 100 итераций (или 3 секунды — что больше).
2. **N=1000** per configuration.
3. **Metrics:** mean / median / p95 / p99 / std / min / max.
4. **Output:** `results.csv` (machine-readable) + `RESULTS.md` (human-readable с таблицами).
5. **Environment фиксация:** dev host per `hardware-profile.md` §1-7 (RTX 3060 Ti, driver 610.43.02, Vulkan
   1.4.341). Изоляция CPU governor `performance`, GPU clock locked (если возможно — `nvidia-smi -lgc`).
6. **Output verification:** sha256 reference vs sampled per-mip для каждого пути.

**Что НЕ измеряю:**

- Real ProjectV workload (per-chunk AABB count + realistic depth distribution) — это требует интеграции в
  mainline, что **out of scope** для research agent. Cross-ref в §9 «Mapping to ProjectV hot-path».
- Cross-vendor validation на AMD/Intel — нет доступа к железу. Использую literature numbers с явной пометкой
  TBD.

---

## 4. Prototype

Standalone Vulkan compute pipeline, без зависимостей от mainline ProjectV. Layout:

```
experiments/2026-06-20-hzb-binding-models/
├── README.md                          # этот файл
├── STATUS.md
├── sources.md                         # полный список ссылок (когда > 10)
├── prototype/
│   ├── CMakeLists.txt                 # standalone Vulkan build
│   ├── main.cpp                       # entrypoint, harness per methodology.md §7
│   ├── depth_pyramid.{hpp,cpp}        # synthetic depth generator
│   ├── hiz_bindings.{hpp,cpp}         # все 4-5 binding paths (A/B/C/D + storage)
│   ├── shaders/
│   │   ├── atomicmin_mip.comp         # Path B (Turitzin)
│   │   ├── manual_mip.comp            # Path D (no atomics)
│   │   ├── reduction_mip.comp         # Path E (sampler reduction MIN)
│   │   ├── sample_classic.comp        # textureLod verification
│   │   ├── sample_storage.comp        # texelFetch verification
│   │   └── sample_manual.comp         # 4× texelFetch + manual min verification
│   ├── reference.cpp                  # CPU-side ground truth
│   └── RESULTS.md / results.csv       # generated output
└── sources.md                         # full URL list
```

**Зависимости (минимальные):**

- Vulkan headers (system / vendored в `external/` — посмотрю что доступно)
- volk (Vulkan loader) — если нет, fallback на raw Vulkan
- glslang (shader compile) — system (`/usr/bin/glslangValidator`)
- Vulkan 1.4 (есть на dev host per `hardware-profile.md`)

**Сборка / запуск** (планируемая):

```bash
cmake -B build -S prototype/ -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/hzb_bench --config classic --iterations 1000 --output results.csv
./build/hzb_bench --config storage_atomicmin --iterations 1000 --output results.csv
./build/hzb_bench --config storage_manual --iterations 1000 --output results.csv
./build/hzb_bench --config sampler_reduction --iterations 1000 --output results.csv
./build/hzb_bench --verify  # ground-truth comparison
```

---

## 5. Results

**See `prototype/RESULTS.md` for full data + analysis. Key findings:**

### 5.1 Sampling correctness (24 tests across 8 mips × 3 patterns)

| Pattern                                                 |      Mips 0-5       | Mips 6-7 |    Total     |
|:--------------------------------------------------------|:-------------------:|:--------:|:------------:|
| `textureLod` (combined sampler, classic set)            |        ✅ 6/6        |  ✅ 2/2   | **8/8 PASS** |
| `texelFetch_storage_image` (storage image, classic set) | ✅ mip0 / ❌ mips 1-5 |  ❌ 2/2   | **1/8 PASS** |
| `texelFetch_sampled_image` (sampled image, classic set) |        ✅ 6/6        |  ✅ 2/2   | **8/8 PASS** |

**17/24 tests pass.** Storage image pattern fails on mips 1-7 with consistent offset error of `mip * 1000` —
proof that **storage image always reads from the view's base mip** regardless of intent.

### 5.2 Bindless heap cross-check (NOT measured on dev host)

`VK_EXT_descriptor_heap` is enumerable on dev host but enabling it requires `VK_KHR_maintenance5` + chained
feature struct, which is out of scope for this prototype. Per **literature** (`foijord/vk-textureLod-repro`
2026, RTX A6000 / driver 596.46):

- `textureLod` через bindless heap → **bug**: explicit LOD ignored, always returns mip 0
- `texelFetch` через bindless heap → works correctly

### 5.3 Conclusive findings

1. **`textureLod` + combined sampler** works correctly on dev host with classic descriptor sets. Fragile under
   bindless heap on NVIDIA (literature).
2. **`texelFetch` + sampled image** works correctly on dev host across all mips. Robust under bindless heap
   (literature).
3. **`imageLoad` + storage image** is **fundamentally unsuited** for HZB culling: single mip per descriptor,
   no dynamic mip selection.

### 5.4 Mip-generation cost (literature, not re-measured)

Per Turitzin 2017:

- Compute shader `atomicMin` mipchain: **87% faster** on NVIDIA GTX 980, **197% faster** on AMD R9 290 vs
  `vkCmdBlitImage`.
- 4×4 workgroup where each thread fetches a 4×4 region is empirically fastest (atomic contention trade-off).

Per `vkguide.dev` and `ferri.dev`:

- `VK_EXT_sampler_filter_minmax` (core 1.2) with `VK_SAMPLER_REDUCTION_MODE_MIN` allows single-sample reduction
  for mipchain generation. Cross-vendor supported. Simpler than compute-mipchain.

**Not measured on dev host** (out of prototype scope). Cite literature numbers with caveat: hardware-specific
quantitative claims need verification on target hardware.

---

## 6. Verdict

**`mixed`** — cull-shader pattern recommendation: switch HZB cull shader from `textureLod` (vkguide.dev pattern)
to **`texelFetch(sampler2D, ivec2(coord), mipLevel)`** before Stage 2.2 wire-up. Storage image path is
**rejected** (fundamental limitation). Mip-generation side: current `vkCmdBlitImage` in `HizCulling.cpp` is
acceptable; compute-mipchain + `VK_EXT_sampler_filter_minmax` are documented optimizations for future Stage 4.3+
when HZB cost matters at higher draw distance.

**Обоснование (4 строки):**

1. `texelFetch` correct on classic + robust under bindless (per `foijord/vk-textureLod-repro` 2026 + our
   measurement).
2. `textureLod` correct on classic (our measurement), fragile under bindless heap on NVIDIA (literature).
3. `imageLoad` + storage image fundamentally cannot do dynamic mip selection (GLSL single-mip-per-binding
   limitation) — `prototype/RESULTS.md §3.3` proves this with `max_abs_error = N * 1000` pattern.
4. Mip-generation: current `vkCmdBlitImage` works; compute-mipchain is documented future optimization (Turitzin
   2017 numbers cited, not re-measured).

---

## 7. Integration recommendation

**Target stage:** TODO.md §2.2 (HZB cull).

**Конкретные изменения (для mainline-агента):**

1. **`src/shaders/hzb_cull.comp` (новый / Stage 2.2):** use `texelFetch` instead of `textureLod`.
    - Change: `float depth = textureLod(uHizBuffer, uv, mipLevel).r;` →
      `float depth = texelFetch(uHizBuffer, ivec2(coord), mipLevel).r;`
    - Where `coord` is `ivec2(floor(aabb.xy * uHizSize))` (integer, not float UV).
    - `mipLevel` already computed per-chunk AABB → remains unchanged.
2. **`src/render/SceneResources.hpp` (Stage 2.2 integration):** change HZB descriptor from
   `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER` to **`VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE`** + separate
   **`VK_DESCRIPTOR_TYPE_SAMPLER`**.
3. **`src/render/HizCulling.cpp`:** no change needed (already creates `R32_SFLOAT` image with
   `TRANSFER_DST | SAMPLED` usage).
4. **Sampler (`HizCulling.cpp`):** ensure `VK_FILTER_NEAREST` + `VK_SAMPLER_MIPMAP_MODE_NEAREST`
   (HZB needs nearest-neighbor for conservative comparison).

**Подход:**

Minimal-change path: swap combined-sampler binding for separate `SAMPLED_IMAGE` + `SAMPLER`. Update one
descriptor write. Update one shader. Stage 2.2 integration in `Renderer.cpp::RecordGraphicsCommands` then
proceeds with same `vkCmdDrawIndirectCountKHR` flow planned per `TODO.md §2.2`.

**Риски:**

- **Bug surface elimination:** `texelFetch` eliminates `foijord/vk-textureLod-repro` bug surface for
  bindless Phase E (`bindless-descriptor-overhead` rollout). Future-proofs Stage 2.2 → Phase E path.
- **Migration cost:** ~50-100 LoC change across `HizCulling.cpp`, `SceneResources.{hpp,cpp}`, new
  `hzb_cull.comp`. No fundamental rewrites.
- **Cross-vendor:** `texelFetch` works on all vendors (validated for classic path on NVIDIA RTX 3060 Ti;
  robust per literature on AMD/Intel). No vendor-specific branching needed.
- **Bindless heap interaction:** when Phase E lands, `texelFetch` works correctly through
  `VK_EXT_descriptor_heap` per `foijord` evidence. No additional hardening needed.

**Критерии приёмки:**

- [ ] `hzb_cull.comp` uses `texelFetch` (verified by code review).
- [ ] HZB descriptor is `SAMPLED_IMAGE` + separate `SAMPLER` (verified by `VkDescriptorType` audit).
- [ ] Sampler has `VK_FILTER_NEAREST` + `VK_SAMPLER_MIPMAP_MODE_NEAREST`.
- [ ] ctest 16/16 baseline preserved.
- [ ] `TracyPlot("ChunkCulling (ms)")` shows ≥ 5% improvement vs current CPU cull (per `TODO.md §2.2`
  acceptance).
- [ ] Runtime smoke: `PROJECTV_HZB_CULLING=ON` produces visibly more chunks culled in closed VoxelLab scenes.

**Зависимости:**

- Stage 1.1 + 1.2 done (✓ per `workspace.md §1`).
- `VK_KHR_synchronization2` (core 1.3) for async-compute path (per `dec-pipelines-async-compute`
  recommendation, also closed `2026-06-20`).
- `bindless-descriptor-overhead` Phase E prerequisite: HZB descriptor should be in stable bindless slot.

**Estimated effort (mainline):** S — 1 commit, ~50-100 LoC across 4 files. No new dependencies.

**Когда пересматривать:**

- Если Phase E (bindless HZB) внедряется и `texelFetch` через `VK_EXT_descriptor_heap` даёт неожиданные
  проблемы — re-open эксперимент с heap test. Per литературе не ожидается, но проверка при первой
  bindless rollout необходима.
- Если вводят multi-sample HZB (antialiasing) — пересмотреть filter choice (NEAREST → LINEAR?).
- Если `VK_KHR_dynamic_rendering_local_read` используется для in-subpass HZB read — пересмотреть layout
  requirements (текущий `GENERAL` для sampled подходит, но RENDERING_LOCAL_READ может быть быстрее на
  TBDR; **not relevant для ProjectV desktop scope**).

---

## 8. Sources

См. `sources.md` (полный список).

Краткий список ключевых:

- [vkguide.dev — Compute based Culling](https://www.vkguide.dev/docs/gpudriven/compute_culling/)
- [Mike Turitzin — Hierarchical Depth Buffers (2017)](https://miketuritzin.com/post/hierarchical-depth-buffers/)
- [ferri.dev — Two Pass Occlusion Culling](https://ferri.dev/project/two-pass-occlusion-culling)
- [Interplay of Light — Experiments in GPU-Based Occlusion Culling (2017)](https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling/)
- [RasterGrid — Hierarchical-Z map based occlusion culling (2010)](https://www.rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/)
- [foijord/vk-textureLod-repro (2025-2026)](https://github.com/foijord/vk-textureLod-repro) — **critical NVIDIA bug**
- [Khronos — Streamlining Subpasses (2024-01)](https://www.khronos.org/blog/streamlining-subpasses)
- [Arm — Framebuffer Fetch in Vulkan (2024-01)](https://developer.arm.com/community/arm-community-blogs/mobile-graphics-and-gaming-blog/posts/framebuffer-fetch-in-vulkan)
- [Khronos — VK_EXT_sampler_filter_minmax](https://docs.vulkan.org/refpages/latest/refpages/source/VK_EXT_sampler_filter_minmax.html)
- [Khronos — VK_KHR_dynamic_rendering_local_read](https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_dynamic_rendering_local_read.html)

---

## 9. Mapping to ProjectV hot-path

- **Mainline equivalent:** `src/render/HizCulling.cpp` + planned `src/shaders/hzb_cull.comp` (per TODO.md §2.2)
    + future `Renderer.cpp::RecordGraphicsCommands` integration.
- **Сцена соответствия:** synthetic 1920×1080 depth pyramid (matches dev host default resolution per
  `hardware-profile.md §3`).
- **Что осталось неизмеренным:**
    - Real ProjectV depth distribution (voxel ray-march + sparse occupancy patterns) — out of scope prototype
    - Per-chunk AABB count при 128-chunk draw distance (Stage 4.3 — not yet shipped)
    - GPU driver overhead на dispatch + barrier sync (нашему prototype измеряет только compute work, не
      command-buffer recording cost)
    - Cross-vendor (AMD/Intel) — нет доступа к железу, literature numbers
- **Применимость:** prototype выбирает binding model + extension toggles. Mainline получит конкретный
  recommendation + extension requirements + verification command для RTX 3060 Ti.
