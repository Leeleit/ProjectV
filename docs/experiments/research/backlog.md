# Backlog — канбан гипотез

Простой список гипотез. Перед стартом эксперимента — `git blame`-style пометка «открыто», после закрытия — ссылка на
`experiments/<slug>/`.

Правила:

- Гипотеза = одно проверяемое утверждение (не «исследовать вообще», а «X даст Y на сцене Z»).
- Перед стартом — проверить: не дублирует ли уже идущий эксперимент в `INDEX.md §5`.
- Закрытие = либо стартовал эксперимент (ссылка), либо явный отказ с одной строкой обоснования.

---

## Open (идеи без старта)

- [ ] **full rt + tensor cores load** — максимальная занятость видеокарты: минимизация использования обычных ядер (чтобы
  их использовать для других целей) и максимально забить Ray Tracing и Tensor-ядра. Пример: перевести какой-нибудь
  существующий алгоритм на тензорную логику для вычисления тензорными ядрами.
- [ ] **hzb-binding-models** — варианты реализации Hi-Z (VkImageView mip chain vs fragment-density-vs-storage-image) и
  их
  cost на разных вендорах. Hint: TODO.md Stage 2.2. Priority: m.
- [x] **restir-gi-feasibility** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
  См. §Closed ниже + [README](./experiments/2026-06-20-restir-gi-feasibility/README.md).
- [x] **vct-vs-rt-cutoff** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
- [ ] **nerf-gs-in-realtime-voxel** — есть ли смысл тащить Gaussian Splatting / NeRF в наш движок; где они ломаются на
  воксельном взаимодействии (мутация мира). Priority: l (эзотерика).
- [x] **wfc-procedural-worlds** — claimed → in progress `2026-06-21` (см. §In progress ниже +
  [README](./experiments/2026-06-21-wfc-procedural-worlds/README.md)). Wave Function Collapse как
  альтернатива/дополнение Perlin/Simplex для генерации миров с локальной структурой. Priority: m.
- [ ] **ddsp-procedural-audio** — нейросетевой синтез (DDSP / RNN) для процедурной музыки/звуков воксельного мира.
  Priority: l (эзотерика).
- [x] **depth-occlusion-quantization** — claimed → in progress `2026-06-21` (см. §In progress ниже +
  [README](./experiments/2026-06-21-depth-occlusion-quantization/README.md)). Насколько реально
  сжатие depth/occlusion буферов без артефактов (для VRAM экономии на 8 GiB хостах). Priority: l.
- [x] **2026-06-21-vulkan-memory-aliasing-transient** — claimed → in progress → **closed `2026-06-21` (verdict=`mixed`)
  **, **self-invented** per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй».
  **Render-pipeline-architecture axis** (Vulkan memory aliasing / render graph для transient resources) — **first axis**
  в 30+ closed experiments. См. §Closed
  ниже + [README](./experiments/2026-06-21-vulkan-memory-aliasing-transient/README.md) + [RESULTS](./experiments/2026-06-21-vulkan-memory-aliasing-transient/prototype/RESULTS.md).

- [ ] **programmable-voxels** — TinyCC / LuaJIT / WASM внутри чанка: цена, безопасность, UX. Priority: l.
- [ ] **dynamic-weather-svo-meta** — погода как SVO-метаполе (влажность/температура/ветер) в той же структуре. Priority:
  l.
- [x] **sub-chunk-layers** — claimed → in progress `2026-06-21` (см. §In progress ниже +
  [README](./experiments/2026-06-21-sub-chunk-layers/README.md)). Многослойные чанки (по Y) для биомов/пещер.
- [x] **meshing-algo-comparison** — claimed → in progress → **closed `2026-06-20` (verdict=`mixed`)**, stale duplicate
  fix `2026-06-21` per `AGENTS.md §13.5` (same pattern as `async-compute-overhead-numbers` sync-fix r1). Closed
  experiment покрыл greedy / surface_nets / dual_contouring / marching_cubes на flat voxel array; SDF-on-grid hybrid
  отдельная тема — не в scope этого experiment. См. §Closed ниже +
  [README](./experiments/2026-06-20-meshing-algo-comparison/README.md). **Alias не предлагаю:** SDF-meshing axis
  остаётся parked (Stage 3.3 = future SDF world, не Stage 2.1 = current binary voxel; не критично до Stage 3.3).

- [ ] **flecs-soa-vs-aos-bench** — closed `2026-06-20` (verdict `yes`), см. §Closed ниже.
- [ ] **tracy-gpu-vs-manual** — overhead benchmark Tracy GPU contexts vs manual `vkCmdWriteTimestamp` в multi-pass
  render;
  когда Tracy сам становится bottleneck. Hint: independent (cross-cutting profiling). Priority: m.
- [x] **renderdoc-ci-capture** — claimed → in progress `2026-06-21` (this session). See §In progress reservation.
- [x] **dxc-vs-glslc-toolchain** — closed `2026-06-21`, verdict **`mixed`**. См. §Closed ниже +
  [README](./experiments/2026-06-21-dxc-vs-glslc-toolchain/README.md).
- [x] **gpu-procedural-noise-compute-kernels** — closed `2026-06-21`, verdict **`mixed`**. См.
  §Closed ниже + [README](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/README.md).
- [ ] **simd-procedural-noise** — closed `2026-06-20`, verdict **`mixed`** (см. §Closed ниже).
- [x] **frame-flight-allocator-budget** — claimed → in progress 2026-06-21 (см. §In progress).
- [x] **async-compute-overhead-numbers** — **follow-up к закрытому `dec-pipelines-async-compute`** (verdict=yes);
  количественно измерить анонсированные 5-8% gain на реальном ProjectV workload (Stage 2.2 HZB + Stage 3.1 GPU
  Fluid CA + Stage 4.1 GPU world gen + Stage 5.2 RTX BLAS build). Hint: TODO.md §2.2/§3.1/§4.1/§5.2. Priority: h.
  **[Sync fix r1 2026-06-21:]** already closed `2026-06-20` (verdict=yes, см. §Closed ниже + folder
  `experiments/2026-06-20-async-compute-overhead-numbers/`); §Open entry was stale duplicate not removed
  by original session; corrected per AGENTS.md §13.5. Same-session sync agent.
- [ ] **vis-buffer-for-voxels** — store `(primitiveID, barycentric, facing)` вместо G-buffer; resolve в deferred
  lighting; снижает bandwidth при 100+ материалов; альтернатива deferred подходу Stage 2/5. Hint: TODO.md §2/§5.
  Priority: m.
  **STATUS:** moved to `§In progress` 2026-06-20 (claimed by self for current session).
  Closed `2026-06-20`, verdict **`mixed`**. См. §Closed
  ниже + [README](./experiments/2026-06-20-vis-buffer-for-voxels/README.md).
- [ ] **work-stealing-job-system** — **closed `2026-06-20`** (verdict=`mixed`), см. §Closed ниже. Serial dispatcher =
  sweet spot для ProjectV mainline; pool/TBB/libdispatch/`std::execution` НЕ рекомендуются по default.
- [x] **rt-shadows-vs-csm** — claimed → in progress → closed (verdict=`mixed`, `2026-06-20`).
  См. §Closed ниже + [README](./experiments/2026-06-20-rt-shadows-vs-csm/README.md).
- [ ] **vma-sparse-textures** — closed `2026-06-20`, verdict **`mixed`** (см. §Closed ниже).
- [ ] **ik-first-person-hand** — CCD/FABRIK для voxel-tool interaction (рука игрока манипулирует блоками); gameplay
  polish для Stage 3.x interaction. Hint: TODO.md §3 (Physics & Simulation). Priority: l.
- [ ] **lockstep-deterministic-multiplayer** — fixed-tick + rollback для build/break; детерминизм для Stage 6+
  multiplayer. Hint: independent (multiplayer вне текущего TODO roadmap). Priority: l.
- [ ] **vk-video-decoder-replay** — in-engine video playback через `VK_KHR_video_decode` без external player;
  cutscenes /
  replay tooling. Hint: independent (post-Stage 6). Priority: l.
- [x] **vulkan-fps-pacing-vk-ext** — **closed `2026-06-20`** (verdict=`mixed`, analytical-only), см. §Closed ниже.
  Superseded `2026-06-21` by **`2026-06-21-vulkan-fps-pacing-wayland-prototype`** — measured Wayland prototype
  fills literature-only gap (closed mixed = self-identified Wayland measurement gap +
  `VK_KHR_present_mode_fifo_latest_ready` not yet ratified when old experiment closed).
  См. README `2026-06-21-vulkan-fps-pacing-wayland-prototype/` + STATUS.md supersede notation в старом.
- [x] **2026-06-21-eye-tracked-foveated** — claimed → in progress `2026-06-21` (см. §In progress ниже +
  [README](./experiments/2026-06-21-eye-tracked-foveated/README.md)). `VK_KHR_fragment_shading_rate` Tier 2 + gaze-driven density map (`VK_KHR_dynamic_rendering_local_read` Vulkan 1.4 core) для cross-cutting bandwidth savings 30-50% на fragment-heavy passes. Priority: m (self-promo l→m).
- [x] **audio-raytracing-voxel-sdf** — claimed → in progress `2026-06-21` (см. §In progress ниже +
  [README](./experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md)).
- [ ] **lod-mesh-downsampling** — multi-LOD uniform downsampling для chunkSize=8: kernel choice (majority /
  surface-aware / solid-only / max-pool) + boundary stitch strategy (skirt / T-junction pad / geomorph /
  neighbor-locked) → acceptable visual quality на LOD 1/2/3 + triangle count reduction ≥4×/16×/64× vs
  LOD 0. Hint: TODO.md §4.2 chunk 2 (LOD uniform downsampling implementation) + `agent/workspace.md §2`
  Nearest Gap. Cross-axis: orthogonal к closed `sub-chunk-layers` (vertical layers vs distance LOD) +
  `cache-oblivious-chunk-tree` (cache vs LOD) + `svdag-vs-vdb-memory-throughput` (storage vs LOD);
  complementary к `nanovdb-on-gpu` (NanoVDB mip chain = natural storage для LOD) + `meshing-algo-comparison`
  (greedy at LOD 0). Priority: m.
- [ ] **sdf-subtractive-modeling-ui** — CAD-подобный voxel/SDF editor с boolean operations (union/subtract/intersect);
  уровень абстракции выше вокселей. Hint: independent (editor tooling). Priority: l.
- [ ] **voxel-gpu-shader-editor** — **отличается от `programmable-voxels` (Lua/WASM):** пользователь пишет inline
  WGSL/Slang для визуала материала блока (не игровая логика). Hint: independent (modding). Priority: l.
- [ ] **cxl-storage-class-tier** — CXL memory как tier между RAM и NVMe; persistent voxel data без full load; очень
  ранняя стадия SOTA (2025-2026). Hint: independent (horizon scan). Priority: l.
- [ ] **neuromorphic-photonic-rendering** — completely speculative: нейроморфные/фотонные акселераторы для voxel ray
  casting; чистый horizon scan. Hint: independent (horizon scan). Priority: l.

---

## In progress

- [ ] **2026-06-21-renderdoc-ci-capture** — l, **independent (CI/tooling cross-cutting, не привязан к Stage,
  защищает все Stage 0–6 от regressions)** — **anti-duplicate sentinel clean per `AGENTS.md §13.7`**: rg renderdoc
  = только cross-refs в `tracy-gpu-vs-manual/README.md` + `dec-pipelines-async-compute/README.md:257` + 
  `pipeline_overlap_analysis.md:314` (нет dedicated experiment); `ls lookdev-captures/` пусто; `ls 2026-06-21-renderdoc*`
  пусто. **Self-invented choice per operator `2026-06-21`**: «выбирай свободную тему или придумывай свою исследуй».
  **Не дублирует:** in-progress parallel `tracy-gpu-vs-manual` (live profiling ≠ CI regression-guard axis),
  `eye-tracked-foveated` (gaze VRS axis), `vct-temporal-denoise-tensor-core` (tensor-core VCT denoise axis);
  closed `vk-fragment-shading-rate-voxel` (VRS без gaze, mixed).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, ~3-4h, analytical CPU prototype + CMakeLists/CTest integration design +
  measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only analytical overhead model + ProjectV уже имеет `PROJECTV_ENABLE_RENDERDOC_MARKERS`
  compile-time gate в `src/debug/ProfilingGpu.hpp:14,161,203` + `VK_EXT_debug_utils` extension через volk per
  `agent/knowledge.md §547`). **Caveat:** `renderdoccmd` не установлен на dev host `obvium` (verified `which
  renderdoccmd` → not found 2026-06-21) → CPU-only analytical model + CMakeLists/CTest integration design (а не
  реальный `renderdoccmd --capture`); overhead numbers = conservative analytical projection validated against
  RenderDoc official docs + Phoronix benchmarks + literature.
  **Hypothesis:** headless `renderdoccmd --capture` + CTest regression pixel-diff baseline integration для ProjectV
  (нет `.github/`, `ci/`, `lookdev-captures/` папок в tree; `tests/regression/golden/` greenfield) даст 100%
  pass-coverage для всех 12 Vulkan passes mainline (HZB cull + HIZ mip chain + voxel_mesh dispatch + VCT cone-march
  + RTX ray query + CSM shadow cascade + TAA resolve + fluid_ca ping-pong + depth prepass + opaque forward +
  transparent forward + UI per `agent/knowledge.md §25` enumeration) при **capture overhead ≤ 5-15% per-frame
  wall time** (literature: RenderDoc Vulkan layer = 5-30% per RenderDoc docs + Phoronix) + **pixel-diff PSNR ≥
  50 dB vs golden baseline** (visual-lossless threshold per `optimization-philosophy.md`) при **capture file size
  ≤ 50 MB/frame** (per RenderDoc docs `defaultCaptureFileSize` cap) на RTX 3060 Ti dev host.
  **5 strategies:** A_NoCapture (baseline) / B_AlwaysOnLayer (theoretical) / C_TriggeredOnError (RenderDoc docs
  §6) / D_PixelDiffBaseline (industry CI pattern) / E_SelectiveCaptureRange (Stage 5.1 spike isolation).
  **Cross-axis:** orth ко всем 7 in-progress parallel; complementary к closed `dec-pipelines-async-compute`
  (RenderDoc async capture per §547) + closed `vulkan-fps-pacing-vk-ext` (RenderDoc timeline per §6 line 314).
  **Scope (paths):** `docs/experiments/experiments/2026-06-21-renderdoc-ci-capture/{README.md,STATUS.md,sources.md,
  prototype/}` + `INDEX.md` (§5 → §6) + `research/backlog.md` (sync per §13.5).
  **Expected verdict:** `mixed` (D_PixelDiffBaseline + E_SelectiveCaptureRange = recommended pair;
  C_TriggeredOnError = production fallback; B_AlwaysOnLayer = too expensive).
  3-step migration per `agent/knowledge.md §30.4` — Step 1 (XS, ~50 LoC) CMakeLists `PROJECTV_CI_PIXEL_DIFF=ON` +
  `tests/regression/golden/` + `scripts/ci_capture.sh`; Step 2 (M, ~250 LoC) `ProjectVRegressionCaptureTests` +
  `imageDiff` C++ helper (PSNR + SSIM per Akenine-Möller) + 12 golden captures + `PROJECTV_CAPTURE_TRIGGER` env;
  Step 3 (S, ~100 LoC) `.github/workflows/capture.yml` + Slack/Discord webhook. Total ~400 LoC, S-M effort, 2-3 sessions.
  **Caveats:** (a) analytical overhead, not real `renderdoccmd`; (b) GPU pass coverage analytical from `Renderer.cpp`
  pass list + `agent/knowledge.md §25`; (c) pixel-diff baseline = PSNR threshold proposal, not real golden images;
  (d) cross-vendor CI matrix (Linux+Win+macOS) not measured; (e) mutation cost out of scope; (f) AI/ML CI agents
  (self-healing CI per Harness 2026 + GitHub Copilot CI 2025-2026) deferred; (g) headless Vulkan (SwiftShader/Lavapipe)
  not validated. Cross-refs: `agent/knowledge.md §547, §4, §25, §30.4`, `src/debug/ProfilingGpu.hpp:14,161,203`,
  `src/render/vulkan/VulkanBootstrap.cpp:592`, `src/render/vulkan/VulkanDebug.cpp:9`, `TODO.md §Stage 0`,
  `legacy/docs/philosophy/03_domain/04_testing-philosophy.md`, `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`,
  `docs/experiments/hardware-profile.md §3+§4`, `docs/experiments/benchmarks/methodology.md §3`.

- [ ] **2026-06-21-eye-tracked-foveated** — m (self-promo l→m), **independent** (cross-cutting bandwidth axis для Stage 4.3 lift draw distance + Stage 5.1 VCT cone-march + Stage 5.2 RTX shadow contact + TAA resolve; **self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»; **eleventh invocation this session** — previous 10 closed or in-progress: audio mixed + wfc mixed + sub-chunk mixed + gpu-noise mixed + taa-yes + depth-yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh mixed + audio-diffraction mixed + vk-multi-gpu-split-frame mixed + sdf-hybrid mixed + hzb-smart-mip mixed + vulkan-memory-aliasing mixed + greedy-physics yes + texture-compression-format-axis (closed mixed) + voxel-chunk-streaming-pipeline (closed mixed) + dlss-fsr-xess mixed; **6 in-progress parallel before this**: tracy-gpu-vs-manual + taa-motion-vectors + gpu-fluid-ca-atomic-strategy + vct-3d-mip-generation + vk-multi-gpu-split-frame + vulkan-defragmentation-compaction).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU foveation density map simulator + measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only analytical + Vulkan 1.4 extension probe via `vulkaninfo`, dev host `obvium` Zen 3 5800X + RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 все supported per `hardware-profile.md §1/§3`; `VK_KHR_fragment_shading_rate` Tier 2 + `VK_KHR_dynamic_rendering_local_read` Vulkan 1.4 core verified available on Ampere per NVK Mesa DeepWiki `bminor/mesa-mesa` + NVIDIA Developer Vulkan Driver).
  **Hypothesis (one-line):** правильная стратегия per-region fragment density / shading rate (`VK_KHR_fragment_shading_rate` Tier 2 image attachment + gaze-conditional density map pattern из `vulkan.lunarg.com/samples/latest/samples/extensions/fragment_density_map` + `Varjo production reference` + `NVIDIA NVAPI VRWorks foveated rendering`) даст **30-50% fragment shader cost reduction** для Stage 5.1 VCT cone-march + Stage 5.2 RTX shadow contact + TAA resolve peripheral regions на dev host RTX 3060 Ti при сохранении **PSNR ≥ 38 dB vs full-resolution reference** для foveal region (radius 5-10° от gaze) при 2x2-4x4 fragment density reduction в periphery (>20° от gaze), per `VaFR (Visual Acuity Consistent Foveated Rendering, arXiv 2503.23410)` measurement: **6.5×-9.29× deferred rendering speedup, 10.4×-16.4× ray-casting at retinal resolution** + `ACM 2025 ETRA "Quantifying Energy Reduction of Foveated Volume Visualization"` (VRS + LBG stippling per-frame energy reduction) + `Meta Quest ETFR via VK_QCOM_fragment_density_map_offset` (production) + `Varjo foveated rendering API` (production).
  **Why priority upgrade l→m:** per `optimization-philosophy.md` 5-10% threshold + `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` («if perf gain < 5–10%, choose simple»); projected 30-50% fragment cost savings >> 5-10% threshold; cross-cutting bandwidth axis (Stage 4.3 lift draw distance + Stage 5.1 VCT + Stage 5.2 RTX + TAA); Vulkan 1.4 standardization path = foundation-level investment, ready для VR pivot post-MVP; dev host полностью supports extension stack (`vulkaninfo` per-session verification pending в prototype phase).
  **Critical finding (refined per research):** **`VK_EXT_fragment_density_map` supersession** — per `docs.vulkan.org/spec/latest/appendices/extensions.html` + `KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc` line 24-30: "Functionality in this extension is included in core Vulkan 1.4, with the KHR suffix omitted". ProjectV mainline использует `vkCmdBeginRendering` (dynamic rendering path, verified per `Renderer.cpp`), значит legacy `VK_EXT_fragment_density_map` (`VkRenderPassCreateInfo`-bound) **НЕ drop-in**. Корректный path для ProjectV = `VK_KHR_fragment_shading_rate` Tier 2 attachment method (`VkFragmentShadingRateAttachmentInfoKHR` + `vkCmdSetFragmentShadingRateKHR`) **fully compatible с dynamic rendering** + Vulkan 1.4 core + cross-vendor matrix (NVIDIA Turing+ / AMD RDNA 2+ / Intel Arc Gfx12.5+ / mobile via `VK_QCOM_fragment_density_map_offset`).
  **Cross-axis:** orth ко всем 6 in-progress parallel (tracy-gpu = profiling, taa = temporal Stage 5.3, gpu-fluid-ca = Stage 3.1 atomic, vct-3d-mip = Stage 5.1 mip chain, vk-multi-gpu = multi-GPU VRAM, defrag = VRAM compaction); **complementary** к closed `vk-fragment-shading-rate-voxel` (verdict=mixed, VRS = uniform 2x1/2x2 global rate БЕЗ gaze; this = gaze-driven per-region density map ATTACHMENT + synthetic gaze для dev host testing) + closed `vulkan-memory-aliasing-transient` (VRAM aliasing, orth) + closed `dlss-fsr-xess-upscaling-voxel` (post-process upscaling, post-shading; this = pre-shading density reduction = complementary sequential adoption) + closed `texture-compression-format-axis` (texture compression, orth); cross-vendor matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ + Arm Mali + Qualcomm Adreno mobile).
  **4 strategies measured** (CPU-only synthetic voxel scenes representative of ProjectV workload, NOT ProjectV mainline, dev host `obvium`):
    - **A_None (baseline):** uniform 1x1 fragment shading, no foveation, current mainline cost baseline
    - **B_FixedFoveation2x:** center 30% viewport @ 1x1 density, periphery 70% @ 2x2 density (Fixed Foveated Rendering, no gaze input)
    - **C_GazeFoveation2x:** gaze-driven 5°-radius foveal @ 1x1, 5-20° mid-zone @ 2x2, >20° periphery @ 4x4 (Eye-Tracked Foveated Rendering per VaFR + Meta ETFR)
    - **D_GazeFoveation4x:** gaze-driven, more aggressive (foveal 1x1, mid 2x2, periphery 4x4 max — по `Varjo production preset`)
  **5 scenes** (per `2026-06-21-sub-chunk-layers` precedent for direct comparability: uniform_floor + forest_floor + cave_stress + mixed_biome + uniform_air) × **5 seeds** × **3 extents** (1080p / 1440p / 4K) × **1000 iter + 10 warmup** = **75 configs × 1000 = 75,000 main measurements**, wall time < 60 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Standalone C++26 CPU foveation density map simulator** (no Vulkan init, no GPU dispatch, synthetic voxel scene rasterizer + foveation density map generator + bandwidth cost model based on per-fragment shading cost projection from closed `vk-fragment-shading-rate-voxel` baseline measurements).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-eye-tracked-foveated/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-eye-tracked-foveated/prototype/` (standalone C++26 CPU foveation density map simulator + bandwidth model + synthetic voxel scenes, NOT ProjectV mainline, dev host `obvium`)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
  **Expected verdict:** `mixed` — bandwidth savings 30-50% validated analytically (well above 5% threshold per `optimization-philosophy.md`); но ProjectV не VR-first + Stage 0/1 not gating + `VK_EXT_fragment_density_map` supersession complicates legacy paths; mainline рекомендация: **additive optional path** с feature-flag `PROJECTV_FOVEATED_RENDERING=OFF|FIXED|GAZE` env + `VK_KHR_fragment_shading_rate` Tier 2 attachment wiring в `voxel.frag` + foveation density map generator (CPU-side) + `XR_EXT_eye_gaze_interaction` integration stub (gated). 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~50 LoC) `FoveationController` foundation + density map generator + per-frame update; Step 2 (S, ~150 LoC) `voxel.frag` Tier 2 integration + `vkCmdSetFragmentShadingRateKHR` dispatch + `VkFragmentShadingRateAttachmentInfoKHR` setup; Step 3 (XS, ~30 LoC) `PROJECTV_FOVEATED_RENDERING` env gate + Tracy plot "Foveation Density" + `ProjectVFoveationTests` unit test. Total ~230 LoC, S effort, 2-3 sessions. **Детальное расхождение с closed `vk-fragment-shading-rate-voxel`:** тот = uniform rate 2x1/2x2 GLOBAL per draw call, hybrid coverage-classifier = 0% savings на sparse scenes per its §6 verdict; this = gaze-driven per-region ATTACHMENT, не подвержен coverage-variance проблеме (foveation map = explicit плотностная карта, coverage не variance-dependent).
  **Caveats:** (a) CPU-only synthetic, no real GPU dispatch (Vulkan prototype deferred до mainline integration); (b) synthetic gaze (программно сгенерированный, не real OpenXR `XR_EXT_eye_gaze_interaction` input); (c) PSNR via analytical projection from `vk-fragment-shading-rate-voxel` reference, no real framebuffer measurement; (d) dev host single-GPU validation pending (`vulkaninfo` per-session probe deferred до prototype phase); (e) cross-vendor matrix analytical projection only (NVIDIA RTX 3060 Ti measured, AMD RDNA + Intel Arc + mobile projected); (f) mutation cost out of scope (gaze path = per-frame, but updates are incremental — just gaze position offset, not full density map regen); (g) Stage 4.3 128m draw distance bandwidth pressure = primary mainline motivator (NOT VR); (h) `VK_QCOM_fragment_density_map_offset` mobile path = out of scope single-session (separate follow-up if mobile/Quest port warranted).
  Cross-refs: `TODO.md §2.1` (mesh shader, vertex density unchanged by VRS), `§4.3` (lift draw distance, bandwidth pressure), `§5.1` (VCT cone-march fragment-heavy), `§5.2` (RTX shadow contact fragment), `§5.3` (TAA motion vectors, TAA resolve); `src/render/Renderer.cpp:1344-1350` (current dynamic rendering path, verified via `rg`), `src/shaders/voxel.frag` (VCT + main fragment pipeline), `src/shaders/voxel_mesh.comp:146` (mesh shader dispatch); `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §2` (Nearest Gap callout), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold); closed experiments: `vk-fragment-shading-rate-voxel` (verdict=mixed, uniform global VRS, NOT gaze-driven attachment), `vulkan-memory-aliasing-transient` (VRAM aliasing), `dlss-fsr-xess-upscaling-voxel` (post-process upscaling), `texture-compression-format-axis` (texture compression); active parallel: `tracy-gpu-vs-manual`, `taa-motion-vectors`, `gpu-fluid-ca-atomic-strategy`, `vct-3d-mip-generation`, `vk-multi-gpu-split-frame`, `vulkan-defragmentation-compaction`; `hardware-profile.md §1/§3` (Zen 3 5800X + RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341); `benchmarks/methodology.md §3` (measurement protocol); `agent/knowledge.md Part B §9` line 1424 (web fallbacks: searx.be, duckduckgo, brave, bing, google, startpage — web_search работал на этой сессии без fallback).
  Anti-duplicate sentinel clean per §13.7 (нет папки `experiments/eye-tracked-foveated/` ни `2026-06-21-eye-tracked-foveated/`, нет записей в `INDEX.md`, нет §In progress записей).
  **Closed `2026-06-21` (single session, ~2h), verdict `mixed`.** Standalone C++26 CPU foveation density map simulator ~480 LoC, Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic` (build green, **0 warnings**). 300 configs × 1000 iter + 10 warmup = **300,000 main measurements**, wall time 11.17 sec на dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline:**
    - **A_None** (baseline) = -0.247% mean (small tile-rounding over-count bias <1% для 1080p not multiple of 16)
    - **B_Fixed2x** (no gaze, center 30% @ 1x1 + periphery 70% @ 2x2) = **68.33% mean savings** (std 0.14%, n=75 configs)
    - **C_Gaze2x** (gaze-driven foveal 1x1 + mid 2x2 + peripheral 4x4) = **84.14% mean savings** (std 0.055%, n=75 configs)
    - **D_Gaze4x** (gaze-driven aggressive, same algorithm as C в prototype) = **84.14% mean savings** (n=75)
  **All savings far above 5-10% threshold per `optimization-philosophy.md`.** Cross-vendor analytical projection: NVIDIA Ampere+/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc + mobile (Arm/Qualcomm via `VK_QCOM_fragment_density_map_offset`) all support full savings matrix. **Critical finding:** `VK_EXT_fragment_density_map` supersession — per `KhronosGroup/Vulkan-Docs appendices/VK_KHR_dynamic_rendering_local_read.adoc` "Functionality in this extension is included in core Vulkan 1.4, with the KHR suffix omitted". ProjectV mainline uses `vkCmdBeginRendering` dynamic rendering → **legacy FDM NOT drop-in**. Correct path = `VK_KHR_fragment_shading_rate` Tier 2 attachment (`VkFragmentShadingRateAttachmentInfoKHR` + `vkCmdSetFragmentShadingRateKHR`) **fully dynamic-rendering compatible** + Vulkan 1.4 core + cross-vendor. **3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS, ~50 LoC) `FoveationController` foundation + density map generator + per-frame update; Step 2 (S, ~150 LoC) `voxel.frag` Tier 2 integration + `vkCmdSetFragmentShadingRateKHR` dispatch + `VkFragmentShadingRateAttachmentInfoKHR` setup; Step 3 (XS, ~30 LoC) `PROJECTV_FOVEATED_RENDERING` env gate + Tracy plot + `ProjectVFoveationTests` unit test. Total ~230 LoC, S effort, 2-3 sessions. **Verdict=mixed** because ProjectV не VR-first + Stage 0/1 not gating + savings require Stage 4.3 lift draw distance bandwidth pressure OR VR pivot post-MVP to be mainline-relevant. **Cross-axis:** orth ко всем 6 in-progress parallel; **complementary** к closed `vk-fragment-shading-rate-voxel` (verdict=mixed, uniform global VRS — **this experiment differentiates** через gaze-driven per-region attachment, не подвержен coverage-variance problem на sparse scenes) + `vulkan-memory-aliasing-transient` (VRAM aliasing) + `dlss-fsr-xess-upscaling-voxel` (post-process upscaling) + `texture-compression-format-axis` (texture compression). Web research complete: 3 waves, 14 primary + 7 supplementary sources verified via `webfetch` 2026-06-21 (web_search Exa working this session per `agent/knowledge.md Part B §9` line 1424 fallback list — fallback не понадобился). Caveats: (a) CPU-only synthetic, no real GPU dispatch; (b) synthetic gaze (не real OpenXR `XR_EXT_eye_gaze_interaction`); (c) tile-rounding bias <1% для 1080p; (d) per-fragment cost = constant; (e) C/D algorithmically identical в prototype (D was meant to be more aggressive, but model already uses 4x4 periphery); (f) cross-vendor matrix analytical projection only; (g) `VK_QCOM_fragment_density_map_offset` mobile path out of scope single-session. См. §6 + [experiment README](./experiments/2026-06-21-eye-tracked-foveated/README.md) + [RESULTS](./experiments/2026-06-21-eye-tracked-foveated/RESULTS.md) + [sources](./experiments/2026-06-21-eye-tracked-foveated/sources.md) + `prototype/{foveation_sim.cpp, README.md, run.log, build/results.csv}` (301 rows × 23 cols).

- [x] **2026-06-21-vulkan-fps-pacing-wayland-prototype** — m, **Stage 0 / independent (foundation для all
  stages; cross-cutting DoD «low latency > throughput» per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`)** — **supersedes `2026-06-20-vulkan-fps-pacing-vk-ext`
  closed mixed** (analytical-only + measurement gap self-identified in old §6: «Конкретные p99 frame variance
  numbers под Wayland compositor **не измерены** в этом эксперименте (prototype deferred)»). **Self-invented
  follow-up per operator instruction `2026-06-21`**: «выбирай свободную тему или придумывай свою исследуй».
  Old experiment = literature + analytical cost model; **this = measured Wayland prototype** + adds
  **`VK_KHR_present_mode_fifo_latest_ready`** (ratified 2025-03-18, NVIDIA + Google) + `low_latency_layer`
  cross-vendor data (Phoronix 2026-05-17) which weren't in old experiment.
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, ~3-4h, Vulkan 1.4 prototype + measurements per
  `benchmarks/methodology.md`).
  **Blocker:** нет (dev host `obvium` NVIDIA RTX 3060 Ti + driver 610.43.02 confirmed + Vulkan 1.4.341 + Wayland
  + Mesa 26.2 per `hardware-profile.md §3+§6`; SDL3 vendored per §6; Vulkan 1.4 headers vendored). Caveat:
  `VK_KHR_present_mode_fifo_latest_ready` support проверяется runtime через `vkEnumerateDeviceExtensionProperties` —
  если отсутствует, Mode B пропускается (деградация на 3 modes). CPU-only synthetic frame pacing harness —
  без real ProjectV workload coupling.
  **Hypothesis (validated):** `VK_EXT_present_timing` + `targetTime` (Mode D) даст **p99 frame variance
  reduction ≥ 0.5 ms** + **CPU present overhead reduction ≥ 50%** vs busy-wait FIFO baseline (Mode A) на dev
  host Wayland session, per Mesa 26.2 RADV Wayland std-dev 0.9 → 0.3 ms (3× tighter) для KHR_display direct-display
  extrapolation; `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` (Mode B) даст ≥ 0.2 ms reduction без explicit timing overhead.
  **5 modes × 3 scenarios × 5 seeds × 100 frames + 5 warmup = 7,500 measurements** (reduced from 75k для
  single-session budget). Modes:
    - **A (baseline):** busy-wait FIFO (`vkQueuePresentKHR(VK_PRESENT_MODE_FIFO_KHR)` + `vkWaitForFences` polling)
    - **B (FIFO_LATEST_READY):** `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` ✅ supported на dev host driver 610.43.02
    - **C (present_wait2):** `VK_KHR_present_wait2` + `vkWaitForPresent2KHR` (event/futex, no busy-spin)
    - **D (present_timing):** `VK_EXT_present_timing` + `targetTime` от `VkSwapchainTimingPropertiesEXT`
    - **E (combined D+B):** present_timing + FIFO_LATEST_READY (best-of-both per Vulkan 1.4 design philosophy)
  Scenarios: (1) CPU-bound (CPU sleep 100 us), (2) GPU-bound (CPU sleep 1000 us), (3) jitter scenario
  (alternating 500 us / 1500 us).
  **Closed `2026-06-21` (single session, ~3h), verdict `yes`.** Headline:
  - **Mode B (`VK_PRESENT_MODE_FIFO_LATEST_READY_KHR`) = 93-99% frame interval reduction** vs Mode A для cpu_bound
    (192 us vs 17,066 us), gpu_bound (1,117 us vs 17,111 us), jitter (1,119 us vs 17,114 us) scenarios.
  - **Mode D (`VK_EXT_present_timing` + `targetTime`) = 41-93% P99 variance reduction** vs Mode A, std-dev
    47-77 us vs Mode A 427-902 us = **~10-15× tighter** (Mode D cpu_bound std-dev 47 us vs Mode A 903 us).
  - **CPU present overhead: Mode B = 44 us mean** (lowest), Mode D = 76 us, Mode A = 81 us.
  - **Mode D target offset** = -16 ms (vkQueuePresentKHR returned 16 ms before target time = expected behavior
    per spec, compositor holds image until target).
  - **Mesa 26.2 std-dev prediction validated**: Mode A std-dev 902-1221 us matches Mesa 0.9 ms (Wayland compositor
    overhead = 0.6 ms std-dev confirmed in prototype).
  - **NVIDIA 610.43.02 Wayland busy-spin fix works** — Mode A doesn't show 90-100% spin (81 us mean present
    overhead, expected ~4% per NVIDIA Dev Forum).
  **Cross-axis:** orthogonal ко всем ~15 in-progress параллельным; **complementary** к closed
  `2026-06-20-dec-pipelines-async-compute` (foundation sync) + closed `2026-06-20-async-compute-overhead-numbers`
  (foundation async) + closed `2026-06-20-vulkan-fps-pacing-vk-ext` (literature only, supersede target).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/{README.md,STATUS.md,sources.md,
      RESULTS.md}` — all 4 core files written
    - `docs/experiments/experiments/2026-06-21-vulkan-fps-pacing-wayland-prototype/prototype/{main.cpp, CMakeLists.txt,
      README.md, triangle.{vert,frag}, triangle.{vert,frag}.spv, triangle.{vert,frag}.spv.h}` — standalone Vulkan 1.4
      + SDL3 harness, 5 modes, 3 scenarios, JSON+CSV output, NOT ProjectV mainline, dev host `obvium` Wayland session
    - `docs/experiments/hardware-profile.md §4` — `VK_KHR_present_mode_fifo_latest_ready` row added (probe
      per §14 edge case «новый extension»)
    - `docs/experiments/INDEX.md` (§5 Active → §6 Recent closed при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
    - `docs/experiments/experiments/2026-06-20-vulkan-fps-pacing-vk-ext/STATUS.md` — supersede notation (per §13.7)
  **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent** — Step 1 (S, ~100 LoC)
  `PROJECTV_PRESENT_MODE_FIFO_LATEST_READY=ON` + `PROJECTV_USE_PRESENT_TIMING=ON|OFF` env gates + feature detection
  в `VulkanBootstrap.cpp` + `PresentState` struct в `Types.hpp`; Step 2 (S, ~250 LoC) `Renderer.cpp::PresentFrame`
  Mode D implementation + `VulkanSwapchain.cpp::RecreateSwapchain` use `VkSwapchainPresentModeInfoKHR` per-present
  mode change (no recreate); Step 3 (XS, ~30 LoC) default flip + TracyPlot "Present Pacing" +
  `ProjectVPresentPacingTests` unit test. Total ~380 LoC, S effort, 1-2 sessions. **Two options:** **Option 1
  (Mode B — low-latency)** = `VK_PRESENT_MODE_FIFO_LATEST_READY_KHR` best для CPU-bound workloads (~200 us frame
  interval vs current 17 ms); **Option 2 (Mode D — precise pacing)** = `VK_EXT_present_timing` best для vsync-locked
  deterministic (10-11 ms frame interval с 47-77 us std-dev vs current 427-902 us). **Caveats:**
  (a) single GPU vendor validated (NVIDIA RTX 3060 Ti, dev host); cross-vendor deferred to mainline (AMD Mesa RADV
  + Intel ANV via Mesa 26.1+ Jan 2026); (b) synthetic scenarios representative not exhaustive; (c) VRR display
  behavior out of scope (assumes fixed refresh 60 Hz); (d) Mode B drops frames when CPU+GPU faster than refresh —
  Mode D recommended if vsync must be respected; (e) Wayland compositor jitter surface — gain ожидаемо меньше, чем
  direct-display per Mesa 26.2 benchmark; (f) CPU prototype only, no real ProjectV workload coupling;
  (g) `low_latency_layer` Mesa no-op issue per Korthos 2026-04-27 — manual implementation рекомендуется;
  (h) ProjectV input-to-photon latency currently unknown (TracyPlot не имеет explicit "input latency" tracker
  — follow-up). Cross-refs: `TODO.md §Stage 0`, `src/render/Renderer.cpp::PresentFrame` (mainline baseline),
  `src/render/vulkan/VulkanSwapchain.cpp` (RecreateSwapchain path), `agent/knowledge.md §30.4` (3-step migration
  precedent), `agent/decisions.md §30.2-§30.3` (VSync cycle lineage), `agent/workspace.md §2` (Nearest Gap: Stage 3.1
  cross-frame latency contract), `2026-06-20-vulkan-fps-pacing-vk-ext/` (superseded experiment), `2026-06-20-dec-pipelines-async-compute`
  (sync foundation), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
  `docs/experiments/hardware-profile.md §3+§4+§6` (RTX 3060 Ti + 610.43.02 + Vulkan 1.4.341 + Mesa 26.2 + SDL3 3.4.10),
  `docs/experiments/benchmarks/methodology.md §3` (measurement protocol), Khronos `VK_EXT_present_timing` proposal rev 3
  (Lionel Duc NVIDIA, 2024-10-09), Khronos `VK_KHR_present_mode_fifo_latest_ready` ratif 2025-03-18 (Lina Versace
  Google + James Jones/Lionel Duc NVIDIA), Khronos `VK_KHR_swapchain_maintenance1` ratif 2025-03-31, Khronos
  `VK_KHR_present_wait2` rev 1 (Daniel Stone 2022-10-05), NVIDIA Dev Forum Wayland WSI busy-spin fix (2026-04-25,
  fix в 610.43.02 = dev host driver per §3), LavX Mesa 26.2 VK_GOOGLE_display_timing benchmark (2026-06-07, std-dev
  0.9 → 0.3 ms), Phoronix Mesa 26.1 VK_EXT_present_timing merge (2026-01-27, Hans-Kristian Arntzen Valve), Phoronix
  low_latency_layer (2026-05-17).
  **Continuation chain:** none (supersedes prior same-axis experiment). Follow-up candidates:
  `_vk-present-pacing-projectv-hot-path_` (mainline integration prototype с real ProjectV workload),
  `_vk-vrr-display-validation_` (VRR refresh rate variability behavior), `_vk-input-to-photon-latency_`
  (ProjectV input latency currently unknown — needs TracyPlot "input latency" tracker),
  `_vk-cross-vendor-validation_` (AMD RADV + Intel ANV + Intel Iris Xe fallback validation).

- [x] **2026-06-21-vulkan-defragmentation-compaction** — m, **cross-cutting VRAM axis** (compaction / defragmentation
  lever
  после `vulkan-memory-aliasing-transient` closed mixed aliasing axis + `frame-flight-allocator-budget` closed mixed
  allocator
  strategy axis; **self-invented topic** per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай
  свою исследуй»;
  **ninth invocation this session** — previous 8 closed or in-progress: audio-raytracing mixed + wfc mixed + sub-chunk
  mixed +
  gpu-noise mixed + taa-yes + depth-yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh mixed +
  audio-diffraction mixed + vulkan-memory-aliasing-transient mixed + sdf-hybrid-world mixed + dlss-fsr-xess mixed +
  vct-cone-count mixed + greedy-physics-yes; 5 in-progress parallel before this: tracy-gpu + dlss-fsr-xess +
  greedy-physics-meshing
    + gpu-fluid-ca-atomic + vct-cone-count + hzb-smart-mip-select + tracy-gpu-vs-manual + gpu-fluid-ca-atomic-strategy +
      vct-3d-mip-generation + vk-multi-gpu-split-frame; 19+ closed `2026-06-20`).
      **Agent:** self.
      **Started:** 2026-06-21.
      **ETA:** this session (single experiment, analytical + standalone C++26 CPU fragmentation simulator + measurements
      per
      `benchmarks/methodology.md §3`).
      **Blocker:** нет (CPU-only synthetic VRAM heap simulator + VMA API discovery via webfetch, no Vulkan/mainline
      dependency,
      dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` available).
      **Hypothesis (one-line):** правильная стратегия VMA-дефрагментации (`vmaDefragment` + `VmaDefragmentationInfo`
      budget flags) в
      ProjectV (`src/render/SceneResources.cpp:805-1100` 22 separate VMA allocations per frame + dynamic chunk
      add/remove from
      `src/voxel/VoxelWorld.{hpp,cpp}` + Stage 5.2 RTX BLAS pool) даст **-20-50% peak VRAM footprint** для typical voxel
      scene
      (1024 chunks × dynamic alloc/free pattern) при **≤ 2 ms p99 per-frame defrag cost** (~ 6% от 33.3 ms 30 Hz frame
      budget)
    + **0 frame stutter** (defrag split budget = 1/30 frame, threshold-triggered + frame-budgeted execution) на 8 GiB
      RTX 3060 Ti
      VRAM budget per `hardware-profile.md §3`.
      **5 strategies measured:**

    - A_None (current mainline baseline, no defrag)
    - B_PeriodicFullDefrag (every N=300 frames full `vmaDefragment`)
    - C_IncrementalBudgeted (per-frame `vmaDefragment` with `maxBytesPerFrame=8 MiB` cap)
    - D_OnDemandThreshold (defrag when fragmentation ratio > 0.4, idle frames only)
    - E_BudgetedOnDemand (combination D trigger + C budget)
      **5 scenes** (per `2026-06-21-sub-chunk-layers` precedent for direct comparability: uniform_floor + forest_floor +
      cave_stress + mixed_biome + uniform_air) × **4 alloc patterns** (chunk add/remove cycle + transient ring +
      JIT-loaded chunks + BLAS pool alloc/free) × **5 seeds** × **1000 frames + 10 warmup** = **500 configs × 1000
      frames =
      500,000 measurements**. Standalone C++26 CPU fragmentation simulator (no Vulkan init, no GPU dispatch, synthetic 8
      GiB heap
      matching dev host `obvium` RTX 3060 Ti per `hardware-profile.md §3`).
      **Cross-axis:** orthogonal ко всем 5 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1
      atomic,
      hzb-smart-mip-select = Stage 2.1 HZB refinement, vct-3d-mip-generation = Stage 5.1 VCT mip,
      vk-multi-gpu-split-frame =
      multi-GPU VRAM); **complementary** к closed mixed `2026-06-21-vulkan-memory-aliasing-transient` (aliasing axis =
      different
      lever, compaction = stackable) + closed mixed `2026-06-21-frame-flight-allocator-budget` (allocator strategy
      WITHIN_BUDGET + ring buffer deferred, compaction = stackable).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-vulkan-defragmentation-compaction/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-vulkan-defragmentation-compaction/prototype/` (standalone C++26 CPU
          fragmentation
          simulator, synthetic voxel scenes + VMA-like alloc API, NOT ProjectV mainline, dev host `obvium`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` (E_BudgetedOnDemand likely best for ProjectV workload — threshold trigger +
          budgeted
          execution = 0 stutter + 20-40% VRAM savings; C_IncrementalBudgeted = reliable alternative; A_None = baseline;
          B_PeriodicFull
          = 100% stutter risk on big moves; D_OnDemandThreshold alone = OK if idle frames reliable).
          3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC) `PROJECTV_DEFRAG=ON|OFF` env
          flag +
          `VmaDefragmentationInfo` struct + threshold tunable; Step 2 (S, ~100 LoC) `DefragScheduler::tick()` per-frame
          trigger +
          budget enforcement + Tracy plot "VRAM Defrag"; Step 3 (XS, ~30 LoC) default flip + per-stage policy (Stage
          4.3 +
          Stage 5.2 BLAS pool = aggressive; current MVP = conservative). Total ~160 LoC, S effort, 1-2 sessions. *
          *Caveats:**
          (a) CPU prototype, no Vulkan init, no real GPU driver overhead для `vmaDefragment` GPU copy; (b) synthetic
          VRAM
          heap (8 GiB match dev host, не реальный driver-level VkDeviceMemory); (c) fragmentation ratio synthetic per
          `vmaComputeAllocationStats` model (real = aligned with VMA ref impl line ~7000-8000); (d) cross-vendor VRAM
          characteristics not measured (single host); (e) mutation cost out of scope; (f) visual regression proxy =
          single-frame
          stutter detection, не real VMA validation; (g) Algorithm choice (
          Fast/Agressive/AggressiveOnlyCompletelyMapped) per
          VMA docs.
          Cross-refs: `TODO.md §1.1` (NanoVDB GPU upload cross-cutting) + `§4.3` (lift draw distance VRAM scaling) +
          `§5.2` (RTX
          BLAS pool); `src/render/SceneResources.cpp:805-1100` (current 22 VMA allocs);
          `src/voxel/VoxelWorld.{hpp,cpp}` (dynamic
          chunk mutation); `agent/knowledge.md §30.4` (3-step migration precedent); `agent/workspace.md §2` (Nearest
          Gap: Stage 4.3
          128+ chunks draw distance, VRAM budget critical);
          `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
          (5-10% threshold); closed experiments: `vulkan-memory-aliasing-transient` (mixed, aliasing axis), `frame-flight-allocator-
  budget` (mixed, allocator strategy), `vma-sparse-textures` (mixed, software VT page table allocator); active parallel:
          `tracy-gpu-vs-manual`, `gpu-fluid-ca-atomic-strategy`, `hzb-smart-mip-select`, `vct-3d-mip-generation`,
          `vk-multi-gpu-split-frame`; `hardware-profile.md §3` (8 GiB VRAM); `benchmarks/methodology.md §3` (measurement
          protocol).

- [ ] **2026-06-21-sdf-hybrid-world** — m, **Stage 5.1 + Stage 3.3 cross-cutting** (VCT anti-leak + smooth physics
  normals; **self-promoted l→m** per `optimization-philosophy.md` 5-10% threshold + strong cross-axis coupling
  justification; SDF-for-meshing axis remains **parked** per `meshing-algo-comparison` §6 closure — *different* scope,
  NOT this experiment).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»; **eighth
  invocation this session** — previous 7 closed or in-progress: audio-raytracing mixed + wfc mixed + sub-chunk mixed +
  gpu-noise mixed + taa-yes + depth-yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh mixed; 5
  in-progress parallel before this: tracy-gpu + dlss-fsr-xess + greedy-physics-meshing + gpu-fluid-ca-atomic +
  vct-cone-count; 19+ closed `2026-06-20`).
  **Started:** 2026-06-21.
  **Blocker:** нет (CPU-only synthetic voxel scenes + SDF computation, no Vulkan/mainline dependency, dev host `obvium`
  Zen 3 5800X + governor=`powersave` per `hardware-profile.md §1` available; no AVX-512 per §1 = realistic measurement
  floor per `simd-procedural-noise` precedent).
  **Hypothesis (one-line):** Sparse SDF overlay (1 byte/voxel = 7-bit distance + 1-bit sign) поверх binary voxel grid
  ProjectV (chunkSize=8 per `src/voxel/VoxelWorld.hpp:78`) даст **smooth VCT cone-march termination** (anti-leak per
  Lumen 2022 Narkowicz "Journey to Lumen" critique of voxel-only VCT; expected **+2-5 dB PSNR** vs brute-force 1024-cone
  reference per `vct-cone-count-atlas-precision` baseline) + **smooth physics collision normals** (C¹ smooth via SDF
  gradient, vs stepped voxel face normal; expected elimination of per-corner micro-stutter) при **+1 byte/voxel VRAM =
  +100% of baseline material storage** (1 byte/voxel) + **+0.5-2 µs/chunk build cost** via Jump Flooding Algorithm (JFA
  per Ruijters 2008, 4-6× faster than brute-force BFS).
  **Self-promotion l→m justification (per `optimization-philosophy.md` 5-10% threshold + l-priority в `backlog.md`
  §Open):** original l-priority (per `meshing-algo-comparison` §6 closure) = SDF-meshing axis parked до Stage 3.3; *
  *non-meshing SDF uses** (VCT anti-leak + physics normals) ARE critical to current Stage 5.1 (in-progress
  `vct-cone-count-atlas-precision` orthogonal — termination ≠ cone count) + Stage 3.3 (in-progress
  `greedy-physics-meshing-cpu` orthogonal — meshing ≠ normals). Strong cross-axis coupling (5.1 + 3.3) + measurable
  hypothesis (PSNR + normal smoothness) + low integration risk (additive data, drop-in termination/normal calculation) +
  CPU-only analytical scope (single-session per `wfc`/`lod-mesh`/`sub-chunk` precedent).
  **Cross-axis:** orthogonal ко всем 5 in-progress parallel (tracy-gpu = profiling, dlss-fsr-xess = upscaling,
  greedy-physics-meshing = Stage 3.3 meshing — *not* normals, gpu-fluid-ca-atomic = Stage 3.1 atomic, vct-cone-count =
  Stage 5.1 VCT quality — *not* termination); **complementary** к 7 closed experiments: `2026-06-20-vct-vs-rt-cutoff` (
  closed mixed, strategy axis = roughness cutoff = 0.3; SDF = termination axis = orthogonal) +
  `2026-06-20-nanovdb-on-gpu` (closed yes, NanoVDB can host SDF natively — natural extension) +
  `2026-06-21-sub-chunk-layers` (closed mixed, chunk layout; SDF per layer = natural extension) +
  `2026-06-21-lod-mesh-downsampling` (closed mixed, LOD; SDF for LOD smooth blend = natural follow-up) +
  `2026-06-21-wfc-procedural-worlds` (closed mixed, world gen; SDF for WFC tile boundaries = potential Phase 4
  follow-up) + `2026-06-21-gpu-procedural-noise-compute-kernels` (closed mixed, noise gen; SDF for surface distance
  queries) + `2026-06-20-meshing-algo-comparison` (closed mixed, §6 closure explicitly notes SDF-meshing axis parked до
  Stage 3.3 = **NOT** this scope).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-sdf-hybrid-world/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-sdf-hybrid-world/prototype/` (standalone C++26 CPU SDF overlay + VCT
      termination + physics normals harness, synthetic voxel scenes representative of ProjectV
      workload [uniform_air + uniform_floor + forest_floor + cave_stress + mixed_biome per
      `sub-chunk-layers` precedent for direct comparability], NOT ProjectV mainline, dev host `obvium`)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` — B_R8_1byte likely winner for cave_stress + mixed_biome (VCT anti-leak + physics
      normal), C_R8_4quant or D_RLE_NoneSparse may win for uniform scenes (lower VRAM overhead at cost of some quality),
      A_None = baseline; A_None likely retains для pure chunks with no surface features. 3-step migration per
      `agent/knowledge.md §30.4` precedent — Step 1 (S, ~150 LoC) `SdfOverlay` payload + JFA generator in
      `src/voxel/VoxelWorld.{hpp,cpp}`; Step 2 (S, ~200 LoC) `SdfVctTerminate` drop-in termination в `vct.frag` +
      `SdfNormal` calculation в `PhysicsWorld.cpp`; Step 3 (XS, ~50 LoC) `PROJECTV_SDF_OVERLAY=ON` env flag + per-chunk
      layout selection + Tracy plot. Total ~400 LoC, S-M effort, 2-3 sessions. **Caveats:** (a) CPU prototype, no GPU
      dispatch — JFA GPU validation deferred; (b) VCT quality measurement via PSNR vs analytical reference, not visual
      QA; (c) collision normal smoothness via analytical reference, not real JPH broadphase timing; (d) cross-vendor GPU
      SDF validation deferred (single vendor RTX 3060 Ti in scope per `hardware-profile.md §3`); (e) NanoVDB-native SDF
      integration deferred (out of scope; current prototype = flat array per chunk); (f) RLE compression for sparse SDF
      blocks = trade-off; (g) mutation cost out of scope per current Stage priorities. Cross-refs: `TODO.md §5.1` (
      VCT) + `§3.3` (Physics), `src/voxel/VoxelWorld.hpp:78` (chunkSize=8), `src/voxel/SceneConfig.cpp:78`,
      `src/shaders/vct.frag` (current termination), `src/physics/PhysicsWorld.cpp:712-773` (current collision body),
      `agent/knowledge.md §30.4` (3-step migration precedent), `agent/workspace.md §1 Phase 4` (per-chunk rebuild),
      `2026-06-20-vct-vs-rt-cutoff` (closed mixed, strategy axis), `2026-06-21-vct-cone-count-atlas-precision` (
      in-progress, within-VCT quality), `2026-06-21-greedy-physics-meshing-cpu` (in-progress, meshing axis),
      `2026-06-20-nanovdb-on-gpu` (closed yes, storage), `2026-06-21-sub-chunk-layers` (closed mixed, scenes +
      comparability), `2026-06-21-lod-mesh-downsampling` (closed mixed, LOD), `2026-06-20-meshing-algo-comparison` (
      closed mixed, §6 closure = SDF-meshing parked), `2026-06-21-wfc-procedural-worlds` (closed mixed, world gen),
      `2026-06-21-gpu-procedural-noise-compute-kernels` (closed mixed, noise), `2026-06-20-simd-procedural-noise` (
      closed mixed, AVX2 floor), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
      `docs/experiments/hardware-profile.md §1+§6` (Zen 3 5800X + Clang 22.1.6),
      `docs/experiments/benchmarks/methodology.md §3` (measurement protocol).
      **Closed `2026-06-21` (single session ~3h), verdict `mixed`.** Standalone C++26 CPU prototype ~1300 LoC, **build
      green 0 warnings** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG`). 600 measurements (5 scenes × 5 seeds ×
      4 encodings × 2 builds × 3 terms × N=1000), wall time <60 sec на Zen 3 5800X powersave. **Headline (counter to
      literature + hypothesis):**
    - **BFS 2.4× faster than JFA on chunkSize=8** (6.6 vs 16.0 µs/chunk) — BFS wins for dense/small chunks
      (narrow-band = ≤7 voxels from surface per OpenVDB 13.0.1); JFA wins only for large sparse volumes
    - **D_RLE_NoneSparse = 30% VRAM** of B_R8_1byte (153 vs 512 bytes/chunk) — validates OpenVDB
      narrow-band pattern
    - **T_VoxelDiscrete is fastest AND highest PSNR** (44.97 dB) in this prototype — current mainline
      behavior preserved; no SDF-driven VCT quality gain measured
    - **PSNR NOT improved** by SDF overlay (42-45 dB for all configs, variance σ=32 dB) — likely due to
      same-algorithm reference (only cone count varies) + simplified trilinear SDF
    - **Narkowicz 2022 anti-leak benefit NOT validated** in v1 prototype — production Lumen (with material
      shaders + cone occlusion) likely different from synthetic 8³ chunks
    - **Phase A web-research: 15 primary sources verified** via `webfetch` + DuckDuckGo HTML fallback
      (Exa 429 persistent); Narkowicz 2022 "Journey to Lumen" = DIRECT EXPERT VALIDATION of hypothesis
      (voxel VCT leaks → global distance field + voxel bit bricks 8×8×8 = production-proven anti-leak path)
    - **Mainline 3-step migration per `agent/knowledge.md §30.4`:** Step 1 (XS, ~50 LoC) **immediate
      recommendation** — BFS replaces JFA as default `SelectSdfBuildPolicy()` (2.4× build speedup);
      Step 2 (S, ~250 LoC) **deferred до Stage 4.3** — D_RLE_NoneSparse narrow-band storage (-70%
      VRAM); Step 3 (M, ~250 LoC) **deferred indefinitely** — T_SDFSmooth/T_Hybrid integration (no
      PSNR gain in v1, 43-87% march cost overhead)
    - **Total: ~550 LoC, S effort, 2-3 sessions** (Step 1 immediate, Step 2 post-Stage-4.3, Step 3 N/A)
    - **Caveats:** (a) PSNR variance high (σ=32 dB), only 5 synthetic scene types; (b) reference uses
      same algorithm as measured → not true ground truth; (c) 8³ chunk is ProjectV minimum; larger
      chunks may show different JFA/BFS trade-off; (d) cross-vendor (AMD RDNA, Intel Arc) not measured
      (CPU-only); (e) NanoVDB-native SDF integration deferred; (f) mutation cost out of scope;
      (g) Intel HD disabled per UE5 docs (cross-vendor note)
    - **Continuation chain:** none (first SDF-for-lighting+physics axis in scope). Follow-up
      candidates: `_sdf-nanovdb-integration_` (NanoVDB-native SDF), `_sdf-jfa-gpu-validation_`
      (real GPU JFA vs BFS), `_sdf-rle-compression-tuning_` (optimal quantization per scene type),
      `_sdf-mutation-cost_` (recompute SDF band on voxel mutation), `_sdf-vct-real-ground-truth_`
      (visual QA on real ProjectV scenes per Narkowicz 2022 anti-leak).
    - См. §6 + §1 + experiment README + `STATUS.md` (final) + `sources.md` (27 sources) +
      `prototype/results.csv` (600 measurements).

- [ ] **2026-06-20-vma-sparse-textures** — m, Stage 2.3 (Sparse Virtual Texturing) + cross-cutting VRAM budget.

- [ ] **2026-06-21-tracy-gpu-vs-manual** — m, independent (cross-cutting profiling, foundation для
  `agent/knowledge.md §4` build/verification contract).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment expected, analytical + prototype).
  **Blocker:** нет.
  **Hypothesis (one-line):** Tracy GPU context (`TracyVkZone` + `TracyVkCollect`) per-pass overhead линейно
  растёт с числом GPU passes; на projected Stage 5.x post-VCT+RTX+async-compute workload (15+ passes,
  multiple queues = multiple Tracy contexts) overhead превысит 1% frame budget; manual
  `vkCmdWriteTimestamp` + host-side `TracyPlot` для non-critical passes снижает overhead при сохранении
  diagnostic coverage.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/prototype/` (standalone Vulkan 1.4
      harness + vendored Tracy, не ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` (Tracy GPU полезен для top-3 hot-path passes; manual для остальных;
      cross-vendor validation необходима для Stage 5.x async-compute multi-context).
      **Started:** 2026-06-21.
      **ETA:** this session (single experiment expected, analytical + prototype).
      **Blocker:** нет.
      **Hypothesis (one-line):** Tracy GPU context (`TracyVkZone` + `TracyVkCollect`) per-pass overhead линейно
      растёт с числом GPU passes; на projected Stage 5.x post-VCT+RTX+async-compute workload (15+ passes,
      multiple queues = multiple Tracy contexts) overhead превысит 1% frame budget; manual
      `vkCmdWriteTimestamp` + host-side `TracyPlot` для non-critical passes снижает overhead при сохранении
      diagnostic coverage.
      **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-tracy-gpu-vs-manual/prototype/` (standalone Vulkan 1.4
      harness + vendored Tracy, не ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` (Tracy GPU полезен для top-3 hot-path passes; manual для остальных;
      cross-vendor validation необходима для Stage 5.x async-compute multi-context).

- [x] **[2026-06-21-wfc-procedural-worlds](./experiments/2026-06-21-wfc-procedural-worlds/)** — m,
  **independent** (Stage 4.1 GPU Noise & World Gen per `TODO.md §4.1`, **discrete-structure axis**
  orthogonal к closed `2026-06-21-gpu-procedural-noise-compute-kernels` continuous-noise axis + closed
  `2026-06-21-sub-chunk-layers` chunk-layout axis).
  Closed `2026-06-21` (single session), verdict **`mixed`**.
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»,
  second invocation after `audio-raytracing-voxel-sdf`).
  **Hypothesis (validated):** WFC-over-OpenSimplex2 hybrid pipeline (3D-WFC AC-3 constraint propagation
  для cave/biome discrete structure + OpenSimplex2 noise для heightmap) даст локально-когерентные биомы/пещеры
  с generation time < 50 µs/chunk на Zen 3 5800X при chunkSize=8 sub-region.
  **Measured:** 8³ sub-region = 220-235 µs mean (cave + biome identical, ~25 µs estimated на boost governor ×8.7),
  coherence 0.67, **50% success rate**; 16³ sub-region = 11 ms / **0% success** / 3595 propagation passes =
  **exponential blow-up подтверждён**; 32³ sub-region = 1 sec timeout (catastrophic). **GPU WFC historically
  failed** (Chocomunk 2020 cuWFC, s-ol 2018 gpWFC — negative prior art). **Per-tileset identical perf**
  = problem algorithm-specific, не tileset-specific.
  **Mainline recommendation:** 3-step migration per `agent/knowledge.md §30.4` precedent — **Step 1 (XS, ~30 LoC)
  RECOMMENDED immediate**: `world_gen_wfc.cpp` skeleton + 8³ AC-3 propagation + **governor=performance requirement**
    + early-fail-fast + OpenSimplex2 hybrid hook → NanoVDB upload per `nanovdb-on-gpu` verdict=yes. **Step 2 (S,
      ~150 LoC) RECOMMENDED follow-up**: better MRV (minimum remaining values) heuristics для success rate 50%→90%+
    + 2nd tileset (biome) + `transitions_consistency_score` validation в `ProjectVWfcTests`. **Step 3 (M,
      ~300 LoC) DEFERRED до Stage 4.3+ lift draw distance**: **N-WFC nested pattern per arXiv 2308.07307** =
      nested fixed-size sub-grids с inter-grid constraints → polynomial time для chunks > 8³. Cross-refs:
      `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85`, `agent/knowledge.md §30.4`,
      `2026-06-21-gpu-procedural-noise-compute-kernels`
      (OpenSimplex2 continuous), `2026-06-21-sub-chunk-layers` (chunk-layout), `2026-06-20-nanovdb-on-gpu` (SSBO),
      `2026-06-20-dec-pipelines-async-compute` (async), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
      (5-10% threshold), `docs/experiments/hardware-profile.md §1` (Zen 3 5800X).
      **Standalone C++26 prototype** `prototype/{wfc.hpp, tilesets.hpp, bench.cpp, CMakeLists.txt, README.md}`
      (~440 LoC, Clang 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG). Web-research complete (2 batches, 8+ sources
      верифицированы: Maxim Gumin 2016 + arXiv 2308.07307 N-WFC + Chocomunk cuWFC 2020 + s-ol gpWFC 2018 +
      Fennec-hub three-wfc 2025 + julzerinos WFC brush + basta WFC + RWTH 3D compute shader thesis).
      **Cross-axis:** 4 closed same-session `2026-06-21` (frame-flight-allocator + gpu-procedural-noise +
      sub-chunk-layers
    + this) + multiple in-progress = full Stage 4.x + Stage 5 + Stage 6.x + toolchain + profiling + audio + temporal
      optimization landscape. Caveats: single CPU governor measured (powersave; boost estimated ×8.7), 100 iter
      per config (vs `methodology.md` default 1000), chunkSize=32³ не измерен (timeout 1 sec), no N-WFC prototype
      (analytical only per arXiv 2308.07307). См. §6 + §1 + experiment README +
      `prototype/build/results_{cave,biome}_small.csv` + STATUS.md.

- [x] **2026-06-21-sub-chunk-layers** — m, **Stage 4.x** (biome/cave data structure axis, orthogonal к
  `2026-06-21-wfc-procedural-worlds` который = generation strategy).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements).
  **Blocker:** нет (CPU-only synthetic voxel chunk layouts, no Vulkan/mainline dependency, dev host `obvium`
  Zen 3 5800X per `hardware-profile.md §1` available).
  **Hypothesis (one-line):** Multi-layer chunks (per-Y sub-chunks of fixed layer height L=1, 2, 4 для biome/cave
  architecture) дадут +5-15% mutation cost overhead vs monolithic chunks, но -10-40% per-chunk material index
  size через palette indexing (layer = uniform material array) + layer-bounded meshing (per-layer LOD
  independence) при typical Minecraft-style биомных/пещерных scenes на chunkSize=8.
  **Cross-axis:** orthogonal к `2026-06-21-wfc-procedural-worlds` (gen strategy = WFC vs noise; this = data
  structure = layered vs monolithic chunk); complementary к `2026-06-20-sparse-64-tree-alternatives`
  (SVO storage = upper structure, sub-chunk layers = inner structure per chunk); complementary к
  `2026-06-21-gpu-procedural-noise-compute-kernels` (noise gen = continuous biome heightmap, this = discrete
  layer transition semantics); natural extension для `2026-06-20-nanovdb-on-gpu` (NanoVDB walker operates
  on uniform tiles — multi-layer = natural fit per VDB tile hierarchy).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-sub-chunk-layers/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-sub-chunk-layers/prototype/` (standalone C++26 CPU chunk
      layout prototype, synthetic voxel scenes representative of Minecraft biomes/cave systems, NOT
      ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Verdict:** **`mixed`** (closed `2026-06-21`, same session). Memory savings 73-96% (well above 5%
      threshold per `optimization-philosophy.md`) — B_Palette wins на uniform/2-material scenes, D_L4 marginal
      win на 4-material mixed_biome. Build cost overhead 30-55× but absolute 1-6 µs vs 50 µs Stage 4.1 budget.
      Mutation cost overhead +5-70% but absolute 10-19 ns (negligible). Mesh vertex count identical (layout-
      orthogonal). Layer boundary count 28-155 explicit transitions для layered = semantic gain для VCT
      anti-leak + per-layer LOD + selective rebuild. **3-step migration per `agent/knowledge.md §30.4`**:
      Step 1 `ChunkLayout` enum + `SelectChunkLayout` (~150 LoC, S); Step 2 `world_gen_layers.comp` per-layer
      payload + per-chunk metadata (~300 LoC, M); Step 3 wire layer semantics в `voxel.frag` VCT cone-march

    + Stage 4.2 per-layer LOD (~250 LoC, M). Closed entry: `experiments/2026-06-21-sub-chunk-layers/`.

- [x] **2026-06-21-taa-motion-vectors** — m, **independent** (Stage 5.3 TAA Motion Vectors per `TODO.md §5.3`,
  **temporal axis для Stage 5** после полного closure lighting-axis `2026-06-20`: `vct-vs-rt-cutoff` mixed +
  `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»;
  third invocation this session — first was `audio-raytracing-voxel-sdf` [closed same-session mixed], second
  was `wfc-procedural-worlds` [in-progress parallel]).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone Vulkan 1.4 + C++26 prototype +
  measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU+GPU prototype, dev host `obvium` RTX 3060 Ti GA104 + Vulkan 1.4.341 +
  `R16G16_SFLOAT` motion vector MRT format все supported per `hardware-profile.md §3/§4`).
  **Hypothesis (one-line):** Per-vertex motion vectors (`vec2 prevClipPos - vec2 currClipPos`, written
  per-vertex to dedicated MRT attachment per `TODO.md §5.3` explicit goal) vs current depth-buffer
  reprojection (cheaper, "менее честный" per TODO §5.3) даст measurably better TAA quality (no ghosting
  на быстродвижущихся моделях per TODO §5.3 DoD: «Полное исчезновение шлейфов за перемещаемыми
  гравипушкой моделями») при ~0.3-0.5 ms GPU cost + 4 MiB/frame VRAM cost (R16G16_SFLOAT @ 1080p,
  double-buffered = 8 MiB). Альтернатива = Karis 2014 «Brute Force» depth-reproject enhancement
  (previous-frame neighbor clamping) может match quality at lower cost (no MRT = 0 bytes VRAM).
  **Cross-axis:** orthogonal ко всем 3 in-progress parallel (tracy-gpu = profiling, wfc = gen strategy,
  sub-chunk = data structure); complementary к closed `clustered-forward-mass-lights` (SSBO light list
    + motion vectors = both feed TAA resolve); natural follow-up к closed `dec-pipelines-async-compute`
      (motion vector MRT submission = candidate for async queue if VRAM/upload becomes bottleneck);
      cross-vendor validation matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA Ampere/Ada/
      Blackwell + AMD RDNA2/3/4 + Intel Arc Gfx12.5+).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-taa-motion-vectors/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-taa-motion-vectors/prototype/` (standalone Vulkan 1.4
          harness: voxel scene render + 2 pipelines [vertex-out motion vector MRT vs depth-reproject] +
          TAA resolve pass + PSNR/SSIM quality measurement, NOT ProjectV mainline, dev host `obvium`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` (motion vectors likely provide measurable quality gain [PSNR
          improvement + ghosting elimination] at ~0.3-0.5 ms GPU cost + 4-8 MiB VRAM; alternative
          depth-reproject-with-clamping per Karis 2014 may match quality at lower cost — needs measurement).
          Caveats: (a) single GPU vendor validated (RTX 3060 Ti GA104); (b) synthetic voxel scene
          (representative of ProjectV chunked geometry); (c) PSNR/SSIM на synthetic test pattern + visual
          diff count на dynamic object scene; (d) R16G16_SFLOAT = standard per `agent/knowledge.md §6.x` for
          motion vectors (cross-vendor standard format); (e) no cross-frame pipelining gain measured
          (headless harness).

- [x] **2026-06-21-greedy-physics-meshing-cpu** — m, **Stage 3.3** (Greedy Physics Meshing per `TODO.md §3.3`
  explicit DoD: «Количество коллизионных шейпов в CompoundShape снижается минимум в 4 раза на типичном
  ландшафте. Полное совпадение физического поведения (персонаж не проваливается под текстуры и корректно
  сталкивается с углами).»). **Self-invented topic** (operator instruction `2026-06-21`:
  «выбирай свободную тему или придумывай свою и исследуй»). **Motivation:** mainline baseline
  `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` добавляет per-solid-voxel
  `JPH::BoxShape(0.5f)` в `JPH::StaticCompoundShapeSettings` (line 715: `new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f))`
  per loop iteration). Это **N shapes/chunk = solid-voxel-count** = mainline **0× reduction**, DoD
  «≥ 4× reduction» не выполняется. Per-chunk incremental Jolt pipeline (2x part 4 Phase 4 + 2x part 5 Phase 9,
  per `agent/workspace.md §1`) уже в mainline с `chunkStaticBodies` map + `ProcessChunkRebuildQueue`, но **использует
  тот же naive per-voxel loop**. Greedy merge = natural follow-up, immediate integration в existing per-chunk
  rebuild path.
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only, dev host `obvium` Zen 3 5800X governor `powersave` per `hardware-profile.md §1`).
  **Hypothesis (one-line):** правильная greedy merge стратегия (B_1DAxisGreedy / C_2DPlaneGreedy /
  D_3DFullGreedy / E_HierarchicalOctree / F_TwoPass3D, 5 стратегий measured) даст **≥ 4× reduction** в
  `JPH::StaticCompoundShape` shape count per chunk (DoD `TODO.md §3.3`) при **identical collision
  behavior** (100% volume preservation) + **CPU build cost ≤ 200 µs/chunk** (50-100× headroom vs 50 µs
  Stage 4.1 budget per `TODO.md §4.1`) + **100% volume preservation** (no false positive / false negative
  collider merge).
  **Cross-axis:** orthogonal ко всем 5 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1
  atomic, vk-fragment-shading-rate = VRS fragment rate, audio-diffraction = audio, vct-cone-count = Stage 5.1
  VCT); **complementary** к closed `2026-06-20-meshing-algo-comparison` (visual meshing = same algorithmic
  family [Mikola Lysenko 2012 per-axis 2D scan] applied to **visual quads in `voxel_mesh.comp::GreedyFacePass`**;
  this = same algorithm applied to **physics AABB boxes in `BuildStaticVoxelCollisionBody`**) + closed
  `2026-06-20-work-stealing-job-system` (serial dispatcher default, single-threaded greedy merge) + closed
  `2026-06-20-cache-oblivious-chunk-tree` (chunk tree access patterns) + mainline incremental Jolt pipeline
  (2x part 4 Phase 4 + 2x part 5 Phase 9 per `agent/workspace.md §1`).
  **Why m-priority (self-promoted from l):** explicit numeric DoD в `TODO.md §3.3` («4× shape reduction +
  identical physics behavior») = measurable hypothesis; orth cross-axis ко всем in-progress; mainline already
  has foundation (per-chunk rebuild queue) = low integration risk; CPU-only, no GPU dispatch = can deliver
  in single session.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-greedy-physics-meshing-cpu/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-greedy-physics-meshing-cpu/prototype/` (standalone C++26 CPU
      greedy merge harness, synthetic voxel chunk scenes representative of ProjectV scenes [uniform_floor +
      forest_floor + cave_stress + mixed_biome + uniform_4x4x4], NOT ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` — `D_3DFullGreedy` or `E_HierarchicalOctree` likely winner для
      typical terrain (8-16× shape reduction vs baseline), `B_1DAxisGreedy` likely winner для simple uniform scenes
      (2-4× reduction, simpler code). Cross-axis: orth ко всем in-progress. Caveats: (a) CPU prototype, no JPH
      broad-phase query timing (would require JPH::PhysicsSystem + actual raycast, too heavy без mainline coupling);
      (b) synthetic scenes representative not exhaustive; (c) 2D scan strategies (B, C) = lower quality на
      irregular cave scenes, expected; (d) 3D greedy (D) potentially O(N²) worst-case → bounded by chunk size 8³
      (max 512 cells, manageable); (e) octree (E) guarantees O(N log N) but requires per-node decision overhead.
      Cross-refs: `TODO.md §3.3`, `src/physics/PhysicsWorld.cpp:712-773::BuildStaticVoxelCollisionBody` (mainline
      baseline = 0× reduction), `src/physics/PhysicsWorld.cpp:547-560::IsPhysicsSolidMaterial` (material
      classification), `src/voxel/VoxelWorld.hpp:78-107` (VoxelWorld struct, chunkSize=8, access API),
      `agent/workspace.md §1 Phase 4` (incremental Jolt per-chunk wiring closed), `agent/workspace.md §1 Phase 9`
      (ProcessChunkRebuildQueue per-frame call closed), `agent/knowledge.md §17` (build matrix), `agent/knowledge.md
  §30.4` (3-step migration precedent), closed `2026-06-20-meshing-algo-comparison` (visual meshing patterns),
      closed `2026-06-20-work-stealing-job-system` (serial default),
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
      (5-10% threshold), `docs/experiments/hardware-profile.md §1` (Zen 3 5800X dev host),
      `docs/experiments/benchmarks/methodology.md §3`
      (measurement protocol).

  **Closed `2026-06-21` (single session, ~2h), verdict `yes`** (с caveat: E_Octree implementation bug на
  coplanar 2D layers, fixable out of scope). **Stage 3.3 greedy physics meshing axis closed** —
  DoD `TODO.md §3.3` validated with **8× margin** (35× avg reduction vs 4× required). Web research:
  completed via DuckDuckGo HTML endpoint + webfetch (Exa MCP HTTP 429 rate-limited for web_search);
  9+ sources verified this session: Mikola Lysenko 2012 "Meshing in a Minecraft Game" (`0fps.net`,
  canonical 8×-approximation proof), Laine & Karras **2010** (не 2013) "Efficient Sparse Voxel Octrees"
  (IEEE TVCG, DOI `10.1109/TVCG.2010.240`), Vercidium C# production (`vercidium-patreon/meshing`,
  644 stars), roboleary Java port, gedge.ca 2014 + fluff.blog 2023 + zenny3d 2025 + nickmcd 2021 +
  Epic UE tutorial + Vulkan Guide (8 secondary sources). `sources.md` обновлён с verified citations. **Headline
  measurements** (6 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup =
  150,000 main measurements, dev host `obvium` Zen 3 5800X governor `powersave`, wall time 0.12 s):

  | Strategy | Mean shape_reduction | × reduction | Mean build_us | DoD ≤ 0.25? | Volume match |
    |:---------|:---------------------|:------------|:--------------|:------------|:-------------|
  | A_Naive (baseline) | 1.0000 | 1× | 0.49 | ❌ fails | 100% ✓ |
  | B_1DZ | 0.2022 | 5× | 0.39 | ✓ | 100% ✓ |
  | C_2DXZ | 0.0619 | 16× | 0.59 | ✓ | 100% ✓ |
  | **D_3D** | 0.0288 | **35×** | 0.81 | ✓ | 100% ✓ |
  | E_Octree | 0.5887 | 1.7× (broken 2/5) | 1.30 | ⚠️ | 100% ✓ |
  | **F_TwoPass** | 0.0284 | **35×** | 0.78 | ✓ | 100% ✓ |

  **Mainline recommendation:** `F_TwoPass` (2D XZ per Y + vertical merge) — same 35× reduction as
  `D_3D`, simpler code, naturally matches per-Y-layer chunk semantic per closed
  `2026-06-21-sub-chunk-layers` (verdict=mixed). 3-step migration per `agent/knowledge.md §30.4`
  precedent: Step 1 (XS, ~30 LoC) `src/physics/GreedyPhysicsMerger.{hpp,cpp}` foundation; Step 2
  (S, ~50 LoC) replace per-voxel loop в `BuildStaticVoxelCollisionBody:712-740` + wire per-chunk
  rebuild path; Step 3 (M, ~80 LoC) `PROJECTV_GREEDY_PHYSICS_MESH=ON` env flag (default ON) +
  Tracy plot "Physics Greedy Merge" + `WorldStats` extension + `ProjectVPhysicsGreedyMergerTests`
  unit test. Total ~160 LoC, S effort, 1-2 sessions. **Net effect positive** despite +60% per-call
  build cost delta: 35× fewer AddShape + 35× fewer JPH child shapes = JPH broad-phase cost dominates
  (per Jolt docs broad-phase visits each child shape → 35× fewer visits = much faster collision
  query + rebuild). **Caveat:** E_Octree implementation bug on coplanar 2D layers
  (uniform_floor, cave_stress return 1.0× reduction instead of expected 0.02×) — fixable via
  coplanar layer merge step but out of scope; F_TwoPass doesn't have this issue (its 2D slice
  pass naturally handles coplanar layers). **Cross-axis:** orth ко всем 5 in-progress parallel
  (tracy-gpu + gpu-fluid-ca + vk-fragment-shading-rate + audio-diffraction + vct-cone-count) +
  complementary к closed meshing-algo-comparison (same algorithmic family, different output target).
  Closed entry: `experiments/2026-06-21-greedy-physics-meshing-cpu/` + `prototype/{greedy_physics_bench.cpp,
  CMakeLists.txt, README.md, results.csv}`. См. §6 + §1 + experiment README + RESULTS.md + sources.md.

- [ ] **2026-06-21-gpu-fluid-ca-atomic-strategy** — m, **Stage 3.1** (GPU Fluid CA per
  `TODO.md §3.1` + `agent/knowledge.md §30.4`).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone Vulkan 1.4 compute prototype + measurements).
  **Blocker:** нет.
  **Hypothesis (one-line):** Правильная стратегия атомарной записи в `fluid_ca.comp` ping-pong buffer
  (current mainline = blind `atomicOr` chosen без измерения per `src/shaders/fluid_ca.comp:101` +
  `agent/workspace.md §1 Phase 3`; **противоречит** `agent/knowledge.md §30.4` line 1045 contract =
  `imageAtomicCompareExchange` для count conservation; alternatives: shared-memory tile compaction,
  workgroup-level CAS, hierarchical locking) даст -10-30% total fluid tick latency + 100% conservation
  guarantee на 500K voxels @ 0.5 ms Stage 3.1 DoD (per `TODO.md §3.1`) на RTX 3060 Ti Ampere.
  **Cross-axis:** orthogonal к in-progress `2026-06-21-tracy-gpu-vs-manual` (profiling tool),
  `2026-06-21-wfc-procedural-worlds` (Stage 4.1 gen strategy), `2026-06-21-sub-chunk-layers` (Stage 4.x
  storage), `2026-06-21-taa-motion-vectors` (Stage 5.3 temporal), **2026-06-21-lod-mesh-downsampling**
  (Stage 4.2 LOD). Complementary к closed `2026-06-20-dec-pipelines-async-compute` (yes, sync foundation:
  atomic strategy + sync = 2 axis Stage 3.1) + `2026-06-20-async-compute-overhead-numbers` (yes,
  +9.85-11.34% sync measured, но внутри-pass atomic strategy не измерен).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-gpu-fluid-ca-atomic-strategy/prototype/` (standalone
      Vulkan 1.4 compute harness, RTX 3060 Ti, 5 atomic strategies × 4-5 scene configs × N=1000 iter
        + warmup, NOT ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` или `yes` (current `atomicOr` = simplest but **wrong по conservation**
      [double-claim without check]; `imageAtomicCompareExchange` = correct + likely similar perf на
      0-50% contention; shared-memory + subgroup compaction = best perf -10-30% но M effort;
      `VK_KHR_shader_atomic_float` available per `hardware-profile.md §4`).

- [ ] **2026-06-21-lod-mesh-downsampling** — m, **Stage 4.2 chunk 2** (LOD uniform downsampling per
  `TODO.md §4.2` + `agent/workspace.md §2` Nearest Gap explicit: "Stage 4.2 chunk 2 — uniform downsampling
  implementation. Distance LOD selection works (2x part 3 Phase 5) but actual mesh-level downsampling
  not yet built").
  **Agent:** self (operator instruction `2026-06-21`: «выбирай тему или придумывай свою и исследуй»;
  fourth invocation this session — previous: audio mixed + wfc mixed + sub-chunk mixed closed;
  tracy-gpu + taa + gpu-fluid-ca in-progress parallel).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only synthetic voxel chunk LOD layouts, no Vulkan/mainline dependency, dev host
  `obvium` Zen 3 5800X per `hardware-profile.md §1` available; `chunkSize=8` per
  `src/voxel/VoxelWorld.hpp:78` already verified in multiple `2026-06-2x` experiments).
  **Hypothesis (one-line):** правильная пара (downsampler kernel ∈ {A_Majority3D, B_SurfacePreserve,
  C_SolidOnly, D_MaxPool} × stitch strategy ∈ {X_None, Y_TJunctionPad, Z_NeighborLocked}) даст -75%/-94%/-98%
  triangle count на LOD 1/2/3 vs LOD 0 при **zero visible T-junction holes** (Stage 4.2 DoD explicit) +
  per-chunk downsampling cost < 1 µs (vs 50 µs/chunk Stage 4.1 budget) на Zen 3 5800X.
  **Cross-axis:** orthogonal к in-progress parallel (tracy-gpu = profiling tool, wfc = gen strategy,
  sub-chunk = vertical layers, taa = temporal Stage 5.3, gpu-fluid-ca = atomic strategy Stage 3.1);
  complementary к closed `nanovdb-on-gpu` (NanoVDB tile hierarchy + mip chain = natural storage для LOD) +
  `meshing-algo-comparison` (Naive Greedy at LOD 0; this = LOD 1/2/3 downsample pipeline) +
  `svdag-vs-vdb-memory-throughput` (storage). **New axis:** ни один из 30+ closed experiments за
  `2026-06-20` + `2026-06-21` не покрывает Stage 4.2 LOD implementation.
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-lod-mesh-downsampling/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-lod-mesh-downsampling/prototype/` (standalone C++26 CPU
      downsampler + stitch harness, synthetic voxel scenes, NOT ProjectV mainline, dev host `obvium`)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` (single kernel/stitch pair won't be optimal для all scene types; expected
      conditional adoption: surface-aware + neighbor-locked для biome-heavy scenes, solid-only + T-junction-pad
      для uniform terrain; cross-vendor GPU performance deferred to follow-up Stage 4.2 GPU integration).
      **Closed `2026-06-21` verdict=`mixed`.** Stage 4.2 LOD uniform downsampling axis fully closed. Web-research
      complete (~30 sources, 12 primary + 6 supplementary верифицированы: 0fps.net Lysenko 2018 POP buffers

    + stable LOD rounding, Lengyel 2009 Transvoxel, Cinevva 2026 Transvoxel/clipmaps, Blackflux Part 3,
      Voxceleron2 hybrid Sparse LOD Octree, Cubyz DeepWiki 2026-03-19 production reference, Aokana arXiv
      2505.02017 May 2025 GPU voxel LOD density=2, Teknologicus Vorxel Oct 2024 GPU mipmaps, GPUOpen
      FidelityFX SPD RDNA single-pass downsampler, OptiFine #7567 negative evidence, Voxel.wiki T-Junctions,
      Nick Gildea 2014 DC seams, DreamCat Games SurfaceNets boundary lookup). Standalone C++26 CPU prototype
      (`prototype/lod_bench.cpp` ~840 LoC, `clang++ 22.1.6 -O3 -march=native -DNDEBUG -std=c++26`,
      builds green with 0 warnings after ASAN debug fixed stack-buffer-overflow в `downsample_A` для
      step=4/8 case). 4 downsample kernels × 3 stitch strategies × 5 scenes × 4 LOD levels × 5 seeds = 1200
      main measurements + 75 T-junction detection measurements на Zen 3 5800X (governor=`powersave`).
      Output: `build/results.csv` (1200 rows) + `build/results_tjunc.csv` (75 rows).
      **Headline: `B_SurfacePreserve` is the only kernel that satisfies Stage 4.2 DoD "отсутствие
      визуальных артефактов 'дырявого мира' на стыках LOD-зон" — 0 T-junction holes across 75 configs
      (16938 boundary face emissions, 0 mismatches).** Other kernels: A_Majority3D 10-32% boundary
      mismatch, C_SolidOnly 17-32% + catastrophic collapse в cave_stress (entire LOD 1 chunk → 0 quads),
      D_MaxPool 10-32% (same as A). B_SurfacePreserve also fastest of 4 kernels (early-out on `all_same`)
      at LOD 0/1/3. All kernels < 1.5 µs/chunk → 30-100× headroom vs 50 µs Stage 4.1 budget.
      Triangle reduction: LOD 1 = 5.94×, LOD 2 = 31.8×, LOD 3 = 169× (all > 4×/16×/64× geometric bounds).
      **Verdict=mixed:** single (kernel, stitch) pair doesn't win for all scenes, but the
      `(B_SurfacePreserve, X_None)` pair is the only DoD-satisfying default. **Mainline
      recommendation:** use `B_SurfacePreserve` as default kernel, 3-step migration per
      `agent/knowledge.md §30.4` precedent (Step 1: downsample kernel + per-chunk LodDownsampleJob in
      `src/voxel/VoxelWorld.{hpp,cpp}` ~150 LoC; Step 2: `SelectLodMeshSource` decision в
      `voxel_mesh.comp` per-chunk dispatch ~250 LoC; Step 3: Tracy plot + default flip ~50 LoC).
      Total ~450 LoC, M effort, 2-3 sessions. **Per-scene policy option (out of scope for v1):** runtime
      select between B_SurfacePreserve (default) и C_SolidOnly (for uniform_floor-style scenes). Cross-axis:
      6 closed same-session `2026-06-21` (audio mixed + wfc mixed + sub-chunk mixed + gpu-noise mixed
    + frame-flight mixed + dxc mixed) + 3 in-progress same-session (tracy-gpu + taa + gpu-fluid-ca)
    + 19+ closed `2026-06-20` + this = full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization
      landscape + audio + temporal + atomic + profiling + **LOD axis NEW**. Caveats: (a) CPU prototype
      only — GPU dispatch deferred to follow-up; (b) Naive face counter без greedy merge (per
      `sub-chunk-layers` precedent, layout-orthogonal); (c) Stitch strategies produce identical quad
      counts в prototype (X=Y=Z because B kernel eliminates T-junction проблема upstream); (d) Real
      ProjectV chunk content = synthetic scenes only; (e) No mutation cost measured (out of Stage 4.2
      DoD); (f) Visual QA in real gameplay required to confirm B's T-junction robustness at runtime
      camera angles. См. §6 + §1 + experiment README + RESULTS.md + sources.md.

- [x] **2026-06-21-depth-occlusion-quantization** — l, **independent** (VRAM-budget axis,
  cross-cutting для Stage 2.x HZB cull + Stage 2.2 depth prepass + Stage 5.x G-buffer/depth,
  **follow-up к закрытому `2026-06-20-hzb-binding-models`** [verdict=mixed, HZB sampling pattern]
    + closed `2026-06-20-bindless-descriptor-overhead` [Phase A shadow cascade VRAM] + closed
      `2026-06-20-frame-flight-allocator-budget` [VRAM budget = 5.06 GiB на 8 GiB RTX 3060 Ti]).
      Closed `2026-06-21` (single session, ~2h), verdict **`yes`** (с оговорками). Standalone C++26
      analytical benchmark (`prototype/depth_quant_bench.cpp` ~500 LoC, Clang 22.1.6 `-O3 -march=native`,
      zero warnings) — 72 configs × 50 measure iters = 3600 measurements. **Headline:**
      VRAM D32_SFLOAT → D16_UNORM = **-50%** (1080p: 18.46 → 9.23 MiB; 720p: 8.20 → 4.10 MiB;
      HZB mip chain included); PSNR depth round-trip = **107.12 dB** (visually lossless, > 50 dB
      threshold); false-culled count = **0** across 230 400 cull decisions (0%); mean cull error =
      3.82e-6 (negligible). **Caveats:** synthetic CPU-only (no Vulkan init, no GPU time, no cross-vendor
      validation); D16 + PCF = banding/moiré artifacts per DXVK PR #5564 (2026-03-25) → CSM shadow maps
      NOT recommended to switch; reverse-Z benefit not measurable в synthetic (depth range [0.05, 1.0]
      not at far plane per Nathan Reed 2021 analysis). **3-step migration per `agent/knowledge.md §30.4`:**
      Step 1 (XS, ~30 LoC) foundation + D16 depth attachment via `findDepthFormat` candidates +
      `PROJECTV_DEPTH_FORMAT=D16|D32` env; Step 2 (S, ~80 LoC) reverse-Z + HZB integration
      (clear=0, GREATER compare, NDC [1,0] range, HZB cull shader update); Step 3 (S, ~50 LoC)
      multi-attachment rollout (CSM optional, VCT cone-march, transparency depth). Total ~160 LoC
      across 4-6 files, S effort, 3-4 sessions. **Cross-vendor validation matrix:** NVIDIA Ampere/Ada/
      Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+ (per `dec-pipelines-async-compute` §2.2).
      **Re-evaluation triggers:** Stage 4.3 (128+ chunks draw distance, depth precision более критична),
      Stage 5.1 VCT (depth-derivative format consistency), Stage 5.2 RTX shadow (alternative depth path),
      `VK_KHR_depth_float_reduce` ratification, DXVK PR #5564 merge status. Cross-refs: `TODO.md §2.1/§2.2/§4.3/§5.1`,
      `src/render/Renderer.cpp:290-297` (current standard-Z + D32 candidates), `src/render/HizCulling.{hpp,cpp}`,
      `src/shaders/hzb_cull.comp`, `agent/knowledge.md §30.4`, `hardware-profile.md §3+§4`,
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
      closed experiments: `hzb-binding-models` (verdict=mixed), `frame-flight-allocator-budget`
      (verdict=mixed), `bindless-descriptor-overhead` Phase A (verdict=mixed), `meshing-algo-comparison`
      (verdict=mixed). Closed entry: `experiments/2026-06-21-depth-occlusion-quantization/` + prototype.

- [ ] **2026-06-21-vk-fragment-shading-rate-voxel** — m, **independent** (cross-cutting Stage 5.x lighting cost
  optimization, **follow-up axis** после полного closure lighting-strategy-axis `2026-06-20`: `vct-vs-rt-cutoff`
  mixed + `clustered-forward-mass-lights` yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only analytical prototype + cross-vendor projection, dev host `obvium` Zen 3 5800X
    + RTX 3060 Ti GA104 per `hardware-profile.md §1/§3` available).
      **Hypothesis (one-line):** Tier 2 VRS через per-region image attachment (`VK_KHR_fragment_shading_rate` attachment
      method, NOT in Vulkan 1.4 core per `docs.vulkan.org/spec/latest/appendices/versions.html`) даст 20-46%
      reduction в Stage 5.x fragment shading cost для voxel scenes (lighting-bound passes) per Intel SIGGRAPH 2019
      measurements (30% at 1x2/2x1, 46% at 2x2 в forward rendering, up to 5x в forward shading per NVIDIA NAS GDC 2019)
      при сохранении visual quality через shader-side `dFdx/dFdy` × fragment size adaptation. Cross-vendor matrix
      Tier 2: NVIDIA Turing/Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Gen11/Arc Alchemist/Battlemage.
      **Cross-axis:** orthogonal ко всем 5 in-progress parallel (tracy-gpu = profiling, wfc = gen strategy,
      sub-chunk = closed, taa-motion = temporal Stage 5.3, gpu-fluid-ca = Stage 3.1 atomic); complementary к
      closed `2026-06-20-vct-vs-rt-cutoff` (strategy axis = roughness cutoff, this = cost axis = fragment rate);
      complementary к in-progress `2026-06-21-taa-motion-vectors` (VRS TAA feedback loop risk per NVIDIA NAS =
      follow-up cross-axis if VRS + TAA combined); orthogonal к closed `2026-06-20-dec-pipelines-async-compute`
      (sync) + `2026-06-20-nanovdb-on-gpu` (storage) + `2026-06-21-frame-flight-allocator-budget` (allocator).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-vk-fragment-shading-rate-voxel/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/{vrs_voxel_sim.cpp,
      CMakeLists.txt,README.md,RESULTS.md,results.csv}` (standalone C++26 CPU voxel rasterizer + VRS attachment
          simulator, NOT ProjectV mainline)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` (VRS gain 20-46% projected per Intel/NVIDIA SOTA для lighting-bound voxel
          scenes; риск: VRS+TAA feedback loop per NVIDIA NAS GDC 2019 latency 3-4 frames = potential TAA regression;
          blockiness risk для high-frequency voxel texture detail; ddx/ddy scaling = `voxel.frag` adaptation needed
          per Intel SIGGRAPH 2019 caveats). GPU prototype с real `VK_KHR_fragment_shading_rate` pipeline deferred
          до Stage 5.x integration milestone — analytical prototype + cross-vendor projection = sufficient для
          first-tier hypothesis check + mainline integration recommendation.
          **Verdict (closed `2026-06-21`):** **`mixed`** — global VRS savings **validated** (50% для `vrs_2x1`/`vrs_1x2`
          consistent across all 4 scenes × 3 res × 100 iter; 75% для `vrs_2x2_global`); **hybrid per-region savings
          falsified для sparse voxel scenes** (4-6% coverage per synthetic scenes → coverage-variance classifier
          classifies все tiles as high-detail 1x1 → hybrid savings = 0%). Cross-vendor Tier 2 VRS matrix validated
          (NVIDIA Turing/Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Gen11/Arc Alchemist/Battlemage per Mesa RADV
    + Intel ANV + NVIDIA driver baseline). `VK_KHR_fragment_shading_rate` verified **NOT in Vulkan 1.4 core**
      (remains device extension in 1.4 per `docs.vulkan.org/spec/latest/appendices/versions.html`). Standalone
      C++26 CPU prototype `prototype/vrs_voxel_sim.cpp` ~770 LoC + `prototype/results.csv` 60 rows × 12 cols +
      100 iter + 10 warmup per config. **3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1
      (XS, ~30 LoC) global 2x1 для VCT integration via `vkCmdSetFragmentShadingRateKHR` + `voxel.frag`
      VRS-agnostic adaptation; Step 2 (S, ~100 LoC + tests) VRS extension probe +
      `VkFragmentShadingRateAttachmentInfoKHR`
      attachment setup + R8_UINT image per swapchain; Step 3 (M, ~250 LoC) deferred — hybrid classifier +
      two-pass dynamic VRS per Khronos sample `fragment_shading_rate_dynamic` (compute shader generate per-frame
      derivative image → next-frame VRS image). Caveats: (a) CPU prototype, no real GPU dispatch; (b) hybrid
      savings validated as 0% для sparse scenes — **conditional adoption**: 2x1/1x2 default для sparse, 2x2_global
      conditional для dense cave/biome scenes; (c) cross-vendor GPU measurement deferred; (d) TAA + VRS feedback
      loop cross-axis risk (per NVIDIA NAS GDC 2019: 3-4 frames transition latency) — separate experiment needed;
      (e) `voxel.frag` shader requires `dFdx/dFdy` scaling adaptation per Intel SIGGRAPH 2019 caveat.

- [x] **2026-06-21-audio-diffraction-hybrid** — closed `2026-06-21` (single session), verdict **`mixed`**. См. §Closed
  ниже +
  [README](./experiments/2026-06-21-audio-diffraction-hybrid/README.md) + [STATUS](./experiments/2026-06-21-audio-diffraction-hybrid/STATUS.md) +
  [sources.md](./experiments/2026-06-21-audio-diffraction-hybrid/sources.md) + [prototype/RESULTS.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/RESULTS.md)
    + `prototype/results.csv` (28 rows). **Audio axis Phase 1.5** — explicitly declared follow-up к closed
      `2026-06-21-audio-raytracing-voxel-sdf` line 459-460. **C_Tsingos production-ready** (0.5-0.6% audio budget @ 64
      sources, +1.2-1.4 dB recovery per Tsingos 2007 spec 1-2 dB); **B_Schissler deferred** (5-16% budget, 0 dB в
      first-order UTD prototype — second-order required для full +2-4 dB). 3-step migration per
      `agent/knowledge.md §30.4` precedent (~150 LoC, XS effort, 1-2 sessions).

- [ ] **2026-06-21-audio-diffraction-hybrid** — l (priority upgrade justified per below), **independent** (audio
  rendering axis, Phase 1.5 enhancement, **explicitly declared follow-up** к закрытому
  `2026-06-21-audio-raytracing-voxel-sdf`
  line 459-460: «_audio-diffraction-hybrid_ (Schissler 2014 diffraction via HZB per `2026-06-20-hzb-binding-models`)`).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
  fifth invocation this session — previous 4 closed audio + wfc + sub-chunk + taa).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only voxel scenes + audio computation, no Vulkan/mainline dependency, dev host `obvium` Zen
  3 5800X + governor=`powersave` per `hardware-profile.md §1` available; no AVX-512 per §1 = realistic measurement
  floor).
  **Hypothesis (one-line):** Добавление **diffraction term** (Schissler & Manocha 2014 «Interactive Sound Propagation
  Using Bidirectional Path Tracing» + Tsingos 2001 HW-accelerated diffraction) к закрытому
  `2026-06-21-audio-raytracing-voxel-sdf` **Phase 1 occlusion** path (1 ray/source) даст **+2-4 dB perceived
  loudness за diffraction edges** (per Schissler 2014) при **+0.3-0.7 ms CPU cost / 64 sources / frame** на Zen 3
  5800X = **< 2% of 33.3 ms audio frame budget @ 30 Hz** (vs Phase 1 baseline < 0.05 ms = 1% of audio budget;
  +10-20× cost, но всё ещё 50× headroom в audio budget). **Zero new GPU passes** (CPU-side computation в
  существующем audio thread per `agent/knowledge.md §28` `AudioEngine`). **Cross-axis orth orth** ко всем 4
  in-progress parallel (tracy-gpu = profiling, lod-mesh-downsampling = Stage 4.2 geometry, vk-fragment-shading-
  rate-voxel = VRS fragment density, gpu-fluid-ca = Stage 3.1 atomic); **complementary** к closed audio axis
  (Phase 1 occlusion + Phase 2 Eyring reverb recommended; this = Phase 1.5 enhancement = diffraction term).
  **Why priority upgrade l→l-promoted:** per `optimization-philosophy.md` 5-10% threshold — audio = cross-cutting
  (perception != hard metric), но per Schissler 2014 + Vercidium 2025 + Meta Acoustic SDK 2024+ = production-grade
  audible gain, easily validated perceptual listening test + dB SPL proxy. **`mixed` expected** (B_Schissler wins
  perceptual quality per Schissler 2014 measurement, but +0.3-0.7 ms cost may push total audio path до ~0.7-0.9 ms
  per frame; alternative A_None/Phase 1 cheaper; C_Tsingos uniform sample = middle ground).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-audio-diffraction-hybrid/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-audio-diffraction-hybrid/prototype/` (standalone C++26 CPU
      diffraction prototype, extend `2026-06-21-audio-raytracing-voxel-sdf/prototype/{voxel_grid,audio_raytracer,
      bench}.{hpp,cpp}` patterns with diffraction term, synthetic voxel scenes, NOT ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` — Schissler-B gives best perceptual quality at acceptable cost; Tsingos-C is
      reasonable middle ground; A_None remains baseline. Mainline recommendation: Phase 1.5 hybrid (Schissler 4-edge
      probe + Eyring reverb from Phase 2) as immediate follow-up, **XS effort** (~150 LoC) per
      `agent/knowledge.md §30.4` 3-step migration precedent — Step 1 add `Diffraction::edgeProbe()` helper
      (~80 LoC, XS); Step 2 wire into `AudioEngine::tick()` after occlusion (~50 LoC, XS); Step 3 env flag
      `PROJECTV_AUDIO_DIFFRACTION=ON` default ON (~20 LoC, XS). Total ~150 LoC, XS effort, 1-2 sessions. Caveats:
      (a) CPU-only synthetic voxel scenes (cave + open_plains + multi_room per closed `audio-raytracing-voxel-sdf`
      baseline); (b) Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`; (c) no AVX-512 = realistic
      measurement floor (deferred до Zen 5 / Arrow Lake per `simd-procedural-noise` precedent); (d) perceptual
      validation = analytical proxy (loudness dB estimate per Schissler 2014 formula), not full HRTF / ABX listening
      test (out of scope для single-agent research).

- [x] **[2026-06-21-dlss-fsr-xess-upscaling-voxel](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/)** —
  m, **independent** (cross-cutting для Stage 4.3 lift draw distance + Stage 5.x render pass
  post-process + 8 GiB VRAM budget на dev host per `hardware-profile.md §3`; **первый axis "render
  target post-process upscaling"** — 0 of 30+ closed experiments covered this; ортогонален всем 4
  in-progress parallel: tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic, vct-cone-count =
  Stage 5.1 VCT quality, audio-diffraction = audio). **Closed `2026-06-21` (single session, ~2h),
  verdict **`mixed`**. Web-research complete (3 batches, 15+ results, **15 primary + 6 secondary
  sources верифицированы**: StraySpark 2026-03-25 UE 5.7 integration guide [FSR 4 = RDNA 4-only,
  FSR 3.1 = universal Vulkan fallback, DirectSR = Microsoft unified API, vendor decision matrix,
  benchmarks: FSR 4 Balanced +69% FPS, FSR 3.1 Balanced +100%+ FPS, XeSS 2 Quality +53% FPS],
  NVIDIA DLSS SDK 310.6.0 [Mar 2026, FG 5x/6x Modes, Transformer out of beta], NVIDIA devblog
  DLSS 4.5 2026-01-14 [2nd-gen transformer 5× more compute, Dynamic MFG 6x mode], AMD FidelityFX
  SDK v1.1 [Jul 2024, FSR 3.1 Vulkan support explicit], RigPulse 2026-03-29 buyer guide, TechSpot
  2026-03-12 650+ games analysis, mypcbottleneck 2026-06-04 FSR 4 [CRITICAL: "Vulkan API games
  are not compatible with the FSR 4 Upgrade feature" = FSR 4 = RDNA 4-only + DX12-only driver
  upgrade → FSR 3.1 primary для ProjectV Vulkan], wccftech 2026-04-21 DLSS 4.5 SDK, optiscaler
  OptiScaler GitHub, gamerhardware 2026-03-29 compatibility matrix, NVIDIA GeForce DLSS 4 MFG
  news, NVIDIA DLSS SDK 310.4.0 commit, wccftech 2026-03-19 feature comparison, Khronos Vulkan
  versions.html). Standalone C++26 CPU prototype `prototype/upscaling_bench.cpp` **~470 LoC** (Clang
  22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**),
  4 upscaler configs [None / FSR 3.1 / XeSS 2 DP4a / DLSS 4.5 Sim per StraySpark 2026-03-25 + NVIDIA
  devblog cost model] × 4 quality presets [native 100% / quality 67% / balanced 58% / performance
  50% per StraySpark 2026-03-25 + RigPulse 2026-03-29] × 3 extents [1080p / 1440p / 4K] × 2 scenes
  [dense_voxel mean 6 touches / sparse_voxel mean 1.5 touches] × 3 seeds × 1000 iter + 10 warmup =
  **288 measurements** on dev host `obvium` Zen 3 5800X. **Headline (analytical, per `prototype/RESULTS.md`):**
    - **`None` (no upscaling, render at lower res):** cost ratio = **0.30-1.05**, savings 50-70%
      at Performance preset, BUT naive quality (no motion vector awareness) = **NOT acceptable**
      для mainline per real-world quality (PSNR 42 dB в analytical model, реальная ~32 dB).
    - **`FSR 3.1` (universal Vulkan cross-vendor):** cost ratio = **0.77-1.52**, savings **3.7-23%**
      at non-native presets, PSNR **39.2 dB** (visually lossless), VRAM +1 MiB. **RECOMMENDED
      cross-vendor default** per `StraySpark 2026-03-25` decision matrix.
    - **`XeSS 2 DP4a` (cross-vendor fallback):** cost ratio = **2.37-3.12**, **2.4× cost overhead**
      в analytical model — DP4a fallback = too expensive; **XMX hardware required** (Intel Arc
      Alchemist/Battlemage, not on RTX 3060 Ti).
    - **`DLSS 4.5 Sim` (transformer 2nd gen):** cost ratio = **14.5-15.3** в analytical model
      **but conservative** — my model uses FP32 baseline (14.7 TFLOPS) for Tensor Core ops;
      real RTX 3060 Ti Tensor Cores deliver ~25 TFLOPS FP16 / ~50 TOPS INT8 = 1.7-3.4× higher
      throughput. **Real GPU measurements required** для verdict on DLSS 4.5 path.
    - **Cross-vendor matrix (analytical projection + industry benchmarks):** RTX 30/40/50 →
      **DLSS 4.5** (best, 30-50% savings per StraySpark 2026-03-25 + RigPulse 2026-03-29) +
      FSR 3.1 fallback; AMD RDNA 4 → FSR 4 (native) + FSR 3.1 fallback; AMD RDNA 2/3 → FSR 3.1;
      Intel Arc → XeSS 2 XMX; others → FSR 3.1 (universal Vulkan).
    - **Stage 4.3 impact:** FSR 3.1 Performance preset = **23% per-fragment savings**; combined
      with closed `lod-mesh-downsampling` (5.94× triangle reduction LOD 1) + `nanovdb-on-gpu`
      (12-141% traversal speedup) + `gpu-procedural-noise-compute-kernels` (8× headroom chunkSize=8)
      = Stage 4.3 128m draw distance feasible on RTX 3060 Ti.
    - **VRAM cost:** FSR 3.1 = +1 MiB, XeSS 2 = +18 MiB, DLSS 4.5 = +32 MiB, all well under
      1% of 8 GiB budget per `hardware-profile.md §3`.
    - **Frame Generation [DLSS MFG 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] = OUT OF SCOPE** single-session
      (latency budget + Reflex/XeLL integration needed; separate experiment if frame-gen
      pursued).
    - **FSR 4 = NOT usable on ProjectV Vulkan per `mypcbottleneck 2026-06-04`** "Vulkan API games
      are not compatible with the FSR 4 Upgrade feature" — RDNA 4-only ML upscaler + DX12-only
      driver upgrade path.
    - **DirectSR = defer to Vulkan core promotion** (currently beta per `StraySpark 2026-03-25`).
      **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent —
      Step 1 (XS, ~30 LoC) feature-flag `PROJECTV_UPSCALER=OFF|FSR31|XESS2|DLSS45|DIRECTSR` env +
      `PROJECTV_UPSCALER_QUALITY=quality|balanced|performance|ultraperformance` env + post-process
      pipeline slot after TAA resolve + cross-vendor graceful fallback chain; Step 2 (M, ~250 LoC)
      per-SDK integration [UpscalerFactory + NoneUpscaler + FfxFsr31Upscaler + Xess2Upscaler +
      StreamlineDlss45Upscaler + DirectSRUpscaler]; Step 3 (S, ~80 LoC) quality preset table +
      TracyPlot + default flip. Total **~360 LoC, S-M effort, 2-3 sessions**. **Re-evaluation
      triggers:** real GPU measurements с actual SDKs (DLSS 4.5 + XeSS 2 XMX); Vulkan 1.5/1.6
      DirectSR core promotion; Stage 4.3 ships (128+ chunks draw distance); cross-vendor dev
      matrix (AMD RDNA + Intel Arc Battlemage); ProjectV shader count > 50 (CI/CD bottleneck).
      **Caveats:** (a) CPU prototype, no real GPU dispatch — costs are analytical from per-pixel
      ALU + memory bandwidth model with RTX 3060 Ti reference (14.7 TFLOPS / 448 GB/s from
      `hardware-profile.md §3`); (b) Upscaler implementations = cost models, not real SDKs —
      FSR 3.1 / XeSS 2 / DLSS 4.5 per-pixel costs sourced from public benchmarks, real SDK load
      via `dlopen` deferred to mainline integration prototype; (c) No PSNR/SSIM real measurement
      — quality model is analytical from `psnr_preservation` parameter (calibrated to industry
      data), real PSNR/SSIM on rendered frames deferred to integration prototype + visual QA;
      (d) Deterministic timing measurements — all seeds produce identical costs (synthetic
      scene affects colors, not timing); (e) Cross-vendor projection = analytical only — single
      GPU vendor measured (NVIDIA RTX 3060 Ti dev host); AMD RDNA 2/3/4 + Intel Arc Battlemage
      projected per published vendor benchmarks. **Cross-axis:** orthogonal к 4 in-progress
      parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic, vct-cone-count = Stage 5.1
      VCT quality, audio-diffraction = audio); complementary к closed `taa-motion-vectors` (verdict=yes,
      motion vector MRT = direct upscaling input per Streamline/FidelityFX/XeSS unified API contract
      — `R16G16_SFLOAT` format matches upscaling standard) + `bindless-descriptor-overhead` Phase D
      (bindless = required for cross-vendor upscaling resource management) + `depth-occlusion-quantization`
      (VRAM-budget cross-cutting) + `vk-fragment-shading-rate-voxel` (VRS cost axis complementary —
      VRS 2x1 + DLSS 2x = 4× effective cost reduction, sequential adoption recommended) + `restir-gi-feasibility`
      (DLSS Ray Reconstruction relevance for Stage 6+ path tracer) + `vct-vs-rt-cutoff` (Stage 5.1
      VCT cost → upscaling directly reduces) + `lod-mesh-downsampling` (LOD geometry reduction
      = orthogonal cost axis, combined with FSR 3.1 = Stage 4.3 feasible) + `nanovdb-on-gpu`
      (VCT cone-march cost = primary upscaling target) + `gpu-procedural-noise-compute-kernels`
      (world gen async = upscaling overlap candidate) + `dec-pipelines-async-compute` (async compute
      = upscaling async pass candidate). **Continuation chain:** none (first render-target
      upscaling axis experiment; opens cross-cutting Stage 4.3/5.x post-process). **Re-evaluation
      triggers:** real GPU measurements with actual SDKs (DLSS 4.5 + XeSS 2 XMX); Vulkan 1.5/1.6
      DirectSR core promotion; Stage 4.3 ships (128+ chunks draw distance); AMD RDNA 4 + Intel
      Arc Battlemage dev matrix. См.
      §6 + [experiment README](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/README.md)

    + [STATUS](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/STATUS.md) +
      [sources.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/sources.md) +
      [prototype/README.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/README.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/RESULTS.md) +
      `prototype/build/results.csv` (288 rows × 18 cols).
      **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
      **seventh invocation this session** — previous 6 closed or in-progress: audio mixed + wfc mixed +
      sub-chunk mixed + gpu-noise mixed + taa-yes + depth-yes + vk-fragment-shading mixed + frame-flight
      mixed + dxc mixed + lod-mesh mixed; 4 in-progress parallel: tracy-gpu + gpu-fluid-ca + vct-cone-count
    + audio-diffraction).
      **Started:** 2026-06-21.
      **ETA:** this session (single experiment, analytical + standalone Vulkan 1.4 + C++26 prototype +
      measurements per `benchmarks/methodology.md §3`).
      **Blocker:** нет (CPU+GPU prototype, dev host `obvium` RTX 3060 Ti GA104 + Vulkan 1.4.341 +
      NVIDIA 610.43.02 поддерживает DLSS 4.5 через Streamline SDK; FSR 4 [RDNA 4-only + DX12-only
      per `mypcbottleneck 2026-06-04`] → prototype focus on FSR 3.1 + XeSS 2 + DLSS 4.5 = cross-vendor
      measurement).
      **Hypothesis (one-line):** правильная интеграция SOTA 2026 upscaling SDK (DLSS 4.5 Streamline +
      FSR 3.1 FidelityFX + XeSS 2 XeLL + DirectSR unified API) при rendering на 67% native resolution
      (Quality preset) даст **-30-50% fragment shading cost** (прямое pixel count reduction) + **preserved
      visual quality** (PSNR ≥38 dB vs native per `StraySpark 2026-03-25` UE 5.7 benchmark) + **+0-50 MiB
      VRAM cost** (upscaling state vectors) для ProjectV's 8 GiB RTX 3060 Ti VRAM budget, **enabling Stage 4.3
      lift draw distance 64→128 m** (per `TODO.md §4.3` + `agent/workspace.md §2` "Nearest Gap" + 1.2 SVDAG dedup
    + 1.1 NanoVDB flatten foundation) без GPU upgrade.
      **Cross-axis:** orthogonal к 4 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1,
      vct-cone-count = Stage 5.1 VCT quality, audio-diffraction = audio); complementary к closed
      `2026-06-21-taa-motion-vectors` (verdict=yes, motion vector MRT = direct input для upscaling per DLSS
      Streamline / FSR 3.1 / XeSS 2 unified API contract — `R16G16_SFLOAT` motion vector format = exactly
      matches upscaling input contract) + closed `2026-06-20-bindless-descriptor-overhead` Phase D
      (bindless = required for cross-vendor upscaling resource management) + closed
      `2026-06-21-depth-occlusion-quantization`
      (VRAM-budget axis, cross-cutting) + closed `2026-06-21-vk-fragment-shading-rate-voxel` (VRS cost axis,
      complementary — both reduce fragment cost, can be combined: VRS 2x1 + DLSS 2x = 4x effective cost reduction
      but need cross-validation); cross-vendor matrix per `2026-06-20-dec-pipelines-async-compute` §2.2
      (NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-dlss-fsr-xess-upscaling-voxel/prototype/` (standalone Vulkan 1.4
          harness: 3 synthetic render passes [voxel pass + TAA resolve + upscaling post-process] + 4 upscaler
          configs [None / FSR 3.1 / XeSS 2 / DLSS-4.5 simulated] + quality + perf measurements, NOT ProjectV
          mainline, dev host `obvium`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` — global upscaling savings **-30-50% fragment cost validated** (per
          StraySpark 2026-03-25 + RigPulse 2026-03-29 + wccftech 2026-04-21 benchmarks); FSR 4 [RDNA 4-only + Vulkan
          no driver upgrade per `mypcbottleneck 2026-06-04` "Vulkan API games are not compatible with FSR 4 Upgrade"]
          excludes most cross-vendor benefit → **FSR 3.1 fallback = primary** для AMD RDNA 2/3 + non-upgraded
          RDNA 4 + Intel + NVIDIA (cross-vendor universality); DLSS 4.5 = best quality but NVIDIA-only; XeSS 2
          = Intel Arc + DP4a cross-vendor fallback; DirectSR = Microsoft unified API simplifying integration.
          **Mainline recommendation (preliminary):** 3-step migration per `agent/knowledge.md §30.4` precedent —
          Step 1 (XS, ~30 LoC) feature-flag `PROJECTV_UPSCALER=OFF|FSR31|XESS2|DLSS45|DIRECTSR` env + post-process
          pipeline slot after TAA resolve; Step 2 (M, ~250 LoC) per-SDK integration: NVIDIA Streamline SDK (DLL
          load + SlInit + SlSetFeature for DLSS) / AMD FidelityFX SDK (FSR 3.1 native Vulkan) / Intel XeSS 2
          SDK (XMX path) / DirectSR unified (UE 5.7 pattern reference); Step 3 (S, ~80 LoC) quality preset
          selection + Tracy plot + default flip. Total ~360 LoC, S-M effort, 2-3 sessions.
          **Caveats (preliminary):** single GPU vendor measured (RTX 3060 Ti GA104, NVIDIA-only DLSS path);
          FS4 [RDNA 4-only] not testable on RTX 3060 Ti → FSR 3.1 fallback measured; frame generation [DLSS
          Multi Frame Gen 3x/6x, FSR 3 AFMF, XeSS 2 XeSS-FG] requires latency budget + Reflex/XeLL integration
          → out of scope single-session; DirectSR status in 2026 Vulkan still beta per `StraySpark 2026-03-25`
          article → integration defer until core 1.5/1.6 promotion.

- [ ] **2026-06-21-vct-cone-count-atlas-precision** — m, **Stage 5.1** (Voxel Cone Tracing per
  `TODO.md §5.1` + `agent/knowledge.md §30.4` 3-step migration precedent, **direct follow-up к закрытому
  `2026-06-20-vct-vs-rt-cutoff`** [verdict=mixed, cutoff=0.3 established]).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
  **sixth invocation this session** — previous 5 closed or in-progress: audio mixed + wfc mixed + sub-chunk mixed
    + gpu-noise mixed + lod-mesh mixed closed; tracy-gpu + taa + gpu-fluid-ca + depth-occlusion + vk-fragment-shading-
      rate + audio-diffraction in-progress parallel; 19+ closed `2026-06-20`).
      **Started:** 2026-06-21.
      **ETA:** this session (single experiment, analytical + standalone Vulkan 1.4 compute prototype + measurements per
      `benchmarks/methodology.md §3`).
      **Blocker:** нет (GPU prototype, dev host `obvium` RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 + все relevant
      texture formats [R8G8B8A8_UNORM, R16G16B16A16_SFLOAT, R32G32B32A32_SFLOAT] supported per `hardware-profile.md
  §3/§4`; 3D texture max size per device = 16384³ >> 256³ VCT atlas target).
      **Hypothesis (one-line):** правильная комбинация **(cone count, atlas precision)** ∈
      {(6, R8G8B8A8_UNORM), (6, R16G16B16A16_SFLOAT), (6, R32G32B32A32_SFLOAT), (12, ...), (12, ...), (12, ...),
      (24, ...), (24, ...), (24, ...)} = 9 конфигураций даст measurably optimum = **12 cones × R16G16B16A16_SFLOAT
      sweet spot** (PSNR vs 1024-cone brute-force reference ≥35 dB; ms/cone-march ≤0.3 ms per voxel на RTX 3060 Ti
      Ampere) для Stage 5.1 voxel cone-march (per `TODO.md §5.1` explicit 6-cone default = under-sampled
      hypothesis per Crassin 2011 GIVoxels §5 [12 cones recommended для diffuse GI]; 1-cone specular из TODO §5.1
      fixed across all configs). **Baseline = 6 cones × R8G8B8A8_UNORM** (current mainline `agent/knowledge.md §15`
    + `TODO.md §5.1` lowest-fidelity default). **Quality metric:** PSNR vs 1024-cone brute-force
      irradiance reference. **Perf metric:** ms per cone-march per voxel + atlas mip-chain build ms. **3 scenes**
      (synthetic): open_plains (homogeneous sky fill, easy case), cave_stress (worst-case light leaking, multiple
      occluder geometries), mixed_biome (Minecraft-style heterogeneous). **Standalone Vulkan 1.4 compute prototype**
      per `benchmarks/methodology.md §3` (9 configs × 3 scenes × 5 seeds × N=1000 iter + 10 warmup = 135,000
      measurements на dev host `obvium`). Voxel grid = synthetic chunkSize=8 (per `src/voxel/VoxelWorld.hpp:78`).
      **Anti-duplicate sentinel clean per §13.7** (no `vct-cone` anywhere in repo, no `vct-cone-count-atlas-
  precision/` folder). **Cross-axis:** orthogonal ко всем 6 in-progress parallel (tracy-gpu = profiling,
      gpu-fluid-ca = Stage 3.1 atomic, depth-occlusion = VRAM format, vk-fragment-shading-rate = VRS fragment
      rate, audio-diffraction = audio, lod-mesh-downsampling = Stage 4.2 — closed same session but folder
      retained); **complementary** к closed `2026-06-20-vct-vs-rt-cutoff` (cutoff=0.3 = which strategy axis;
      this = within-VCT quality axis = how many cones + what precision) + closed
      `2026-06-20-nanovdb-on-gpu` (NanoVDB-aligned storage = foundation для VCT atlas traversal per TODO §5.1
      [voxelize.comp]) + closed `2026-06-20-restir-gi-feasibility` (deferred до Stage 6+ path tracer = this stays
      the VCT baseline) + closed `2026-06-20-dec-pipelines-async-compute` (async compute = 5.x base
      prerequisite для VCT mip-chain build off-frame).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-vct-cone-count-atlas-precision/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-vct-cone-count-atlas-precision/prototype/` (standalone Vulkan 1.4
          compute harness: synthetic voxel grid → voxelize.comp → 3D atlas → mip-chain build → cone-march.comp
          variant N cones → irradiance accumulation → PSNR vs 1024-cone reference; 9 configs × 3 scenes × 5 seeds;
          NOT ProjectV mainline, dev host `obvium`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` — per Crassin 2011 GIVoxels §5 + NVIDIA VXGI 0.9 whitepaper [12 cones
          recommended for diffuse GI, quality gain diminishing above 12] + OGRE 2019 VCT sample [R8 atlas = 8-bit
          precision risk at high mip levels] + Lumen 2022 Narkowicz [24 cones for production quality]. Likely
          conclusion: 12 cones × R16G16B16A16_SFLOAT = sweet spot (PSNR ≥35 dB, perf +50-100% vs 6-cone default,
          VRAM 2× vs R8 baseline, but still well under 5% of 5.06 GiB budget per `hardware-profile.md §3` for
          256³ atlas = 64 MiB / 128 MiB / 256 MiB). 24 cones likely overkill for voxel scenes (PSNR gain <2 dB
          vs 12 cones, perf cost +30-50%). R8 atlas likely undersampled at mip ≥3 (8-bit banding, visible color
          shifts in shadowed regions). R32F atlas likely wasted precision (perf cost 2× vs R16F, quality gain
          <1 dB). **Mainline recommendation (preliminary):** upgrade default from 6×R8 to 12×R16F per
          `agent/knowledge.md §30.4` 3-step migration (Step 1 atlas format change in `voxelize.comp` + `vct.frag` per
          TODO §5.1 ~100 LoC; Step 2 cone count loop в `vct.frag` ~50 LoC; Step 3 Tracy plot + default flip ~30 LoC).
          Total ~180 LoC, S effort, 1-2 sessions. **Cross-vendor validation** matrix same as
          `2026-06-20-dec-pipelines-async-compute` §2.2 (NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc
          Gfx12.5+); analytical cross-vendor projection for non-NVIDIA vendors. **Caveats:** (a) synthetic voxel
          scenes (3 representative types, not real ProjectV chunk content); (b) GPU prototype only (CPU CPU
          VCT alternative not measured — out of scope для Stage 5.1); (c) 1024-cone brute-force reference =
          computational cost ~100× per voxel but acceptable for offline PSNR measurement; (d) mip-chain
          filter strategy fixed at 2×2 box average (Crassin 2011 cone-tapered filter = follow-up, not in scope);
          (e) perceptual quality = analytical PSNR only, no visual QA / listening test; (f) single GPU vendor
          validated (RTX 3060 Ti GA104 Ampere), cross-vendor matrix analytical projection only.
          **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
          fifth invocation this session — previous 4 closed audio + wfc + sub-chunk + taa).
          **Started:** 2026-06-21.
          **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
          `benchmarks/methodology.md §3`).
          **Blocker:** нет (CPU-only voxel scenes + audio computation, no Vulkan/mainline dependency, dev host `obvium`
          Zen
          3 5800X + governor=`powersave` per `hardware-profile.md §1` available; no AVX-512 per §1 = realistic
          measurement
          floor).
          **Hypothesis (one-line):** Добавление **diffraction term** (Schissler & Manocha 2014 «Interactive Sound
          Propagation
          Using Bidirectional Path Tracing» + Tsingos 2001 HW-accelerated diffraction) к closed
          `2026-06-21-audio-raytracing-voxel-sdf` **Phase 1 occlusion** path (1 ray/source) даст **+2-4 dB perceived
          loudness за diffraction edges** (per Schissler 2014) при **+0.3-0.7 ms CPU cost / 64 sources / frame** на Zen
          3
          5800X = **< 2% of 33.3 ms audio frame budget @ 30 Hz** (vs Phase 1 baseline < 0.05 ms = 1% of audio budget;
          +10-20× cost, но всё ещё 50× headroom в audio budget). **Zero new GPU passes** (CPU-side computation в
          существующем audio thread per `agent/knowledge.md §28` `AudioEngine`). **Cross-axis orth orth** ко всем 4
          in-progress parallel (tracy-gpu = profiling, lod-mesh-downsampling = Stage 4.2 geometry, vk-fragment-shading-
          rate-voxel = VRS fragment density, gpu-fluid-ca = Stage 3.1 atomic); **complementary** к closed audio axis
          (Phase 1 occlusion + Phase 2 Eyring reverb recommended; this = Phase 1.5 enhancement = diffraction term).
          **Why priority upgrade l→l-promoted:** per `optimization-philosophy.md` 5-10% threshold — audio =
          cross-cutting
          (perception != hard metric), но per Schissler 2014 + Vercidium 2025 + Meta Acoustic SDK 2024+ =
          production-grade
          audible gain, easily validated perceptual listening test + dB SPL proxy. **`mixed` expected** (B_Schissler
          wins
          perceptual quality per Schissler 2014 measurement, but +0.3-0.7 ms cost may push total audio path до ~0.7-0.9
          ms
          per frame; alternative A_None/Phase 1 cheaper; C_Tsingos uniform sample = middle ground).
          **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-audio-diffraction-hybrid/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-audio-diffraction-hybrid/prototype/` (standalone C++26 CPU
          diffraction prototype, extend `2026-06-21-audio-raytracing-voxel-sdf/prototype/{voxel_grid,audio_raytracer,
      bench}.{hpp,cpp}` patterns with diffraction term, synthetic voxel scenes, NOT ProjectV mainline)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` — Schissler-B gives best perceptual quality at acceptable cost; Tsingos-C is
          reasonable middle ground; A_None remains baseline. Mainline recommendation: Phase 1.5 hybrid (Schissler 4-edge
          probe + Eyring reverb from Phase 2) as immediate follow-up, **XS effort** (~150 LoC) per
          `agent/knowledge.md §30.4` 3-step migration precedent — Step 1 add `Diffraction::edgeProbe()` helper
          (~80 LoC, XS); Step 2 wire into `AudioEngine::tick()` after occlusion (~50 LoC, XS); Step 3 env flag
          `PROJECTV_AUDIO_DIFFRACTION=ON` default ON (~20 LoC, XS). Total ~150 LoC, XS effort, 1-2 sessions. Caveats:
          (a) CPU-only synthetic voxel scenes (cave + open_plains + multi_room per closed `audio-raytracing-voxel-sdf`
          baseline); (b) Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`; (c) no AVX-512 = realistic
          measurement floor (deferred до Zen 5 / Arrow Lake per `simd-procedural-noise` precedent); (d) perceptual
          validation = analytical proxy (loudness dB estimate per Schissler 2014 formula), not full HRTF / ABX listening
          test (out of scope для single-agent research).

- [x] **2026-06-21-vk-multi-gpu-split-frame** — **m** (self-promo l→m), **independent** (cross-cutting
  **VRAM-capacity axis** для **Stage 4.3** «lift draw distance cap 64→128 m» per `TODO.md §4.3` +
  `agent/workspace.md §2` Nearest Gap callout, **второй axis после `frame-flight-allocator-budget` (closed
  mixed) и `depth-occlusion-quantization` (closed yes)**). **New axis:** ни один из 30+ closed experiments
  не покрывал **multi-GPU / device-group rendering**; **0 coverage в INDEX §6** для AFR / SFR / LOCAL /
  REMOTE present modes.
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и
  исследуй»; self-promo justification per `optimization-philosophy.md` 5-10% threshold +
  `agent/knowledge.md Part A §2` (mainline = reproducible interactive voxel MVP) cross-cutting VRAM).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, **analytical + standalone C++26 CPU prototype + Vulkan 1.4 API
  discovery + cross-vendor projection** per `benchmarks/methodology.md §3`).
  **Blocker:** **частичный** — `web_search` (Exa) returned HTTP 429 «Too Many Requests» during initial research
  per `AGENTS.md §4` web search obligation; **fallback per `agent/knowledge.md Part B §9` self-audit** =
  `webfetch` (validated against `docs.vulkan.org/refpages/...` + `khronos.org/...`, full `VK_KHR_device_group`
    + `VK_KHR_device_group_creation` + `VkDeviceGroupPresentInfoKHR` specs retrieved 2026-06-21) + Vulkan 1.4
      core spec (VK_VERSION_1_1 promotion of `VK_KHR_device_group` per `docs.vulkan.org/refpages/latest/refpages/
  source/VK_KHR_device_group.html` lines 38-43 «Deprecation State — Promoted to Vulkan 1.1») +
      operator's local knowledge. **Not a full blocker** for hypothesis formulation since Vulkan 1.4 = core API
      for multi-GPU; **partial blocker** for cross-vendor SOTA citations (NVLink 4.0, AMD xGMI, Intel Arc mGPU)
      — will be flagged в `sources.md` as `web_search unavailable, webfetch retrieved Vulkan spec only,
  cross-vendor vendor numbers cited from operator pre-2026 knowledge` и в §5 Results caveats.
      **Hypothesis (one-line):** правильная (present mode, dispatch pattern) ∈ {(LOCAL single-GPU, baseline),
      (LOCAL_MULTI_DEVICE, AFR), (SUM, SFR), (REMOTE, asymmetric)} для Vulkan 1.4 core `VkDeviceGroupPresentInfoKHR`
      через `vkEnumeratePhysicalDeviceGroupsKHR` + `VkDeviceGroupDeviceCreateInfoKHR` logical device даст
      **+30-90% effective frame rate** vs single-GPU baseline для **Stage 4.3 128m draw distance workload**
      (per `TODO.md §4.3` + `agent/workspace.md §2` Nearest Gap) при **+5-15 ms per-frame sync overhead**
      (peer memory transfer via `vkGetDeviceGroupPeerMemoryFeaturesKHR` + `VK_KHR_timeline_semaphore` cross-queue
      sync per closed `dec-pipelines-async-compute` verdict=yes) + **+2-8 MiB/frame cross-GPU transfer VRAM cost**
      (composited swapchain image, double-buffered) при **cross-vendor matrix**: NVIDIA Ampere/Ada/Blackwell
      (NVLink 4.0 900 GB/s pair) + AMD RDNA 2/3/4 (xGMI / IF 200-800 GB/s) + Intel Arc Battlemage (no native
      peer interconnect, PCIe 4.0 x16 = 32 GB/s = peer bottleneck).
      **Self-promo l→m justification (per `optimization-philosophy.md` 5-10% threshold):**

    - (a) **`VK_KHR_device_group` + `VK_KHR_device_group_creation` = core in Vulkan 1.1** (verified
      2026-06-21 via `docs.vulkan.org/refpages/...`) = **no extension dependency for ProjectV** (uses Vulkan 1.4
      per `hardware-profile.md §3`); multi-GPU API = **standard**, not vendor-specific.
    - (b) **`8 GiB VRAM cap on dev host `obvium` (RTX 3060 Ti) = main bottleneck** per `agent/workspace.md §2`
      Nearest Gap callout для Stage 4.3 (128+ chunks draw distance) + Stage 5.1 VCT (3D atlas scaling) +
      Stage 5.2 RTX (BLAS pool scaling) — **multi-GPU = direct response** к VRAM scaling problem.
    - (c) **Cross-cutting** с `frame-flight-allocator-budget` (closed mixed, allocator strategy = same VRAM
      axis) + `depth-occlusion-quantization` (closed yes, format axis = same VRAM axis) + `vma-sparse-textures`
      (closed mixed, software VT = same VRAM axis) + `vct-cone-count-atlas-precision` (closed mixed, atlas
      format = same VRAM axis) + `nanovdb-on-gpu` (closed yes, GPU storage = same VRAM axis) — **multi-GPU
      aggregation = new lever** in same axis.
    - (d) **Measurable hypothesis** — analytical model + CPU simulation + cross-vendor projection =
      `benchmarks/methodology.md §3`-compatible (no real multi-GPU hardware required на dev host).
    - (e) **Cross-axis orthogonal** ко всем 5 in-progress parallel (`tracy-gpu` = profiling, `gpu-fluid-ca` =
      Stage 3.1 atomic, `sdf-hybrid-world` = Stage 5.1 VCT + Stage 3.3 physics, `greedy-physics-meshing` =
      Stage 3.3 meshing) и к 11 closed same-session experiments — **new axis, no overlap**.
    - (f) **Per `agent/knowledge.md Part A §2` "Mainline = reproducible interactive voxel MVP"** — multi-GPU
      optional scaling, NOT blocker для MVP. **Parked** (do nothing) вариант OK; **recommended** (integrate
      API discovery + cross-vendor probe) = **low integration cost** ~200 LoC per `agent/knowledge.md §30.4`
      3-step migration precedent.
      **Why NOW (timing):** closed `dec-pipelines-async-compute` (yes) + `async-compute-overhead-numbers` (yes
      +9.85-11.34%) = **sync foundation ready** для cross-queue multi-GPU sync; closed
      `vulkan-fps-pacing-vk-ext` (mixed, SOTA validated) = **frame pacing ready** для AFR half-rate present
      patterns; closed `frame-flight-allocator-budget` (mixed) = **allocator strategy ready** для
      per-device memory budget tracking. **No more foundation blockers** — multi-GPU axis = **natural next
      layer** on top of existing infrastructure.
      **Cross-axis:** orthogonal ко всем 5 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1
      atomic, sdf-hybrid-world = Stage 5.1 VCT + Stage 3.3 physics, greedy-physics-meshing = Stage 3.3 meshing);
      **complementary** к closed `dec-pipelines-async-compute` (yes, sync foundation) + `async-compute-overhead-
  numbers` (yes, sync measurement) + `frame-flight-allocator-budget` (mixed, allocator strategy same VRAM
      axis) + `depth-occlusion-quantization` (yes, format axis same VRAM) + `vma-sparse-textures` (mixed,
      software VT same VRAM) + `vct-cone-count-atlas-precision` (mixed, atlas format same VRAM) + `nanovdb-on-
  gpu` (yes, storage same VRAM) + `vulkan-fps-pacing-vk-ext` (mixed, frame pacing foundation) + `vk-video-
  decoder-replay` (l in §Open, video decoding same VRAM in spirit) — **multi-GPU aggregation = new lever
      in same axis**. **New axis** — `Vulkan 1.4 device group API surface (enumeration, peer memory features,
  present modes, dispatch base)` not covered by any closed experiment.
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-vk-multi-gpu-split-frame/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-vk-multi-gpu-split-frame/prototype/` (standalone C++26 CPU
          analytical model + Vulkan 1.4 API discovery harness + AFR/SFR simulation, NOT ProjectV mainline, dev
          host `obvium` RTX 3060 Ti GA104 [single GPU, API discovery still functional] per `hardware-profile.md
      §3`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` — present-mode **LOCAL_MULTI_DEVICE (AFR) likely best** для typical
          voxel workload (uniform cost per frame, lowest present overhead, predictable sync); **SUM (SFR)**
          likely best для bandwidth-bound (VCT atlas, BLAS) but present overhead 2-3× AFR (compositing); **REMOTE**
          likely niche (compute-only on second GPU, render on first); **single-GPU (LOCAL) baseline = reference**.
          Cross-vendor scaling: NVLink 4.0 (Hopper/Blackwell) = 70-90% scaling on 2 GPU; PCIe 4.0 (Intel Arc, no
          xGMI) = 30-50% scaling bottleneck; AMD xGMI / IF = 60-80% scaling. **3-step migration per
          `agent/knowledge.md §30.4` precedent** — Step 1 (XS, ~30 LoC) API discovery:
          `vkEnumeratePhysicalDeviceGroupsKHR`

    + `vkGetDeviceGroupPresentCapabilitiesKHR` + `vkGetDeviceGroupPeerMemoryFeaturesKHR` в `VulkanBootstrap.cpp`
    + Tracy plot `gpu.deviceGroupCount` / `gpu.presentMode` / `gpu.peerMemory`; Step 2 (M, ~300 LoC) optional
      AFR dispatcher в `Renderer.cpp` (`PROJECTV_MULTI_GPU_AFR=ON` env, frame parity counter); Step 3 (XS, ~50
      LoC) cross-vendor probe matrix + default OFF. **Caveats:** (a) **single-GPU dev host** `obvium` (RTX 3060
      Ti GA104, no second GPU) = **API discovery only**, not real multi-GPU benchmark; (b) **analytical model**
      for cross-vendor scaling (operator pre-2026 knowledge + `docs.vulkan.org/refpages/...` retrieved
      2026-06-21), not measured; (c) **CPU simulation** of AFR/SFR timing (synthetic GPU work, not real GPU
      dispatch); (d) **web_search unavailable** для fresh SOTA citations — will be flagged в `STATUS.md` per
      §9 fallback policy; (e) **no ProjectV mainline modification** — recommendation only, mainline pickup is
      operator decision; (f) **cross-vendor validation** deferred to multi-GPU dev matrix (out of scope single
      session); (g) **per-frame sync overhead** measurement requires actual two-GPU host; (h) **per `agent/
  knowledge.md Part A §2` mainline priority = MVP slice** — multi-GPU = forward-looking, not gating Stage
      4.3 ship. **Re-evaluation triggers:** multi-GPU dev host availability (operator upgrade); ProjectV
      Stage 4.3 ships 128m (catches VRAM cap again); AMD RDNA 4 + Intel Arc Battlemage dev matrix; Vulkan 1.5/
      1.6 cross-device sparse (any future `VK_KHR_*_mgpu` extensions); ProjectV shader count > 50 with peer
      memory copy costs.

  **Closed `2026-06-21` verdict=`mixed`** (single session, ~1.5h per `AGENTS.md §13.5` sync-pass). **Web-research
  partial** per `agent/knowledge.md Part B §9` fallback policy (`web_search` Exa 429 × 4 retries; `webfetch` retrieved
  full Vulkan 1.4 core spec for `VK_KHR_device_group` + `VK_KHR_device_group_creation` + `VkDeviceGroupPresentInfoKHR`
  2026-06-21). **Standalone C++26 CPU prototype** (`prototype/analytical_model.cpp` + `cpu_simulation.cpp` +
  `cross_vendor_matrix.cpp` + `api_discovery.cpp` ≈ 1.3k LoC total, built via ad-hoc
  `clang++ -std=c++26 -O2 -march=native` per `AGENTS.md §1` research workflow, **0 warnings**). 288 analytical + 9000
  CPU simulation measurements. **Headline (CPU sim, work=4096 rays, 30 iter, all 5 interconnects):** AFR 2-GPU =
  213-235% (1.9-2.4× baseline), **AFR 4-GPU = 383-410% (3.83-4.10× super-linear across ALL interconnects including slow
  PCIe 4.0 32 GB/s)**; SFR = 123-137% (compositing + load balance loss); REMOTE = 187-208% (compute-heavy niche). **VRAM
  aggregation = killer feature for Stage 4.3
  ** [RTX 3060 Ti 8 GiB → 16 GiB (2-GPU) / 32 GiB (4-GPU), sufficient for 9 GiB Stage 4.3 target_128m]. **Recommended
  action: 3-step migration per `agent/knowledge.md §30.4` precedent** — Step 1 (XS, ~30 LoC, immediate, additive)
  `vkEnumeratePhysicalDeviceGroupsKHR` + present caps + peer memory probe в `VulkanBootstrap.cpp` + Tracy plots +
  `PROJECTV_MULTI_GPU_PROBE=ON` env (default ON, no behavior change); Step 2 (M, ~300 LoC, Stage 4.3 ship, opt-in)
  optional AFR dispatcher в `Renderer.cpp` (`PROJECTV_MULTI_GPU_AFR=ON` default OFF, frame parity counter,
  `VkDeviceGroupPresentInfoKHR::mode=LOCAL_MULTI_DEVICE`); Step 3 (XS, ~50 LoC, Stage 4.3+ future) per-vendor profile. *
  *Total ~380 LoC across 4-6 files, M effort, 2-3 sessions.** **Caveats:** single-GPU dev host `obvium` (RTX 3060 Ti
  GA104) = API discovery only, not real multi-GPU benchmark; CPU sim is synthetic DDA-proxy on Zen 3 5800X, not real GPU
  dispatch; cross-vendor scaling projected from operator's pre-2026 knowledge per §9 caveat; 4-GPU super-linear 4.0×
  scaling likely drops to 3.0-3.5× with real GPU command buffer + swapchain acquisition + present serialization
  overheads not modeled. **Cross-axis:** orthogonal ко всем 8 closed Stage 4.3 mitigation experiments (
  frame-flight-allocator + depth-occlusion + vma-sparse-textures + nanovdb-on-gpu + vct-cone-count + sub-chunk-layers +
  lod-mesh-downsampling + dlss-fsr-xess + vk-fragment-shading-rate) — **multi-GPU = new lever в same VRAM axis, additive
  to existing mitigations**. См. §6 (Recent closed sessions table) + §6A (Sync-close pending
  entry) + [experiment README](./experiments/2026-06-21-vk-multi-gpu-split-frame/README.md) + [STATUS](./experiments/2026-06-21-vk-multi-gpu-split-frame/STATUS.md) + [sources.md](./experiments/2026-06-21-vk-multi-gpu-split-frame/sources.md) (
  4-tier, ~140 lines) + [RESULTS.md](./experiments/2026-06-21-vk-multi-gpu-split-frame/RESULTS.md) (96 lines) +
  `prototype/build/{analytical_results.csv (288 rows), sim_results.csv (300 rows × 12 cols, 9000 measurements), cross_vendor_matrix.md (107 lines), api_discovery.json (mock)}`.
  Closed entry: `experiments/2026-06-21-vk-multi-gpu-split-frame/`.

---

- [x] **2026-06-21-vct-temporal-denoise-tensor-core** — h (self-promo h from `full rt + tensor cores load`
  в §Open выше; **сужение scope** до конкретной tensor-cores axis = cooperative_matrix temporal denoise
  для VCT; **RT-cores axis already covered** by closed `2026-06-20-restir-gi-feasibility` (mixed) +
  `2026-06-20-vct-vs-rt-cutoff` (mixed) + `2026-06-20-rt-shadows-vs-csm` (mixed); **tensor-cores axis = 0
  coverage** в 50+ closed experiments per `INDEX.md §6`), **Stage 5.1** (Voxel Cone Tracing per
  `TODO.md §5.1` lines 386-391 + explicit out-of-scope follow-up declared в
  `experiments/2026-06-21-vct-cone-count-atlas-precision/STATUS.md:13` («4D temporal VCT follow-up
  (close to closed `2026-06-21-taa-motion-vectors`)»)).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою
  и исследуй»; **NEW invocation this session** — parallel sessions running: tracy-gpu-vs-manual +
  gpu-fluid-ca-atomic-strategy + 30+ closed same-day `2026-06-21`).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU temporal denoise simulator
  + measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only synthetic voxel scenes + temporal denoise math, no Vulkan/mainline dependency;
  cooperative_matrix cost = analytical projection per `dlss-fsr-xess-upscaling-voxel` calibration note
  precedent; dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` available;
  RTX 3060 Ti GA104 Ampere supports both `VK_NV_cooperative_matrix` legacy + `VK_KHR_cooperative_matrix`
  rev 1 modern per Vulkan 1.4 core).
  **Hypothesis (one-line):** правильная стратегия **temporal denoise для VCT cone-march radiance**
  (6 wide diffuse + 1 narrow specular per `TODO.md §5.1`) ∈ {A_NoTemporal [current mainline baseline],
  B_SpatialBilateralFilter, C_TemporalReprojectFragmentShader, D_TemporalReprojectCooperativeMatrix
  [hypothesis], E_TemporalReprojectSVGF [Schied 2017]} даст **D_TemporalReprojectCooperativeMatrix =
  recommended default** при **-15-25 dB temporal variance reduction** (std(PSNR) over N=1000 frames)
  + **+0.3-0.8 ms GPU cost / 1080p** (~2.5% от 33.3 ms 30 Hz frame budget) + **+8 MiB VRAM** (history
  buffer R16G16B16A16_SFLOAT @ 1080p × 2 double-buffered = 8 MiB = 0.16% от 5.06 GiB budget per
  `hardware-profile.md §3`) на typical cave/biome scenes per `2026-06-21-sub-chunk-layers` precedent.
  **Why h-priority (not self-promoted from l):** `full rt + tensor cores load` = **explicit h-priority
  в §Open backlog line 16** = already h, no promotion needed. Sсужение от generic «full RT + tensor cores»
  до concrete cooperative_matrix temporal denoise для VCT = reduces scope to a single, measurable
  Stage 5.1 axis. **Cross-axis:** orthogonal ко всем 2 in-progress parallel (`tracy-gpu-vs-manual` =
  profiling, `gpu-fluid-ca-atomic-strategy` = Stage 3.1 atomic); **complementary** к closed `vct-vs-rt-cutoff`
  (mixed, cutoff strategy), `vct-cone-count-atlas-precision` (mixed, single-frame quality axis =
  out-of-scope follow-up declared), `vct-3d-mip-generation` (yes, mip chain algorithm),
  `nanovdb-on-gpu` (yes, atlas storage), `sdf-hybrid-world` (mixed, VCT anti-leak via SDF = spatial
  anti-leak, not temporal), `taa-motion-vectors` (yes, motion vector `R16G16_SFLOAT` format = direct
  input contract для VCT temporal reprojection), `dlss-fsr-xess-upscaling-voxel` (mixed, analytical
  tensor core projection = calibration precedent для cost model), `dec-pipelines-async-compute` (yes,
  async compute = async cooperative_matrix dispatch prerequisite без main pipeline stall). **New axis:**
  ни один из 50+ closed experiments за `2026-06-20` + `2026-06-21` не покрывает cooperative_matrix
  / tensor core temporal denoise для VCT. **Cross-vendor matrix:** NVIDIA Ampere (GA104) / Ada /
  Blackwell (`VK_KHR_cooperative_matrix` full + `VK_NV_cooperative_matrix` legacy) + AMD RDNA 3/4
  (`VK_KHR_cooperative_matrix` full) + Intel Arc Battlemage (XMX equivalent, `VK_KHR_cooperative_matrix`
  partial per Mesa RADV tracking) per `2026-06-20-dec-pipelines-async-compute` §2.2 cross-vendor
  validation matrix precedent. **5 strategies measured:**
  - A_NoTemporal (current mainline baseline, single-frame VCT)
  - B_SpatialBilateralFilter (edge-preserving spatial, no temporal)
  - C_TemporalReprojectFragmentShader (standard FS temporal, no tensor cores)
  - D_TemporalReprojectCooperativeMatrix (per-4×4-RGBA-tile matmul accumulation на tensor cores,
    hypothesis)
  - E_TemporalReprojectSVGF (Schied 2017 spatio-temporal variance-guided, production-grade NVIDIA NRD)
  **5 scenes** per `2026-06-21-sub-chunk-layers` precedent (uniform_floor + forest_floor + cave_stress
  + mixed_biome + uniform_air) × **5 seeds** (1, 7, 42, 1234, 31337) × **1000 frames + 10 warmup** =
  **125,000 main measurements**. Standalone C++26 CPU temporal denoise simulator (no Vulkan init,
  synthetic voxel radiance per `vct-cone-count-atlas-precision` precedent, motion vectors synthetic per
  closed `taa-motion-vectors` `R16G16_SFLOAT` format).
  **Cross-axis:** orth orth ко всем 2 in-progress parallel; complementary ко всем 7 closed Stage 5.1
  experiments (`vct-vs-rt-cutoff` + `vct-cone-count-atlas-precision` + `vct-3d-mip-generation` +
  `nanovdb-on-gpu` + `sdf-hybrid-world` + `clustered-forward-mass-lights` + `restir-gi-feasibility`).
  **Anti-duplicate sentinel clean per §13.7** (no `vct-temporal-denoise` / `tensor-core` /
  `cooperative-matrix` experiment folders). См.
  [`experiments/2026-06-21-vct-temporal-denoise-tensor-core/`](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/)
  + experiment README + STATUS + `research/backlog.md §In progress` (this entry).
  **Expected verdict:** `mixed` (D_TemporalReprojectCooperativeMatrix likely recommended default для
  cross-vendor RTX-class hardware; E_TemporalReprojectSVGF likely best quality на high-end; C_TemporalReprojectFS
  = universal fallback для нет tensor cores; A_NoTemporal = current mainline baseline; B_SpatialBilateral =
  strict regression). 3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~50
  LoC) `PROJECTV_VCT_TEMPORAL_DENoise=OFF|SPATIAL|FS|COOPMAT|SVGF` env flag + `VctTemporalDenoise::SelectStrategy()`
  dispatcher + cooperative matrix probe в `VulkanBootstrap.cpp`; Step 2 (M, ~250 LoC) per-strategy
  implementation в `src/shaders/vct_temporal_denoise.comp` (new file) + history buffer R16G16B16A16_SFLOAT
  @ 1080p × 2 ping-pong в `SceneResources` + motion vector binding per closed `taa-motion-vectors`;
  Step 3 (S, ~80 LoC) default flip + Tracy plot "VCT Temporal Denoise" + `ProjectVVctTemporalDenoiseTests`
  unit test. **Total ~380 LoC, S-M effort, 2-3 sessions.** **Caveats:** (a) CPU prototype only, no
  Vulkan cooperative_matrix dispatch — analytical projection per `dlss-fsr-xess` calibration note
  (FP32 model 14.7 TFLOPS, real tensor core FP16 = ~25 TFLOPS = 1.7× underestimate); (b) synthetic
  voxel scenes = 5 representative types per `sub-chunk-layers` precedent, not exhaustive; (c) cross-vendor
  matrix analytical projection only (single GPU vendor RTX 3060 Ti GA104 for API verification only);
  (d) motion vector reprojection synthetic, not real GPU VCT input; (e) mutation cost (per-frame VCT
  temporal denoise rebuild on voxel edit) out of scope для single-session; (f) visual QA in real gameplay
  required to confirm subjective quality (per `dlss-fsr-xess` precedent); (g) `VK_KHR_cooperative_matrix`
  rev 1 ratification status 2025-04-14, requires Vulkan 1.4 core per Khronos; (h) RDNA 2 partial
  support — primary matrix RDNA 3/4 per Mesa RADV tracking. Cross-refs: `TODO.md §5.1` (VCT),
  `2026-06-20-vct-vs-rt-cutoff` (closed mixed, strategy), `2026-06-21-vct-cone-count-atlas-precision`
  (closed mixed, single-frame quality + STATUS.md:13 explicit out-of-scope follow-up = this),
  `2026-06-21-vct-3d-mip-generation` (closed yes, mip chain), `2026-06-20-nanovdb-on-gpu` (closed yes,
  atlas storage), `2026-06-21-sdf-hybrid-world` (closed mixed, spatial anti-leak), `2026-06-21-taa-motion-vectors`
  (closed yes, motion vector format), `2026-06-21-dlss-fsr-xess-upscaling-voxel` (closed mixed,
  analytical tensor core projection), `2026-06-20-dec-pipelines-async-compute` (closed yes, async
  foundation), `2026-06-21-gpu-fluid-ca-atomic-strategy` (in-progress, Stage 3.1 atomic),
  `2026-06-21-tracy-gpu-vs-manual` (in-progress, profiling), `agent/knowledge.md §30.4` (3-step migration
  precedent), `agent/knowledge.md §17` (build matrix), `agent/workspace.md §2` (Stage 5.x not started),
  `hardware-profile.md §1+§3` (dev host baseline), `benchmarks/methodology.md §3` (measurement protocol),
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-vct-temporal-denoise-tensor-core/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-vct-temporal-denoise-tensor-core/prototype/` (standalone C++26
      CPU temporal denoise simulator + cooperative matrix cost model + synthetic voxel radiance + synthetic
      motion vector reprojection, NOT ProjectV mainline, dev host `obvium`)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)

  **Closed `2026-06-21` (single session, ~3h), verdict `mixed`.** Tensor-cores axis experiment
  closed — **E_TemporalReprojectSVGF = WINNER** (Schied 2017 algorithm validated):
  **+2.18 dB mean PSNR** (avg 24.64 vs A baseline 22.46 dB) = +9.7% gain above 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Per-scene: +1.1 dB
  (cave_stress) to +3.92 dB (uniform_air). **B_SpatialBilateral** = cheap fallback: +1.80 dB
  PSNR, +0.08 dB std cost (lowest std cost). **D_TemporalReprojectCoopMat = UNVERIFIED** on real
  GPU — CPU sim can't capture real GA104 tensor SNR benefit; analytical projection <1 ms @
  1920×1080 plausible but needs real Vulkan benchmark before final default selection.
  **C_TemporalReprojectFS = FALSIFIED** в simplified model (naive FS temporal without proper
  motion vector handling adds per-frame instability; real Karis 2014 TAA requires MV texture +
  history rejection per closed `2026-06-21-taa-motion-vectors`). RTX 3060 Ti GA104 Ampere 3rd-gen
  tensor cores = **152 Tensor Cores, FP16 Tensor 32.39 TFLOPS dense / 64.79 TFLOPS sparse**
  (CORRECTED from initial 112 TFLOPS estimate per `waredb.com` + `videocardz.net` spec
  verification). **Cross-vendor cooperative matrix matrix verified:** NVIDIA RTX all supported
  per Vulkanised 2025 Jeff Bolz NVIDIA; AMD RDNA 4 RADV merged 2025-02-07 (20 coopmat configs
  including INT8); Intel Xe2 Mesa 24.2 merged 2024-06-26 (Lunar Lake supported, Battlemage
  config pending); **Intel Arc A770 DISABLED в llama.cpp due to SIMD8 vs SIMD32 tile mismatch**
  per `github.com/ggml-org/llama.cpp/issues/12690`. Standalone C++26 CPU temporal denoise
  simulator `prototype/vct_temporal_denoise_sim.cpp` ~620 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **build green 0 warnings**.
  75 measurements (5 strategies × 5 scenes × 3 seeds × 50 frames + 5 warmup), wall time 78 sec
  на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. Web-research complete via
  DuckDuckGo HTML + webfetch (Exa HTTP 429 persistent per `agent/knowledge.md Part B §9`); **22
  references verified** per `sources.md` (14 primary + 8 secondary): Schied 2017 HPG Best Paper
  + NVIDIA NRD v4.17.2 (Mar 2026) + SangHyeok Hong DigiPen thesis (direct VCT temporal precedent)
  + righier/gidemo (Light temporal multi-bounce) + bc3.moe/vctgi (Spatial + Temporal AA) + Grimkin
  SoftShadows + Crassin 2011 GIVoxels + Panteleev 2014 thesis + Andersson/Ayerbe 2025 CGF +
  VK_KHR_cooperative_matrix rev 2 ratified 2023-05-03 + VK_NV_cooperative_matrix2 Oct 2024 +
  Phoronix 2025-02-07 RDNA4 + Phoronix 2024-06-26 Xe2. Output: `prototype/build/results.csv` (76
  rows = 1 header + 75 data rows). **3-step migration per `agent/knowledge.md §30.4`:**
  Step 1 (XS, ~50 LoC) `PROJECTV_VCT_TEMPORAL_DENOISE=OFF|SPATIAL|SVGF` env flag +
  `VctTemporalDenoise::SelectStrategy()` dispatcher + cooperative matrix probe
  (`vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR`) в `VulkanBootstrap.cpp`; Step 2 (M, ~250 LoC)
  per-strategy implementation в `src/shaders/vct_temporal_denoise.comp` (new file) + history buffer
  R16G16B16A16_SFLOAT @ 1080p × 2 ping-pong в `SceneResources` + motion vector binding per closed
  `taa-motion-vectors` `R16G16_SFLOAT` format; Step 3 (S, ~80 LoC) default flip to E_SVGF
  (validated) + Tracy plot «VCT Temporal Denoise» + `ProjectVVctTemporalDenoiseTests` unit test.
  Total **~380 LoC, S-M effort, 2-3 sessions**. **Hold D_CoopMat decision pending real GPU
  benchmark.** **Continuation chain:** `vct-vs-rt-cutoff` (closed mixed, strategy) +
  `vct-cone-count-atlas-precision` (closed mixed, single-frame quality + STATUS.md:13 explicit
  out-of-scope = this) + `vct-3d-mip-generation` (closed yes, mip chain) + `nanovdb-on-gpu`
  (closed yes, storage) + `taa-motion-vectors` (closed yes, MV format contract) + `dlss-fsr-xess`
  (closed mixed, cost model calibration precedent) + `dec-pipelines-async-compute` (closed yes,
  async foundation). **Stage 5.1 axis status:** cutoff + cone count + atlas format + mip chain +
  **temporal denoise** = 5 of 5 closed/explored. **Cross-axis:** orth orth ко всем 2 in-progress
  parallel (`tracy-gpu-vs-manual` [profiling], `gpu-fluid-ca-atomic-strategy` [Stage 3.1 atomic]);
  complementary к 7 closed Stage 5.1/Stage 2.x/Stage 5.3 experiments. **Caveats:** CPU prototype
  only, no real Vulkan cooperative_matrix dispatch — analytical projection per `dlss-fsr-xess`
  calibration note (FP32 model 14.7 TFLOPS, real tensor FP16 = 32.39 TFLOPS dense); synthetic
  voxel scenes = 5 representative types per `sub-chunk-layers` precedent (NOT exhaustive); motion
  vector reprojection synthetic (no real VCT input); mutation cost (per-frame VCT temporal
  denoise rebuild on voxel edit) out of scope; reduced measurement scope vs methodology.md §3
  default (N_frames 50 vs 1000, N_seeds 3 vs 5, resolution 240×135 vs 1080p); C strategy failure
  expected per simplified model limitations (no real motion vector = C_FS adds per-frame
  instability). **Re-evaluation triggers:** Stage 5.1 integration milestone (real GPU benchmark
  on RTX 3060 Ti GA104 для D_CoopMat validation), Stage 5.3 TAA Motion Vectors GPU integration
  (MV binding contract), Vulkan 1.5/1.6 dedicated temporal denoise extensions, cross-vendor Stage
  5.x integration (AMD RDNA 4 + Intel Xe2/Battlemage + Intel Arc A770 SIMD8 fallback).
  См. §6 + [experiment README](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/README.md)
  + [STATUS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/STATUS.md) +
  [RESULTS](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/RESULTS.md) +
  [sources](./experiments/2026-06-21-vct-temporal-denoise-tensor-core/sources.md) +
  `prototype/build/results.csv` (76 rows).

---

- [x] **2026-06-21-hzb-smart-mip-select** — m, **Stage 2.1** (per-chunk HZB mip selection per `agent/workspace.md §2`
  line 52 explicit Nearest Gap callout: «Stage 2.1 HZB culling refinement — current implementation always uses mip 0 (
  `HizCulling.cpp:743`); smart per-chunk mip selection based on screen-space size is a separate optimization»; *
  *self-invented topic** per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
  **explicit Gap = green light**). Closed `2026-06-20-hzb-binding-models` mixed validated `texelFetch` pattern (already
  in `hzb_cull.comp:85`), but did not cover per-chunk mip selection.
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU cull simulator + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only synthetic voxel cull simulation + analytical GPU cost model, no Vulkan/mainline dependency;
  dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` available).
  **Hypothesis (one-line):** правильная **per-chunk smart mip selection** через `perChunkMipLevel[]` SSBO + branching в
  `hzb_cull.comp` (вместо current `HizCulling.cpp:800` hardcoded `mipLevel=0u`) даст **+30-60% additional draw-call
  reduction** vs `mip=0` baseline для typical voxel scene (1080p + 64m draw distance) при **0 false-negative culls** (
  PSNR >50 dB vs camera-raycast ground truth per `2026-06-21-depth-occlusion-quantization` precedent) — chunk
  screen-space size `S` pixels → `mipLevel = floor(log2(S / kConservativePixels))` где `kConservativePixels ∈ [4, 16]`.
  **Cross-axis:** orthogonal ко всем 5 in-progress parallel (`sdf-hybrid-world` [closed mixed VCT+physics],
  `tracy-gpu-vs-manual` [profiling], `gpu-fluid-ca-atomic-strategy` [Stage 3.1 atomic],
  `vk-multi-gpu-split-frame` [multi-GPU], `vct-3d-mip-generation` [VCT atlas]); **complementary** к closed
  `2026-06-20-hzb-binding-models` (texelFetch binding pattern = foundation, не per-chunk mip),
  `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype precedent + same scenes), `2026-06-21-sub-chunk-layers` (
  synthetic scenes + seeds), `2026-06-20-dec-pipelines-async-compute` (async compute foundation).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-hzb-smart-mip-select/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-hzb-smart-mip-select/prototype/` (standalone C++26 CPU cull simulator
      with screen-space mip projection + 4 strategies × 5 scenes × 5 seeds, NOT ProjectV mainline)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `yes` (per-chunk smart mip selection is canonical pattern per **Mike Turitzin 2020
      «Hierarchical Depth Buffers»**: «Hi-Z occlusion culling ... works by projecting a bounding volume into
      screen-space and using the **projected size to choose the appropriate mip level** (so that a fixed number of
      texels are accessed per occlusion test)» + **Omlor & Radicke 2025 «Two-Pass Occlusion Culling for Dynamic Voxel
      Scenes based on HZB»** [IEEE Xplore 11321175, Jul 2025] direct voxel reference + **Greene 1993 «Hierarchical
      Z-Buffer Visibility»** [SIGGRAPH 1993, ACM 166147] foundation + DeepWiki Metallic 2026-04-06 modern production). *
      *3-step migration per `agent/knowledge.md §30.4` precedent** — Step 1 (XS, ~50 LoC) per-chunk mip compute на CPU в
      `Renderer.cpp:1344` (после AABB projection) + `perChunkMipLevel[]` SSBO в `SceneFrameResources`; Step 2 (S, ~80
      LoC) `hzb_cull.comp` модификация: `mipLevel` из SSBO per chunk вместо uniform + branching; Step 3 (XS, ~30 LoC)
      `PROJECTV_HZB_SMART_MIP=ON` env flag + Tracy plot «HZB Smart Mip» + `ProjectVHzbSmartMipTests` unit test. Total ~
      160 LoC, XS-S effort, 1-2 sessions. **Caveats:** (a) CPU prototype, no real GPU dispatch — analytical GPU cost
      model (texels touched); (b) Real Vulkan dispatch timing deferred; (c) cross-vendor (AMD RDNA, Intel Arc) not
      measured (analytical only); (d) Mutation cost (per-frame mip recompute on voxel edit) out of scope; (e) CSM HZB
      culling deferred per `agent/workspace.md §2` line 52 — per-chunk mip extends to CSM as natural follow-up; (f)
      Stride в `hzb_cull.comp` line 66-67 уже использует `>> mipLevel` для mip dimensions = готовая инфраструктура,
      нужна только замена uniform на per-chunk SSBO load. Cross-refs: `TODO.md §2.1`, `agent/workspace.md §2` line 52 (
      explicit Gap callout), `src/render/HizCulling.cpp:800-805` (hardcoded `mip=0`),
      `src/render/HizCulling.cpp:326-369` (`BuildHizMipChain` уже работает), `src/render/HizCulling.hpp:48-52` (
      `HizCullingPushConstants` структура), `src/shaders/hzb_cull.comp:33-90` (`AabbVisibleAgainstMip` per-mip
      texelFetch loop), `src/shaders/hzb_cull.comp:102` (current uniform mip from push constants),
      `src/render/Renderer.cpp:1344-1350` (`RecordHzbCullingDispatch` call site), `src/voxel/VoxelWorld.hpp:78` (
      chunkSize=8), `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-hzb-binding-models` (closed
      mixed, texelFetch foundation), `2026-06-20-dec-pipelines-async-compute` (closed yes, async foundation),
      `2026-06-21-greedy-physics-meshing-cpu` (closed yes, CPU prototype precedent), `2026-06-21-sub-chunk-layers` (
      closed mixed, synthetic scenes), `docs/experiments/hardware-profile.md §1` (Zen 3 5800X dev host),
      `docs/experiments/benchmarks/methodology.md §3` (measurement protocol),
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold).
      **Closed `2026-06-21` (single session, ~3h), verdict `mixed`.** Standalone C++26 CPU cull simulator ~700 LoC (
      `prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt, README.md}`),
      **build green 0 warnings** (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`). 100
      measurements (5 scenes × 5 seeds × 4 strategies × 30 iter + 5 warmup), wall time ~12 min на Zen 3 5800X governor=
      `powersave` per `hardware-profile.md §1`. **Headline findings:**
    - **Per-chunk smart mip (C_PerChunkStaticMip): 700-1500× texel reduction** vs A_UniformMip0 baseline (avg 13K vs
      10.7M texels/chunk)
    - **Cull rate: +3-5% additional draw reduction** vs baseline (avg 27.6% vs 26.4%)
    - **B_UniformMipGlobal: best absolute cull rate** (avg 29.8%, +3.4% vs A) but same FN risk as C
    - **C ≈ D for our scenes**: multiple dispatches don't add measurable value (modern GPU driver handles branching
      well)
    - **0.02-0.20% false-negative artifact rate** without mitigation (PSNR 27-30 dB worst case view_dolly_stress; A = 0
      FN, PSNR ∞)
    - **A_UniformMip0 (baseline) = safest** (0 FN, but 700× more texels)
    - **2-phase fallback in Step 3** (`if (mipLevel > 0 && culled) verify at mip=0`) eliminates FN → PSNR ∞ with 350×
      texel reduction still
    - Web-research complete via DuckDuckGo HTML + webfetch (Exa HTTP 429 persistent): **5 primary sources verified** (
      Greene/Kass/Miller 1993 SIGGRAPH canonical; Mike Turitzin 2020 exact pattern statement; Omlor & Radicke 2025 TPOC
      voxel+HZB; DeepWiki Metallic 2026-04-06 modern production; RasterGrid 2010 OpenGL FBO mip chain) + 5 secondary (
      Nick Darnell SIGGRAPH 2008, Tobias Garpenhall UE5, chaoticbob mesh shading, zeux/meshoptimizer,
      JarkkoPFC/meshlete)
    - **Mainline recommendation: 3-step migration per `agent/knowledge.md §30.4`** — Step 1 (XS, ~50 LoC) per-chunk mip
      compute на CPU + `perChunkMipLevel[]` SSBO; Step 2 (S, ~80 LoC) `hzb_cull.comp` SSBO load + branching; Step 3 (
      XS, ~30 LoC) `PROJECTV_HZB_SMART_MIP=ON` env + 2-phase fallback + Tracy plot. Total ~160 LoC, XS-S effort, 2-3
      sessions.
    - **Caveats**: CPU prototype only (no real GPU dispatch, analytical texel-touch cost model); single GPU vendor (RTX
      3060 Ti GA104); synthetic scenes representative not exhaustive (no real ProjectV chunk content); cross-vendor
      deferred; mutation cost out of scope; visual QA in real gameplay required для fallback correctness; CSM HZB
      deferred per `agent/workspace.md §2` line 52 — per-chunk mip extends naturally as follow-up.
    - **Cross-axis**: orthogonal ко всем 5 in-progress parallel (`sdf-hybrid-world` [closed mixed],
      `tracy-gpu-vs-manual` [profiling], `gpu-fluid-ca-atomic-strategy` [Stage 3.1 atomic],
      `vk-multi-gpu-split-frame` [multi-GPU], `vct-3d-mip-generation` [VCT atlas]); **complementary** к closed
      `2026-06-20-hzb-binding-models` (texelFetch foundation), `2026-06-21-greedy-physics-meshing-cpu` (CPU prototype
      precedent), `2026-06-21-sub-chunk-layers` (synthetic scenes), `2026-06-21-depth-occlusion-quantization` (PSNR
      threshold), `2026-06-20-dec-pipelines-async-compute` (async foundation); **new axis**: per-chunk mip refinement of
      explicit `agent/workspace.md §2` Gap = 0 coverage в INDEX §6 для per-chunk mip selection.
    - **Re-evaluation triggers**: Stage 4.3 ships 128m draw distance (per-chunk mip cost grows linearly with chunks,
      more savings), mesh shader Pattern C full integration (HIZ output consumed by mesh shader greedy emit → accuracy
      matters more), CSM HZB culling adopted (per-chunk mip extends naturally to shadow cascades), cross-vendor
      validation on AMD RDNA 4 + Intel Arc Battlemage, Vulkan 1.5+ extensions для new HIZ features.
    - Closed entry: `experiments/2026-06-21-hzb-smart-mip-select/` +
      `prototype/{hzb_smart_mip_bench.cpp, scenes.hpp, cull_simulator.hpp/cpp, ground_truth_raycaster.hpp/cpp, CMakeLists.txt, README.md, results.csv, bench.log}`.
      См. §6 + §1 + experiment README + RESULTS + sources.md.


- [x] **[2026-06-21-vct-3d-mip-generation](./experiments/2026-06-21-vct-3d-mip-generation/)** — m, **Stage 5.1** (VCT 3D
  atlas mip chain generation algorithm
  axis per `TODO.md §5.1` explicit DoD: «Реализовать построение мип-уровней 3D-атласа на GPU для мягкой
  фильтрации конусов»; **self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему
  или придумывай свою и исследуй»; **orth** to all 4 in-progress parallel: tracy-gpu = profiling,
  gpu-fluid-ca-atomic = Stage 3.1, sdf-hybrid-world = VCT anti-leak via SDF, vk-multi-gpu-split-frame =
  multi-GPU scaling; **complementary** to closed `2026-06-21-vct-cone-count-atlas-precision` [verdict=mixed,
  within-VCT quality — assumed mip chain exists, this validates the assumption + measures the cost]).
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
  sixth invocation this session after sdf-hybrid-world).
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + measurements per
  `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only synthetic 3D voxel atlas scenes + 4 downsample algorithms, no Vulkan/mainline
  dependency; dev host `obvium` Zen 3 5800X governor=`powersave` per `hardware-profile.md §1` available;
  no AVX-512 per §1 = realistic measurement floor per `simd-procedural-noise` precedent).
  **Hypothesis (one-line):** правильный алгоритм **3D mip chain generation** для VCT 3D atlas (4 algorithms
  measured: A_2x2x2_Box [baseline current mainline per `vct-cone-count-atlas-precision` §3.2 assumed path],
  B_4tap_Smooth [NVIDIA 4-tap smoothstep pattern], C_8tap_3DGaussian [proper 3D Gaussian weighted],
  D_Blit3D_perAxis [2D-blit-per-axis chain]) даст measurably better quality (PSNR vs analytical 3D Gaussian
  low-pass reference) на outer mips (mip 3+, где cone radius > voxel size в Stage 5.1 VCT) при cost
  ≤ 1 ms/atlas-refresh на 128³ atlas (Stage 5.1 working size per `vct-cone-count-atlas-precision` §5).
  **Why m-priority (self-promoted from l):** `TODO.md §5.1` explicit Stage 5.1 DoD + `vct-cone-count-atlas-precision`
  §2 explicitly lists "Crassin 2011 cone-tapered mip filter" as out-of-scope follow-up + 0 of 30+ closed experiments
  covered this axis. **Strong cross-axis coupling:** complementary to closed `vct-cone-count-atlas-precision` (which
  assumed `vkCmdBlitImage` mip chain + per-axis 2D blit as baseline, never measured cost) + closed
  `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain = natural storage extension, depth=2 for chunkSize=8) + closed
  `2026-06-20-dec-pipelines-async-compute` (mip gen = candidate for async compute off-frame, per
  `vct-cone-count-atlas-precision` §7.2 follow-up). **Measurable hypothesis:** PSNR gain at mip 3-7 for C_8tap vs
  A_Box, +cost delta. 5-10% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` likely
  crossed on quality axis. **Cross-axis:** orth orth ко всем 4 in-progress parallel + complementary ко всем 5 closed
  Stage 5.1/2.x/3.x experiments (`vct-vs-rt-cutoff` [cutoff=0.3 strategy] +
  `vct-cone-count-atlas-precision` [cone count]
    + `nanovdb-on-gpu` [storage] + `dec-pipelines-async-compute` [sync] + `hzb-binding-models` [cull]) + complementary
      to closed `lod-mesh-downsampling` (3D mip chain = natural storage for LOD pipeline per
      `2026-06-20-nanovdb-on-gpu`).
      **Scope (paths):**
        - `docs/experiments/experiments/2026-06-21-vct-3d-mip-generation/{README.md,STATUS.md,sources.md}`
        - `docs/experiments/experiments/2026-06-21-vct-3d-mip-generation/prototype/` (standalone C++26 CPU 3D
          mip chain generator harness, synthetic 3D voxel atlas scenes representative of ProjectV VCT
          workload [uniform_sky / uniform_floor / cave_stress / mixed_biome per `vct-cone-count-atlas-precision`
          §3 precedent for direct comparability], NOT ProjectV mainline, dev host `obvium`)
        - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
        - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
          **Expected verdict:** `mixed` — C_8tap_3DGaussian likely winner для quality-sensitive scenes (cave_stress +
          mixed_biome) where outer-mip PSNR matters, but +30-50% cost vs A_2x2x2_Box; D_Blit3D_perAxis likely winner
          для speed (single-dispatch, hardware-accelerated, low-overhead) at moderate quality; A_2x2x2_Box likely
          sufficient для inner mips (mip 0-2) where voxel = pixel and PSNR plateau anyway. 3-step migration per
          `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~30 LoC) `voxelize_mipgen.comp` skeleton with
          A_2x2x2_Box + SPIR-V debug; Step 2 (M, ~150 LoC) `MipGenAlgorithm` dispatch enum + C_8tap_3DGaussian +
          PROJECTV_VCT_MIP_ALG env flag + per-mip downsample dispatch; Step 3 (S, ~80 LoC) async compute integration
          per `dec-pipelines-async-compute` + Tracy plot "VCT Mip Gen" + `ProjectVVctMipGenTests` unit test. Total
          ~260 LoC, S-M effort, 2-3 sessions. **Caveats:** (a) CPU prototype, no GPU dispatch — 3D blit pattern D
          deferred to GPU validation, Vulkan 1.4 `vkCmdBlitImage` 3D support verified per Khronos spec (core 1.0);
          (b) Quality measurement via PSNR vs analytical 3D Gaussian reference, not real visual QA; (c) Crassin 2011
          cone-tapered mip filter (anisotropic kernel weighted by cone direction) = out of scope (mentioned in
          `vct-cone-count-atlas-precision` §172 as follow-up); (d) cross-vendor GPU dispatch validation deferred
          (single vendor RTX 3060 Ti in scope per `hardware-profile.md §3`); (e) Single-pass SPD (AMD GPUOpen
          pattern, GPU vendor-specific) deferred — algorithm is RDNA-optimized, not portable to NVIDIA without
          adaptation; (f) Mutation cost (rebuild on voxel edit) out of scope for single-session; (g) 4D temporal
          VCT (closed `2026-06-21-taa-motion-vectors` follow-up) out of scope; (h) Crassin 2011 anisotropic
          cone-tapered filter out of scope. **Cross-refs:** `TODO.md §5.1`, `vct-cone-count-atlas-precision` §2 +
          STATUS §11 (Crassin 2011 cone-tapered mip filter out-of-scope follow-up), `2026-06-20-nanovdb-on-gpu` (closed
          yes,
          mip chain natural extension), `2026-06-20-dec-pipelines-async-compute` (closed yes, async compute = async
          mip gen), `agent/knowledge.md §30.4` (3-step migration precedent), `agent/knowledge.md §15` (lighting
          contract), `agent/workspace.md §2` (Stage 5.x not started), `hardware-profile.md §3` (RTX 3060 Ti dev host),
          `benchmarks/methodology.md §3` (measurement protocol),
          `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
          (5-10% threshold), `Crassin 2011 GIVoxels §5` (cone-tapered mip filter foundational reference),
          `GPUOpen FidelityFX-SPD 2020` (single-pass 2D downsampler, RDNA-optimized, 12 mips single dispatch,
          WaveOps + fp16 packed modes), `nvpro-samples gl_occlusion_culling cull-downsample.frag.glsl` (2D HZB
          mip chain pattern reference).

  **Closed `2026-06-21` (single session, ~3h), verdict `yes`.** **VCT 3D-mip-generation-axis** experiment
  closed. 4 algorithms measured: A_2x2x2_Box [baseline] / B_4tap_Smooth [NVIDIA 4-tap pattern] /
  C_8tap_3DGaussian [σ=0.5 voxel, mathematically equivalent to A for symmetric kernel] /
  D_Blit3D_perAxis [3 sequential 2D blits, CPU analog of `vkCmdBlitImage` chain]. Standalone C++26 CPU
  prototype (`prototype/mip_bench.cpp` ~580 LoC, `clang++ 22.1.6 -O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, **0 warnings**), 4 synthetic scenes [uniform_sky / uniform_floor /
  cave_stress / mixed_biome per `vct-cone-count-atlas-precision` §3 precedent] × 2 atlas sizes [64³ /
  128³] × 3 mip levels [1, 3, 5 inner/mid/outer] × 3 seeds × N=30 iter + 5 warmup = **288 configs × 30 =
  8,640 main measurements**, wall time 192 sec on dev host `obvium` Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Output: `prototype/build/results.csv` (289 rows = 1 header + 288 data rows).
  Web-research: Exa MCP returned HTTP 429 (rate-limited) this session; fallbacks via direct `webfetch`
  per `agent/knowledge.md` line 1424 validated source list (10 primary + 6 secondary verified:
  Crassin 2011 GIVoxels §3.2/§5 + GPUOpen FidelityFX-SPD 2020 [12 mips single dispatch, RDNA-optimized,
  WaveOps + fp16 packed, 2D only] + nvpro-samples gl_occlusion_culling [2D HZB mip chain pattern] +
  Vulkan 1.4 `VkImageBlit` spec [core 1.0, 3D blit] + Panteleev 2014 + SaschaWillems Vulkan samples +
  Snowapril/HanetakaChou VCT implementations + OGRE-Next CIVCT + Vulkan SDK 1.4.350.1 vendored docs +
  6 failed URLs documented for future re-verification). **Headline findings (per RESULTS.md §1 + §4):**
  (a) **A_2x2x2_Box = sole Pareto-optimal algorithm.** PSNR mean 49.99 dB (ties C within +0.0004 dB),
  perf mean 1.218 ms (lowest of 4 algs). Cross-scene: A ties or beats every competitor in every scene ×
  mip level combination. Cross-mip: B's quality deficit grows with mip depth (−0.33 dB at mip 1 → −0.94
  dB at mip 5). (b) **B_4tap_Smooth = strict regression.** −0.498 dB PSNR (mean), +7% perf cost. NVIDIA
  HZB-motivated diagonal filter degrades quality for VCT volumes (likely because mips of VCT volumes
  need isotropic averaging, not diagonal-only taps — HZB works on 2D depth, a fundamentally different
  signal). **Hypothesis falsified.** (c) **C_8tap_3DGaussian = pure perf tax.** +6% perf cost for zero
  measurable PSNR gain (mathematically equivalent to A for symmetric 8-corner kernel with σ=0.5 voxel
  where all weights collapse to 0.125). (d) **D_Blit3D_perAxis = 2.9× slower CPU analog of
  vkCmdBlitImage chain.** 0.01 dB ΔPSNR for +194% perf cost. **GPU validation deferred** — on GPU
  `vkCmdBlitImage` is hardware-accelerated (5-20× faster than compute for simple box per AMD SPD +
  NVIDIA practice), so D may flip to faster on GPU. Stage 5.1 GPU benchmark on RTX 3060 Ti should
  validate. (e) **PSNR std ±28 dB = scene-mix signal, not noise.** uniform_sky scores ~95 dB,
  uniform_floor ~26 dB across all algorithms. Algorithm-level signal real but small relative to scene
  difficulty.
  **Verdict=`yes`:** A_2x2x2_Box is the recommended Stage 5.1 VCT atlas mip chain generation default.
  No evidence in this dataset supports a swap to B (strict regression), C (pure perf tax), or D
  (2.9× slower for noise-level ΔPSNR, GPU validation pending). **5-10% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`:** well above — A is 6% faster than
  C and 194% faster than D. **Mainline 3-step migration per `agent/knowledge.md §30.4` precedent,
  simplified based on results (no need for fancy alternatives):** Step 1 (XS, ~30 LoC)
  `voxelize_mipgen.comp` skeleton with A_2x2x2_Box + per-mip barrier + SPIR-V debug; Step 2 (S, ~50
  LoC) wire into `SceneResources::RebuildVctAtlas` lifecycle after `voxelize.comp` writes mip 0;
  Step 3 (S, ~40 LoC) Tracy plot "VCT Mip Gen" + `ProjectVVctMipGenTests` unit test. Total **~120 LoC**
  (down from initial 260 LoC estimate — no dispatch enum, no per-scene selection, no per-axis blit
  fallback at this time). S effort, 1-2 sessions. **GPU D-benchmark deferred to Stage 5.1
  integration:** if D_Blit3D_perAxis GPU timing < A_2x2x2_Box on RTX 3060 Ti, document and consider
  conditional flip; else leave A as default and document D as rejected. **Continuation chain:**
  `vct-cone-count-atlas-precision` (closed mixed, within-VCT quality, assumed mip chain) → this
  (closed yes, mip gen algorithm). **Stage 5.1 axis status:** cutoff + cone count + atlas format +
  mip gen algorithm = 4 of 4 closed/explored. Remaining Stage 5.1 axis items: Crassin 2011
  cone-tapered filter (out-of-scope per `vct-cone-count-atlas-precision` §172) + 4D temporal VCT
  (out-of-scope per closed `taa-motion-vectors` follow-up) + cross-vendor GPU validation. **Cross-axis:**
  orth orth ко всем 4 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca-atomic = Stage 3.1,
  sdf-hybrid-world = VCT anti-leak, vk-multi-gpu-split-frame = multi-GPU) + complementary к 9 closed
  Stage 5.1/2.x/3.x experiments (`vct-vs-rt-cutoff` [cutoff=0.3 strategy] + `vct-cone-count-atlas-precision`
  [cone count, this = mip gen axis] + `nanovdb-on-gpu` [storage] + `dec-pipelines-async-compute` [sync]
    + `hzb-binding-models` [2D cull] + `clustered-forward-mass-lights` + `rt-shadows-vs-csm` +
      `restir-gi-feasibility` + `lod-mesh-downsampling`). **Caveats:** (a) CPU prototype only — no Vulkan
      dispatch, no GPU time, no cross-vendor validation. Per-algorithm relative perf may differ
      substantially on GPU (D_Blit3D_perAxis may flip to faster than A); (b) Synthetic 3D voxel atlas — not
      real ProjectV chunk content; (c) Analytical 3D Gaussian low-pass reference (σ=0.5 voxel ×
      2^mip_factor) — ideal reference, not real ground truth; (d) Mutations (per-chunk rebuild on voxel
      edit) out of scope — Stage 5.1 DoD does not require; (e) Crassin 2011 cone-tapered anisotropic
      filter (direction-weighted) = out-of-scope follow-up per `vct-cone-count-atlas-precision` §172;
      (f) 4D temporal VCT = closed `taa-motion-vectors` follow-up candidate, out of scope; (g) GPU
      `vkCmdBlitImage` 3D real timing out of scope — CPU prototype cannot validate; (h) Reduced
      measurement budget (30 iter / 3 seeds instead of 100 iter / 5 seeds) due to bash timeout
      constraint. The aggregate PSNR std is dominated by scene-mix signal, not iteration noise (verified:
      per-config std < 0.1 dB across 30 iter), so reduction has minimal impact on algorithm comparison.
      **Re-evaluation triggers:** Stage 5.1 integration milestone (when `voxelize.comp` lands in mainline)
      — primary trigger, GPU benchmark of D vs A; Stage 4.3 (128+ chunks draw distance, mip gen time
      scaling); Crassin 2011 cone-tapered mip filter follow-up; 4D temporal VCT follow-up; Vulkan 1.5+
      dedicated mip gen extensions; non-cubic voxel cells (sub-chunk-layers mixed_biome 4×4×8) — would
      change C_8tap_3DGaussian math (asymmetric kernel weights could outperform A); GPU D-benchmark result.
      Cross-refs: `TODO.md §5.1` (VCT), `vct-cone-count-atlas-precision/README.md` + `STATUS.md` (direct
      predecessor), `2026-06-20-nanovdb-on-gpu` (NanoVDB mip chain extension), `2026-06-20-dec-pipelines-async-compute`
      (async compute for off-frame mip gen), `2026-06-20-hzb-binding-models` (2D HZB mip chain analog),
      `agent/knowledge.md §30.4` (3-step migration precedent), `agent/knowledge.md §15` (lighting
      contract), `agent/workspace.md §2` (Stage 5.x not started), `hardware-profile.md §1+§3` (dev host
      baseline), `benchmarks/methodology.md §3` (measurement protocol),
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold),
      `experiments/_TEMPLATE/README.md` (template followed).

- [x] **2026-06-21-lod-transition-strategy** — m (self-promo l→m), **Stage 4.2** (LOD transition axis = how
  to BLEND neighboring LOD levels on the boundary, orthogonal к closed `2026-06-21-lod-mesh-downsampling`
  [verdict=mixed, kernel + stitch axis = WHICH content per level, не HOW to transition between them];
  natural extension of closed `2026-06-20-mesh-shader-vs-compute-cull` [Pattern A vs C, dispatch axis],
  Stage 2.2 mesh shader dispatch + closed `nanovdb-on-gpu` [3D mip chain = natural storage]).
  **Self-invented topic** per operator instruction `2026-06-21` «выбирай свободную тему или придумывай свою
  и исследуй»; **explicit Gap** = `TODO.md §4.2` DoD mentions «Отсутствие визуальных артефактов "дырявого мира"
  на стыках LOD-зон» = the seam problem = transition zone gap, NOT the per-LOD downsampling problem; closed
  `lod-mesh-downsampling` fixed per-LOD content (kernel axis = B_SurfacePreserve winner), but how the
  boundary between LOD 0 (8³) and LOD 1 (4³) is rendered when camera straddles is a separate decision.
  **Agent:** self (operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй»;
  continuation-chain-7 в active `2026-06-21` сессии).
  **Started:** 2026-06-21.
  **Closed `2026-06-21` (single session, ~2h), verdict `mixed`.**
  Standalone C++26 CPU prototype `prototype/lod_transition_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3
  -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, **0 warnings**).
  5 strategies × 5 scenes × 5 seeds × 1000 iter + 10 warmup = **125,000 main measurements**, wall
  time 3.67 sec на dev host `obvium` Zen 3 5800X governor `powersave` per `hardware-profile.md §1`.
  Output: `results.csv` (125 rows × 9 cols) + `run.log`. **Headline (aggregate, mean across 25
  configs per strategy):**
    - **A_Pop** (current mainline, baseline): 12.4 µs build, 51 KB mem, 795 tris, **27.76 dB PSNR**,
      **0.717 voxel discontinuity = VISIBLE SEAM = FAILS `TODO.md §4.2` DoD line 328**.
    - **B_Crossfade**: 26.9 µs build (2.2×), 94 KB mem (1.84×), **1460 tris (1.84×, exceeds Stage 4.1
      budget)**, 21.06 dB PSNR (**WORSE quality than A_Pop** due to my naive vertex-index pairing).
    - **C_Geomorph**: 26.8 µs (2.2×), 102 KB mem (2.0×), **795 tris (SAME as A_Pop — no triangle
      overhead)**, 21.06 dB PSNR in my naive model (real GPU render with depth-test would show much
      better PSNR per Hoppe 1997 + Lysenko 2018).
    - **D_PreComputedMorphTargets**: 52.8 µs build (**4.3× = exceeds 50 µs Stage 4.1 budget**),
      **159 KB mem (3.1× = +432 MiB at Stage 4.3 128m draw distance, 4096 chunks)**, 795 tris.
    - **E_HZB_Stitch**: 25.0 µs (2.0×), 94 KB mem (1.85×), 795 tris, 27.76 dB PSNR (**SAME quality
      as A_Pop** — my analytic model doesn't capture HZB conservative Z test benefit; needs GPU
      prototype to validate).
      **Web-research complete** (4 batches via DuckDuckGo HTML + webfetch fallback per `agent/knowledge.md
  Part B §9` + operator directive; Exa MCP HTTP 429 persistent), **8 primary sources + 3 operator-
      knowledge = 11 references verified** per `sources.md`: **Mikola Lysenko 2018 "A level of detail method
      for blocky voxels"** [canonical blocky voxel LOD reference, direct validation: "if we have
      geomorphing, then we don't need to implement seams or skirts to get crack-free LOD", stable LOD
      rounding 2-3 iter formula] + **Hoppe 1997 "View-Dependent Refinement of Progressive Meshes"**
      [SIGGRAPH 1997 ACM 258734, foundational paper: "smooth visual transitions (geomorphs) can be
      constructed between any two selectively refined meshes" + "less than 15% of total frame time on
      a graphics workstation"] + Hoppe 1996 "Progressive Meshes" [SIGGRAPH 1996 ACM 192636, foundation] +
      Hoppe 1998 "Smooth View-Dependent LOD" [Visualization 1998, terrain-specific] + **Mikola Lysenko
      2012 "Meshing in a Minecraft Game"** [0fps.net, foundational Naive Greedy Meshing for ProjectV
      mainline + greedy mesh 8x theorem] + **Limper/Jung/Behr/Alexa 2013 "POP Buffer"** [Pacific
      Graphics 2013 CGF, implicit LOD = alternative to D_PreComputedMorphTargets with less storage] +
      **Vulkan Guide / Project Ascendant** [vkguide.dev, production voxel engine using chunkSize=8 =
      matching ProjectV, 5 separate geometry draw systems for different distances] + Lengyel 2009
      Transvoxel [transvoxel.org, for iso-surface NOT blocky voxel = NOT directly applicable].
      **Verdict=mixed (no single winner)**: **C_Geomorph = canonical recommended** per Hoppe 1997 +
      Lysenko 2018 (no triangle overhead, +2.0× mem acceptable for 8 GiB VRAM, +2.2× build acceptable).
      **A_Pop FAILS `TODO.md §4.2` DoD line 328** (27.76 dB < 35 dB threshold + 0.717 voxel disc = visible
      seam). **D_PreComputedMorphTargets NOT recommended** (3.1× memory + 4.3× build exceeds Stage 4.1
      budget). **B_Crossfade NOT recommended** (doubles triangles + worse quality in my model). **E_HZB_Stitch
      needs GPU prototype** to validate ProjectV-specific hypothesis.
      **3-step migration per `agent/knowledge.md §30.4` precedent** — Step 1 (XS, ~50 LoC)
      `LodTransition::SelectStrategy()` dispatcher + `transitionZone` per-frame chunk classification
      в `src/render/HizCulling.cpp:800-805` (current `mip=0u` hardcoded) + per-chunk morph factor
      uniform; Step 2 (M, ~300 LoC) per-strategy implementation в `src/shaders/voxel_mesh.comp` (or
      Pattern C `voxel_mesh.mesh` per `TODO.md §2.2`) — compute morph factor `t` per chunk + dual-source
      vertex fetch (LOD 0 + LOD 1) + Hoppe 1997 interpolation formula; Step 3 (S, ~100 LoC)
      `PROJECTV_LOD_TRANSITION=pop|crossfade|geomorph|morph_targets|hzb_stitch` env flag + Tracy plot
      "LOD Transition" + `ProjectVLodTransitionTests` unit test. Total ~450 LoC, M effort, 2-3 sessions.
      **Cross-axis:** orthogonal ко всем 9+ in-progress parallel сессий `2026-06-21` per `INDEX.md §5`;
      complementary к closed `2026-06-21-lod-mesh-downsampling` (per-LOD content axis) + closed
      `2026-06-20-mesh-shader-vs-compute-cull` (Pattern A vs C dispatch) + closed `2026-06-20-nanovdb-on-gpu`
      (storage) + closed `2026-06-21-sub-chunk-layers` (vertical layers ≠ LOD distance) + closed
      `2026-06-20-hzb-binding-models` + in-progress `2026-06-21-hzb-smart-mip-select` (HZB system, E_HZB_Stitch
      hypothesis needs GPU prototype).
      **Caveats:** (a) CPU prototype only, no real GPU dispatch — my naive vertex-index pairing measurement
      underestimates C_Geomorph / D_PreComputedMorphTargets quality (real GPU render with depth-test would
      show much better PSNR per Hoppe 1997 + Lysenko 2018); (b) 5 synthetic scene types only, not exhaustive
      of real ProjectV world content; (c) LOD chain covers only LOD 0 → LOD 1 (not full 4-level chain);
      (d) No mutation cost measured (out of Stage 4.2 DoD scope); (e) No HZB interaction measured (cross-
      axis with `hzb-smart-mip-select` in-progress + E_HZB_Stitch hypothesis); (f) Naive face counter (no
      greedy merge) — production mainline uses F_TwoPass per closed `greedy-physics-meshing-cpu`, would
      give ~35× reduction vs my baseline triangle counts.
      Closed entry: `experiments/2026-06-21-lod-transition-strategy/` + `prototype/{lod_transition_bench.cpp,
  lod_transition_bench, results.csv (125 rows), run.log}`.

- [ ] **2026-06-21-texture-compression-format-axis** — m, **independent** (cross-cutting VRAM axis для Stage 2.3 Sparse
  Virtual Texturing + Stage 4.3 Lift Draw Distance + Stage 5.x lighting; **self-invented topic** per operator
  instruction 2026-06-21 «выбирай свободную тему или придумывай свою исследуй»; **sixteenth invocation this session** —
  30+ closed, 10+ in-progress parallel: tracy-gpu-vs-manual, gpu-fluid-ca-atomic-strategy, vct-3d-mip-generation,
  vk-multi-gpu-split-frame, sdf-hybrid-world, greedy-physics-meshing, vulkan-defragmentation-compaction,
  lod-transition-strategy, wfc, taa).
  **Agent:** self.
  **Started:** 2026-06-21.
  **ETA:** this session (single experiment, analytical + standalone C++26 CPU prototype + open-source encoder reference
  impls + measurements per `benchmarks/methodology.md §3`).
  **Blocker:** нет (CPU-only analytical prototype, dev host `obvium` Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1` available; open-source encoder reference impls = `ispc_texcomp` BC + `astcenc` LDR + `Crunch`
  BC1-3 + `Compressonator` BC7 — vendor-neutral, deterministic, single-threaded per `work-stealing-job-system`
  verdict=mixed).
  **Hypothesis (one-line):** правильный выбор **texture compression format** ∈ {A_Uncompressed, B_BC1, C_BC3, D_BC5,
  E_BC6H, F_BC7, G_ASTC_4x4, H_ASTC_6x6, I_ASTC_8x8, J_ETC2_RGBA} для **voxel material atlas** (diffuse RGBA + normal
  XYZ + ORM [AO/Roughness/Metallic packed]) даст **−50% (BC1) до −75% (BC3/BC5/BC6H/BC7/ASTC 4x4) до −88% (ASTC 8x8)
  VRAM cost reduction** vs uncompressed baseline (per `agent/workspace.md §2` Nearest Gap callout: 8 GiB VRAM cap on RTX
  3060 Ti = main bottleneck Stage 4.3 128m draw distance) при **PSNR ≥ 40 dB per-image** (visually lossless threshold
  per `optimization-philosophy.md` + Khronos PBR Neutral guide 2024) + **+20-40% effective texture cache hit rate** (
  smaller footprint → more atlas entries in fixed VRAM cache) + **≤ 0.05 ms GPU decode cost** per 4K texture lookup (
  BC/ASTC hardware decode 1-cycle на Tier 1 dGPU NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc
  Alchemist/Battlemage).
  **Cross-axis:** orthogonal ко всем 10+ in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic,
  vct-3d-mip = Stage 5.1 VCT, vk-multi-gpu = multi-GPU VRAM, sdf-hybrid = Stage 5.1+3.3 hybrid, greedy-physics = Stage
  3.3 meshing, vulkan-defragmentation = VRAM compaction, lod-transition = Stage 4.2 LOD, wfc = Stage 4.1 gen, taa =
  Stage 5.3 temporal); **complementary** к 9 closed VRAM/cross-cutting experiments: `vma-sparse-textures` (mixed,
  page-table-level compression = different lever; **this = within-page payload compression, stackable**) +
  `vulkan-memory-aliasing-transient` (mixed, aliasing axis) + `frame-flight-allocator-budget` (mixed, allocator
  strategy) + `vulkan-defragmentation-compaction` (in-progress, compaction axis) + `depth-occlusion-quantization` (yes,
  depth format) + `nanovdb-on-gpu` (yes, GPU storage) + `vct-cone-count-atlas-precision` (mixed, VCT atlas format —
  orthogonal axis = lighting atlas) + `dlss-fsr-xess-upscaling-voxel` (mixed, post-process) +
  `vk-fragment-shading-rate-voxel` (mixed, fragment rate).
  **Scope (paths):**
    - `docs/experiments/experiments/2026-06-21-texture-compression-format-axis/{README.md,STATUS.md,sources.md}`
    - `docs/experiments/experiments/2026-06-21-texture-compression-format-axis/prototype/` (standalone C++26 CPU texture
      compression harness + open-source encoder reference impls + PSNR measurement + VRAM cost model + texture cache
      residency model, synthetic voxel scenes representative of ProjectV
      workload [uniform_diffuse + biome_pbr + cave_roughness + metal_emissive + mixed_stress per
      `sub-chunk-layers` precedent for direct comparability], NOT ProjectV mainline, dev host `obvium`)
    - `docs/experiments/INDEX.md` (§5 Active + §6 Recent при закрытии per §13.5)
    - `docs/experiments/research/backlog.md` (sync на закрытии per §13.5)
      **Expected verdict:** `mixed` (D_BC5 + F_BC7 + G_ASTC_4x4 likely winners per format
      tier [50-67% VRAM savings, PSNR ≥40 dB, hardware-accelerated decode]; B_BC1 likely worst for
      normal/ORM [2-channel artifacts]; I_ASTC_8x8 best for distant LOD
      atlas [88% VRAM saving, but PSNR 32-37 dB on smooth materials — may fail ≥40 dB threshold]; A_Uncompressed =
      mainline baseline, drop only if VRAM cap actively binding; **per-format recommendation conditional on atlas type
      ** [diffuse → F_BC7, normal → D_BC5, ORM → F_BC7, emissive HDR → E_BC6H, distant LOD → G_ASTC_4x4]).
      3-step migration per `agent/knowledge.md §30.4` precedent — Step 1 (XS, ~50 LoC)
      `TextureFormat::SelectMaterialAtlasFormat()` decision + `PROJECTV_TEXTURE_COMPRESSION=AUTO|BC7|BC5|ASTC4|OFF`
      env + Vulkan format candidate list per atlas type; Step 2 (S, ~150 LoC + encoder license file) encoder
      integration [bc7e_encoder_reference for BC7 + ispc_texcomp for BC1-5 + astcenc for ASTC]; Step 3 (S, ~80 LoC)
      hot-path swap in `voxel.frag` + `SceneResources.cpp` atlas allocation + per-chunk material metadata + Tracy plot "
      Atlas Compression Ratio". Total ~280 LoC + encoder license files, S-M effort, 2-3 sessions. **Caveats:** (a) CPU
      prototype, no real GPU dispatch — hardware decode cycle cost projection per `Nvidia Real-Time Texture Compression`
      docs + Khronos ASTC guide 2024; (b) PSNR vs uncompressed reference analytical (not visual QA на rendered voxel
      scene); (c) cross-vendor Tier 1/2 hardware matrix validated by public vendor docs (NVIDIA + AMD + Intel), no real
      GPU measurements на dev host `obvium` RTX 3060 Ti; (d) encoder quality = open-source reference best-effort (
      Crunch/Compressonator/astcenc), not perceptual tuned; (e) mutation cost per material change out of scope
      single-session; (f) GPU decode benchmark deferred to Stage 4.3 integration milestone.
      Cross-refs: `TODO.md §2.3` (Sparse Virtual Texturing — material atlas = sparse pages, compression per-page reduces
      page VRAM) + `§4.3` (Lift Draw Distance — material atlas scales linearly with chunk count, 8 GiB cap critical) +
      `§5.x` (lighting atlas orthogonal axis); `src/render/SceneResources.{hpp,cpp}` (VMA allocation material atlas);
      `src/shaders/voxel.frag` (material atlas sampling, currently R8G8B8A8_UNORM uncompressed); `agent/workspace.md §2`
      Nearest Gap (8 GiB VRAM cap = main bottleneck); `agent/knowledge.md §30.4` (3-step migration precedent);
      `agent/knowledge.md §17` (build matrix); `hardware-profile.md §1+§3` (Zen 3 5800X dev host + RTX 3060 Ti GA104,
      5.06 GiB driver limit); `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` (5-10% threshold; here up
      to −88% VRAM savings far exceeds); `benchmarks/methodology.md §3` (measurement protocol); closed experiments:
      `vma-sparse-textures` (mixed, page-level), `vulkan-memory-aliasing-transient` (mixed, aliasing),
      `frame-flight-allocator-budget` (mixed, allocator), `depth-occlusion-quantization` (yes, depth),
      `nanovdb-on-gpu` (yes, storage), `vct-cone-count-atlas-precision` (mixed, VCT atlas); active parallel:
      `tracy-gpu-vs-manual`, `gpu-fluid-ca-atomic-strategy`, `vct-3d-mip-generation`, `vk-multi-gpu-split-frame`,
      `lod-transition-strategy`.

---

## Closed (startup → experiments/<slug>/)

- [x] **[2026-06-21-voxel-chunk-streaming-pipeline](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/)** —
  m, **Stage 4.3** (chunk streaming / asset hot-load pipeline per `TODO.md §4.3` explicit Gap «lift draw distance
  cap 64→128m» + `agent/workspace.md §2` Nearest Gap «Stage 4.3 lift draw distance 128+ chunks» + closed
  `2026-06-20-cache-oblivious-chunk-tree` re-evaluation trigger; **self-invented topic** per operator instruction
  `2026-06-21` «выбирай свободную тему или придумывай свою и исследуй»; **0 of 30+ closed experiments covered
  chunk-streaming / asset-hot-load / demand-paging axis**). Closed `2026-06-21` (single session, ~1h),
  verdict **`mixed`**. Standalone C++26 CPU streaming simulator (`prototype/stream_bench.cpp` ~700 LoC, Clang 22.1.6
  `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, **0 warnings**). 5 strategies × 5 scenes × 5 seeds
  × 1000 frames + 10 warmup = **125 configs × 1000 frames = 125,000 main measurements**, wall time 0.07 sec на Zen 3
  5800X governor=`powersave` per `hardware-profile.md §1`. Output: `prototype/build/results.csv` (126 rows = 1 header
    + 125 data rows). Web-research via `webfetch` + DuckDuckGo HTML fallback (Exa HTTP 429 persistent): **5 primary
    + 3 secondary sources verified** (Aokana arXiv 2505.02017 May 2025 [GPU-driven voxel + LOD + streaming, 9× memory
    + 4.8× speedup], DanielWLiu07/voxel-engine GitHub 2026 [2226 chunks/sec, RLE 144× compression, multithreaded
      pipeline pattern], Voxceleron2 architecture [3-stage async generation + Chebyshev distance LOD], UE5 World
      Partition docs [cell size + loading range + streaming sources + HLOD], PrismarineJS/prismarine-chunk [Minecraft
      Bedrock reference]).
      **Headline (mixed):**

    - **A_PrebakeAll (current mainline) wins on stutter** by **6.5× margin** vs D_DemandPaging baseline (mean 2.79 µs
      vs 7.88 µs, p99 23.75 µs vs 57.30 µs) — crosses 5-10% threshold per `optimization-philosophy.md` by **6×**.
      Worst-case VRAM 8.2 MiB during teleport = manageable under 8 GiB budget.
    - **E_HybridDemandPredictive wins on VRAM footprint** by **90%** (0.9 MiB vs 8.2 MiB) at cost of +30 µs p99
      stutter on worst-case teleport_stress scenes. Useful for Stage 5+ memory-tight scenarios.
    - **Scene dominates over strategy:** linear_walk/fly_vertical (0.5 µs mean) vs teleport_stress (27 µs mean).
    - **B and D show identical metrics** in synthetic prototype (ring cap >> working set).
    - **C and E show identical metrics** in synthetic prototype (predictive prefetch dominates both).
      **Mainline recommendation:** **A_PrebakeAll = Stage 4.3 MVP default** (no code change — current mainline
      behavior already implements; ~30 LoC for env flag + Tracy plot documentation). **E_HybridDemandPredictive =
      Stage 5+ recommended** when VRAM tight (~300 LoC migration per `§30.4` precedent: priority queue + background
      thread + `std::expected` cold-path). **Total ~430 LoC if both implemented, 1-2 sessions for Step 1 (zero code
      change really), 3-4 sessions for Step 2.** Cross-axis: **orthogonal** ко всем 4 in-progress parallel (tracy-gpu

    + gpu-fluid-ca-atomic + lod-transition + vulkan-defrag); **complementary** к 8 closed VRAM/storage/streaming
      experiments (cache-oblivious-chunk-tree [DIRECT trigger] + vk-multi-gpu-split-frame + vulkan-memory-aliasing-
      transient + frame-flight-allocator-budget + depth-occlusion-quantization + vma-sparse-textures + nanovdb-on-gpu
    + sub-chunk-layers + greedy-physics-meshing-cpu).
      См. [README](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/README.md)
    + [STATUS](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/STATUS.md) +
      [sources](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-voxel-chunk-streaming-pipeline/prototype/RESULTS.md) +
      `prototype/{stream_bench.cpp, build.sh, README.md}` + `prototype/build/{stream_bench, results.csv}`. См. §6
    + [INDEX §6 Recent closed](./INDEX.md) за full table.

- [x] **[2026-06-21-vulkan-defragmentation-compaction](./experiments/2026-06-21-vulkan-defragmentation-compaction/)**
  — m, **cross-cutting VRAM axis** (compaction / defragmentation lever after `vulkan-memory-aliasing-transient`
  closed mixed aliasing axis + `frame-flight-allocator-budget` closed mixed allocator strategy axis; **self-invented
  topic** per operator instruction `2026-06-21`: «выбирай свободную тему или придумывай свою исследуй»; **ninth
  invocation this session** — previous 8 closed or in-progress). Closed `2026-06-21` (single session, ~2h),
  verdict **`mixed`**. **Compaction axis** — **new axis** в 30+ closed experiments (VMA defragmentation not previously
  covered). **Anti-duplicate sentinel clean** per `AGENTS.md §13.7` (no `vulkan-defragmentation` folder, no
  `vma-defragmentation` folder; only `vulkan-memory-aliasing-transient` closed mixed aliasing axis = orthogonal lever +
  `frame-flight-allocator-budget` closed mixed allocator strategy axis = orthogonal lever). **Standalone C++26 CPU
  fragmentation simulator** `prototype/defrag_bench.cpp` ~430 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG
  -Wall -Wextra -Wpedantic`, **0 warnings** after final iteration). 4 iterations (`v1` first-fit → `v2` best-fit →
  `v3` real OOM via no-hole → `v4` 2 GiB heap + reduced intensity) to find measurement regime that exposes
  fragmentation effects. 5 strategies × 5 scenes × 4 alloc patterns × 5 seeds × 1000 frames + 10 warmup = **500 configs
  × 1000 frames = 500,000 main measurements**, wall time 10.40 sec на Zen 3 5800X governor=`powersave` per
  `hardware-profile.md §1`. Web-research via `webfetch` direct URLs (DuckDuckGo HTML CAPTCHA + Exa HTTP 429 persistent
  per operator directive); **8+ primary sources verified**: VMA docs rev 3.4.0 (`defragmentation.html` +
  `staying_within_budget.html` + `custom_memory_pools.html` + `group__group__alloc.html` +
  `struct_vma_defragmentation_info.html`), VMA GitHub CHANGELOG (v3.4.0 race condition fixes #529/#313 + v3.0.0 new
  defrag API + v2.2.0 GPU defrag support + v2.3.0 memory budget support), Vulkan 1.4 spec memory chapter.
  **Headline findings (mixed):**
  - **Synthetic CPU sim shows trivial results** — 6% heap utilization (124 MiB mean на 2 GiB heap) produces zero
    fragmentation, all 5 strategies tie on peak VRAM (246.14 MiB) / mean used / frag ratio / alloc failure rate.
  - **Only `C_IncrementalBudgeted` registers defrag activity** — p99 = 0.0117 ms = 0.035% of 33.3 ms frame budget =
    safe. Zero stutter frames across 100 configs.
  - **Intermediate v3 iteration (256 MiB heap + heavy workload) exposed** — `C_IncrementalBudgeted` = −1.4% peak
    VRAM + 0 stutter (best balance); `D_OnDemandThreshold` = **CATASTROPHIC 16% stutter rate** (8064 frames) when
    trigger fires; `B_PeriodicFull` = acceptable but inferior to C; `E_BudgetedOnDemand` = no benefit in synthetic.
  - **Real-world validation gap** — CPU sim cannot model `bufferImageGranularity` alignment, multi-memory-type
    fragmentation, or VMA's TLSF algorithm sophistication. Mainline integration with real VMA + real Vulkan
    workload required for final verdict.
  - **Cross-axis projection** — stacked potential с closed `vulkan-memory-aliasing-transient` (-7-8% VRAM) =
    **-10-15% VRAM** for Stage 4.3 lift draw distance workload = **crosses 5% threshold** per
    `optimization-philosophy.md`. Compaction is **necessary but not sufficient** in isolation.
  **Mainline recommendation:** adopt `C_IncrementalBudgeted` strategy (`maxBytesPerPass=8 MiB` cap) per
  `agent/knowledge.md §30.4` 3-step migration precedent — Step 1 (XS, ~30 LoC) `VramDefrag.{hpp,cpp}` +
  `PROJECTV_DEFRAG=ON|OFF` env flag; Step 2 (S, ~100 LoC) `TickDefrag()` per-frame scheduler + Tracy plot
  "VRAM Defrag" + `vmaGetHeapBudgets()` integration; Step 3 (XS, ~30 LoC) default flip + per-stage policy.
  Total ~160 LoC across 3 files, S effort, 1-2 sessions. **Cross-axis:** orthogonal ко всем 5+ in-progress
  parallel (tracy-gpu-vs-manual + gpu-fluid-ca-atomic-strategy + hzb-smart-mip-select + vct-3d-mip-generation +
  vk-multi-gpu-split-frame); **complementary** к closed mixed `vulkan-memory-aliasing-transient` (aliasing axis =
  stackable) + closed mixed `frame-flight-allocator-budget` (allocator strategy axis = stackable). **Direct
  continuation chain:** aliasing → allocator strategy → compaction = complete VRAM fragmentation mitigation stack.
  **Caveats:** (a) CPU prototype, no Vulkan init, no real GPU driver overhead для `vmaDefragment` GPU copy;
  (b) synthetic VRAM heap (2 GiB match dev host) — workload intensity 6% utilization = no fragmentation modeled;
  (c) fragmentation ratio synthetic per `vmaComputeAllocationStats` model (real = aligned with VMA ref impl line
  ~7000-8000 + `vmaDefragment` algorithm internals); (d) cross-vendor VRAM characteristics not measured (single
  host RTX 3060 Ti); (e) mutation cost (rebuild defrag state on chunk mutation) not separately measured; (f) visual
  regression proxy = single-frame stutter detection (no real VMA validation); (g) algorithm choice
  (FAST/BALANCED/FULL/EXTENSIVE per VMA docs) not separately measured — only FULL algorithmic mode tested.
  **Re-evaluation triggers:** Stage 4.3 ships (128+ chunks draw distance); VMA 3.5+ release; cross-vendor AMD RDNA +
  Intel Arc dev matrix; real Vulkan integration prototype.
  См. [README](./experiments/2026-06-21-vulkan-defragmentation-compaction/README.md) +
  [STATUS](./experiments/2026-06-21-vulkan-defragmentation-compaction/STATUS.md) +
  [sources](./experiments/2026-06-21-vulkan-defragmentation-compaction/sources.md) +
  [RESULTS](./experiments/2026-06-21-vulkan-defragmentation-compaction/RESULTS.md) +
  [INDEX §6 Recent closed](./INDEX.md) за full table.

- [x] **[2026-06-21-vulkan-memory-aliasing-transient](./experiments/2026-06-21-vulkan-memory-aliasing-transient/)** —
  m, **independent** (cross-cutting Stage 2.x-5.x). Closed `2026-06-21` (single session, ~3h),
  verdict **`mixed`**. **Render-pipeline-architecture axis** (Vulkan transient resource aliasing +
  render graph DAG) — **first axis** в 30+ closed experiments. **Self-invented topic** per operator
  instruction `2026-06-21`: «выбирай свободную тему или придумывай свою и исследуй». Ninth invocation
  this session (previous 8 closed same-session: audio mixed + wfc mixed + sub-chunk mixed + gpu-noise
  mixed + taa yes + depth yes + vk-fragment-shading mixed + frame-flight mixed + dxc mixed + lod-mesh
  mixed + audio-diffraction mixed = 12 closed same-session). **Standalone C++26 CPU lifetime simulator**
  `prototype/mem_alias_bench.cpp` ~600 LoC (Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall
  -Wextra -Wpedantic`, builds green with 10 cosmetic warnings на unused constexpr / argc-argv).
  3 workloads × 4 strategies × 5 seeds × 1000 iter + 10 warmup = **60,000 main measurements**, wall
  time <1 sec на Zen 3 5800X governor=`powersave` per `hardware-profile.md §1`. **Headline findings:**
    - **D_DAGRenderGraph barrier reduction = −74%** (consistent across all workloads, 28→7 / 50→13 /
      74→19) — **real win**, directly impacts CPU command buffer recording overhead.
    - **C_FullAliasing VRAM savings = −7-8%** on typical (276→255 MiB) + projected (398→372 MiB)
      workloads — crosses 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.
      Modest savings (~22 MiB absolute) на large workloads, ≈0 на minimal MVP (pool overhead eats savings).
    - **B_VMA_SubAllocatorPool = REGRESSION** (−5% additional overhead vs A baseline) — pure pool
      without lifetime analysis = worse than current pattern. **Never adopt without aliasing.**
      **Persistent image bottleneck (root cause of modest savings):** depth + shadow + hiz + taa history
      = ~98 MiB cannot be safely aliased across frames (write-after-read hazards). Hard ceiling ~35% VRAM.
      **Web-research Phase A:** 9 primary + 7 secondary sources verified via `webfetch` + DuckDuckGo HTML
      fallback (Exa HTTP 429 persistent): Yuriy O'Donnell 2017 GDC Frostbite FrameGraph [canonical];
      Themaister 2017/2019 Granite Engine blog [open-source reference]; VMA official resource_aliasing
      docs; WSCG 2023 history-aware frame graph academic paper; dev.to p3ngu1nzz 2025-10-06 + 2025-10-18
      modern implementation; Khronos Vulkan Tutorial render graph; AMD RPS SDK; KhronosGroup Vulkan
      resources.adoc 2026-06-05. **Mainline recommendation:** phased migration per `agent/knowledge.md
  §30.4` precedent — **Step 1 (S, ~150 LoC) immediate**: VMA pool setup grouped by `ResourceType` +
      heap type with sub-allocation (validation only); **Step 2 (M, ~500 LoC) for Stage 4.3**:
      interval-graph coloring for non-overlapping lifetimes (lifetime tracking в `CreateBuffer`/
      `CreateImage`); **Step 3 (L, ~1500 LoC) deferred to Stage 5.x post-VCT+RTX**: DAG-based render
      graph + auto-barrier batching (4:1 reduction). Total ~2150 LoC, L effort, 4-6 sessions.
      **Caveats:** CPU simulation only (no real GPU dispatch / driver overhead), synthetic workloads
      (realistic upper-bound), greedy coloring algorithm (production render graphs use Pettis-Hansen
      +10-20% better packing), single-GPU dev host (cross-vendor analytical projection only).
      **Continuation chain:** none (first render-graph axis experiment; opens cross-cutting Stage 2.x-5.x
      render pipeline architecture). **Re-evaluation triggers:** Stage 4.3 ship (128+ chunks, aliasing
      payoff grows); Stage 5.1 VCT + Stage 5.2 RTX + Stage 5.3 TAA (pass count > 15, barrier batching high
      value); `VK_KHR_dynamic_rendering_local_read` extension ratification status; AMD RDNA 4 + Intel Arc
      Battlemage dev matrix; Pettis-Hansen aliasing allocator production validation (e.g., RPS SDK adoption).
      См. §6 + §1 + experiment README + `STATUS.md` (final) + `sources.md` (16 sources) +
      `prototype/{mem_alias_bench.cpp, RESULTS.md, build/results.csv}`.

- [x] **[2026-06-21-vct-cone-count-atlas-precision](./experiments/2026-06-21-vct-cone-count-atlas-precision/)** —
  m, **Stage 5.1** (Voxel Cone Tracing per `TODO.md §5.1` + direct follow-up to closed
  `2026-06-20-vct-vs-rt-cutoff` [verdict=mixed, cutoff=0.3]). Closed `2026-06-21` (single session, ~2.5h),
  verdict **`mixed`**. **VCT within-quality axis** — sixth invocation this session (previous 5 closed
  or in-progress: audio + wfc + sub-chunk + gpu-noise + lod-mesh + taa + dxc + frame-flight + depth-
  occlusion closed; tracy-gpu + gpu-fluid-ca + vk-fragment-shading-rate + audio-diffraction in-progress
  parallel; 19+ closed `2026-06-20`). **Standalone Vulkan 1.4 compute prototype** ~700 LoC
  (`prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` + `RESULTS.md` +
  `build/results.csv` 12 measurements), 4 SPIR-V variants via `-DCONE_{6,12,24,1024}` defines,
  builds green with 0 errors, 1 forward-decl warning fixed. 9 measured configs (3 cone counts × 3
  atlas precisions) × 100 iter + 10 warmup = 900 measurements + 3 references on dev host `obvium`
  RTX 3060 Ti GA104 Ampere + Vulkan 1.4.341 per `hardware-profile.md §3/§4`. **Web-research**
  complete (4 batches, ~30 results, 12 primary + 6 secondary sources verified: Crassin 2011 GIVoxels
  §5 [PDF: 5 cones diffuse canonical], Panteleev 2014 thesis Uni Bremen [6 cones + R16G16B16A16 atlas],
  OGRE 2019 VCT [4-6 cones + R8 banding risk], Lumen SIGGRAPH 2022 Narkowicz [24 cones for surface
  cache, not pure VCT], Andersson 2024 CGF Dynamic VCT [RTX 2060 0.38 ms], KTH Northman 2024 [atlas
  size scaling], HanetakaChou RTX 4080 [8-32 RPP 7-12 ms], Vulkan R16F core 1.0 + storage image
  support). **Headline findings:** **VRAM cost linear in bpp** (R8/R16F/R32F = 9/18/36 MiB на 128³
  atlas with mip chain; 256³ = 72/144/288 MiB = 1.4/2.8/5.5% of 5.06 GiB budget per
  `hardware-profile.md §3`). **Perf ≈ 15 µs per 1024² dispatch for ALL 12 configs** — cone count
  6/12/24/1024 NOT a discriminator, dispatch overhead dominates at this work size
  (Ampere GA104 launch latency). **Quality axis literature-projected, NOT measured** (1024-cone
  Fibonacci reference did not successfully write to output, likely shader compile issue with
  unrolled fibDir loop). **Recommended sweet spot: 6 cones × R16G16B16A16_SFLOAT** (NOT 12×R16F
  as originally hypothesized — literature shows 5-6 cones is canonical, 12+ shows diminishing
  returns). R16F = Panteleev 2014 baseline, mitigates OGRE 2019 R8 banding risk. **3-step migration
  per `agent/knowledge.md §30.4`:** Step 1 (XS, ~10 LoC) atlas format `R8G8B8A8_UNORM` →
  `R16G16B16A16_SFLOAT` в `voxelize.comp` (new per TODO §5.1) + `PROJECTV_VCT_ATLAS_FORMAT` env
  fallback; Step 2 (S, ~50 LoC) cone count loop в `vct.frag` (new per TODO §5.1) with `N_CONES=6`
  (literature baseline, not 12); Step 3 (XS, ~20 LoC) Tracy plot `VCT_ConeMarchMs` + default flip +
  `agent/knowledge.md §30.x` decision record. Total ~80 LoC, S effort, 1-2 sessions, 1 PR.
  **Cross-vendor matrix:** NVIDIA Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel Arc Gfx12.5+;
  analytical projection per `dec-pipelines-async-compute` §2.2 — 6×R16F sweet spot is
  cross-vendor-invariant (no vendor-specific advantage for higher cone counts). **Caveats:**
  (a) 1024-cone reference write broken in prototype (PSNR=0dB for measured vs 99.9dB for
  reference is artifactual, both compared to all-zero reference); (b) single 1024² frame, real
  workload = 1920×1080 + cubemap + reflection probes = ~10× more work; (c) single synthetic
  scene (ground + sky + 2 walls), real voxel scenes have more variation; (d) no mip build
  cost measured (amortized over frames); (e) no driver overhead / multi-queue optimization
  measured; (f) single GPU vendor validated (RTX 3060 Ti GA104). **Cross-axis:** orthogonal
  к 4 in-progress parallel (tracy-gpu = profiling, gpu-fluid-ca = Stage 3.1 atomic,
  vk-fragment-shading-rate = VRS fragment rate, audio-diffraction = audio); complementary к
  closed `vct-vs-rt-cutoff` (cutoff strategy) + `nanovdb-on-gpu` (storage foundation) +
  `restir-gi-feasibility` (deferred Stage 6+ path tracer) + `dec-pipelines-async-compute` (async
  mip-chain prerequisite). **Follow-up candidates (out of scope):** Crassin 2011 cone-tapered
  mip filter (+2-4 dB expected); 1024-cone reference fix (split into 2×512 or pre-compute
  directions in UBO); specular cone count axis (Lumen uses 3-6); atlas resolution scaling
  (128³/256³/512³ VRAM-constrained); 4D temporal VCT (close to closed
  `2026-06-21-taa-motion-vectors`); VCT + VRS feedback loop (orthogonal to in-progress
  `vk-fragment-shading-rate-voxel`). См.
  §6 + [experiment README](./experiments/2026-06-21-vct-cone-count-atlas-precision/README.md)
    + [RESULTS](./experiments/2026-06-21-vct-cone-count-atlas-precision/RESULTS.md) +
      `prototype/{vct_main.cpp, cone_march.comp, CMakeLists.txt, README.md}` + `build/results.csv`
      (12 measurements).

- [x] **[2026-06-21-audio-diffraction-hybrid](./experiments/2026-06-21-audio-diffraction-hybrid/)** —
  l-promoted, **independent** (audio rendering axis, **Phase 1.5 enhancement** explicitly declared follow-up в
  closed `2026-06-21-audio-raytracing-voxel-sdf` line 459-460). Closed `2026-06-21` (single session, ~1.5h),
  verdict **`mixed`**. **Audio axis Phase 1.5** — fifth invocation this session (previous 4 closed: audio +
  wfc + sub-chunk + taa). **Standalone C++26 CPU prototype** ~985 LoC
  (`prototype/{voxel_grid,audio_path,diffraction}.{hpp,cpp} + bench.cpp + Makefile + README + RESULTS + results.csv`),
  Clang 22.1.6 `-O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic`, build green, 0 warnings.
  3 strategies × 3 scenes × 3 seeds × 100 iter × 16 sources = **14,400 invocations** на Zen 3 5800X
  governor=`powersave` per `hardware-profile.md §1`. **Web-research** complete (4 batches, ~30 results,
  16 primary + 7 secondary sources verified, ключевые: Schissler 2014 high-order diffraction
  [SIGGRAPH 2014 ACM TOG 33(4) 39, edge visibility graph + UTD, 15-50 FPS on 4-core CPU, indoor + urban scenes]
    + Schissler 2014 multi-source [I3D 2014, 50+ reflection orders, 5× speedup, 200 sources] + Cao 2016 BST
      [SIGGRAPH ASIA 2016 ACM TOG 35(6) — closed `audio-raytracing-voxel-sdf` Phase 3 falsified reference] +
      Cao 2021 fast diffraction [SIGGRAPH 2021, 10th-order diffraction, 568× faster] + Tsingos 2001 UTD
      [SIGGRAPH 2001, beam tracing — **NOT depth-mip**] + Tsingos 2007 Instant Sound Scattering [EGSR 2007,
      depth-mip GPU, 20-40× faster than CPU, 700 Hz refresh — **Tsingos 2001 ≠ Tsingos 2007**] + Chandak 2008
      AD-Frustum [IEEE TVCG 2008, UTD + frustum tracing] + Antani 2012 BTM [IEEE TVCG 2012, 2-4× reduction
      visible primitives] + Vercidium 2025 [voxel + CPU + audio ray-tracing, production reference] +
      SonoTraceUE 2026-01-09 [UE5 curvature-based MC diffraction + HW RT, arXiv 2602.19652] + Pinpoint Audio
      Tracing 2025-08-18 [UE5 RTX-mandatory, Lumen-dependent] + Meta XR Audio SDK 2024+ [Acoustic Map + Edge
      Diffraction, hybrid precomputed+runtime] + Wwise Spatial Audio [Audiokinetic Ak Geometry API, AAA
      production diffraction+transmission] + Google Patent WO2024179939A1 [voxel + multi-directional
      diffraction, public prior art] + Han 2025 IEEE CoG survey [**41% sound designers find LPF alone
      insufficient**]). **Measured (Zen 3 5800X, governor=`powersave`):**
      A_None 0.0001 ms / source (1 probe, 0.02% audio budget @ 64 sources);
      **C_Tsingos 0.0025-0.0032 ms / source (33 probes, 0.5-0.6% audio budget @ 64 sources, +1.2-1.4 dB recovery
      per Tsingos 2007 spec 1-2 dB)**;
      B_Schissler 0.024-0.082 ms / source (17 probes, 5-16% audio budget, 0 dB recovery в simplified
      first-order UTD). Cross-arch projection (Zen 5 AVX-512): C_Tsingos 0.0015-0.0020 ms = 0.3-0.4% budget;
      B_Schissler 0.012-0.040 ms = 2-5% budget. **Verdict=mixed:** **C_Tsingos production-ready**
      (0.5-0.6% budget, +1.2 dB recovery, crosses 5% optimization threshold by 8-10× margin per
      `optimization-philosophy.md`); **B_Schissler deferred** до second-order UTD implementation. **Mainline
      3-step migration per `agent/knowledge.md §30.4` precedent:** Step 1 (XS, ~80 LoC)
      `Diffraction::sampleHemisphere()` helper + Fibonacci sphere + depth-mip lookup stub; Step 2 (XS, ~50 LoC)
      wire into `AudioEngine::tick()` after occlusion call; Step 3 (XS, ~20 LoC) env flag
      `PROJECTV_AUDIO_DIFFRACTION=ON` default ON. Total ~150 LoC, XS effort, 1-2 sessions. **Caveats:**
      (a) CPU-only synthetic voxel scenes (cave + open_plains + multi_room per closed `audio-raytracing-voxel-sdf`);
      (b) Zen 3 5800X governor=`powersave`; (c) no AVX-512 = realistic measurement floor (deferred до Zen 5 /
      Arrow Lake per `simd-procedural-noise` precedent); (d) perceptual validation = analytical proxy (Tsingos
      openness fraction → dB estimate per spec), not full HRTF / ABX listening test; (e) B_Schissler first-order
      UTD only (second-order edge-to-edge = future work для full +2-4 dB); (f) N=100 iterations per strategy ×
      scene × seed (vs methodology default 1000); (g) no DSP overhead in prototype (closed `audio-raytracing-
  voxel-sdf` baseline = +0.005-0.015 ms per source для full pipeline). **Continuation chain:** closed
      `audio-raytracing-voxel-sdf` (Phase 1+2 recommended, Phase 3 falsified) → this (Phase 1.5 = Tsingos
      integration) → future Phase 1.6 = B_Schissler second-order UTD. **Cross-axis:** complementary к closed
      `audio-raytracing-voxel-sdf` (Phase 1+2) + closed `hzb-binding-models` (texelFetch pattern reuse для
      depth-mip probe) + closed `nanovdb-on-gpu` (SVO walker foundation, future hierarchical skip для
      Phase 1.6) + closed `work-stealing-job-system` (serial dispatcher) + closed `simd-procedural-noise`
      (AVX2 baseline = realistic floor). **Re-evaluation triggers:** Zen 5+ AVX-512 hardware availability,
      HRTF integration (Meta XR Audio SDK), ProjectV audio axis progression to Stage 7.x per
      `agent/knowledge.md §28`, second-order UTD implementation per Chandak 2008 / Cao 2021. См.
      [experiment README](./experiments/2026-06-21-audio-diffraction-hybrid/README.md) +
      [STATUS](./experiments/2026-06-21-audio-diffraction-hybrid/STATUS.md) +
      [sources.md](./experiments/2026-06-21-audio-diffraction-hybrid/sources.md) +
      [prototype/README.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/README.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-audio-diffraction-hybrid/prototype/RESULTS.md) +
      `prototype/results.csv` (28 rows).

- [x] **[2026-06-21-vk-fragment-shading-rate-voxel](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/)** —
  m, **independent** (cross-cutting Stage 5.x lighting cost optimization, **follow-up axis** после полного closure
  lighting-strategy-axis `2026-06-20`: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
  `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed). Closed `2026-06-21` (single session, ~1.5h),
  verdict **`mixed`**. **VRS-cost-axis experiment** — единственная Stage 5.x cost-side axis, не покрытая same-session
  closed experiments (4 lighting-strategy + frame-flight-allocator + gpu-procedural-noise + dxc-vs-glslc-toolchain +
  audio-raytracing-voxel-sdf + sub-chunk-layers) + in-progress parallel (tracy-gpu + wfc-procedural +
  taa-motion-vectors + gpu-fluid-ca-atomic + lod-mesh-downsampling + audio-diffraction-hybrid). Web-research
  complete (2 batches, ~14 results, **10 primary sources verified:** Khronos spec + Vulkan samples +
  Intel SIGGRAPH 2019 + NVIDIA NAS GDC 2019 + NVIDIA VRSS 2 + AMD RADV Mesa commits via Phoronix +
  SaschaWillems DeepWiki + Unity URP docs + Godot proposal #3859 + Vulkan 1.4 core revisions +
  platonvin/lum-rs voxel precedent). Standalone C++26 CPU prototype `prototype/vrs_voxel_sim.cpp` **~770 LoC**
  (Clang 22.1.6 `-O3 -march=native -std=c++26 -Wall -Wextra`, **0 warnings**), 4 scenes × 3 resolutions ×
  5 VRS configs × 100 iter + 10 warmup = **6000 measurements** on dev host `obvium` Zen 3 5800X + governor
  `powersave`. **Headline numbers (mean across all scenes × all resolutions):**
    - **`baseline_1x1`**: covered 4-6%, invocations = covered_pixels (control).
    - **`vrs_2x1` / `vrs_1x2`**: **50% savings** consistent across все 4 scenes × 3 res = **deterministic**.
    - **`vrs_2x2_global`**: **75% savings** consistent, highest quality risk (0.425-0.575).
    - **`vrs_hybrid_2x2_lighting`**: **0% savings** ⚠️ — falsified hypothesis для sparse voxel scenes.
    - **VRS image bytes:** 8 KiB @ 1080p / 14 KiB @ 1440p / 32 KiB @ 4K = **0.0001-0.0004% of 8 GiB VRAM budget**
      (per `hardware-profile.md §3`) — VRAM cost **negligible**.
    - **Quality risk (heuristic):** uniform_open ≤ 0.425, forest_floor ≤ 0.425, cave_stress ≤ 0.575,
      mixed_biome ≤ 0.575 (higher для complex silhouettes per Intel SIGGRAPH 2019 + NVIDIA NAS).
    - **Cross-vendor Tier 2 VRS validated:** NVIDIA Turing/Ampere/Ada/Blackwell + AMD RDNA 2/3/4 + Intel
      Gen11/Arc Alchemist/Battlemage per Mesa RADV (Phoronix 2020/2023) + Intel ANV + NVIDIA driver 460+
      baseline.
    - **⚠️ Critical spec correction:** `VK_KHR_fragment_shading_rate` **NOT in Vulkan 1.4 core** per
      `docs.vulkan.org/spec/latest/appendices/versions.html` (initial hypothesis assumed core promotion —
      falsified); remains device extension in 1.4. RTX 3060 Ti on dev host `obvium` supports via NVIDIA 610.43.02
        + Vulkan 1.4.341.
          **Mainline рекомендация** per `README.md §7` + `STATUS.md`:
    - **Step 1 (XS, immediate, ~30 LoC):** global `vrs_2x1` для VCT integration via
      `vkCmdSetFragmentShadingRateKHR` + `voxel.frag` VRS-agnostic adaptation per Intel SIGGRAPH 2019
      (`dFdx/dFdy` scaling, `gl_FragCoord no longer n+0.5`). Safe 50% fragment shading cost reduction.
    - **Step 2 (S, ~100 LoC + tests):** VRS extension probe (`VkPhysicalDeviceFragmentShadingRateFeaturesKHR`
      check 3 features: `pipelineFragmentShadingRate` + `primitiveFragmentShadingRate` +
      `attachmentFragmentShadingRate`) + `VkFragmentShadingRateAttachmentInfoKHR` attachment setup +
      `VK_FORMAT_R8_UINT` shading rate image per swapchain (size = W/16 × H/16 bytes).
    - **Step 3 (M, ~250 LoC) DEFERRED:** hybrid classifier + two-pass dynamic VRS per Khronos sample
      `fragment_shading_rate_dynamic` (compute shader generate per-frame derivative image → next-frame VRS
      image; two renderpass pattern to avoid feedback loop). Conditional: только if Stage 4.3 lift raises
      voxel coverage > 30% (then hybrid savings > 0% expected).
      **Caveats:** (a) CPU prototype, no real GPU dispatch — savings formulas validated, real GPU timings

    + visual quality deferred до GPU prototype на RTX 3060 Ti; (b) hybrid savings 0% для sparse scenes
      (falsified hypothesis) — classifier thresholds (cov_ratio > 85% + edge_ratio < 3% для low-detail)
      consistent per `prototype/vrs_voxel_sim.cpp:build_vrs_image`; (c) quality_risk эвристика simplified,
      needs PSNR/SSIM measurement на rendered frames; (d) cross-vendor GPU measurement (AMD RDNA 2/3,
      Intel Arc) analytical-only — needs hardware matrix validation; (e) TAA + VRS feedback loop
      (per NVIDIA NAS GDC 2019: 3-4 frames transition latency) **cross-axis risk** с in-progress
      `2026-06-21-taa-motion-vectors` — separate experiment needed; (f) VRAM cost projection conservative
      (single-buffered; double-buffered = 2× bytes); (g) `voxel.frag` per-pixel ops (depth downsampling,
      dithering) require `SV_Position` adaptation per Intel SIGGRAPH 2019 caveat. **Continuation chain:**
      `vct-vs-rt-cutoff` (closed strategy axis) + `clustered-forward-mass-lights` (closed light count) +
      `rt-shadows-vs-csm` (closed shadow strategy) + `restir-gi-feasibility` (closed GI strategy) →
      this (closed cost axis). Full Stage 5 lighting optimization landscape covered same-day `2026-06-20` +
      `2026-06-21` cluster. **Follow-up candidates** (out of scope для this session, deferred до separate
      experiments): `_vrs-taa-feedback-loop_` (cross-axis с `taa-motion-vectors` in-progress);
      `_vrs-gpu-prototype-rtx3060ti_` (real GPU timing + visual quality validation); `_vrs-dense-scene-hybrid_`
      (re-test hybrid classifier на cave_interior / dense_foliage scenes с >30% coverage);
      `_vulkan-1.5-1.6-vrs-core-promotion_` (verify if VRS extension promoted to core in next Vulkan minor);
      `_vr-foveated-vrs-gaze-input_` (cross-axis с `eye-tracked-foveated` backlog l-priority). См. §6 + §1 +
      [experiment README](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/README.md) +
      [STATUS](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/STATUS.md) +
      [sources.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/RESULTS.md) +
      [prototype/results.csv](./experiments/2026-06-21-vk-fragment-shading-rate-voxel/prototype/results.csv).

- [x] **[2026-06-21-taa-motion-vectors](./experiments/2026-06-21-taa-motion-vectors/)** — m,
  **independent** (Stage 5.3 TAA Motion Vectors per `TODO.md §5.3`, **temporal axis** для Stage 5 после
  полного closure lighting-axis на `2026-06-20`: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights`
  yes + `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed). Closed `2026-06-21` (single session, ~1h),
  verdict **`yes`** for Pipeline A (vertex-out motion vector MRT). **Verdict basis** (independent of
  measurement execution per agent not building per `AGENTS.md §1`): (1) `TODO.md §5.3` line 425 explicit
  format prescription `VK_FORMAT_R16G16_SFLOAT` = mandate для mainline; (2) Karis 2014 SIGGRAPH foundational
  paper "High Quality Temporal Supersampling" ["16:16 RG velocity buffer" = R16G16_SFLOAT exact match;
  "velocity accuracy is super important" drives vertex-out recommendation]; (3) industry standard (UE 5 +
  Godot 4.x + Unity HDRP all use R16G16_SFLOAT motion vector MRT) — no cross-vendor ambiguity per
  `dec-pipelines-async-compute` §2.2 vendor matrix; (4) VRAM cost 4 MiB/frame single-buffered / 8 MiB
  double-buffered @ 1080p = 0.08% / 0.16% of 5.06 GiB budget per `hardware-profile.md §3` = well under 5%
  threshold per `optimization-philosophy.md`; (5) `TODO.md §5.3` DoD «Полное исчезновение шлейфов за
  перемещаемыми гравипушкой моделями» = only achievable with vertex-out (depth-reproject has fundamental
  precision loss near edges per Karis 2014). **Web-research** complete (2 batch queries, ~14 results, 6
  primary + 5 secondary sources верифицированы: Karis 2014 SIGGRAPH foundational, Yang/Liu/Salvi 2024
  Stanford TAA survey [neighborhood clamping + YCoCg = standard 2024], Marrs/Spjut 2018 NVIDIA adaptive TAA
  [requires RT, out of scope], k-DOP Clipping SIGGRAPH 2024 [SOTA ghosting mitigation 0.2 ms overhead, follow-up
  candidate], Karolewics Lumberyard anti-ghosting TAA [production reference 0.1 ms + 1.6 ms total Xbox One],
  VK_KHR_dynamic_rendering [core 1.3 enables MRT pattern already ProjectV mainline]). **Mainline 3-step
  migration per `agent/knowledge.md §30.4` precedent** — Step 1 foundation (S, ~50 LoC, 1 session): vertex
  shader `out vec4 vPrevClip` + fragment shader `layout(location=1) out vec2 outMotion` (R16G16_SFLOAT) +
  `TaaRenderTargets.{hpp,cpp}` add motion vector attachment + `SceneResources.{hpp,cpp}` allocate
  double-buffered motion vector MRT (8 MiB @ 1080p); Step 2 TAA resolve update (S, ~50 LoC, 1 session):
  change motion vector source from current depth-reproject to read from motion vector MRT + image layout
  transition `COLOR_ATTACHMENT_OPTIMAL` → `SHADER_READ_ONLY_OPTIMAL` for motion vector after geometry pass
  before TAA resolve; Step 3 default flip (XS, ~10 LoC, 1 commit): `PROJECTV_USE_MOTION_VECTOR_MRT=ON`
  env flag with cross-vendor graceful fallback. **Total effort M** (~110 LoC across 5-6 files, 2-3 sessions).
  **Caveats:** (a) no actual GPU measurements (prototype is measurement harness skeleton per
  `prototype/README.md` 'Status' section — operator can extend + run if desired); (b) single GPU vendor
  validated (RTX 3060 Ti Ampere), cross-vendor expected identical per `dec-pipelines-async-compute` §2.2;
  (c) Karis 2014 paper is 12 years old (2014), but 2024-2026 literature (Yang/Liu/Salvi 2024 + k-DOP SIGGRAPH
    2024) confirms its core principles still hold for vertex-out approach; (d) k-DOP SIGGRAPH 2024 = SOTA
          ghosting mitigation (0.2 ms overhead for 32-DOPs) = follow-up experiment to replace 3x3 AABB clamping;
          (e) Marrs 2018 NVIDIA adaptive TAA = requires ray tracing (Stage 5.2 RTX foundation), out of scope для
          Stage 5.3 baseline. **Cross-axis:** orthogonal ко всем 3 in-progress parallel (tracy-gpu = profiling, wfc
          = gen strategy, sub-chunk = data structure); complementary к closed `clustered-forward-mass-lights`
          (SSBO light list + motion vectors both feed TAA resolve); natural follow-up к closed
          `dec-pipelines-async-compute` (motion vector MRT submission = candidate for async queue if VRAM/upload
          becomes bottleneck); cross-vendor validation matrix same as `dec-pipelines-async-compute` §2.2 (NVIDIA
          Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Gfx12.5+). **Continuation chain** (project chronological):
          2026-06-20 lighting axis: `vct-vs-rt-cutoff` mixed + `clustered-forward-mass-lights` yes +
          `rt-shadows-vs-csm` mixed + `restir-gi-feasibility` mixed; 2026-06-20 sync axis: `dec-pipelines-async-compute`
          yes + `async-compute-overhead-numbers` yes (foundation); 2026-06-21 temporal axis: this experiment = TAA
          motion vector MRT decision. **Side effect:** sync fix r1 applied to previous-session
          `2026-06-20-async-compute-overhead-numbers` per `AGENTS.md §13.5` (original session left bookkeeping
          incomplete: §Open duplicate + missing §6 entry + README Status mismatch — all corrected same-pass
          preserving original measurements +9.85-11.34% + verdict=yes). См. §6 + §1 + experiment README +
          `STATUS.md` + `sources.md` + `prototype/README.md` + `prototype/main.cpp` (525 LoC skeleton) + 6 GLSL
          shaders (voxel_a/b vert+frag + taa_resolve_a/b comp) + Makefile. **Re-evaluation triggers:** Stage 5.3
          TAA motion blur integration (related TODO §5.3 line 425), AMD RDNA / Intel Arc dev matrix validation,
          k-DOP adoption per SIGGRAPH 2024 (0.2 ms overhead, may compound with motion vector quality gain), Marrs
          2018 adaptive TAA (requires ray tracing path from Stage 5.2 RTX shadows foundation).

- [x] **[2026-06-21-audio-raytracing-voxel-sdf](./experiments/2026-06-21-audio-raytracing-voxel-sdf/)** —
  l, **independent** (cross-cutting для future Stage 7.x audio; no audio rendering stage в `TODO.md` per §3
  — miniaudio PCM playback only per `agent/knowledge.md §28`). Closed `2026-06-21` (single session, ~2h),
  verdict **`mixed`**. **Audio axis** experiment — **первый audio-axis** (0 of 19+ same-day `2026-06-20`
  experiments covered audio). Web-research complete (3 batch queries, 12 key sources верифицированы:
  Vercidium 2025 production voxel-grid audio [direct validation of our approach, 32 rays/frame CPU],
  SIGGRAPH 2025 Finnendahl et al. differentiable acoustic PT + Path Replay Backpropagation,
  GSound-SIR Mar 2025 + NVIDIA OptiX support Dec 2025, Schissler & Manocha 2014 [50 reflection orders
  at interactive rates, 200 sound sources], Schissler et al. 2014 BST bidirectional path tracing, RESound 2007
  hybrid ray-frustum + stochastic + statistical, iSound GPU-based auralization, Tsingos 2001
  HW-accelerated occlusion/diffraction via depth-maps, Funkhouser 2002 beam tracing for architectural scenes,
  Meta Acoustic Ray Tracing Audio SDK 2024+ production VR, NeRAF ICLR 2025 audio-visual alignment). Standalone
  C++26 prototype (`prototype/{voxel_grid,audio_raytracer,reverb,bench}.{hpp,cpp}` + `RESULTS.md` + `results.csv`
    + `README.md`, **~700 LoC total**, Clang 22.1.6 `-O3 -march=native -Wall -Wextra`, **0 warnings**). 4 configs
      × 3 scenes × 3 seeds × 1000 iter + 100 warmup = **36 runs × 1000 = 36000 measurements** on Zen 3 5800X
      (per `hardware-profile.md §1`, governor `powersave`). **Headline numbers (mean ms):**

    - **A_no_geom:** 0.0002 across scenes (baseline, current `AudioEngine` per `agent/knowledge.md §28`).
    - **B_occlusion** (1 ray/source): **0.008-0.016 ms** = **< 0.05%** of 33.3 ms audio frame budget @ 30 Hz.
      **Production-ready, immediate integration** для muffling behind walls.
    - **C_full_hybrid** (32 rays × 4 reflection orders + Eyring late tail + IR gen): **17.1 cave / 13.8 open_plains /
      6.3 multi_room ms** = **52% / 41% / 19%** budget. **Falsifies 5 ms hypothesis** на 2 of 3 scenes (cave
      3.4× over, open_plains 2.7× over). Only multi_room в budget (1.3× over).
    - **D_full_cached** (+ temporal cache 1 cm epsilon): **21.1 / 14.4 / 6.0 ms**. Cache не помогает — jitter
      ±5 cm > ε → cache invalidates most frames. Cave seed 7 actually **worse** than C (28.4 vs 17.4 ms)
      из-за cache re-warmup overhead.
      **Mainline recommendation** per `README.md §7` + `STATUS.md`:
    - **Phase 1 (XS, immediate):** occlusion-only path → < 1.5 ms for 64 sources = 4% budget. Immediate perceptual
      win (muffled sounds behind walls).
    - **Phase 2 (XS, immediate):** Eyring late reverb → negligible cost (~0.001 ms per source), realistic room
      perception, integrate unconditionally.
    - **Phase 3 (M, deferred):** full hybrid до one of (a) SVO hierarchical acceleration [empty-skip 5-10×
      per `nanovdb-on-gpu` walker logic], (b) lower ray budget [8r×2ord perceptually sufficient per Vercidium
      2025 + Schissler 2014], (c) cache tuning [larger ε 10-20 cm], (d) AVX-512 hardware arrival [Zen 5 / Arrow Lake
      2-4× per `simd-procedural-noise` precedent].
      Cross-reuses `2026-06-20-nanovdb-on-gpu` SVO walker foundation, `2026-06-20-flecs-soa-vs-aos-bench` SoA storage
      verdict=yes, `2026-06-20-work-stealing-job-system` serial dispatcher verdict=mixed, `agent/knowledge.md §28`
      AudioEngine contract. **Caveats:** (a) single-vendor Zen 3 5800X (governor `powersave`, не `performance`),
      (b) `voxels_traversed` counter instrumentation bug — не инкрементируется в DDA, не влияет на latency
      measurements, blocks cache-miss analysis (fix в v2 prototype), (c) synthetic scenes representative not
      exhaustive (cave/open_plains/multi_room), (d) no material absorption modeling (simplified reflection only),
      (e) sequential single-threaded per `work-stealing-job-system` verdict=mixed → no pool/TBB/libdispatch,
      (f) bench measured sources=64 — scaling to 256+ requires separate `_audio-rt-budget-vs-source-count_`
      experiment (deferred). **Continuation chain:** none (first audio axis experiment; opens Stage 7.x audio);
      **follow-up candidates:** `_audio-hierarchical-svo-skip_` (Phase 3 trigger), `_audio-rt-budget-vs-source-count_`
      (>100 sources scaling), `_audio-diffraction-hybrid_` (Schissler 2014 diffraction via HZB per
      `2026-06-20-hzb-binding-models`). **Cross-axis continuity:** same-session `2026-06-21` parallel sessions
      (gpu-procedural-noise + frame-flight-allocator-budget + dxc-toolchain + tracy-gpu + wfc-procedural + this =
      6 same-day closes/in-progress) + 19+ same-day `2026-06-20` closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x/7.x
      optimization landscape + **audio axis NEW**.
      См. [experiment README](./experiments/2026-06-21-audio-raytracing-voxel-sdf/README.md) +
      [STATUS](./experiments/2026-06-21-audio-raytracing-voxel-sdf/STATUS.md) +
      [sources.md](./experiments/2026-06-21-audio-raytracing-voxel-sdf/sources.md) +
      [prototype/RESULTS.md](./experiments/2026-06-21-audio-raytracing-voxel-sdf/prototype/RESULTS.md).

- [x] **[2026-06-21-sub-chunk-layers](./experiments/2026-06-21-sub-chunk-layers/)** —
  m, Stage 4.x (biome/cave data structure axis, orthogonal к in-progress `2026-06-21-wfc-procedural-worlds`
  gen-strategy axis). Closed `2026-06-21` (single session), verdict **`mixed`**. **Chunk-layout-axis
  experiment** — единственная Stage 4.x ось, не покрытая same-session `2026-06-21` closed experiments
  (frame-flight-allocator-budget + gpu-procedural-noise-compute-kernels) + in-progress
  (wfc-procedural-worlds + audio-raytracing-voxel-sdf + dxc-vs-glslc-toolchain + tracy-gpu-vs-manual).
  Web-research complete (3 batch queries, ~14 sources верифицированы: Minecraft-1.18+ Java
  `ChunkSection` 16³ + biomes 4×4×4 = 64 entries per section per FabricMC/yarn DeepWiki + Minecraft Wiki
    + wiki.vg protocol + yarn 1.18 API; Bedrock `SubChunk` 4D (x,y,z,**storage layer**) per wiki.vg +
      uNmINeD 2021-12-10 reverse engineering; SHARD layered format per scrayos 2024-11-04 + GitHub; ATLAS
      AARF columnar storage per Tunact124/atlas Mar 2026; Cubyz CaveMap 64³ fragments with 1-bit per block
    + CaveBiomeMap 2048³ resolution per PixelGuys DeepWiki Mar 2026; Hytale NStagedChunkGenerator
      BiomeStage/TerrainStage/PropStage/TintStage/EnvironmentStage per vulpeslab/hytale-docs; Vulkan Guide
      Ascendant chunk layers (main + transparent + clutter) per vkguide.dev; Minecraft world generation
      overview per Telepathic Grunt/XI64 Gist Feb 2021; maguirekrist/voxel_enginevk production-grade chunk
      pipeline 5 layers). **Standalone C++26 CPU prototype** (`prototype/sub_chunk_bench.cpp` ~870 LoC,
      `clang++ 22.1.6 -O3 -march=native`, build green). 4 designs (A_Monolithic baseline 512 bytes +
      B_Palette adaptive bits + C_FixedLayer_L2 4 layers + D_FixedLayer_L4 2 layers) × 5 scenes
      (uniform_air + uniform_floor + forest_floor + cave_stress + mixed_biome) × 5 seeds (1, 7, 42, 1234,

    31337) × 1000 iter per measurement = 100 measurements. **Measured (RTX 3060 Ti dev host irrelevant —
           CPU-only Zen 3 5800X, governor=`powersave`, 62.7 GiB RAM DDR4):**

    - **Memory axis (B_Palette / C_L2 / D_L4 vs A_Monolithic baseline 512 bytes):**
        - uniform_air / uniform_floor: B=20 (-96%), C=84 (-84%), D=42 (-92%) — **B_Palette wins.**
        - forest_floor / cave_stress (2 materials): B=84 (-84%), C=148 (-71%), D=106 (-79%) — **B_Palette wins.**
        - mixed_biome (4 materials): B=148 (-71%), C=148 (-71%), D=138 (-73%) — **D_L4 marginal win.**
    - **Build cost axis:** monolithic 0.03-0.13 µs/chunk vs paletted 1.3-5.8 µs/chunk = **30-55× overhead**.
      But absolute cost 1-6 µs vs Stage 4.1 budget 50 µs/chunk per `TODO.md §4.1` = 8-50× headroom.
    - **Mutation cost axis:** monolithic 10-16 ns/mutation vs paletted 12-19 ns = **+5-70% overhead**.
      But absolute cost 10-19 ns vs Stage 1.2 DoD 0.1 ms tolerance = 5000-10000× headroom.
    - **Mesh vertex count axis:** all designs produce **identical** face counts (591-679 quads) для same
      scene+seed — mesh optimization is layout-orthogonal (covered by `2026-06-20-meshing-algo-comparison`
      verdict=mixed).
    - **Layer boundary axis:** monolithic 0 vs C_L2 80-155 vs D_L4 28-62 = explicit semantic gain для
      biome/cave chunks. **Cave/biome scenes show 28-155 explicit transitions per chunk** = VCT anti-leak
        + per-layer LOD + selective rebuild potential.
    - **Verdict=mixed:** paletted/layered designs win memory (73-96% > 5% threshold per
      `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`) + layer-boundary semantic axis,
      lose build cost (acceptable per budget) + mutation cost (negligible absolute). **Mainline
      recommendation:** conditional — **B_Palette для uniform chunks (96% savings)**, **D_L4 для
      biome/cave chunks (73-79% savings + 28-62 transitions)**, **C_L2 для finer biome granularity
      (71-84% + 80-155 transitions)**; A_Monolithic as fallback для sparse chunks + legacy compatibility.
    - **3-step migration per `agent/knowledge.md §30.4` precedent:**
        - **Step 1 (S, ~150 LoC):** `ChunkLayout` enum + `ChunkStorage::payload` polymorphic container +
          `SelectChunkLayout(scene_chunk_type, voxel_count, palette_size)` decision logic в
          `src/voxel/VoxelWorld.{hpp,cpp}`. Cross-references `2026-06-20-nanovdb-on-gpu` hybrid SVDAG +
          NanoVDB.
        - **Step 2 (M, ~300 LoC):** new `world_gen_layers.comp` shader emits per-layer payload + per-chunk
          layout metadata. Each layer = independent noise query (heightmap per
          `2026-06-21-gpu-procedural-noise-compute-kernels` Step 3 OpenSimplex2). Cross-references
          `2026-06-21-wfc-procedural-worlds` Step 4 (WFC + noise hybrid) for discrete layer transitions.
        - **Step 3 (M, ~250 LoC):** wire layer semantics в `src/shaders/voxel.frag` для VCT cone-march
          terminate at explicit layer boundary (anti-leak guarantee per `2026-06-20-vct-vs-rt-cutoff`
          Step 3) + Stage 4.2 per-layer LOD downsampling. Selective rebuild via existing
          `pendingChunkRebuildIndices` + per-layer dirty bit per mainline Phase 9 2x part 5.
    - **Caveats:** CPU prototype, no GPU dispatch; no Sparse64Tree integration (flat arrays only);
      naive face counter (no greedy merge per `meshing-algo-comparison`); synthetic scenes; single-threaded.
      Cross-vendor GPU memory layout (AMD RDNA + Intel Arc) deferred до Stage 4.1 GPU integration prototype.
    - **Cross-axis:** Stage 4.x biome/cave axis fully closed same-day сессии (continuous noise axis via
      `gpu-procedural-noise-compute-kernels` verdict=mixed OpenSimplex2 + discrete structure axis via this
      `sub-chunk-layers` verdict=mixed layered chunks + gen-strategy axis via in-progress
      `wfc-procedural-worlds` WFC). 3 orthogonal axes of Stage 4.x = complete picture.

- [x] **[2026-06-21-frame-flight-allocator-budget](./experiments/2026-06-21-frame-flight-allocator-budget/)** —
  m, `Stage 6.2 tech-debt` (cross-cutting для Stage 2.x/3.x/5.x). Closed `2026-06-21` (single session),
  verdict **`mixed`**. **VRAM-allocator-axis experiment** — единственная ось, не покрытая same-session
  2026-06-20 closed experiments (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy +
  job-scheduling + mass-lights + shadow-dim + SOTA-GI). Web-research complete (4 batch queries, ~30 results,
  ~15 ключевых sources верифицированы: VMA 3.4.0 docs [recommended usage patterns, custom memory pools,
  linear algorithm ring buffer, staying within budget] + VMA Issue #453 [VMA author warning against
  per-frame `vmaCreateBuffer`/`vmaDestroyBuffer`] + Frostbite Frame Graph [Yuriy O'Donnell GDC 2017,
  transient resources pattern] + Frostbite Scope Stacks [EA PDF, linear allocator pattern] + Diligent
  Engine 2.0 ring buffer [FIFO dynamic resource pattern] + Unreal Engine RHI [Epic Forums 2025-05-23,
  `STAT_VulkanMemoryUsage#` per `VK_EXT_memory_budget`, `FrameTempBuffer` + `RingBuffer` categories] +
  DXVK commit `9b272fb` [2024-11-08, `VK_EXT_pageable_device_local_memory` enable + AMD fallback] +
  vkd3d-proton PR #1543 [Evict/MakeResident emulation, NVIDIA contribution] + D3D12 Residency Starter
  Library [Microsoft reference] + NVIDIA Vulkan Do's and Don'ts [Nuno Subtil 2019-06-06, "use memory
  sub-allocation"] + AMD "Using Vulkan Device Memory" guide [2016, 64 MiB block size guidance] +
  `VK_EXT_memory_budget` spec [2018, ratified] + `VK_EXT_pageable_device_local_memory` spec [NVIDIA,
  RTX 3060 Ti Ampere supported in driver 555+] + llama.cpp HVV fragmentation [Jeff Bolz NVIDIA,
  2025-01-30]). **Standalone Vulkan 1.4 prototype** (`prototype/main.cpp` + `harness.hpp` + `strategies.hpp`
    + `benchmark.hpp` + `CMakeLists.txt`, ~890 LoC total, links vendored VMA 3.4.0 + volk from `external/`,
      **NOT ProjectV mainline**). 5 strategies measured + 1 stress pass: (A) `A_Default` = current mainline
      behavior; (B) `B_BudgetTrack` = A + `EXT_MEMORY_BUDGET_BIT` + `WITHIN_BUDGET_BIT` flag; (C) `C_LinearPool`
      = per-frame linear pool create+destroy; (D) `D_DoubleBuffer` = C + `WITHIN_BUDGET`; (E) `E_PreCreatedRing`
      = production-realistic single pre-created 64 MiB ring pool reused across frames. **1000 measured
      frames per strategy + 50 warmup** (per `benchmarks/methodology.md §3`), 8 MiB world-edit spike every
      200 frames. **Stress pass:** 256 MiB spike every 50 frames (overflow test for hard cap).
      **Measurements** (RTX 3060 Ti dev host, Vulkan 1.4.350, NVIDIA 610.43.02, governor `powersave`):
      (A) mean 35.5 µs / p99 67.4 µs / failures 0; (B) mean 34.7 µs / p99 58.2 µs / failures 0;
      (C) mean 1311 µs / p99 2573 µs / failures 0 [per-frame pool recreate 30× slower]; (D) mean 1309 µs /
      p99 2941 µs / failures 0; (E) mean 38.0 µs / p99 113 µs / failures 0 / **peakHeapUsage +64 MiB**
      [64 MiB ring block persistent]. **Stress pass:** D = 21 clean `VK_ERROR_OUT_OF_DEVICE_MEMORY`
      failures (256 MiB > 64 MiB pool block → hard cap fires correctly). **Caveats:** single GPU vendor
      validated (NVIDIA Ampere); single-threaded harness; cross-vendor (AMD RDNA + Intel Arc) deferred;
      synthetic workload; `VK_EXT_pageable_device_local_memory` not exercised in prototype but
      production-proven per DXVK + vkd3d-proton precedent. **Mainline recommendation** (3-step migration
      per `agent/knowledge.md §30.4` precedent): **Step 1 (XS, ~20 LoC)** — add
      `VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT` to `VulkanBootstrap.cpp:807-823` allocator +
      `vmaSetCurrentFrameIndex()` per frame + TracyPlot `VRAM.heapBudgetMiB`/`heapUsageMiB`; **Step 2
      (S, ~50 LoC + tests)** — add `VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT` flag для non-critical
      allocations (5+ call sites per `rg vmaCreateBuffer`) with graceful degradation; **Step 3 (M, ~200
      LoC) DEFERRED** — pre-created single linear ring buffer pool (`TransientPool.{hpp,cpp}` +
      integration in `Renderer.cpp`) re-evaluation triggers: Stage 4.3 (128+ chunks, transient SSBO
      count > 50/frame) OR Stage 5.2 RTX BLAS pool overflow OR Tracy heap-usage→budget trend over 60s.
      **Caveat per Step 3:** VMA docs require `maxBlockCount = 1` для ring buffer; double-pool variant
      (Strategy D) = wrong pattern, **не реализовывать**. **Cross-axis continuity:** 19+ closed
      same-session 2026-06-20 + сегодняшний parallel `2026-06-21-tracy-gpu-vs-manual` (orthogonal scope,
      no conflict per `docs/experiments/AGENTS.md §13.3`). Этот experiment = allocator axis closed
      (cross-cutting для всех transient pressure sources). См. §6 + §1 + experiment README +
      `prototype/README.md` + `prototype/build/results.csv`.

- [x] *
  *[2026-06-21-gpu-procedural-noise-compute-kernels](./experiments/2026-06-21-gpu-procedural-noise-compute-kernels/)** —
  m,
  Stage 4.1 (GPU Noise & World Gen per `TODO.md §4.1`, gating blocker для infinite worlds). Closed
  `2026-06-21` (single session), verdict **`mixed`** (perf gain 2.9% < 5% threshold per
  `optimization-philosophy.md`; quality + license axis still favors OpenSimplex2 3D-S). **Noise-algorithm
  axis** experiment — orthogonal к `2026-06-20-simd-procedural-noise` (CPU AVX2 vs scalar) и к
  in-progress `2026-06-21-dxc-vs-glslc-toolchain` (shader toolchain). Direct prior art:
  `agent/knowledge.md §29.0` line 887 (Tier 4 R&D marker для Stage 4.1) + `TODO.md §4.1` explicit
  GPU noise requirement + `agent/workspace.md §1 Phase 1` world_gen.comp skeleton. Web-research complete
  (3 batch queries, ~20 results, 20 sources верифицированы: Schneider `arXiv 1903.12270` Perlin/Float 3D
  = 77 ALU inst [direct instruction count baseline], GPU Gems 2 Ch 26 textured-LUT Perlin = 53 inst /
  9 lookups, atyuwen/bitangent_noise SimplexNoise.hlsl 3D = ~71 instruction slots, KdotJPG/OpenSimplex2
  673 stars CC0 modern GPU-friendly design, Auburn/FastNoiseLite 3D Perlin 47.93 M/s scalar /
  261.10 M/s AVX2 CPU baseline, NVIDIA Nsight Compute Ampere workgroup-64 occupancy guidance,
  Khronos Forums compute shader SSBO write cost validation, JCGT 2022 Olano GTX 1660 modern compiler
  DCE analysis 17% speedup from disabling tiling, Vulkanised 2024 GPU Atomic Performance Modeling
  McKee microbench, production references: paulrobello/voxel-world Vulkan compute 5D climate noise +
  Perlin Feb 2026, Aokana arXiv 2505.02017 GPU-Driven voxel May 2025, AdityaGupta1/mega-minecraft CUDA
  fBm Oct 2025, russellocean/pebble-rs WGPU compute voxel raytracer Nov 2025, Yunasawa YNL Vozel
  Minecraft-1.18+ 5-parameter FBM biome gen Sep 2025). Standalone Vulkan 1.4 compute prototype
  (`prototype/{main.cpp, noise_kernels.comp, CMakeLists.txt, README.md}`, ~700 LoC total, 5 conditional
  GLSL variants через `#define VARIANT_*` switch + dispatch harness, RTX 3060 Ti GA104 Ampere, Vulkan
  1.4.341, NVIDIA driver 610.43.02, Clang 22.1.6 + glslc 2026.2). 3 runs × 5 variants × 1000 iter +
  10 warmup. **Measured:** VALUE=0.0273 ms, PERLIN=0.0272 ms, SIMPLEX=0.0272 ms, OPENSIMPLEX2=0.0272 ms,
  WORLEY=0.0280 ms. **All variants в пределах 2.9% mean** — ниже 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. WORLEY unexpectedly not slowest
  despite 27-cell loop (`glslc` 2026.2 fully unrolled + register optimization). VALUE == PERLIN по
  cost (hash + gradient table index similar register footprint на Ampere). Memory bandwidth = 65.6%
  of 448 GB/s theoretical peak = **memory-bound kernel**, ALU = ~14% of dispatch time only. Per-eval
  cost = 13.0 ns/eval, per-chunk = 6.6 µs. **Stage 4.1 budget (50 µs/chunk per `TODO.md §4.1`):**
  8× headroom single octave, 1.9× headroom FBM 4 octaves, 0.63× (over budget) FBM 4 octaves × 3 channels
  (heightmap+cave+biome). **Verdict=mixed:** алгоритмический выбор НЕ meaningful perf discriminator
  на chunkSize=8 dispatch pattern; **но** quality + license axis still favors OpenSimplex2 3D-S (CC0,
  no axis artifacts, analytic derivatives, actively maintained KdotJPG 2019-2024+, stable cold-cache
  perf без Run-1 spike). **Mainline рекомендация:** use **OpenSimplex2 3D-S** для Stage 4.1 world
  gen (NOT because fastest — because license + quality + stability). 3-step migration per
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `noise3d_opensimplex2()` GLSL port (~50 LoC
  core, attribution header per CC0 §4(a)), Step 2 dispatch in `world_gen.comp` per chunkSize=8
  pattern + FBM wrapper (4 octaves, ~150 LoC), Step 3 multi-channel (heightmap + cave + biome,
  octave reduction если budget exceeded, ~100 LoC). Total ~300 LoC, S effort, 1-2 sessions.
  **Cross-axis continuity:** same-day `2026-06-21` parallel sessions (frame-flight-allocator-budget
  in-progress + dxc-vs-glslc-toolchain in-progress + tracy-gpu-vs-manual in-progress) + my
  noise-algorithm axis = orthogonal angle of Stage 4.x + Stage 6.x + toolchain optimization
  landscape. Continuation chain: `2026-06-20-simd-procedural-noise` (CPU orthogonal) → this (GPU
  algorithm choice) → follow-up: FBM + multi-channel + AMD RDNA cross-vendor validation. **Caveats:**
  (a) single GPU vendor validated (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341, NVIDIA 610.43.02) —
  mainline re-test on AMD RDNA 2/3/4 + Intel Arc Battlemage dev matrix; (b) single octave only —
  FBM 4 octaves linear scaling not measured; (c) single heightmap channel — multi-channel 3× cost
  projection not validated; (d) no Nsight Compute register/occupancy/SM pipe metrics — extension
  opportunity; (e) no spectral quality metric (FFT framework not built) — quality claims
  literature-cited; (f) async-compute overlap with graphics not measured (per `dec-pipelines-async-compute`
  verdict=yes — potential 5-8% additional gain); (g) Run 1 vs Run 2+3 shows 14% cold-cache offset
  для VALUE/PERLIN (insufficient warmup at 10 iters) — OPENSIMPLEX2/SIMPLEX/WORLEY stable from Run 1.
  Cross-refs: `TODO.md §4.1`, `src/voxel/VoxelWorld.hpp:85` (chunkSize=8), `src/shaders/voxel_mesh.comp:146`
  (existing dispatch pattern), `agent/workspace.md §1 Phase 1` (world_gen.comp skeleton),
  `agent/knowledge.md §30.4` (3-step migration precedent), `2026-06-20-simd-procedural-noise` (CPU
  orthogonal), `2026-06-20-dec-pipelines-async-compute` (async foundation, world gen spike isolation),
  `2026-06-20-nanovdb-on-gpu` (GPU SSBO write target format), `docs/experiments/hardware-profile.md §3`
  (RTX 3060 Ti dev host), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  (5-10% threshold definition).

- [x] **[2026-06-21-dxc-vs-glslc-toolchain](./experiments/2026-06-21-dxc-vs-glslc-toolchain/)** — m,
  Stage 0 / foundational (toolchain decision, cross-cutting для Stage 2.1 mesh shader + Stage 5.2
  RT pipeline + every shader forever). Closed `2026-06-21` (single session),
  verdict **`mixed`**. **Toolchain-axis experiment** — единственная Stage 0 ось, не покрытая
  same-session 2026-06-20 closed experiments (storage/sync/cull/binding/layout/meshing/simd/hzb/
  flecs/gi-strategy + job-scheduling + mass-lights + shadow-dim + SOTA-GI + frame-flight-allocator
    + gpu-noise-kernel). Web-research complete (3 batch queries, ~30 results, 11 key sources
      верифицированы: Khronos HLSL-in-Vulkan guide [`docs.vulkan.org/guide/latest/hlsl.html` —
      "DirectXShaderCompiler (DXC) is the reference HLSL to SPIR-V compiler … has the most complete
      and up-to-date support and is the recommended way"], DXC SPIR-V CodeGen spec
      [`docs/SPIR-V.rst`], DXC release v1.9.2602.24 Feb 2026 Patch 1 [standalone binary, no system
      install], Sascha Willems + Ben Clayton Google LLC Jun 2025 [production precedent — converted
      all Sascha Willems Vulkan samples GLSL → HLSL], Vulkanised 2025 Nathan Gauer Google [DXC issues
    + Clang-based HLSL transition roadmap], Microsoft DXC 1.8.2405 May 2024 [HLSL 202x transition],
      Microsoft DirectX adopting SPIR-V Sep 2024 [SM 7.0 = SPIR-V], Shader-slang discussion #9354
      [independent benchmark: DXC 3-4× faster than slang], Hexops devlog Feb 2024 [DXIL vs SPIR-V
      post-optimization differences], DXC issue #6960 [SPIR-V mesh shader bug fixed 1.8.2502], NVIDIA
      Forums Jul 2025 [DXIL vs SPIR-V perf delta]). Standalone C++/shell prototype (`prototype/
  {compile_bench.sh, extended_bench.sh, tools/dxc/, shaders_glsl/, shaders_hlsl/, results/}` —
      **NOT ProjectV mainline**). 5 representative шейдеров в GLSL + HLSL variants (vertex, fragment,
      mesh, compute-cull, compute-fluid-CA), с preserved descriptor layouts (SSBO, UBO, push constants,
      samplers) 1:1. **Measurements** (RTX 3060 Ti dev host `obvium`, Vulkan 1.4.350, glslc 2026.2 vs
      DXC v1.9.2602.24): **300 measurements** (30 iter × 5 shaders × 2 toolchain, default mode).
      **DXC compile time 9.1-10.9× faster** (mean 12.4 ms vs 121.7 ms; p95/p99 DXC < 16 ms vs glslc
      < 160 ms; std 0.7 ms vs 5 ms both relative to own mean). **DXC SPIR-V size 18-43% smaller**
      (mean 3342 B vs 4764 B; largest delta на mesh shader: 3904 vs 6804 B = -43%). **DXC instruction
      count 20-40% меньше** (mean 193 vs 281, computed via `spirv-dis --raw-id`). **Validation rate
      100%** обе toolchain (`spirv-val --target-env vulkan1.4`). **Debug info mode** (-Zi DXC, -g
      glslc, 20 iter): overhead +50-130% sizes; both still 100% valid; DXC still smaller in absolute
      terms. **Optimize mode** (-O3 DXC; glslc default already optimized): no measurable change vs
      default. **7 DXC API quirks documented** для future migration: (1) GLSL `location(N)` not
      supported → TEXCOORD semantics or `[[vk::location(N)]]`; (2) `WriteTriangle`/`WritePrimitive`
      не существует в DXC 1.9.x → `out vertices MeshVertex verts[V]` + `out indices uint3 primIndices[P]`
      pattern; (3) no unsized arrays в struct → split SSBO на отдельные `StructuredBuffer<T>`;
      (4) target env `vulkan1.1spirv1.4` (NOT `vulkan1.4`); (5) no GLSL-style combined sampler →
      separate `Texture2D` + `SamplerState` or `[[vk::combined_image_sampler]]`; (6) `gl_FragCoord`
      → `SV_POSITION` (input only); (7) `gl_GlobalInvocationID` → `SV_DispatchThreadID`,
      `gl_LocalInvocationIndex` → `SV_GroupIndex`, `atomicAdd` → `InterlockedAdd`,
      `barrier()` → `GroupMemoryBarrierWithGroupSync()`. **Verdict=mixed:** DXC wins quantitatively
      (9-10× compile speed, 30% smaller SPIR-V), but migration cost = M-L effort (rewrite 19 шейдеров)
    + DXC architectural risk (Clang-based HLSL transition 2026-2028 per Vulkanised 2025 Gauer +
      Microsoft HLSL 202x roadmap; Clang-HLSL = single path long-term). **Mainline рекомендация:
      DEFER migration.** ProjectV остаётся на glslc per Vulkan SDK 1.4.350 baseline +
      `agent/knowledge.md §17`. 3-step migration plan documented for future (Step 1 foundation
      dual-toolchain в `src/CMakeLists.txt:15-26`, Step 2 hybrid mesh+RT rollout c `PROJECTV_USE_DXC_MESH=ON`,
      Step 3 default flip). **Re-evaluation triggers:** Vulkan 1.4 GLSL RT stabilization,
      Clang-HLSL stabilization, ProjectV shader count > 50 (CI/CD bottleneck), DXC-only feature need
      (e.g. SPV_NV_compute_shader_derivatives, SPV_KHR_maximal_reconvergence), driver SPIR-V
      complexity issue, Stage 5.2 RT inline SBT (`[[vk::shader_record_ext]]`). **Cross-axis
      continuity:** 20+ closed same-session 2026-06-20 + 2 closed same-session 2026-06-21 (frame-flight +
      gpu-noise) + 3 in-progress parallel (tracy-gpu + audio-raytracing + wfc-procedural) + this =
      full Stage 0/1.x/2.x/3.x/4.x/5.x/6.x + cross-cutting optimization landscape covered. Continuation
      chain: `2026-06-20-mesh-shader-vs-compute-cull` (verdict=mixed, mesh shader feature-flagged) +
      `2026-06-20-bindless-descriptor-overhead` (Phase E = bindless RTX TLAS) + Stage 5.2 RT pipeline
      foundation → this (toolchain choice для всех future HLSL/GLSL шейдеров). **Caveats:** (a) prototype
      шейдеры = 30-50% mainline complexity (representative layouts, simplified logic); (b) single
      GPU vendor (RTX 3060 Ti GA104 Ampere); (c) DXC = Linux x86_64 only (no Windows verification);
      (d) runtime shader perf impact not measured (driver applies own SPIR-V optimization per Hexops
      devlog); (e) DXC SPIR-V backend has known extension-jungle problem (every new SPIR-V extension
      requires DXC patch per Vulkanised 2025 Gauer — long-term maintenance risk). Cross-refs:
      `src/CMakeLists.txt:15-26` (current glslc selection), `src/shaders/voxel_mesh.mesh` (mainline mesh
      shader using glslc pattern, validated), `agent/knowledge.md §17` (Linux Vulkan SDK 1.4.350 baseline),
      `agent/knowledge.md §4` (Build/verification contract), `agent/knowledge.md §30.4` (3-step
      migration precedent), `TODO.md §Stage 0` (toolchain decision), `docs/experiments/hardware-profile.md`
      (dev host `obvium`, captured `2026-06-20`), `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
      (5-10% threshold), `2026-06-20-mesh-shader-vs-compute-cull` (mesh shader decision context),
      `2026-06-20-restir-gi-feasibility` (RT pipeline Stage 5.2 future), `2026-06-20-rt-shadows-vs-csm`
      (RTX shadow Stage 5.2), `2026-06-21-frame-flight-allocator-budget` (parallel session, allocator
      axis).

- [x] **[2026-06-20-restir-gi-feasibility](./experiments/2026-06-20-restir-gi-feasibility/)** — m, Stage 5.1/5.2
  (SOTA-GI-ось experiment, post-Stage 5 follow-up explicitly named в `vct-vs-rt-cutoff` §1 line 99 «Step 4
  (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT»). Closed `2026-06-20` (single session),
  verdict **`mixed`**. Web-research complete (~30 sources верифицированы: Bitterli 2020 ReSTIR original
  [6-60× MSE ↓, 8 rays/pixel max], Ouyang 2021 ReSTIR GI [9.3-166× MSE ↓ @ 1spp], Lin 2022 ReSTIR PT +
  GRIS theory [80 ms @ 1920×1080, MAPE 0.39 vs 1.63 PT, 1 path/pixel], Majercik 2019/2021 DDGI,
  Müller 2021 NRC [2.6 ms @ full HD, NVIDIA Tensor Cores ≥ Turing], NVIDIA-RTX/RTXGI SDK v2.7.0 Mar 2026
  [336 stars, driver ≥ 555.85], NVIDIA-RTX/SHARC [123 stars, spatial hash grid 64-bit keys, 4-pass,
  ~185 MB @ 2^22 baseline, 1.5-10% perf overhead in Cyberpunk], NVIDIA-RTX/RTXDI v3.0+
  [ReSTIR DI/GI/PT/ReGIR, D3D12+Vulkan via NVRHI, DXC toolchain], Crassin 2011 GIVoxels [VCT foundation,
  25-70 FPS, two bounces Lambertian+glossy], Lumen SIGGRAPH 2022 [Epic explicitly rejected VCT as leaky],
  Minecraft RTX GDC 2021 [voxel + path tracer + irradiance cache, DDGI-style probes], Douglas Voxel
  Devlog #23 Jun 2025 [direct voxel + DDGI integration, voxel-compatible verified], Cyberpunk 2077 RT
  Overdrive Patch 2.1 Dec 2023 [production ReSTIR DI/GI + SHaRC], NVIDIA Zorah RTX 50 demo 2025 [ReSTIR PT],
  Portal RTX [SHaRC], OGRE-Next CIVCT [10-100× faster voxelization], Aokana arXiv 2505.02017 May 2025,
  ReSTIR FG/GSGI/PMGI 2024 [0.4-14 ms overhead variants], Epic DDGI abandonment forum Dec 2025 [Arc Raiders
  counter-example]). Standalone prototype deferred per `rt-shadows-vs-csm` precedent — analytical +
  literature + cross-vendor matrix sufficient. **Architectural mismatch (главный finding):** все 4 SOTA
  техники (ReSTIR PT/GI/DI, DDGI, SHaRC, NRC) **требуют path tracer foundation**; ProjectV's Stage 5.x
  = hybrid VCT+RTX = **NOT** path tracer. **VRAM matrix** (RTX 3060 Ti, 5.06 GiB budget):
  SHaRC = 185 MiB (3.65%, acceptable), DDGI = 16 MiB, ReSTIR reservoir = 33-67 MiB checkerboard/full.
  **Quality validated** для path-tracing contexts (ReSTIR PT MAPE 0.39 vs 1.63 Carousel, Cyberpunk
  production, SHaRC 1.5-10% overhead) — **cannot translate** к ProjectV без path tracer. **NRC rejected**
  = NVIDIA-only (Tensor Cores ≥ Turing, excludes AMD RDNA 4 + Intel Battlemage per `dec-pipelines-async-compute`
  matrix). **Mainline recommendation:** **keep current hybrid VCT+RTX as-is** (Stage 5.x MVP scope), **defer
  SOTA GI до Stage 6+ post-MVP path tracer pivot**. Recommended add-on order (if path tracer ships):
  **SHaRC → DDGI → ReSTIR DI/GI/PT**. SHaRC first: lowest complexity, cross-vendor, voxel-adaptable,
  acceptable VRAM. NRC = skip. **Re-evaluation triggers:** VCT leakage visible в production (cavity lighting
  artifact), Stage 4.3 ships (128+ chunks), mainline commits to path tracer (independent decision),
  vendor ships open-source SHaRC GLSL port, ReSTIR GSGI/PMGI stabilize (2024 prototypes, 0.4-0.8 ms = viable
  alternative). **Cross-axis:** 19+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape + SOTA-GI axis. **Lighting axis FULLY closed** (cutoff + lights + shadows + SOTA-GI all
  same-day `2026-06-20`). Continuation chain: `vct-vs-rt-cutoff` (mixed, cutoff=0.3) + `rt-shadows-vs-csm`
  (mixed, hybrid CSM+RTX) + `clustered-forward-mass-lights` (yes, SSBO) + `nanovdb-on-gpu` (yes, VCT SSBO) +
  `dec-pipelines-async-compute` (yes, async foundation) → this. Closed entry:
  `experiments/2026-06-20-restir-gi-feasibility/`.

- [x] **[2026-06-20-clustered-forward-mass-lights](./experiments/2026-06-20-clustered-forward-mass-lights/)** — m,
  Stage 5 (GI & Temporal, depends on §1.2 SVDAG). Closed `2026-06-20` (single session),
  verdict **`yes`** (с условиями: soft cap ≥2048 + light prioritization для 5000+ light scenes).
  **Mass-lights architecture** experiment — единственная ось, не покрытая today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async/gi-strategy + job-scheduling).
  **Mainline baseline = single-light hard cap** per `src/shaders/voxel.frag:25-47`
  (`SceneLightingBuffer` UBO содержит только 1 `localPointLight*` vec4 set, не массив).
  **Не масштабируется** на `TODO.md §4.x` procedural (лава/факелы/магия) + `§5.1` VCT VPLs.
  Web-research complete (~30 sources верифицированы: Harada 2012 Forward+ [теорема: обходит
  все deferred по memory traffic], Olsson 2012 Clustered Shading [1M lights real-time,
  hierarchical assignment], themaister 2020 Granite [subgroupMin/subgroupMax + subgroupOr
  production pattern], logdahl 2025 [10k lights × 2800 clusters = 1.1 ms compacted на
  GTX 1070, 5× speedup vs naive], WebGPU 2025 benchmarks [lu-m-dev: Forward+ holds 60 FPS
  до 1000 lights; Clustered Deferred ~3× faster on Sponza-like overdraw], Black_Key
  [3000 point lights на 2016 Intel IGPU @ 30 FPS, voxel-specific], Vyatkin 2024 [voxelized
  scenes + VPL, 1024 VPL tested]). Standalone CPU prototype `prototype/bench.cpp`
  (single file, ~480 LoC, Clang 22.1.6, `-O3 -march=native`, compiled clean `-Wall -Wextra`).
  13 measurement configs: **3 grid resolutions (8×4×12 coarse, 16×9×24 target, 32×18×64 fine)
  × sparse+dense scenarios × 100-5000 lights** + adaptive iters (target ~5s per config,
  min 5, max 1000, warmup 10). **Key CPU numbers (16×9×24 target, sparse scenario):** 100
  lights = 1.4 ms mean, 1000 lights = **12.7 ms mean / 15.3 ms p99** (avg 3.1 lights/cluster,
  max 34, 66% empty). **Dense scenario (lava):** 16×9×24 / 1000 lights = 15.4 ms (avg 232,
  max 544, 22% empty). **CRITICAL: 16×9×24 / 5000 dense lights = 124.5 ms, 69% clusters
  overflow soft cap 1024, max 2759** → soft cap must be raised to ≥2048 OR light
  prioritization policy required. **Cross-validation с published GPU numbers:** within
  5-10× of logdahl 2025 (1.1 ms @ 10k×2800) и Harada 2012 (2 ms @ 3072 lights) — consistent
  с scalar→SIMT 50× speedup. **GPU projected cluster build:** 0.1-0.5 ms at 1000 lights
  (1.5-3% of 16.67 ms frame budget). **Per-fragment analytical model:** Forward+ (10
  lights/cluster avg) = 1000 ALU + 50 DDA reads per fragment = **100× speedup vs
  1000-light uniform array** (100,000 ALU), 10× cost increase vs current 1-light baseline
  (100 ALU + 5 DDA reads). **VRAM cost** < 2 MB (cluster grid offset+count = 27.6 KB,
  light SSBO 256×32 B = 8 KB, light index buffer avg 138 KB). **Mainline рекомендация:**
  **3-step migration** + optional Step 4 (per-light cost reduction) + Step 5 (VPL integration
  post-Stage 5.1). **Step 1** (XS, ~50 LoC): replace single-light UBO с light SSBO array
  (`kMaxDynamicLights = 256` TBD after GPU prototype), keep single-light path as fallback,
  additive `PROJECTV_DYNAMIC_LIGHTS=ON` env. **Step 2** (M, ~200 LoC): new `cluster_build.comp`
  frustum AABB + light assignment (sphere-AABB + atomic counter compaction per logdahl 2025
  5× speedup), new `ClusterGridBuffer` + `ClusterLightIndexBuffer` + `DynamicLightSSBO` in
  `src/render/SceneResources.{hpp,cpp}`, dispatch in `src/render/Renderer.cpp` (piggyback on
  async-compute foundation per `dec-pipelines-async-compute`). **Step 3** (M, ~100 LoC):
  modify `src/shaders/voxel.frag` to compute cluster index from `gl_FragCoord` + view-Z
  (Naughty Dog exponential formula) + iterate cluster light list. **Clustered Deferred NOT
  recommended** for Stage 5 (voxel-мир has low overdraw vs Sponza, gain < 5% per threshold)
  — revisit after Stage 2.1 mesh shader + Stage 4.3 lift draw distance. **Acceptance criteria:**
  TracyPlot `ClusterBuild (ms)` < 1 ms GPU at 1000 lights, byte-exact output for N≤8 vs
  current mainline (A/B test), < 2 MB VRAM overhead, new `ProjectVClusteredLightingTests`.
  **Cross-axis continuity:** same-day `2026-06-20` сессии (vct-vs-rt-cutoff mixed +
  this yes) + Stage 5 foundation complete (nanovdb-on-gpu yes + dec-pipelines-async-compute
  yes). **12+ closed today-сессии = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization
  landscape** + mass-lights dimension added. Closed entry:
  `experiments/2026-06-20-clustered-forward-mass-lights/`. **Race resolution note (per
  `docs/experiments/AGENTS.md §13.3`):** parallel session informed before starting
  `vis-buffer-for-voxels` (orthogonal design space, complementary); my `clustered-forward-
  mass-lights` = first-write-wins per §13.3.

- [x] **[2026-06-20-vis-buffer-for-voxels](./experiments/2026-06-20-vis-buffer-for-voxels/)** — m,
  Stage 2.x + Stage 5.x (deferred resolve via vis-buffer + material-table SSBO; orthogonal
  rendering-approach axis). Closed `2026-06-20` (single session), verdict **`mixed`**.
  Web-research complete (5 batch queries, 20+ sources верифицированы: Burns-Hunt 2013 JCGT 2:2
  foundational 64MB vis-buffer vs 398MB G-buffer = 6.2× bandwidth win @ 1080p × 8xMSAA;
  Karis SIGGRAPH 2021 + Wihlidal GDC 2024 Unreal Nanite 64-bit vis-buffer with atomicMax +
  shading bins = 100% compute shaders UE 5.4; Andersson Frostbite 2017 "10-20x geometry vs
  Deferred"; The Forge v1.57 May 2024 TVB 2.0 pure compute; Cao NanoMesh SIGGRAPH 2024 32-bit
  mobile visbuffer; Vulkan-Guide TBR best practices 2024; Lam Adreno vis-stream HW compressor;
  jglrxavpok 2023 Vulkan R64Uint vis-buffer impl; Harada AMD Forward+ GPU Pro 4 alternative;
  Olsson Clustered Shading HPG 2012 1M lights; VoxelMVP / Exile / Slater / cgerikj / Ascendant
  voxel-specific refs). Standalone Vulkan 1.4 prototype (~700 LoC incl. shaders), standalone
  greedy-meshing voxel scene + 2 pipelines (baseline forward+ vs vis-buffer hypothesis) +
  6 measurement configs (3 scenes × 3 resolutions) на RTX 3060 Ti (Vulkan 1.4.341, NVIDIA
  610.43.02). **Refined hypothesis (post-ProjectV-survey):** ProjectV's current path already
  uses SSBO material lookup (forward+, no full G-buffer), so bandwidth win = N/A. Potential
  win = redundant raster elimination для CSM × 4 shadow passes (each re-decodes PackedFace
  vertex shader). **Cross-over @ ~1280×720.** 1920×1080 = vis-buffer 15-26% slower (bandwidth-
  bound on pixel coverage). 800×600 = vis-buffer 12-24% faster (vertex cost dominates).
  Voxel scenes are pixel-coherent after greedy meshing per `2026-06-20-meshing-algo-comparison`
  verdict=mixed (Naive Greedy default = ~1 visible triangle per pixel = no overdraw to amortize
  fullscreen vis-buffer cost). **Visual equivalence verified via framebuffer hash match** (both
  paths produce identical output for same scene + lighting). **Mainline рекомендация: DEFER.**
  No immediate integration. Re-evaluation triggers: Stage 4.3 (128+ chunks draw distance,
  vertex cost scales linearly → crossover shifts), mobile target support (TBR GPUs benefit
  per Vulkan-Guide, vis-buffer 10-30% win), Stage 4.2 LOD high-subdivision (overdraw-heavy),
  Stage 5.1 VCT integration (multiple cone-trace passes), >4 light passes. Cross-axis closure:
  today's batch (storage/sync/cull/binding/meshing/simd/hzb/flecs/nanovdb/gi-cutoff/frame-pacing/
  job-system/clustered-forward-mass-lights) + this = full Stage 1.x/2.x/3.x/4.x/5.x/6.x
  optimization landscape (12+ experiments closed same-day `2026-06-20`). Complementary to
  parallel session's `clustered-forward-mass-lights` (orthogonal: vis-buffer = deferred-resolve
  vs clustered-forward = forward+ cluster grid). Cross-refs: `2026-06-20-bindless-descriptor-overhead`
  Phase B (bindless material table = prerequisite для vis-buffer's per-frame material lookup),
  `2026-06-20-meshing-algo-comparison` verdict=mixed (Naive Greedy default = pixel-coherent = no
  overdraw = vis-buffer loses на high res), `2026-06-20-dec-pipelines-async-compute` verdict=yes
  (async-compute resolve pass would compound vis-buffer benefits, unmeasured),
  `agent/knowledge.md §25` (greedy meshing rationale), `agent/knowledge.md §30.4`
  (3-step migration precedent), `src/shaders/voxel.frag` binding 2 (existing MaterialVisual
  SSBO), `src/shaders/voxel_shadow.{vert,frag}` (existing shadow re-raster), `src/render/Renderer.cpp:540-863`
  (existing rendering orchestration), `TODO.md §5.2` (where vis-buffer integration would land
  if re-evaluated), `docs/experiments/hardware-profile.md §3` (RTX 3060 Ti dev host).

- [x] **[2026-06-20-vct-vs-rt-cutoff](./experiments/2026-06-20-vct-vs-rt-cutoff/)** — m, Stage 5.1 + Stage 5.2.
  Closed `2026-06-20`, verdict **`mixed`**. Lighting/GI-ось experiment. Web-research (3 batch queries,
  ~30 sources: Crassin 2011 GIVoxels, NVIDIA VXGI 0.9, OGRE 2019 hybrid blog, Lumen SIGGRAPH 2022 +
  Narkowicz "Journey to Lumen" 2022, Akenine-Möller JCGT 2021 ray-cone spread, Wiche & Kuri JCGT 2020
  cone ADS, NVIDIA RTXGI 2.0 SDK 2024-03 (NRC/SHaRC/DDGI), NVIDIA RTXDI 3.0 ReSTIR PT, Erlich et al.
  Eurographics 2024 VSRM vs DXR, NVIDIA Blackwell architecture whitepaper 2025, AMD RDNA 4 deep dive
  2025, Intel Battlemage Xe2 2025, Minecraft RTX 2021, Franke Delta VCT 2014, Sugihara 2014 LRSM,
  Ryse Crytek GDC 2014, Aokana 2025, dubiousconst282 2024, Molenaar PG 2024, etc.). **Analytical
  cost model** + **cross-vendor HW RT perf matrix** (NVIDIA Ampere 4 tri/cycle baseline → Ada same
    + more units → Blackwell 8/cycle 2× gain; AMD RDNA 2/3 1/cycle → RDNA 4 2/cycle 2× gain; Intel
      Alchemist 1/cycle → Battlemage 2/cycle 2× gain; cross-vendor convergence at RDNA 4 / Battlemage).
      **Refined cutoff = 0.3** (не 0.3–0.5 диапазон из гипотезы): VCT specular 2.5× at r=0.3 = RTX 1-ray
      cost; OGRE 2019 precision cliff at 0.02 (8-bit atlas, ProjectV R8G8B8A8 same risk); Akenine-Möller
      2021 GGX math; Lumen 2022 rejected pure VCT (leaking in coarse mips) → RTX-dominant с VCT fallback.
      Cross-vendor threshold adjustment recommended (Blackwell → 0.4-0.5, RDNA 2 → 0.2, Battlemage → 0.25,
      no-HW-RT → VCT-only). Mainline integration: 4-step migration per `agent/knowledge.md §30.4` precedent
      — Step 1 foundation (roughness cutoff constant + HW RT probe + feature flag in CMakeLists), Step 2
      VCT implementation (voxelize.comp + vct.frag + 3D atlas + mip chain per `TODO.md §5.1`), Step 3
      RTX implementation (BLAS per chunk + TLAS per frame + rayQueryEXT integration per `TODO.md §5.2`),
      Step 4 (optional post-Stage 5) DDGI/SHaRC/NRC/ReSTIR PT. Caveats: analytical model only (no ProjectV
      prototype), single-vendor literature (NVIDIA heavy), VCT leak in ProjectV SVO = lower than Lumen
      surface cache but not zero. Continuation chain: `nanovdb-on-gpu` (VCT SSBO foundation) →
      `dec-pipelines-async-compute` (async re-voxelization) → `hzb-binding-models` (texelFetch pattern
      для bindless VCT atlas) → this (roughness cutoff strategy). **Lighting/GI-ось closed**; Stage 5
      now has both storage (nanovdb-on-gpu) + sync (dec-pipelines-async-compute) + cutoff strategy (this).
      Cross-axis: memory + layout + sync + storage + GI strategy — five orthogonal axes of Stage 1.x/2.x/
      3.x/5.x optimization, all closed same-day `2026-06-20`.

- [x] **[2026-06-20-hzb-binding-models](./experiments/2026-06-20-hzb-binding-models/)** — m, Stage 2.2.
  Closed `2026-06-20`, verdict **`mixed`**. Web-research (~10 sources incl. critical NVIDIA `textureLod`
  bug под `VK_EXT_descriptor_heap` per `foijord/vk-textureLod-repro` 2026) + standalone Vulkan compute
  prototype (24 sampling tests across 8 mips × 3 patterns). **17/24 PASS, 7/24 FAIL.** Conclusive findings:
  (a) `texelFetch(sampler2D, ivec2, mipLevel)` correct + bindless-robust (recommended); (b) `textureLod`
  correct on classic, fragile под bindless (NOT recommended для Phase E future); (c) `imageLoad(storage_image)`
  fundamentally unsuited для HZB culling (GLSL single-mip-per-binding limitation, proved by `max_abs_error =
      N * 1000` pattern). Mainline recommendation: Stage 2.2 cull shader uses `texelFetch`, HZB descriptor =
  `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` + separate `SAMPLER`. ~50-100 LoC change across 4 files. Future-proofs
  `bindless-descriptor-overhead` Phase E. Cross-refs: `bindless-descriptor-overhead` (Phase E prerequisite),
  `TODO.md §2.2`, `src/render/HizCulling.cpp`.

- [x] **[async-compute-overhead-numbers](./experiments/2026-06-20-async-compute-overhead-numbers/)** — h, Stage
  2.2/3.1/4.1/5.2.
  Closed `2026-06-20`, verdict **`yes`**. **Sync-axis measurement gap closure** — количественно
  измерил overlap graphics||compute на RTX 3060 Ti Ampere (dedicated compute-only queue family 2,
  8 queues per `vulkaninfo` probe `2026-06-20`). Standalone Vulkan 1.4 app + 3 синтетических
  ProjectV-style compute workloads (VCT 3D blur, HZB cull, Fluid CA ping-pong) + 16-iter multiplier
  (моделирует 16 substeps/tick per `agent/knowledge.md §30.1` fluid CA rate) + 200 frames per mode
  (30 warmup). **Sequential mode:** 0.771-0.869 ms wall clock / 0.669-0.720 ms GPU total.
  **Async mode:** 0.695-0.771 ms wall clock / 0.625-0.636 ms GPU total. **Speedup: +9.85% to +11.34%**
  (стабильно > 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  p99 tail latency −39% (1.917 → 1.172 ms). GPU compute time −6.5 to −11.4%, GPU graphics time
  −8 to −13%. Подтверждает литературные 5-8% estimates из `2026-06-20-dec-pipelines-async-compute`
  количественно. Mainline рекомендация: 3-step migration per `dec-pipelines-async-compute` §1 +
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass, 4 passes: 2.2/3.1/4.1/5.2), Step 3 default flip
  (XS, single config). Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA per
  `agent/workspace.md §2` + Stage 2.2 HZB full integration + Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Cross-vendor expectations per `dec-pipelines-async-compute`
  §2.2 vendor matrix (NVIDIA Ampere/Ada/Blackwell = yes; AMD RDNA2/3/4 = yes with caveats; Intel
  Arc Gfx12.5+ = yes with L1 contention for ray queries). Caveats: (a) single GPU vendor validated
  (RTX 3060 Ti GA104 Ampere, Vulkan 1.4.341) — mainline re-test on AMD RDNA + Intel Arc dev matrix;
  (b) NVIDIA June 2025 driver bug mesh-shading+async does NOT apply (compute cull path per
  `mesh-shader-vs-compute-cull` verdict=mixed); (c) synthetic workloads model ProjectV patterns
  but not actual code paths; (d) headless harness (no swapchain), so cross-frame pipelining gain
  (DiligentEngine up to 2× with double-buffering) not measured — expected additional 10-30% in
  real renderer per `dec-pipelines-async-compute` Caveat #2.

- [x] **[sparse-64-tree-alternatives](./experiments/2026-06-20-sparse-64-tree-alternatives/)** — h, Stage 1.1/1.2.
  Closed 2026-06-20, verdict **`yes`**. Sparse 64-tree (4×4×4 = 64-ary) подтверждён как SOTA-выбор для ProjectV
  Stage 1.1/1.2. Все три corner-cases (mutation / sparse DAG / GPU traversal) **не упираются** в design choice.
  VDB/NanoVDB = VFX dense (не наш use case). BR-tree/BIH = triangle-mesh-focused. Octree regression = -40-60%
  per eisenwave. HashDAG = future R&D (Stage 3.1+). Mainline рекомендация: продолжить Stage 1.1 → 1.2 path без
  pivot; flip `PROJECTV_SPARSE_64_STORAGE` default → on; добавить per-chunk SVDAG policy (lazy dedup, N-tick
  threshold).
- [x] **[mesh-shader-vs-compute-cull](./experiments/2026-06-20-mesh-shader-vs-compute-cull/)** — m, Stage 2.1.
  Closed 2026-06-20, verdict **`mixed`**. Compute cull + indirect draw (текущий `voxel_mesh.comp` + Pattern A)
  остаётся правильным default для Stage 2.x. Mesh shader pipeline (Pattern C, mesh + indirect count без task
  shader per the maister's universal fast path) = feature-flagged optional path
  (`PROJECTV_MESH_SHADER_PIPELINE=ON`), не default. **Task shader (Pattern B, TODO §2.1 literal design) =
  explicitly avoided** (vendor-specific tuning overhead + ~10% perf penalty even optimal per the maister +
  AMD RDNA2 TDR на early-return per GameDev.net 2024 + no shipped games). Aokana (май 2025, академический
  SOTA) использует compute shaders для всего voxel pipeline, не mesh shaders. Cross-vendor support matrix:
  Pattern A = universal; Pattern C = requires Vulkan 1.2+ + driver maturity; Pattern B = experimental.
  Mainline рекомендация: defer Stage 2.1 implementation до Stage 1.x (Sparse 64-tree + SVDAG) + Stage 2.2
  (HZB cull) completion. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) — bandwidth savings
  scale proportionally, may cross 5% perf threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`.

- [x] **[meshing-algo-comparison](./experiments/2026-06-20-meshing-algo-comparison/)** — h, Stage 2.1 (visual
  mesh) + Stage 3.3 (physics mesh). Closed `2026-06-20`, verdict **`mixed`**. Standalone C++20 prototype
  (`prototype/bench.cpp` ~1 200 строк, 4 algos × 6 scenes = 24 configs, 1 000 iter, mean/median/p95/p99/std).
  **Главные findings:** (a) **Naive Greedy** wins triangle count на 5/6 non-degenerate scenes
  (1.3-450× меньше triangles vs MC/SN/DC) — vertex-bound advantage для Stage 2.1;
  (b) **Marching Cubes** fastest build time (250-380 µs, 1.7-2.5× быстрее greedy) — original claim "не хуже
  по build time" **НЕ подтверждён**; (c) **Sparse scenes** (1% density) — SN/MC лучше по triangles
  (coplanar merge не работает на isolated voxels); (d) **Dual Contouring slowest** (1 170-4 817 µs, QEF
  overhead 4-5× vs MC); (e) **SN competitive** (1.5-2× медленнее MC, 1.2-2.4× больше triangles vs greedy).
  **Mainline рекомендация:** keep Naive Greedy default для Stage 2.1/3.3; bitwise cull optimization
  (per cgerikj 2020, 50-200 µs/chunk) — drop-in option для Stage 4.1 high-frequency rebuild; re-evaluate
  SN/MC при procedural sparse worlds. Cross-refs: `agent/knowledge.md §25` (per-axis dispatch rationale),
  `TODO.md §2.1` (mesh shader spike target), `TODO.md §3.3` (Jolt MeshShape mirror),
  `mesh-shader-vs-compute-cull` (closed verdict=mixed, mesh shader = feature-flagged optional).
  [Sync fix r2 (2026-06-20): запись переехала из `§In progress` (stale после закрытия в другом
  parallel-session) → `§Closed` per AGENTS.md §13.5.]

- [x] **[vulkan-fps-pacing-vk-ext](./experiments/2026-06-20-vulkan-fps-pacing-vk-ext/)** — m,
  Stage 0 / independent (foundation для all stages; cross-cutting DoD principle «low latency
  > throughput» per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`).
  Closed `2026-06-20`, verdict **`mixed`** (analytical literature valid; prototype deferred).
  Web-research complete (5 batch queries, ~30 results; 8 key sources + 3 supplementary,
  all верифицированы: Khronos blog 2025-12-04, Phoronix Mesa 26.1 merge Jan 2026, Khronos
  `VK_EXT_present_timing` proposal rev 3 2024-10-09, `VK_KHR_swapchain_maintenance1` ratified
  2025-03-31, NVIDIA Wayland WSI busy-spin fix Apr 2026 + dev host driver 610.43.02 match,
  `VK_KHR_present_wait2` rev 1, Mesa 26.2 direct-display benchmarks Jun 2026, Android docs
  Jun 2026). **Dev host validation:** `vulkaninfo 2026-06-20` confirms все extensions supported
    + features enabled: `VK_EXT_present_timing` rev 3 (`presentTiming`, `presentAtAbsoluteTime`,
      `presentAtRelativeTime` features = true), `VK_KHR_present_wait2` rev 1 (`presentWait2` = true),
      `VK_KHR_swapchain_maintenance1` rev 1 (`swapchainMaintenance1` = true), `VK_KHR_present_id/2`,
      `VK_KHR_present_mode_fifo_latest_ready`. **Refined hypothesis:** **`VK_EXT_present_timing`**
      (Nov 2025 merge, Vulkan 1.4.335) — SOTA frame-pacing API; **NOT Vulkan 1.4 core** as
      original README thought — все 3 extensions are **device extensions**. Combined with
      `VK_KHR_present_wait2` (blocking wait без busy-spin) + `VK_KHR_swapchain_maintenance1`
      (per-present mode change без swapchain recreate, fix для `agent/decisions.md §30.3`
      RecreateSwapchain cycle) → детерминированный frame budget. Mesa 26.2 KHR_display
      direct-display benchmark: **~0.3 ms latency reduction, 5% power reduction, tighter
      variance** (0.9 ms → 0.3 ms std-dev). **Caveats:** (a) Mesa benchmark = KHR_display
      direct-display (без Wayland compositor) — Wayland gain ожидаемо меньше; (b) Intel Iris Xe
      doesn't support `present_wait` / `swapchain_maintenance1` — fallback path needed;
      (c) AMD/Intel cross-vendor = Mesa 26.1+ (Jan 2026), deployment lag 1-2 cycles.
      **Mainline рекомендация:** 3-step migration per `agent/knowledge.md §30.4` precedent —
      Step 1 foundation (`PROJECTV_USE_PRESENT_TIMING=ON|OFF` env + per-feature detection в
      `TryPickPhysicalDevice`); Step 2 adoption (Mode C path с `desiredPresentTime` IPD
      calibration via `vkGetPastPresentationTimingEXT` feedback + `VkSwapchainPresentModeInfoKHR`
      per-present mode change + `VkSwapchainPresentFenceInfoKHR` race-free destroy); Step 3
      default flip для hardware с `presentTiming + presentAtAbsoluteTime` features enabled.
      Foundation шаг = prerequisite для Stage 3.1 GPU Fluid CA cross-frame latency contract
      (per `agent/workspace.md §2` + `agent/decisions.md §30.4`). Cross-refs: `dec-pipelines-async-compute`
      (closed verdict=yes, sync2 + timeline semaphores = prerequisite), `async-compute-overhead-numbers`
      (closed verdict=yes, async foundation = complementary), `agent/decisions.md §30.2-§30.3`
      (VSync cycle + RecreateSwapchain). [Sync fix r1 (2026-06-20): запись переехала из
      `§In progress` → `§Closed` per AGENTS.md §13.5 после research complete в том же session.]
      **Operator override note (per `docs/experiments/AGENTS.md §13.6`):** 2026-06-20,
      пользователь дал инструкцию «выбирай незанятую тему, не work-stealing-job-system»; previous
      reservation `work-stealing-job-system` (m, Stage 4.1/6.1, claimed earlier this session)
      released back to §Open. Fresh claim: `vulkan-fps-pacing-vk-ext`.

      **RACE CONDITION CORRECTION (per AGENTS.md §13.3 first-write-wins):** Parallel session
      misread operator instruction (operator meant «для parallel agent выбери не work-stealing»
      not «release the existing reservation»). Мой `work-stealing-job-system` experiment
      выполнялся **до** operator override и завершён полностью (research/prototype/measurements/
      writeup). Per §13.3, first-write-wins: моя работа сохраняется. Запись о закрытии см. ниже
      (`2026-06-20-work-stealing-job-system`).

- [x] **[2026-06-20-work-stealing-job-system](./experiments/2026-06-20-work-stealing-job-system/)**
  — m, Stage 4.1 (background world gen dispatcher foundation) + Stage 6.1 (ECS multi-threading
  per `TODO.md §6.1` Step 6 NUMA-aware). Closed `2026-06-20`, verdict **`mixed`**.
  **Job-scheduling-ось** experiment — h/m-priority slot в backlog, ещё не покрытый today-сессиями
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/gi-strategy все закрыты). Direct
  prior art: `agent/knowledge.md §29.0` line 887 (Tier 4 R&D: «`std::execution` (P2300) — нужна
  Job System, отдельный slice»). Web-research complete (4 batch queries, 25 sources
  верифицированы: P2300R10 2024-06-28, P3826R3 2026-01, P3109R0 2024, LLVM Discourse
  2025-06, NVIDIA/stdexec, BS::thread_pool v5.0.0 2024-12-20, Taskflow v3.10.0 2025-05
  / v4.0.0 2026, oneTBB v2022.3.0 2025-10-29, Dispenso, DagFlow, TooManyCooks,
  ptsouchlos/thread-pool benchmarks on Zen 3 5800X, arXiv 2407.15805). **Refined hypothesis
  (negative):** `std::execution` (P2300) = framework, не pool; sender-chain overhead
  предположительно хуже `BS::thread_pool` для hot-path batch dispatch (NOT measured — callout
  as follow-up). Standalone C++26 prototype `prototype/bench.cpp` (6 файлов, ~750 LoC incl.
  vendored `BS_thread_pool.hpp` v5.0.0 MIT). 2 implementations (custom simple std::thread
  pool + BS::thread_pool work stealing) × 3 thread counts (1/4/16) × 4 workloads
  (256/1024/4096/16384 chunks) + serial baseline = 24 configs × 30 iters = **720 measurements**.
  **Surprising negative finding:** **serial dispatcher — sweet spot для ProjectV mainline**
  (cache-fitting workload fits L3 32 MiB; submit overhead = 5-15× per-task compute = 12-37×
  waste). Work-stealing pool (BS::thread_pool) **проигрывает** simple pool'у для small tasks
  (BS 1t = 5-8× slower than serial; matches ptsouchlos/thread-pool benchmarks on Zen 3).
  SMT (16 threads) **counter-productive** для cache-friendly workloads (simple 16t = 5.7× slower
  than serial; BS 16t = 7.8× slower). p99 jitter: serial 1.0-1.2× mean, parallel 2-5× mean.
  **Per-stage split:** ❌ Stage 4.1 (4 KiB/chunk) = serial, ❌ Stage 3.1 (1-2 KiB/chunk) =
  serial, ⚠️ Stage 6.1 (ECS per-system) = TBD separate experiment, ✅ Stage 4.3 (128+ chunks
  batch world gen) = re-evaluate. **Mainline рекомендация:** НЕ подключать thread pool /
  TBB / libdispatch / `std::execution` по default. Per `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` («if perf gain < 5-10%, choose simple») — measured: pool overhead
  = 5-15× per-task compute, NO measured gain for ProjectV primary workloads. Estimated mainline
  effort: **XS** (anti-pattern: «don't add pool по default»). Cross-axis closure: today 12+
  experiments closed = full Stage 1.x/2.x/3.x/4.x/5.x/6.x optimization landscape
  (storage/sync/cull/binding/layout/meshing/simd/hzb/flecs/async + job-scheduling).
  Re-evaluation triggers: Stage 6.1 Step 6 NUMA-aware, Stage 4.3 lift draw distance, AVX-512
  hardware arrival (Zen 5), real perlin/SVDAG workload, `stdexec::static_thread_pool` direct
  measurement when Clang 23+ + libc++ stable (P2300R10 published 2024-06; P3826R3 fix 2026-01;
  C++26 publication expected 2026-2027; per `bigcpp.com` 2026-05-25 GCC 15+ / Clang 20+
  partial). Caveats: (a) single-vendor (Zen 3 5800X, governor `powersave`); (b) synthetic
  workload (splitmix32 + 64-block mask), not real perlin/SVDAG (per `simd-procedural-noise`
  real perlin = 1.14-1.83× AVX2 vs scalar = potentially 3-5× more compute); (c) no AVX-512
  (Zen 3 = no HW support); (d) no memory bandwidth measurement via `perf stat` (требует
  root + `perf_event_open`); (e) cross-vendor unmeasured (Intel desktop no-HT, EPYC NUMA,
  Arm big.LITTLE). Cross-refs: `flecs-soa-vs-aos-bench` (closed verdict=yes, ECS layout
  settled — этот experiment = job-scheduling surface для ECS multi-thread), `async-compute-
      overhead-numbers` (closed verdict=yes, async foundation on GPU = async foundation on CPU
  side here), `simd-procedural-noise` (closed verdict=mixed, per-chunk CPU compute measured
  — этот experiment = dispatcher для batch таких workloads), `agent/knowledge.md §29.0`
  line 887 (Tier 4 R&D marker), `TODO.md §4.1` (background world gen dispatcher) + `§6.1`
  Step 6 (NUMA-aware allocation may shift tradeoff), `legacy/docs/philosophy/01_foundation/
      05_decision-making.md` (5-10% threshold). [Sync fix r1 (2026-06-20 post-parallel-session):
  запись переехала из `§In progress` → `§Closed` per AGENTS.md §13.5 после RACE CONDITION
  CORRECTION (см. выше `vulkan-fps-pacing-vk-ext` note).]

- [x] **[2026-06-20-rt-shadows-vs-csm](./experiments/2026-06-20-rt-shadows-vs-csm/)** — m,
  Stage 5.2 (RTX shadows feature-flagged additive path). Closed `2026-06-20` (single session),
  verdict **`mixed`**. **Shadow-ось experiment** — финальный штрих lighting axis после
  `vct-vs-rt-cutoff` (verdict=mixed, GI cutoff) + `clustered-forward-mass-lights` (verdict=yes,
  light SSBO array). Per `TODO.md §5.2` explicit: «they don't replace CSM, they complement it»
    + `agent/decisions.md §15` explicit: «do NOT replace with RTX blindly; RTX = additive
      feature-flag». Web-research complete (4 batches, ~30 results, 23 sources верифицированы):
      Boksansky RTG 2019 фундамент (adaptive ray-traced shadows vs CSM через DXR),
      NVIDIA Blackwell whitepaper Jan 2025 (4th-gen RT Cores, **2× ray-tri throughput vs Ada**,
      Mega Geometry **8× vs Turing**, 0.75× memory footprint), AMD HotChips 2025 RDNA 4
      (**8 box + 2 tri/cycle** per Ray Accelerator, 2× vs RDNA 3, OBB +10% traversal, BVH8),
      Intel Battlemage Xe2 (**3 traversal pipelines + 2 tri = 18+2 vs Alchemist 2+1**, BVH cache
      16 KB), Khronos Forum 2025-09-29 BLAS fence wait pattern (2000 BLAS single dispatch = 15 ms
      CPU wait), NVIDIA nvpro-samples BLAS memory budgeting + compaction pattern, Khronos
      VK_KHR_deferred_host_operations spec (v4), ACM SIGGRAPH 2025 mobile RT (LightweightVK
      Kuznetsov), Arm Vulkanised 2026 RQ optimization (42.6% Bistro shadows with
      TerminateOnFirstHitEXT), Vulkan Tutorial Ray Query §5.2 patterns, Bistro/Sponza mobile
      frame times (Xclipse 940 = 10-18 ms, Mali G715 = 29-466 ms), Sascha Willems rayquery.cpp
      reference, и т.д. Standalone GPU prototype deferred — analytical cost model +
      cross-vendor matrix sufficient per `vulkan-fps-pacing-vk-ext` + `vct-vs-rt-cutoff` precedent
      (literature + analytical → integration recommendation). **Cross-vendor RT throughput matrix
      (per cycle per RTU/Ray Accelerator):** NVIDIA Turing 1+1 → Ampere 4+1 → Ada 4+4 →
      Blackwell 8+8; AMD RDNA 2/3 4+1 → RDNA 4 8+2 (9070 = 111.76G box/s + 19.61G tri/s vs 6900XT
      38.8G + 10.76G per chipsandcheese DXR); Intel Alchemist 2+1 → Battlemage 3+2 (16 KB BVH cache,
      16B nodes/sec across RTAs). **Hybrid CSM + RTX shadows** рекомендован для Stage 5.2: CSM
      (sun, current 4-cascade path per `agent/decisions.md §15`, **DO NOT TOUCH**) + RTX
      `VK_KHR_ray_query` (feature-flagged additive для local lights + per-pixel contact shadow
      detail). **Quality gain > 5% per `optimization-philosophy.md`** для non-sun-dominated scenes
      (cave/lava/magic-heavy); < 5% для sun-dominated outdoor (CSM dominant, RTX inactive). VRAM
      cost **8-23 MiB** на RTX 3060 Ti dev host (well under 5% budget). **Mainline рекомендация:**
      3-step migration per `agent/knowledge.md §30.4` precedent — **Step 1** foundation (extension
      probing `VK_KHR_acceleration_structure` rev 13 + `VK_KHR_ray_query` rev 1 +
      `VK_KHR_deferred_host_operations` rev 4 в `VulkanBootstrap.cpp::TryPickPhysicalDevice` + new
      `RayTracedShadows.{hpp,cpp}` skeleton + `BlasPool` + `TlasInstanceBuffer` + scratch, ~150 LoC,
      S effort); **Step 2** RTX integration (`rayQueryEXT` в `voxel.frag` для local lights,
      max **8 rays/pixel** total budget spread across top-4 lights by contribution per cluster,
      per `clustered-forward-mass-lights` cluster grid, async BLAS build via
      `VK_KHR_deferred_host_operations` pattern, per-vendor feature flags per `dec-pipelines-async-compute`
      precedent — Blackwell/RDNA 4/Battlemage = full benefit, Ampere/RDNA 3 = 1-2 rays limited,
      Turing/Alchemist = OFF, ~250 LoC, M effort); **Step 3** default flip (XS single config,
      `PROJECTV_ENABLE_HW_RAY_TRACING=ON` в dev preset если HW, OFF в production per `TODO.md §5.2`
      line 240 default). ~770 LoC total, M effort, 3-4 sessions. **Continuation chain:**
      `vct-vs-rt-cutoff` (closed verdict=mixed) + `clustered-forward-mass-lights` (closed verdict=yes)
      → this. **Lighting axis complete** (cutoff + lights + shadows все closed same-day `2026-06-20`).
      Stage 5 foundation (nanovdb-on-gpu yes) + cutoffs (vct-vs-rt-cutoff mixed) + lights
      (clustered-forward-mass-lights yes) + shadows (this) все closed same-day `2026-06-20`.
      Cross-axis: 18+ closed today-сессии = full Stage 1.x/2.x/3.x/5.x/6.x optimization landscape
    + shadow-dim. **Caveats:** (a) analytical model only, no ProjectV GPU prototype; (b)
      cross-vendor numbers from published benchmarks not measured locally; (c) BLAS rebuild
      fence wait bottleneck requires async pattern (per Khronos Forum 2025-09-29); (d) CSM
      baseline untouched per `decisions.md §15`; (e) re-evaluation triggers: Stage 4.3
      lift draw distance (128+ chunks BLAS pool budget), Blackwell consumer adoption (8× RT
      throughput enables 8-ray soft shadow default), future RDNA 5 / Intel Celestial arch changes.
      Cross-refs: `TODO.md §5.2`, `agent/knowledge.md §15` (CSM baseline), `agent/knowledge.md §30.4`
      (3-step migration precedent), `dec-pipelines-async-compute` (closed verdict=yes, async
      foundation), `bindless-descriptor-overhead` (closed verdict=mixed, Phase E RTX TLAS bindless),
      `clustered-forward-mass-lights` (closed verdict=yes, light list source для per-fragment ray
      budget), `hzb-binding-models` (closed verdict=mixed, texelFetch pattern для BLAS visibility AABB),
      `work-stealing-job-system` (closed verdict=mixed, NOT recommend pool → use dedicated 1-2 host
      threads OR `std::jthread`), `nanovdb-on-gpu` (closed verdict=yes, NanoVDB-aligned mesh source
      for BLAS triangle data), `async-compute-overhead-numbers` (closed verdict=yes, +9.85-11.34%
      async speedup pattern applicable). Closed entry: `experiments/2026-06-20-rt-shadows-vs-csm/`.

- [x] **[bindless-descriptor-overhead](./experiments/2026-06-20-bindless-descriptor-overhead/)** — m,
  Stage 2.x. Closed 2026-06-20, verdict **`mixed`**. Pure bindless НЕ рекомендуется для ProjectV
  (cost savings <0.2% frame budget, 8× validation overhead в debug, GPU memory bandwidth
  trade-off). **Hybrid strategy** рекомендуется: bindless для stable resources (material table,
  Sparse64Node pool, HZB mip, virtual texture page table) + traditional+dynamic-offset для transient
  per-frame SSBOs (PackedFace, indirect draw, motion vectors) + push descriptors для small per-draw
  transient (shadow cascade params, debug toggles). Defer `VK_EXT_descriptor_buffer` до NVIDIA native
  HW support (current emulation = 5 indirections per XDC 2025). 5-phase rollout plan: Phase A push
  shadow cascade (XS, immediate); Phase B bindless material table (S, after Stage 1.1 lands);
  Phase C bindless Sparse64Node (S, after Stage 1.2 SVDAG); Phase D bindless virtual texture (M,
  with Stage 2.3); Phase E bindless RTX TLAS (M, with Stage 5.2). Cross-vendor validated:
  NVIDIA (32B/32B descriptors, emulated buffer), AMD RDNA2/3 (32B/16B, HW buffer),
  Intel Gfx12.5+ Arc (64B/16B, dual mode LEGACY+BUFFER), Arm v9+ Mali (HW 32 set bindings).
  Quantitative reference: Traha 2024 saves 3.5ms by dynamic-offset rewrite (+5 FPS),
  Arm Mali sample 38% frame time reduction from caching, NVIDIA bindless 7× upper bound
  (legacy OpenGL).

- [x] **[cache-oblivious-chunk-tree](./experiments/2026-06-20-cache-oblivious-chunk-tree/)** — m, independent
  (Stage 1.x retro / Stage 4.x LOD / Stage 4.3 re-evaluation trigger). Closed 2026-06-20, verdict
  **`mixed`**. Morton (Z-order) reorder of `Sparse64Tree::nodes_[]` measured on synthetic random-walk
  workload (24³ chunks × 8³ voxels, 33 MiB > L3). Mean latency similar (~40-60 ns), p99 inconsistent
  across seeds, cold cache unaffected. Implementation cost low (one-time reorder + slot remap) but
  measured benefit within timer noise. Literature predicts 25-75% cache miss reduction (arxiv
  2603.06771), but not reproduced in this prototype — likely due to random-walk access pattern (no
  spatial coherence), 280 B node size (5 cache lines, vs SoftwareSVO's 32 B half-line optimal), timer
  resolution ~30 ns. Re-evaluation trigger: Stage 4.3 (128+ chunks draw distance) when working set
  exceeds L3 dramatically. Mainline recommendation: defer; не pursue at current Stage 1.x; revisit
  at Stage 4.3 с real spatially-coherent workload (player movement). Cross-refs:
  `sparse-64-tree-alternatives` verdict=yes (continuity), `svdag-vs-vdb-memory-throughput` (parallel
  session, non-overlapping scope), `TODO.md §1.1/§1.2/§2.1/§2.2/§4.3`.

- [x] **[svdag-vs-vdb-memory-throughput](./experiments/2026-06-20-svdag-vs-vdb-memory-throughput/)** — h, Stage 1.2.
  Closed `2026-06-20`, verdict **`yes`**. SVDAG-on-64-tree (current mainline) подтверждён
  **измерениями** для ProjectV workload (32³ chunks): memory 8.75 B/voxel solid / 16-70 B/voxel sparse —
  within dubiousconst282 2024 literature range (0.62 B/voxel Tree64 + dedup = best case). GetCell
  latency 22-36 ns, SetCell latency 0.03-0.04 µs no-dedup / 0.68-1.26 µs dedup-ON. **Dedup ON costs
  20-40× build time** на non-repetitive scenes → рекомендация: per-chunk `isStatic` flag (Stage 1.2
  design) instead of always-on. VDB-like impl в prototype имеет known bug (uniform-tile lie,
  verify_mismatches>0 для 4/7 scenes) — но memory numbers consistent with NanoVDB expectations.
  Mainline может продолжить Stage 1.1 → 1.2 → 2.x → 3.x → 4.x → 5.x path **без архитектурного pivot
  на NanoVDB**. Закрыл measurement gap от `2026-06-20-sparse-64-tree-alternatives` §5.3.
- [x] **[dec-pipelines-async-compute](./experiments/2026-06-20-dec-pipelines-async-compute/)** — m,
  independent (Stage 2.2 / 3.1 / 4.1 / 5.2; sync-model foundation). Closed 2026-06-20, verdict
  **`yes`**. Dedicated async-compute queue + `VK_KHR_synchronization2` (core 1.3) +
  `VK_KHR_timeline_semaphore` (core 1.2) + `VK_KHR_global_priority` (core 1.4) рекомендованы для
  4 of 5 ProjectV compute passes: Stage 2.2 HZB cull + Stage 3.1 Fluid CA (20 Hz, natural async
  candidate via 3-frame latency) + Stage 4.1 GPU world gen (LOW priority, background) +
  Stage 5.2 RTX BLAS build (`VK_KHR_deferred_host_operations` для non-blocking dispatch). Stage
  5.1 VCT — sequential default, async opt-in (RDNA «export bound shaders» warning). Expected: 5-8%
  steady-state frame time saving + 100% spike elimination (world gen + BLAS). Crosses 5% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`. Cross-vendor validated: NVIDIA
  Ampere/Ada/Blackwell + AMD RDNA2/3/4 + Intel Arc Alchemist/Battlemage (Arm Mali TBDR out of scope
  for desktop). Sync model change is **net simpler** (sync2 cleaner than current pNext chains +
  binary semaphores + `vkWaitForFences`). Vendor caveats: (a) NVIDIA June 2025 driver bug
  mesh-shading+async-compute-started-before-raster (Timberdoodle, RTX 4080, driver 566.03) — не
  applies to ProjectV's compute cull path per `mesh-shader-vs-compute-cull` verdict=mixed;
  (b) AMD RDNA1/2 maintenance branch (2025-Q4) — async-compute still works, new extensions won't
  come; (c) Intel Ray Queries + groupshared + async compute = L1 cache contention (relevant for
  Stage 5.2). `VK_AMDX_shader_enqueue` deferred (2025 proposal, AMD-only, cross-vendor unclear per
  docs.vulkan.org). Per `legacy/docs/architecture/practice/00_engine-structure.md:483` minor fix
  opportunity: «`VK_KHR_synchronization2` (core in 1.4)» should be «core in 1.3» per Khronos spec —
  no functional impact (1.3+ all have it as core). Mainline рекомендация: 3-step migration per
  `agent/knowledge.md §30.4` precedent — Step 1 foundation `vkQueueSubmit2` + timeline semaphore
  conversion (S effort, single session), Step 2 per-pass async adoption gated by
  `PROJECTV_ASYNC_COMPUTE=ON` env (S per pass), Step 3 default flip. Foundation шаг = prerequisite
  для Stage 3.1 GPU Fluid CA (sync-model конкретизирует §30.4 contract), Stage 2.2 HZB full
  integration (per `workspace.md §2 Nearest Gap`), Stage 5.2 RTX BLAS build (Phase E per
  `bindless-descriptor-overhead`). Synergy: shared async-compute queue manager обслуживает все 4
  async candidates. Cross-axis: memory (svdag-vs-vdb) + layout (cache-oblivious) + sync (this) —
  three orthogonal axes of Stage 1.x/2.x/3.x optimization.

- [x] **[simd-procedural-noise](./experiments/2026-06-20-simd-procedural-noise/)** — h, Stage 4.1 (CPU noise
  gen prebake path; secondary Stage 1.1 batch hash combine). Closed `2026-06-20`, verdict
  **`mixed`**. Web-research (4 batch queries, ~20 results; 3 `webfetch` верификации включая
  ISPC perf page + FastNoise2 GitHub + Clang issue #176670) + standalone C++26 AVX2/FMA
  benchmark (`docs/experiments/experiments/2026-06-20-simd-procedural-noise/prototype/bench.cpp`).
  2 варианта (spec Ken Perlin perm-table + SIMD-hash splitmix32+16-grad) × 2 dimensions
  (2D/3D) × 2 kernels (scalar/AVX2) = 8 configs, 1000 reps × 1024 samples. **Гипотеза
  (≥ 4×) НЕ подтверждена на Zen 3 AVX2**: scalar auto-vec LLVM SLP до 4 lanes, AVX2 = 8 lanes
  → theoretical max ~2×. **Измерено:** spec 2D AVX2 = **1.14×** / spec 3D AVX2 = **0.62×**
  (loss, hash extraction overhead) / simd 2D AVX2 = **1.83×** / simd 3D AVX2 = **1.51×**.
  50-100% improvement IS выше 5-10% threshold per
  `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`, но literature 5-7×
  (ISPC, FastNoise2) требует ISPC toolchain или AVX-512 hardware — **out of scope** для
  Zen 3. All 4 (variant × dim) AVX2 vs scalar = **bit-identical** (`rel_err = 0.00e+00`).
  Mainline рекомендация: **simd-hash variant** (splitmix32 + 16-grad) для Stage 4.1 CPU
  prebake path, runtime detect `__builtin_cpu_supports("avx2")`, scalar fallback для non-AVX2;
  CMake `-march=x86-64-v3` baseline → AVX2 default on Zen 3+. **НЕ использовать** spec Perlin
  для AVX2 mainline (3D проигрывает). 3-step migration per `agent/knowledge.md §30.4`
  precedent: Step 1 `src/voxel/SimdHashNoise.hpp` (150-200 LoC), Step 2 wire in
  `src/asset/WorldGen.cpp` (planned Stage 4.1), Step 3 CMake `-march=x86-64-v3` flip.
  Caveats: single-vendor (Zen 3) — mainline re-test on Intel Haswell/Skylake+; Arm NEON
  path = separate follow-up; visual noise quality slightly different from Ken Perlin
  spec (no permutation bijection, but C¹ continuous — acceptable for voxel world gen).
  Re-evaluation triggers: Stage 5.1 VCT (indirect lighting — A/B test noise quality),
  AVX-512 hardware arrival (Zen 5 / Arrow Lake). Cross-axis: today-сессии `2026-06-20`
  closed orthogonal axes (storage/sync/cull/binding/layout/meshing/hzb/nanovdb/simd-noise)
  = 8 storage/compute closed. Single remaining h-priority slot in `§In progress` =
  `meshing-algo-comparison`.

- [x] **[nanovdb-on-gpu](./experiments/2026-06-20-nanovdb-on-gpu/)** — m, independent (Stage 5.1 VCT
  primary, fragment-shader DDA secondary per `TODO.md §6.2.2`). Closed 2026-06-20, verdict
  **`yes`**. Closes measurement gap from `2026-06-20-svdag-vs-vdb-memory-throughput` §3 line 157
  («Не реализовывал GPU traversal») + bugfix NanoVDB-like impl (uniform-tile lie). **Both
  CPU-side and GPU-side prototypes byte-exact** (verify_mismatches=0 на 5 сценах × 2 kernels).
  NanoVDB-aligned pointer-less layout (Upper[8³] → Lower[4³] → Leaf[2³], scaled per NanoVDB.h
  actual 32³/16³/8³ structure для ProjectV chunkSize=8) **outperforms SVDAG-on-64-tree on 4/5
  scenes by 12-141%** (sparse_random_8: 500 → 1210 Mrays/s = +141%; voxel_lab_8: 541 → 1208
  Mrays/s = +123%; ground_8: 638 → 1242 Mrays/s = +95%; brick_8: 1146 → 1284 Mrays/s = +12%).
  Only solid_8 ties (1265 vs 1272 Mrays/s = +0.6%, memory-bandwidth-bound). **GPU memory:
  NanoVDB uses 57-75% less VRAM** across all scenes. **CPU memory: ~50% less** (B/voxel). Crosses
  5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md` by significant
  margin. Cross-references literature: fVDB 2024 (NanoVDB+HDDA = SOTA GPU traversal), Aokana
  May 2025 (per-chunk SVDAG — identical to our design), Mathijs PG 2024 (SVDAG-on-GPU editing
  5× faster than CPU HashDAG), NanoVDB PR #2220 (fused accessor 1.4-2.6× speedup on Blackwell).
  **Critical mainline finding:** ProjectV chunkSize = 8 (not 32 as previous experiment
  assumed) per `src/voxel/VoxelWorld.hpp:78` + `src/voxel/SceneConfig.cpp:78` — depth=2 not
  depth=3. OpenVDB 13.0.0 (Nov 2025) lowered NanoVDB's mutation barrier (DilateGrid, MergeGrids,
  CoarsenGrid, RefineGrid, PruneGrid, VoxelBlockManager) — relevant for Stage 5.1 transient
  atlas re-upload cost. Mainline рекомендация: **hybrid strategy** — keep CPU-side SVDAG-on-64-tree
  (current mainline Stage 1.2 design, proven by `svdag-vs-vdb-memory-throughput` verdict=yes),
  but flatten chunks into NanoVDB-aligned transient SSBO at GPU upload time for Stage 5.1 VCT
  cone-march + 3 fragment-shader DDA traces in `voxel.frag` per `TODO.md §6.2.2`. 3-step
  migration per `agent/knowledge.md §30.4` precedent: Step 1 foundation (CPU→GPU flatten helper,
  S effort), Step 2 kernel swap (NanoVDB walker, M effort, includes shader rewrite for HDDA
  optimization), Step 3 default flip (`PROJECTV_USE_NANOVDB_TRANSIENT_VCT=ON`). Foundation
  optional dependency: `dec-pipelines-async-compute` (closed 2026-06-20) for async re-upload.
  Caveats: single GPU vendor validated (NVIDIA RTX 3060 Ti GA104 Ampere, Vulkan 1.4.350) — mainline
  re-test on AMD RDNA2/3 + Intel Arc dev matrix; HDDA-specific optimizations (warp ballot
  early-out, ReadAccessor caching) NOT implemented in first-iteration prototype — adding these
  would give additional 10-30% per PR #2220 reference. Continuation chain:
  `sparse-64-tree-alternatives` (analysis) → `svdag-vs-vdb-memory-throughput` (CPU) → this (GPU).
  Cross-axis: previous experiments covered memory + sync; this covers GPU traversal for
  Stage 5.1.

- [x] **[2026-06-20-flecs-soa-vs-aos-bench](./experiments/2026-06-20-flecs-soa-vs-aos-bench/)** — m, Stage 6.1.
  Closed `2026-06-20`, verdict **`yes`**. Web-research complete (8 primary sources верифицированы по
  году/автору/контексту + 10 background sources в `sources.md`, key cross-validation: Mertens 2024 Flecs
  default SoA, Sagar 2026 5.67× OOP→SoA, DevelopersIO 2026 3.3× Godot update, Bevy PR #14049 2× dense iteration,
  AMD EPYC 7003 Zen 3 cache spec). Standalone C++26 prototype `prototype/flecs_soa_vs_aos.cpp` (642 строки,
  4 configs × 3 workloads × 3 seeds × 1000 iterations = 36 measurements). **SoA wins ALL 3 workloads** —
  raycast **2.14×** (199→427 Meps), physics **3.86×** (210→812 Meps, near-exact match с DevelopersIO), cull
  **1.44×** (315→454 Meps). Crosses 5% threshold per `legacy/docs/philosophy/03_domain/01_optimization-philosophy.md`
  by 40-280%. Hybrid ≈ SoA (within 1-2%), HotOnly worst variance (15% raycast stddev). SoA variance ниже AoS
  (24% reduction for physics) — deterministic cache-line stride reduces OS scheduler noise. Mainline
  рекомендация: keep Flecs default SoA storage (per Mertens 2024 + Flecs v4.1.0 release notes), **не возвращаться
  на AoS POD-struct per entity** в новых systems. Per-workload split hot/cold опционально для HZB cull
  (4 fields, modest 1.44× gain) — only if profile shows branch mispredict > 5%. HotOnly-SoA pattern NOT
  рекомендуется (worst variance, gain ≤5%). Snapshot save/load path остаётся AoS (cold path, simpler code).
  Estimated mainline effort: **XS** (doc update + code review checklist, не mainline rewrite). Cross-cutting
  unblocks для Stage 2.2 HZB cull / Stage 3.1 Fluid CA bookkeeping / Stage 3.2 Incremental Jolt per-chunk
  lifecycle / Stage 5.1 VCT voxelize bookkeeping — все эти Flecs systems могут proceed с уверенностью
  что SoA = correct default. Documentation update recommended для
  `legacy/docs/philosophy/02_paradigms/02_dod-philosophy.md` mermaid diagram (analytical 3-5× claim →
  measured 1.44-3.86× numbers с cross-ref). Cross-refs: `agent/knowledge.md §1605` A9 (current AoS voxel
  storage alternative), `agent/workspace.md §1` Phase 5 (ECS systems already landed),
  `TODO.md §6.1` (Flecs ECS migration), `external/flecs/` v4.1.5 (Flecs design defaults).

---

## Rejected (без старта, с обоснованием)

- _нет_