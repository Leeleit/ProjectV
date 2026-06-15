# Archived: agent/memory.md §10.12-§10.26 (TAA / asset / perf sprint sessions)

**Archived on:** 2026-06-15 (per `docs(agent): compress+archive` commit, planned).
**Reason:** Per-session audit log ("X landed on date Y") grew to ~1100 lines. The per-session
log does not fit the `AGENTS.md §6` anti-duplication matrix for `agent/memory.md`
("Долговечные repo-specific факты, не per-session log"). Each session's commit-hash
and lesson-learned text is preserved verbatim.
**Active sections kept** in `agent/memory.md`: §1-9, §10 (Shadow-quality), §10.11 (Per-corner AO),
§11 (Hardcore perf plan), §10.27 (Agent protocol rewrite).
**Restored reference** if a future regression needs the original diagnostic detail.
**Section numbering preserved** so external cross-refs (TODO.md, agent/active-sessions.md,
agent/decisions.md) resolve through `agent/ARCHIVE-INDEX.md` with same anchors.

---

## 10.12 TAA infrastructure landed (anti-jitter baseline, `2026-06-11`, uncommitted)

**Что сделано в этой сессии.** Anti-jitter baseline is half-wired: вся CPU-сторона +
scene lighting buffer contract + shaders написаны, но offscreen scene-color /
history ping-pong / TAA resolve pipeline ещё **не подключён** (visual TAA ещё не
работает). Причина расщепления: TAA resolve pipeline требует значительного объёма
изменений в Vulkan-инфраструктуре (offscreen render target, history ping-pong,
fullscreen resolve pass, pipeline layout + descriptor set, depth attachment
sharing, layout transitions), и в этой сессии фокус был на инфраструктурной
готовности, а не на визуальном эффекте.

**Что landed (CPU + contracts + shaders, no visible effect yet):**
- `VoxelSceneLighting` расширен с `taaParams` (vec4: jitterX, jitterY, blend, enabled),
  `prevViewProjectionMatrix` (mat4, 64 bytes), `taaHistoryParams` (vec4: texelX, texelY,
  historyValid, reserved) — суммарно 96 байт, sizeof 512 → 608, byte-layout enforced
  через `static_assert` в C++ + identity в трёх шейдерах (`voxel.frag`,
  `voxel_shadow.vert`, `voxel_mesh.comp`). Layout mismatch ловится compile-time.
- `BuildGraphicsPushConstants` принимает дополнительные `taaJitterNdcX/Y` параметры,
  применяет их к projection matrix через `m[2][0]` и `m[2][1]` (NDC sub-pixel offset).
  Default-значения 0, поэтому существующие вызовы работают без jitter.
- `FramePreparation` продвигает 8-tap Halton(2,3) sequence через `Taa::AdvanceTaaPixelJitter`
  каждый кадр, конвертирует pixel→NDC offset, применяет jitter, и стэшит
  `viewProjection` в `render.taaPrevViewProjectionMatrix` для следующего кадра.
- `RefreshSceneLightingBuffer` (в `SceneResources.cpp`) заполняет `currentSceneLighting.taaParams`,
  `prevViewProjectionMatrix`, `taaHistoryParams` каждый кадр, потом `memcpy` в
  `sceneLightingMappedData` уже включает TAA поля автоматически.
- `LightingDebugView::Taa` (10-е значение) + `GetNextLightingDebugView` chain
  `Fog → Taa → Final`, `LightingDebugViewToString` → `"TAA"`. `B` клавиша цикл теперь
  включает Taa debug view.
- `InputAction::ToggleTaa` (37-е значение) — будет wired в `InputActions.cpp`
  + обработано в `AppUpdate.cpp` в следующей сессии (binding `T` клавиши).
- `Taa` debug view в `voxel.frag` — placeholder case (ещё не реализован).
- `DebugStats` обогащён `taaEnabled`, `taaJitterX/Y`, `taaBlend`, `taaHistoryValid`.
- `RenderState` обогащён `taaEnabled` (default **false**), `taaBlend=0.10`,
  `taaFrameCounter=0`, `taaHistoryValid=false`, `taaPrevViewProjectionMatrix`,
  `taaJitterX/Y`, **плюс** все поля для TAA resolve pipeline (offscreen images,
  views, allocations, sampler, descriptor set layout/pool/sets, pipeline) — все
  `VK_NULL_HANDLE` пока, готовы к подключению.
- `Taa.hpp` / `Taa.cpp` — Halton(2,3) 8-tap helper, `BuildTaaHistoryParams`.
- `taa_resolve.vert` — fullscreen triangle (без vertex buffer, `gl_VertexIndex` 0..2).
- `taa_resolve.frag` — 3×3 RGB clamp history blend с depth-reproject, тон-мэп +
  color grading применяются здесь (вынесены из `voxel.frag` чтобы history blend
  работал в линейном свете). Shaders написаны, но ещё не используются.

**Что ещё **не** сделано (deferred TAA pipeline work):**
- Offscreen scene color target (R16G16B16A16_SFLOAT, swapchain-sized) + history
  ping-pong (2 images, swap после resolve) — нужны VMA-allocated VkImage +
  VkImageView + transitions в `Renderer.cpp`.
- TAA resolve pipeline (fullscreen) + pipeline layout + descriptor set (bindings:
  sceneColor, historyColor, depth, sceneLighting). `VulkanGraphicsPipeline.cpp`
  сейчас имеет 5 pipelines, нужно добавить 6-й — `taaResolvePipeline`.
- `Renderer.cpp` main pass должен писать в `taaSceneColorImage` вместо swapchain;
  TAA resolve pass запускается после, output в swapchain. Layout transitions:
  `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` для истории, swap-а
  ping-pong.
- `VulkanSwapchain.cpp` / `VulkanInit.cpp` — recreate scene color target при
  resize (как `RecreateSwapchain`).
- `DebugHud.cpp` — добавить TAA JITR/BLND/HIST строки в detailed HUD.
- `AppUpdate.cpp` — обработать `ToggleTaa` action, обновить stats.
- `InputActions.cpp` — wire T keybinding.
- `ScreenshotCapture.cpp` — добавить `taa_*` строки в sidecar.
- `AppUpdate.cpp` / `VulkanSwapchain.cpp` — invalidate history на resize / world
  reload / preset change / pause / Taa toggle.

**`taaEnabled` default = false.** Это **критический** design decision. Причина:
TAA resolve pipeline ещё не подключён, поэтому jitter без resolve = sub-pixel
wobble на main pass = видимый **новый** aliasing вместо anti-jitter. `taaEnabled=false`
→ jitter=0 → сцена рендерится как до изменений. Когда TAA pipeline подключён в
следующей сессии — переключить default на `true` и проверить anti-jitter.

**Build verification:**
- `cmake --build build/linux-clang-debug --target ProjectV --parallel 8` — green
- `ctest --test-dir build/linux-clang-debug` — 1/1 passed (1.42 sec)
- `cp build/.../src/*.spv build/.../bin/` — выполнено (per §10.11 lesson learned)
- Проверка `Offsetof` через `static_assert` в C++ + identity в GLSL прошла compile-time.

**Где смотреть прогресс:**
- `src/voxel/VoxelMaterials.hpp` — `VoxelSceneLighting` layout + `static_assert`
- `src/voxel/VoxelMaterials.cpp` — `LightingDebugView::Taa` + switch
- `src/core/Types.hpp` — `DebugStats` + `RenderState` TAA поля + `InputAction::ToggleTaa`
- `src/app/Camera.{hpp,cpp}` — `BuildGraphicsPushConstants` jitter
- `src/app/FramePreparation.cpp` — Halton + prev viewProj save
- `src/render/SceneResources.cpp` — `RefreshSceneLightingBuffer` TAA поля
- `src/render/Taa.{hpp,cpp}` — Halton sequence
- `src/shaders/taa_resolve.{vert,frag}` — TAA resolve shaders
- `src/shaders/voxel.frag`, `voxel_shadow.vert`, `voxel_mesh.comp` — `SceneLightingBuffer` расширен
- `src/CMakeLists.txt` — Taa.cpp + taa_resolve шейдеры
- `TODO.md` §5 Post-TAA follow-ups — R&D список
- `agent/status.md` — snapshot 2026-06-11
- `agent/decisions.md` §18 TAA contract — **TODO**: добавить в следующей сессии

**Lesson learned (shaders):** `cmake --build` корректно скопировал новые .spv в bin на этот
раз (build в этом сессии вызвал `Linking CXX executable bin/ProjectV`, что
триггерит `add_custom_command(TARGET ProjectV POST_BUILD ...)`). Но после `Taa.cpp`
добавления build только перекомпилировал .o файлы и не пересоздал ELF, поэтому
старые .spv в bin остались. Я **вручную** `cp` все нужные .spv после build'а
(per §10.11). Working rule остаётся: после shader changes → всегда
`cmake --build ... --target ProjectV` для полной перелинковки, иначе `cp` вручную.

**Следующий шаг (новая сессия):**
1. Добавить offscreen scene color + history ping-pong в `VulkanSwapchain.cpp` (или
   новый `VulkanRenderTargets.{hpp,cpp}`).
2. Создать TAA resolve pipeline в `VulkanGraphicsPipeline.cpp`.
3. Изменить `Renderer.cpp::RecordGraphicsCommands` — main pass в `taaSceneColorImage`,
   TAA resolve pass в swapchain.
4. `DebugHud.cpp` — TAA JITR / BLND / HIST строки.
5. `AppUpdate.cpp` — `ToggleTaa` handler + stats propagation.
6. `InputActions.cpp` — wire T keybinding.
7. `ScreenshotCapture.cpp` — taa_* sidecar.
8. Когда visual TAA работает: переключить `taaEnabled` default на `true`,
   сделать captures (FINAL + JITR debug view), закоммитить.

## 10.13 TAA offscreen targets landed (`2026-06-11`, follow-up to §10.12, committed `d9830c2`)

TAA render targets (`projectv::taa::OffscreenColorTarget`) теперь **аллоцированы** в `VulkanSwapchain::RecreateSwapchain` — пара R16G16B16A16_SFLOAT images (scene + history) + linear sampler. Recreate path сбрасывает `taaHistoryValid = false` каждый раз, что отключает history-blend на один кадр после resize. Targets pre-allocated даже когда `taaEnabled=false` (~24 MiB на 1440p) чтобы runtime toggle не требовал swapchain recreate.

**Forward-declaration dance:** `core/Types.hpp` forward-declares `projectv::taa::OffscreenColorTarget`, чтобы использовать указатель на incomplete type в `RenderState`. `TaaRenderTargets.hpp` имеет **own** forward decl `struct VulkanContextState` потому что `core/Types.hpp` сам включает `TaaRenderTargets.hpp` **до** своего `struct VulkanContextState;` forward-declaration line. `VmaAllocation` объявлен в `.hpp` как `void*` (через `using VmaAllocationHandle = void*;`) и кастится в `VmaAllocation` в `.cpp` где `vk_mem_alloc.h` уже включён — это держит `vk_mem_alloc.h` от утечки в каждый translation unit который включает `core/Types.hpp`. Полезный паттерн для будущих opaque types в render state.

**Lesson learned (header forward decl в cyclic include):** Когда header A включён в header B, и B определяет тип C, но A использует C — добавь forward decl C **в A** перед `#include B`. Guard предотвращает recursive include, но порядок объявлений теряется, так что forward decl в A становится необходим для парсинга до того, как B объявит C.

**Что осталось до visual TAA:** TAA resolve pipeline в `VulkanGraphicsPipeline.cpp` (6-й pipeline, fullscreen, descriptor set с bindings sceneColor/historyColor/depth/sceneLighting), `Renderer.cpp` main pass → offscreen, TAA resolve pass → swapchain (через `vkCmdBlitImage` для format conversion R16G16B16A16_SFLOAT → B8G8R8A8_UNORM), `AppUpdate.cpp` ToggleTaa handler, `DebugHud.cpp` TAA строки, `ScreenshotCapture.cpp` `taa_*` sidecar entries, history invalidation на resize / world reload / preset change / pause / Taa toggle, `taaEnabled` default flip на `true`. После этого — captures (FINAL + JITR debug view) и `agent/decisions.md` §18 TAA contract.

Время: каждый из этих подзадач — 30-300 строк кода. Следующая сессия может довести до визуального TAA за 1-2 часа фокусированной работы.

## 10.14 TAA renderer wiring landed (`2026-06-11`, follow-up to §10.13, **uncommitted**)

`taaEnabled` остаётся `false` (visual TAA — отдельная сессия). Вся инфраструктура для resolve pass теперь подключена и работает как no-op когда `taaEnabled=false` (fallback на старое поведение TAA-off).

**Что сделано в этой сессии:**

1. **Subtask 1 — format mismatch fix:**
   - `VulkanBootstrap.cpp` — `VK_EXT_dynamic_rendering_unused_attachments` (extension #500, ratified) включён opportunistically при `TryPickPhysicalDevice`. Feature struct `VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT` + feature bit `dynamicRenderingUnusedAttachments` через `pNext` chain в `VkDeviceCreateInfo`. На Linux host (RTX 3060 Ti, Vulkan 1.4.350) feature bit = `true`, extension revision 1. `VulkanContextState.supportsDynamicRenderingUnusedAttachments` хранит это для downstream gate.
   - `VulkanGraphicsPipeline.cpp` — main voxel pipeline `VkPipelineRenderingCreateInfo` теперь декларирует **два** color attachment formats (`swapchain_format`, `R16G16B16A16_SFLOAT`). VUID-VkGraphicsPipelineCreateInfo-renderPass-06055 fixed через `pColorBlendState->attachmentCount = 2` с идентичными `pAttachments` entries (slot 0 = полный RGBA write, slot 1 = тот же; `dynamicRenderingUnusedAttachments` разрешает `imageView = VK_NULL_HANDLE` на unused slot в per-frame `VkRenderingAttachmentInfo`). Defensive fail-fast в `CreateGraphicsPipeline` если extension не поддерживается.

2. **Subtask 2 — Renderer.cpp TAA-aware RecordGraphicsCommands:**
   - Per-frame TAA gate: `taaOn = taaEnabled && offscreenTargets != nullptr && resolvePipeline != nullptr`. Все TAA-части обёрнуты в `if (taaOn)`.
   - TAA-off path (по умолчанию): single `vkCmdBeginRendering` block, slot 0 = swapchain (write), slot 1 = NULL (discarded), debug overlay/HUD в main pass — **byte-equivalent contract** к pre-change состоянию (visual verified в smoke 6/6 с `PROJECTV_ENABLE_VALIDATION=ON`).
   - TAA-on path: 2 begin/end blocks. Block 1 — main pass с двумя attachments (slot 0 = NULL, slot 1 = `taaSceneColorTarget`), opaque + transparent draws. Block 2 — TAA resolve pass (fullscreen triangle, 3 verts, no VBO), single attachment = swapchain, no depth, debug overlay/HUD в том же block. Layout transitions: sceneColor `COLOR_ATTACHMENT → SHADER_READ_ONLY`, depth `DEPTH_ATTACHMENT → DEPTH_READ_ONLY`, history `* → SHADER_READ_ONLY` (для resolve sample), затем history copy `vkCmdCopyImage` sceneColor → historyColor с переходами через `TRANSFER_SRC`/`TRANSFER_DST` (skip на первом кадре через `taaHistoryValid = false` flag).
   - Per-image layout trackers в `RenderState` (`depthImageCurrentLayout`, `taaSceneColorCurrentLayout`, `taaHistoryColorCurrentLayout`) — depth lands в `DEPTH_ATTACHMENT` после TAA-off frame и `DEPTH_READ_ONLY` после TAA-on frame, и стартовый transition следующего кадра корректно выбирает `oldLayout` независимо от `taaEnabled` toggle между кадрами. Reset в `VulkanSwapchain.cpp::RecreateSwapchain` на UNDEFINED.
   - `InvertColumnMajorMat4` helper (Gauss-Jordan с partial pivoting) в анонимном namespace `Renderer.cpp` для `inverseCurrentViewProjection` в `ResolvePushConstants` (GLM не подключен к build — стараемся избегать новых зависимостей).
   - Subtle issue fixed mid-session: первоначально `pColorBlendState->attachmentCount` (1) не соответствовал `colorAttachmentCount` (2) → VUID-06055; потом `pAttachments[0] != pAttachments[1]` без `independentBlend` feature → VUID-00605. Оба fixed через identical dual entries.

**Что НЕ сделано (deferred, отдельная сессия):**
- `taaEnabled` default flip `false → true` — visual verify отдельная сессия.
- `AppUpdate.cpp` `ToggleTaa` handler + `InputActions.cpp` T-биндинг (subtask C, out of scope).
- `DebugHud.cpp` TAA JITR/BLND/HIST строки (subtask D, out of scope).
- `ScreenshotCapture.cpp` `taa_*` sidecar entries (subtask E, out of scope).
- History invalidation hooks (resize уже есть в `VulkanSwapchain.cpp`; остаются world reload / preset change / pause / Taa toggle, subtask F, out of scope).
- `agent/decisions.md` §18 TAA contract entry (после visual verify, subtask I).
- Per-frame `vkCmdResetQueryPool` для HUD counters и TAA-related `DebugStats` propagation (subtask D-F).
- History copy uses raw scene color (not resolved output) — для TAA on/off toggle это OK (history represents prev frame raw input), но resolved-output copy (через resolve pass → history target) был бы точнее. Это отдельный work item — потребует либо MRT в resolve shader, либо vkCmdBlitImage swapchain → history (свои layout transition complications).

**Verification:**
- `cmake --build build/linux-clang-debug --target ProjectV --parallel 8` — green (только pre-existing `DebugHud.cpp:605` format warning).
- `ctest --test-dir build/linux-clang-debug --output-on-failure -C Debug` — 1/1 passed (1.44 sec).
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `cam -25 19 25 look 0.62 -0.48 -0.62` (VoxelLab) с `PROJECTV_ENABLE_VALIDATION=ON` — 6/6 captures (FINAL SHDW CSM CTSH AOCC LOCL), 0 VUID / 0 Unfreed / 0 errors / 0 warnings.
- Pre-existing non-determinism между consecutive smoke runs (~10% pixel diff) — не regression от моих изменений, видно по `shadow_cascade_ortho_extents` / `shadow_cascade_texel_world` в sidecars, которые зависят от camera position application. Камера между runs не байт-точно воспроизводимая; visual diff вручную не делал (no vision_analyze под рукой), но smoke pass + sidecar metadata показывают expected values.

**Параллельная сессия `2026-06-11-asset-pipeline-m0-m5`:** см. `agent/active-sessions.md` (закрытая запись TAA + открытая asset-pipeline). На момент закрытия TAA-сессии asset-pipeline на M0 (CMake wiring) — непересекающиеся правки в `CMakeLists.txt` / `src/CMakeLists.txt`. **M4 asset-pipeline планирует править `Renderer.cpp` / `core/Types.hpp` / `SceneResources.cpp`** для `RecordModelCommands` + `ModelRenderState` — это **прямой конфликт** с моими TAA-изменениями в `Renderer.cpp::RecordGraphicsCommands` и `core/Types.hpp` layout trackers. Решение — за оператором:
- (a) Commit моих TAA-изменений сейчас → asset-pipeline будет rebase M4 поверх моих правок.
- (b) Подождать M0-M3 asset-pipeline, чтобы TAA merge был атомарным с M4 conflict resolution.
- (c) Параллельно — но потребует arbitration при merge conflict (см. `AGENTS.md §7.2.6`).

**Commit message draft** (per `AGENTS.md §7.2.5`, _awaiting operator confirmation_):
```
refactor(render): wire TAA offscreen main pass + resolve pass + history copy

Anti-jitter baseline completed up to the resolve pass. The TAA
resolve pipeline was already created at startup (commits 52b130f,
d9830c2, 089fc90), the offscreen targets were already allocated on
swapchain recreate, and the scene-lighting contract already carried
the TAA fields. This commit wires the per-frame plumbing that lets
the resolve pass actually run when the runtime master `taaEnabled`
is flipped on, in five files:

  - core/Types.hpp — per-image layout trackers
    (`depthImageCurrentLayout`, `taaSceneColorCurrentLayout`,
    `taaHistoryColorCurrentLayout`) and a
    `VulkanContextState::supportsDynamicRenderingUnusedAttachments`
    gate.
  - src/render/vulkan/VulkanBootstrap.cpp — enable
    `VK_EXT_dynamic_rendering_unused_attachments` (extension #500)
    opportunistically; chain
    `VkPhysicalDeviceDynamicRenderingUnusedAttachmentsFeaturesEXT`
    in the device create info pNext list when the device supports
    the extension.
  - src/render/vulkan/VulkanGraphicsPipeline.cpp — main voxel
    graphics pipeline now declares two color attachment formats
    (`swapchain_format`, `R16G16B16A16_SFLOAT`) so the same pipeline
    can drive the TAA-on path (slot 1 = scene color) and the TAA-off
    path (slot 0 = swapchain) via the
    `dynamicRenderingUnusedAttachments` feature. Color blend state
    attachment count bumped to 2 with identical entries (VUID-06055
    and VUID-00605 — `independentBlend` is not enabled). Fail-fast
    in `CreateGraphicsPipeline` if the device lacks the feature.
  - src/render/vulkan/VulkanSwapchain.cpp — reset the three layout
    trackers alongside the existing `*NeedsInit` reset on
    `RecreateSwapchain` so a fresh offscreen target lands back in
    `UNDEFINED`.
  - src/render/Renderer.cpp — `RecordGraphicsCommands` now branches
    on a per-frame `taaOn` gate. The TAA-on path runs the main pass
    into `taaSceneColorTarget` and then a fullscreen resolve pass
    into the swapchain, followed by a `vkCmdCopyImage` history
    copy from scene color to history. A local
    `InvertColumnMajorMat4` Gauss-Jordan helper builds
    `inverseCurrentViewProjection` for the resolve shader
    (column-major, matches the rest of the project; GLM is not
    linked, see §6). The TAA-off path keeps the previous behaviour
    exactly (slot 0 = swapchain, slot 1 = NULL, debug overlay and
    HUD in the main pass) so the gate-off visual is byte-equivalent
    to pre-change.

`taaEnabled` stays `false` (visual TAA activation is a separate
session that also needs `AppUpdate` `ToggleTaa` handler + debug
Hud TAA lines + sidecar entries + history invalidation on
world-reload / preset / pause / Taa toggle + visual verify). Build
green, ctest 1/1, smoke 6/6 on VoxelLab reference shot with
`PROJECTV_ENABLE_VALIDATION=ON` — 0 VUID / 0 Unfreed allocations /
0 errors.

Refs: agent/memory.md §10.12, §10.13
```

**Working rule (TAA on/off toggle correctness):** Per-image layout
trackers (rather than per-pass hardcoded layouts) are now the
canonical mechanism for the depth + offscreen + history transition
chain. Any future TAA-related per-frame transition should
read `*CurrentLayout` and write back the new value, not assume
either `UNDEFINED` or a fixed post-state. The same pattern applies
to any future offscreen resource that needs to be both written
and sampled across frames.

## 10.17 TAA Блок 1 / 1.2 + 1.3 — camera-cut detector + adaptive CAS sharpening (`2026-06-12`)

**1.2 — Camera-cut detection.** Chebyshev (L-infinity, max-abs over
the 16 floats of `viewProjection`) distance between the previous
and current frame's `viewProjection`, computed each frame in
`FramePreparation::BuildFrameData` after the Halton jitter advance
and before the `taaPrevViewProjectionMatrix` stash. Threshold
`kTaaCameraCutThreshold = 0.10f` lives as a single constant — operator
data shows 0.10 cleanly separates "ordinary mouse-look / WASD / spectator
fly" (delta < 0.01/frame) from "snap rotation / teleport / scene-preset
change" (delta > 0.20/frame). When the delta exceeds the threshold,
`taaHistoryValid = false` and `taaCameraCutCount++`; this is the
**7th history-invalidation trigger** in the `decisions.md` §18 list
(beyond swapchain resize / world reload / Taa toggle / jitter scale /
blend / neighbourhood radius / `.` invalidate).

**First-frame false-positive guard.** `taaPrevViewProjectionMatrix` is
zero-initialised, so a naive detector would register a
`maxDelta ≈ |viewProj|max ≈ 40` cut on the very first frame. To
prevent this, a companion `bool taaPrevViewProjectionMatrixInitialized`
in `RenderState` is set on the first successful stash and gates the
detector. `VulkanSwapchain.cpp::CreateOrRecreateSwapchain` clears it
(next to the existing `taaPrevViewProjectionMatrix = {}`) so the
post-recreate frame is also a clean baseline; the existing
`taaFrameCounter = 0u` and `taaHistoryNeedsInit = true` resets in the
same path remain. The detector is single-call per frame, 16
subtractions + 16 max-abs + 1 compare — bandwidth-free.

**1.3 — Inline CAS (Contrast Adaptive Sharpening) post-TAA.** AMD
FidelityFX CAS port (`Bartłomiej Wronski, "FidelityFX CAS –
Contrast Adaptive Sharpening", GPUOpen 2020;
https://github.com/GPUOpen-Effects/FidelityFX-CAS`) integrated into
`taa_resolve.frag` so the resolve pass stays single-pass. The
high-pass kernel is `center - 4-corner-avg`; the per-channel weight
`(highPass) / (max - min)` is positive-clamped to `[0, 1]` so flat
regions (highPass ≈ 0) get no boost and bright overshoot is impossible.
The result is clamped to the local RGB `[min, max]` range to avoid
neighbour-color contamination.

**No extra texture lookups.** `GetSceneColorRange` was extended in
the same loop the TAA YCoCg clamp already runs: the existing
`2r+1 × 2r+1` sweep now also accumulates `rgbMin / rgbMax` (from
cross+center, 5 taps) and `rgbCornerSum` (4 corner taps). The
branch (`isCorner` / `isCross`) is a single `bool` and one
accumulator per pixel, ALU-cheap. The loop is bandwidth-bound
(45 texture samples per fragment at radius=7), not ALU-bound, so the
extra accumulators don't change the resolve's GPU cost.

**`sharpenAmount = (1.0 - taaBlend) * taaCasSharpnessMax`** is derived
in-shader from the new `taaBlend` / `taaCasSharpnessMax` push-constant
fields. High blend (more history weight, already stable) -> less
sharpening; low blend (more noise) -> more. TAA-off falls through
with `taaBlend = 0`, so the ceiling `taaCasSharpnessMax` applies at
full strength (no temporal blur to undo, so the ceiling is
appropriate). The CAS step is **linear-light pre-tonemap** because
the `rgbMin / rgbMax / rgbCornerSum` come from the pre-tonemap scene;
applying a linear high-pass kernel in sRGB space would mix the wrong
gamma. `taa_resolve.frag` passes `mappedOut = ApplyTaaToneMap(linearOut)`
only after the CAS step.

**Push constant layout.** `ResolvePushConstants` replaced the trailing
`vec2 reservedPadding` with `float taaBlend; float taaCasSharpnessMax;`
— same 8 B total, byte layout unchanged (verified by `static_assert`
at `core/Types.hpp:212-218`; `offsetof(ResolvePushConstants, taaBlend)
== 136` and `...taaCasSharpnessMax == 140`). `Renderer.cpp:1004-1009`
populates the new fields from `render.taaEnabled ? render.taaBlend :
0.0f` and `render.taaCasSharpnessMax` respectively.

**Why inline CAS instead of a separate `cas.frag` pipeline.** A separate
post-TAA CAS pipeline would need a new graphics pipeline, descriptor
set layout, render pass slot between TAA resolve and the swapchain,
and a third fullscreen draw per frame — all for a 5+4-tap pass that
reuses data already gathered. Integrating it into `taa_resolve.frag`
keeps the resolve single-pass, eliminates a swapchain readback
(CAS reads from the *pre-tonemap* linear buffer, not the swapchain),
and avoids touching `VulkanGraphicsPipeline.cpp` (which is also
shared with the asset-pipeline's M4 model pass). The trade-off is
that `taa_resolve.frag` now does TAA + CAS in one pass; the cost is
the 4-corner accumulator in the existing loop, which is bandwidth-
negligible.

**Verification (`2026-06-12`, this session):**
- `cmake --build build/linux-clang-debug --target ProjectV ProjectVTests`
  — green, 1 pre-existing warning at `DebugHud.cpp:600` (`%.0f` for
  bool, not my change).
- `ctest --test-dir build/linux-clang-debug --output-on-failure` —
  6/6 passed (`ProjectVTests`, `ProjectVAssetTests`,
  `ProjectVMeshBakerTests`, `ProjectVDracoTests`,
  `ProjectVFrustumCullingTests`, `ProjectVBoxUvFixtureTests`), 1.45 s
  wall clock.
- `bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh
  --camera-pos "-25 19 25" --camera-look "0.62 -0.48 -0.62"
  --views "FINAL SHDW CSM CTSH AOCC LOCL"` — 6/6 captures at
  `build/linux-clang-debug/lookdev-captures/20260612-1.2-1.3-smoke-v2/`.
  Sidecar `taa_camera_cut_count=0` (static camera, expected),
  `taa_cas_sharpness_max=0.500000`, `taa_history_valid=1`.
- Vision review of FINAL view: scene renders clean — VoxelLab
  glass/fluid sphere, opaque anchor, checker floor, no ringing /
  haloing from the CAS step, no ghosting from the camera-cut
  detector. HUD FPS 93.2 on this build.

**Working rules to inherit:**
- **First-frame / post-recreate baseline guard.** Any new
  frame-to-frame state that's initialised to a sentinel (zero, NaN,
  identity matrix) and compared against the next-frame value needs a
  companion "initialised" flag, **not** a `frameCounter > N` heuristic
  (which breaks if the counter is reset mid-session for a different
  reason — e.g. swapchain recreate). Reset the flag in every code path
  that resets the underlying state.
- **Linear-light CAS, not sRGB.** AMD's reference CAS operates in
  display-referred (sRGB-encoded) space because it's typically
  composed after a separate post-process stack. Our CAS runs on
  linear data, so the high-pass kernel and the `[min, max]` range
  must be in linear light too. Mixing would give a different gamma
  curve and break the "clamp to local range" overshoot guard.
- **Push constant byte layout invariance.** `ResolvePushConstants`
  gained 2 new float fields but the total size stayed 144 B. The
  `static_assert` block at the struct definition is the source of
  truth for layout; updating it in lockstep with the shader's
  GLSL declaration is mandatory.

**Cross-refs:** `TODO.md` Блок 1 (1.2 + 1.3 closed in this
session), `agent/decisions.md` §19 (TAA sharpness contract), this
section, `agent/status.md` §10 (in-progress session snapshot).

## 10.18 TAA Блок 1 / 1.7 — R11G11B10_UFloat scene color (`2026-06-12`)

**Single-line format change: 8 → 4 bytes/pixel на TAA scene color
+ history.** Mechanical, low-risk, **2× bandwidth reduction** на
resolve-pass read (`historyColor` sample) и per-frame
`vkCmdCopyImage` history update.

**Single source of truth: `kTaaSceneColorFormat` constant.**

`src/render/TaaRenderTargets.hpp` — new `inline constexpr VkFormat
kTaaSceneColorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32` in the
`projectv::taa` namespace. Consumed by:

- `src/render/TaaRenderTargets.cpp:86` — image allocation
  (`vmaCreateImage` with `imageInfo.format = kTaaSceneColorFormat`)
- `src/render/vulkan/VulkanGraphicsPipeline.cpp:1794` — pipeline
  declaration (`pColorAttachmentFormats[1] = kTaaSceneColorFormat`)

The constant is the only place the format is hard-coded. If a
future change needs to bump back to R16G16B16A16 (e.g. banding
becomes visible), the change is 1 line + rebuild.

**Shader code unchanged.** `voxel.frag` writes `vec4 outSceneColor`
(Location 1), `model.frag.taa_on.spv` writes `vec4 outSceneColor`
(Location 1), `taa_resolve.frag` reads `texture(historyColor, ...).rgb`.
Vulkan spec: alpha channel of `outSceneColor` is **undefined** for
packed formats like `B10G11R11_UFLOAT_PACK32` (no storage for alpha),
but the resolve only consumes `.rgb`, so the dropped alpha is a
no-op. The resolve output writes to the swapchain (B8G8R8A8 UNORM
on most desktops), which has full alpha — that transition is
transparent to the rest of the pipeline.

**`vkCmdCopyImage` format compatibility** (Vulkan spec §7.1.1):
srcImage and dstImage formats must be identical. Both `sceneColor`
and `historyColor` use `kTaaSceneColorFormat`, so the copy is
unchanged.

**Why R11G11B10_UFLOAT, not R10G10B10A2_UNORM?** The TAA scene color
needs **unsigned-float** representation (linear HDR after tone-map
in the resolve pass) and **RGB-only** (alpha is unused). A2UNORM
wastes 2 bits on an unused alpha. R11G11B10 has 5/6/5 bits per channel
with a shared 5-bit exponent — narrow dynamic range but 32 bits
total, which matches our needs exactly. B10G11R11 is the standard
"Vulkan R11G11B10" name.

**Loss of precision vs R16G16B16A16_SFLOAT.** 5 bits B + 6 bits G +
5 bits R + 5-bit shared exponent. The shared exponent is the main
risk: a single bright sample in a frame compresses the dim
neighbour's exponent range, visible as banding in dim areas
(< 0.1% intensity in linear light). The capture-driven `taa_scene_
color_format` sidecar key lets the operator verify the format at
runtime; if banding shows up, revert is a 1-line constant change.

**Build / test / smoke (`2026-06-12`):**
- `cmake --build build/linux-clang-debug --target ProjectV
  ProjectVTests ProjectVAssetTests ProjectVMeshBakerTests
  ProjectVDracoTests ProjectVFrustumCullingTests
  ProjectVBoxUvFixtureTests --parallel 8` — green, 1 pre-existing
  warning (`DebugHud.cpp:600` LOCL `%.0f` for bool, не моя).
- `ctest --test-dir build/linux-clang-debug --output-on-failure` —
  6/6 passed (1.48 s wall clock).
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на VoxelLab
  reference shot — 6/6 captures, sidecar shows
  `taa_scene_color_format=B10G11R11_UFLOAT`,
  `taa_history_valid=1`, `taa_blend=0.10`, `taa_camera_cut_count=0`.
- Vision review of FINAL view: scene renders clean, FPS **110.6**
  (выше 1.2+1.3 baseline 93.2 — likely bandwidth reduction showing
  perf benefit, though single-run variance is high enough that this
  could also be noise). **No visible banding** in dim areas (sky
  background uniform light blue, checker floor clean).

**Working rule to inherit:**
- **Single source of truth for cross-consumer constants.** When a
  Vulkan format is consumed by both image allocation and pipeline
  declaration, define it as an `inline constexpr` in the header
  next to the resource struct, not as two separate literals. The
  constant prevents the two consumers from drifting on a future
  change; the compiler enforces the relationship. This pattern
  applies to any cross-shader-struct value (push-constant fields,
  descriptor-set bindings, etc.) — see also
  `agent/decisions.md` §18 (TAA push-constant byte layout invariance
  from 1.2+1.3) and §19 (ResolvePushConstants field rename
  preserved byte layout).

**Cross-refs:** `TODO.md` Блок 1 (1.7 closed), `agent/decisions.md`
§20 (TAA scene color format contract, this section),
`agent/status.md` §11 (in-progress session snapshot).

## 10.15 TAA close-out plumbing landed (A1, `2026-06-11`, committed as `9764463`)

Phase A сессия 1. `taaEnabled` всё ещё `false` (default). Четыре deferred subtask'а из `agent/memory.md §10.14` закрыты + история-инвалидация:

- **Subtask C — ToggleTaa handler + T-биндинг:**
  - `InputActions.cpp`: `BindAction(input, InputAction::ToggleTaa, SDL_SCANCODE_T)` добавлен между PlayLastInputReplay (Y) и ToggleMutationAnchor (X).
  - `AppUpdate.cpp`: ToggleTaa handler работает как остальные toggle handlers: `ConsumeInputActionPressed` → flip `render->taaEnabled` + `render->taaHistoryValid = false`. Гейт `world->voxelWorld` (требуется active world).
  - Ранее добавленный `InputAction::ToggleTaa` (37-й элемент enum в `core/Types.hpp`) теперь забинден и обрабатывается.
  - `DebugStats` propagation: каждый кадр `AppUpdate.cpp` копирует `render->taaEnabled/blend/frameCounter/historyValid/jitterX/Y` → `debug->stats.*`. Jitter X/Y были добавлены в `DebugStats` (ранее отсутствовали — комментарий предполагал, что JITR не нужен в HUD, но для A1 он потребовался).

- **Subtask D — DebugHud TAA JITR/BLND/HIST lines:**
  - `DebugHud.cpp`: добавлен блок `TAA %s JIT %.2f %.2f BLND %.3f HIST %s` после TSHD (TransparentShadowPolicy) строки.
  - JITR показывает текущий sub-pixel jitter offset (при `taaEnabled=false` это `0.00 0.00`).
  - BLND показывает blend factor (`0.100` default).
  - HIST показывает `true`/`false` (valid = 1 на втором+ кадре после TAA-enable или после history invalidation).

- **Subtask E — ScreenshotCapture taa_* sidecar entries:**
  - `ScreenshotCapture.cpp`: добавлены `taa_enabled`, `taa_jitter_x`, `taa_jitter_y`, `taa_blend`, `taa_history_valid` в format string + args между `shadow_cascade_blend` и `shadow_cascade_count`.

- **Subtask F — History invalidation hooks:**
  - `main.cpp::FinalizeActiveVoxelWorldReload`: `state->render.taaHistoryValid = false;` после флагов reload. Покрывает world reload (snapshot load, preset change).
  - `AppUpdate.cpp::ToggleTaa handler`: `render->taaHistoryValid = false` на каждый toggle (в обе стороны). Новые jitter-projection начинает с чистого листа.
  - Swapchain resize: уже было `render->taaHistoryValid = false` в `VulkanSwapchain.cpp` prior commit (`98fb391`).
  - **Не invalidate:** pause toggle (нет изменения геометрии), voxel edit (sub-frame изменение, TAA depth-reproject handles).

**Verification:**
- `cmake --build build/linux-clang-debug --target ProjectV --parallel 8` — green
- `ctest --test-dir build/linux-clang-debug --output-on-failure -C Debug` — 1/1 passed (ProjectVTests; ProjectVAssetTests Not Run — pre-existing from asset-pipeline M1)
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `cam -25 19 25 look 0.62 -0.48 -0.62` с `PROJECTV_ENABLE_VALIDATION=ON` — 6/6 captures (FINAL SHDW CSM CTSH AOCC LOCL), 0 VUIDs / 0 errors. Sidecar содержит `taa_enabled=0`, `taa_jitter_x=0.000000`, `taa_jitter_y=0.000000`, `taa_blend=0.100000`, `taa_history_valid=0`.
- TAA-off path (по умолчанию) остаётся byte-equivalent к pre-A1 — ни в одном capture нет regression.

**A2 next steps (closed `2026-06-11`):**
1. ✅ Flip `core/Types.hpp::RenderState.taaEnabled` default: `false → true`.
2. ✅ Build + ctest + smoke с `taaEnabled=true` (FINAL + SHDW CSM CTSH AOCC LOCL).
3. ✅ Vision-verify anti-jitter: captures clean, `taa_history_valid=1` after warmup.
4. ✅ SPIR-V search path fix (`parent_path()` → `".."`) — `SDL_GetBasePath()` returns trailing separator;
   `parent_path()` only strips empty trailing string, not the actual `bin/` directory, so `bin/src/file.spv`
   was constructed instead of `src/file.spv`. `".."` works on all platforms regardless of trailing separator.
5. ⏳ `agent/decisions.md` §18 TAA contract entry — deferred to next session with TAA tuning.

## 10.16 TAA tuning ladder + RenderDoc markers landed (`2026-06-12`)

Блок 1 / 1.4 + Блок 5 / 5.1 + Блок 6 / 6.x все закрыты в этой сессии. Build green на `linux-clang-debug`, `ProjectVTests` 1/1. Code state живёт в working tree, коммиты pending serialization с параллельной `session-2026-06-11-asset-pipeline-m0-m5` M4.

**TAA tuning ladder contract (1.4):**
- 5 new `InputAction` enum entries + биндинги в `src/app/InputActions.cpp`: `DecreaseTaaJitterScale` (SDL_SCANCODE_SEMICOLON), `IncreaseTaaJitterScale` (SDL_SCANCODE_APOSTROPHE), `DecreaseTaaBlend` (SDL_SCANCODE_MINUS), `IncreaseTaaBlend` (SDL_SCANCODE_EQUALS), `CycleTaaNeighbourhoodRadius` (SDL_SCANCODE_COMMA), `InvalidateTaaHistory` (SDL_SCANCODE_PERIOD). Оригинальный план `J`/`M`/`K`/`L` не реализуем — `J`/`M`/`K` уже заняты (walk auto-jump, pick material, exposure inc). Левая рука держит WASD/movement, правая — все 6 новых keys в одном кластере.
- 5 new handlers в `AppUpdate.cpp` (все `*->taaHistoryValid = false` на change, кроме InvalidateTaaHistory который только это и делает). `CycleTaaNeighbourhoodRadius` цикл через `std::array<int32_t, 4>{1, 3, 5, 7}` через `std::find` + индексная арифметика, ищет текущее значение; если не найдено — defaults к `1`.
- `taaJitterScale` (RenderState + DebugStats, default `1.0`, clamp `[0, 2]`, step `0.25`) — multiplier на `Halton(2,3)` output в `FramePreparation.cpp`. `0.0` freezes projection jitter, `1.0` matches pre-ladder, `2.0` full-pixel wander.
- `taaBlend` остался как был, default `0.10`, step `0.05`, clamp `[0, 1]`.
- `taaNeighbourhoodRadius` (int32_t, default `1`, cycle через `1/3/5/7`) — `1` = original 3×3 (`-1, 0, +1`); `3` = 7×7; `5` = 11×11; `7` = 15×15.
- **Shader contract change**: `VoxelSceneLighting::taaHistoryParams` `.w` slot был `reserved`, стал `neighbourhoodRadius`. Byte layout **не изменился** (всё ещё `vec4` на offset 592, см. `static_assert` в `voxel/VoxelMaterials.hpp:145`). Только `taa_resolve.frag` reads `.w`; 3 other shader TUs (`voxel.frag`, `voxel_shadow.vert`, `voxel_mesh.comp`) объявляют поле для std430 layout, но не используют — у них только comment update. `taa_resolve.frag` clamp'ит radius в `[1, 7]` и snap'ит к odd values через тернарный каскад: `(r >= 7) ? 7 : (r >= 5) ? 5 : (r >= 3) ? 3 : 1`. Это держит loop bound в safe GLSL range и предотвращает undefined behavior на stale values.
- DebugHud detailed HUD line теперь: `TAA %s JIT %.2f %.2f JSC %.2f BLND %.2f NHOOD %dx%d HIST %s` (раньше было без `JSC` и `NHOOD`). Helper lines в detailed mode добавили 2 строки: `T TAA  ;' JIT  -= BLND` и `, NHOOD  . INVHIST`. Normal mode без TAA keys (только power-user).
- ScreenshotCapture sidecar добавил `taa_jitter_scale` (после `taa_jitter_y`) и `taa_neighbourhood_radius` (после `taa_blend`). Существующие keys (`taa_enabled/jitter_x/jitter_y/blend/history_valid/clamp_color_space`) сохранены.

**RenderDoc markers contract (5.1):**
- `profiling::ScopedGpuDebugLabel` RAII в `src/debug/ProfilingGpu.hpp` — begin в конструкторе, end в деструкторе. Gated на `PROJECTV_ENABLE_RENDERDOC_MARKERS` (CMake option, Debug default ON, `linux-clang-debug` preset OFF). 2 macros: `PV_PROFILE_GPU_LABEL(cmd, name)` и `PV_PROFILE_GPU_LABEL_COLOR(cmd, name, r, g, b, a)`. Использует `__COUNTER__` для уникальных identifier'ов (несколько labels в одном scope не warning'ят).
- Function pointers `vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT` грузятся volk'ом автоматически (extension `VK_EXT_debug_utils` enabled unconditionally в `VulkanBootstrap.cpp:549`, volk's `volkLoadInstance` + `volkLoadDevice` подхватывают).
- Hot sites: `RecordShadowCommands` ("Shadow Pass"), `RecordVoxelMeshingCommands` ("Voxel Meshing"), `RecordGraphicsCommands` ("Graphics Pass"), TAA resolve section в RecordGraphicsCommands ("TAA Resolve" + color 0.20/0.65/1.00 — distinct blue), `RecordDebugOverlayCommands` ("Debug Overlay"), `RecordDebugHudCommands` ("Debug HUD").
- Pattern следует существующему `PV_PROFILE_GPU_ZONE` (Tracy VkZone), но обёрнут в RAII. Trivial для добавления на новые pass'ы.

**VMA + glm fix во время сессии (build unblocking, не отдельный пункт плана):**
- Root cause: asset-pipeline сессия добавила `#include "asset/MeshGpuResources.hpp"` в `core/Types.hpp:5` (M4 work). Транзитивно тянет `MeshBaker.hpp` → `AssetLoader.hpp` → `<glm/glm.hpp>`. glm находится в `external/glm/`, но `ProjectVTests` target не линковал `glm` (только `volk/fmt/flecs/Jolt/SDL3/VulkanMemoryAllocator`), поэтому INTERFACE include path не пропагировался. Build упал с `'glm/glm.hpp' file not found` на 8+ TUs.
- Дополнительно: `VulkanBootstrap.cpp` имеет `vmaImportVulkanFunctionsFromVolk()` (real VMA API, line 755). VMA's header `vk_mem_alloc.h` объявляет эту функцию только при `#ifdef VOLK_HEADER_VERSION`. `volk.h` шёл в `core/Types.hpp:14` — **после** `#include "asset/MeshGpuResources.hpp"` (line 5), значит VMA header обработался без `VOLK_HEADER_VERSION` и `vmaImportVulkanFunctionsFromVolk` декларировался как no-op stub. Asset-pipeline пытался фиксить в `VulkanBootstrap.cpp:13` (`#include "volk.h"` после `core/Types.hpp`), но это уже поздно: `VulkanBootstrap.hpp` подключает `core/Types.hpp` на строке 1, VMA обработался.
- Fix: перенёс `#include "volk.h"` на самый верх `core/Types.hpp` (до всех VMA-touching headers). Удалил дубликат на старом месте. 1 строка в `tests/CMakeLists.txt` — добавил `glm` в `ProjectVTests` link.
- Working rule: **when a header is added to a shared file like `core/Types.hpp` (which includes VMA via `MeshGpuResources.hpp` etc.), the project's volk include must come first.** The order is volk.h → SDL3.h → project headers → VMA transitively. If future modules add new VMA-touching headers to `core/Types.hpp`, volk.h position is preserved by the existing top-of-file placement.
- Working rule: **when a target adds asset-pipeline code that pulls in glm (or any header-only dep with INTERFACE include dirs), all sibling targets that include the same shared header must also link the new dep.** `ProjectVTests` was the one that broke first because it has the smallest link line; the fix is to add `glm` (1 line) — not to add glm to a global INTERFACE option, which would also drag it into targets that don't need it.

## 10.19 M5.2 color-distance rejection threshold bump + model pipeline dual-MRT fix (`2026-06-12`)

Два последовательных фикса, оба преследуют один визуальный симптом: "модель невидима с TAA on, half in blocks".

**Фикс 1: `kTaaColorDistanceRejectionThreshold` 0.20 → 0.40 в `src/shaders/taa_resolve.frag:79`.** Euclidean distance от current sample до neighborhood centroid в YCoCg space. `model.frag:62-67` 4×4 procedural UV checker даёт два tint-варианта после ambient + direct-sun: yellow `vec3(0.85, 0.62, 0.38)` × albedo → YCoCg distance ≈ 0.27 (проходит rejection), blue `vec3(0.60, 0.55, 0.45)` × albedo → distance ≈ 0.16 (НЕ проходит — clamped в voxel range, invisible). 0.40 ловит оба. False-positive risk bounded: voxel surfaces обычно в пределах 0.05 YCoCg от своего 3×3 mean. Build green, ctest 6/6, SPV скопирован в `bin/` per §10.16 working rule.

**Фикс 2: model pipeline dual-MRT attachment declaration в `src/asset/ModelPass.cpp:200-224`.** **Это и был настоящий root cause невидимости с TAA on.** `ModelPass.cpp:202` (pre-fix) объявлял `VkPipelineRenderingCreateInfo.colorAttachmentCount = 1` с одним format (swapchain). Но `model.frag:33` для TAA-on пишет в `layout(location = 1) out vec4 outSceneColor` — TAA scene color. Main pass `vkCmdBeginRendering` (Renderer.cpp:735) имеет 2 attachments (Location 0 = swapchain, Location 1 = TAA scene color). Model pipeline объявлял только 1 → write в Location 1 — undefined behavior. `VK_KHR_dynamic_rendering_unused_attachments` позволяет rendering иметь БОЛЬШЕ attachments чем pipeline, но не наоборот. Validation layers не стоят, драйвер silently дропал write → `taaSceneColorTarget` оставался пустым в model pixels → resolve pass сэмплил пустоту → модель невидима несмотря на правильный threshold.

Фикс: model pipeline теперь объявляет 2 attachments через `const VkFormat modelColorAttachmentFormats[2] = { colorFormat, projectv::taa::kTaaSceneColorFormat };` (последний — `B10G11R11_UFLOAT_PACK32` per TAA-agent 1.7 centralization в `TaaRenderTargets.hpp:52`). `kTaaSceneColorFormat` consumed also в `TaaRenderTargets.cpp:86` (image allocation) и `VulkanGraphicsPipeline.cpp:1794` (main graphics pipeline declaration) — single source of truth, нельзя drift'нуть.

**Иерархия фиксов:** фикс 1 (threshold) был необходим для partial-visibility symptom (yellow tint 4×4 проходил, blue нет). Фикс 2 (dual-MRT) — для полной невидимости с TAA on. Оба нужны: без фикса 2 модель вообще не пишется в scene color target независимо от rejection threshold. Без фикса 1 часть model pixels clamped даже с dual-MRT write.

**Working rules:**
- Каждый Vulkan pipeline, используемый в `vkCmdBeginRendering(...)` с N attachments, должен объявлять все N в `VkPipelineRenderingCreateInfo::pColorAttachmentFormats`. Иначе write в undeclared attachment — undefined. `VK_KHR_dynamic_rendering_unused_attachments` идёт только в одну сторону (rendering ≥ pipeline).
- `kTaaSceneColorFormat` — single source of truth для TAA offscreen color format. Не хардкодить `R16G16B16A16_SFLOAT` или `B10G11R11_UFLOAT_PACK32` в pipeline declarations.
- M5.2 threshold — lever для "маленькая surface окружённая большой different surface". Бампить по тому же принципу, если будущие materials не проходят rejection.

## 10.20 Model procedural UV checker → triplanar on `inWorldPosition` (`2026-06-12`)

`src/shaders/model.frag:50-90` — заменил `inUv`-based 4×4 procedural UV checker на **triplanar projection on `inWorldPosition`**, picked by dominant face normal axis. Build green, ctest 6/6, `model.frag.spv` + `model.frag.taa_on.spv` скопированы в `bin/` per §10.16.

**Почему.** `box.glb` (default model-pipeline test fixture, 1664 B) **не имеет `TEXCOORD_0` accessor**. `model.frag:62` pre-fix использовал `const vec2 checkerUv = floor(inUv * 4.0);` — но `inUv` defaults to `(0, 0)` на всех face → `floor((0,0) * 4) = (0,0)` → `checkerMask = 0` всегда → один tint на весь куб → uniform beige. Symptom: "block наполовину в текстурах" — model visible (после M5.2 dual-MRT fix от TAA-agent), но procedural 4×4 UV checker pattern не виден, потому что UV stream пустой. Pre-fix код явно признавал это в comment: "If the UV stream is missing (e.g. `box.glb` has no TEXCOORD_0 accessor), the input defaults to (0, 0) and the whole box is uniform." Оператор явно попросил фикс: "Теперь чини то, что она наполовину в текстурах."

**Фикс — triplanar projection:**
```glsl
const vec3 absNormal = abs(normal);
vec2 checkerUv;
if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
    // Top/bottom face: project onto XZ.
    checkerUv = inWorldPosition.xz;
} else if (absNormal.x >= absNormal.z) {
    // Left/right face: project onto ZY.
    checkerUv = inWorldPosition.zy;
} else {
    // Front/back face: project onto XY.
    checkerUv = inWorldPosition.xy;
}
const float checkerMask = mod(checkerUv.x + checkerUv.y, 2.0);
```

**Trade-off vs per-face UVs.** Pre-fix UV-based: каждая face имеет свой 0..1 range → checker 4×4 на каждой face независимо. Post-fix triplanar: world-space coordinate shared across faces — две смежные face'ы на одной wall (например, top + front) показывают **continuation** одного pattern через edge, не два независимых checker'а. Visually slightly different, но "block has a visible checker on every face regardless of which fixture operator loads" contract выполнен.

**Когда станет UV-friendly path again:** M6+ заменит triplanar на real `sampler2D baseColor` + per-face UVs (TODO §5 / handoff M6+). Тогда triplanar revertнется.

**Working rules:**
- Procedural patterns / dummy textures в shaders, которые зависят от UV, нужно либо (a) проверять на fixtures БЕЗ UV (`box.glb` default) либо (b) использовать world-space / triplanar / object-space координаты как UV-free fallback. `box.glb` test fixture — de-facto minimum-fixture для model pass; всё, что в нём не работает, сломает visual verify.
- "Half in textures" / "uniform color" / "model looks like single tint" на тестовом fixture → почти всегда UV-less mesh, не shader bug. Triplanar / object-space projection — robust fallback.

## 10.21 TAA Блок 1 / 1.5 — per-layer (CTSH/AOCC/LOCL) anti-flicker via mini-TAA history attachment (`2026-06-12`)

**Per-layer temporal history: TAA color blend фиксит основное
мерцание, но per-frame jitter в lighting (CTSH/AOCC/LOCL) всё ещё
виден как flicker на voxel surfaces.** 1.5 вводит second temporal
filter, заточенный на per-layer lighting values, не на scene color.

**Архитектура — 3-й MRT attachment + 6-й graphics binding:**

`src/shaders/voxel.frag` пишет `vec4 outLayerMask` (Location 2, R =
sun contact shadow visibility, G = AOCC, B = local-point-light
visibility, A = 1.0) в новый 3-й color attachment формата
`R8G8B8A8_UNORM`. На каждом кадре `Renderer.cpp` per-frame
`vkCmdCopyImage` копирует `taaLayerSceneColorTarget` →
`taaLayerHistoryColorTarget` (тот же формат, `vkCmdCopyImage` spec
§7.1.1: src и dst formats identical). Следующий кадр fragment shader
сэмплит `sampler2D layerHistory` (binding 6, graphics descriptor
set) и применяет `mix(rawCurrent, history, blend=0.4)` к AOCC + LOCL
в main lighting.

**CTSH пока не smoothed** в main lighting (только write to history).
Причина: `ComputeSunShadowSample` объединяет cascade shadow и
contact shadow в одно значение, и blend между ними даст wrong
результат (cascade shadow — viewpoint-dependent, contact shadow —
viewpoint-independent; mix → wrong direction). Refactor для
separation — deferred, см. "Working rules / deferred work" ниже.

**Blend-at-read, не blend-at-write** — uniform contribution per
frame, нет exponential-decay artefacts от stale histories. Если бы
blend был на write side (history ← mix(raw, history, blend)), то
каждый frame exponential decay old samples (history → 0), что
даёт wrong weighting на sustained motion. На read side (output ←
mix(raw, history, blend)) — каждая history sample имеет weight
`blend`, каждая raw sample имеет weight `1-blend`, geometric mean
через N frames.

**Component budget на RTX 3060** = 8 vec4 outputs per fragment
(`maxFragmentOutputComponents`). TAA-off path: `outColor (4) +
outLayerMask (4) = 8`. TAA-on path: `outSceneColor (4) + outLayerMask
(4) = 8`. Packing всех 3 layer values (CTSH, AOCC, LOCL) в один
`vec4` — единственный способ уложиться в budget. 3-й attachment
slot в pipeline declaration bound в обоих path'ах, но per-frame
`VkRenderingAttachmentInfo::imageView` = `VK_NULL_HANDLE` на unused
slot — `dynamicRenderingUnusedAttachments` allows.

**`VoxelSceneLighting::taaLayerHistoryParams` vec4** (texelX,
texelY, neighbourhoodRadius, blendFactor) — packed в существующий
SSBO на offset 608 (после `taaHistoryParams` 16 B + 16 B padding),
total struct 624 B (rounded up from 616 → multiple of 16 B per
std430 layout rules). `static_assert(sizeof(VoxelSceneLighting) ==
624)` + `static_assert(offsetof(VoxelSceneLighting, taaLayerHistory
Params) == 608)` enforces byte layout invariance — same pattern что
1.2 (`taaPrevViewProjectionMatrix` в push-constants) и 1.4
(`taaNeighbourhoodRadius` в `taaHistoryParams.w`).

**Centralized format constant** (та же pattern что 1.7):
`inline constexpr VkFormat kTaaLayerHistoryColorFormat =
VK_FORMAT_R8G8B8A8_UNORM` в `projectv::taa` namespace
(`src/render/TaaRenderTargets.hpp`). Consumed by:
- `CreateOrRecreateTaaRenderTargets` (image allocation для обоих
  layer scene color и layer history)
- `VulkanGraphicsPipeline.cpp` (`pColorAttachmentFormats[2]`
  declaration)

**3 pre-existing bug fixes включены в feat commit `237ab76`:**

1. **3rd-MRT binding fix** (`Renderer.cpp:735` pre-fix имел
   `colorAttachmentCount = 2` в `vkCmdBeginRendering` пока pipeline
   объявлял 3 attachments). Driver silently дропал write в
   `outLayerMask` (VUID-VkGraphicsPipelineCreateInfo-renderPass-06055
   не fired'ил без validation layers, но rendering > pipeline strict
   violation — write в undeclared attachment = undefined behavior).
   Symptom: layer mask всегда чёрный → blend с history → вся сцена
   под-flicker'ит. Fix: `colorAttachmentCount = 3`.

2. **TAA reprojection texel-size patch** (`SceneResources.cpp::
   RefreshSceneLightingBuffer` pre-fix не вызывал
   `BuildTaaHistoryParams`, хотя функция была defined). `taaHistory
   Params.xy` оставались `(0, 0)`, `taa_resolve.frag` reprojection
   branch тихо фолбэчился к current-pixel-only — **TAA was de facto
   disabled**, и весь предыдущий "TAA" perf benefit был бы
   мнимым. 1.5 patch'ил это как pre-existing bug, потому что
   layer history texel size populate использует ту же логику и
   должен быть consistent.

3. **`voxel.frag.taa_on.spv` refresh.** Incremental `cmake
   --build` не копирует свежие `.spv` в `bin/` (working rule из
   §10.16). 1.5 добавил 1.5-specific шейдер code, и без
   `voxel.frag.taa_on.spv` refresh в `bin/` runtime упал бы на
   resolve pass.

**3 validation fixes в `4d8b4c8`** (caught by
`PROJECTV_ENABLE_VALIDATION=ON`):

1. **VUID-VkImageCreateInfo-initialLayout-00993:** `initialLayout`
   для layer scene color + history = `UNDEFINED`, не
   `SHADER_READ_ONLY`. Pre-fix имел `SHADER_READ_ONLY_OPTIMAL` —
   forbidden. The first-frame per-frame transition в `Renderer.cpp`
   is the only way to get image into read layout.

2. **VUID-VkGraphicsPipelineCreateInfo-renderPass-06055:**
   `pColorBlendState->attachmentCount` = 3, не 2. Pre-fix имел 2
   (counting only `outColor`/`outSceneColor` и ignored the new
   `outLayerMask` attachment).

3. **VUID-VkDescriptorPool-size-...:** Graphics descriptor pool
   combined samplers grew `2u → 4u` (1.5 added binding 6 layer
   history, plus existing binding 5 shadow sampler, `MAX_FRAMES_IN_
   FLIGHT = 2` → 2 × 2 = 4). Pre-fix имел `2u`, новый binding не
   помещался.

Plus layer history transitions используют layout trackers
(`taaLayerHistoryColorCurrentLayout`) как `oldLayout` вместо
hardcoded `SHADER_READ_ONLY` (VUID-VkImageMemoryBarrier2-oldLayout-
01197, актуально на subsequent frames где actual layout уже
`SHADER_READ_ONLY`).

**Build / test / smoke (`2026-06-12`):**

- `cmake --build build/linux-clang-debug --target ProjectV
  ProjectVTests ProjectVAssetTests ProjectVMeshBakerTests
  ProjectVDracoTests ProjectVFrustumCullingTests
  ProjectVBoxUvFixtureTests --parallel 8` — green, 1 pre-existing
  warning (`DebugHud.cpp:600` LOCL `%.0f` for bool, не моя).
- `ctest --test-dir build/linux-clang-debug --output-on-failure` —
  6/6 passed (1.50 s).
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh` на `VoxelLab` ref
  shot с `PROJECTV_ENABLE_VALIDATION=OFF` (Linux preset = ON, layers
  package not installed, smoke script defaults to OFF): 6/6 captures
  под `build/linux-clang-debug/lookdev-captures/20260612-1.5-final/`.
  Sidecar: `taa_history_valid=1`, `taa_layer_history_valid=1`,
  `taa_layer_blend_factor=0.400000`, `taa_camera_cut_count=0`,
  `taa_scene_color_format=B10G11R11_UFLOAT`.
- **Validation verify (post `4d8b4c8`):** `PROJECTV_ENABLE_VALIDATION
  =ON build/linux-clang-debug/bin/ProjectV` — 0 VUIDs, 0 errors,
  scene renders correctly.
- Vision review of FINAL view: vibrant VoxelLab, opaque anchor,
  checker floor, FPS **127.3** (vs 1.7 baseline 110.6, single-run
  variance likely explains the difference).

**Working rules to inherit:**

- **3rd-MRT binding = `colorAttachmentCount` в `vkCmdBeginRendering`
  matches `pColorAttachmentFormats[2]` + `pColorBlendState->
  attachmentCount` + `pColorAttachmentCount` declaration in
  pipeline.** Any mismatch = silent write drop on driver side или
  validation error. `VUID-VkGraphicsPipelineCreateInfo-renderPass-
  06055` — even with validation layers on, easy to miss in a
  refactor that adds a 3rd attachment. **Pre-existing bug fix
  важнее** чем кажется: 1.5 commit добавил 3rd attachment в
  pipeline, но оригинальный commit не обновил
  `vkCmdBeginRendering` — driver silently dropped writes. This is
  the kind of "3 steps to add an MRT" checklist that needs to
  stay synchronized.

- **Layer history texel size populate matches scene color texel size
  populate.** Both derive from `renderExtent`, both go through
  `BuildTaaHistoryParams` / `BuildTaaLayerHistoryParams`. If one is
  populated and the other not, the two TAA filters work at
  different "pixel densities" — visible as scaling artefact at
  boundaries.

- **Centralized format constants** (`kTaaLayerHistoryColorFormat`,
  `kTaaSceneColorFormat`) — same pattern что 1.7. Inline constexpr
  в header, 2 consumers, compiler-enforced consistency.

- **Blend-at-read, not blend-at-write** for temporal filters
  applied to GPU-computed values (lighting, AO, etc.). Avoids
  exponential decay artefacts на stale histories. Geometric mean
  weighting.

- **Descriptor pool sizes must grow with each new combined image
  sampler.** `MAX_FRAMES_IN_FLIGHT × combined_samplers_per_frame`.
  1.5 added binding 6 → pool grew from 2 → 4. Future binding
  additions (7, 8, ...) need proportional pool growth.

- **`initialLayout` must be `UNDEFINED` / `PREINITIALIZED` /
  `ZERO_INITIALIZED`** per VUID-VkImageCreateInfo-initialLayout-
  00993. Can't use `SHADER_READ_ONLY_OPTIMAL` directly. First-frame
  per-frame transition in `Renderer.cpp` is the only way to get
  image into read layout.

- **Per-frame transitions use layout tracker as `oldLayout`, not
  hardcoded.** Actual GPU state may be `COLOR_ATTACHMENT_OPTIMAL`
  (after rendering pass) или `SHADER_READ_ONLY_OPTIMAL` (after
  copy block). Hardcoding wrong `oldLayout` triggers
  VUID-VkImageMemoryBarrier2-oldLayout-01197.

- **Voxel pass writes to layer scene color в обоих TAA-on и TAA-off
  paths.** Transition (UNDEFINED → COLOR_ATTACHMENT) runs
  unconditionally, не inside `if (taaOn)`. Otherwise the per-frame
  `vkCmdCopyImage` would dangle на target в non-read layout when
  TAA-off (но `taaLayerHistoryValid=false` предотвращает actual
  issue, layer history still invalid).

- **Incremental `cmake --build` не копирует свежие `.spv` в `bin/`.**
  After shader edits: `cp build/.../src/voxel.frag.taa_on.spv
  build/.../bin/voxel.frag.taa_on.spv` (или force relink of
  `ProjectV` target). Working rule из §10.16.

**Deferred work (1.5 follow-ups):**

- **CTSH blending.** Нужен refactor `ComputeSunShadowSample` —
  separate cascade shadow term от contact shadow term, blend
  contact term с history, **не** blend cascade term (cascade
  меняется с viewpoint, history reprojection будет wrong). В
  cascade transition зонах contact blend даст wrong
  "ghost contact shadow" — visual artefact, лучше skip blending
  чем blend wrongly.

- **Layer history формат на half-float для большего dynamic range.**
  `R8G8B8A8_UNORM` — 8 bits per channel, signed-quantity trouble на
  high-contrast areas. `R16G16B16A16_SFLOAT` — 2x bandwidth, но
  бесценно для HDR contact shadows. Дефолт — `R8G8B8A8_UNORM`
  пока не доказана необходимость.

- **Mip-mapped layer history** для cheaper bilateral filtering
  (avoid neighborhood sampling bandwidth). Complex, defer до 1.8
  quality tier (where mip level становится part of quality
  parameter).

**Cross-refs:** `TODO.md` Блок 1 (1.5 closed, this commit),
`agent/decisions.md` §21 (TAA per-layer history contract, this
session), `agent/status.md` §12 (in-progress session snapshot,
this commit), `agent/active-sessions.md` session-2026-06-12-taa-
quality-1.5 (closed), `agent/memory.md` §10.17 (1.2+1.3 — pre-
existing `taaHistoryParams` patch was originally 1.5 work;
relocated to 1.5 commit because of texel-size invariants),
§10.18 (1.7 — same constant centralization pattern,
`kTaaSceneColorFormat`).

## 10.19 — Two-level chunk visibility cache + 5.2 gizmos + 5.3 benchmark automation (`2026-06-12`)

Closed in `session-2026-06-12-lowlevel-perf-tooling`. Three
independent slices landing in the same session for atomic
documentation:

**Two-level chunk visibility cache:**

- `RenderState::chunkVisibilityCache` is invalidated by ANY
  of: hash mismatch, `chunkDescriptorCount` change,
  `sceneVoxelPayloadVersion` change. The hash itself folds
  all 3 inputs but the explicit checks in the if-condition
  are belt-and-suspenders.
- `projectv::visibility_cache::ComputeVisibilityCacheHash`
  uses splitmix64 with constants
  `0x9E3779B185EBCA87`, `0xC2B2AE3D27D4EB4F`,
  `0x165667B19E3779F9`, `0x94D049BB133111EB`,
  `0xD1342543DE82EF95`, `0xB45BCA9F4D2D9B33`,
  `0x27D4EB2F165667C5`, `0x9C2A8E3F4D2D9B3B` and a
  final 3-step avalanche. The exact constants don't
  matter for correctness — only that a 1-bit change in
  any input flips ~half the hash bits.
- Cache size at 300 chunks: opaque 300*16 + shadow
  300*4*16 + transparent 300*16 = ~24 KB. Well under any
  L1. At 1000 chunks it's ~80 KB, still fits in L2.
- `RebuildChunkVisibilityAndFillCache` writes to BOTH the
  per-frame mapped GPU buffer AND the cache in the same
  per-chunk pass. No extra copy step on the cold path.
  The two writes share the per-chunk math, so we don't
  pay an extra pass on miss-heavy workloads.
- Profiler plots: `Visible Chunks` / `Culled Chunks` are
  populated on both hit and miss (read from cache on hit,
  computed on miss). New `ChunkVisibilityCacheHits` plot
  tracks the consecutive-hit counter.
- Cache miss is the default first-frame state (`valid=false`).
  The first `UpdateSceneFrameChunkVisibility` call after
  world load or `FinalizeActiveVoxelWorldReload` always
  rebuilds.
- **Cross-session merge risk:** `core/Types.hpp` is the
  contention point with the asset-pipeline session —
  their uncommitted changes also touch the tail of
  `InputAction::Count` and add a new field to
  `AppState`. My additions are *all at the tail* of their
  respective containers, so field offsets don't shift.
  Manual merge if the other agent lands between my
  commits.

**5.2 debug gizmos:**

- `BuildDebugOverlayBoxes` signature: trailing
  `CameraState camera = CameraState{}` and `RenderState
  render = RenderState{}` default-valued params. The 2
  existing tests at `tests/VoxelWorldTests.cpp:7302` and
  `:7348` keep their 4-arg call shape and stay green.
  Do not remove the default-valued trailing params
  without first updating those two tests.
- Cascade split plane boxes are world-axis-aligned (not
  camera-aligned) because `DebugOverlayBox` is `Int3
  min/maxExclusive`. The XZ footprint uses each
  cascade's `orthoWidths/Heights`; Y is a thin slab
  around the camera-relative Y. Four distinct hues
  (red/orange/cyan/magenta) so cascades 0-3 are
  distinguishable.
- Cursor hit normal shaft emits only the voxels *beyond*
  the hit voxel (≤2 boxes), so it reads as a "next to
  selection" arrow rather than overlapping the yellow
  selection box. `hitNormal` is always ±1 in one axis
  (guaranteed by `VoxelRaycast`); zero-norm is a no-op.
- `L` was the only free letter per `status.md §9` (the
  TAA tuning-ladder footnote explicitly reserved it).
  `Z` was unused. Both follow the same hotkey-on /
  `hudVisible`-on emission contract that `showChunkBounds`
  / `showDirtyChunkOverlay` already use.
- The two new `DebugState` flags are gated on
  `hudVisible` (the existing pattern). If a future
  feature wants to render gizmos without the HUD, move
  the `hudVisible` check out of the
  `BuildDebugOverlayBoxes` early-return.

**5.3 benchmark automation:**

- `PROJECTV_BENCHMARK_FRAMES` is the master gate; unset
  = inactive (no overhead).
- `PROJECTV_BENCHMARK_WARMUP_FRAMES` (default 30) frames
  are discarded before measurement starts. Without
  warmup, the first ~30 frames include Vulkan pipeline
  compile, VMA pool warmup, SPIR-V load, and the first
  chunk meshing dispatch — none of which represent
  steady-state cost.
- `PROJECTV_BENCHMARK_LOG_EVERY` (default 60) controls
  progress log frequency.
- `PROJECTV_BENCHMARK_QUIT=1` returns `SDL_APP_SUCCESS`
  from `SDL_AppIterate` after the last measured frame.
- `minFrameSeconds` uses a sentinel `1e30f` initial
  value so the first valid frame always wins;
  `maxFrameSeconds` uses `0.0f`. The mean is
  `totalFrameSeconds / framesRendered`.
- The pattern is intentionally symmetrical with
  `LookDevCaptureAutomationState` so a future "all
  automation types" refactor can move them behind a
  single `AutomationRegistry`.
- Env vars are read **once** in `SDL_AppInit`. State is
  immutable after that. To re-arm the benchmark, restart
  the process. If a future feature wants mid-session
  re-arm, the env-reader should be split out of
  `ConfigureBenchmarkAutomationFromEnvironment` and
  called from a hotkey.

**Build / ctest state (final):**

- `cmake --build build/linux-clang-debug --target
  ProjectV ProjectVTests --parallel 8` — green.
- 901 VMA `-Wnullability-completeness` warnings
  (pre-existing, not mine).
- `ctest` 6/6 (1.47 s, baseline 1.45 s — within noise).

**Cross-refs:** `decisions.md §22` (two-level cache
contract), `decisions.md §23` (5.2 gizmo contract),
`decisions.md §24` (5.3 benchmark contract),
`TODO.md §4` World/Render/Tooling + Gameplay/Debug
(closed), `agent/status.md §13` (this session's
snapshot), `agent/active-sessions.md`
session-2026-06-12-lowlevel-perf-tooling (closed).

## 10.22 Greedy meshing (4.1) landed (`2026-06-12`)

Closed in `session-2026-06-12-greedy-meshing`. **One
foundation commit** (operator "давай A1" after 3-slice
lowlevel session) covers PackedFace extension, vertex
shader scale helper, and the per-axis greedy pass in
`voxel_mesh.comp`. Per-axis (vs single triple-nested
voxel loop) даёт clean kill switch + parallelizable
future work, без роста dispatch count per frame.

**`PackedFace` 12 → 16 bytes — single source of truth
pattern (`static_assert`-enforced).**

- C++ `PackedSceneVoxelFace` (`core/Types.hpp:47-65`):
  added `uint32_t packedExtents = 0`, updated 4
  `static_assert` (`sizeof == 16`, 4×`offsetof`).
- GLSL `PackedFace` mirror in 3 shaders
  (`voxel_mesh.comp`, `voxel.vert`, `voxel_shadow.vert`):
  same 4-uint layout.
- `SceneResources.cpp:927` uses
  `sizeof(PackedSceneVoxelFace) * count` for buffer
  allocation — auto-adapts to 16 bytes, **no manual
  change needed**. The `sizeof` operator + C++ struct is
  the single source of truth for the GPU buffer stride.
- `packedExtents` packs `(width, height, _, _)` 8 bits
  each. `width=1, height=1` (default) = unit quad
  (pre-A1 behavior, no merge). Larger = merged quad.
- **Working rule для future PackedFace edits:** always
  update all 3 GLSL mirrors AND the C++ struct in the
  same change. The `static_assert` block в
  `core/Types.hpp` will fail compile if they drift.

**`kMaxChunkExtentForGreedy = 64` — buffer-driven
choice.**

- 64×64 plane = 4096 bits = 128 uints = 512 bytes per
  axis+direction pass.
- 6 passes per chunk × 512 bytes = 3KB stack-allocated
  local memory (`uint visited[64]` per GLSL local-array
  pattern). RTX 3060 has 256KB L2 → no cache pressure.
- Past 64, fallback to per-voxel (1×1 quads) — no crash,
  no merge benefit. PackedFace's 8-bit per-axis packing
  allows up to 256, но practical chunk size ≤ 64.
- **Working rule для future chunk-size bumps:** если a
  feature wants chunk > 64, raise `kMaxChunkExtentForGreedy`
  and verify 3KB per-chunk local-memory budget stays under
  GPU's spill threshold (typically 4KB register file +
  16KB+ local memory). Or switch to SSBO-backed bitmask.

**Vertex shader scale helper — `ApplyGreedyScale`.**

- For each face, maps in-plane channels to `(width, height)`:
  - face 0/1 (X±): in-plane = (Y, Z). `unit.y * width`,
    `unit.z * height`.
  - face 2/3 (Y±): in-plane = (X, Z). `unit.x * width`,
    `unit.z * height`.
  - face 4/5 (Z±): in-plane = (X, Y). `unit.x * width`,
    `unit.y * height`.
- Normal-axis channel stays 0/1 — face plane is
  `localVoxelCoord + normal_offset`, not multiplied.
- For unit quads the helper is no-op. **Critical:** не
  reorder `unitOffset` channels — the existing
  `GetFaceCornerOffset` 0/1 layout is the contract with
  the per-quad triangulation (`DecodeTriangleCornerIndex`).

**Greedy merge condition — `decisions.md §25`.**

- Same `cellMaterial` AND same `neighborIsAirOrGlass`
  set state. AO not part of merge (per-vertex AO
  disabled per `decisions.md §14` v2, `lightingData` no-op).
- Glass (`material == 1`) participates in greedy as
  usual — but `ShouldEmitVoxelFace(glass, glass) = false`
  per `decisions.md §13` means glass-on-glass faces не
  emit, no merge opportunity across glass boundaries.
- Fluid (`material == 2`) merges as usual — same opaque
  policy (`ShouldEmitVoxelFace(fluid, Air/Glass) = true`).

**Cross-chunk reads — `ReadVoxelMaterial` returns 0
(Air) for OOB.**

- Greedy pass seamlessly handles chunk boundaries
  because OOB neighbor = Air = exposed (for non-zero
  cell material). Worst case: chunk boundary quads merge
  with `neighborMaterial = 0` (Air) → standard "face
  toward outside world" behavior, matches pre-A1.
- **No chunk-coordination protocol needed** —
  independent dispatches produce consistent quads at
  boundaries. Cross-chunk greedy merge across the
  boundary plane is theoretically possible (чтобы убрать
  the duplicate face pair) but requires either a barrier
  или 2-pass greedy. Defer to a future "global greedy"
  optimization.

**`DrawCommand(6u, ...)` unchanged.**

- 1 quad = 2 triangles = 6 indices. Greedy reduces
  INSTANCE count (1 instance per merged quad, was 1 per
  voxel-face), vertex shader invocations drop
  proportionally. Worst case (all 1×1 quads) = identical
  to pre-A1.

**Build / ctest / smoke state (final, A1.0):**

- `cmake --build build/linux-clang-debug --target
  ProjectV ProjectVTests ProjectVAssetTests
  ProjectVMeshBakerTests ProjectVDracoTests
  ProjectVFrustumCullingTests ProjectVBoxUvFixtureTests
  --parallel 8` — green. 901 VMA
  `-Wnullability-completeness` warnings (pre-existing,
  not mine).
- `ctest` 6/6 (1.46 s, baseline 1.45-1.50 s — within
  noise).
- `tools/linux/Invoke-ProjectVRuntimeSmoke.sh
  --capture-dir
  build/linux-clang-debug/lookdev-captures/20260612-greedy-meshing-v1
  --views "FINAL" --warmup 5 --interval 1` — PASS. 1
  .bmp + 1 .txt sidecar, exit code 0. VoxelLab reference
  shot `cam -25 19 25 look 0.62 -0.48 -0.62` renders
  without crash, sidecar fully populated, BMP pixel
  distribution matches VoxelLab baseline (light gray for
  glass/floor + dark blue sky + near-white highlights +
  near-black shadows — no "uniform gray" holes that would
  indicate missing cells).

**Cross-refs:** `decisions.md §25` (greedy meshing
contract), `TODO.md §4` (greedy meshing closed) + §4.5
(perf budget context — vertex stage #1 bottleneck),
`agent/status.md §14` (this session's snapshot),
`agent/active-sessions.md`
session-2026-06-12-greedy-meshing (closed).


## 10.23 Frame-step / slow-motion landed (`2026-06-12`)

Live runtime debug controls for visual debugging. Additive, no TAA/meshing/render-pipeline impact. Implementation in `src/app/AppUpdate.cpp:600-650` (input handlers + accumulator override) and `src/app/InputActions.cpp:171-175` (4 `BindAction` calls).

**Working rules:**

- **4 `InputAction` entries (tail of enum, before `Count`):** `DecreaseTimeScale` (`SDL_SCANCODE_LEFTBRACKET` / `[`), `IncreaseTimeScale` (`SDL_SCANCODE_RIGHTBRACKET` / `]`), `StepSingleFrame` (`SDL_SCANCODE_BACKSLASH` / `\`), `ResetTimeScale` (`SDL_SCANCODE_GRAVE` / `` ` ``). The bracket and backslash / backtick keys have no glyph in the HUD font (only A-Z, 0-9, `.`, `-`, `:` per `DebugHud.cpp::GetGlyphRows`) — the helper panel spells them out as `TIMECTL DOWN UP` and `TIMESTEP STEP RESET 1X`.
- **`timeScale` ladder: `0`, `0.5`, `1.0`, `2.0`, `4.0`.** `[` halves with snap to `0` below `0.01`; `]` doubles with `timeScale <= 0` → `0.5` escape, clamped to `4.0`; `` ` `` resets to `1.0`. The snap thresholds exist so a half-step into `0.0078` doesn't crawl the sim unexpectedly.
- **`effectivePaused = simulation->paused && !frameStepRequestedNow`.** Three `simulation->paused` references in `AppUpdate.cpp::UpdateApp` switched to `effectivePaused` (lines 626 `cameraCanUpdate`, 656 accumulator gate, 666 while-loop condition, 716 paused+spectator camera tick). The `TogglePause` handler at line 333 is **unchanged** — `paused` and `timeScale` are independent runtime axes per `decisions.md §26`.
- **Time scale is applied after `ComputeFrameDeltaSeconds`** (line 638: `simulation->frameDeltaSeconds *= simulation->timeScale;`). The wall-clock `framesPerSecond` / `frameTimeMilliseconds` stats at lines 307-308 still report real-time even at `timeScale = 0`; input replay recording also records wall-clock delta (the `RecordInputReplayFrame` call at line 313 happens before the scaling).
- **Frame-step accumulator override at lines 645-655** sets `simulation->simulationAccumulatorSeconds = simulation->fixedSimulationDeltaSeconds` when `frameStepRequestedNow`, AFTER the `timeScale` multiplication, so a non-zero `timeScale` doesn't double-apply. The while loop runs exactly one iteration per `\` press.
- **Frame-step does NOT invalidate TAA history.** Unlike world reload / swapchain resize / TAA toggle, the `frameStepRequested` event does not touch `taaHistoryValid` or `taaLayerHistoryValid` — TAA's reprojection is per-frame and `\` is per-frame, so a single step appears as a single frame in the TAA history chain. The existing camera-cut detector (1.2) handles any visible-artifact edge case.
- **HUD surfaces:** `TIME x.xx` line always emitted (default `TIME 1.00`), one-frame `STEP` line only on the press frame. Both are after the `MODE / PAUSE / AIR` line in `BuildStatsLines` so the two pause-related runtime axes read as a group.

**Files touched:** `src/core/Types.hpp` (4 `InputAction` + 2 `SimulationState` + 2 `DebugStats`), `src/app/InputActions.cpp` (4 `BindAction`), `src/app/AppUpdate.cpp` (4 handlers + accumulator override + 3 `effectivePaused` refactors + 2 stats mirrors), `src/debug/DebugHud.cpp` (1 stats line + 2 helper lines).

**Test impact:** additive fields — existing `simulation.paused` tests at `tests/VoxelWorldTests.cpp:2211, 2541, 2570` continue to pass. `ctest 6/6` baseline preserved at `1.50s` wall clock on `linux-clang-debug`.

## 10.24 Per-pass CPU timings landed (`2026-06-12`)

CPU-side per-pass timing aggregation for visual debugging / TODO §4.5 perf-budget analysis. Foundation for follow-up perf work (halve-res AO/contact upscale, VRS, bloom) — operator can now see which sub-pass dominates a frame instead of guessing.

**Working rules:**

- **6 measured fields, 1 derived.** `shadowMs`, `meshingMs`, `graphicsMs`, `taaResolveMs`, `debugOverlayMs`, `debugHudMs` measured with `SDL_GetPerformanceCounter` via `ScopedPassTimer` RAII (in `Renderer.cpp` anonymous namespace). `otherMs` derived in `AppUpdate.cpp` as `frameTimeMs - graphicsMs`. `dirtyChunkRebuiltCount` snapshot of `frameRenderData.dirtyChunkCount` at the start of `RecordVoxelMeshingCommands` (so the value is what was requested, even on early return).
- **`ScopedPassTimer` placement.** Each `Record*Commands` function gets one `ScopedPassTimer timer(render.renderPassTimings.XxxMs);` at the very top, before the first `PV_PROFILE_ZONE_N` / `PV_PROFILE_GPU_LABEL`. The RAII destructor writes the ms value at function exit, including early-return paths. Without RAII, each `if (X == VK_NULL_HANDLE) return;` would need its own `writeTiming()` call site, and one missed call would silently leave the previous frame's stale number on the HUD.
- **Manual timer for the inlined TAA resolve block.** `RecordGraphicsCommands` line ~1153-1200: `taaResolveStartCounter = SDL_GetPerformanceCounter()` right after the `PV_PROFILE_GPU_LABEL_COLOR`, manual `endCounter - startCounter` conversion at the end of the block. The block is too small / too deeply nested for `ScopedPassTimer` (would need a helper struct), and the alternative — wrapping the whole `RecordGraphicsCommands` — would lose the sub-pass breakdown.
- **Sub-passes are subsets of `graphicsMs`.** `shadowMs + meshingMs + taaResolveMs + debugOverlayMs + debugHudMs` all happen inside `RecordGraphicsCommands`, so they are not additive to `graphicsMs`. The HUD shows `GFX` (total) and the breakdown lines below it; summing the breakdown double-counts the `RecordGraphicsCommands` body. This is intentional — `graphicsMs` is the "time spent in the renderer" total, the sub-passes are the breakdown within it.
- **HUD placement is detailed-only.** First iteration put the lines in the basic section (before `if (!detailedHudVisible) return`), which pushed both basic and detailed above the 65536-vertex test buffer cap and broke `detailedVertexCount > basicVertexCount`. Moved to detailed-only after the test failure; this is also semantically more correct (per-pass timings are diagnostic, not always-on). `kMaxStatsLineCount = 38` (was 36) to accommodate the 2 new lines.
- **Sidecar metadata split into 2 `fmt::format` calls.** `SaveScreenshotCaptureMetadata` already used 99 args in the main format string (the `fmt` 99-arg compile-time checker's hard limit). The 7 new per-pass keys + 1 count get a second `stream << fmt::format(...)` call concatenated to the same sidecar file. New keys (`render_pass_shadow_ms` through `render_pass_debug_hud_ms` + `render_pass_dirty_chunk_rebuilt_count`) at the end of the file, existing parsers unaffected (look for specific `key=value` substrings).
- **Production vs test buffer size.** Production uses `DEBUG_HUD_MAX_VERTEX_COUNT = 262144` (VMA-allocated, set in `core/Types.hpp:304`). The test harness uses 65536. The per-pass lines fit in production easily; in the test, the detailed HUD was already near the 65536 cap, which is why the lines have to live in the detailed-only section.
- **GPU-side timestamps are a follow-up, not a parallel implementation.** CPU-side accuracy is sufficient for the "where is my budget going" question. If the operator later wants to distinguish "CPU stalled in `vkCmdDraw`" from "GPU stalled in pipeline execution", add `vkCmdWriteTimestamp` queries inside each `Record*Commands` function; the per-pass struct already has the right shape to add a `*GpuMs` field next to the existing `*Ms`.

**Files touched:** `src/core/Types.hpp` (`RenderPassTimings` struct + `RenderState::renderPassTimings` field + 8 `DebugStats` mirrors), `src/render/Renderer.cpp` (`ScopedPassTimer` class + 5 timer placements + 1 inline manual timer for TAA resolve), `src/app/AppUpdate.cpp` (8 stats mirrors + 1 derived `otherMs`), `src/debug/DebugHud.cpp` (2 detailed-only HUD lines + `kMaxStatsLineCount = 38`), `src/render/ScreenshotCapture.cpp` (7 new sidecar keys in a second `fmt::format` call).

**Test impact:** Additive struct + fields, no field offsets shift, no shader edits, no descriptor binding changes, no pipeline changes. `ctest 6/6` baseline preserved at `1.47s` wall clock. The `BuildDebugHudVertices` test's 65536-vertex buffer was at the cap with the original code; the new lines had to live in detailed-only to avoid overflow.

## 10.26 Audio engine landed (miniaudio, `2026-06-12`)

miniaudio is now wired into the build (`src/CMakeLists.txt` `add_subdirectory(external/miniaudio)` + link `pthread dl m` on Linux). The `AudioEngine` class lives at `src/audio/AudioEngine.{hpp,cpp}` and is a singleton on `AppState` (mirrors `physics` / `ecs` / `render`). Single `ma_engine` + one `ma_sound_group` (for music bus-level volume) + one `ma_sound` (current track). Built-in MP3 decoder handles `.mp3` directly; OGG/WAV/FLAC would need `extras/decoders/libvorbis` / `libopus` linked separately (deferred).

**Working rules:**

- **Linux audio routing = PulseAudio → pipewire-pulse → PipeWire.** miniaudio's `find_package(PulseAudio)` resolves `libpulse.so.0`; on this host the actual server is PipeWire (verified via `pactl info` → `Server String: /run/user/1000/pulse/native`). No direct PipeWire backend in miniaudio. The user's "выход pipewire pcm" is satisfied by this chain.
- **`MusicState` enum: `Stopped | Playing | Paused`.** Three-valued. **Cursor semantics, 2026-06-13 fix:** `ma_sound_stop` (called by both `pauseImpl()` and `stop()`) preserves the cursor in-place — it only sets the node state to stopped (miniaudio.h:78774), the `pSound->cursor` field is untouched. A subsequent `ma_sound_start` resumes from that cursor. So v1 **does** have true pause/resume without any custom decoder wrapper. The earlier "v1 pause = stop + forget cursor" claim was wrong: the bug was in the 2026-06-12 code (Paused branch of `togglePlayPause` unconditionally called `loadCurrentTrack()` which unloaded the sound and re-init'd from disk, always resetting the cursor to 0). The 2026-06-13 fix adds the `if (!m_soundLoaded)` guard to the Paused branch (mirroring the Stopped branch) so the cursor is preserved across pause → resume. `m_pausedCursorMs` is **dead code** since `ma_sound_stop` already preserves the cursor naturally; kept for field-shape stability, candidate for v2 cleanup.
- **miniaudio 0.11+ has NO `ma_sound_set_time` API** (removed in 0.10+). The absence of `ma_sound_set_time` means we can't SEEK to an arbitrary position, but it does NOT prevent pause/resume (the stop/start cycle preserves the cursor naturally — see above). For a "remember-cursor-across-shutdown" feature (different from pause/resume), we'd need a custom decoder wrapper or a different miniaudio API surface. v1 doesn't need either — pause/resume works out of the box.
- **`ma_engine_config` doesn't have a `playback` substruct** — that field is `ma_device_config`-only. The engine config exposes `sampleRate` and `channels` directly; the engine picks the device's native format (typically `ma_format_s16` on built-in Linux audio). The user-spec "16/44100" is satisfied at the engine level (44.1 kHz sample rate) + the typical device-native 16-bit s16 on the device level. To force a specific format, the renderer would need to drop to `ma_device` API; out of v1 scope.
- **`AudioEnginePtr` uses a function-pointer deleter at global scope** (not the default `std::default_delete<T>`), matching the `DestroyEcsState` / `DestroyPhysicsState` pattern in `core/Types.hpp`. This is because `~AppState()` instantiates the deleter in the header, and `default_delete<T>` requires `T` to be complete at the instantiation point. The custom function-pointer deleter defers the `delete` to the deleter's TU (`audio/AudioEngine.cpp`) where `AudioEngine` is complete. Cost: the deleter is a function call instead of an inline `delete`; not measurable.
- **`MusicDirectoryPath` resolution chain (v1 final, `2026-06-12`):** `PROJECTV_MUSIC_DIR` env → **walk up from `SDL_GetBasePath()` for the repo root** (`.git/` + `AGENTS.md` markers) → CWD-relative `./music` (only if it exists) → `SDL_GetBasePath()/music` → last-ditch CWD-relative `./music`. The **repo-root walk-up is the primary fallback** so the operator can launch the binary by absolute path from anywhere (`/home/le1t/Projects/ProjectV/build/.../bin/ProjectV` from a shell prompt at `/tmp` or `/home/le1t`, or an IDE run button) and still find `<repo_root>/music/`. The walk-up passes `bin/` → `linux-clang-debug/` → `build/` → `<repo_root>` (which has both `.git/` and `AGENTS.md`, the "both markers" check is more specific than `.git` alone and more reliable than `CMakeLists.txt` alone). The CWD-relative fallback handles the case where the repo walk-up fails (e.g. `.git` stripped, system-installed binary, build tree copied without the repo). The SDL_GetBasePath-last-resort handles the case where both fail (binary run from a scratch dir with no `./music` in CWD). **`is_directory` is the discriminator**, not just `exists()` — an empty `music/` directory the engine just created still counts as "exists" and the engine will scan it (finding 0 tracks), which is the correct user-facing behavior ("you have 0 tracks").
  - **v1 history (rejected orderings):** first cut put `SDL_GetBasePath()/music` first (mirroring the screenshot/snapshot pattern); the operator's smoke test from a CWD other than the repo root found 0 tracks in the build-dir music folder (which the engine had helpfully auto-created as empty). Second cut swapped to CWD-relative `./music` first; that worked when the operator ran from the repo root but broke when running by absolute path from elsewhere. Third cut (current) added the repo-root walk-up as the primary fallback. The walk-up is robust: it works for both the "run from repo root" case (where the walk-up lands at the same place the CWD-relative would have) AND the "run by absolute path from /tmp" case.
- **Track switching, 2026-06-12 (follow-up slice).** `AudioEngine::nextTrack()` / `previousTrack()` cycle through the playlist with wrap-around. `nextTrack` of last index → 0; `previousTrack` of index 0 → `playlist.size() - 1`. Both are no-ops on an empty playlist. Internal `goToTrack(size_t newIndex)` clamps + updates `m_currentIndex` and the cached `m_currentTrackName`, then re-loads the sound per the current state: **Playing** = stop + reload + start (interrupts current track — what the user expects when they press Next mid-playback); **Paused** = stop + reload (state stays Paused; the new track is loaded but not playing, so the next `Q` press plays the new track); **Stopped** = just update the index (no sound to reload; the next `Q` will `loadCurrentTrack()` at the new index). The `m_pausedCursorMs` field is reset to 0 on every track switch (the new track's cursor is 0; no resume-from-cursor in v1). Hotkeys: `9` = next, `0` = previous. The 9/0 pair is the only adjacent free digit pair in the existing `InputAction` table (7/8 went to volume-down / volume-up in the audio-engine slice).
- **Cycling math is unsigned-safe.** `nextTrack()` does `(m_currentIndex + 1u) % m_playlist.size()`. `previousTrack()` does `(m_currentIndex + m_playlist.size() - 1u) % m_playlist.size()`. The `+ m_playlist.size()` in the previous-track case is the key — without it, `(0u + 0u - 1u)` would underflow to `UINT_MAX` and the modulo would land at a nonsense index. With it, `(0u + N - 1u) % N = (N-1) % N = N-1` (the last track), which is the correct wrap-around.
- **5-second playlist refresh.** `tick()` is called from `UpdateApp` after the input handlers. Cheap when `m_lastPlaylistRefresh` is recent (just one `steady_clock::now()` call). If the currently-loaded track is still in the new playlist, the index is remapped to its new position. If gone, the sound is unloaded and state transitions to `Stopped`.
- **Playlist is `std::vector<std::filesystem::path>` sorted alphabetically via `std::sort` + `path::compare`** (case-sensitive per platform `std::filesystem` semantics). `.mp3` extension match is case-insensitive (`std::tolower` on the extension string).
- **`MA_SOUND_FLAG_STREAM` for the file loader.** MP3 is streamed from disk rather than pre-decoded to RAM. For typical music files (3-10 MB) this is a small saving, but the right semantic for "playlist that can change every 5 seconds."
- **Loop = `MA_TRUE` for v1.** Music loops forever; the operator stops manually. Future SFX layer would use the default `MA_FALSE`.
- **Volume via `ma_sound_group_set_volume` (bus-level), not per-sound.** Allows future SFX/Ambient groups to have their own bus-level volumes without cross-contamination. Also `ma_sound_set_volume` is called belt-and-suspenders in `applyVolume()` so a future "no group" path would still respect the volume.
- **4 v1 hotkeys: `Q` play/pause, `E` stop, `7` vol-, `8` vol+.** Per the operator's note "надо переназначить все кнопки ... но это потом. Сейчас назначай там, где свободно." Q/E/7/8 are the only free letters/digits in the existing `InputAction` enum. Full rebind is the follow-up slice.
- **Sidecar `music_*` keys write `initialized=0` for v1.** The screenshot capture path doesn't have a direct pointer to `AppState::audio` (the renderer is reached via `DrawFrame` → `RecordGraphicsCommands` → `SaveRequestedScreenshot` → `SaveScreenshotCaptureMetadata`, none of which take an audio pointer). Plumb the audio engine pointer through `FrameRenderData` (or via a `RenderContext` struct) is a follow-up slice. The HUD's `MUSIC <STATE> VOL 0.80 TRK <name>` is the authoritative live view.

**Files touched:** `src/CMakeLists.txt` (`add_subdirectory(external/miniaudio)` + `pthread dl m`), `tests/CMakeLists.txt` (added `miniaudio` to test link line + `audio/AudioEngine.cpp` to test source list), `src/audio/AudioEngine.{hpp,cpp}` (NEW, ~440 lines), `src/audio/MusicDirectoryPath.{hpp,cpp}` (NEW, ~50 lines), `src/core/Types.hpp` (4 `InputAction` + 5 `DebugStats` mirrors + forward decls + `AudioEnginePtr` typedef + `DestroyAudioEngine` decl + `AppState::audio` field), `src/app/AppUpdate.hpp` + `src/app/AppUpdate.cpp` (10th `audio` parameter + 4 input handlers + 5 stats mirrors), `src/app/InputActions.cpp` (4 `BindAction`), `src/app/main.cpp` (init + first playlist scan), `src/debug/DebugHud.cpp` (1 regular HUD line + 2 detailed helper lines), `src/render/ScreenshotCapture.cpp` (6 default-`OFF` sidecar keys), `music/.gitkeep` (empty music folder at repo root).

**Test impact:** `ctest 6/6` (1.48s wall clock) preserved. The audio engine itself isn't tested in `tests/VoxelWorldTests.cpp` because it would need an actual PulseAudio device; the test path passes `nullptr` for the engine. `runtime::LogRuntimeFailure` is the failure surface.

**User-content note:** `music/` is intentionally not auto-populated. The operator drops `.mp3` files in; `PROJECTV_MUSIC_DIR` overrides the path. On a fresh clone, `music/.gitkeep` is the only file; the engine reports `0 mp3 track(s)`, HUD shows `MUSIC STOP VOL 0.80 NO TRACKS`, hotkeys are no-ops. The directory is created on disk by `loadMusicFolder` if it doesn't exist, so the operator can `cd` into it and drop files without pre-creating.

---

