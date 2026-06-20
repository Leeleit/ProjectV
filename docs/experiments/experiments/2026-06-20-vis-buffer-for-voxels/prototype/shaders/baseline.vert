#version 460
// Vertex shader (BASELINE — forward+, inline lighting).
// Decodes PackedFace (matching ProjectV's src/shaders/voxel.vert layout).
// Per-quad instance → 6 vertices (one quad, 2 triangles).
// Pass-through faceIndex + materialId to fragment shader for lighting computation.

struct PackedFace {
    uint localVoxelFace; // [face(8) | z(8) | y(8) | x(8)]
    uint chunkIndexMaterial; // [mat(8) | chunk(24)]
    uint lightingData;
    uint packedExtents; // [extV(8) | extU(8) | 0(16)]
};
struct ChunkDescriptor {
    ivec4 chunkOrigin;
    uvec4 chunkExtentAndNonAir;
    uvec4 voxelDataInfo;
    uvec4 drawRanges;
};

layout(set = 0, binding = 0, std430) readonly buffer PackedFacePayload {
    PackedFace packedFaces[];
};
layout(set = 0, binding = 1, std430) readonly buffer PackedChunkDescriptors {
    ChunkDescriptor chunkDescriptors[];
};
layout(set = 0, binding = 2, std430) readonly buffer MaterialVisualBuffer {
    vec4 baseColor;
    vec4 surface;
    vec4 medium;
    vec4 shading;
} materials[]; // dynamic array of 64-byte MaterialVisual entries.

layout(push_constant, std430) uniform PushConstants {
    mat4 viewProjection;
} pc;

layout(location = 0) flat out uint outFaceIndex;
layout(location = 1) flat out uint outMaterialId;
layout(location = 2) out vec3 outWorldPos;

uint DecodeTriCorner(uint vi) {
    if (vi == 0u) return 0u;
    if (vi == 1u) return 1u;
    if (vi == 2u) return 2u;
    if (vi == 3u) return 0u;
    if (vi == 4u) return 2u;
    return 3u;
}

uvec3 FaceCornerOffset(uint face, uint corner) {
    if (face == 0u)
        return corner == 0u ? uvec3(1u, 0u, 0u) :
               corner == 1u ? uvec3(1u, 1u, 0u) :
               corner == 2u ? uvec3(1u, 1u, 1u) : uvec3(1u, 0u, 1u);
    if (face == 1u)
        return corner == 0u ? uvec3(0u, 0u, 1u) :
               corner == 1u ? uvec3(0u, 1u, 1u) :
               corner == 2u ? uvec3(0u, 1u, 0u) : uvec3(0u, 0u, 0u);
    if (face == 2u)
        return corner == 0u ? uvec3(0u, 1u, 0u) :
               corner == 1u ? uvec3(0u, 1u, 1u) :
               corner == 2u ? uvec3(1u, 1u, 1u) : uvec3(1u, 1u, 0u);
    if (face == 3u)
        return corner == 0u ? uvec3(0u, 0u, 1u) :
               corner == 1u ? uvec3(0u, 0u, 0u) :
               corner == 2u ? uvec3(1u, 0u, 0u) : uvec3(1u, 0u, 1u);
    if (face == 4u)
        return corner == 0u ? uvec3(1u, 0u, 1u) :
               corner == 1u ? uvec3(1u, 1u, 1u) :
               corner == 2u ? uvec3(0u, 1u, 1u) : uvec3(0u, 0u, 1u);
    return corner == 0u ? uvec3(0u, 0u, 0u) :
           corner == 1u ? uvec3(0u, 1u, 0u) :
           corner == 2u ? uvec3(1u, 1u, 0u) : uvec3(1u, 0u, 0u);
}

uvec3 ApplyScale(uint face, uvec3 unit, uvec2 ext) {
    if (face == 0u || face == 1u)
        return uvec3(unit.x, unit.y * ext.x, unit.z * ext.y);
    if (face == 2u || face == 3u)
        return uvec3(unit.x * ext.x, unit.y, unit.z * ext.y);
    return uvec3(unit.x * ext.x, unit.y * ext.y, unit.z);
}

void main() {
    PackedFace pf = packedFaces[gl_InstanceIndex];
    uint face = (pf.localVoxelFace >> 24u) & 0xFFu;
    uint x = pf.localVoxelFace & 0xFFu;
    uint y = (pf.localVoxelFace >> 8u) & 0xFFu;
    uint z = (pf.localVoxelFace >> 16u) & 0xFFu;
    uint chunkIdx = pf.chunkIndexMaterial & 0x00FFFFFFu;
    uint matId = (pf.chunkIndexMaterial >> 24u) & 0xFFu;
    uvec2 ext = uvec2(pf.packedExtents & 0xFFu, (pf.packedExtents >> 8u) & 0xFFu);

    uint corner = DecodeTriCorner(gl_VertexIndex % 6u);
    uvec3 off = FaceCornerOffset(face, corner);
    uvec3 scaled = ApplyScale(face, off, ext);
    vec3 local = vec3(x + scaled.x, y + scaled.y, z + scaled.z);
    vec3 worldPos = vec3(chunkDescriptors[chunkIdx].chunkOrigin.xyz) + local;

    outFaceIndex = face;
    outMaterialId = matId;
    outWorldPos = worldPos;
    gl_Position = pc.viewProjection * vec4(worldPos, 1.0);
}
