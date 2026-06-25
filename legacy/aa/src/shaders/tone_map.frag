#version 460

#include "lighting.glsl"

layout(set = 0, binding = 0, std430) readonly buffer SceneLightingBuffer {
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
    vec4 vctParams;
    vec4 vctSpecularParams;
} sceneLighting;

layout(set = 0, binding = 3) uniform sampler2D sceneColor;

layout(push_constant) uniform PushConstants {
    vec2 renderExtentInverse;
    float toneMapOperator;
    float exposure;
} pushConstants;

layout(location = 0) out vec4 outColor;

void main() {
    const vec2 uv = gl_FragCoord.xy * pushConstants.renderExtentInverse;
    vec3 hdr = texture(sceneColor, uv).rgb;

    hdr *= pushConstants.exposure;

    const uint op = uint(pushConstants.toneMapOperator + 0.5);
    vec3 ldr;
    if (op == 0u) {
        ldr = clamp(hdr, 0.0, 1.0);
    } else if (op == 1u) {
        ldr = hdr / (1.0 + max(hdr, vec3(0.0)));
    } else {
        const vec3 a = hdr * (2.51 * hdr + 0.03);
        const vec3 b = hdr * (2.43 * hdr + 0.59) + 0.14;
        ldr = clamp(a / b, 0.0, 1.0);
    }

    const float whitePoint = clamp(sceneLighting.colorGrading.x, 0.25, 4.0);
    const float contrast = clamp(sceneLighting.colorGrading.y, 0.0, 2.0);
    const float saturation = clamp(sceneLighting.colorGrading.z, 0.0, 2.0);
    const float lift = clamp(sceneLighting.colorGrading.w, -0.25, 0.25);
    const vec3 normalizedColor = ldr / whitePoint;
    const float luma = dot(normalizedColor, vec3(0.2126, 0.7152, 0.0722));
    const vec3 saturatedColor = mix(vec3(luma), normalizedColor, saturation);
    ldr = clamp((saturatedColor - vec3(0.5)) * contrast + vec3(0.5 + lift), 0.0, 1.0);

    outColor = vec4(ldr, 1.0);
}
