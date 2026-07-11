#version 460

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
    uint packedExtents;
};

layout(set = 0, binding = 0, std430) readonly buffer PackedFacePayload {
    PackedFace packedFaces[];
};

struct ChunkDescriptor {
    ivec4 chunkOrigin;
    uvec4 chunkExtentAndNonAir;
    uvec4 voxelDataInfo;
    uvec4 drawRanges;
};

layout(set = 0, binding = 1, std430) readonly buffer PackedChunkDescriptors {
    ChunkDescriptor chunkDescriptors[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 cameraForward;
    ivec4 worldMinAndChunkSize;
    uvec4 chunkGridAndFlags;
} pushConstants;


layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) flat out uint outMaterialIndex;
layout(location = 3) out float outAmbientVisibility;
layout(location = 4) out vec2 outQuadUV;

vec3 DecodeFaceNormal(const uint faceIndex) {
    switch (faceIndex) {
        case 0u: return vec3(1.0, 0.0, 0.0);
        case 1u: return vec3(-1.0, 0.0, 0.0);
        case 2u: return vec3(0.0, 1.0, 0.0);
        case 3u: return vec3(0.0, -1.0, 0.0);
        case 4u: return vec3(0.0, 0.0, 1.0);
        default : return vec3(0.0, 0.0, -1.0);
    }
}

uint DecodeTriangleCornerIndex(const uint triangleVertexIndex) {
    switch (triangleVertexIndex) {
        case 0u: return 0u;
        case 1u: return 1u;
        case 2u: return 2u;
        case 3u: return 0u;
        case 4u: return 2u;
        default : return 3u;
    }
}

uvec3 GetFaceCornerOffset(const uint faceIndex, const uint cornerIndex) {
    switch (faceIndex) {
        case 0u:
        return cornerIndex == 0u ? uvec3(1u, 0u, 0u) :
        cornerIndex == 1u ? uvec3(1u, 1u, 0u) :
        cornerIndex == 2u ? uvec3(1u, 1u, 1u) :
        uvec3(1u, 0u, 1u);
        case 1u:
        return cornerIndex == 0u ? uvec3(0u, 0u, 1u) :
        cornerIndex == 1u ? uvec3(0u, 1u, 1u) :
        cornerIndex == 2u ? uvec3(0u, 1u, 0u) :
        uvec3(0u, 0u, 0u);
        case 2u:
        return cornerIndex == 0u ? uvec3(0u, 1u, 0u) :
        cornerIndex == 1u ? uvec3(0u, 1u, 1u) :
        cornerIndex == 2u ? uvec3(1u, 1u, 1u) :
        uvec3(1u, 1u, 0u);
        case 3u:
        return cornerIndex == 0u ? uvec3(0u, 0u, 1u) :
        cornerIndex == 1u ? uvec3(0u, 0u, 0u) :
        cornerIndex == 2u ? uvec3(1u, 0u, 0u) :
        uvec3(1u, 0u, 1u);
        case 4u:
        return cornerIndex == 0u ? uvec3(1u, 0u, 1u) :
        cornerIndex == 1u ? uvec3(1u, 1u, 1u) :
        cornerIndex == 2u ? uvec3(0u, 1u, 1u) :
        uvec3(0u, 0u, 1u);
        default :
        return cornerIndex == 0u ? uvec3(0u, 0u, 0u) :
        cornerIndex == 1u ? uvec3(0u, 1u, 0u) :
        cornerIndex == 2u ? uvec3(1u, 1u, 0u) :
        uvec3(1u, 0u, 0u);
    }
}


uvec3 ApplyGreedyScale(const uint faceIndex, const uvec3 unitOffset, const uvec2 quadExtents) {
    if (faceIndex == 0u || faceIndex == 1u) {
        return uvec3(unitOffset.x, unitOffset.y * quadExtents.x, unitOffset.z * quadExtents.y);
    } else if (faceIndex == 2u || faceIndex == 3u) {
        return uvec3(unitOffset.x * quadExtents.x, unitOffset.y, unitOffset.z * quadExtents.y);
    } else {
        return uvec3(unitOffset.x * quadExtents.x, unitOffset.y * quadExtents.y, unitOffset.z);
    }
}

void main() {
    const PackedFace packedFace = packedFaces[uint(gl_InstanceIndex)];
    const uint localVoxelFace = packedFace.localVoxelFace;
    const uint chunkIndexMaterial = packedFace.chunkIndexMaterial;

    const uint faceIndex = (localVoxelFace >> 24u) & 0xFFu;
    const uint materialIndex = (chunkIndexMaterial >> 24u) & 0xFFu;
    const uint chunkIndex = chunkIndexMaterial & 0x00FFFFFFu;
    const uvec3 localVoxelCoord = uvec3(
    localVoxelFace & 0xFFu,
    (localVoxelFace >> 8u) & 0xFFu,
    (localVoxelFace >> 16u) & 0xFFu);


    const uvec2 quadExtents = uvec2(
    packedFace.packedExtents & 0xFFu,
    (packedFace.packedExtents >> 8u) & 0xFFu);

    const uint triangleVertexIndex = uint(gl_VertexIndex) % 6u;
    const uint cornerIndex = DecodeTriangleCornerIndex(triangleVertexIndex);
    const uvec3 unitOffset = GetFaceCornerOffset(faceIndex, cornerIndex);
    const uvec3 scaledOffset = ApplyGreedyScale(faceIndex, unitOffset, quadExtents);
    const vec3 localCornerPosition = vec3(localVoxelCoord + scaledOffset);
    const vec3 worldPosition = vec3(chunkDescriptors[chunkIndex].chunkOrigin.xyz) + localCornerPosition;

    gl_Position = pushConstants.viewProjection * vec4(worldPosition, 1.0);
    outNormal = DecodeFaceNormal(faceIndex);
    outWorldPosition = worldPosition;
    outMaterialIndex = materialIndex;
    outQuadUV = vec2[4](vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0))[cornerIndex];

    outAmbientVisibility = 1.0;
}
