// fluid_ca_minimal.comp.hlsl — HLSL port of fluid_ca_minimal.comp.
// Compute with shared memory, ping-pong SSBO, neighborhood CA update.

[[vk::binding(0, 0)]]
StructuredBuffer<float> fluidIn;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> fluidOut;

struct FluidParams {
    int4  gridDims;
    uint  timestep;
    float flowRate;
    float decay;
    uint  enableGravity;
};

[[vk::push_constant]]
ConstantBuffer<FluidParams> fluidParams;

groupshared float localCache[64];

uint coordToCellIndex(int3 coord, int3 dims) {
    return uint(coord.x + dims.x * (coord.y + dims.y * coord.z));
}

[numthreads(8, 8, 1)]
void main(uint3 globalId : SV_DispatchThreadID,
          uint  localIndex : SV_GroupIndex) {
    int3 dims = fluidParams.gridDims.xyz;
    int3 coord = int3(globalId.xyz);
    if (any(coord >= dims)) return;

    uint flatIndex = coordToCellIndex(coord, dims);

    localCache[localIndex] = fluidIn[flatIndex];
    GroupMemoryBarrierWithGroupSync();

    float sum = 0.0;
    uint count = 0u;
    [unroll]
    for (int dz = -1; dz <= 1; ++dz) {
        [unroll]
        for (int dy = -1; dy <= 1; ++dy) {
            [unroll]
            for (int dx = -1; dx <= 1; ++dx) {
                int3 ncoord = coord + int3(dx, dy, dz);
                if (any(ncoord < int3(0, 0, 0)) || any(ncoord >= dims)) continue;
                uint nflat = coordToCellIndex(ncoord, dims);
                sum += fluidIn[nflat];
                count += 1u;
            }
        }
    }

    float avg = (count > 0u) ? (sum / float(count)) : 0.0;
    float value = fluidIn[flatIndex];
    float updated = lerp(value, avg, fluidParams.flowRate) * fluidParams.decay;

    if (fluidParams.enableGravity == 1u) {
        float heightBias = float(coord.y) / float(max(dims.y - 1, 1));
        updated *= (1.0 - 0.05 * heightBias);
    }

    fluidOut[flatIndex] = updated;
}