// voxel_minimal.vert — Representative ProjectV vertex shader (GLSL).
// Mimics structure of src/shaders/voxel.vert + voxel.frag entry: vertex stage
// transforms a 3-component position, passes through world/normal/material data
// to fragment shader. Uses push constants + SSBO read for per-draw chunk info.
//
// Target: Vulkan 1.4, glslc, --target-env=vulkan1.4

#version 460

layout(push_constant, std430) uniform PushConstants {
    mat4 viewProjection;
    mat4 modelMatrix;
} pc;

struct ChunkInstance {
    vec4 chunkOrigin;
    vec4 chunkExtent;
    uint materialIndex;
    uint pad0;
    uint pad1;
    uint pad2;
};

layout(set = 0, binding = 0, std430) readonly buffer ChunkInstances {
    ChunkInstance instances[];
} chunkInstances;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in uint inInstanceId;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUv;
layout(location = 3) flat out uint outMaterialIndex;

void main() {
    ChunkInstance instance = chunkInstances.instances[inInstanceId];

    vec4 worldPos = pc.modelMatrix * vec4(inPosition, 1.0);
    gl_Position = pc.viewProjection * worldPos;

    outWorldPos = worldPos.xyz + instance.chunkOrigin.xyz;
    outNormal = mat3(pc.modelMatrix) * inNormal;
    outUv = inUv;
    outMaterialIndex = instance.materialIndex;
}