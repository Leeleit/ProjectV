// reverb.hpp — late reverberation statistical model
//
// Eyring formula (sabine variant) — given room volume and total absorption,
// estimate RT60 reverberation time. Applied to IR buffer as exponential decay tail.

#pragma once

#include "audio_raytracer.hpp"

namespace audio_rt {

struct RoomEstimate {
	float volume_m3 = 0.0f;     // V
	float total_absorption = 0.0f; // A (sabins)
	float rt60_eyring = 0.0f;      // seconds
	float rt60_sabine = 0.0f;      // seconds
};

// Estimate room volume + absorption from voxel grid (air = volume, solid = wall surface).
[[nodiscard]] RoomEstimate estimateRoom(const VoxelGrid &grid) noexcept;

// Apply Eyring late-reverb tail to IR samples (decays past early reflections).
void applyEyringTail(ImpulseResponse &ir, const RoomEstimate &room, float crossover_seconds = 0.08f) noexcept;

} // namespace audio_rt
