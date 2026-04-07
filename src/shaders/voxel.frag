#version 460

struct MaterialVisual {
    vec4 baseColor;
    vec4 lighting;
    vec4 edgeTintAndPower;
    vec4 shadingExtras;
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
} sceneLighting;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
} pushConstants;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPosition;
layout(location = 2) flat in uint inMaterialIndex;

layout(location = 0) out vec4 outColor;

bool IsGlass(const uint materialIndex) {
    return materialIndex == 1u;
}

bool IsFluid(const uint materialIndex) {
    return materialIndex == 2u;
}

vec3 SampleAmbientGradient(const vec3 normal) {
    const float skyBlend = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = mix(
    sceneLighting.groundColorAndFogMax.rgb,
    sceneLighting.skyColorAndFogDensity.rgb,
    skyBlend);
    const float horizonBlend = clamp(1.0 - abs(normal.y), 0.0, 1.0);
    ambient = mix(ambient, sceneLighting.horizonColorAndFogStart.rgb, horizonBlend * 0.55);
    return ambient;
}

void main() {
    const MaterialVisual material = materials[inMaterialIndex];
    const vec3 normal = normalize(inNormal);
    const vec3 sunDirection = normalize(sceneLighting.sunDirectionAndWrap.xyz);
    const vec3 sunColor = sceneLighting.sunColorAndIntensity.rgb * sceneLighting.sunColorAndIntensity.w;
    const vec3 viewDirection = normalize(pushConstants.cameraPosition.xyz - inWorldPosition + vec3(0.0001));
    const vec3 halfVector = normalize(sunDirection + viewDirection);
    const float diffuseWrap = max(sceneLighting.sunDirectionAndWrap.w, 0.0);
    const float wrappedDiffuse = clamp((dot(normal, sunDirection) + diffuseWrap) / (1.0 + diffuseWrap), 0.0, 1.0);
    const float fresnelPower = max(material.edgeTintAndPower.w, 1.0);
    const float fresnel = pow(clamp(1.0 - max(dot(normal, viewDirection), 0.0), 0.0, 1.0), fresnelPower);

    float specular = 0.0;
    if (material.lighting.z > 0.0) {
        specular = pow(max(dot(normal, halfVector), 0.0), material.lighting.w) * material.lighting.z;
    }

    const vec3 ambient = SampleAmbientGradient(normal) * material.lighting.x;
    vec3 color =
    material.baseColor.rgb * (ambient + sunColor * (wrappedDiffuse * material.lighting.y + specular));
    color += material.edgeTintAndPower.rgb * (material.shadingExtras.x * fresnel);

    if (material.shadingExtras.y > 0.0) {
        float transmission = material.shadingExtras.y * mix(0.35, 1.0, wrappedDiffuse);
        if (IsGlass(inMaterialIndex)) {
            transmission *= mix(0.75, 1.15, fresnel);
        } else if (IsFluid(inMaterialIndex)) {
            transmission *= 0.60 + fresnel * 0.40;
        }
        color += sceneLighting.horizonColorAndFogStart.rgb * transmission;
    }

    if (material.shadingExtras.w > 0.0) {
        color += material.baseColor.rgb * material.shadingExtras.w;
    }

    const float viewDistance = length(pushConstants.cameraPosition.xyz - inWorldPosition);
    const float fogDensity = max(sceneLighting.skyColorAndFogDensity.w, 0.0);
    const float fogStart = max(sceneLighting.horizonColorAndFogStart.w, 0.0);
    const float fogMax = clamp(sceneLighting.groundColorAndFogMax.w, 0.0, 1.0);
    const vec3 fogColor = mix(
    sceneLighting.horizonColorAndFogStart.rgb,
    sceneLighting.skyColorAndFogDensity.rgb,
    clamp(normal.y * 0.5 + 0.5, 0.0, 1.0));
    const float fog = clamp(max(viewDistance - fogStart, 0.0) * fogDensity, 0.0, fogMax) * material.shadingExtras.z;
    color = mix(color, fogColor, fog);

    outColor = vec4(color, material.baseColor.a);
}
