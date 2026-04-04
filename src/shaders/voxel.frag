#version 460

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inDepth;
layout(location = 3) in float inMaterialKind;

layout(location = 0) out vec4 outColor;

void main() {
    const vec3 normal = normalize(inNormal);
    const vec3 skyColor = vec3(0.73, 0.84, 0.96);
    const vec3 groundColor = vec3(0.34, 0.36, 0.40);
    const vec3 sunColor = vec3(1.00, 0.97, 0.92);
    const vec3 lightDirection = normalize(vec3(-0.35, 0.80, -0.45));
    const bool isGlass = inMaterialKind > 0.5 && inMaterialKind < 1.5;
    const bool isFluid = inMaterialKind > 1.5 && inMaterialKind < 2.5;

    const float hemisphere = normal.y * 0.5 + 0.5;
    const vec3 ambient = mix(groundColor, skyColor, hemisphere);

    const float ndotl = dot(normal, lightDirection);
    const float wrappedDiffuse = clamp((ndotl + 0.35) / 1.35, 0.0, 1.0);
    vec3 lighting = ambient * 0.70 + sunColor * (wrappedDiffuse * 0.55);

    if (isFluid) {
        // Fluid should read as a cohesive volume, not as a set of harshly lit voxel cards.
        const float fluidDiffuse = clamp((ndotl + 0.85) / 1.85, 0.0, 1.0);
        lighting = ambient * 0.92 + skyColor * 0.18 + sunColor * (fluidDiffuse * 0.18);
    } else if (isGlass) {
        lighting = ambient * 0.82 + sunColor * (wrappedDiffuse * 0.28);
    }

    vec3 color = inColor.rgb * lighting;
    if (isFluid) {
        color *= 1.06;
    }

    const vec3 fogColor = skyColor;
    const float fog = clamp((inDepth + 0.05) * 0.12, 0.0, 0.35);
    color = mix(color, fogColor, fog);

    outColor = vec4(color, inColor.a);
}
