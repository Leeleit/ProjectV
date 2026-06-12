#version 460

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
    // A1 (4.1 greedy meshing): (width, height) in 8 bits each for the
    // in-plane quad size. See `decisions.md §25`.
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

layout(set = 0, binding = 3, std430) readonly buffer SceneLightingBuffer {
    vec4 skyColorAndFogDensity;
    vec4 horizonColorAndFogStart;
    vec4 groundColorAndFogMax;
    vec4 sunColorAndIntensity;
    vec4 sunDirectionAndWrap;
    vec4 postProcess;
    vec4 sunShadowParams;
    vec4 sunContactShadowParams;
    vec4 ambientOcclusionParams;
    mat4 sunShadowViewProjections[4];
    vec4 colorGrading;
    vec4 exposureControl;
    vec4 shadowCascadeDepthSplits;
    vec4 shadowCascadeBlendParams;
    vec4 localPointLightPositionAndRadius;
    vec4 localPointLightColorAndIntensity;
    vec4 localPointLightParams;
    // TAA contract (mirrors VoxelSceneLighting, see agent/decisions.md §18):
    // Currently shadow pass only consumes sceneLighting fields it already uses;
    // the TAA fields are present for byte-layout parity with voxel.frag /
    // voxel_mesh.comp. Layout is enforced by static_assert on the C++ side.
    vec4 taaParams;
    mat4 prevViewProjectionMatrix;
    vec4 taaHistoryParams;
    // 1.5 anti-flicker layer history params (texelX, texelY, valid,
    // blendFactor). Mirrors the C++ `VoxelSceneLighting` byte layout.
    // This shader doesn't read it, but the field is declared so the
    // std430 layout matches the C++ struct byte-for-byte.
    vec4 taaLayerHistoryParams;
} sceneLighting;

layout(push_constant) uniform ShadowPushConstants {
    uint cascadeIndex;
} pushConstants;

layout(location = 0) flat out uint outMaterialIndex;

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

// A1 (4.1 greedy meshing): scale the unit corner offset by the merged
// quad extents. Mirror of `voxel.vert::ApplyGreedyScale`.
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
    const uint chunkIndex = chunkIndexMaterial & 0x00FFFFFFu;
    const uint materialIndex = (chunkIndexMaterial >> 24u) & 0xFFu;
    const uvec3 localVoxelCoord = uvec3(
    localVoxelFace & 0xFFu,
    (localVoxelFace >> 8u) & 0xFFu,
    (localVoxelFace >> 16u) & 0xFFu);

    // A1 (4.1 greedy meshing): decode merged-quad extents.
    const uvec2 quadExtents = uvec2(
    packedFace.packedExtents & 0xFFu,
    (packedFace.packedExtents >> 8u) & 0xFFu);

    const uint triangleVertexIndex = uint(gl_VertexIndex) % 6u;
    const uint cornerIndex = DecodeTriangleCornerIndex(triangleVertexIndex);
    const uvec3 unitOffset = GetFaceCornerOffset(faceIndex, cornerIndex);
    const uvec3 scaledOffset = ApplyGreedyScale(faceIndex, unitOffset, quadExtents);
    const vec3 localCornerPosition = vec3(localVoxelCoord + scaledOffset);
    const vec3 worldPosition = vec3(chunkDescriptors[chunkIndex].chunkOrigin.xyz) + localCornerPosition;

    outMaterialIndex = materialIndex;
    gl_Position = sceneLighting.sunShadowViewProjections[pushConstants.cascadeIndex] * vec4(worldPosition, 1.0);
}
