#version 460

layout(set = 0, binding = 0, std430) readonly buffer PackedVertexPayload {
    uvec2 packedVertices[];
};

struct ChunkDescriptor {
    ivec4 chunkOrigin;
    uvec4 drawRanges;
};

layout(set = 0, binding = 1, std430) readonly buffer PackedChunkDescriptors {
    ChunkDescriptor chunkDescriptors[];
};

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
} pushConstants;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;
layout(location = 2) out float outDepth;
layout(location = 3) out float outMaterialKind;

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

vec4 GetMaterialBaseColor(const uint material) {
    switch (material) {
        case 1u: return vec4(0.94, 0.97, 1.00, 0.32);
        case 2u: return vec4(0.02, 0.62, 1.00, 1.00);
        case 3u: return vec4(1.00, 1.00, 1.00, 1.00);
        case 4u: return vec4(0.78, 0.80, 0.82, 1.00);
        default : return vec4(0.0, 0.0, 0.0, 0.0);
    }
}

float GetMaterialKind(const uint material) {
    if (material == 1u) {
        return 1.0;
    }
    if (material == 2u) {
        return 2.0;
    }
    return 0.0;
}

void main() {
    const uint chunkIndex = uint(gl_InstanceIndex);
    const ChunkDescriptor chunkDescriptor = chunkDescriptors[chunkIndex];
    const uvec2 packedVertex = packedVertices[uint(gl_VertexIndex)];

    const uint localPositionMaterial = packedVertex.x;
    const uint material = (localPositionMaterial >> 24u) & 0xFFu;
    const vec3 localPosition = vec3(
    float(localPositionMaterial & 0xFFu),
    float((localPositionMaterial >> 8u) & 0xFFu),
    float((localPositionMaterial >> 16u) & 0xFFu));
    const vec3 worldPosition = vec3(chunkDescriptor.chunkOrigin.xyz) + localPosition;

    gl_Position = pushConstants.viewProjection * vec4(worldPosition, 1.0);
    outNormal = DecodeFaceNormal(packedVertex.y & 0xFFu);
    outColor = GetMaterialBaseColor(material);
    outDepth = gl_Position.z / gl_Position.w;
    outMaterialKind = GetMaterialKind(material);
}
