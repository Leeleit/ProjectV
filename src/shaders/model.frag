#version 460

#include "lighting.glsl"

layout(set = 0, binding = 3, std430) readonly buffer SceneLightingBuffer {
    vec4 skyColorAndFogDensity;
    vec4 horizonColorAndFogStart;
    vec4 groundColorAndFogMax;
    vec4 sunColorAndIntensity;
    vec4 sunDirectionAndWrap;
    vec4 postProcess;
    vec4 sunContactShadowParams;
    vec4 ambientOcclusionParams;
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 localPointLightPositionAndRadius;
    vec4 localPointLightColorAndIntensity;
    vec4 localPointLightParams;
} sceneLighting;

layout(location = 0) in vec3 inWorldPosition;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec2 inUv;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    mat4 modelTransform;
} pushConstants;

layout(location = 0) out vec4 outColor;
#define OUT_COLOR outColor

void main() {
    const vec3 normal = normalize(inWorldNormal);
    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const vec3 sunColor = sceneLighting.sunColorAndIntensity.rgb * sceneLighting.sunColorAndIntensity.w;
    const vec3 viewDirection = normalize(sceneLighting.localPointLightPositionAndRadius.xyz - inWorldPosition + vec3(0.0001));

    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const vec3 albedo = vec3(0.85, 0.65, 0.45);

    const vec3 absNormal = abs(normal);
    vec2 checkerUv;
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        checkerUv = inWorldPosition.xz;
    } else if (absNormal.x >= absNormal.z) {
        checkerUv = inWorldPosition.zy;
    } else {
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

    outColor = vec4(color, 1.0);
}

