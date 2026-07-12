# Post-RTX Rendering Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or
`superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Довести до production-ready состояния три открытые post-RTX задачи: **7.1 VCT cone density**, **7.3 tone
mapping polish**, **7.4 post-FX (bloom + aerial perspective)**. Убедиться, что они собираются, проходят тесты, не дают
Vulkan validation errors при runtime smoke, и документация актуальна.

**Architecture:**

- **7.1 VCT cones:** diffuse VCT переходит с 6 на 12 cone (Fibonacci spiral + down bias); specular VCT использует 4-cone
  set для `roughness ≤ 0.6`; добавлены debug views `VctConeCount`/`VctConeDirections`.
- **7.3 Tone mapping:** tone mapping (ACES approx), exposure bias и color grading выполняются в `voxel.frag`/
  `model.frag`; `LightingDebugView` расширен значениями `ToneMapOutput`/`ColorGradingOutput`/`ExposureCurve`.
- **7.4 Post-FX:** compute-based post-FX chain после main graphics pass — bloom threshold → downsample mips → upsample →
  composite (scene + depth + bloom + aerial perspective) → `postFxOutput` → blit → swapchain. Feature flags
  `PROJECTV_BLOOM=1` и `PROJECTV_AERIAL_PERSPECTIVE=1`.

**Tech Stack:** C++26, Vulkan 1.4, GLSL 460, compute shaders, VMA, Tracy profiling.

## Global Constraints

- **RTX-only path forward** — non-RTX GPU hard fail.
- **CSM удалён** — shadow path только RTX.
- **TAA удалён из mainline** — будущий antialiasing через DLSS/Streamline.
- Feature flags через env vars (`PROJECTV_BLOOM=1`, `PROJECTV_AERIAL_PERSPECTIVE=1`) и `core/EnvUtils.hpp`.
- Все env-gated флаги читают `std::getenv` каждый вызов (нет static cache).
- `MAX_FRAMES_IN_FLIGHT = 2`.
- `sceneColorImage` формат `B10G11R11_UFLOAT_PACK32`.
- Комментарии — одной строкой после кода; `// EVIL:` только для неочевидных хаков.
- Pre-commit gate: green build, ctest 43/43, 0 Vulkan validation errors в runtime smoke.

---

## Task 1: Verify build & test baseline

**Files:**

- Run: `ninja -C build/linux-clang-debug`
- Run: `ctest --test-dir build/linux-clang-debug --output-on-failure`

**Interfaces:**

- Consumes: текущий код в рабочем дереве.
- Produces: подтверждение baseline green.

- [ ] **Step 1: Configure (if needed)**

Run: `cmake --preset linux-clang-debug`
Expected: configure completes with 0 errors.

- [ ] **Step 2: Build main target**

Run: `ninja -C build/linux-clang-debug ProjectV`
Expected: 0 errors, 0 warnings (или только известные pre-existing).

- [ ] **Step 3: Run ctest**

Run: `ctest --test-dir build/linux-clang-debug --output-on-failure`
Expected: 43/43 tests pass.

- [ ] **Step 4: Record baseline note**

Записать результат в `agent/workspace.md` (агент не коммитит).

---

## Task 2: Tone mapping polish (7.3)

**Files:**

- Modify: `src/shaders/voxel.frag` (tone map + color grading branches)
- Modify: `src/voxel/VoxelMaterials.cpp` (`BuildVoxelSceneLighting` wiring)
- Modify: `src/debug/DebugHud.cpp` (debug view names)
- Test: `tests/VoxelizePipelineTests.cpp`, `tests/VoxelWorldTests.cpp`

**Interfaces:**

- Consumes: `VoxelLightingDebugControls` (`exposureBiasStops`, `toneMapOperator`, `debugView`), `VoxelSceneLighting` (
  `postProcess`, `colorGrading`, `exposureControl`).
- Produces: `LightingDebugView` values 14–16; tone-mapped LDR output.

- [ ] **Step 1: Verify ACES approx function**

Найти `ApplyToneMap` / `ApplyColorGrading` в `src/shaders/voxel.frag`. Убедиться, что `ToneMapOperator::AcesApprox`
использует ACES Filmic approximation, а не Reinhard.

Expected shader snippet:

```glsl
vec3 ApplyToneMap(vec3 color, int operator) {
    if (operator == 2) { // AcesApprox
        const float a = 2.51;
        const float b = 0.03;
        const float c = 2.43;
        const float d = 0.59;
        const float e = 0.14;
        return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
    }
    // ...
}
```

- [ ] **Step 2: Verify debug view branches**

Check `voxel.frag` branches for `lightingDebugView == 14u`, `15u`, `16u` map to `ToneMapOutput`/`ColorGradingOutput`/
`ExposureCurve`.

Expected:

```glsl
} else if (lightingDebugView == 14u) {
    outColor = vec4(toneMapped, 1.0);
    return;
} else if (lightingDebugView == 15u) {
    outColor = vec4(graded, 1.0);
    return;
} else if (lightingDebugView == 16u) {
    outColor = vec4(vec3(exposure), 1.0);
    return;
}
```

- [ ] **Step 3: Verify exposure bias wiring**

Check `VoxelMaterials.cpp::BuildVoxelSceneLighting` writes `exposureControl` and `postProcess` from
`VoxelLightingDebugControls`.

Expected:

```cpp
lighting.exposureControl[0] = std::exp2(controls.exposureBiasStops);
lighting.postProcess[0] = lighting.exposureControl[0];
```

- [ ] **Step 4: Update DebugHud labels**

Add/verify labels for `ToneMapOutput`, `ColorGradingOutput`, `ExposureCurve` in `src/debug/DebugHud.cpp`.

Expected: HUD cycles through all 19 debug views with correct names.

- [ ] **Step 5: Run affected tests**

Run:

```bash
ctest --test-dir build/linux-clang-debug -R VoxelizePipelineTests -V
ctest --test-dir build/linux-clang-debug -R VoxelWorldTests -V
```

Expected: PASS.

- [ ] **Step 6: Runtime smoke baseline**

Run:

```bash
bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "FINAL TONEMAP GRADING EXPOSURE"
```

Expected: exit 0, 0 Vulkan validation errors, captures produced in `runtime/captures/`.

---

## Task 3: VCT cone density upgrade (7.1)

**Files:**

- Modify: `src/shaders/voxel.frag` (`kVctConeDirectionCount`, cone arrays, specular set)
- Test: `tests/VoxelizePipelineTests.cpp`, `tests/VoxelWorldTests.cpp`

**Interfaces:**

- Consumes: `sceneLighting.vctParams`, `sceneLighting.vctSpecularParams`, normal, viewDirection.
- Produces: 12-cone diffuse irradiance, 4-cone specular reflection, debug views 17/18.

- [ ] **Step 1: Verify 12-cone diffuse array**

Check `kVctConeDirectionCount = 12u` and `kVctConeDirections[12]` defined.

Expected:

```glsl
const uint kVctConeDirectionCount = 12u;
const vec3 kVctConeDirections[12] = vec3[12](...);
```

- [ ] **Step 2: Fix cone rotation to use world-space TBN basis**

Current code uses `mat3(pushConstants.viewProjection) * kVctConeDirections[coneIndex]`, что некорректно — cones должны
вращаться вокруг нормали в world space, а не через view-projection матрицу.

Replace diffuse cone loop in `voxel.frag`:

```glsl
const vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
const vec3 tangent = normalize(cross(up, normal));
const vec3 bitangent = cross(normal, tangent);
const mat3 tbn = mat3(tangent, bitangent, normal);
for (uint coneIndex = 0u; coneIndex < kVctConeDirectionCount; ++coneIndex) {
    const vec3 rotated = normalize(tbn * kVctConeDirections[coneIndex]);
    vctDiffuseIrradiance += VctSampleDirectionalCone(
        inWorldPosition,
        rotated,
        vctConeApertureTan,
        vctMaxDistance,
        kVctMaxMipLevel);
}
```

Expected: cones sample hemisphere oriented around `normal`.

- [ ] **Step 3: Verify 4-cone specular set**

Check `VctSampleReflectionConeSet` returns average of 4 cones around reflection vector, gated by roughness.

Expected function signature:

```glsl
vec3 VctSampleReflectionConeSet(
    vec3 worldOrigin,
    vec3 viewDirection,
    vec3 normal,
    float roughness,
    float maxDistance,
    float maxMip);
```

- [ ] **Step 4: Verify debug views 17/18**

Check `voxel.frag` branches:

```glsl
} else if (lightingDebugView == 17u) {
    const float coneCountVis = float(kVctConeDirectionCount) / 16.0;
    outColor = vec4(vec3(coneCountVis), 1.0);
    return;
} else if (lightingDebugView == 18u) {
    outColor = vec4(abs(kVctConeDirections[uint(gl_FragCoord.x) % kVctConeDirectionCount]) * 0.5 + 0.5, 1.0);
    return;
}
```

- [ ] **Step 5: Run tests & smoke**

Run:

```bash
ctest --test-dir build/linux-clang-debug -R VoxelizePipelineTests -V
ctest --test-dir build/linux-clang-debug -R VoxelWorldTests -V
bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "VCT_CNT VCT_DIR FINAL"
```

Expected: PASS, 0 validation errors.

---

## Task 4: Post-FX resource & barrier correctness (7.4)

**Files:**

- Modify: `src/render/PostFx.cpp`
- Modify: `src/render/RendererRecordCommands.cpp`
- Modify: `src/core/Types.hpp`
- Modify: `src/shaders/post_composite.comp`

**Interfaces:**

- Consumes: `sceneColorImage` (COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL), `depthImage` (
  DEPTH_ATTACHMENT_OPTIMAL → DEPTH_READ_ONLY_OPTIMAL), `bloomResultImage`, `postFxOutputImage`.
- Produces: `postFxOutputImage` in `TRANSFER_SRC_OPTIMAL`.

- [ ] **Step 1: Add depth layout transition before composite**

In `src/render/PostFx.cpp::RecordPostFxPass`, before composite descriptor write, transition `render.depthImage` to
`DEPTH_READ_ONLY_OPTIMAL`.

```cpp
::TransitionImage(
    commandBuffer,
    render.depthImage,
    VK_IMAGE_ASPECT_DEPTH_BIT,
    render.depthImageCurrentLayout,
    VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
    VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
```

Expected: validation layer no longer reports layout mismatch for depth descriptor.

- [ ] **Step 2: Add post-FX extent/resize invalidation**

In `src/core/Types.hpp` add near post-FX fields:

```cpp
VkExtent2D postFxExtent{};
```

In `src/render/PostFx.cpp::CreatePostFxResources`:

```cpp
if (render->postFxOutputImage != VK_NULL_HANDLE &&
    (render->postFxExtent.width != extent.width ||
     render->postFxExtent.height != extent.height)) {
    DestroyPostFxResources(context, render);
}
render->postFxExtent = extent;
```

Expected: resizing window recreates post-FX images.

- [ ] **Step 3: Remove dead aerial-perspective fields**

`Types.hpp` has `aerialPerspectivePipeline` and `aerialPerspectiveShaderModule` that are never created/used. Remove
them.

Edit `src/core/Types.hpp`:

- Remove `VkPipeline aerialPerspectivePipeline = VK_NULL_HANDLE;`
- Remove `VkShaderModule aerialPerspectiveShaderModule = VK_NULL_HANDLE;`

Expected: build still green.

- [ ] **Step 4: Wire aerial perspective fog color from scene lighting**

Replace hardcoded fog color in `src/shaders/post_composite.comp` with sky/horizon color from `sceneLighting`.

Add new push constant in `src/render/PostFx.hpp`:

```cpp
struct PostFxPushConstants {
    std::array<float, 4> params0{}; // x=threshold, y=softKnee, z=bloomIntensity, w=mipLevel
    std::array<float, 4> params1{}; // x=fogDensity, y=fogMax, z=exposure, w=time
    std::array<float, 4> params2{}; // xyz=sunDirection
    std::array<float, 4> params3{}; // xyz=cameraPosition
    std::array<float, 4> params4{}; // xyz=fogColor, w=reserved
};
static_assert(sizeof(PostFxPushConstants) == 80);
```

Update pipeline layout push constant size in `src/render/PostFx.cpp`:

```cpp
pushConstantRange.size = sizeof(PostFxPushConstants);
```

Write fog color in `RecordPostFxPass`:

```cpp
push.params4[0] = lighting.horizonColorAndFogStart[0];
push.params4[1] = lighting.horizonColorAndFogStart[1];
push.params4[2] = lighting.horizonColorAndFogStart[2];
```

Update `post_composite.comp`:

```glsl
layout(push_constant) uniform PushConstants {
    vec4 params0;
    vec4 params1;
    vec4 params2;
    vec4 params3;
    vec4 params4; // xyz=fogColor
} pushConstants;
```

Replace fog line:

```glsl
const vec3 fogColor = pushConstants.params4.xyz;
```

Expected: aerial perspective uses scene lighting fog color instead of hardcoded value.

- [ ] **Step 5: Verify image layout transitions after composite**

Ensure `postFxOutputImage` ends in `TRANSFER_SRC_OPTIMAL` and swapchain image is transitioned to `TRANSFER_DST_OPTIMAL`
before blit. Current code already does this; verify no regressions.

Expected: no layout transition validation errors.

---

## Task 5: Post-FX runtime smoke & visual validation

**Files:**

- Run: `tools/linux/Invoke-ProjectVRuntimeSmoke.sh`

**Interfaces:**

- Consumes: env flags `PROJECTV_BLOOM=1`, `PROJECTV_AERIAL_PERSPECTIVE=1`.
- Produces: smoke captures in `runtime/captures/`.

- [ ] **Step 1: Smoke without post-FX**

Run:

```bash
bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "FINAL"
```

Expected: exit 0, validation clean.

- [ ] **Step 2: Smoke with bloom only**

Run:

```bash
PROJECTV_BLOOM=1 bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "FINAL"
```

Expected: exit 0, bloom visible on bright voxels (glass/fluid highlights), 0 validation errors.

- [ ] **Step 3: Smoke with aerial perspective only**

Run:

```bash
PROJECTV_AERIAL_PERSPECTIVE=1 bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "FINAL"
```

Expected: exit 0, depth cue visible (distant geometry desaturated/foggy), 0 validation errors.

- [ ] **Step 4: Smoke with all polish features**

Run:

```bash
PROJECTV_BLOOM=1 PROJECTV_AERIAL_PERSPECTIVE=1 bash tools/linux/Invoke-ProjectVRuntimeSmoke.sh --build-dir build/linux-clang-debug --views "FINAL TONEMAP GRADING EXPOSURE VCT_CNT VCT_DIR"
```

Expected: exit 0, combined effect correct, 0 validation errors.

---

## Task 6: Documentation update

**Files:**

- Modify: `TODO.md`
- Modify: `agent/workspace.md`
- Modify: `CHANGELOG.md`

**Interfaces:**

- Consumes: completed tasks, smoke results.
- Produces: updated docs.

- [ ] **Step 1: Mark tasks closed in TODO.md**

Update status of 7.1, 7.3, 7.4 to ✅ closed. Update summary counts (0 open next-priority). Remove TAA leftovers if any.

- [ ] **Step 2: Update workspace.md session log**

Add entry for completion of 7.1/7.3/7.4, build/test results, smoke results.

- [ ] **Step 3: Add CHANGELOG entries**

Add `[Unreleased]` Added/Changed entries:

```markdown
### Added
- VCT cone density upgrade: 12-cone diffuse + 4-cone specular with roughness gating and debug views.
- Tone mapping polish: ACES approx, exposure curve, color grading debug views.
- Compute-based bloom and aerial perspective post-FX behind `PROJECTV_BLOOM`/`PROJECTV_AERIAL_PERSPECTIVE` env flags.

### Changed
- `LightingDebugView` extended from 14 to 19 values (tone map, color grading, exposure, VCT debug).
```

- [ ] **Step 4: Propose commit message**

Per `AGENTS.md` §5.1, provide commit message for operator:

```
feat(render): integrate post-RTX rendering polish (7.1/7.3/7.4)

- Upgrade VCT to 12-cone diffuse + 4-cone specular with debug views
- Add ACES tone mapping, exposure curve, and color grading debug views
- Integrate compute-based bloom and aerial perspective post-FX
- Fix depth layout transition and post-FX resize handling
```

---

## Self-Review

1. **Spec coverage:** TODO.md §7.1, §7.3, §7.4 покрыты task-by-task.
2. **Placeholder scan:** нет `TBD`, `TODO`, `implement later`.
3. **Type consistency:** `PostFxPushConstants` size updated to 80 bytes; pipeline layout and shader updated together.
