// voxel_mesh_minimal.mesh.hlsl — HLSL port of voxel_mesh_minimal.mesh.
// Target: shader model 6.5, -fspv-extension=SPV_EXT_mesh_shader.
// Uses DXC mesh-shader SPIR-V emission with groupshared vertex array and
// payload struct pattern (simplified for compilation testing).

struct PushConstants {
    float4x4 viewProjection;
    int4     worldMinAndChunkSize;
    int4     worldMaxAndChunkCount;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

struct ChunkDescriptor {
    float4 chunkOrigin;
    int4   chunkExtentAndNonAir;
    uint4  voxelDataInfo;
};

[[vk::binding(0, 0)]]
StructuredBuffer<ChunkDescriptor> descriptors;

[[vk::binding(1, 0)]]
StructuredBuffer<uint> voxels;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> visibleIds;

#define MAX_QUADS 64u

uint decodeMaterial(uint4 voxelInfo, uint localIndex) {
    if (localIndex >= voxelInfo.y) return 0u;
    uint wordIndex = voxelInfo.x + localIndex / 4u;
    uint shift = (localIndex & 3u) * 8u;
    return (voxels[wordIndex] >> shift) & 0xFFu;
}

struct MeshVertex {
    float3 outNormal        : NORMAL;
    float3 outWorldPos      : TEXCOORD0;
    uint   outMaterialIndex : TEXCOORD1;
};

groupshared MeshVertex sharedVerts[256];

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
[maxvertexcount(256)]
void main(uint3 groupId : SV_GroupID,
          uint  threadIdx : SV_GroupIndex,
          out vertices MeshVertex verts[256],
          out indices uint3 primIndices[256]) {
    uint chunkId = visibleIds[groupId.x];
    ChunkDescriptor desc = descriptors[chunkId];

    if (desc.chunkExtentAndNonAir.x <= 0) {
        SetMeshOutputCounts(0, 0);
        return;
    }

    float3 origin = desc.chunkOrigin.xyz;
    uint quadCount = min((uint)desc.chunkExtentAndNonAir.w, MAX_QUADS);

    uint vIdx = 0u;
    uint pIdx = 0u;
    for (uint q = 0u; q < quadCount; ++q) {
        if (vIdx + 3u > 256u || pIdx + 1u > 256u) break;

        float3 base = origin + float3(
            float(q & 0x3Fu),
            float((q >> 6u) & 0x3Fu),
            float((q >> 12u) & 0x3Fu)
        );
        uint material = decodeMaterial(desc.voxelDataInfo, q);

        verts[vIdx + 0u].outWorldPos = base;
        verts[vIdx + 0u].outNormal = float3(0.0, 1.0, 0.0);
        verts[vIdx + 0u].outMaterialIndex = material;
        verts[vIdx + 1u].outWorldPos = base + float3(1.0, 0.0, 0.0);
        verts[vIdx + 1u].outNormal = float3(0.0, 1.0, 0.0);
        verts[vIdx + 1u].outMaterialIndex = material;
        verts[vIdx + 2u].outWorldPos = base + float3(0.0, 0.0, 1.0);
        verts[vIdx + 2u].outNormal = float3(0.0, 1.0, 0.0);
        verts[vIdx + 2u].outMaterialIndex = material;

        primIndices[pIdx] = uint3(vIdx + 0u, vIdx + 1u, vIdx + 2u);

        vIdx += 3u;
        pIdx += 1u;
    }

    // For DXC mesh shader SPIR-V emission: vertex writes through `verts[]` are
    // translated to SPIR-V's vertex output array. Primitive indices are inferred
    // from [outputtopology] + sequential vertex writes. SetMeshOutputCounts is
    // required once to declare final vertex + primitive counts.
    // (DXC issue #6960 tracking historical SPIR-V mesh shader bugs; closed in
    // 1.8.2502. This compile-only benchmark verifies entry-point + descriptor
    // + push-constant plumbing; runtime correctness is verified by ProjectV's
    // GLSL pipeline.)

    SetMeshOutputCounts(vIdx, pIdx);
}