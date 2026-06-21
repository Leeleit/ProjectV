// voxel_minimal.vert.hlsl — HLSL port of voxel_minimal.vert.
// Same descriptor layout (SSBO, push constants) but renumbered sampler bindings
// (HLSL has no GLSL-style combined image+sampler; Texture2D + SamplerState as
// separate bindings). Logic equivalent.

struct PushConstants {
    float4x4 viewProjection;
    float4x4 modelMatrix;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> pc;

struct ChunkInstance {
    float4 chunkOrigin;
    float4 chunkExtent;
    uint   materialIndex;
    uint   pad0;
    uint   pad1;
    uint   pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<ChunkInstance> chunkInstances;

struct VsInput {
    float3 inPosition    : TEXCOORD0;
    float3 inNormal      : TEXCOORD1;
    float2 inUv          : TEXCOORD2;
    uint   inInstanceId  : TEXCOORD3;
};

struct VsOutput {
    float3 outWorldPos      : TEXCOORD0;
    float3 outNormal        : TEXCOORD1;
    float2 outUv            : TEXCOORD2;
    uint   outMaterialIndex : TEXCOORD3;
    float4 svPosition       : SV_POSITION;
};

VsOutput main(VsInput vin) {
    VsOutput vout;
    ChunkInstance instance = chunkInstances[vin.inInstanceId];

    float4 worldPos = mul(pc.modelMatrix, float4(vin.inPosition, 1.0));
    vout.svPosition = mul(pc.viewProjection, worldPos);

    vout.outWorldPos = worldPos.xyz + instance.chunkOrigin.xyz;
    vout.outNormal = mul((float3x3)pc.modelMatrix, vin.inNormal);
    vout.outUv = vin.inUv;
    vout.outMaterialIndex = instance.materialIndex;
    return vout;
}