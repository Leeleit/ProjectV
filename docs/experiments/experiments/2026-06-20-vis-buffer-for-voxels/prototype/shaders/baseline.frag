#version 460
#extension GL_EXT_nonuniform_qualifier : enable
// Fragment shader (BASELINE — inline lighting).
// Computes GGX + sun lighting + tone mapping in one pass.
// Per-pixel cost ≈ 1 SSBO fetch (material SSBO by matId) + ~30 ALU.
// This is the "inline shading" cost that vis-buffer tries to amortize across N light passes.

layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    vec4 baseColor;
    vec4 surface;
    vec4 medium;
    vec4 shading;
} materials[];

layout(set = 0, binding = 3, std140) uniform FrameUBO {
    vec4 sunDirection; // xyz direction, w unused
    vec4 sunColor; // rgb, a intensity
    vec4 cameraPos; // xyz, w unused
    vec4 viewport; // xy = res, zw = 1/res
} frame;

layout(location = 0) flat in uint inFaceIndex;
layout(location = 1) flat in uint inMaterialId;
layout(location = 2) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

vec3 FaceNormal(uint face) {
    if (face == 0u) return vec3( 1.0, 0.0, 0.0);
    if (face == 1u) return vec3(-1.0, 0.0, 0.0);
    if (face == 2u) return vec3(0.0,  1.0, 0.0);
    if (face == 3u) return vec3(0.0, -1.0, 0.0);
    if (face == 4u) return vec3(0.0, 0.0,  1.0);
    return vec3(0.0, 0.0, -1.0);
}

vec3 EvaluateLight(vec3 N, vec3 V, vec3 L, vec3 albedo, float rough, float metal, vec3 lightColor) {
    vec3 H = normalize(L + V + vec3(1e-4));
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    float a = rough * rough;
    float a2 = a * a;
    float denom = max(NdotH * NdotH * (a2 - 1.0) + 1.0, 1e-4);
    float D = a2 / (3.14159265 * denom * denom);
    float k = (rough + 1.0); k = (k * k) / 8.0;
    float Gv = NdotV / max(NdotV * (1.0 - k) + k, 1e-4);
    float Gl = NdotL / max(NdotL * (1.0 - k) + k, 1e-4);
    float G = Gv * Gl;
    vec3 f0 = mix(vec3(0.04), albedo, metal);
    vec3 F = f0 + (1.0 - f0) * pow(1.0 - clamp(HdotV, 0.0, 1.0), 5.0);
    vec3 spec = (D * G * F) / max(4.0 * NdotL * NdotV, 1e-4);
    vec3 diff = (vec3(1.0) - F) * (1.0 - metal) * albedo;
    return (diff + spec) * lightColor * NdotL;
}

void main() {
    vec3 albedo = materials[inMaterialId].baseColor.rgb;
    float rough = materials[inMaterialId].surface.x;
    float metal = materials[inMaterialId].surface.y;

    vec3 N = FaceNormal(inFaceIndex);
    vec3 V = normalize(frame.cameraPos.xyz - inWorldPos);
    vec3 L = normalize(frame.sunDirection.xyz);
    vec3 sunLight = frame.sunColor.rgb * frame.sunColor.a;

    vec3 color = EvaluateLight(N, V, L, albedo, rough, metal, sunLight);

    // Simple environment ambient.
    vec3 ambient = albedo * 0.15;
    color += ambient;

    outColor = vec4(color, 1.0);
}
