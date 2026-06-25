# Workspace

Текущий рабочий контекст: снимок проекта + активные задачи + недавние milestones.
Долговечные факты и договорённости — `agent/knowledge.md`. Протокол — `AGENTS.md`.
Roadmap — `TODO.md`.

**Pre-reset content (2026-06-24, 24+ активных сессий, phase-by-phase narrative):**
archived at `legacy/docs/archive/2026-06-24-pre-reset-snapshot/workspace.md`. Treat
as historical artifact — see WARNING header in that file. **DO NOT cite as authoritative.**

`COMMENTS.md` was DELETED this session (25x) per operator directive: every file
with a `## \`path\` section got a 1-line trailing comment pointer to the archive
at `legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md`. Archive is
read-only historical reference; per §4 sources-of-truth, the active documentation
lives in `agent/knowledge.md` + `agent/workspace.md` + `TODO.md` + `CHANGELOG.md`.

---

## 1. Now

**2026-06-25 session 27x (TAA motion-vector reprojection fix, this session).**

Свежий baseline после operator-инициированного reset `2026-06-24`:
- 274 pre-reset коммитов squashed в один `chore(reset): pre-fresh-start baseline`
  (`ec6ce4d`). Только master branch; `forge/rtx-feature-lab`,
  `forge/backlog-diversification` удалены.
- `legacy/docs/archive/2026-06-24-pre-reset-snapshot/` — полный pre-reset git history
  bundle + 4 service files (`CHANGELOG.md`, `COMMENTS.md`, `knowledge.md`,
  `workspace.md`) с WARNING headers.
- `CHANGELOG.md` / `COMMENTS.md` / `agent/knowledge.md` / `agent/workspace.md`
  пересозданы как minimal baseline. Содержимое intentional empty до первой
  post-reset сессии.

**Что сделано в этой сессии (27x):**
- **Phase 1a / 6 «Ideal AA pipeline»:** устранён root cause тряски TAA при `taaJitterScale > 0`.
  Джиттер был запечён в projection matrix (`Camera.cpp:242` `projection.c[2] = {jitterNdcX, jitterNdcY, ...}`),
  и `FramePreparation.cpp:280` сохранял jittered prev, и `voxel.frag:1391-1402` использовал
  jittered обе матрицы → motion vector содержал синтетический sub-pixel offset → history lookup
  в неправильную точку → тряска.
- Добавлен `viewProjectionUnjittered` (offset 128, sizeof 128→192) в `GraphicsPushConstants`,
  byte-exact mirrors в `voxel.vert` + `probe_update.comp`.
- `voxel.frag` motion vector теперь `prevUnjittered - currUnjittered` (без sub-pixel jitter).
- `RtxGiProbes.cpp:815` push-constant range bumped от literal 128 к `sizeof(GraphicsPushConstants)`.
- `GraphicsPushConstantsTests` extended 6→8 tests (zero-jitter equivalence, jitter-only difference).
- **Phase 1b (по feedback оператора после теста 1a):** defaults TAA были слишком
  консервативные — `taaBlend=0.10` (10% history!), `taaJitterScale=0.0` (jitter OFF по умолчанию),
  `taaNeighbourhoodRadius=1` (3×3 clamp). При таких defaults приходится крутить jitter
  до 1.5+ чтобы увидеть AA, и тогда rendered scene трясётся sub-pixel каждый кадр. Новые
  defaults: `taaBlend=0.40` (4× stronger history), `taaJitterScale=1.0` (jitter ON by default),
  `taaNeighbourhoodRadius=1` (3×3 — для outlier clamp, не CAS).
- **Variant A полностью (color-space fix, по диагностике оператора):** TAA смешивал
  linear HDR (current frame) с LDR (history, post-tonemap+grading) → undefined operation →
  обводка и тряска. Tonemap + color grading перенесены в `voxel.frag` и `model.frag`
  (применяются ДО output в любом режиме). `taa_resolve.frag` стал pure LDR blend + CAS
  + output. Удалены `ApplyTaaToneMap` / `ApplyTaaColorGrading` и post-blend exposure
  multiplication (второй баг — `x * exposure` после tonemap не равно `tonemap(x * exposure)`,
  яркость осциллировала по frame'ам при смене blend). 39/39 ctest pass, build green,
  validation clean.
- **Known limitation:** `model.frag.taa_on` и `voxel.frag.taa_on` пишут в один и тот же
  attachment `taaSceneColorTarget` (Location 1). Model pass запускается ПОСЛЕ voxel
  и затирает voxel output. TAA resolve видит только model output (без motion vector
  для model). Это отдельный баг, фикс не в scope Variant A — нужно либо отдельный
  attachment для model, либо alpha-compositing перед resolve.
- **Model motion vector fix (по directive оператора):** `model.frag` теперь тоже пишет
  motion vector (Location 3, в shared `taaMotionVectorTarget`). Model pipeline attachments
  2→4 (color, scene, layer, motion). `ModelPushConstants` 128→192 (добавлен
  `viewProjectionUnjittered` для motion vector compute). `model.frag` compute: тот же
  unjittered reprojection что и в `voxel.frag`. Где model рисует — TAA resolve видит
  motion vector модели (корректный reprojection). Где нет model — load op LOAD
  сохраняет voxel motion vector. Был второй источник ghosting'а (model fragments
  reproject'ились по фоновому motion vector'у). 39/39 ctest pass, validation clean.
- Build green (320/320), 39/39 ctest pass, validation layer clean (только pre-existing DDGI
  descriptor warnings про rtxGiIrradiance/rtxGiVolume/rtxGiDistance — НЕ от моего фикса).
- Next phases: 2 (CAS extract) + 3 (SMAA) + 4 (Streamline/DLSS/DLAA) + 5 (UX) + 6 (tests).

**Что сделано в предыдущей сессии (26x):**
- **Корневая причина ярких точек в воде найдена и исправлена:** в `TraceVoxelIntersection`
  (обе копии — `probe_update.comp` и `voxel.frag`) материал после DDA перечитывался через
  `floor(worldHitPos)`, а DDA коммитит hit точно на стенке вокселя → FP-rounding мог выбрать
  воздушный воксель → `EvaluateVoxelLighting` возвращал яркое sky → загрязнение irradiance
  проб / refraction → яркие **белые** точки. Фикс: capture DDA-авторитетного материала
  (`capturedHitMaterial`) вместо re-read.
- **Откатаны 7 DEBUG workaround'ов** в `voxel.frag` (session-26x isolation): они не починили
  точки (источник — DDGI probe data, который они не трогали) и коллатерально отключили RTX sun
  shadows (`sunVisibility=1`), refraction, specular воды, GI shadow-modulation. Тени/refraction/
  specular восстановлены.
- Метод exclusion (3 suppression-теста в `probe_update.comp`): остаточные **голубые** точки на
  water back face — НЕ код-баг, а inherent DDGI coarse-grid (8m) артефакт (opaque floor-bounce,
  видимый на разрешении сетки проб). Open как DDGI quality item (TODO §7.x).
- Chebyshev→Gaussian visibility falloff (follow-up #1) оставлен — легитимный фикс.
- Build green, 39/39 тестов (100%).

**Build state:** green (успешно собирается ProjectV и ProjectVTests).
**Tests:** 39/39 тестов успешно пройдено (100% green).
**Operator policy:** не восстанавливать pre-reset контент без явной команды.

---

## 2. Active tasks

(пусто — post-reset старт. См. `TODO.md` §5.5+ post-RTX-shadow milestones
[7.1 VCT cones, 7.2 TAA, 7.3 tonemap, 7.4 post-FX] как natural next-priority.)

Per TODO.md active section (2026-06-22):
- ⭐ 7.1 VCT cone density upgrade (12-cone diffuse + 4-cone specular).
- ⭐ 7.2 TAA jitter + neighborhood quality (per TODO, but §7.2 CSM quality
  pass was REMOVED 2026-06-22 — different "7.2" kept; agent must disambiguate
  in future planning).
- ⭐ 7.3 Lighting exposure + tone mapping (Reinhard → ACES).
- ⭐ 7.4 Post-processing chain polish (bloom + aerial perspective).
- 🔒 6.2 PIMPL for AppState — DEFERRED PENDING FEASIBILITY.
- 🔒 2.3 Sparse Virtual Texturing — DEFERRED PENDING FEASIBILITY.

---

## 3. Recent milestones

**Snapshot at `2026-06-24` (post-reset baseline) — все pre-reset milestones в
`legacy/docs/archive/2026-06-24-pre-reset-snapshot/CHANGELOG.md` (3420 строк)
и `workspace.md` (331 строк session narrative).** Краткая сводка:

| Milestone | Session | Status |
|---|---|---|
| **Phase 1** SVO + GPU storage | 1.1-1.3 | ✅ closed |
| **Phase 2** GPU-driven geometry | 2.1 HZB, 2.2 Mesh Shaders | ✅ · 2.3 SVT 🔒 deferred-pending |
| **Phase 3** Physics & simulation | 3.1 GPU Fluid CA, 3.2 Incremental Jolt, 3.3 Greedy merger | ✅ all closed |
| **Phase 4** Procedural generation & LOD | 4.1 World Gen, 4.2 LOD, 4.3 Draw distance | ✅ all closed |
| **Phase 5.2** RTX shadows | A (TLAS) / B (ray query) / C (default-on, hard-fail non-RTX) / D (CSM removal) / E (voxel-aware procedural intersection) | ✅ all closed (16x-22x) |
| **Phase 5.3** TAA | motion vectors, YCoCg, CAS, jitter, neighborhood | ✅ closed (post-5.2) |
| **Phase 5.4** RTX AO | replace DDA | ✅ closed (20x) |
| **Phase 5.5** DDGI probes | replace VCT diffuse | ✅ closed (23x) |
| **Phase 5.6** RTX refraction | replace fake transmission | ✅ closed (23x) |
| **Phase 5.7** RTX multi-bounce GI | for specular | ✅ closed (23x) |
| **Phase 5.2.D refactor** | DDA consolidation, refraction self-intersection fix | ✅ closed (24x) |
| **Phase 6** Refactoring | 6.1 ECS migration (UpdateApp 355→49 LoC), 6.3 Async Compute | ✅ · 6.2 PIMPL 🔒 deferred-pending |
| **Phase 7** Rendering polish | 7.1 VCT cones, 7.2 TAA, 7.3 tonemap, 7.4 post-FX | 🔓 all open (post-RTX-shadow) |

Strategic pivots 2026-06-22 (per TODO.md §2 / §26-32):
- **CSM bias tuning → RTX-only path forward** (operator decision).
- **Hardware target = NVIDIA RTX 20/30/40/50** (Turing RT cores или новее).
- **No non-RTX fallback, no legacy уступки** (pet-project scope).

Key per-session snapshots (from `workspace.md` archive):

- **22x (2026-06-22)**: 5.2.E Voxel-aware procedural intersection shadows. 4 new
  shader files (rgen/rint/rchit/rmiss) + RtxShadowPipeline + RtxShadowSBT classes
  + shadow mask image + camera UBO + per-frame descriptor sets. 4 new sub-tests.
  RTX 3060 Ti smoke log clean: `rayTracingPipeline=1`, `tlasInstanceCount > 0`,
  0 validation errors, 0 VMA assertions on exit.
- **23x (2026-06-23)**: RTX shadow instability fixes (blocky shadows, glass
  shadow casting, pitch-black occlusion) + DDGI probe update + RTX refraction
  + RTX multi-bounce GI. 4 closed milestones (5.5, 5.6, 5.7 + refactor).
- **24x (2026-06-24)**: DDA consolidation in `voxel.frag` (`TraceVoxelIntersection`
  helper with `ignoreGlass`/`ignoreFluid`/`rayFlags` parameters) + refraction
  self-intersection fix (glass/fluid columns now render distorted background).
  37/37 tests passing.
- **25x (2026-06-25)**: Post-reset documentation refresh. Knowledge
  + workspace + comments rebuilt from current code. No code changes.
- **26x (2026-06-25, this session)**: Fixed chunk boundary precision misses in `voxel.frag`
  and `probe_update.comp` preventing flickering white dots inside water volume
  (refraction and GI). 39/39 tests passing. **Follow-up:** replaced the sharp
  Chebyshev visibility test in `SampleRtxGiProbeIrradiance` (`voxel.frag`) with
  a smooth Gaussian falloff to eliminate probe-grid aliasing on the water back
  face (small static dots on a regular 8 m probe spacing that jumped on camera
  motion). 39/39 tests still passing. **Follow-up #2:** fixed DDA bug for rays
  starting inside a non-air voxel in `TraceVoxelIntersection` (both
  `voxel.frag` and `probe_update.comp`, plus the inline shadow-ray DDA inside
  `probe_update.comp::EvaluateVoxelLighting`). When a probe was placed inside
  water/glass geometry, the DDA committed at `tCurrent = tMin` and the normal
  computed downstream was derived from the 5 mm position offset instead of the
  actual wall direction, causing the shadow ray inside `EvaluateVoxelLighting`
  to escape to sky for ALL directions → probes stored bright "sky" values in
  their octahedral irradiance map. Fix advances `tMin` past the wall of the
  starting voxel before the DDA loop runs. 39/39 tests still passing.
  **Follow-up #3 (this round):** fixed the hit-normal calculation in the
  `TraceVoxelIntersection` hit block (in both `voxel.frag` and
  `probe_update.comp`). The previous code derived the normal from the 5 mm
  position offset (`insidePos - voxelCenter`), which picked the closest of 6
  face directions based on FP micro-fluctuation — frequently NOT the actual
  wall the ray exited through. The wrong normal propagated into
  `EvaluateVoxelLighting`'s shadow ray: for refraction hits just past a water
  back face, the shadow ray often escaped into the air gap above the water
  (instead of finding more water), giving `shadowFactor = 1` → bright "sky"
  in the refraction result → small bright dots visible in the Final view but
  NOT in any debug view (since refraction is not exposed as a separate debug
  view). Fix: compute normal from the ray's dominant-axis direction (the wall
  a DDA ray exits the voxel through is perpendicular to the axis with the
  largest `|dir|` component). 39/39 tests still passing.
- **27x (2026-06-25, this session)**: Phase 1 of 6 «Ideal AA pipeline». Fixed TAA
  motion-vector reprojection that contained a synthetic sub-pixel offset when
  `taaJitterScale > 0`. Root cause: `BuildGraphicsPushConstants` baked jitter into
  projection `M[2][0..1]`, and that jittered matrix was stored as `taaPrevViewProjectionMatrix`,
  so the reprojection difference carried the per-frame jitter. Fix: added parallel
  `viewProjectionUnjittered` to `GraphicsPushConstants` (offset 128, sizeof 128→192),
  use it for both prev and current in `voxel.frag`. Byte-exact mirrors updated in
  `voxel.vert` and `probe_update.comp`. `RtxGiProbes.cpp` push-constant range bumped
  from literal 128 to `sizeof(GraphicsPushConstants)`. `GraphicsPushConstantsTests`
  extended 6→8 tests. 39/39 ctest pass, validation layer clean (DDGI descriptor
  warnings are pre-existing, unrelated to this fix). **Phase 1b (operator feedback
  after 1a test):** defaults were too conservative — `taaBlend=0.10` (10% history),
  `taaJitterScale=0.0` (jitter OFF), `taaNeighbourhoodRadius=1` (3×3). Bumped to
  `0.40 / 1.0 / 1` for visible AA without per-frame scene wobble. **Phase 1c
  (operator feedback after 1b test, halos):** CAS corner samples in
  `GetSceneColorRange` were reusing the outlier-rejection radius. At radius=3 the
  corners span 6 texels and the CAS high-pass over-shoots contrast edges, producing
  visible halos around every element. Refactored to collect CAS corners in a
  separate fixed ±1-texel window independent of the outlier radius; outlier radius
  default reverted to 1 to keep history clamp conservative. Also fixed inverted
  CAS sharpening formula in `taa_resolve.frag` (`sharpenAmount` was `(1 - blend) *
  max`, now `blend * max` — more temporal averaging correctly applies more
  sharpening). Outlier threshold relaxed `0.40 → 0.60`. 39/39 tests still pass.
  **Variant A (operator directive after 1c test):** root cause of remaining
  outlines/тряска diagnosed as color-space mismatch — `voxel.frag` and
  `model.frag` wrote linear HDR (pre-tonemap+grading) to the TAA scene color,
  while the resolve blended with LDR history. Tonemap+grading moved into
  `voxel.frag` / `model.frag` (applied unconditionally before output),
  `taa_resolve.frag` simplified to pure LDR blend + CAS + output. Removed
  `ApplyTaaToneMap` / `ApplyTaaColorGrading` and post-blend exposure
  multiplication. 39/39 tests still pass, validation clean. Known limitation
  (subsequently fixed this session): model pass overwrites voxel output in
  `taaSceneColorTarget` (both write to Location 1); the TAA resolve sees
  only the model output for that fragment. **Model motion vector fix:**
  `model.frag` now also writes motion vector (Location 3) to the shared
  `taaMotionVectorTarget`. Model pipeline attachments 2→4 (color, scene,
  layer, motion). `ModelPushConstants` 128→192 (added `viewProjectionUnjittered`).
  TAA resolve now reprojects each fragment using the correct motion source:
  model for model fragments, voxel for background. Second TAA ghosting
  source eliminated. 39/39 tests still pass, validation clean.
  **MSAA skeleton (incomplete, default `aaMode = TAA`):** added
  `AntialiasingMode` enum + `MsaaSamplesForMode`/`IsTaaEnabledForMode` helpers
  (`src/render/AntialiasingMode.hpp`), `taaSceneColorMsTarget` field, dynamic
  rendering attachments с conditional MS resolve (только при `msaaSamples > 1`),
  pipeline multisampling derived from `aaMode`. Two blockers for actual MSAA:
  (1) `multisampledRenderToSingleSampled` доступен только в
  `VK_EXT_multisampled_render_to_single_sampled`, не Vulkan 1.4 core — без него
  multi-sampled scene color + single-sample layer/motion attachments в одном
  dynamic rendering pass дают validation errors; (2) альтернатива (все attachments
  multi-sampled) съедает память. После неудачной попытки `storeOp = DONT_CARE`
  в colorAttachment1 сломал single-sample путь (TAA resolve читал uninitialized
  memory), исправлено: `storeOp = DONT_CARE` только для MSAA пути, `STORE` для
  single-sample. Default `aaMode = TAA` — single-sample TAA работает корректно.
  39/39 tests still pass, smoke clean (только pre-existing DDGI descriptor
  warnings). **Operator directive after 27x:** остаточная тряска + слабое
  сглаживание — фундаментальные лимиты single-sample TAA. Переходим к Phase 4
  DLSS/DLAA через NVIDIA Streamline (кросс-платформенный DLSS Super Resolution
  для Linux с драйвером 525.72+, текущий 610.43.02 OK; DLSS-G/Frame
  Generation — Windows-only, не в scope). Next: Phase 4 DLSS/Streamline
  integration (5-7 дней), Phase 5 UX, Phase 6 tests.

---

## 4. Risks / blockers

**Post-reset baseline risks:**

1. **No Windows host verification** — `CMakePresets.json` Windows paths defined
   (clang-cl + LLD) но не post-reset verified. Per AGENTS.md §3: основной dev tree
   = `linux-clang-debug`; Windows = secondary.
2. **Benchmark gated on Linux debug** — `ProjectVFrustumCullBenchmark` only
   builds in `linux-clang-debug*` presets (gated by
   `PROJECTV_ENABLE_BENCHMARKS=ON`).
3. **RTX-only hardware requirement** — non-RTX GPU refuses to start. Это
   сознательное решение (pet-project), но может исключать некоторых контрибьюторов.
4. **CSM fully removed** — нет fallback если RTX не работает. Любая RTX regression
   = complete shadow outage. Mitigation: aggressive ctest coverage на
   `ProjectVRayTracedShadowTests` (29 sub-tests per session 22x).
5. **Stale HUD fields** — `DebugStats::sunShadow*` (strength, depthBias,
   normalBias, filterRadius) никогда не записываются current code; только
   `currentSceneLighting` записывается. Display-строки могут показывать 0.0
   permanently. Future cleanup: remove display-строки or repurpose fields.
6. **Dead hotkeys** — `O`/`U`/`I` (CycleShadowTuningTarget / Decrease-Value /
   Increase-Value) и `L` (ToggleCascadeSplitPlanes) не имеют producer после CSM
   removal. Keys остаются в `InputActions` enum, флаг `showCascadeSplitPlanes`
   сохраняется в `DebugState`. Cleanup candidate.
7. **Many pre-reset invariants** (release flags, Tracy UI split, sccache setup,
   build-preset target list) — re-validated against current code, но отдельные
   cmake-флаги были перемещены с preset-level на CMake-level (release compile
   flags теперь в `CMakeLists.txt:58-71`, не в preset override).

---

## 5. Safety-net

(пусто)

---

## Cross-refs

- `agent/knowledge.md` — 36 действующих engineering contracts + 5 runtime facts
  (post-reset, rebuilt from code 2026-06-25).
- `AGENTS.md §7` — рабочий чеклист, §4 — sources of truth, §5 — протокол коммитов.
- `TODO.md` — roadmap + 5.2-5.7 RTX milestones (closed) + 7.x post-RTX polish
  (open).
- `legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md` — pre-reset
  design-rationale archive (read-only; pointer in each source file).
- `docs/VulkanSDK-Linux-Docs-1.4.350.1/` — вендорная документация Vulkan 1.4.
- `runtime/scene.json` — default scene config (VoxelLab preset).
- `runtime/captures/` — lookdev capture outputs.
- `CHANGELOG.md` — minimal post-reset [Unreleased] entry.
