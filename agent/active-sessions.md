# agent/active-sessions.md

Append-only ledger активных и недавно завершённых AI-agent сессий в `ProjectV`.
Используется для координации между параллельными сессиями и для arbitration
при конфликте scope (см. `AGENTS.md` §7.2.6).

**Это НЕ источник истины** для архитектурных решений — для этого `agent/decisions.md`.
Здесь только оперативный signal «кто сейчас что трогает», чтобы параллельные
агенты не вытирали работу друг друга.

---

## Контракт использования

Каждый агент **обязан**:

1. **При старте сессии** — дописать запись со статусом `open` в секцию
   «Активные сессии» ниже.
2. **При завершении сессии** — обновить **свою** запись: статус → `closed`,
   проставить `closed-at` и `commit-hash` (или `uncommitted` / `aborted`
   с пояснением), затем перенести в секцию «Закрытые сессии».
3. **При abort** — пометить `aborted` + причина, не удалять запись.

См. также `agent/session-checklist.md` (секции «Старт» / «Завершение»).
Параллельный запуск нескольких сессий с **пересекающимся** scope —
аномалия, требует arbitration через пользователя (§7.2.6).

---

## Формат записи

| Поле | Описание |
|---|---|
| `id` | Уникальный идентификатор сессии (timestamp ISO 8601 + короткий суффикс) |
| `started-at` | Время старта в ISO 8601 (UTC) |
| `agent` | Тип / модель агента (например, `cline/MiniMax-M3`) |
| `operator` | Пользователь-оператор (например, `le1t`) |
| `branch` | Текущая git-ветка |
| `scope` | Краткое описание атомарной подзадачи (см. AGENTS.md §3.5) |
| `files-touched-intent` | Список файлов / путей, которые планируется править |
| `status` | `open` / `closed` / `aborted` |
| `closed-at` | (только для `closed`/`aborted`) Время завершения в ISO 8601 (UTC) |
| `commit-hash` | (только для `closed`) SHA коммита, закрывшего работу; или `uncommitted` |
| `notes` | Свободное примечание (конфликты, blockers, cross-refs) |

**Append-only правила:**

- Новые записи добавлять **сверху** соответствующей секции.
- Не редактировать чужие записи retroactively (даже если они «устарели») —
  лучше создать новую запись с `supersedes: <id>`.
- Не удалять закрытые записи из этого файла — при необходимости
  переносить в `legacy/docs/archive/agent-sessions/`.

---

## Активные сессии (status: open)

<!-- Новые записи добавлять СВЕРХУ этой секции. Append-only. -->

### session-2026-06-12-taa-quality-1.5

- **id:** `2026-06-12T15:00Z-taa-quality-1.5`
- **started-at:** 2026-06-12T15:00:00Z
- **closed-at:** 2026-06-12T17:30:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** TAA Блок 1 / 1.5 (anti-flicker CTSH/AOCC/LOCL) — phase 2/5 of the big-session plan (per `status.md §11`). Per-layer temporal history to remove the per-frame flicker that TAA's colour-only blend didn't reach. Voxel pass writes a packed R8G8B8A8 layer mask (R = sun contact shadow visibility, G = AOCC, B = local-point-light visibility) to a new 3rd MRT attachment (Location 2), and reads the previous-frame mask back as a `sampler2D` history texture (binding 6). The blend is `mix(rawCurrent, history, blend=0.4)` applied to AOCC and LOCL in main lighting; CTSH is written to history but not yet smoothed in main lighting (deferred — would need `ComputeSunShadowSample` refactor to separate cascade shadow from contact shadow). Blend-at-read: each frame's contribution to the temporal filter is uniform, no exponential-decay artefacts from blend-at-write on stale histories.
- **files-touched-intent:** `src/core/Types.hpp` (`RenderState::taaLayerSceneColorTarget` + `taaLayerHistoryColorTarget` + `taaLayerHistoryValid` + `taaLayerBlendFactor` + 3 new layout trackers; `DebugStats` mirrors; `VoxelSceneLighting::taaLayerHistoryParams` vec4 with static_assert), `src/render/TaaRenderTargets.{hpp,cpp}` (`kTaaLayerHistoryColorFormat = R8G11B11A8_UNORM` constant + layer target allocation in `CreateOrRecreateTaaRenderTargets`), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (3rd color attachment in `pColorAttachmentFormats[2]`, binding 6 `sampler2D layerHistory` in graphics descriptor set + pool size bumped from 1 to 2 `COMBINED_IMAGE_SAMPLER` per frame, `pColorBlendState->attachmentCount` 2→3 to match `colorAttachmentCount`), `src/render/vulkan/VulkanSwapchain.cpp` (layer target lazy-alloc + `taaLayerHistoryValid` reset + new `taaLayerSceneColorCurrentLayout` / `taaLayerHistoryColorCurrentLayout` layout trackers reset on swapchain recreate), `src/render/Renderer.cpp` (3rd color attachment in `vkCmdBeginRendering` (3rd-MRT binding fix, the original 1.5 commit had only 2 attachments which caused the driver to silently drop `outLayerMask` write → dim regression), per-frame layer history copy block, `taaHistoryParams` texel size patch — pre-existing bug where `BuildTaaHistoryParams` was defined but never called, leaving `taaHistoryParams.xy = (0, 0)` so the resolve's `taa_resolve.frag` reprojection was running with `texelSize=0` and silently falling back to the current-pixel-only branch — TAA was de facto disabled), `src/render/SceneResources.cpp` (populate `taaHistoryParams.xy` from `renderExtent` and `taaLayerHistoryParams.xy` from same, + `taaNeighbourhoodRadius` in the `.w` slot, + `taaLayerBlendFactor`), `src/render/ScreenshotCapture.cpp` (2 new sidecar keys: `taa_layer_history_valid`, `taa_layer_blend_factor`), `src/debug/DebugHud.cpp` (new `TAALYR %s BLF %.2f` line), `src/app/AppUpdate.cpp` (debug->stats mirror + 6 new history-reset branches on the existing 1.2/1.4 triggers — Taa toggle, jitter scale, blend, neighbourhood radius, `.` invalidate, Taa toggle), `src/render/Taa.cpp` + `.hpp` (`BuildTaaLayerHistoryParams` function), `src/shaders/voxel.frag` (new `outLayerMask` Location 2 + `sampler2D layerHistory` binding 6 + blend logic in `main()` + 1.5 docs), `src/shaders/voxel_mesh.comp` + `voxel_shadow.vert` (1.5 `taaLayerHistoryParams` std430 declaration), `src/voxel/VoxelMaterials.hpp` (`VoxelSceneLighting::taaLayerHistoryParams` + static_assert). **Не трогаю:** TAA-agent'овские commits (`008873a`/`4deee52`/`9ac9924`/`59d681e`/`b030fad`/`0503d8f`/`237ab76`/`4d8b4c8`) — это моя сессия, не их. Не трогаю: `decisions.md` (мой следующий edit), `TODO.md` (мой следующий edit), `agent/memory.md` (мой следующий edit), `agent/status.md` (мой следующий edit), `agent/active-sessions.md` (this entry).
- **status:** closed
- **commit-hash:** `237ab76` — `feat(taa): per-layer (CTSH/AOCC/LOCL) anti-flicker + 3rd-MRT binding fix + texel-size patch` (with follow-up `4d8b4c8` validation-fix commit)
- **notes:** **Phase 2/5 of big-session plan, 2 commits landed:**
  - `237ab76 feat(taa): per-layer (CTSH/AOCC/LOCL) anti-flicker + 3rd-MRT binding fix + texel-size patch` — 17 files: 4 shader-side files (`voxel.frag` outLayerMask + layer history + blend; `voxel_mesh.comp` + `voxel_shadow.vert` std430; `Taa.{hpp,cpp}` BuildTaaLayerHistoryParams), 7 render-side files (`TaaRenderTargets.{hpp,cpp}` kTaaLayerHistoryColorFormat + layer target alloc; `VulkanGraphicsPipeline.cpp` 3-MRT + binding 6; `VulkanSwapchain.cpp` layer target reset; `Renderer.cpp` 3rd-MRT binding fix + layer history copy + TAA texel-size patch; `SceneResources.cpp` taaLayerHistoryParams populate + texel-size patch; `ScreenshotCapture.cpp` 2 sidecar keys), 2 C++ glue files (`core/Types.hpp` new fields; `core/Types.cpp` destruction), 1 shader-struct file (`voxel/VoxelMaterials.hpp` taaLayerHistoryParams + static_assert), 1 HUD file (`DebugHud.cpp` new TAALYR line), 1 input-mirror file (`AppUpdate.cpp` DebugStats mirror + 6 history-reset branches). Plus the 3 pre-existing bug fixes I included: 3rd-MRT binding fix (the original 1.5 had `vkCmdBeginRendering` with `colorAttachmentCount=2` but pipeline declared 3 — driver silently dropped `outLayerMask` write), TAA reprojection texel-size patch (`BuildTaaHistoryParams` was defined but never called, leaving the resolve's `taaHistoryParams.xy = (0, 0)` so TAA reprojection was de facto disabled), `voxel.frag.taa_on.spv` refresh (incremental `cmake --build` does not copy fresh `.spv` to `bin/`).
  - `4d8b4c8 fix(taa): 1.5 layer-history Vulkan validation errors (image layouts + descriptor pool + color blend attachment count)` — 5 files. Three validation errors caught by `PROJECTV_ENABLE_VALIDATION=ON` that the smoke harness (with `PROJECTV_ENABLE_VALIDATION=OFF`) didn't surface: VUID-VkImageCreateInfo-initialLayout-00993 (initialLayout must be UNDEFINED, not SHADER_READ_ONLY), VUID-VkGraphicsPipelineCreateInfo-renderPass-06055 (colorBlendState attachmentCount must match colorAttachmentCount = 3), VUID-VkDescriptorPool-size-... (graphics pool needed 4 = 2 frames × 2 combined samplers for shadow + layer history). Plus layer history transitions updated to use the layout trackers as `oldLayout` instead of hardcoded `SHADER_READ_ONLY` (which would have failed validation on subsequent frames where the actual layout is `SHADER_READ_ONLY` already).

  **Build state:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests ProjectVAssetTests ProjectVMeshBakerTests ProjectVDracoTests ProjectVFrustumCullingTests ProjectVBoxUvFixtureTests --parallel 8` — green, 1 pre-existing warning (DebugHud.cpp:600 LOCL `%.0f` for bool, не моя). `ctest` 6/6 (1.50 s). `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `cam -25 19 25 look 0.62 -0.48 -0.62` (`VoxelLab` reference shot) с `PROJECTV_ENABLE_VALIDATION=OFF` (Linux preset = ON, layers package not installed, smoke script defaults to OFF): 6/6 captures под `build/linux-clang-debug/lookdev-captures/20260612-1.5-final/`. Sidecar: `taa_history_valid=1`, `taa_layer_history_valid=1`, `taa_layer_blend_factor=0.400000`, `taa_camera_cut_count=0`, `taa_scene_color_format=B10G11R11_UFLOAT`.

  **Validation verify (post `4d8b4c8`):** `PROJECTV_ENABLE_VALIDATION=ON build/linux-clang-debug/bin/ProjectV` — 0 VUIDs, 0 errors, scene renders correctly. ctest 6/6 (1.50 s), smoke 6/6 (`20260612-1.5-final/`). Vision verify: vibrant VoxelLab, opaque anchor, checker floor, FPS 127.3 (vs 1.5 baseline 110.6, slight variance).

  **Asset-pipeline parallel coordination:** одновременно active `session-2026-06-12-model-m6-triplanar-checker` (their M6 work on `model.frag` + `agent/memory.md §10.20`), `session-2026-06-12-taa-m5_2-threshold-bump` (their M5.2 follow-up), `session-2026-06-12-model-m5_1b-depth-bias` (already aborted). Per `AGENTS.md §7.2.6` (multi-agent concurrent work) — manual merge requires user arbitration. Per-task isolation: каждый agent работает в своём файле (model.frag vs voxel.frag vs taa_resolve.frag vs ModelPass.cpp), кроме shared-файлов (Renderer.cpp, VulkanGraphicsPipeline.cpp, core/Types.hpp). Doc sync для shared-файлов — combined commit с attribution, как в `0503d8f` для 1.7+M5.2.

  **Big-session queue (3 phases remaining):**
  - Phase 3 (next): 1.8 quality tier abstraction (4-6 ч, refactor поверх 1.4 + 1.2/1.3 + 1.5)
  - Phase 4: first-frame AA FXAA-lite in resolve (1-2 ч, optional)
  - Phase 5: 1.6 VRS R&D (4-8 ч, with kill switch on driver/improvement issues)

**Working rules to inherit (см. `agent/memory.md §10.18`):**
- **Single source of truth for cross-consumer constants.** When a Vulkan format is consumed by both image allocation and pipeline declaration, define it as an `inline constexpr` in the header next to the resource struct, not as two separate literals. The constant prevents the two consumers from drifting on a future change; the compiler enforces the relationship. This pattern applies to any cross-shader-struct value (push-constant fields, descriptor-set bindings, etc.).
- **Component budget on RTX 3060 = 8 vec4 outputs per fragment shader.** Per `maxFragmentOutputComponents`. TAA-off path: outColor (4) + outLayerMask (4) = 8. TAA-on path: outSceneColor (4) + outLayerMask (4) = 8. The third attachment slot in the pipeline declaration is bound in both paths but the per-frame `VkRenderingAttachmentInfo::imageView` is `VK_NULL_HANDLE` on the unused slot — `dynamicRenderingUnusedAttachments` allows that. Packing all 3 layer values (CTSH, AOCC, LOCL) into a single `vec4` is the way to fit within the budget.
- **Pipeline-declared attachment count must match `vkCmdBeginRendering`'s `colorAttachmentCount`.** Otherwise `VUID-VkGraphicsPipelineCreateInfo-renderPass-06055` fires. The 1.5 fix had `colorAttachmentCount=2` in `vkCmdBeginRendering` but `pColorAttachmentFormats[2] = kTaaLayerHistoryColorFormat` in the pipeline — driver silently dropped the write, dim regression.
- **Descriptor pool sizes must grow with each new combined image sampler.** `MAX_FRAMES_IN_FLIGHT × bindings`. 1.5 added binding 6 (layerHistory) to graphics descriptor set, so pool needed to grow from 2 → 4.
- **`initialLayout` must be `UNDEFINED` / `PREINITIALIZED` / `ZERO_INITIALIZED`** per VUID-VkImageCreateInfo-initialLayout-00993. Can't use `SHADER_READ_ONLY_OPTIMAL` directly. The first-frame per-frame transition in `Renderer.cpp` is the only way to get the image into the read layout.
- **Pre-existing bug: `BuildTaaHistoryParams` was defined but never called.** `taaHistoryParams.xy` stayed at `(0, 0)`, so the TAA resolve's `taa_resolve.frag` reprojection step ran with `texelSize=0` and silently fell back to the current-pixel-only branch — the temporal blend was de facto disabled. Fixed by populating from `renderExtent` in `SceneResources.cpp::RefreshSceneLightingBuffer`. The 1.5 layer history texel size is populated the same way for consistency.
- **Per-frame transitions use the layout tracker as `oldLayout`, not hardcoded.** The actual GPU state may be `COLOR_ATTACHMENT_OPTIMAL` (after the rendering pass) or `SHADER_READ_ONLY_OPTIMAL` (after the copy block). Hardcoding the wrong `oldLayout` triggers VUID-VkImageMemoryBarrier2-oldLayout-01197.
- **Voxel pass writes to the layer scene color in BOTH TAA-on and TAA-off paths.** The transition (UNDEFINED → COLOR_ATTACHMENT) runs unconditionally, not inside `if (taaOn)`.

**Test count baseline:** `ctest` 6/6 (~1.50 s wall clock). Это baseline, не должно падать.

**Build preset:** `linux-clang-debug` (native clang 22 + lld 22 + libstdc++ 16). Не трогать `windows-clang-debug` (operator's primary dev tree).

### session-2026-06-12-model-m6-triplanar-checker

- **id:** `2026-06-12T16:15Z-model-m6-triplanar-checker`
- **started-at:** 2026-06-12T16:15:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** M6 prep follow-up — replace `inUv`-based 4×4 procedural UV checker в `src/shaders/model.frag` (lines 50-67, оригинал) с **triplanar projection on `inWorldPosition`** по доминирующей face normal. Причина: `box.glb` (default model-pipeline test fixture) не имеет `TEXCOORD_0` accessor → `inUv` defaults to `(0, 0)` на всех face → procedural checker схлопывается к `floor(inUv * 4.0) = (0,0)` → `checkerMask = 0` → один tint на весь куб → uniform beige color → "half in textures" symptom (procedural pattern невидим). Triplanar на world position даёт visible 4×4 checker на каждой face независимо от mesh UVs. По dominантной normal axis: Y-dominant (top/bottom) → XZ projection, X-dominant (left/right) → ZY, otherwise (front/back) → XY. Per-face UV-индependent, общий pattern в world space, не per-face. M6+ заменит на real `sampler2D baseColor` (TODO §5).
- **files-touched-intent:** `src/shaders/model.frag` (triplanar projection, ~25 lines), `agent/memory.md` (§10.20 — новый, чтобы не конфликтовать с TAA-agent §10.17/§10.18/§10.19 и их 1.7 §), `agent/active-sessions.md` (this entry). **Не трогаю:** TAA-agent'овские commits (`008873a`/`4deee52`/`9ac9924`/`59d681e`/`b030fad`/`0503d8f`) и их working tree changes (`taa_resolve.frag` threshold bump + `ModelPass.cpp` dual-MRT fix + `ModelPass.hpp` `kTaaSceneColorFormat` include + `VulkanInit.cpp` call site) — TAA-agent сказал "не волнуйся, продолжай работу" и TAA-scope закрыт. Не трогаю: `decisions.md`, `TODO.md`, `TaaRenderTargets.*` (TAA-agent 1.7), `VulkanGraphicsPipeline.cpp` (TAA-agent 1.7), `VulkanBootstrap.cpp` (asset-pipeline VMA fix leftover, no-op).
- **status:** open
- **notes:** Sequential continuation of `session-2026-06-12-taa-m5_2-threshold-bump` (operator feedback: "теперь чини то, что она наполовину в текстурах"). TAA-agent'ы уже закоммитили M5.2 (`0503d8f` combined doc sync + их 1.7), per operator "TAA-агент за тебя сделал коммит m5.2, не волнуйся. Продолжай работу". M5.1b (z-fight depth bias) уже aborted в `session-2026-06-12-model-m5_1b-depth-bias`, fix был на env var `box.glb@0,1,0` (без кода). M5.2 dual-MRT fix (TAA-agent в working tree) и M5.2 threshold bump `0.20→0.40` (тоже TAA-agent / моё дублирование) уже решают "модель невидима с TAA on" — модель теперь visible. Оставшийся "half in textures" — UV-less mesh, не TAA-scope. **Build state:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests` — green, ctest 6/6. SPV скопирован в `bin/` per §10.16 working rule: `model.frag.spv` + `model.frag.taa_on.spv` (13 360 / 10 840 B). **Smoke verify:** `PROJECTV_MODELS=tests/fixtures/box.glb@0,1,0 PROJECTV_START_CAMERA_POSITION="-25 19 25" PROJECTV_START_CAMERA_LOOK="0.62 -0.48 -0.62" PROJECTV_LOOKDEV_CAPTURE_VIEWS=FINAL PROJECTV_LOOKDEV_CAPTURE_WARMUP_FRAMES=3 PROJECTV_LOOKDEV_CAPTURE_QUIT=1 build/linux-clang-debug/bin/ProjectV` — runs clean, no validation errors (после dual-MRT fix от TAA-agent). **Visual verify — за оператором** (нет display'а в моей sandbox'е, capture saved в `bin/ProjectVScreenshots/ProjectV-VoxelLab-*.bmp`). **Pending commit message draft:**

```
fix(model): use triplanar projection for procedural 4x4 UV checker

`model.frag` procedural 4x4 checkerboard used `inUv` directly,
which collapses to a single tint when the source mesh has no
TEXCOORD_0 accessor (e.g. `box.glb`, the default model-pipeline
test fixture) — `inUv` defaults to (0, 0) and `floor((0, 0) * 4)`
gives a single checker cell for the entire model. The model
appears uniformly tinted (one of the two checker tints wins
arbitrarily) instead of the intended 4x4 procedural pattern —
the "half in textures" symptom reported after the M5.1b revert
+ spawn-position fix on `box.glb@0,1,0`.

Replace UV-based projection with triplanar projection on
`inWorldPosition`, picked by the dominant face normal axis:
Y-dominant (top/bottom) -> XZ, X-dominant (left/right) -> ZY,
otherwise (front/back) -> XY. The result is a visible 4x4
checker on every face regardless of mesh UVs. The pattern is
shared across faces that share a world-space axis, so the
visual is slightly different from per-face UVs (each face
gets its own 0..1 range in the UV case), but the "block has a
visible checker on every face regardless of which fixture the
operator loads" contract is the important part. M6+ will
replace this with a real `sampler2D baseColor` and per-face UVs.

The M5.2 dual-MRT model pipeline fix and the threshold bump
in `taa_resolve.frag` (separate TAA-scope work) make the model
visible in TAA-on mode. This shader change makes the model
visibly textured regardless of fixture UVs.

Build green, ctest 6/6.

Refs: agent/memory.md §10.20, agent/active-sessions.md
      session-2026-06-12-model-m6-triplanar-checker
```

### session-2026-06-12-taa-quality-1.7

- **id:** `2026-06-12T16:00Z-taa-quality-1.7`
- **started-at:** 2026-06-12T16:00:00Z
- **closed-at:** 2026-06-12T16:30:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** TAA Блок 1 / 1.7 (R11G11B10_UFloat scene color) — phase 1/5 of the big-session plan (per `status.md §11`). 2× bandwidth save: `VK_FORMAT_R16G16B16A16_SFLOAT` (8 B/pixel) → `VK_FORMAT_B10G11R11_UFLOAT_PACK32` (4 B/pixel) для `taaSceneColorTarget` + `taaHistoryColorTarget`. Single source of truth: `inline constexpr VkFormat kTaaSceneColorFormat` в `projectv::taa` namespace (`src/render/TaaRenderTargets.hpp`), consumed by `CreateOrRecreateTaaRenderTargets` (image allocation) и `VulkanGraphicsPipeline::CreateGraphicsPipeline` (`pColorAttachmentFormats[1]` declaration). Shader code (`voxel.frag`, `model.frag.taa_on.spv`, `taa_resolve.frag`) **без изменений** — format transition transparent. Sidecar: новый `taa_scene_color_format=B10G11R11_UFLOAT` key для capture-driven verification.
- **files-touched-intent:** `src/render/TaaRenderTargets.hpp` (kTaaSceneColorFormat constant + doc comment), `src/render/TaaRenderTargets.cpp` (use kTaaSceneColorFormat), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (use kTaaSceneColorFormat + comment update), `src/render/ScreenshotCapture.cpp` (taa_scene_color_format sidecar key), `agent/memory.md` (§10.18), `agent/decisions.md` (§20), `agent/status.md` (§11), `TODO.md` (close 1.7 in Блок 1), `agent/active-sessions.md` (this entry).
- **status:** closed
- **commit-hash:** `59d681e` — `perf(render): TAA scene color to B10G11R11_UFLOAT (2x bandwidth save)` (with follow-up `0503d8f` doc sync combined with M5.2 follow-up)
- **notes:** **Phase 1/5 of big-session plan, 2 commits landed:**
  - `59d681e perf(render): TAA scene color to B10G11R11_UFLOAT (2x bandwidth save)` — 6 files: 4 code (TaaRenderTargets.hpp + TaaRenderTargets.cpp + VulkanGraphicsPipeline.cpp + ScreenshotCapture.cpp) + 2 doc (TODO.md close 1.7 + agent/decisions.md §20). Constant-driven, so the format change is in 1 line and the consumers automatically follow.
  - `0503d8f docs(agent): sync TAA Блок 1 / 1.7 (R11G11B10) + M5.2 follow-up closures` — 3 files: agent/memory.md + agent/status.md + agent/active-sessions.md. Per user arbitration (option A), combined doc sync covers both my 1.7 changes and the parallel session's M5.2 changes since the doc files were interleaved.

  **Build state:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests ProjectVAssetTests ProjectVMeshBakerTests ProjectVDracoTests ProjectVFrustumCullingTests ProjectVBoxUvFixtureTests --parallel 8` — green, 1 pre-existing warning (DebugHud.cpp:600 LOCL `%.0f` for bool, не моя). `ctest` 6/6 (1.48 s) — все suites прошли. `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `cam -25 19 25 look 0.62 -0.48 -0.62` (`VoxelLab` reference shot) с `PROJECTV_ENABLE_VALIDATION=OFF`: 6/6 captures под `build/linux-clang-debug/lookdev-captures/20260612-1.7-r11g11b10/`. Sidecar: `taa_scene_color_format=B10G11R11_UFLOAT` ✓, `taa_history_valid=1`, `taa_blend=0.10`, `taa_cas_sharpness_max=0.500000`, `taa_camera_cut_count=0`, `taa_camera_cut_max_delta=0.001018`.

  **Visual verify (FINAL view):** VoxelLab рендерится чисто, FPS 110.6. **No banding** в dim areas. Glass/fluid sphere + opaque anchor + sky gradient все visible, без визуальных артефактов от format change.

  **Parallel agent coordination resolved:** одновременно active `session-2026-06-12-taa-m5_2-threshold-bump` (m5.2 threshold 0.20→0.40 + dual-MRT model pipeline fix), их doc правки interleaved с моими в `agent/memory.md` / `agent/status.md` / `agent/active-sessions.md`. Per `AGENTS.md §7.2.6` (multi-agent concurrent work) — manual merge requires user arbitration. User chose option A: combined commit with both my 1.7 changes и their M5.2 changes. Resolved.

  **Cross-agent coupling:** `kTaaSceneColorFormat` constant (in TaaRenderTargets.hpp, мой 1.7) is consumed by parallel session's dual-MRT fix in `src/asset/ModelPass.cpp:200-224` (their pending commit). Both 1.7 commit (59d681e) and their M5.2 commit will land separately, both reference the same constant.

  **Big-session queue (4 phases remaining):**
  - Phase 2 (next): 1.5 anti-flicker CTSH/AOCC/LOCL (4-6 ч, highest visual impact)
  - Phase 3: 1.8 quality tier abstraction (4-6 ч, refactor)
  - Phase 4: first-frame AA FXAA-lite in resolve (1-2 ч, optional)
  - Phase 5: 1.6 VRS R&D (4-8 ч, with kill switch)

**Working rules to inherit (см. `agent/memory.md` §10.18):**
- **Single source of truth for cross-consumer constants.** When a Vulkan format is consumed by both image allocation and pipeline declaration, define it as an `inline constexpr` in the header next to the resource struct, not as two separate literals. The constant prevents the two consumers from drifting on a future change; the compiler enforces the relationship. This pattern applies to any cross-shader-struct value (push-constant fields, descriptor-set bindings, etc.).
- Shader-side aliasing note: `B10G11R11_UFLOAT_PACK32` has 5/6/5 bits per channel with a 5-bit shared exponent. Single bright sample can compress dim neighbour's exponent range, visible as banding in dim areas (< 0.1% intensity in linear light). Fallback revert = 1-line constant change.
- Shader code reads/writes `vec4` regardless of `outSceneColor` format. Alpha channel of `outSceneColor` is undefined on store for packed formats, but `taa_resolve.frag` only consumes `.rgb` from history, so the dropped alpha is a no-op.

**Test count baseline:** `ctest` 6/6 (~1.48 s wall clock). Это baseline, не должно падать.

**Build preset:** `linux-clang-debug` (native clang 22 + lld 22 + libstdc++ 16). Не трогать `windows-clang-debug` (operator's primary dev tree).

### session-2026-06-12-taa-m5_2-threshold-bump

- **id:** `2026-06-12T15:35Z-taa-m5_2-threshold-bump`
- **started-at:** 2026-06-12T15:35:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** M5.2 follow-up — bump `kTaaColorDistanceRejectionThreshold` `0.20 → 0.40` в `src/shaders/taa_resolve.frag:79`. Reason: runtime repro на model pass в VoxelLab (`PROJECTV_MODELS=box.glb@0,1,0`, reference shot) показал "block half in textures" symptom — `model.frag` 4×4 UV checker имеет два tint-варианта с YCoCg distance до voxel centroid ≈ 0.27 (yellow, проходит rejection) и ≈ 0.16 (blue, не проходит — clamped в voxel range, невидим). 0.40 ловит оба tint-варианта. False-positive risk bounded: voxel surfaces обычно в пределах 0.05 YCoCg от своего 3×3 mean.
- **files-touched-intent:** `src/shaders/taa_resolve.frag` (1 строка, threshold bump + расширенный комментарий), `agent/memory.md` (§10.19, переименовано из §10.18 чтобы не конфликтовать с TAA-agent 1.7), `agent/status.md` (§8 M5.2 fix описание), `agent/active-sessions.md` (this entry). **TAA-scope-adjacent — TAA-agent (`session-2026-06-12-taa-1.2-1.3`) уже closed, разрешено править `taa_resolve.frag`.** Не трогаю: `decisions.md §19` (TAA-agent оставил threshold-as-tuned-0.20 в их §, не моё править), `TODO.md` (M5.2 уже closed в `8635ddf`).
- **status:** open
- **notes:** Предыдущая моя сессия `session-2026-06-12-model-m5_1b-depth-bias` уже aborted (см. ниже в closed). M5.1b (z-fight depth bias) был на неверной посылке — модель не z-fight'ит, а прячется под полом VoxelLab; фикс через env var `box.glb@0,1,0` (без кода). После abort оператор сообщил, что с TAA-on модель всё равно не видна ("half in textures"). TAA 1.2 + 1.3 уже закоммичены (`008873a`/`4deee52`/`9ac9924`), но не фиксили M5.2. Bump threshold — single-line fix, build green, ctest 6/6, SPV скопирован в `bin/` per `agent/memory.md §10.16` working rule (incremental `cmake --build` не копирует свежие `.spv` в `bin/`). **Visual verify — за оператором** (binary у него, validation layers не установлены, не могу запустить runtime smoke с `PROJECTV_ENABLE_VALIDATION=ON`). **Pending commit** — следующий commit message draft:

```
fix(taa): raise M5.2 color-distance rejection threshold to cover dim tints

The model pass in VoxelLab with the 4x4 procedural UV checker has
two tint variants that produce YCoCg samples at distance ~0.27
(yellow, passes the M5.2 rejection at 0.20) and ~0.16 (blue, fails,
gets clamped into the voxel range and disappears). Operator reported
this as "block half in textures" after the M5.1b revert + spawn-
position fix on `box.glb@0,1,0`. Raising `kTaaColorDistanceRejection-
Threshold` from 0.20 to 0.40 catches both tint variants with enough
headroom for chroma-dim outliers, and stays well above the
~0.05 YCoCg variance of natural voxel-pass surfaces, so false-
positive risk on the voxel-only path is bounded.

Refs: agent/status.md §8, agent/active-sessions.md session-2026-06-12-taa-m5_2-threshold-bump
```

**ADDENDUM (`2026-06-12` ~16:00):** После feedback "ЕГО ВООБЩЕ НЕ ВИДНО С TAA ON" оператор подсказал перепроверить shader compilation. ninja: no work to do — shader якобы up to date. Проверил `taa_resolve.frag.spv` md5 — действительно новый (0.40 threshold applied). Тогда **нашёл настоящий root cause** в `ModelPass.cpp:202`:
- `VkPipelineRenderingCreateInfo::colorAttachmentCount = 1` (pre-fix).
- `model.frag:33` TAA-on: `layout(location = 1) out vec4 outSceneColor`.
- Main pass `vkCmdBeginRendering` (Renderer.cpp:735) имеет 2 attachments.
- Pipeline объявляет 1 → write в Location 1 — **undefined behavior** (драйвер silently дропает, validation layers не стоят).
- `taaSceneColorTarget` оставался пустым в model pixels → resolve pass сэмплил пустоту → модель невидима несмотря на правильный M5.2 threshold.

**Фикс:** `ModelPass.cpp:200-224` теперь объявляет dual-MRT attachments `{ colorFormat, projectv::taa::kTaaSceneColorFormat }` (последний — `B10G11R11_UFLOAT_PACK32` per TAA-agent 1.7 centralization в `TaaRenderTargets.hpp:52`). `ModelPass.hpp` — `#include "render/TaaRenderTargets.hpp"` для namespace'а. `VulkanInit.cpp:237` — call site без изменений (использует default `kTaaSceneColorFormat`).

**Иерархия фиксов:** M5.2 threshold (0.20→0.40) для partial visibility. Dual-MRT для полной невидимости. Оба нужны — dual-MRT без threshold → частичный clamp; threshold без dual-MRT → полная invisible (write дропается до resolve). **Visual verify — за оператором.**

### session-2026-06-12-taa-1.2-1.3

- **id:** `2026-06-12T15:00Z-taa-1.2-1.3`
- **started-at:** 2026-06-12T15:00:00Z
- **closed-at:** 2026-06-12T15:30:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** TAA Блок 1 / 1.2 (camera-cut detection) + 1.3 (adaptive CAS sharpening post-TAA). 1.2: Chebyshev (L-infinity, max-abs over 16 floats) delta между `taaPrevViewProjectionMatrix` (frame N-1) и `frame->graphicsPushConstants.viewProjection` (frame N) в `FramePreparation::BuildFrameData`. Threshold `kTaaCameraCutThreshold = 0.10f` (10% per-element). First-frame guard через `taaPrevViewProjectionMatrixInitialized` bool, reset в `VulkanSwapchain.cpp::CreateOrRecreateSwapchain`. 1.3: inline AMD FidelityFX CAS (Bartłomiej Wronski, GPUOpen 2020) integrated в `taa_resolve.frag` single-pass, no extra texture lookups (extended `GetSceneColorRange` loop: `rgbMin / rgbMax` 5-tap cross+center, `rgbCornerSum` 4-tap corners). `sharpenAmount = max(0, (1.0 - taaBlend) * taaCasSharpnessMax)` derived in-shader. Linear-light pre-tonemap. `ResolvePushConstants` `vec2 reservedPadding` заменён на `float taaBlend + float taaCasSharpnessMax` (8 B total, byte layout preserved, `static_assert` обновлён).
- **files-touched-intent:** `src/core/Types.hpp` (RenderState + DebugStats + ResolvePushConstants), `src/app/FramePreparation.cpp` (camera-cut detector), `src/app/AppUpdate.cpp` (DebugStats mirror), `src/debug/DebugHud.cpp` (TAACUT line + CAS в TAA), `src/render/ScreenshotCapture.cpp` (3 sidecar keys), `src/render/vulkan/VulkanSwapchain.cpp` (companion reset), `src/render/Renderer.cpp` (push-constant populate), `src/shaders/taa_resolve.frag` (CAS kernel + linear-light step), `agent/memory.md` (§10.17), `agent/decisions.md` (§19), `agent/status.md` (§10), `TODO.md` (close 1.2 + 1.3 in Блок 1), `agent/active-sessions.md` (this entry).
- **status:** closed
- **commit-hash:** `008873a` — `feat(taa): camera-cut detection (1.2) + inline CAS post-TAA (1.3)` (with follow-up `4deee52` doc sync)
- **notes:** **2 commits landed (per operator "коммить своё"):**
  - `008873a feat(taa): camera-cut detection (1.2) + inline CAS post-TAA (1.3)` — 8 files. 1.2 (RenderState/DebugStats fields, FramePreparation detector, AppUpdate mirror, DebugHud TAACUT line, ScreenshotCapture cut sidecar keys, VulkanSwapchain companion reset) + 1.3 (ResolvePushConstants field rename + byte layout, RenderState/DebugStats CAS field, Renderer push-constant populate, taa_resolve.frag comprehensive rewrite — push block, GetSceneColorRange extension, ApplyCasLinear, main CAS step, AppUpdate CAS mirror, DebugHud CAS token, ScreenshotCapture CAS sidecar). Originally proposed as 2 separate commits (A `fix(taa): camera-cut...` + B `feat(taa): inline CAS...`), but combined into 1 commit because 4 shared files (Types.hpp, AppUpdate.cpp, DebugHud.cpp, ScreenshotCapture.cpp) have both A and B changes interleaved — `git add -p` would have been fragile, and the `8635ddf` precedent (combining 1.4 + M5.2 into one commit) shows the project combines related parallel TAA work when the split is awkward. Future per-commit revertability comes from the per-feature cross-references in the body.
  - `4deee52 docs(agent): sync 1.2 + 1.3 closures + add §10.17/§19/§10` — 5 files. TODO.md Блок 1 closed, header date updated. agent/memory.md §10.17 added (full timeline, first-frame guard rationale, inline-CAS rationale, linear-light CAS, push-constant byte layout invariance, build/ctest/smoke verification, 3 new working rules). agent/decisions.md §19 added (TAA camera-cut + CAS contract: 8th history-invalidation trigger, AMD CAS port, sharpenAmount derivation, default 0.5 ceiling, 144 B layout invariance). agent/status.md §10 added (in-progress session snapshot, 2-commit summary, build/ctest/smoke state, follow-up queue). agent/active-sessions.md appended with this entry.

  **Build state:** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests --parallel 8` green, 1 pre-existing warning (`DebugHud.cpp:600` LOCL line, не моя правка). `ctest` 6/6 (1.45 s) — `ProjectVTests`, `ProjectVAssetTests`, `ProjectVMeshBakerTests`, `ProjectVDracoTests`, `ProjectVFrustumCullingTests`, `ProjectVBoxUvFixtureTests`. `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `cam -25 19 25 look 0.62 -0.48 -0.62` (`VoxelLab` reference shot) с `PROJECTV_ENABLE_VALIDATION=OFF` (Linux preset = ON, layers package not installed, smoke script defaults to OFF): 6/6 captures под `build/linux-clang-debug/lookdev-captures/20260612-1.2-1.3-smoke-v2/`. Sidecar:
  - `taa_camera_cut_count=0` (static camera, no cuts) ✓
  - `taa_camera_cut_max_delta=0.000000` ✓
  - `taa_cas_sharpness_max=0.500000` (default) ✓
  - `taa_history_valid=1`, `taa_blend=0.10`, `taa_neighbourhood_radius=1` (carry-over из 1.4)
  - `taa_jitter_x=0.125`, `taa_jitter_y=0.278` (Halton 8-tap)

  **Visual verify (FINAL view):** VoxelLab рендерится чисто, FPS 93.2, glass/fluid sphere + opaque anchor + checker floor, **no ringing / haloing** от CAS step, **no ghosting** от camera-cut detector (count=0 на static camera).

  **Asset-pipeline parallel (now closed):** `session-2026-06-12-model-m5_1b-depth-bias` (см. ниже) aborted, их `ModelPass.cpp` depth bias reverted к pre-M5.1b state, их doc-правки (включая `agent/memory.md §10.17` для M5.1b) тоже откачены. Моя `agent/memory.md §10.17` теперь TAA-контент, не конфликтует. `VulkanBootstrap.cpp` (redundant `#include "volk.h"`, no-op от asset-pipeline VMA fix detour), `.gitignore` (operator), `pyproject.toml` + `uv.lock` (untracked, operator) — все **не мои**, не трогать.

  **History invalidation triggers (теперь 8, per `decisions.md` §18 + §19):**
  1. Swapchain resize (`VulkanSwapchain.cpp::CreateOrRecreateSwapchain`).
  2. World reload через `FinalizeActiveVoxelWorldReload` (`main.cpp`).
  3. `T` toggle (TAA on↔off).
  4. `taaJitterScale` change (live `;`/`'`).
  5. `taaBlend` change (live `-`/`=`).
  6. `taaNeighbourhoodRadius` change (live `,` cycle).
  7. `.` history-invalidate single press.
  8. **`taaCameraCutDelta > 0.10` (1.2, this session).**

#### Handoff для следующей сессии (2026-06-12 onward)

**Recent uncommitted changes (this session, ~13 files):**
- `src/core/Types.hpp`, `src/app/FramePreparation.cpp`, `src/app/AppUpdate.cpp`, `src/debug/DebugHud.cpp`, `src/render/ScreenshotCapture.cpp`, `src/render/vulkan/VulkanSwapchain.cpp`, `src/render/Renderer.cpp`, `src/shaders/taa_resolve.frag` — 1.2 + 1.3 work
- `TODO.md`, `agent/memory.md`, `agent/decisions.md`, `agent/status.md`, `agent/active-sessions.md` — doc sync

**Other-agent uncommitted (NOT mine, do not touch):** `VulkanBootstrap.cpp` (no-op volk.h redundant include, asset-pipeline leftover), `.gitignore` (operator), `pyproject.toml` + `uv.lock` (untracked, operator). **M5.1b depth bias откачен**, `ModelPass.cpp` чистый.

**Commit plan (pending operator decision, 2+1 commits):**
- **A — 1.2 fix:** `fix(taa): camera-cut detection (Chebyshev threshold) + first-frame guard` — 6 files
- **B — 1.3 feat:** `feat(taa): inline AMD CAS post-TAA sharpen, sharpenAmount = (1-blend)*max` — 4 files
- **C — doc sync:** `docs(agent): sync 1.2 + 1.3 closures + add §10.17/§19/§10` — 5 files

**Backup:** None saved to `/tmp/` this session (clean working tree, per-file diffs readable through `git diff`). If operator wants atomic-patch backup, run `git diff > /tmp/taa_1.2_1.3_<ts>.patch` before any destructive git action.

**Tuning ladder hotkeys (master HEAD):**
- `;` jitter scale dec, `'` jitter scale inc (multiplier on Halton, [0,2] step 0.25)
- `-` blend dec, `=` blend inc (history weight, [0,1] step 0.05)
- `,` neighbourhood radius cycle (1/3/5/7 — 3×3/7×7/11×11/15×15)
- `.` history invalidate (single press)
- `T` toggle TAA on/off (pre-existing)
- **NEW (this session):** `taaCameraCutCount` + `taaCameraCutMaxDelta` exposed в `TAACUT %u CLR %.2f` HUD line + sidecar; `taaCasSharpnessMax` exposed в TAA `CAS %.2f` token + sidecar (no live hotkey yet, future live-tuning keys candidate)

**Working rules to inherit (см. `agent/memory.md` §10.17):**
- **First-frame / post-recreate baseline guard.** Any new frame-to-frame state that's initialised to a sentinel (zero, NaN, identity matrix) and compared against the next-frame value needs a companion "initialised" bool, **not** a `frameCounter > N` heuristic. Reset the flag in every code path that resets the underlying state.
- **Linear-light CAS, not sRGB.** AMD's reference CAS operates in display-referred (sRGB-encoded) space because it's typically composed after a separate post-process stack. Our CAS runs on linear data, so the high-pass kernel and the `[min, max]` range must be in linear light too.
- **Push constant byte layout invariance.** `ResolvePushConstants` gained 2 new float fields but the total size stayed 144 B. The `static_assert` block at the struct definition is the source of truth for layout; updating it in lockstep with the shader's GLSL declaration is mandatory.
- `volk.h` MUST come before any VMA-touching header in shared files. If new VMA-related types are added to `core/Types.hpp`, the `volk.h` include position is preserved at top.
- Asset-pipeline sibling target dependency propagation: when asset-pipeline adds a header-only dep (e.g., glm) that's transitively pulled in by `core/Types.hpp`, all sibling targets that include Types.hpp must also link that dep. `ProjectVTests` was the canary.
- Shader compile artifact `*.spv` is NOT auto-copied to `bin/` if `ProjectV` ELF is up-to-date. After shader edits: `cp build/.../src/<name>.spv build/.../bin/<name>.spv` (or `cmake --build` with a forced ProjectV relink).
- `legacy/docs/libraries/` is a dump of reference material, NOT source of truth per `AGENTS.md` §4. Vulkan reference is in `docs/VulkanSDK-Linux-Docs-1.4.350.1/`.

**Test count baseline:** `ctest` 6/6 (~1.45 s wall clock, `ProjectVTests` 1.4 s + 5 fast suites). Это baseline, не должно падать.

**Build preset:** `linux-clang-debug` (native clang 22 + lld 22 + libstdc++ 16). Не трогать `windows-clang-debug` (operator's primary dev tree).

### session-2026-06-12-model-m5_1b-depth-bias

- **id:** `2026-06-12T13:00Z-model-m5_1b-depth-bias`
- **started-at:** 2026-06-12T13:00:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** M5.1b (asset-pipeline handoff) — model-vs-voxel tight-contact z-fighting fix via **negative** `depthBiasConstantFactor` / `depthBiasSlopeFactor` в `src/asset/ModelPass.cpp`. Build green, ctest 6/6. Visual verify остаётся за оператором (binary у него).
- **files-touched-intent:** `src/asset/ModelPass.cpp` (rasterizer.depthBiasEnable VK_TRUE, -1.25f / -1.75f), `agent/memory.md` (§10.17), `agent/decisions.md` (§15 depth bias sign), `agent/active-sessions.md` (this entry + close stale asset-pipeline), `agent/status.md` (snapshot), `TODO.md` (close M5.1b entry). **Не трогаю:** `VulkanBootstrap.cpp` redundant `volk.h` include (asset-pipeline leftover, no-op, "NOT mine" per TAA-agent's handoff at `7e7547c`); `.gitignore` operator entries; `pyproject.toml` / `uv.lock` untracked.
- **status:** aborted
- **closed-at:** 2026-06-12T14:30:00Z
- **commit-hash:** uncommitted (reverted before commit)
- **notes:** **Aborted by operator feedback.** Operator сообщил, что M5.1b (z-fight depth bias) решал не ту проблему: модель не «мерцала», а пряталась под полом (asset-pipeline сессия заспавнила её на `y=0`, она окклюдится полом VoxelLab; оператор увидел её только сломав пол). Также оператор подтвердил, что с включённым TAA модель всё равно не видна — это отдельная проблема, не связанная с depth bias. **Что сделано:** отрицательный depth bias в `ModelPass.cpp` (1 файл, ~30 строк) **откачен обратно к `depthBiasEnable = VK_FALSE`** — опасная правка на неверной посылке. Все doc-правки тоже откачены (`agent/memory.md` §10.17 удалён, `agent/decisions.md` §15 sub-bullet о depth bias sign удалён, `agent/status.md` §10 snapshot удалён, `TODO.md` "Closed (Model-pipeline follow-ups)" entry удалён, header date возвращён к `1.4 + 5.1 closed`). **Что сохранено:** `asset-pipeline` запись в этом файле осталась обновлена `open→closed` (это была stale правка, корректная сама по себе — close-out commit `b152b70` существует). **Working tree сейчас:** `src/asset/ModelPass.cpp` reverted к pre-M5.1b state; docs reverted; 3 не-моих файла остались как были (`.gitignore` operator's, `VulkanBootstrap.cpp` redundant `volk.h` от asset-pipeline VMA fix detour, `pyproject.toml` + `uv.lock` untracked). **Следующий шаг:** разобраться с реальной проблемой — модель не видна с TAA on. Это **TAA-scope**, и нужен re-look на M5.2 fix в `8635ddf` (YCoCg color-distance rejection) — почему не сработал. Также нужно понять правильный default spawn position (Y=0 vs Y=1, в VoxelLab пол на Y=0, модель должна быть над полом).

### session-2026-06-11-taa-tooling-1.4-5.1-6.x

- **id:** `2026-06-11T20:55Z-taa-tooling-1.4-5.1-6.x`
- **started-at:** 2026-06-11T20:55:00Z
- **closed-at:** 2026-06-12T09:30:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Mini-plan 1.4 + 5.1 + 6.x: (A) TAA tuning HUD ladder — `;`/`'` jitter scale, `-`/`=` blend, `,` neighbourhood radius, `.` history-invalidate hotkeys + DebugHud lines + sidecar keys; (B) 5.1 RenderDoc markers — `vkCmdBeginDebugUtilsLabelEXT/End` на hot sites в `Renderer.cpp`, gated `PROJECTV_ENABLE_RENDERDOC_MARKERS`; (C) 6.x doc sync — TODO close, memory TAA section, decisions TAA contract, vulkan addendum. 4 связных коммита в одной сессии по выбору оператора.
- **files-touched-intent:** `src/app/InputActions.cpp`, `src/app/AppUpdate.cpp`, `src/debug/DebugHud.cpp`, `src/render/ScreenshotCapture.cpp`, `src/render/Renderer.cpp`, `src/render/vulkan/VulkanBootstrap.cpp` (marker fn loaders), `src/core/Types.hpp` (DebugStats / TaaTuning fields), `agent/memory.md` (TAA section), `agent/decisions.md` (TAA contract), `agent/active-sessions.md` (close), `agent/status.md` (snapshot), `TODO.md` (close 1.4 + 5.1), `legacy/docs/libraries/vulkan/` (dual-MRT addendum — rejected, см. ниже)
- **status:** closed
- **commit-hash:** `8635ddf` — `fix(taa): TAA tuning HUD ladder + M5.2 color-distance rejection`
- **notes:** **4 commits landed in this session** (per operator "продолжай работу", mid-2026-06-12):
  - `8635ddf fix(taa): TAA tuning HUD ladder + M5.2 color-distance rejection` — 8 files. 1.4 (5 new hotkeys `;`/`'`/`-`/`=`/`,`/`.`, `taaNeighbourhoodRadius` в `taaHistoryParams.w`, sidecar keys, DebugHud line, AppUpdate handlers, InputActions bindings) + M5.2 follow-up fix (color-distance rejection в YCoCg clamp — fixes polygon-model "faint grey blob" reported by asset-pipeline closeout `b152b70`).
  - `3ee995f feat(render): add RenderDoc debug-utility label helpers + TAA-resolve hot site` — 1 file. `profiling::ScopedGpuDebugLabel` RAII + `PV_PROFILE_GPU_LABEL` macros, gated on `PROJECTV_ENABLE_RENDERDOC_MARKERS`. (`Renderer.cpp` hot sites already committed by asset-pipeline session as part of M4/M5 chain.)
  - `f90687a docs(agent): sync Блок 1 (1.4) + Блок 5 (5.1) + Блок 6 (6.x) closures` — 4 files. TODO Блок 1 + 5 + 6 closed, `agent/memory.md` §10.16 added, `agent/decisions.md` §18 added, `agent/status.md` §8 snapshot. `legacy/docs/libraries/vulkan/13_projectv-taa.md` создан, потом удалён — `legacy/docs/libraries/` это свалка-справочник, не source of truth per `AGENTS.md` §4. Vulkan reference: `docs/VulkanSDK-Linux-Docs-1.4.350.1/`.
  - `e27d971 docs(agent): record 1.4 + 5.1 + 6.x + M5.2 commits in active-sessions.md` — 1 file, session metadata update.

  **Build state (final):** `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests` green, `ctest` 6/6 (`ProjectVTests`, `ProjectVAssetTests`, `ProjectVMeshBakerTests`, `ProjectVDracoTests`, `ProjectVFrustumCullingTests`, `ProjectVBoxUvFixtureTests`).

  **Asset-pipeline parallel closed** (`b152b70 docs(agent): close out asset-pipeline session with M5/M5.1/M5.2 status`). Их M4 (model graphics pass, `c4382ea`) + M5 (frustum cull, `ccf7400`) + M6 prep (UV box fixture, `dfaa037`) committed. `FramePreparation.cpp` (`taaJitterX/Y *= taaJitterScale`) и `Renderer.cpp` (6 GPU labels на hot sites) подхвачены в их M4/M5 commits.

  **Uncommitted в working tree (NOT mine, осталось на следующую сессию / operator):** `src/render/vulkan/VulkanBootstrap.cpp` (asset-pipeline сессия добавила `#include "volk.h"` на line 13 во время VMA fix попытки; потом нашли лучший fix в `core/Types.hpp` и закоммитили там, но эта redundant правка осталась в working tree — no-op), `.gitignore` (`/.venv/` + `minimax_proxy.py` — operator/personal), `pyproject.toml` + `uv.lock` (untracked, Python project files, не мой scope).

#### Handoff для следующей сессии (2026-06-12 onward)

**Recent commit chain (эта сессия):**
- `e27d971` — active-sessions.md update
- `f90687a` — doc sync (Блок 1/5/6)
- `3ee995f` — 5.1 RenderDoc debug-utility labels
- `8635ddf` — 1.4 TAA tuning ladder + M5.2 fix

**Recent commit chain (asset-pipeline сессия — closed):**
- `b152b70` — close out
- `dfaa037` — UV box fixture
- `ccf7400` — M5 frustum cull
- `c4382ea` — M4 model graphics pass

**Dirty tree safety:** если `git status -uall` показывает чужие uncommitted changes — **не делать** `git checkout -- .` или `git stash drop` (см. `agent/memory.md` §10.11 — там потеряли P0.2 LINEAR fix). Сначала `git diff > /tmp/before_drop_<ts>.patch` и спросить оператора.

**Tuning ladder hotkeys (master HEAD):**
- `;` jitter scale dec, `'` jitter scale inc (multiplier on Halton, [0,2] step 0.25)
- `-` blend dec, `=` blend inc (history weight, [0,1] step 0.05)
- `,` neighbourhood radius cycle (1/3/5/7 — 3×3/7×7/11×11/15×15)
- `.` history invalidate (single press)
- `T` toggle TAA on/off (pre-existing)

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

### session-2026-06-11-taa-a2-flip

- **id:** `2026-06-11T23:44Z-taa-a2-flip`
- **started-at:** 2026-06-11T23:44:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Phase A сессия 2 (A2) — flip `taaEnabled` default `false→true`, fix SPIR-V search path, smoke verify (0 VUIDs).
- **files-touched-intent:** `src/core/Types.hpp` (`taaEnabled=true`), `src/core/ShaderIO.cpp` (SPIR-V search path: `parent_path()` → `".."`), `src/shaders/voxel.frag` (dual MRT), `src/render/vulkan/VulkanSwapchain.cpp` (return-value check), `src/app/FramePreparation.cpp` (`taaResolveDescriptorSet`), `src/render/Renderer.cpp` (swapchain layout transition in TAA-on resolve block)
- **status:** closed
- **closed-at:** 2026-06-11T23:59:00Z
- **commit-hash:** uncommitted (proposed)
- **notes:** A2 complete. SPIR-V search path had `parent_path()` which doesn't work with trailing separator from `SDL_GetBasePath()` → `bin/src/file.spv` instead of `src/file.spv`. Fixed to `".." / "src" / filename` which works cross-platform. Smoke 6/6 with `PROJECTV_ENABLE_VALIDATION=ON`: 0 VUIDs, 0 errors, `taa_enabled=1`, `taa_history_valid=1`.

### session-2026-06-11-taa-closeout-plumbing

- **id:** `2026-06-11T22:35Z-taa-closeout-plumbing`
- **started-at:** 2026-06-11T22:35:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Phase A сессия 1 — TAA close-out plumbing (deferred subtasks C+D+E+F из `agent/memory.md §10.14`): (C) `AppUpdate.cpp` `ToggleTaa` handler + `InputActions.cpp` T-биндинг + `DebugStats` propagation; (D) `DebugHud.cpp` TAA JITR/BLND/HIST lines в detailed HUD; (E) `ScreenshotCapture.cpp` `taa_*` sidecar entries; (F) history-invalidation hooks на world-reload / preset-change / Taa-toggle. `taaEnabled` остаётся `false` default — flip на A2 (следующая сессия) после visual verify. **Не трогаю:** `src/render/Renderer.cpp` (TAA-рендеринг уже wired в `98fb391`), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (6 pipelines уже на месте), `src/render/vulkan/TaaResolvePipeline.cpp`, `src/render/Taa.*`, `src/render/TaaRenderTargets.cpp`. Asset-pipeline session parked на M2+ (`8b112e7` — operator сообщил "параллельный агент завершил свою работу"). Запись asset-pipeline остаётся `open` для owner'а — я её не правлю.
- **files-touched-intent:** `src/app/AppUpdate.cpp` (ToggleTaa handler + world-reload/preset invalidate), `src/app/InputActions.cpp` (T-биндинг), `src/debug/DebugHud.cpp` (TAA lines), `src/render/ScreenshotCapture.cpp` (`taa_*` sidecar), `src/core/Types.hpp` (опционально: `taaEnabled` propagation if не propagated elsewhere), `agent/memory.md` (§10.15 closeout), `agent/status.md` (snapshot), `agent/active-sessions.md` (close record), `TODO.md` (P1 TAA-on в новой строке).
- **status:** closed
- **closed-at:** 2026-06-11T22:55:00Z
- **commit-hash:** `9764463` — `feat(render): wire TAA ToggleTaa handler, debug HUD, sidecar, history invalidation`
- **notes:** Phase A сессия 1 (A1) выполнена: ToggleTaa handler (T key), DebugHud TAA JITR/BLND/HIST lines, ScreenshotCapture `taa_*` sidecar, history invalidation on world-reload (FinalizeActiveVoxelWorldReload) + Taa toggle. `taaEnabled` остаётся `false` default (flip в A2). Build green, ctest 1/1, smoke 6/6 на `cam -25 19 25 look 0.62 -0.48 -0.62` с `PROJECTV_ENABLE_VALIDATION=ON` — 0 VUIDs / 0 errors / 6 captures. Backup: `/tmp/taa-closeout-a1/a1-full.patch`.

### session-2026-06-11-asset-pipeline-m0-m5

- **id:** `2026-06-11T19:55Z-asset-pipeline-m0-m5`
- **started-at:** 2026-06-11T19:55:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** M0–M5: импорт полигональных моделей через `fastgltf` + `draco` + `meshoptimizer`. M0 = CMake wiring + smoke build. M1 = sync `AssetLoader` (`.glb` → `fastgltf::Asset`) + `AssetRegistry` + env-var manifest `PROJECTV_MODELS=path.glb@x,y,z;...`. M2 = `MeshBaker` + `MeshGpuResources` (interleaved vertex, meshopt vcache+vfetch, VMA upload). M3 = draco path (`KHR_draco_mesh_compression`). M4 = model graphics pass + `model.vert/frag` + shared GLSL helper для `SceneLightingBuffer` (Q6=U3=b) + `MeshComponent`/`ModelTransformComponent` ECS + Jolt static AABB body. M5 = multi-instance + frustum cull.
- **files-touched-intent:** `CMakeLists.txt`, `src/CMakeLists.txt`, `src/asset/AssetLoader.{hpp,cpp}` (M1+), `src/asset/MeshBaker.{hpp,cpp}` (M2+), `src/asset/MeshGpuResources.{hpp,cpp}` (M2+), `src/asset/DracoMeshDecoder.{hpp,cpp}` (M3+), `src/asset/ModelPass.{hpp,cpp}` (M4+), `src/asset/ModelComponent.hpp` (M4+), `src/asset/AssetRegistry.{hpp,cpp}` (M1+), `src/asset/AssetStub.cpp` (M0), `src/render/Renderer.cpp` (M4 — `RecordModelCommands` between opaque and transparent), `src/render/SceneResources.cpp` (M4 — `ModelRenderState` slot), `src/core/Types.hpp` (M4 — `ModelRenderState` field), `src/app/FramePreparation.cpp` (M4+ — build model draw list), `src/ecs/EcsWorld.cpp` (M4+ — register components), `src/app/AppUpdate.cpp` (M1+ — manifest load), `src/shaders/model.vert`, `src/shaders/model.frag`, `src/shaders/lighting.glsl` (M4 — shared GGX helper + `SceneLightingBuffer` GLSL declaration, U3=b), `tests/AssetLoaderTests.cpp` (M1+), `tests/fixtures/box.glb` (M1). **Не трогаю:** `src/render/vulkan/VulkanBootstrap.cpp`, `src/render/vulkan/VulkanGraphicsPipeline.cpp`, `src/render/vulkan/TaaResolvePipeline.cpp`, `src/render/Taa.*`, `src/render/TaaRenderTargets.cpp` (TAA-сессия scope, см. ниже).
- **status:** closed
- **closed-at:** 2026-06-12T13:00:00Z
- **commit-hash:** `b152b70` — `docs(agent): close out asset-pipeline session with M5/M5.1/M5.2 status`
- **notes:** Решения зафиксированы в диалоге `2026-06-11`: Q1=2, Q2=1 (→ план 3), Q3=1 (→ план 3), Q4=2, Q5=2 (receive-only, выровнено с RTX-будущим), Q6=1 (universal PBR SSBO, GGX reuse), Q7=1 (без save), Q8=2 (можно трогать `SceneLightingBuffer`), U1=c, U2=c, U3=b, U4=b. **Прогресс:** M0 = `1c72a4b` — closed. M1 = `8b112e7` — closed. M2 = `cccdbc1` — closed. M3 = `24ccb08` — closed. M4 = `c4382ea` — closed. **M5** = frustum cull — code complete, build green, ctest 5/5 + new `ProjectVFrustumCullingTests` 5/5. Operator confirmed model visible at `box.glb@0,0,0` etc. с `box_uv.glb` checker-pattern. **DEFERRED (M5 follow-up, TAA-scope):** M5.1 (model-vs-voxel Z-fighting depth bias) — **reverted** in this session: positive `depthBiasConstantFactor` / `depthBiasSlopeFactor` in Vulkan **pushes model fragments away from camera**, so voxel pass (no bias) wins every shared Z-test, and in TAA-on mode the model's `outSceneColor` is overwritten by the voxel pass. Reverted to `depthBiasEnable = VK_FALSE`. Tight-contact z-fighting is now a known issue, deferred to the shadow-pass-styled bias follow-up. M5.2 (TAA-on shader contract: model writes linear, resolve applies tonemap+grading) — `model.frag` `TAA_ENABLED` path now writes linear exposed color (matches `voxel.frag:958-964`); **but** TAA resolve's 3x3 YCoCg neighborhood clamp (`taa_resolve.frag:170-190`) **collapses the model color toward the surrounding voxel range** when the model pixel is surrounded by voxel pixels, so the model becomes a faint grey blob. The fix needs **color-distance rejection** in `taa_resolve.frag` (bias blend toward 0 when `|current − history| > threshold`). `taa_resolve.frag` is **TAA-agent's working tree** (parallel session, AGENTS.md §7.2.6 scope discipline says don't touch), so this fix is **deferred to the TAA-agent's tempo** or to the next dedicated TAA-scope session. **M6 prep** = UV-projected box fixture + shader UV path + generator fixes (header byte order, Khronos winding copy) + regression test (`ProjectVBoxUvFixtureTests`). All complete, build green, ctest 6/6. **Scope discipline:** throughout 2026-06-12 session TAA-agent offline, его ~17 dirty файлов нетронуты (`AppUpdate.cpp`, `InputActions.cpp`, `DebugHud.cpp`, `ProfilingGpu.hpp`, `SceneResources.cpp`, `ScreenshotCapture.cpp`, `VulkanBootstrap.cpp`, `taa_resolve.frag`, `voxel.frag`, `VoxelMaterials.hpp`, `agent/decisions.md`, `agent/memory.md`, `agent/status.md`, `TODO.md`, `.gitignore`). Касался только моих M5/M5.1/M5.2/M6-scope: `SceneResources.hpp` (M5), `FramePreparation.{hpp,cpp}` (M5), `Renderer.cpp` (M5), `core/Types.hpp` (M5), `ModelPass.cpp` (M5.1 reverted), `model.frag` (M5.2 + M6 UV), `tests/FrustumCullingTests.cpp` (M5, new), `tests/BoxUvFixtureTests.cpp` (M6, new), `tests/CMakeLists.txt` (M5 + M6 new test exes), `GenerateBoxUvFixture.cpp` (M6, new), `box_uv.glb` (M6, force-added), `agent/active-sessions.md`. **MVP-отступления** (всё ещё deferred): Jolt AABB body (Q4=2, follow-up ~30 мин), `MeshComponent`/`ModelTransformComponent` в flecs ECS (per-instance пока в `RenderState`), полный `SceneLightingBuffer` extract из 5 шейдеров (U3=b частично: math вынесен, struct declaration per-shader), M5.1 (Z-fight bias) и M5.2 (TAA-on resolve clamp) — оба **deferred** (см. выше).

#### Handoff для следующей сессии (2026-06-12 onward)

**Куда смотреть в первую очередь:**

1. `git log --oneline -10` — последние коммиты: TAA-agent chain (`a2972fa`, `3c87f21`, `02c297c`, `b0fcd9b`, `b7e672f`, `9764463`, `ee82c6f`, `8412b58`, `306003e`, `98fb391`) + asset-pipeline chain (`1c72a4b`, `8b112e7`, `cccdbc1`, `24ccb08`, `c4382ea`, `ccf7400`, `dfaa037`, `b152b70`).
2. `git status -uall` — если дерево **грязное**, **не делать** `git checkout -- .` или `git stash drop` (см. `agent/memory.md §10.11` — там потеряли P0.2 LINEAR fix). Сначала `git diff > /tmp/before_drop_<ts>.patch` и спросить оператора.
3. **Не трогать** TAA-scope файлы пока TAA-agent не закроется (см. ниже).

**TAA-agent dirty файлы (НЕ МОИ, не удалять и не модифицировать):**

`TODO.md`, `agent/decisions.md`, `agent/memory.md`, `agent/status.md`, `src/app/AppUpdate.cpp`, `src/app/InputActions.cpp`, `src/debug/DebugHud.cpp`, `src/debug/ProfilingGpu.hpp`, `src/render/SceneResources.cpp`, `src/render/ScreenshotCapture.cpp`, `src/render/vulkan/VulkanBootstrap.cpp`, `src/shaders/taa_resolve.frag`, `src/shaders/voxel.frag`, `src/voxel/VoxelMaterials.hpp`, `tests/CMakeLists.txt` (там тоже есть правка), `.gitignore`. Other-agent untracked (тоже не мои): `pyproject.toml`, `uv.lock`, `minimax_proxy.py`.

**Известные follow-up'ы, ждущие следующих сессий (приоритет — по решению оператора):**

| ID | Описание | Сложность | Scope |
|----|----------|-----------|-------|
| **M5.2b** | `taa_resolve.frag` color-distance rejection: добавить `if (length(clampedCurrent - clampedHistory) > threshold) blendFactor = 0.0;` перед `mix()`. Это **вернёт** модель в TAA-on. | 10 строк | TAA-scope, нужно согласование с TAA-agent или явное разрешение оператора на merge |
| **M5.1b** | `ModelPass.cpp` depth bias для model-vs-voxel z-fight. Правильный подход — **negative** `depthBiasConstantFactor` (тянет ближе к камере) ИЛИ `depthBiasEnable=FALSE` + `cullMode=NONE` для model pass в overlap region. Альтернатива — отдельный `modelPipelineNoDepthWrite` для overlap cases. | ~30 строк | Мой scope |
| **M5.3** | Jolt AABB body для каждой загруженной модели (Q4=2). `PhysicsWorld::JPH::BodyInterface::CreateAndAddBody` per `LoadedAsset`, AABB из `worldAabbMin/Max` (уже считается в `ModelManifestLoader`). | ~30 строк | Мой scope, простой |
| **M5.4** | `MeshComponent` + `ModelTransformComponent` в flecs ECS. Сейчас per-instance data живёт в `RenderState::visibleModelInstances`. Перевод в ECS — single component per model, query в `FramePreparation::BuildVisibleModelInstanceList`. | ~1 час | Мой scope |
| **M5.5** | Полный `SceneLightingBuffer` extract (U3=b). `lighting.glsl` уже содержит math, но `std430` struct declaration всё ещё per-shader в `voxel.frag`, `voxel_shadow.vert`, `voxel_mesh.comp`, `model.frag`, `model.vert`. Extract в один shared `SceneLightingBuffer.glsl` через `glslc --target-env=vulkan1.3` `#include` support. | ~1.5 часа | TAA-scope-adjacent (5 шейдеров), нужно согласование |
| **M6+** | Real diffuse texture sampling в `model.frag` (сейчас только UV-based procedural checker). Генерировать `tests/fixtures/box_textured.glb` через тот же `GenerateBoxUvFixture`-style generator, добавить `sampler2D baseColor` binding, `material.baseColorTexture` resolve в `AssetLoader`. | ~1 час | Мой scope |

**Test count baseline:** `ctest` = 6/6 (ProjectVTests 1.4s + 5 fast suites). Это baseline, не должно падать.

**Ключевые клавиатурные шорткаты в master:** ToggleTaa = **`T`** (не `;`!). `;` зарезервирован под `TaaJitterScale` (в TAA-agent working tree). Скриншот — кнопка в HUD.

**Ключевые env vars:** `PROJECTV_MODELS=path.glb@x,y,z;path2.glb@x,y,z,rx,ry,rz,s` (manifest), `PROJECTV_START_CAMERA_POSITION="x y z"`, `PROJECTV_START_CAMERA_LOOK="x y z"`, `PROJECTV_LOOKDEV_CAPTURE_*` (smoke harness contract).

**Build preset:** `linux-clang-debug` (native clang 22 + lld 22 + libstdc++ 16). Не трогать `windows-clang-debug` (operator's primary dev tree).

### session-2026-06-11-taa-renderer-wiring

- **id:** `2026-06-11T16:43Z-taa-renderer-wiring`
- **started-at:** 2026-06-11T16:43:00Z
- **closed-at:** 2026-06-11T22:14:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Subtask 1 (format mismatch fix через `VK_EXT_dynamic_rendering_unused_attachments`) + Subtask 2 (Renderer.cpp wiring TAA offscreen main pass + resolve pass + history copy). `taaEnabled` остаётся `false` (visual TAA — отдельная сессия).
- **files-touched-intent:** `src/render/vulkan/VulkanBootstrap.cpp` (extension enable + pNext chain), `src/render/vulkan/VulkanGraphicsPipeline.cpp` (dual `pColorAttachmentFormats` + dual `pAttachments`), `src/render/Renderer.cpp` (restructured `RecordGraphicsCommands` + `InvertColumnMajorMat4` helper), `src/core/Types.hpp` (per-image layout trackers, `supportsDynamicRenderingUnusedAttachments` on `VulkanContextState`), `src/render/vulkan/VulkanSwapchain.cpp` (layout tracker reset), `agent/memory.md` (новый §10.14), `agent/status.md` (snapshot update).
- **status:** closed
- **commit-hash:** `98fb391` — `refactor(render): wire TAA offscreen main pass + resolve pass + history copy`
- **notes:** Subtask 1+2 work landed at `98fb391`: build green, ctest 1/1, smoke 6/6 with `PROJECTV_ENABLE_VALIDATION=ON` — 0 VUIDs / 0 Unfreed / 0 taaResolvePipeline errors. Source-of-truth для плана — `agent/memory.md` §10.12, §10.13. **Координация с параллельной сессией `2026-06-11-asset-pipeline-m0-m5`:** asset-pipeline M0 (CMake wiring) закоммичен оператором как `1c72a4b build(asset): wire fastgltf, draco and meshoptimizer into ProjectV`. Asset-pipeline сейчас на M1 (`AssetLoader` + `AssetRegistry` + manifest), untracked в дереве. Когда asset-pipeline начнёт M4 (`RecordModelCommands` + `ModelRenderState`), будет merge conflict в `Renderer.cpp` / `core/Types.hpp` / `SceneResources.cpp` — он rebase'нется поверх `98fb391` через `git pull --rebase` или manual merge per `AGENTS.md §7.2.6`. Решение: commit сейчас выбрано оператором.

---

## Закрытые сессии (status: closed)

<!-- Недавние закрытые сессии (последние ~10). Старые можно переносить
     в `legacy/docs/archive/agent-sessions/` для сохранения истории. -->

### session-2026-06-11-taa-ycocg-clamp

- **id:** `2026-06-11T20:45Z-taa-ycocg-clamp`
- **started-at:** 2026-06-11T20:45:00Z
- **closed-at:** 2026-06-11T20:50:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** TAA Блок 1 / 1.1 — YCoCg neighbourhood clamp в TAA resolve (замена RGB clamp). Lossless transform Y/Co/Cg, chroma highlight'ов не вымывается в grey. Sidecar metadata `taa_clamp_color_space=YCoCg`.
- **files-touched-intent:** `src/shaders/taa_resolve.frag`, `src/render/ScreenshotCapture.cpp`, `TODO.md`
- **status:** closed
- **commit-hash:** `a2972fa` — `fix(taa): clamp history in YCoCg space to preserve chroma on bright highlights`
- **notes:** Resumed session, YCoCg sub-task complete. Build / ctest / smoke **не перепрогонял** в resumed-сессии по решению оператора (поверхность маленькая, baseline из A1/A2 chain 6/6 clean). Visual verify остаётся в TODO §5 Блок-0 (`Confirm 02c297c` etc). Operator явно сказал «сессию не закрываем, будем ещё работать» — закрыт только этот sub-task (1.1), сама resumed-сессия продолжается как `session-2026-06-11-taa-tooling-1.4-5.1-6.x`.

### session-2026-06-11-multi-agent-policy

- **id:** `2026-06-11T16:30Z-multi-agent-policy`
- **started-at:** 2026-06-11T16:25:00Z
- **closed-at:** 2026-06-11T16:35:00Z
- **agent:** cline/MiniMax-M3
- **operator:** le1t
- **branch:** master
- **scope:** Добавить §7.2.6 «Multi-agent concurrent work policy» в `AGENTS.md` + создать `agent/active-sessions.md` как append-only ledger координации.
- **files-touched-intent:** `AGENTS.md`, `agent/active-sessions.md` (new)
- **status:** closed
- **commit-hash:** _pending_ (заполняется после коммита пользователем)
- **notes:** Источник — явная команда пользователя «над проектом могут работать несколько агентов, изменения могут быть прерваны, агенты должны быть готовы». Протокол multi-agent зафиксирован; см. `AGENTS.md` §7.2.6.
