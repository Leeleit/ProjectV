// voxel_minimal.frag.hlsl — HLSL port of voxel_minimal.frag.
// Material atlas + sampler split into separate bindings (Texture2D binding 1,
// SamplerState binding 2; material table SSBO moved to binding 3).

struct LightingUBO {
    float4 sunDirection;
    float4 sunColor;
    float4 ambientColor;
    float4 cameraPosition;
    uint   materialCount;
    uint   pad0;
    uint   pad1;
    uint   pad2;
};

[[vk::binding(0, 1)]]
ConstantBuffer<LightingUBO> lighting;

[[vk::binding(1, 1)]]
Texture2D materialAtlas;

[[vk::binding(2, 1)]]
SamplerState materialSampler;

[[vk::binding(3, 1)]]
StructuredBuffer<uint> materialAlbedoOffsets;

struct PsInput {
    float3 inWorldPos      : TEXCOORD0;
    float3 inNormal        : TEXCOORD1;
    float2 inUv            : TEXCOORD2;
    uint   inMaterialIndex : TEXCOORD3;
    float4 fragCoord       : SV_POSITION;
};

struct PsOutput {
    float4 outColor : SV_TARGET0;
};

float interleavedGradientNoise(float2 pos) {
    return frac(52.9829189 * frac(dot(pos, float2(0.06711056, 0.00583715))));
}

PsOutput main(PsInput pin) {
    PsOutput pout;
    uint packed = materialAlbedoOffsets[pin.inMaterialIndex];
    float2 atlasOffset = float2(
        float(packed & 0xFFFFu),
        float((packed >> 16u) & 0xFFFFu)
    ) * (1.0 / 4096.0);
    float4 albedo = materialAtlas.Sample(materialSampler, pin.inUv + atlasOffset);

    float3 N = normalize(pin.inNormal);
    float3 L = normalize(-lighting.sunDirection.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 diffuse = albedo.rgb * lighting.sunColor.rgb * NdotL;
    float3 ambient = albedo.rgb * lighting.ambientColor.rgb;
    float3 color = diffuse + ambient;

    float dither = interleavedGradientNoise(pin.fragCoord.xy) - 0.5;
    pout.outColor = float4(color + dither * (1.0 / 255.0), albedo.a);
    return pout;
}