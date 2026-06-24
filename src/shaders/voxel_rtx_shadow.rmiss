#version 460
#extension GL_EXT_ray_tracing : require // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

// Stage 5.2.E voxel-aware RTX shadow miss shader.
// Executed when the ray exits the world without hitting any occupied voxel.
// Writes payload.shadowFactor = 1.0 to indicate the surface point is fully lit.

struct ShadowPayload {
    float shadowFactor;
    float hitT;
};

layout(location = 0) rayPayloadInEXT ShadowPayload shadowPayload;

void main() {
    shadowPayload.hitT = -1.0;
    shadowPayload.shadowFactor = 1.0;
}