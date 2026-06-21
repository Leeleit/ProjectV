// Shared GLSL constants for cross-shader sync.
//
// EVIL: depth distribution parameters for volumetric fog froxel sampling.
// MUST match between volumetric_fog.comp (dispatch / write side) and
// voxel.frag (consume side). Per Wronski 2014 froxel pattern + Frostbite 2015
// (Hillaire slide 28: per-slice integral = slice * (1 - transmittance) / density).
// If changed in one shader, MUST change in the other — otherwise fog sampling
// happens at wrong depths.
const float kFogDepthDistributionExp = 0.5;    // pow exponent for normalizedDepth
const float kFogDepthDistributionScale = 0.995; // 1.0 - bias, keeps within [0, 1]
const float kFogDepthDistributionBias = 0.005;  // near-plane offset, avoids 0-t at zero depth