#version 460
#extension GL_EXT_ray_tracing : require

// Stage 5.2.E voxel-aware RTX shadow closest-hit shader.
// Executed when RT traversal confirms the ray hit an occupied voxel inside a
// chunk BLAS (the procedural intersection shader reported a real voxel hit).
// Writes payload.shadowFactor = 0.0 to indicate the surface point is in shadow.

struct ShadowPayload {
    float shadowFactor;
    float hitT;
};

layout(location = 0) rayPayloadInEXT ShadowPayload shadowPayload;

void main() {
    shadowPayload.hitT = gl_HitTEXT;
    shadowPayload.shadowFactor = 0.0;
}