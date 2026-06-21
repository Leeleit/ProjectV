// voxel_minimal.frag — Representative ProjectV fragment shader (GLSL).
// Mimics structure of src/shaders/voxel.frag (35 KB): consumes vertex outputs,
// samples material atlas (texture), computes Lambert lighting from single UBO
// light, outputs final color with TAA-friendly dithering noise.
//
// Target: Vulkan 1.4, glslc, --target-env=vulkan1.4

#version 460

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) flat in uint inMaterialIndex;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0, std140) uniform LightingUBO {
    vec4 sunDirection;
    vec4 sunColor;
    vec4 ambientColor;
    vec4 cameraPosition;
    uint materialCount;
    uint pad0;
    uint pad1;
    uint pad2;
} lighting;

layout(set = 1, binding = 1) uniform sampler2D materialAtlas;

layout(set = 1, binding = 2, std430) readonly buffer MaterialTable {
    uint albedoOffsets[];
} materials;

float interleavedGradientNoise(vec2 pos) {
    return fract(52.9829189 * fract(dot(pos, vec2(0.06711056, 0.00583715))));
}

void main() {
    vec2 atlasUv = inUv + vec2(float(materials.albedoOffsets[inMaterialIndex] & 0xFFFFu),
                               float((materials.albedoOffsets[inMaterialIndex] >> 16u) & 0xFFFFu)) * (1.0 / 4096.0);
    vec4 albedo = texture(materialAtlas, atlasUv);

    vec3 N = normalize(inNormal);
    vec3 L = normalize(-lighting.sunDirection.xyz);
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = albedo.rgb * lighting.sunColor.rgb * NdotL;
    vec3 ambient = albedo.rgb * lighting.ambientColor.rgb;
    vec3 color = diffuse + ambient;

    float dither = interleavedGradientNoise(gl_FragCoord.xy) - 0.5;
    outColor = vec4(color + dither * (1.0 / 255.0), albedo.a);
}