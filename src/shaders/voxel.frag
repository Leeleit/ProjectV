#version 460

struct MaterialVisual {
    vec4 baseColor;
    vec4 surface;
    vec4 medium;
    vec4 shading;
};

layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    MaterialVisual materials[];
};

layout(set = 0, binding = 3, std430) readonly buffer SceneLightingBuffer {
    vec4 skyColorAndFogDensity;
    vec4 horizonColorAndFogStart;
    vec4 groundColorAndFogMax;
    vec4 sunColorAndIntensity;
    vec4 sunDirectionAndWrap;
    vec4 postProcess;
    vec4 sunShadowParams;
    mat4 sunShadowViewProjections[4];
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 shadowCascadeDepthSplits;
    vec4 shadowCascadeBlendParams;
} sceneLighting;

layout(set = 0, binding = 4) uniform sampler2DArrayShadow sunShadowMap;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 cameraForward;
} pushConstants;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) flat in uint inMaterialIndex;
layout(location = 3) flat in float inAmbientVisibility;

layout(location = 0) out vec4 outColor;

const uint kSunShadowCascadeCount = 4u;

bool IsGlass(const uint materialIndex) {
    return materialIndex == 1u;
}

bool IsFluid(const uint materialIndex) {
    return materialIndex == 2u;
}

vec3 SampleEnvironmentDiffuse(const vec3 normal) {
    const float up = clamp(normal.y, -1.0, 1.0);
    const float skyWeight = smoothstep(-0.25, 0.85, up);
    const float groundWeight = smoothstep(0.35, -0.75, up) * 0.65;
    const float horizonWeight = pow(1.0 - abs(up), 2.0) * 0.55;
    const vec3 environment =
    sceneLighting.skyColorAndFogDensity.rgb * skyWeight +
    sceneLighting.groundColorAndFogMax.rgb * groundWeight +
    sceneLighting.horizonColorAndFogStart.rgb * (horizonWeight + 0.08);
    return environment * max(sceneLighting.postProcess.y, 0.0);
}

vec3 ApplyToneMap(const vec3 linearColor) {
    const uint toneMapOperator = uint(sceneLighting.postProcess.z + 0.5);
    if (toneMapOperator == 0u) {
        return clamp(linearColor, 0.0, 1.0);
    }
    if (toneMapOperator == 1u) {
        return linearColor / (1.0 + max(linearColor, vec3(0.0)));
    }

    const vec3 a = linearColor * (2.51 * linearColor + 0.03);
    const vec3 b = linearColor * (2.43 * linearColor + 0.59) + 0.14;
    return clamp(a / b, 0.0, 1.0);
}

vec3 ApplyColorGrading(const vec3 mappedColor) {
    const float whitePoint = clamp(sceneLighting.colorGrading.x, 0.25, 4.0);
    const float contrast = clamp(sceneLighting.colorGrading.y, 0.0, 2.0);
    const float saturation = clamp(sceneLighting.colorGrading.z, 0.0, 2.0);
    const float lift = clamp(sceneLighting.colorGrading.w, -0.25, 0.25);
    const vec3 normalizedColor = mappedColor / whitePoint;
    const float luma = dot(normalizedColor, vec3(0.2126, 0.7152, 0.0722));
    const vec3 saturatedColor = mix(vec3(luma), normalizedColor, saturation);
    return clamp((saturatedColor - vec3(0.5)) * contrast + vec3(0.5 + lift), 0.0, 1.0);
}

float DistributionGGX(const float nDotH, const float roughness) {
    const float alpha = roughness * roughness;
    const float alphaSq = alpha * alpha;
    const float denom = max(nDotH * nDotH * (alphaSq - 1.0) + 1.0, 0.0001);
    return alphaSq / (3.14159265 * denom * denom);
}

float GeometrySchlickGGX(const float nDotX, const float roughness) {
    const float r = roughness + 1.0;
    const float k = (r * r) / 8.0;
    return nDotX / max(nDotX * (1.0 - k) + k, 0.0001);
}

float GeometrySmith(const float nDotV, const float nDotL, const float roughness) {
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

vec3 FresnelSchlick(const float cosTheta, const vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - clamp(cosTheta, 0.0, 1.0), 5.0);
}

float GetCameraViewDepth(const vec3 worldPosition) {
    return dot(worldPosition - pushConstants.cameraPosition.xyz, normalize(pushConstants.cameraForward.xyz));
}

uint SelectSunShadowCascadeByViewDepth(const float viewDepth) {
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.x) {
        return 0u;
    }
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.y) {
        return 1u;
    }
    if (viewDepth <= sceneLighting.shadowCascadeDepthSplits.z) {
        return 2u;
    }
    return 3u;
}

float GetSunShadowCascadeNearDepth(const uint cascadeIndex) {
    if (cascadeIndex == 0u) {
        return max(sceneLighting.shadowCascadeBlendParams.y, 0.0);
    }
    return sceneLighting.shadowCascadeDepthSplits[cascadeIndex - 1u];
}

float ComputeSunShadowCascadeBlendWeight(const float viewDepth, const uint cascadeIndex) {
    const float blendFraction = clamp(sceneLighting.shadowCascadeBlendParams.x, 0.0, 0.5);
    if (blendFraction <= 0.0 || cascadeIndex + 1u >= kSunShadowCascadeCount) {
        return 0.0;
    }

    const float cascadeNearDepth = GetSunShadowCascadeNearDepth(cascadeIndex);
    const float cascadeFarDepth = sceneLighting.shadowCascadeDepthSplits[cascadeIndex];
    const float cascadeRange = max(cascadeFarDepth - cascadeNearDepth, 0.0001);
    const float blendStartDepth = max(cascadeFarDepth - cascadeRange * blendFraction, cascadeNearDepth);
    return smoothstep(blendStartDepth, cascadeFarDepth, viewDepth);
}

vec3 GetSunShadowCascadeDebugColor(const uint cascadeIndex) {
    if (cascadeIndex == 0u) {
        return vec3(0.15, 0.75, 1.0);
    }
    if (cascadeIndex == 1u) {
        return vec3(0.35, 1.0, 0.35);
    }
    if (cascadeIndex == 2u) {
        return vec3(1.0, 0.85, 0.25);
    }
    return vec3(1.0, 0.35, 0.25);
}

vec2 SampleSunShadowCascade(
const uint cascadeIndex,
const vec3 receiverPosition,
const float receiverDepthBias,
const float filterRadius,
const float shadowStrength) {
    const vec4 shadowClip = sceneLighting.sunShadowViewProjections[cascadeIndex] * vec4(receiverPosition, 1.0);
    const float shadowW = max(shadowClip.w, 0.0001);
    const vec3 shadowNdc = shadowClip.xyz / shadowW;
    const vec2 shadowUv = shadowNdc.xy * 0.5 + 0.5;

    if (shadowUv.x <= 0.0 || shadowUv.x >= 1.0 ||
    shadowUv.y <= 0.0 || shadowUv.y >= 1.0 ||
    shadowNdc.z <= 0.0 || shadowNdc.z >= 1.0) {
        return vec2(1.0, 0.0);
    }

    if (filterRadius <= 0.0) {
        const float lit = texture(sunShadowMap, vec4(shadowUv, float(cascadeIndex), shadowNdc.z - receiverDepthBias));
        return vec2(mix(1.0 - shadowStrength, 1.0, lit), 1.0);
    }

    // A small weighted PCF kernel is still cheap enough for the current CSM baseline,
    // but hides the nearest-neighbor texel staircase better than the old 3x3 box filter.
    const vec2 texelSize = 1.0 / vec2(textureSize(sunShadowMap, 0).xy);
    float litAccum = 0.0;
    float weightAccum = 0.0;
    const float pcfStepScale = filterRadius * 0.75;
    for (int offsetY = -2; offsetY <= 2; ++offsetY) {
        for (int offsetX = -2; offsetX <= 2; ++offsetX) {
            const float sampleWeight =
            (3.0 - abs(float(offsetX))) *
            (3.0 - abs(float(offsetY)));
            const vec2 sampleUv = shadowUv + vec2(offsetX, offsetY) * texelSize * pcfStepScale;
            litAccum += texture(sunShadowMap, vec4(sampleUv, float(cascadeIndex), shadowNdc.z - receiverDepthBias)) * sampleWeight;
            weightAccum += sampleWeight;
        }
    }
    const float lit = litAccum / max(weightAccum, 1.0);
    return vec2(mix(1.0 - shadowStrength, 1.0, lit), 1.0);
}

vec3 ComputeSunShadowSample(const vec3 worldPosition, const vec3 normal) {
    const float viewDepth = GetCameraViewDepth(worldPosition);
    const uint cascadeIndex = SelectSunShadowCascadeByViewDepth(viewDepth);
    const float shadowStrength = clamp(sceneLighting.sunShadowParams.x, 0.0, 1.0);
    if (shadowStrength <= 0.0) {
        return vec3(1.0, 1.0, float(cascadeIndex));
    }

    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const float depthBias = max(sceneLighting.sunShadowParams.y, 0.0);
    const float normalBias = max(sceneLighting.sunShadowParams.z, 0.0);
    const float filterRadius = max(sceneLighting.sunShadowParams.w, 0.0);
    const float nDotL = clamp(dot(normal, sunDirection), 0.0, 1.0);
    if (nDotL <= 0.02) {
        return vec3(1.0, 1.0, float(cascadeIndex));
    }

    const float shadowSlope = 1.0 - nDotL;
    // Keep the authored bias controls stable, but make them respond to the sun angle so
    // acne on grazing receivers does not require the same brute-force offset on flat tops.
    const float receiverDepthBias = depthBias * (1.0 + shadowSlope * 2.0);
    const float receiverNormalBias = normalBias * mix(0.25, 1.0, shadowSlope);
    const float receiverLightBias = max(normalBias * 0.5, depthBias * 4.0);
    const vec3 receiverPosition =
    worldPosition +
    normal * receiverNormalBias +
    sunDirection * receiverLightBias;
    vec2 shadowSample = SampleSunShadowCascade(
    cascadeIndex,
    receiverPosition,
    receiverDepthBias,
    filterRadius,
    shadowStrength);
    const float cascadeBlendWeight = ComputeSunShadowCascadeBlendWeight(viewDepth, cascadeIndex);
    if (cascadeBlendWeight > 0.0 && cascadeIndex + 1u < kSunShadowCascadeCount) {
        const vec2 nextShadowSample = SampleSunShadowCascade(
        cascadeIndex + 1u,
        receiverPosition,
        receiverDepthBias,
        filterRadius,
        shadowStrength);
        if (nextShadowSample.y > 0.5) {
            shadowSample.x = shadowSample.y > 0.5
            ? mix(shadowSample.x, nextShadowSample.x, cascadeBlendWeight)
            : nextShadowSample.x;
            shadowSample.y = 1.0;
        }
    }
    return vec3(shadowSample, float(cascadeIndex));
}

void main() {
    const MaterialVisual material = materials[inMaterialIndex];
    const vec3 normal = normalize(inNormal);
    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const vec3 sunColor = sceneLighting.sunColorAndIntensity.rgb * sceneLighting.sunColorAndIntensity.w;
    const vec3 sunShadowSample = ComputeSunShadowSample(inWorldPosition, normal);
    const float sunVisibility = sunShadowSample.x;
    const bool shadowCovered = sunShadowSample.y > 0.5;
    const uint sunShadowCascadeIndex = uint(sunShadowSample.z + 0.5);
    const float sunShadowViewDepth = GetCameraViewDepth(inWorldPosition);
    const vec3 shadowedSunColor = sunColor * sunVisibility;
    const vec3 viewDirection = normalize(pushConstants.cameraPosition.xyz - inWorldPosition + vec3(0.0001));
    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const float ao = clamp(material.surface.x, 0.0, 1.0);
    const float roughness = clamp(material.surface.y, 0.045, 1.0);
    const float metallic = clamp(material.surface.z, 0.0, 1.0);
    const float reflectance = clamp(material.surface.w, 0.0, 1.0);
    const float ambientStrength = clamp(material.shading.z, 0.0, 1.0);
    const float directDiffuseStrength = clamp(material.shading.w, 0.0, 1.0);
    const float nDotL = max(dot(normal, sunDirection), 0.0);
    const float nDotV = max(dot(normal, viewDirection), 0.0);
    const float wrappedDiffuse = clamp((dot(normal, sunDirection) + diffuseWrap) / (1.0 + diffuseWrap), 0.0, 1.0);
    const vec3 halfVector = normalize(sunDirection + viewDirection + vec3(0.0001));
    const float nDotH = max(dot(normal, halfVector), 0.0);
    const float hDotV = max(dot(halfVector, viewDirection), 0.0);
    const vec3 albedo = material.baseColor.rgb;
    const vec3 f0 = mix(vec3(0.16 * reflectance * reflectance), albedo, metallic);
    const vec3 fresnel = FresnelSchlick(hDotV, f0);
    const float distribution = DistributionGGX(nDotH, roughness);
    const float geometry = GeometrySmith(nDotV, nDotL, roughness);
    const vec3 specular = (distribution * geometry * fresnel) / max(4.0 * nDotL * nDotV, 0.0001);
    const vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo;
    const float ambientOcclusion = mix(0.35, 1.0, ao);
    // This is the cheap meshing-side local sky-visibility term, not screen-space AO/GI.
    const float geometryAmbientVisibility = clamp(inAmbientVisibility, 0.0, 1.0);
    const vec3 ambient =
    SampleEnvironmentDiffuse(normal) *
    albedo *
    ambientStrength *
    ambientOcclusion *
    geometryAmbientVisibility;
    const vec3 directDiffuse = diffuse * shadowedSunColor * wrappedDiffuse * directDiffuseStrength;
    const vec3 directSpecular = specular * shadowedSunColor * nDotL;
    const float grazing = pow(1.0 - nDotV, 5.0);
    const vec3 mediumTint = material.medium.rgb;
    const vec3 grazingTint = mediumTint * material.medium.w * grazing * (1.0 - metallic) * 0.12;
    vec3 color = ambient + directDiffuse + directSpecular + grazingTint;

    if (material.medium.w > 0.0) {
        float transmission = material.medium.w * mix(0.35, 1.0, wrappedDiffuse);
        if (IsGlass(inMaterialIndex)) {
            transmission *= mix(0.75, 1.15, grazing);
        } else if (IsFluid(inMaterialIndex)) {
            transmission *= 0.60 + grazing * 0.40;
        }
        color += sceneLighting.horizonColorAndFogStart.rgb * mediumTint * transmission;
    }

    if (material.shading.y > 0.0) {
        color += albedo * material.shading.y;
    }

    const float viewDistance = length(pushConstants.cameraPosition.xyz - inWorldPosition);
    const float fogDensity = max(sceneLighting.skyColorAndFogDensity.w, 0.0);
    const float fogStart = max(sceneLighting.horizonColorAndFogStart.w, 0.0);
    const float fogMax = clamp(sceneLighting.groundColorAndFogMax.w, 0.0, 1.0);
    const vec3 fogColor = mix(
    sceneLighting.horizonColorAndFogStart.rgb,
    sceneLighting.skyColorAndFogDensity.rgb,
    clamp(normal.y * 0.5 + 0.5, 0.0, 1.0));
    const float fog = clamp(max(viewDistance - fogStart, 0.0) * fogDensity, 0.0, fogMax) * material.shading.x;
    const uint lightingDebugView = uint(sceneLighting.postProcess.w + 0.5);

    if (lightingDebugView == 1u) {
        color = ambient;
    } else if (lightingDebugView == 2u) {
        color = directDiffuse + directSpecular + grazingTint;
    } else if (lightingDebugView == 3u) {
        outColor = vec4(shadowCovered ? vec3(sunVisibility) : vec3(1.0, 0.15, 0.10), 1.0);
        return;
    } else if (lightingDebugView == 4u) {
        vec3 cascadeColor = GetSunShadowCascadeDebugColor(sunShadowCascadeIndex);
        const float cascadeBlendWeight = ComputeSunShadowCascadeBlendWeight(sunShadowViewDepth, sunShadowCascadeIndex);
        if (cascadeBlendWeight > 0.0 && sunShadowCascadeIndex + 1u < kSunShadowCascadeCount) {
            cascadeColor = mix(
            cascadeColor,
            GetSunShadowCascadeDebugColor(sunShadowCascadeIndex + 1u),
            cascadeBlendWeight);
        }
        outColor = vec4(shadowCovered ? mix(cascadeColor * 0.28, cascadeColor, sunVisibility) : vec3(1.0, 0.15, 0.10), 1.0);
        return;
    } else if (lightingDebugView == 5u) {
        color = vec3(fog);
    }

    if (lightingDebugView != 5u) {
        color = mix(color, fogColor, fog);
    }
    color *= max(sceneLighting.postProcess.x, 0.0);
    color = ApplyToneMap(color);
    color = ApplyColorGrading(color);

    outColor = vec4(color, material.baseColor.a);
}
