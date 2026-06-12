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
    // M4 MVP has no shadow receive: model is lit by direct sun only,
    // the same GGX path as the voxel pass. Shadow-cast / shadow-receive
    // for models is a follow-up (per Q5=2 contract, RTX-aware future).
    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const vec3 albedo = vec3(0.85, 0.65, 0.45);
    // M6 prep: use `inUv` to vary the base albedo per-fragment so
    // the model pass's UV path is observable in the rendered
    // framebuffer. The pattern is a 4×4 procedural checkerboard
    // tinted in two distinct hues — when the UV stream is wired
    // correctly (mesh has TEXCOORD_0, baked vertex carries the
    // attribute, the model.vert passes it through), the box shows
    // a clear checker pattern. If the UV stream is missing (e.g.
    // `box.glb` has no TEXCOORD_0 accessor), the input defaults
    // to (0, 0) and the whole box is uniform. This is the
    // cheapest possible "is UV actually flowing through the
    // pipeline?" test: a real diffuse texture sampling is a M6
    // follow-up.
    const vec2 checkerUv = floor(inUv * 4.0);
    const float checkerMask = mod(checkerUv.x + checkerUv.y, 2.0);
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
    // Apply exposure to the linear (pre-tonemap) signal so the
    // tonemap and grading stages operate on the same range the
    // voxel pass does. The exposure stays applied in the
    // TAA-on output below: the resolve pass treats `outSceneColor`
    // as **linear light already exposed**, and applies its own
    // tonemap / grading on top. The TAA-off output goes through
    // the same exposure + tonemap + grading path here so the
    // non-TAA framebuffer matches the post-resolve framebuffer.
    color *= max(sceneLighting.postProcess.x, 0.0);

#ifdef TAA_ENABLED
    // M5.2: TAA-on contract — the resolve pass owns the tonemap
    // and grading. Mirror what `voxel.frag` does (line 958-964):
    // write the linear, exposed, pre-tonemap color so the
    // resolve pass can clamp-blend it against history and only
    // then apply tonemap + grading. The earlier `model.frag`
    // wrote the post-tonemap, post-grading color, so the
    // resolve pass applied a second round of tonemap and
    // grading on top — visible as a desaturated, brown-shifted
    // tint on model surfaces. Bug traced via the comment trail
    // in `voxel.frag` (linearColor / color split at line 958).
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

