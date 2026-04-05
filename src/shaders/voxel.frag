#version 460

struct MaterialVisual {
    vec4 baseColor;
    float ambient;
    float diffuse;
    float specular;
    float specularPower;
};

layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    MaterialVisual materials[];
};

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

void main() {
    const MaterialVisual material = materials[inMaterialIndex];
    const vec3 normal = normalize(inNormal);
    const vec3 skyColor = vec3(0.73, 0.84, 0.96);
    const vec3 groundColor = vec3(0.34, 0.36, 0.40);
    const vec3 sunColor = vec3(1.00, 0.97, 0.92);
    const vec3 lightDirection = normalize(vec3(-0.35, 0.80, -0.45));
    const vec3 viewDirection = normalize(pushConstants.cameraPosition.xyz - inWorldPosition);
    const vec3 halfVector = normalize(lightDirection + viewDirection);

    const float hemisphere = normal.y * 0.5 + 0.5;
    const vec3 hemisphereAmbient = mix(groundColor, skyColor, hemisphere);

    float wrappedDiffuse = clamp((dot(normal, lightDirection) + 0.35) / 1.35, 0.0, 1.0);
    if (IsFluid(inMaterialIndex)) {
        wrappedDiffuse = clamp((dot(normal, lightDirection) + 0.85) / 1.85, 0.0, 1.0);
    }

    float specular = 0.0;
    if (material.specular > 0.0) {
        specular = pow(max(dot(normal, halfVector), 0.0), material.specularPower) * material.specular;
    }

    vec3 lighting =
    hemisphereAmbient * material.ambient +
    sunColor * (wrappedDiffuse * material.diffuse) +
    sunColor * specular;

    if (IsGlass(inMaterialIndex)) {
        lighting += skyColor * 0.08;
    } else if (IsFluid(inMaterialIndex)) {
        lighting += skyColor * 0.14;
    }

    vec3 color = material.baseColor.rgb * lighting;

    const vec3 fogColor = skyColor;
    const float fog = clamp((gl_FragCoord.z + 0.05) * 0.12, 0.0, 0.35);
    color = mix(color, fogColor, fog);

    outColor = vec4(color, material.baseColor.a);
}
