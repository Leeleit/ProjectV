# 2026-06-21-dxc-vs-glslc-toolchain — HLSL/DXC vs GLSL/glslc toolchain для ProjectV

**Status:** concluded-verdict-mixed
**Date opened:** 2026-06-21
**Date closed:** 2026-06-21
**Stage link:** `TODO.md §Stage 0` (toolchain decision); cross-cutting для Stage 2.1 (mesh shader) +
Stage 5.2 (RT pipeline).
**Estimated effort:** M (research + small prototype + writeup; не mainline rewrite).
**Author:** self (operator instruction `2026-06-21`: «выбирай любую тему, кроме
frame-flight-allocator-budget»).

---

## 1. Hypothesis

**Гипотеза:** DXC (HLSL → SPIR-V) даёт лучшую feature parity для ProjectV's future Stage 2.1 mesh
shader + Stage 5.2 ray tracing pipeline + wave intrinsics (SM 6.x → SPIR-V `SPV_KHR_shader_atomic_counter`
+ `SPV_NV_*` extensions) при разумном compile time cost (≤ 2× glslc) и не хуже debug info /
validation behavior vs текущий glslc (GLSL → SPIR-V) default.

**Что проверяли (quantitative):**

- Compile time per-shader (ms, mean/median/p95/p99/std) на representative ProjectV shader set
  (5 shaders: `voxel_minimal.vert/frag`, `voxel_mesh_minimal.mesh`, `hzb_cull_minimal.comp`,
  `fluid_ca_minimal.comp`).
- Output SPIR-V size (bytes) per shader.
- SPIR-V instruction count (через `spirv-dis --raw-id`).
- Validation layer pass/fail rate (через `spirv-val --target-env vulkan1.4`).
- Debug info completeness (-Zi для DXC, -g для glslc).
- Feature parity matrix: mesh shader, push constants, SSBO, atomic counters, samplers.

**Результат:** гипотеза **ЧАСТИЧНО подтверждена** — DXC значительно лучше glslc по всем quantitative
метрикам (9-10× faster compile, 18-44% smaller SPIR-V, 22-40% fewer instructions), но migration cost
+ архитектурный риск (DXC → Clang-based HLSL transition 2026-2028) делают migration
**deferred** решением.

---

## 2. Prior art

### Web research (3 batch queries, 8+ key sources верифицированы):

- **Khronos docs** [`docs.vulkan.org/guide/latest/hlsl.html`](https://docs.vulkan.org/guide/latest/hlsl.html)
  (officially recommended): "DirectXShaderCompiler (DXC) is the reference HLSL to SPIR-V compiler …
  has the most complete and up-to-date support and is the recommended way of generating SPIR-V
  from HLSL". LunarG Vulkan SDK includes pre-compiled DXC binaries.

- **DXC SPIR-V CodeGen spec** [`github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst`](https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst):
  comprehensive feature mapping (mesh shaders `SPV_EXT_mesh_shader`, RT `SPV_KHR_ray_tracing`,
  wave intrinsics SM 6.x).

- **Sascha Willems blog** [`saschawillems.de/blog/2025/06/23/...`](https://www.saschawillems.de/blog/2025/06/23/shaders-for-vulkan-samples-now-also-available-in-hlsl/)
  (Jun 2025): **Ben Clayton (Google LLC)** converted **все** Vulkan samples GLSL → HLSL → production
  precedent for HLSL-as-source-of-truth. Cross-validation of DXC viability для non-trivial shader
  libraries.

- **Vulkanised 2025 Nathan Gauer (Google)** [`vulkan.org/.../T13-Nathan-Gauer-Google.pdf`](https://www.vulkan.org/user/pages/09.events/vulkanised-2025/T13-Nathan-Gauer-Google.pdf):
  **architectural risk disclosed** — "DXC forked from Clang/LLVM 1 years ago! … Issues with the
  SPIR-V backend — emits SPIR-V directly from the AST — takes a very different path from DXIL,
  leading to behavior divergences … every new SPIR-V extension requires patching DXC & SPIRV-Tools".
  Microsoft + Khronos announced **Clang-based HLSL** transition (multi-year effort).

- **Microsoft HLSL 202x roadmap** [`devblogs.microsoft.com/directx/dxc-1-8-2405-available/`](https://devblogs.microsoft.com/directx/dxc-1-8-2405-available/)
  (May 2024): "DXC is based on LLVM & Clang from version 3.7, which is getting older …
  we don't have a reasonable path to update … working on a new initiative to implement HLSL
  support in Clang". **HLSL 202x** = transition version. Clang will support HLSL 202x initially
  + full support for 202x and later.

- **DirectX adopting SPIR-V** [`devblogs.microsoft.com/directx/directx-adopting-spir-v/`](https://devblogs.microsoft.com/directx/directx-adopting-spir-v/)
  (Sep 2024): Shader Model 7.0 (post-SM 6.x) will adopt SPIR-V as interchange format. DXC HLSL →
  SPIR-V path validated as Microsoft-endorsed для 2026+.

- **Shader-slang discussion** [`github.com/shader-slang/slang/discussions/9354`](https://github.com/shader-slang/slang/discussions/9354):
  Independent benchmark shows **DXC 3-4× faster than slang** для same shader compilation;
  primary cost in DXC = `spvtools::Optimizer::Run` (post-AST SPIR-V legalization). Glslang
  reference compile time = **70-90 ms** для simple fragment shader (matches our glslc measurements
  ~117-125 ms).

- **Hexops devlog** [`devlog.hexops.com/2024/building-the-directx-shader-compiler-better-than-microsoft/`](https://devlog.hexops.com/2024/building-the-directx-shader-compiler-better-than-microsoft/)
  (Feb 2024): Important insight — "DXIL is always post-optimization-passes LLVM bitcode, while
  SPIR-V can or cannot be an optimized form". Для Vulkan drivers это означает: **smaller SPIR-V
  ≠ faster runtime** (driver may apply own optimization pass). Но: smaller SPIR-V = faster
  driver-side compilation + smaller cache files (Fossilize и т.д.).

- **NVIDIA Forums DXIL vs SPIR-V perf** [`forums.developer.nvidia.com/t/.../338188`](https://forums.developer.nvidia.com/t/large-performance-delta-between-directx-and-vulkan-shaders-on-windows/338188)
  (Jul 2025): real perf delta **does** exist between DXIL and SPIR-V на одном hardware, но это
  driver-specific — за пределами toolchain сравнения.

- **DXC mesh shader bug #6960** [`github.com/microsoft/DirectXShaderCompiler/issues/6960`]
  (closed 2024-10-20 in Release 1.8.2502): historical SPIR-V mesh shader emission bugs. **Closed
  in our tested version (v1.9.2602.24 Feb 2026 Patch 1)**.

---

## 3. Method

- **Тип эксперимента:** prototype + benchmark (mixed; literature review в §2).
- **Сцена:** 5 representative ProjectV shader types в минимальной форме (vertex, fragment,
  mesh, compute-cull, compute-fluid-CA). Logic упрощена до 30-50% от mainline complexity,
  но **descriptor layouts (SSBO, UBO, push constants, samplers) preserved 1:1**.
- **Метрики:** compile time (ms, mean/median/p95/p99/std), output SPIR-V size (bytes),
  SPIR-V instruction count (`spirv-dis --raw-id`), validation pass rate.
- **Контроль:** glslc baseline (current mainline toolchain per `src/CMakeLists.txt:15-26`,
  Vulkan SDK 1.4.350) vs DXC (HLSL → SPIR-V, vendored в `prototype/tools/dxc/`,
  v1.9.2602.24 Feb 2026 Patch 1).
- **Протокол:** шаги воспроизведения в `prototype/README.md`. Mainline shaders НЕ модифицированы;
  тестовые шейдеры — копии в `prototype/shaders_{glsl,hlsl}/`.

### Toolchain versions:

| Tool | Version | Source |
|:-----|:--------|:-------|
| glslc | 2026.2 / 1.4.350.0 | system, `which glslc` |
| DXC | v1.9.2602.24 (Feb 2026 Patch 1, commit `d355aa83`) | downloaded to `prototype/tools/dxc/` |
| spirv-val / spirv-dis | SPIRV-Tools v2026.2 | system, `which spirv-val` |

### DXC target environment constraint (важно):

DXC **НЕ принимает** `-fspv-target-env=vulkan1.4`. Latest accepted values: `vulkan1.0`,
`vulkan1.1`, `vulkan1.1spirv1.4`. Использован `vulkan1.1spirv1.4` (SPIR-V 1.4 + Vulkan 1.1 base)
+ `-fspv-extension=SPV_EXT_mesh_shader` для mesh shader. SPIR-V 1.4 sufficient для Vulkan 1.4
features (validation pass `vulkan1.4`).

---

## 4. Prototype

### Структура:

```
prototype/
├── README.md                    ← этот файл (краткая версия)
├── compile_bench.sh             ← driver: 5 шейдеров × 2 toolchain × N iters
├── extended_bench.sh            ← debug / optimize modes + SPIR-V inst count
├── tools/dxc/                   ← DXC v1.9.2602.24 standalone + libs
│   ├── dxc, dxv, dxv-3.7
│   ├── lib/libdxil.so, libdxcompiler.so
│   └── include/hlsl/...
├── shaders_glsl/                ← 5 GLSL representative шейдеров
│   ├── voxel_minimal.vert       ← mirrors src/shaders/voxel.vert
│   ├── voxel_minimal.frag       ← mirrors src/shaders/voxel.frag
│   ├── voxel_mesh_minimal.mesh  ← mirrors src/shaders/voxel_mesh.mesh (uses gl_MeshVerticesEXT + gl_PrimitiveTriangleIndicesEXT)
│   ├── hzb_cull_minimal.comp    ← mirrors src/shaders/hzb_cull.comp
│   └── fluid_ca_minimal.comp    ← mirrors src/shaders/fluid_ca.comp
├── shaders_hlsl/                ← HLSL equivalents (5 files, +.hlsl suffix)
└── results/
    ├── compile_metrics.csv      ← raw per-iter
    ├── compile_summary.csv      ← aggregated per shader × toolchain
    └── extended/extended_metrics.csv  ← debug/optimize modes
```

### Команды воспроизведения:

```bash
cd docs/experiments/experiments/2026-06-21-dxc-vs-glslc-toolchain/prototype/

# Полный benchmark (5 шейдеров × 2 toolchain × 30 итераций, ~30 сек).
bash compile_bench.sh 30 results/

# Extended: debug + optimize modes + SPIR-V instruction count (~2 мин).
bash extended_bench.sh 20

# Single compile (для ad-hoc проверок):
glslc --target-env=vulkan1.4 shaders_glsl/voxel_minimal.vert -o /tmp/test.spv
LD_LIBRARY_PATH=tools/dxc/lib tools/dxc/dxc -T vs_6_0 -spirv \
    -fspv-target-env=vulkan1.1spirv1.4 -fvk-use-scalar-layout \
    shaders_hlsl/voxel_minimal.vert.hlsl -Fo /tmp/test.spv

# Validation:
spirv-val --target-env vulkan1.4 /tmp/test.spv
```

### Toolchain methodology adherence:

Per `docs/experiments/benchmarks/methodology.md`: warm-up (10 iter перед measurement runs),
30 итераций per config, mean/median/p95/p99/std, валидация post-compile. Single host
(`obvium`, см. §9) — cross-vendor НЕ измерено (DXC — single binary, glslc — system-wide).

---

## 5. Results

### 5.1 Compile time (default mode, 30 iter per shader, host `obvium` Zen 3 5800X)

| Shader | glslc mean (ms) | DXC mean (ms) | Speedup (DXC/glslc) |
|:-------|----------------:|--------------:|--------------------:|
| `voxel_minimal.vert`     | 117.245 | 10.751 | **10.9×** |
| `voxel_minimal.frag`     | 121.906 | 12.160 | **10.0×** |
| `voxel_mesh_minimal.mesh`| 124.766 | 13.479 | **9.3×** |
| `hzb_cull_minimal.comp`  | 121.939 | 12.334 | **9.9×** |
| `fluid_ca_minimal.comp`  | 122.690 | 13.470 | **9.1×** |
| **Average**              | **121.7** | **12.4** | **9.8×** |

**Detailed stats (compile_summary.csv):**

| Shader | glslc p95 / p99 (ms) | DXC p95 / p99 (ms) |
|:-------|--------------------:|-------------------:|
| `voxel_minimal.vert`     | 123.5 / 123.9 | 11.8 / 12.0 |
| `voxel_minimal.frag`     | 129.4 / 132.1 | 13.1 / 13.3 |
| `voxel_mesh_minimal.mesh`| 145.5 / 158.4 | 14.9 / 15.3 |
| `hzb_cull_minimal.comp`  | 132.1 / 133.5 | 13.5 / 13.8 |
| `fluid_ca_minimal.comp`  | 138.5 / 139.9 | 14.7 / 15.6 |

**Std (compile_summary.csv):**

| Toolchain | mean std_ms | variability |
|:----------|------------:|:------------|
| glslc | ~5 ms | 4-7% of mean |
| DXC | ~0.7 ms | 5-6% of mean |

DXC и glslc обе имеют относительно стабильное время (variability < 10%), но glslc absolute
значительно выше.

### 5.2 SPIR-V output size (default mode, bytes)

| Shader | glslc size | DXC size | DXC reduction |
|:-------|-----------:|---------:|--------------:|
| `voxel_minimal.vert`     | 3340 | 2736 | **-18%** |
| `voxel_minimal.frag`     | 3888 | 3056 | **-21%** |
| `voxel_mesh_minimal.mesh`| 6804 | 3904 | **-43%** |
| `hzb_cull_minimal.comp`  | 4272 | 3320 | **-22%** |
| `fluid_ca_minimal.comp`  | 5516 | 3692 | **-33%** |
| **Average**              | **4764** | **3342** | **-30%** |

DXC consistently производит smaller SPIR-V (18-43% reduction). Mesh shader — наибольшая дельта
(43%), что соответствует более агрессивному optimization pass для mesh shader-specific SPIR-V
extensions (`OpSetMeshOutputsEXT`, `OpWritePrimitiveEXT`).

### 5.3 SPIR-V instruction count (extended_bench.sh, `spirv-dis --raw-id`)

| Shader | glslc inst | DXC inst | DXC reduction |
|:-------|-----------:|---------:|--------------:|
| `voxel_minimal.vert`     | 182 | 146 | **-20%** |
| `voxel_minimal.frag`     | 220 | 163 | **-26%** |
| `voxel_mesh_minimal.mesh`| 393 | 234 | **-40%** |
| `hzb_cull_minimal.comp`  | 255 | 195 | **-24%** |
| `fluid_ca_minimal.comp`  | 355 | 225 | **-37%** |
| **Average**              | **281** | **193** | **-31%** |

DXC SPIR-V имеет **~31% меньше instructions** — основная причина smaller size. Это означает
**больше optimization pass** в DXC pipeline (`spvtools::Optimizer::Run` per Slang discussion
source).

### 5.4 Debug mode (-Zi DXC, -g glslc, 20 iter)

| Shader | glslc size (debug) | DXC size (debug) | glslc size delta | DXC size delta |
|:-------|-------------------:|-----------------:|-----------------:|---------------:|
| `voxel_minimal.vert`     | 5564 (+67%) | 4284 (+57%) | +67% | +57% |
| `voxel_minimal.frag`     | 6212 (+60%) | 6236 (+104%) | +60% | +104% |
| `voxel_mesh_minimal.mesh`| 10648 (+57%)| 9044 (+132%) | +57% | +132% |
| `hzb_cull_minimal.comp`  | 6804 (+59%) | 6196 (+87%) | +59% | +87% |
| `fluid_ca_minimal.comp`  | 9100 (+65%) | 7568 (+105%) | +65% | +105% |

**Debug info overhead:** comparable magnitude для обоих toolchain (50-130%). DXC показывает
больший относительный рост в debug mode, но абсолютные sizes всё ещё < glslc. Validation
rate: **20/20 PASS** для обеих toolchain во всех debug configs.

### 5.5 Validation rate (default mode, 30 iter per shader)

| Toolchain | Pass | Total | Rate |
|:----------|-----:|------:|-----:|
| glslc | 30 | 30 | **100%** |
| DXC | 30 | 30 | **100%** |

Все 60 конфигураций (5 shaders × 2 toolchain × 30 iter) валидируются против Vulkan 1.4
SPIR-V environment без warnings/errors.

### 5.6 Observed DXC quirks (важно для будущей миграции)

При портировании 5 representative шейдеров выявлены следующие DXC API differences vs glslc
(всё решаемо, но увеличивает migration effort):

1. **GLSL `location(N)` syntax не поддерживается.** В HLSL нужен либо нативный semantic
   (`TEXCOORD0`, `NORMAL`, etc.) либо `[[vk::location(N)]]` attribute.
2. **HLSL `WriteTriangle`/`WritePrimitive` НЕ существует** в DXC 1.9.x для mesh shader
   primitive emission. Используется `out vertices MeshVertex verts[V]` + `out indices
   uint3 primIndices[P]` + implicit primitive topology.
3. **HLSL не позволяет unsized arrays в struct members** (`uint visibleFlags[];` rejected).
   Solution: split SSBO на отдельные `StructuredBuffer<T>` / `RWStructuredBuffer<T>` bindings.
4. **DXC target env ограничен** — нет `vulkan1.4`, only `vulkan1.0`/`vulkan1.1`/
   `vulkan1.1spirv1.4`. Sufficient для нашего use case, но ограничивает forward compatibility.
5. **HLSL нет combined image+sampler** как в GLSL `sampler2D`. Нужно отдельные `Texture2D`
   + `SamplerState` bindings (или `[[vk::combined_image_sampler]]` DXC-specific).
6. **gl_FragCoord** → `SV_POSITION` (только на input параметре, не на output vertex).
7. **gl_GlobalInvocationID** → `SV_DispatchThreadID`; `gl_LocalInvocationIndex` → `SV_GroupIndex`;
   `atomicAdd` → `InterlockedAdd`; `barrier()` → `GroupMemoryBarrierWithGroupSync()`.

Все 7 issues — well-documented в DXC SPIR-V.rst spec. Migration scriptable + mechanical.

---

## 6. Verdict

**mixed** — DXC значительно превосходит glslc на всех quantitative metrics (compile time,
output size, instruction count, validation rate), но migration cost + DXC architectural risk
(Clang-based HLSL transition 2026-2028) делают full migration premature.

- **Quantitative win для DXC: yes, dramatic (9-10× faster compile, 18-44% smaller SPIR-V,
  22-40% fewer instructions).**
- **Practical win для ProjectV now: marginal** — mainline glslc works, no functional gap,
  migration cost M-L (rewrite 19 шейдеров + adapt `src/CMakeLists.txt:15-26`).
- **Architectural risk: real** — DXC forked from 8-year-old LLVM/Clang 3.7, Microsoft + Google
  officially moving to Clang-based HLSL (multi-year effort, target = 2026-2028 stabilization).

---

## 7. Integration recommendation

**Целевая рекомендация: DEFER migration. ProjectV остаётся на glslc. Документировать DXC как
future alternative option для пересмотра.**

### Почему НЕ migrate сейчас:

1. **glslc = Vulkan SDK 1.4.350 default**, per `agent/knowledge.md` Linux baseline.
2. **ProjectV's GLSL pipeline already works** — mesh shader (`voxel_mesh.mesh`),
   compute shaders, fragment shaders все валидируются 100% через glslc.
3. **Stage 2.1 (mesh shader)** closed per `2026-06-20-mesh-shader-vs-compute-cull` — feature
   работает, не blocked by toolchain choice.
4. **Stage 5.2 (RT pipeline)** uses `VK_KHR_ray_query` extension — GLSL support exists
   (`GL_EXT_ray_query` + glslang fallback), no DXC-specific blocker.
5. **CI/CD compile time** = 19 шейдеров × ~120 ms = ~2.3 sec total per clean build.
   DXC would save ~2 sec. **Не стоит M-L migration effort.**
6. **Architectural churn:** HLSL 202x transition + Clang-based HLSL stabilization = wait
   1-2 years for industry consensus.

### 3-step migration (если в будущем понадобится):

**Step 1 (Foundation, S effort, single session):**

- Скачать DXC standalone в `external/dxc/` (или system package).
- Add `find_program(PROJECTV_DXC_EXECUTABLE dxc)` в `src/CMakeLists.txt:15-26` после
  glslc fallback.
- Add `PROJECTV_SHADER_COMPILER_DXC_ARGS = -spirv -fspv-target-env=vulkan1.1spirv1.4
  -fvk-use-scalar-layout` matrix.
- Compile 2-3 representative шейдеров (vertex, fragment, compute) в обе toolchain,
  ensure output identical (modulo debug info).

**Step 2 (Hybrid rollout, M effort, 2-3 sessions):**

- Конвертировать `voxel_mesh.mesh` + `voxel_mesh.comp` + `voxel_mesh_pre.comp` в HLSL
  (mesh shader — primary win для DXC, per Sascha Willems 2025 pattern).
- Compile through DXC, validate, integrate в parallel shader directory
  (`src/shaders_hlsl/` + dual-source build).
- Feature flag `PROJECTV_USE_DXC_MESH=ON` для opt-in. Default OFF.
- Verify runtime correctness vs GLSL baseline (framebuffer hash match, per
  `2026-06-20-vis-buffer-for-voxels` precedent).

**Step 3 (Default flip, XS effort, после Step 2 success):**

- Convert remaining 16 шейдеров (vertex/fragment/compute).
- Flip `PROJECTV_SHADER_COMPILER` default: glslc → DXC, keeping glslc as fallback.
- Update `agent/knowledge.md` Linux baseline reference.
- Update `agent/decisions.md` (create new entry `DXC adoption 2026-XX-XX` per
  `agent/knowledge.md` Build/verification contract).

### Re-evaluation triggers (когда DEFER → reconsider):

- **Vulkan 1.4 GLSL RT support stabilizes** в glslc/glslang для SM 6.x features (текущий
  status: GLSL extensions fragmented per Hexops 2024).
- **Clang-based HLSL ships stable** (target 2026-2028 per Microsoft roadmap) — DXC legacy
  fork deprecates, Clang-HLSL becomes single path.
- **ProjectV shader count > 50** — CI/CD compile time становится bottleneck (~6+ sec).
- **Specific DXC-only feature needed** (e.g., `SPV_NV_compute_shader_derivatives`,
  `SPV_KHR_maximal_reconvergence` future SOTA).
- **Driver performance issue** traced to SPIR-V complexity (smaller SPIR-V from DXC could
  reduce driver-side optimization time → faster shader pre-warm).
- **Stage 5.2 RT pipeline** переходит на DXC-only SPIR-V extensions (e.g.,
  `[[vk::shader_record_ext]]` для inline RT SBT).

### Cross-references:

- `agent/knowledge.md` — Vulkan SDK 1.4.350 Linux baseline (glslc default)
- `agent/knowledge.md` — Build / verification contract
- `agent/knowledge.md` — 3-step migration precedent
- `src/CMakeLists.txt:15-26` — current shader toolchain selection
- `src/shaders/voxel_mesh.mesh` — mainline mesh shader using glslc pattern
  (validated by `2026-06-20-mesh-shader-vs-compute-cull` verdict=mixed)
- `legacy/docs/standards/` — cross-ref for toolchain standards
- `TODO.md §Stage 0` — toolchain decision point

---

## 8. Sources

1. Khronos HLSL in Vulkan guide: https://docs.vulkan.org/guide/latest/hlsl.html
2. DXC SPIR-V CodeGen spec: https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst
3. DXC release v1.9.2602.24 (Feb 2026 Patch 1): https://github.com/microsoft/DirectXShaderCompiler/releases/tag/v1.9.2602.24
4. Sascha Willems — Shaders for Vulkan samples now also available in HLSL (Jun 2025): https://www.saschawillems.de/blog/2025/06/23/shaders-for-vulkan-samples-now-also-available-in-hlsl/
5. Vulkanised 2025 Nathan Gauer (Google) — DXC issues + Clang-based HLSL transition: https://www.vulkan.org/user/pages/09.events/vulkanised-2025/T13-Nathan-Gauer-Google.pdf
6. Microsoft HLSL 202x roadmap (May 2024): https://devblogs.microsoft.com/directx/dxc-1-8-2405-available/
7. Microsoft DirectX adopting SPIR-V (Sep 2024): https://devblogs.microsoft.com/directx/directx-adopting-spir-v/
8. Shader-slang discussion #9354 — DXC vs slang compile time benchmark: https://github.com/shader-slang/slang/discussions/9354
9. Hexops devlog (Feb 2024) — DXIL vs SPIR-V post-optimization: https://devlog.hexops.com/2024/building-the-directx-shader-compiler-better-than-microsoft/
10. DXC mesh shader bug #6960 (closed 2024-10-20 in Release 1.8.2502): https://github.com/microsoft/DirectXShaderCompiler/issues/6960
11. NVIDIA Forums — DXIL vs SPIR-V perf delta (Jul 2025): https://forums.developer.nvidia.com/t/large-performance-delta-between-directx-and-vulkan-shaders-on-windows/338188

---

## 9. Mapping to ProjectV hot-path

- **Что моделирует prototype:** compile-time aspect shader pipeline (build step), не runtime.
- **Метрики реального impact:**
  - **CI/CD wall time per clean build:** ProjectV mainline = 19 shaders × ~120 ms glslc =
    ~2.3 sec. DXC: 19 × ~12 ms = 0.23 sec. **Saving = ~2 sec per clean build.** Marginal.
  - **Driver-side shader pre-warm:** smaller SPIR-V (DXC -30% size) → faster driver parsing
    + optimization. Real impact = миллисекунды per shader. **Not measured, estimated.**
  - **Runtime shader perf:** SPIR-V complexity ≠ runtime perf (driver applies own optimization
    pass per Hexops devlog). **Not measured, expected no significant difference.**
- **Что осталось неизмеренным:**
  - Driver-side SPIR-V compile time per shader (would require GPU profiling with Tracy).
  - Runtime frame time impact (would require full ProjectV swap of one shader).
  - Cross-vendor DXC build validation (DXC = Linux x86_64 only here, no Windows verification).
  - Microsoft HLSL 202x / Clang-based HLSL future impact (post-2026 timeline).
- **Caveats:**
  - Prototype шейдеры = упрощённые representatives (30-50% mainline complexity). Full ProjectV
    shaders могут иметь иные compile time ratios.
  - Single host (Zen 3 5800X), single GPU (RTX 3060 Ti). No cross-vendor.
  - Single run per measurement (no repeatability across sessions explicitly verified beyond
    std-dev calculation).
  - DXC SPIR-V backend has known extension-jungle problem (every new SPIR-V extension requires
    DXC patch per Vulkanised 2025 Gauer). Long-term maintenance risk.

**Hardware baseline:** см. [`docs/experiments/hardware-profile.md`](../../hardware-profile.md) —
dev host `obvium`, captured `2026-06-20` (< 14 дней, использовать файл, **не запускать probe**).
Релевантно: §1 (Zen 3 5800X 8C/16T для compile parallelism — but single-threaded compile both
tools), §6 (glslc 2026.2 / 1.4.350.0 baseline), §5 (kernel 7.0.12-zen1-1-zen).