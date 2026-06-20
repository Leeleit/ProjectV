#version 460
// Vertex shader (VIS-BUFFER geometry pass).
// Same world-position decode as baseline, but fragment output is just primitiveId.
// Vis-buffer per pixel: 32-bit quadId (= gl_InstanceIndex).
// In production (Nanite): 64-bit vis-buffer = 32 depth + 32 ID. We only need ID for
// voxel scenes because all attributes are constant-per-quad.

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
    uint packedExtents;
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

layout(push_constant, std430) uniform PushConstants {
    mat4 viewProjection;
} pc;

layout(location = 0) flat out uint outQuadId;

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
    uvec2 ext = uvec2(pf.packedExtents & 0xFFu, (pf.packedExtents >> 8u) & 0xFFu);

    uint corner = DecodeTriCorner(gl_VertexIndex % 6u);
    uvec3 off = FaceCornerOffset(face, corner);
    uvec3 scaled = ApplyScale(face, off, ext);
    vec3 local = vec3(x + scaled.x, y + scaled.y, z + scaled.z);
    vec3 worldPos = vec3(chunkDescriptors[chunkIdx].chunkOrigin.xyz) + local;

    outQuadId = gl_InstanceIndex;
    gl_Position = pc.viewProjection * vec4(worldPos, 1.0);
}
