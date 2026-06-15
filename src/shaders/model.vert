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
    // 1.5 anti-flicker layer history params (texelX, texelY, valid, blendFactor).
    // Mirrors the C++ `VoxelSceneLighting` byte layout. This vertex shader does
    // not read it, but the field is declared so the std430 layout matches the
    // C++ struct byte-for-byte (see agent/decisions.md §18).
    vec4 taaLayerHistoryParams;
} sceneLighting;

// Reuses the existing `GraphicsPushConstants` layout from
// `voxel.vert` (offset 0, size 128) so the model pipeline shares
// one push constant range with the main voxel pass. `viewProjection`
// is at offset 0; `modelTransform` rides along at offset 64 in
// the same struct. This is intentionally co-allocated with the
// existing struct so we can re-use the same `vkCmdPushConstants`
// call instead of issuing a second push for the model transform.
layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    mat4 modelTransform;
} pushConstants;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(location = 0) out vec3 outWorldPosition;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec2 outUv;

void main() {
    const vec4 worldPosition = pushConstants.modelTransform * vec4(inPosition, 1.0);
    outWorldPosition = worldPosition.xyz;
    outWorldNormal = normalize(mat3(pushConstants.modelTransform) * inNormal);
    outUv = inUv;
    gl_Position = pushConstants.viewProjection * worldPosition;
}
