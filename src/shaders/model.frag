#version 460

#include "lighting.glsl"

layout(set = 0, binding = 3, std430) readonly buffer SceneLightingBuffer {
    vec4 skyColorAndFogDensity;
    vec4 horizonColorAndFogStart;
    vec4 groundColorAndFogMax;
    vec4 sunColorAndIntensity;
    vec4 sunDirectionAndWrap;
    vec4 postProcess;
    vec4 sunShadowParams;
    vec4 sunContactShadowParams;
    vec4 ambientOcclusionParams;
    mat4 sunShadowViewProjections[4];
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 shadowCascadeDepthSplits;
    vec4 shadowCascadeBlendParams;
    vec4 localPointLightPositionAndRadius;
    vec4 localPointLightColorAndIntensity;
    vec4 localPointLightParams;
    vec4 taaParams;
    mat4 prevViewProjectionMatrix;
    vec4 taaHistoryParams;
    /// \brief 1.5 anti-flicker layer history params (texelX, texelY, valid, blendFactor).
    ///
    /// \details
    ///  Mirrors the C++ `VoxelSceneLighting` byte layout. This shader does not

    ///  read it, but the field is declared so the std430 layout matches the

    ///  C++ struct byte-for-byte (see agent/decisions.md §18).

    ///
    /// \see agent/decisions.md §18
    vec4 taaLayerHistoryParams;
} sceneLighting;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec2 inUv;

#ifdef TAA_ENABLED
layout(location = 1) out vec4 outSceneColor;
#define OUT_COLOR outSceneColor
#else
layout(location = 0) out vec4 outColor;
#define OUT_COLOR outColor
#endif

void main() {
    const vec3 normal = normalize(inWorldNormal);
    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const vec3 sunColor = sceneLighting.sunColorAndIntensity.rgb * sceneLighting.sunColorAndIntensity.w;
    const vec3 viewDirection = normalize(sceneLighting.localPointLightPositionAndRadius.xyz - inWorldPosition + vec3(0.0001));
    /// \brief M4 MVP has no shadow receive:
    ///
    /// \details
    /// model is lit by direct sun only,
    ///  the same GGX path as the voxel pass. Shadow-cast / shadow-receive

    ///  for models is a follow-up (per Q5=2 contract, RTX-aware future).

    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const vec3 albedo = vec3(0.85, 0.65, 0.45);
    /// \brief M6 prep:
    ///
    /// \details
    /// triplanar procedural checker on `inWorldPosition`,
    ///  picked by the dominant face normal axis. Cell size 0.3

    ///  (NOT a divisor of 1.0 — the 1×1 box side length) plus an

    ///  arbitrary `vec2(0.137, 0.241)` offset, so the cell grid is

    ///  NOT aligned with the box vertices. Without the offset

    ///  (and with cell sizes like 0.25 or 0.5, both divisors of

    ///  1.0), all 4 vertices of a face land on the same cell

    ///  corner and `floor()` collapses the face to a single

    ///  tint — the previous "two triangles with different

    ///  gradients" complaint. With cell 0.3, the 4 vertices of

    ///  a face land in different cells, so the face shows a

    ///  visible chess-board pattern that varies between faces

    ///  (each face uses a different world-space axis pair, so

    ///  the offsets land in different cell positions). M6+ will

    ///  replace this with a real `sampler2D baseColor` and

    ///  per-face UVs.

    const vec3 absNormal = abs(normal);
    vec2 checkerUv;
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        /// \brief Top/bottom face:
        ///
        /// \details
        /// project onto XZ.
        checkerUv = inWorldPosition.xz;
    } else if (absNormal.x >= absNormal.z) {
        /// \brief Left/right face:
        ///
        /// \details
        /// project onto ZY.
        checkerUv = inWorldPosition.zy;
    } else {
        /// \brief Front/back face:
        ///
        /// \details
        /// project onto XY.
        checkerUv = inWorldPosition.xy;
    }
    const float cellSize = 0.3;
    const vec2 cellCoord = floor((checkerUv + vec2(0.137, 0.241)) / cellSize);
    const float checkerMask = mod(cellCoord.x + cellCoord.y, 2.0);
    const vec3 checkerTint = mix(
        vec3(1.0, 0.95, 0.85),
        vec3(0.7, 0.85, 1.0),
        checkerMask);
    const vec3 baseAlbedo = albedo * checkerTint;
    const float roughness = 0.55;
    const float metallic = 0.0;
    const float reflectance = 0.5;
    const float directDiffuseStrength = 1.0;
    const vec3 ambient = ProjectV_SampleEnvironmentDiffuse(
        normal,
        sceneLighting.skyColorAndFogDensity.rgb,
        sceneLighting.horizonColorAndFogStart.rgb,
        sceneLighting.groundColorAndFogMax.rgb,
        sceneLighting.postProcess.y) * baseAlbedo;
    const vec3 directSun = ProjectV_EvaluateDirectLighting(
        sunDirection,
        sunColor,
        normal,
        viewDirection,
        baseAlbedo,
        roughness,
        metallic,
        reflectance,
        directDiffuseStrength,
        diffuseWrap);
    vec3 color = ambient + directSun;
    /// \brief Apply exposure to the linear (pre-tonemap) signal so the
    ///
    /// \details
    ///  tonemap and grading stages operate on the same range the

    ///  voxel pass does. The exposure stays applied in the

    ///  TAA-on output below: the resolve pass treats `outSceneColor`

    ///  as **linear light already exposed**, and applies its own

    ///  tonemap / grading on top. The TAA-off output goes through

    ///  the same exposure + tonemap + grading path here so the

    ///  non-TAA framebuffer matches the post-resolve framebuffer.

    color *= max(sceneLighting.postProcess.x, 0.0);

#ifdef TAA_ENABLED
    /// \brief M5.2:
    ///
    /// \details
    /// TAA-on contract — the resolve pass owns the tonemap
    ///  and grading. Mirror what `voxel.frag` does (line 958-964):

    ///  write the linear, exposed, pre-tonemap color so the

    ///  resolve pass can clamp-blend it against history and only

    ///  then apply tonemap + grading. The earlier `model.frag`

    ///  wrote the post-tonemap, post-grading color, so the

    ///  resolve pass applied a second round of tonemap and

    ///  grading on top — visible as a desaturated, brown-shifted

    ///  tint on model surfaces. Bug traced via the comment trail

    ///  in `voxel.frag` (linearColor / color split at line 958).

    outSceneColor = vec4(color, 1.0);
#else
    const uint toneMapOperator = uint(sceneLighting.postProcess.z + 0.5);
    color = ProjectV_ApplyToneMap(color, toneMapOperator);
    color = ProjectV_ApplyColorGrading(
        color,
        clamp(sceneLighting.colorGrading.x, 0.25, 4.0),
        clamp(sceneLighting.colorGrading.y, 0.0, 2.0),
        clamp(sceneLighting.colorGrading.z, 0.0, 2.0),
        clamp(sceneLighting.colorGrading.w, -0.25, 0.25));
    outColor = vec4(color, 1.0);
#endif
}

