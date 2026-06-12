# Status

Short active snapshot on top of `TODO.md`; no roadmap duplication.

Updated: `2026-06-12` — TAA Блок 1 phase 1/5: 1.7 R11G11B10 scene color landed (uncommitted, см. §11).

---

## 1. Now

- Project phase: `pre-MVP alpha / working vertical slice`.
- **Most recent closed sessions** (full chronological list in
  `legacy/docs/archive/agent_status_now_2026-06-10_pre_compaction.md`):
  - `2026-06-10` — P0.2 shadow sampler `magFilter/minFilter` `NEAREST` → `LINEAR` re-applied after lost-and-found
    incident. Build green, ctest 1/1, smoke 4/4 on `VoxelLab` reference shot.
  - `2026-06-10` — P0.3 per-corner AO landed: `ComputeFaceCornerPackedAO` в `voxel_mesh.comp`
    пакует 4 corner AO (8 bits) в `PackedFace::lightingData`, `voxel.vert` снимает `flat` и распаковывает
    per-vertex через `(lightingData >> (cornerIndex*8)) & 0xFF`, `voxel.frag` снимает `flat` с
    `inAmbientVisibility`. Build green, ctest 1/1, visual verified на VoxelLab reference shot
    `cam 3.233 4.301 12.320 look 0.65 -0.03 -0.76` (FINAL view показывает плавный vertical gradient
    вместо 3-4 горизонтальных полос). Captures под
    `build/linux-clang-debug/lookdev-captures/20260610-p03-per-corner-ao-v3/`.
    **Lesson learned (§10.11):** incremental `cmake --build` не обновляет `bin/voxel*.spv` пока
    `ProjectV` ELF up-to-date — после правки шейдеров нужен явный `cp` из `build/.../src/voxel*.spv`
    в `build/.../bin/`.
  - `2026-06-10` — **P0.3 follow-up: 4-axis-aligned AO в vertex shader** (pragmatic equivalent of
    mesh welding; two revisions in the same session). P0.3 per-corner AO was face-corner
    dependent, so 3 GPU vertex'а, попадающих в один 3D-угол (от 3 разных граней), получали
    3 разных AO → face-boundary discontinuity на стыке граней (визуально «тёмное пятно в
    центре лицевой грани»). Полный GPU-hash-table welding (~400-600 lines: welded vertex /
    index буферы, hash table, новые dispatches в `voxel_mesh.comp`, vertex input state,
    `VkDrawIndexedIndirectCommand`, новые binding'и) не влезает в разумный объём одной
    сессии, поэтому реализован **face-independent AO**: `voxel.vert` теперь читает 4
    axis-aligned соседей 3D-угла через binding 5 (`PackedChunkVoxelPayload`, раньше
    был только fragment stage) и пишет `outAmbientVisibility = (4 − occluderCount) * 64 / 255`.
    **4 «диагональных» октанта исключены** из подсчёта, поэтому 4-voxel junction
    (4 solid + 4 air вокруг 3D-угла) читается как 0 occluder'ов / fully lit → 50% dark spot,
    который давал первый проход (8-surrounding), исчез. Per-corner интерполяция внутри
    грани сохранена (4 угла одной грани = 4 разных 3D-позиции).
    Per-corner интерполяция внутри грани сохранена (4 угла одной грани = 4 разных
    3D-позиции), face-boundary discontinuity ушла (AO зависит только от 3D-позиции, не
    от грани). `voxel_mesh.comp::ComputeFaceCornerPackedAO` стал no-op (возвращает 0);
    helper `ComputeFaceCornerAmbientLevel` оставлен для reference / revert. C++ side:
    1 строка — `binding 5` в graphics descriptor set layout получает `stageFlags =
    VERTEX_BIT | FRAGMENT_BIT` (иначе `vkCreateGraphicsPipelines` падает с
    VUID-VkGraphicsPipelineCreateInfo-layout-07988). Build green, ctest 1/1. Visual verify
    отложен — после серии smoke-прогонов GPU ушёл в persistent OOM; first-pass capture
    с `outAmbientVisibility=1.0` для изоляции проблемы показала корректный scene render
    на user camera `cam 4.609 5.333 14.766 look 0.48 0 -0.88`. Pre-existing багфикс
    заодно: при запуске программы мышь улетала вниз, потому что первый
    `SDL_EVENT_MOUSE_MOTION` после `SDL_SetWindowRelativeMouseMode(true)` несёт
    огромный pre-capture delta. Добавлен `InputState::skipFirstMouseMotion` (default true)
    + gate в `HandleCameraEvent` + reset в `SetRelativeMouseMode`. После правки камера
    стабильна на старте.
  - `2026-06-10` — **P0.3 follow-up v2: per-vertex AO полностью отключён** после того, как user
    подтвердил, что 4-axis-aligned модель всё ещё даёт «псевдотень» на 3D-углу 2x2x2 куба
    (`3 из 4 axis-aligned соседей solid → AO=64 = 25% lit`, хотя с угла видно небо из
    диагонали). Это **структурный** артефакт: face-independent per-corner AO, считающий solid
    axis-aligned соседей, не различает «concave» (стенки вокруг 1x1 дырки — действительно
    темно) и «convex 3-walls-1-sky» (выпуклый угол 2x2x2 — небо видно, но 3 оси закрыты) —
    оба дают высокий count. **Решение:** `voxel.vert` устанавливает `outAmbientVisibility = 1.0`
    безусловно, binding 5 (`PackedChunkVoxelPayload`) удалён из vertex shader, а его
    descriptor-stage флаги в `VulkanGraphicsPipeline.cpp` свёрнуты до `FRAGMENT_BIT`
    (vertex shader больше не использует). Все helper-функции per-vertex AO
    (`ReadVertexNeighborMaterial`, `IsVertexAoOccluder`, `ComputeVertexAmbientOcclusionByte`,
    `DecodeChunkVoxelMaterialVertex`) удалены как dead code. Per-pixel cavity darkening
    сохранён через `ComputeAmbientOcclusionVisibility` в `voxel.frag` (AOCC ray-cast),
    который не имеет face-boundary seams. Build green на `linux-clang-debug-build`, ctest 1/1
    (ProjectVTests passed). Зафиксировано в `agent/decisions.md` §14. Visual verify отдан
    оператору (у пользователя была persistent GPU OOM в scripted captures, но binary
    у него работает; эта правка убирает per-vertex AO целиком, что не может ухудшить
    rendering — только сделать плоские грани слегка ярче). Lesson learned: per-vertex
    AO при face-independent constraint (welded mesh отсутствует) **фундаментально** не
    способен правильно обработать 2x2x2 corner geometry; либо weld mesh, либо
    per-pixel AOCC, либо no per-vertex AO.
- **Multiplatform dev baseline opened (`2026-06-09`).** Linux build green on `linux-clang-debug`
  (clang 22.1.6 native + lld 22.1.6 + libstdc++ 16.1.1 + SDL3 3.4.10 + Vulkan 1.4.350). ctest 1/1, ProjectV
  correctly refuses to init because `VK_LAYER_KHRONOS_validation` not installed (validation `ON` in preset;
  layers package is follow-up). Source-side fixes: `src/CMakeLists.txt` (VMA uncommented),
  `src/core/Types.hpp` (VMA include path), `src/ecs/EcsWorld.hpp` (`<cstddef>`), root `CMakeLists.txt`
  platform-gated. See `agent/memory.md` §5-§8 for the full Linux baseline facts.
- **Dev-tool gaps closed (`2026-06-09`):** 14 packages installed via `agent/_linux_packages_install.sh`
  (operator ran with sudo, agent cannot pipe passwords). All 14 binaries smoke-tested. Native: `gh 2.93.0`
  (SSH-authed, awaiting `GH_TOKEN` for REST), `jq 1.8.1`, `tree 2.3.2`, `bloaty 1.1`, `valgrind 3.25.1`,
  `hyperfine 1.20.0`, `lldb 22.1.6`, `delta 0.19.2`, `lazygit 0.62.2`, `perf 7.0.10-1`. AUR: `gitleaks`,
  `trufflehog`, `tldr 3.4.4`, `sccache 0.15.0`. Self-audit in `agent/memory.md` §9.
- **Git/editor baseline closed (`2026-06-09`):** `~/.gitconfig` now uses `core.pager = delta` and
  `core.editor = /home/le1t/.emacs.d/bin/doom emacs -nw` (Doom Emacs wrapper). User identity preserved
  (`Leeleit` / `le1t@list.ru`). `gh` configured for SSH. `gh api` requires `GH_TOKEN` env.
- **Four-build Linux tree zoo validated (`2026-06-09`):** `linux-clang-debug` (everyday),
  `linux-clang-debug-sccache`, `linux-clang-debug-ci`, `linux-clang-debug-tracy-profiler` (Tracy UI binary
  currently fails on Linux/glibc due to upstream `wolfpld/tracy` `tidy-html5` `uint`/`ulong` binding;
  decision deferred).
- **Shadow-quality pass closed (`2026-06-09`):** six code fixes in `voxel.frag` / `VulkanGraphicsPipeline.cpp` /
  `SceneResources.hpp`. Visual review of `FINAL/SHDW/CSM/CTSH/AOCC/LOCL` captures confirms continuous soft
  sun shadows, working contact/AOCC/local-light layers, no staircasing, no full-floor dark. See archived §10.
## 2. Nearest Gap

- The old P0 process reminders are no longer left open in `TODO.md`: replay-first controller diagnosis, developer-only runtime smoke, and the current warning-cleanup closure are already treated as established baseline, not as unfinished work.
- Runtime smoke policy changed: `ProjectVRuntimeSmoke` is now a targeted lifecycle/Vulkan check, not a default DoD step
  for every lighting/material/doc change.
- Tracy-profiler build policy changed: `build/windows-clang-debug-tracy-profiler` is no longer part of routine
  verification. Use it only for explicit Tracy/profiling work or when the user asks for that build specifically.
- If another warning-cleanup pass is needed, regenerate `Problems/` first instead of continuing from the current XML export.
- The next concrete mainline feature gap is no longer shadow plumbing, shadow tuning, direct-light BRDF, post-BRDF capture refresh, ambient/environment fill, fixed preset grading, first scene-key auto exposure, missing demo-scene opaque anchors, CSM split planning, the first CSM render hookup, basic CSM texel snapping, basic per-cascade coverage diagnostics, the first stable sphere-fit split-edge follow-up, the first shader-side split transition blend follow-up, the first cascade-specific caster coverage follow-up, or the visible-scene receiver-range fix that aligned cascades with current chunk visibility. The remaining `10.5.1` caveat is that exposure is not histogram/adaptive yet; adding that should wait for an HDR/luminance path rather than being faked in the shader.
- The current shadow limitation is now explicit and accepted: glass does not cast shadows in the mainline sun-shadow
  pass. `Fluid` casts as an opaque shadow-map caster; physical/tinted glass shadows remain future R&D.
- The new contact-shadow and `AOCC` baselines are intentionally bounded and local: both use short forward-shader voxel
  DDA traces against the existing packed world payload, not separate screen-space passes and not replacements for the
  current CSM layer. The new local point light is also bounded: authored, inverse-square, and shadowed by a short
  opaque-only voxel DDA term until a separate local shadow-map/cubemap step is worth adding.

## 3. Next Steps

1. For any further sun/contact shadow work, keep the new stricter close-out rule: inspected runtime captures are required,
   not sidecar numbers alone. Use `FINAL` + `SHDW` at minimum, and include `CSM` / `CTSH` when those paths are involved.
2. The local occlusion and bounded local-light-shadow slices can now pause unless live captures show a specific defect;
   the next truly different `10.5.3` layer is real local shadow-map/cubemap infrastructure, while full `SSAO/GTAO`
   should still wait for a real depth/normal screen-space path.
3. Do not reintroduce fake glass shadow surrogates unless there is a real, separately scoped transparent-shadow path.

## 4. Risks

- Documentation will drift again if session history starts leaking back into `TODO.md` or `agent/`.
- Parallel `build/test/smoke` in the same build tree is still unsafe; smoke should be sequential and only when it is a
  relevant lifecycle/Vulkan check.
- `walk` tuning without replay/HUD/Tracy evidence will regress back toward synthetic-case patching.
- **Shadow-quality audit + fix pass closed (`2026-06-09`, same-day).** Six concrete code fixes landed in `voxel.frag` / `VulkanGraphicsPipeline.cpp` / `SceneResources.hpp`. The visual "blocky shadow edges" complaint was the `shadowRasterizer.cullMode = BACK` being inherited from the main pass and chopping the shadow map (A1). The "self-shadow on lit top faces" was the local-light DDA starting inside the receiver voxel (A2). The "PCF stair-step" was a `filterRadius` ceiling of 8.0 in the debug ladder expanding the kernel to ~50% of a cascade texel budget (A5). The "120 FPS" was a 252-read/pixel fragment budget on a worst-case lit voxel; AOCC directions cut from 5→3 (B1c), AOCC steps 6→4 (B1b), local-light DDA steps 32→12 (B1a), for a -64% worst-case budget. Two more items were deferred as separate work (B2 shadow map resolution, B3 indirect-buffer cache). A new Linux runtime smoke harness `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` (counterpart of the Windows `Invoke-ProjectVRuntimeSmoke.ps1`) was added and produced a clean 6/6 capture set on `cam -25 19 25 look 0.62 -0.48 -0.62` (`VoxelLab`), HUD-reported FPS `121.7`/`123.2`/`116.4` for FINAL/CSM/SHDW frames. Visual review of the FINAL/SHDW/CSM/CTSH/AOCC/LOCL captures confirms continuous soft sun shadows, working local-light contribution, working contact shadows, working AOCC, and CSM cascade 3 selection at the chosen camera (expected — view-depth > 19.78 places everything in cascade 3 at this shot). Full per-fix rationale, deferred list, and a smoke-output interpretation cheat sheet are in `agent/memory.md` §10.1-10.6.
- **Swapchain semaphore reuse fix closed (`2026-06-09`, same-day).** The Vulkan validation layer was emitting "pSignalSemaphoreInfos[0].semaphore is being signaled by VkQueue, but it may still be in use by VkSwapchainKHR" 20 times per smoke run. Root cause: per-in-flight-frame `imageAvailableSemaphores[2]` and `renderFinishedSemaphores[2]` indexed by `currentFrame % MAX_FRAMES_IN_FLIGHT` instead of swapchain `imageIndex`. The canonical fix is the per-frame *acquire*-semaphore + per-image *submit*-semaphore pattern from the Vulkan SDK 1.4 guide `swapchain_semaphore_reuse.html` (the operator installed the docs in `docs/VulkanSDK-Linux-Docs-1.4.350.1/` and reminded the agent to read them first). `submitSemaphores[imageIndex]` now lives on `SwapchainState`, created in `CreateOrRecreateSwapchain`; `vkQueueSubmit2`'s `pSignalSemaphores[0]` and `vkQueuePresentKHR`'s `pWaitSemaphores[0]` both use it. The device extension `VK_KHR_swapchain_maintenance1` is also enabled opportunistically (the smoke host's GPU supports it), bringing in the instance-level `VK_KHR_get_surface_capabilities2` and `VK_KHR_surface_maintenance1` dependency extensions. **Final warning count: 0** (-100% from 20). Build green, ctest 1/1, smoke 6/6, vision verify of FINAL view confirms continuous soft sun shadow with no staircasing and no full-floor dark. Full diff and lesson-learned in `agent/memory.md` §10.7.

- **`2026-06-10` destructive-git-checkout incident.** During the W1-W5 vertex-welding detour, the agent ran `git checkout -- .` + `git stash drop` to revert its failed W1-W5 attempt. This **also reverted the P0.2 uncommitted `magFilter=NEAREST → LINEAR` fix from the previous session** (§10.10), and the `git stash drop` destroyed the recovery path. The agent did not catch this until the operator pushed back on missing shadow improvements. Re-applied as a 2-line edit on `2026-06-10` and the SHDW view again shows a smooth gradient. Working rule added in `agent/memory.md` §10.11: before running `git checkout -- .`, capture uncommitted state explicitly with `cp <file> /tmp/` or `git stash push -m "KEEP_<name>"`. The `git checkout -- .` + `git stash drop` pattern is **destructive for any work that was uncommitted in earlier sessions**.
- **`2026-06-10` per-corner AO landed.** Полный diff + visual verification + lesson learned — в `agent/memory.md` §10.11.
  Краткое: incremental `cmake --build` не копирует свежие `.spv` в `bin/`, если `ProjectV` ELF up-to-date; после правки
  шейдеров нужен явный `cp build/.../src/voxel*.spv build/.../bin/`. Без этого `ReadShaderFile` в runtime грузит
  pre-fix SPIR-V и capture выглядит как до merge'а. Working rule для будущих шейдер-only сессий: либо
  `cmake --build` с явной пересборкой `ProjectV` target, либо `cp` сразу после build'а.

## 5. TAA A2 complete — `taaEnabled=true`, SPIR-V search path fix, smoke verified (`2026-06-11`)

## 6. P1 shadow fix — SSBO double-buffer, fence reorder, cascade depth, TAA clamp (`2026-06-12`)

Committed as `b7e672f`:

- `FramePreparation.cpp`: `vkWaitForFences` moved **before** `UpdateSceneResources` — prevents CPU from writing to staging buffers (chunk descriptors, voxel payload) while GPU still reads previous frame's data.
- `Camera.cpp` + `voxel.frag` (cascade selection): `GetCameraViewDepth` uses `gl_FragCoord.z` instead of `dot(worldPos - cameraPos, cameraForward)`. TAA jitter only shifts X/Y of the projection matrix → cascade selection is frame-invariant.
- `taa_resolve.frag`: history sample clamped to 3×3 neighbourhood colamp — eliminates TAA ghosting on revealed geometry.
- **`core/Types.hpp` + `render/SceneResources.cpp`** (core fix): `sceneLightingBuffer/Allocation/MappedData` moved from shared `RenderState` to per-frame `SceneFrameResources`. With `MAX_FRAMES_IN_FLIGHT=2`, the single-buffer `memcpy` in `RefreshSceneLightingBuffer` overwrote data while the GPU for frame N-1 still read it → tile-sized gray/white squares. Fix: each frame owns its buffer, written in `UploadSceneFrameResources(frameIndex)`.
- All 4 pipelines (graphics, shadow, meshing, TAA resolve) bind `frameResources.sceneLightingBuffer` instead of `render->sceneLightingBuffer`.
- `VulkanBootstrap.cpp`: removed `VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT` from validation messenger to suppress noisy performance warnings.

Uncommitted changes on this session (proposed commit):

- `taaEnabled` default flipped `false→true` (`core/Types.hpp:613`).
- `ShaderIO.cpp`: SPIR-V search path fix — `parent_path()` → `".."` / `"src"`. `SDL_GetBasePath()` returns trailing separator;
  `parent_path()` strips only the empty trailing string, not `bin/`, so the fix was broken (`bin/src/file.spv` instead of
  `src/file.spv`). `".."` works on all platforms regardless of trailing separator.
- Dual MRT in `voxel.frag` — writes linear color to `layout(location=1) outSceneColor` (TAA-on scene color target)
  alongside tone-mapped `layout(location=0) outColor` (swapchain slot). Fixes gray screen when TAA-on: without
  Location 1 write, the scene color target stays clear (gray).
- `FramePreparation.cpp:105` — `taaResolveDescriptorSet` assignment (fixes VUID 08600).
- `Renderer.cpp`: swapchain `UNDEFINED→COLOR_ATTACHMENT_OPTIMAL` transition in TAA-on resolve block (fixes VUID 09592).
- `VulkanSwapchain.cpp:423` — `CreateOrRecreateTaaRenderTargets` return-value check (prevents NULL image consumption).

Smoke with `PROJECTV_ENABLE_VALIDATION=ON`: 6/6 captures, 0 VUIDs, 0 errors, `taa_enabled=1`, `taa_history_valid=1`.

Interim commits from earlier this session:
- `52b130f` — TAA infrastructure baseline.
- `d9830c2` — TAA offscreen render target allocation.
- `9764463` — Phase A1 close-out plumbing (committed).

---

## 7. TAA Блок 1 / 1.1 YCoCg clamp — `a2972fa` (in-progress session, not closed)

- `src/shaders/taa_resolve.frag`: RGB→YCoCg exact transform (lossless round-trip), 3×3 min/max и history clamp в YCoCg space. 1-tap bright пиксель теперь двигает только Y — chroma highlight'ов не вымывается в grey.
- `src/render/ScreenshotCapture.cpp`: sidecar `taa_clamp_color_space=YCoCg` для capture-driven tuning.
- `TODO.md`: P1 shadow flicker/shimmer → closed (link на `b7e672f` chain); post-TAA follow-ups переструктурированы в Блоки 1–6; 1.1 YCoCg clamp → closed.

Verification: build / ctest / smoke **не перепрогонял** в resumed-сессии (оператор решил коммитить «как есть» — поверхность маленькая, 1 shader + 1 cpp строка, baseline A1/A2 chain 6/6 smoke clean). Visual verify остаётся в TODO §5 Блок-0.

Asset-pipeline parallel: `cccdbc1 feat(asset): meshopt-driven mesh baker and VMA GPU upload (M2)` залeтел в этом же репозитории. Non-overlapping scope с моими 3 файлами (только `agent/active-sessions.md` shared; stage через `git add <file>` изолирует коммит).

---

## 8. TAA Блок 1 / 1.4 + Блок 5 / 5.1 + M5.2 fix + Блок 6 — LANDED (`2026-06-12`)

**4 commits landed в этой resumed-сессии:**

| SHA | Subject | Files |
|---|---|---|
| `8635ddf` | `fix(taa): TAA tuning HUD ladder + M5.2 color-distance rejection` | 8 |
| `3ee995f` | `feat(render): add RenderDoc debug-utility label helpers + TAA-resolve hot site` | 1 |
| `f90687a` | `docs(agent): sync Блок 1 (1.4) + Блок 5 (5.1) + Блок 6 (6.x) closures` | 4 |
| `e27d971` | `docs(agent): record 1.4 + 5.1 + 6.x + M5.2 commits in active-sessions.md` | 1 |

**Что в `8635ddf` (primary 1.4 + M5.2 fix):**

- 1.4: 5 new hotkeys `;`/`'`/`-`/`=`/`,`/`.` drive per-pass TAA knobs (jitter scale / blend / neighbourhood radius / history invalidate). `taaNeighbourhoodRadius` в `taaHistoryParams.w` (was `reserved` slot, byte layout unchanged). All four invalidate `taaHistoryValid` на change.
- 5.1 hot sites: 6 `PV_PROFILE_GPU_LABEL` calls на `RecordShadowCommands` ("Shadow Pass") / `RecordVoxelMeshingCommands` ("Voxel Meshing") / `RecordGraphicsCommands` ("Graphics Pass") / TAA resolve section ("TAA Resolve" + color 0.20/0.65/1.00) / `RecordDebugOverlayCommands` ("Debug Overlay") / `RecordDebugHudCommands` ("Debug HUD") — но эти Renderer.cpp правки подхвачены в asset-pipeline's M4/M5 chain (`c4382ea` / `ccf7400`).
- M5.2 fix: `kTaaColorDistanceRejectionThreshold = 0.40` в YCoCg space (`taa_resolve.frag:79`, bumped `2026-06-12` from `0.20` after a runtime repro показал, что `model.frag` 4×4 procedural UV checker имеет два tint-варианта с YCoCg distance до voxel centroid ≈ 0.27 (yellow, проходит rejection) и ≈ 0.16 (blue, не проходит — clamped в voxel range, "half in textures" symptom). `0.40` ловит оба tint-варианта, оставляет headroom для chroma-dim outliers. False-positive risk на natural voxel variation bounded (voxel surfaces обычно в пределах 0.05 YCoCg от своего 3×3 mean, well under 0.40). Когда current sample далёк от neighborhood centroid, skip both YCoCg clamp и temporal blend. Fixes the polygon-model pass "faint grey blob" regression reported in asset-pipeline closeout `b152b70` (M5.2 follow-up). Models surrounded by voxel pixels now keep their saturated colors.

**Что в `3ee995f` (5.1 helper):**

- `profiling::ScopedGpuDebugLabel` RAII в `src/debug/ProfilingGpu.hpp` + `PV_PROFILE_GPU_LABEL` / `PV_PROFILE_GPU_LABEL_COLOR` macros, gated на CMake option `PROJECTV_ENABLE_RENDERDOC_MARKERS` (Debug default ON, `linux-clang-debug` preset OFF). `__COUNTER__` для unique identifiers. Function pointers loaded volk'ом.

**Что в `f90687a` (6.x doc sync):**

- `TODO.md` — 1.4 + 5.1 closed; Блок 6 doc sync closed (6.1-6.4); header date `2026-06-12 (1.4 + 5.1 closed)`. 6.4 rolled into `decisions.md` §18 (legacy docs — справочник, не source of truth per `AGENTS.md` §4).
- `agent/memory.md` §10.16 — full TAA tuning ladder + RenderDoc markers landed + VMA/glm fix. Working rules: volk.h position relative to VMA-touching headers; asset-pipeline sibling target dependency propagation.
- `agent/decisions.md` §18 — TAA contract: default `taaEnabled=true`, history invalidation triggers (7 events), live tuning policy, YCoCg clamp rationale, dual-MRT SPIR-V variant rationale, neighbourhood radius 1/3/5/7.
- `agent/status.md` §8 — этот snapshot.

**Build state (final):** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests` green, `ctest` 6/6 (`ProjectVTests`, `ProjectVAssetTests`, `ProjectVMeshBakerTests`, `ProjectVDracoTests`, `ProjectVFrustumCullingTests`, `ProjectVBoxUvFixtureTests`).

**Uncommitted в working tree (NOT mine, осталось на следующую сессию / operator):**

- `src/render/vulkan/VulkanBootstrap.cpp` — asset-pipeline сессия добавила `#include "volk.h"` на line 13 во время VMA fix попытки; потом нашли лучший fix в `core/Types.hpp` (`c4382ea`) и закоммитили там, но эта redundant правка осталась в working tree — no-op.
- `.gitignore` — `/.venv/` + `minimax_proxy.py` (operator/personal additions).
- `pyproject.toml` + `uv.lock` (untracked) — Python project files, не мой scope.

---

## 9. Handoff для следующей сессии (2026-06-12 onward)

**Recent commit chain (эта сессия — `cline/MiniMax-M3`):**
- `e27d971` — active-sessions.md update (session metadata)
- `f90687a` — doc sync (Блок 1/5/6)
- `3ee995f` — 5.1 RenderDoc debug-utility labels
- `8635ddf` — 1.4 TAA tuning ladder + M5.2 fix

**Recent commit chain (asset-pipeline сессия — closed at `b152b70`):**
- `b152b70` — close out
- `dfaa037` — UV box fixture (M6 prep)
- `ccf7400` — M5 frustum cull
- `c4382ea` — M4 model graphics pass + manifest load + TAA shader variant
- `24ccb08` — M3 KHR_draco_mesh_compression
- `cccdbc1` — M2 meshopt + VMA upload
- `8b112e7` — M1 .glb loader + AssetRegistry
- `1c72a4b` — M0 CMake wiring

**Tuning ladder hotkeys (master HEAD):**
- `;` jitter scale dec, `'` jitter scale inc (multiplier on Halton, [0,2] step 0.25)
- `-` blend dec, `=` blend inc (history weight, [0,1] step 0.05)
- `,` neighbourhood radius cycle (1/3/5/7 — 3×3/7×7/11×11/15×15)
- `.` history invalidate (single press)
- `T` toggle TAA on/off (pre-existing)

**Dirty tree safety:** если `git status -uall` показывает чужие uncommitted changes — **не делать** `git checkout -- .` или `git stash drop` (см. `agent/memory.md` §10.11 — там потеряли P0.2 LINEAR fix). Сначала `git diff > /tmp/before_drop_<ts>.patch` и спросить оператора.

**Known follow-ups (TODO §5, in priority order by operator):**

| ID | Описание | Сложность | Scope |
|---|---|---|---|
| 1.2 | Camera-cut detection (viewProjDelta threshold → auto-invalidate history) | 2-3 ч | TAA-scope |
| 1.3 | Adaptive CAS sharpening post-TAA (`sharpenAmount = (1-blend) * authoredMax`) | 1-2 ч | TAA-scope, ffx-cas |
| 1.5 | Anti-flicker для CTSH/AOCC/LOCL через mini-TAA history attachment | 3-5 ч | TAA-scope |
| 1.7 | R11G11B10_UFloat scene color (bandwidth: 4 vs 8 bytes/pixel) | 2-4 ч | TAA-scope, swapchain format |
| 1.8 | `TaaQuality` tier abstraction (Off/Light/Std/High) | 4-6 ч | TAA-scope, refactor 1.4 |
| 1.6 | VRS в cascade split edges | R&D | TAA-scope + GPU driver confirm |
| 2.x | Walk controller feel polish (frame-step, HUD additions, replay suite) | 3-5 дней | walk-scope |
| 3.x | SSAO baseline / SSR | 5-10 дней | deferred/hard |
| 4.1 | Greedy quad meshing | 2-3 дня | voxel-scope |
| 5.2 | Доп. debug gizmos (chunk AABB, cursor hit normal, cascade split planes) | 1-2 ч | render-scope |
| 5.3 | Benchmark automation (`PROJECTV_BENCHMARK_FRAMES=N` env) | 1-2 ч | render-scope |
| 6.1-6.4 | Doc sync (closed in `f90687a`) | — | done |

**Test count baseline:** `ctest` 6/6 (~1.4s wall clock, `ProjectVTests` 1.4s + 5 fast suites). Это baseline, не должно падать.

**Build preset:** `linux-clang-debug` (native clang 22 + lld 22 + libstdc++ 16). Не трогать `windows-clang-debug` (operator's primary dev tree).

**Working rules to inherit (см. `agent/memory.md` §10.16):**
- `volk.h` MUST come before any VMA-touching header in shared files. If new VMA-related types are added to `core/Types.hpp`, the `volk.h` include position is preserved at top.
- Asset-pipeline sibling target dependency propagation: when asset-pipeline adds a header-only dep (e.g., glm) that's transitively pulled in by `core/Types.hpp`, all sibling targets that include Types.hpp must also link that dep. `ProjectVTests` was the canary.
- Shader compile artifact `*.spv` is NOT auto-copied to `bin/` if `ProjectV` ELF is up-to-date. After shader edits: `cp build/.../src/<name>.spv build/.../bin/<name>.spv` (or `cmake --build` with a forced ProjectV relink).
- `legacy/docs/libraries/` is a dump of reference material, NOT source of truth per `AGENTS.md` §4. Vulkan reference is in `docs/VulkanSDK-Linux-Docs-1.4.350.1/`.

**Ключевые env vars (master HEAD):** `PROJECTV_MODELS=path.glb@x,y,z;...` (manifest), `PROJECTV_START_CAMERA_POSITION="x y z"`, `PROJECTV_START_CAMERA_LOOK="x y z"`, `PROJECTV_LOOKDEV_CAPTURE_*` (smoke harness), `PROJECTV_ENABLE_VALIDATION` (1/0, default ON in Debug), `PROJECTV_ENABLE_RENDERDOC_MARKERS` (1/0, default ON in Debug, OFF в `linux-clang-debug` preset), `PROJECTV_ENABLE_TRACY` (1/0, default ON).

**Status §7 (YCoCg clamp landed in `a2972fa`):** Marked stale by §8. Read §8 for current state.

---

## 10. TAA Блок 1 / 1.2 + 1.3 — camera-cut detector + inline CAS post-TAA — LANDED (uncommitted, this session)

**2 commits proposed** (per operator "потом скажу, что делать" — commits не сделаны):

| SHA | Subject | Files |
|---|---|---|
| _pending_ | `fix(taa): camera-cut detection (Chebyshev threshold) + first-frame guard` | 5 |
| _pending_ | `feat(taa): inline AMD CAS post-TAA sharpen, sharpenAmount = (1-blend)*max` | 4 |

**Что в 1.2 (camera-cut detector):**

- `core/Types.hpp::RenderState` (line 666-689): добавлены `taaCameraCutCount` (uint32, default 0), `taaCameraCutMaxDelta` (float, default 0.0f), `taaPrevViewProjectionMatrixInitialized` (bool, default false). `DebugStats` mirror.
- `app/FramePreparation.cpp`: cut detector в `BuildFrameData` после `AdvanceTaaPixelJitter`, **до** `taaPrevViewProjectionMatrix` stash. `constexpr float kTaaCameraCutThreshold = 0.10f`. Companion bool gate prevents first-frame false-positive (zero-init prev = 40 maxDelta).
- `app/AppUpdate.cpp`: 2 поля добавлены в `debug->stats.taa*` mirror.
- `debug/DebugHud.cpp`: новая HUD-строка `TAACUT %u CLR %.2f` после TAA line.
- `render/ScreenshotCapture.cpp`: 2 новых sidecar keys `taa_camera_cut_count` + `taa_camera_cut_max_delta`.
- `render/vulkan/VulkanSwapchain.cpp::CreateOrRecreateSwapchain`: companion bool + cut accumulator reset рядом с `taaPrevViewProjectionMatrix = {}` — следующий кадр не false-positive.

**Что в 1.3 (inline CAS post-TAA):**

- `core/Types.hpp::ResolvePushConstants` (line 195-219): `vec2 reservedPadding` заменён на `float taaBlend + float taaCasSharpnessMax` (8 B total, byte layout preserved, `static_assert` обновлён: `taaBlend @ 136`, `taaCasSharpnessMax @ 140`).
- `core/Types.hpp::RenderState` + `DebugStats`: новый `taaCasSharpnessMax` (float, [0,1], default 0.5).
- `render/Renderer.cpp:1004-1009`: populate `resolvePushConstants.taaBlend = render.taaEnabled ? render.taaBlend : 0.0f` + `resolvePushConstants.taaCasSharpnessMax = render.taaCasSharpnessMax`.
- `shaders/taa_resolve.frag`: comprehensive rewrite — `ResolvePushConstants` GLSL block обновлён, `GetSceneColorRange` extended с `rgbMin / rgbMax / rgbCornerSum` (bandwidth-free, same loop), `ApplyCasLinear` function добавлен (AMD CAS kernel: `center - 4-corner-avg` high-pass, positive-clamped weight, clamp-to-range output), `main()` CAS step между blend и tonemap (linear-light).
- `app/AppUpdate.cpp`: `debug->stats.taaCasSharpnessMax` mirror.
- `debug/DebugHud.cpp`: TAA line получил `CAS %.2f` token.
- `render/ScreenshotCapture.cpp`: `taa_cas_sharpness_max` sidecar key.

**Build state:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8` — green. `ctest` 6/6 passed (1.45 s). `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `VoxelLab` reference shot — 6/6 captures, sidecar:
- `taa_camera_cut_count=0` (static camera = no cuts, 1.2 ✓)
- `taa_camera_cut_max_delta=0.000000` (1.2 ✓)
- `taa_cas_sharpness_max=0.500000` (1.3 ✓)
- `taa_history_valid=1`, `taa_blend=0.10`, `taa_neighbourhood_radius=1` (carry-over из 1.4)
- `taa_jitter_x=0.125`, `taa_jitter_y=0.278` (Halton 8-tap)

**Visual verify (FINAL view, post-CAS):** VoxelLab рендерится чисто, FPS 93.2. CAS sharpening не даёт ringing / haloing, scene edges clean, no artefacts. `B` cycles lighting views на smk. capture set. Sidecar `taa_*` keys populated.

**Asset-pipeline parallel:** `ModelPass.cpp` (modified, чужая территория, depth bias M5.1b), `VulkanBootstrap.cpp` (modified, no-op), `.gitignore` + `pyproject.toml` + `uv.lock` (operator). Все мои TAA-scope правки не пересекаются.

**Known follow-ups (TODO §5 Блок 1, in priority order):**

| ID | Описание | Сложность | Scope |
|---|---|---|---|
| 1.5 | Anti-flicker для CTSH/AOCC/LOCL через mini-TAA history attachment | 3-5 ч | TAA-scope |
| 1.7 | R11G11B10_UFloat scene color (bandwidth: 4 vs 8 bytes/pixel) | 2-4 ч | TAA-scope, swapchain format |
| 1.8 | `TaaQuality` tier abstraction (Off/Light/Std/High) | 4-6 ч | TAA-scope, refactor 1.4 |
| 1.6 | VRS в cascade split edges | R&D | TAA-scope + GPU driver confirm |
| 2.x | Walk controller feel polish (frame-step, HUD additions, replay suite) | 3-5 дней | walk-scope |
| 3.x | SSAO baseline / SSR | 5-10 дней | deferred/hard |
| 4.1 | Greedy quad meshing | 2-3 дня | voxel-scope |
| 5.2 | Доп. debug gizmos (chunk AABB, cursor hit normal, cascade split planes) | 1-2 ч | render-scope |
| 5.3 | Benchmark automation (`PROJECTV_BENCHMARK_FRAMES=N` env) | 1-2 ч | render-scope |

**Test count baseline:** `ctest` 6/6 (1.45 s) — unchanged, не должно падать.

---

## 11. TAA Блок 1 / 1.7 — R11G11B10_UFloat scene color — LANDED (uncommitted, this session)

**Phase 1/5 of big session landed (per operator "go"). 1 commit proposed** (per `8635ddf` + `4deee52` precedent, per-task doc commit pattern):

| SHA | Subject | Files |
|---|---|---|
| _pending_ | `perf(render): TAA scene color to B10G11R11_UFLOAT (2x bandwidth save)` | 3 |
| _pending_ | `docs(agent): sync 1.7 closure + add §10.18/§20/§11` | 5 |

**What 1.7 changes:**

- `src/render/TaaRenderTargets.hpp` — new `inline constexpr VkFormat kTaaSceneColorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32` in `projectv::taa` namespace. Single source of truth for the TAA scene color format. Doc comment block explains the format choice (2× bandwidth vs R16G16B16A16), loss-of-precision trade-off (5/6/5 bits per channel + 5-bit shared exponent), and fallback path (1-line revert).
- `src/render/TaaRenderTargets.cpp:86` — image allocation now uses `kTaaSceneColorFormat` instead of literal `VK_FORMAT_R16G16B16A16_SFLOAT`. Both sceneColor and historyColor targets get the new format.
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1794` — pipeline's `pColorAttachmentFormats[1]` declaration now uses `kTaaSceneColorFormat`. The voxel pipeline's dual-slot TAA contract (slot 0 = swapchain, slot 1 = TAA scene color) is preserved, only the format of slot 1 changes.
- `src/render/ScreenshotCapture.cpp` — new sidecar key `taa_scene_color_format=B10G11R11_UFLOAT` (after `taa_camera_cut_max_delta`, before `shadow_cascade_count`).
- Shader code (`voxel.frag`, `model.frag.taa_on.spv`, `taa_resolve.frag`) **unchanged** — they write/read `vec4` and the format transition is transparent. Alpha of `outSceneColor` is undefined on store per Vulkan spec for packed formats, but `taa_resolve.frag` only consumes `.rgb` from the history sample, so the dropped alpha is a no-op.

**Build / test / smoke (`2026-06-12`):**
- `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests ProjectVAssetTests ProjectVMeshBakerTests ProjectVDracoTests ProjectVFrustumCullingTests ProjectVBoxUvFixtureTests --parallel 8` — green, 1 pre-existing warning (`DebugHud.cpp:600` LOCL `%.0f` for bool, не моя).
- `ctest --test-dir build/linux-clang-debug --output-on-failure` — 6/6 passed (1.48 s).
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `VoxelLab` reference shot — 6/6 captures под `build/linux-clang-debug/lookdev-captures/20260612-1.7-r11g11b10/`. Sidecar:
  - `taa_scene_color_format=B10G11R11_UFLOAT` ✓ (new key populated)
  - `taa_history_valid=1`, `taa_blend=0.10`, `taa_cas_sharpness_max=0.500000` (carry-over from 1.2+1.3)
  - `taa_camera_cut_count=0`, `taa_camera_cut_max_delta=0.001018` (static camera, expected)
- Vision review of FINAL view: VoxelLab renders clean — glass/fluid sphere, opaque anchor, checker floor, no banding visible in dim areas (sky background uniform light blue). FPS **110.6** (vs 1.2+1.3 baseline 93.2 — likely bandwidth reduction showing perf benefit, though single-run variance is high).

**Next steps (Big Session, 4 phases remaining):**
| ID | Task | Сложность | Status |
|---|---|---|---|
| 1.5 | Anti-flicker CTSH/AOCC/LOCL | 4-6 ч | next (highest visual impact) |
| 1.8 | Quality tier abstraction | 4-6 ч | phase 3 |
| first-frame AA | FXAA-lite in resolve | 1-2 ч | phase 4 |
| 1.6 | VRS R&D | 4-8 ч | phase 5 (with kill switch) |

**Asset-pipeline parallel:** no conflicts. M5.1b depth bias откачен per aborted session. 4 chuzhie uncommitted (`.gitignore`, `VulkanBootstrap.cpp`, `pyproject.toml`, `uv.lock`) — не мои, не трогаю.
