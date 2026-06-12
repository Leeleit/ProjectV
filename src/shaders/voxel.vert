#version 460

struct PackedFace {
    uint localVoxelFace;
    uint chunkIndexMaterial;
    uint lightingData;
    // A1 (4.1 greedy meshing): (width, height) in 8 bits each for the
    // in-plane quad size. width = axisU extent, height = axisV extent.
    // (1, 1) = unit quad (no merge). See `decisions.md §25`.
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

// Per-vertex ambient occlusion has been **disabled** as a deliberate follow-up
// to P0.3. The earlier face-corner AO (Lysenko 3-neighbor), the 8-surrounding
// formula, and the 4-axis-aligned variant all produced a "pseudo-shadow" at
// 3D-угол 2x2x2 cube (or any 4-voxel junction) because the count of solid
// axis-aligned neighbors peaks at convex corners with three abutting voxels
// (3 of 4 = AO 64 = 25% lit), even though sky is visible from the outward
// diagonal direction. A face-independent model cannot distinguish "concave"
// from "convex" from a single neighbor count, so any per-corner AO will
// always have a discrete darkening at the cube-corner junctions of a 2x2x2
// mass. AO is now supplied entirely by the fragment shader's
// `ComputeAmbientOcclusionVisibility` ray-cast AOCC, which evaluates sky
// visibility per-pixel from a stable face-plane origin and produces smooth
// cavity darkening without face-boundary seams.
layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPosition;
layout(location = 2) flat out uint outMaterialIndex;
layout(location = 3) out float outAmbientVisibility;

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

// A1 (4.1 greedy meshing): scale the unit corner offset by the merged
// quad extents in the face's in-plane channels. `unitOffset.x/y/z` are 0
// or 1 (per existing `GetFaceCornerOffset`); the in-plane channels get
// multiplied by `quadExtents.x` (width = axisU) and `quadExtents.y`
// (height = axisV) respectively, while the normal-axis channel stays at
// its 0/1 value. For unit quads (W=H=1) this is a no-op.
uvec3 ApplyGreedyScale(const uint faceIndex, const uvec3 unitOffset, const uvec2 quadExtents) {
    if (faceIndex == 0u || faceIndex == 1u) {
        // Face 0/1 (X±): in-plane = (Y, Z). axisU = Y, axisV = Z.
        return uvec3(unitOffset.x, unitOffset.y * quadExtents.x, unitOffset.z * quadExtents.y);
    } else if (faceIndex == 2u || faceIndex == 3u) {
        // Face 2/3 (Y±): in-plane = (X, Z). axisU = X, axisV = Z.
        return uvec3(unitOffset.x * quadExtents.x, unitOffset.y, unitOffset.z * quadExtents.y);
    } else {
        // Face 4/5 (Z±): in-plane = (X, Y). axisU = X, axisV = Y.
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

    // A1 (4.1 greedy meshing): decode (width, height) of the merged quad.
    // Unit quads carry (1, 1) and behave exactly like the pre-A1 path.
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
    // Per-vertex AO is now a no-op: the AO term is supplied entirely by the
    // fragment shader's `ComputeAmbientOcclusionVisibility` (per-pixel ray-cast
    // AOCC, see voxel.frag), which has no face-boundary discontinuities and
    // produces smooth cavity darkening for genuine concavities (e.g. a 1x1
    // hole) while leaving flat voxel faces and convex cube corners at full
    // brightness. The `inAmbientVisibility` interpolator is kept non-`flat` so
    // a future per-vertex AO term can be re-introduced without re-plumbing
    // the input layout.
    outAmbientVisibility = 1.0;
}
