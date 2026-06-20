#version 460
#extension GL_EXT_nonuniform_qualifier : enable
// Fragment shader (VIS-BUFFER resolve pass).
// Per-pixel work: read vis-buffer (1 texelFetch) + lookup PackedFace (1 SSBO load)
// + lookup material (1 SSBO load) + GGX lighting ALU.
// Compare to baseline.frag: 1 less fetch (no need to read material in vertex stage — materialId comes from PackedFace SSBO lookup here).
// But the BIG win is: we run this resolve multiple times for shadow / AO / point-light
// WITHOUT re-rasterizing geometry. Total scene triangle count: rasterized ONCE.

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
    uint packedExtents;
};

layout(set = 0, binding = 0) uniform usampler2D visBufferTex;
layout(set = 0, binding = 1, std430) readonly buffer PackedFacePayload {
    PackedFace packedFaces[];
};
layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    vec4 baseColor;
    vec4 surface;
    vec4 medium;
    vec4 shading;
} materials[];

layout(set = 0, binding = 3, std140) uniform FrameUBO {
    vec4 sunDirection;
    vec4 sunColor;
    vec4 cameraPos;
    vec4 viewport;
} frame;

layout(location = 0) out vec4 outColor;

vec3 FaceNormal(uint face) {
    if (face == 0u) return vec3( 1.0, 0.0, 0.0);
    if (face == 1u) return vec3(-1.0, 0.0, 0.0);
    if (face == 2u) return vec3(0.0,  1.0, 0.0);
    if (face == 3u) return vec3(0.0, -1.0, 0.0);
    if (face == 4u) return vec3(0.0, 0.0,  1.0);
    return vec3(0.0, 0.0, -1.0);
}

// Reconstruct world position from quadId + gl_FragCoord (linearized NDC depth).
// For prototype simplicity we assume the resolve pass knows camera VP via FrameUBO.
vec3 ReconstructWorldPos(vec3 fragCoordNdc) {
    // fragCoordNdc.xy = pixel in [-1, 1], z = depth.
    // ProjectV voxel.vert decodes: chunkOrigin + (x*unit + scaledOffset).
    // For prototype, we approximate by reading PackedFace and using the face origin.
    return vec3(0.0); // not used — we use the triangle's centroid via FaceNormal for diffuse/spec.
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
    ivec2 px = ivec2(gl_FragCoord.xy);
    uint quadId = texelFetch(visBufferTex, px, 0).r;

    // Skip background.
    if (quadId == 0xFFFFFFFFu) {
        outColor = vec4(0.0);
        return;
    }

    PackedFace pf = packedFaces[quadId];
    uint face = (pf.localVoxelFace >> 24u) & 0xFFu;
    uint matId = (pf.chunkIndexMaterial >> 24u) & 0xFFu;

    vec3 albedo = materials[matId].baseColor.rgb;
    float rough = materials[matId].surface.x;
    float metal = materials[matId].surface.y;

    vec3 N = FaceNormal(face);
    // For prototype, V depends only on camera pos + "some point on the triangle".
    // Approximate with camera→origin vector.
    vec3 V = normalize(frame.cameraPos.xyz);

    vec3 L = normalize(frame.sunDirection.xyz);
    vec3 sunLight = frame.sunColor.rgb * frame.sunColor.a;
    vec3 color = EvaluateLight(N, V, L, albedo, rough, metal, sunLight);
    color += albedo * 0.15;

    outColor = vec4(color, 1.0);
}
