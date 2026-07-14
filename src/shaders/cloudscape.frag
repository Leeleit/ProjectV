#version 460
// SPDX-License-Identifier: MIT
// ProjectV — cloudscape fragment shader (single-layer ray-march).
//
// Per Schneider "Nubis" 2017 + closed `2026-06-21-cloudscape-rendering`
// B_SingleLayerRayMarch (universal default per-platform). MVP analytical
// form: flat horizontal cloud slab at y = cloudBaseHeight, ray-march from
// camera-position-reconstructed world ray through the slab, sample 2D FBM
// noise per step, accumulate Beer-Lambert transmittance. Schlick phase
// for sun lighting. Cheaper than multi-layer (3 layer Nubis) but adequate
// for Stage 5.x MVP.
//
// All push constants carry linear-space color (no tone mapping applied).
// Tonemap + grade happen in the main voxel pass to keep cloud/voxel
// consistent.
//
// EVIL: cloudBaseHeight=80m, cloudThickness=24m hard-coded for VoxelLab
// reference scene. Production tunable via push constants or per-scene
// VoxelScenePreset.

layout(location = 0) in vec2 inNdcXY;

layout(location = 0) out vec4 outCloudColor;

layout(set = 0, binding = 0) uniform sampler2D cloudNoise;
layout(set = 0, binding = 1) uniform sampler2D sceneColorIn;
layout(set = 0, binding = 2) uniform sampler2D depthIn;

layout(push_constant) uniform CloudscapePushConstants {
    vec4 cloudColorAndCoverage;
    vec4 sunDirectionAndIntensity;
    vec4 cloudLayerParams;
    vec4 viewParams;
} pc;

const float kCloudBaseHeight = 80.0;
const float kCloudThickness = 24.0;
const int kMarchCount = 24;
const float kSchlickG = 0.5;

float SchlickPhase(float cosTheta, float g)
{
    const float k = 1.55 + g * (-50.0 + g * (230.0 + g * (-490.0 + g * 425.0)));
    return (1.0 + k * cosTheta) / (1.0 + k);
}

void main() {
    if (inNdcXY.y < -0.05) {
        outCloudColor = vec4(0.0);
        return;
    }

    const float aspectRatio = max(pc.viewParams.y, 0.0001);
    const float tanHalfFovY = max(pc.viewParams.z, 0.0001);
    const vec2 ndc = inNdcXY + vec2(pc.viewParams.w, pc.cloudLayerParams.w);
    const vec3 rayDir = normalize(vec3(
        ndc.x * aspectRatio * tanHalfFovY,
        ndc.y * tanHalfFovY,
        -1.0));

    const vec3 cameraPos = vec3(
        pc.viewParams.x * 0.0,
        pc.viewParams.x,
        pc.viewParams.w * 0.0);

    if (rayDir.y >= -0.01) {
        outCloudColor = vec4(0.0);
        return;
    }

    const float tEntry = (kCloudBaseHeight - cameraPos.y) / rayDir.y;
    const float tExit = (kCloudBaseHeight - kCloudThickness - cameraPos.y) / rayDir.y;
    if (tEntry <= 0.0 || tExit <= 0.0) {
        outCloudColor = vec4(0.0);
        return;
    }
    const float tStart = min(tEntry, tExit);
    const float tEnd = max(tEntry, tExit);
    const float stepLength = (tEnd - tStart) / float(kMarchCount);

    const vec3 sunDir = normalize(pc.sunDirectionAndIntensity.xyz);
    const float cosTheta = max(dot(rayDir, sunDir), 0.0);
    const float phase = SchlickPhase(cosTheta, kSchlickG);
    const vec3 cloudColor = pc.cloudColorAndCoverage.rgb;
    const float coverage = clamp(pc.cloudColorAndCoverage.w, 0.0, 1.0);
    const float sunIntensity = pc.sunDirectionAndIntensity.w;

    vec3 accumulated = vec3(0.0);
    float transmittance = 1.0;

    for (int i = 0; i < kMarchCount; ++i) {
        const float t = tStart + stepLength * (float(i) + 0.5);
        const vec3 worldPos = cameraPos + rayDir * t;
        const vec2 noiseUv = worldPos.xz * 0.0015 + vec2(pc.cloudLayerParams.x, pc.cloudLayerParams.y);
        const float density = texture(cloudNoise, noiseUv).r;
        const float shaped = clamp(density - (1.0 - coverage) * 0.5, 0.0, 1.0);
        const float stepTransmittance = exp(-shaped * stepLength * 0.06);
        const float sunContrib = sunIntensity * phase * 0.18 * shaped;
        accumulated += cloudColor * (shaped * 0.04 + sunContrib) * transmittance * (1.0 - stepTransmittance);
        transmittance *= stepTransmittance;
        if (transmittance < 0.02) {
            break;
        }
    }

    outCloudColor = vec4(accumulated, 1.0 - transmittance);
}
