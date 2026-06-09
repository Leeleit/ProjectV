#version 460

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
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

    const uint triangleVertexIndex = uint(gl_VertexIndex) % 6u;
    const uint cornerIndex = DecodeTriangleCornerIndex(triangleVertexIndex);
    const vec3 localCornerPosition = vec3(localVoxelCoord + GetFaceCornerOffset(faceIndex, cornerIndex));
    const vec3 worldPosition = vec3(chunkDescriptors[chunkIndex].chunkOrigin.xyz) + localCornerPosition;

    outMaterialIndex = materialIndex;
    gl_Position = sceneLighting.sunShadowViewProjections[pushConstants.cascadeIndex] * vec4(worldPosition, 1.0);
}
