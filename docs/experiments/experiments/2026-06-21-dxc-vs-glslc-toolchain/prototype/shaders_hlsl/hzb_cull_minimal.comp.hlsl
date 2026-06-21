// hzb_cull_minimal.comp.hlsl — HLSL port of hzb_cull_minimal.comp.
// Compute shader with structured buffer + atomic counter, push constants.

struct DrawData {
    float4x4 mvp;
    float4   boundingSphere;
    float4   boundingBoxMin;
    float4   boundingBoxMax;
    uint     meshId;
    uint     pad0;
    uint     pad1;
    uint     pad2;
};

[[vk::binding(0, 0)]]
StructuredBuffer<DrawData> drawBuffer;

// Split SSBO into two typed resources (mirrors GLSL runtime unsized array,
// but HLSL requires explicit type).
[[vk::binding(1, 0)]]
RWStructuredBuffer<uint> visibleFlagsBuffer;

[[vk::binding(2, 0)]]
RWStructuredBuffer<uint> visibleCounterBuffer; // Single-element, atomic increment.

struct CullParams {
    float4x4 viewProjection;
    float4   frustumPlanes[6];
    uint     drawCount;
    uint     enableOcclusionCulling;
    uint     enableFrustumCulling;
    uint     pad;
};

[[vk::push_constant]]
ConstantBuffer<CullParams> cullParams;

bool sphereInFrustum(float4 sphere, float4 planes[6]) {
    [unroll]
    for (int i = 0; i < 6; ++i) {
        if (dot(planes[i], sphere) < -sphere.w) return false;
    }
    return true;
}

[numthreads(64, 1, 1)]
void main(uint3 globalId : SV_DispatchThreadID) {
    uint drawId = globalId.x;
    if (drawId >= cullParams.drawCount) return;

    DrawData draw = drawBuffer[drawId];

    bool visible = true;
    if (cullParams.enableFrustumCulling == 1u) {
        visible = visible && sphereInFrustum(draw.boundingSphere, cullParams.frustumPlanes);
    }

    visibleFlagsBuffer[drawId] = visible ? 1u : 0u;
    if (visible) {
        uint prev;
        InterlockedAdd(visibleCounterBuffer[0], 1u, prev);
    }
}