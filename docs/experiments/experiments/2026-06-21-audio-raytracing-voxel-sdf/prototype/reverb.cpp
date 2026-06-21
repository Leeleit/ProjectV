// reverb.cpp — late reverberation tail

#include "reverb.hpp"

#include <cmath>

namespace audio_rt {

RoomEstimate estimateRoom(const VoxelGrid &grid) noexcept {
	RoomEstimate r;
	// World volume (cubic meters — assuming voxel size = 1m)
	int wx = grid.worldSizeX();
	int wy = grid.worldSizeY();
	int wz = grid.worldSizeZ();
	r.volume_m3 = static_cast<float>(wx) * wy * wz;
	// Surface area = wall voxels * 6 (6 faces per solid voxel contributing to surface)
	// Approximation: solid voxels * 1 m² per exposed face. Simpler: count solid voxels and estimate.
	int solid_count = 0;
	int wall_count = 0; // solid voxels adjacent to air
	for (int x = 0; x < wx; ++x) {
		for (int y = 0; y < wy; ++y) {
			for (int z = 0; z < wz; ++z) {
				auto m = grid.getVoxel(x, y, z);
				if (m && *m != Material::Air) {
					solid_count += 1;
					bool adj_air = !grid.getVoxel(x + 1, y, z).has_value() ||
								   (grid.getVoxel(x + 1, y, z) && *grid.getVoxel(x + 1, y, z) == Material::Air) ||
								   !grid.getVoxel(x - 1, y, z).has_value() ||
								   (grid.getVoxel(x - 1, y, z) && *grid.getVoxel(x - 1, y, z) == Material::Air) ||
								   !grid.getVoxel(x, y + 1, z).has_value() ||
								   (grid.getVoxel(x, y + 1, z) && *grid.getVoxel(x, y + 1, z) == Material::Air) ||
								   !grid.getVoxel(x, y - 1, z).has_value() ||
								   (grid.getVoxel(x, y - 1, z) && *grid.getVoxel(x, y - 1, z) == Material::Air) ||
								   !grid.getVoxel(x, y, z + 1).has_value() ||
								   (grid.getVoxel(x, y, z + 1) && *grid.getVoxel(x, y, z + 1) == Material::Air) ||
								   !grid.getVoxel(x, y, z - 1).has_value() ||
								   (grid.getVoxel(x, y, z - 1) && *grid.getVoxel(x, y, z - 1) == Material::Air);
					if (adj_air)
						wall_count += 1;
				}
			}
		}
	}
	float surface_area = static_cast<float>(wall_count);
	// Average absorption coefficient across walls — assume mixed materials avg 0.30.
	// Stone 0.30, Wood 0.60, Glass 0.10, Sand 0.80, Water 0.20.
	const float kAvgAbsorption = 0.30f;
	r.total_absorption = surface_area * kAvgAbsorption;
	if (r.total_absorption > 1e-3f) {
		// Sabine: RT60 = 0.161 * V / A
		r.rt60_sabine = 0.161f * r.volume_m3 / r.total_absorption;
		// Eyring: RT60 = 0.161 * V / (-S * ln(1 - alpha))
		float alpha = kAvgAbsorption;
		float denom = -surface_area * std::log(std::max(0.01f, 1.0f - alpha));
		r.rt60_eyring = (denom > 1e-3f) ? 0.161f * r.volume_m3 / denom : r.rt60_sabine;
	}
	(void)solid_count;
	return r;
}

void applyEyringTail(ImpulseResponse &ir, const RoomEstimate &room, float crossover_seconds) noexcept {
	if (ir.samples.empty())
		return;
	int crossover = static_cast<int>(crossover_seconds * ir.sample_rate);
	if (crossover >= static_cast<int>(ir.samples.size()))
		return;
	float rt60 = std::max(0.1f, room.rt60_eyring);
	// Exponential decay: amplitude(t) = amplitude_0 * exp(-6.91 * t / rt60)
	// (-6.91 ≈ ln(0.001), so amplitude drops to 0.1% at RT60)
	const float kDecayCoeff = 6.9078f; // ln(1000)
	// Find peak amplitude at crossover.
	float peak = 0.0f;
	for (size_t i = static_cast<size_t>(crossover); i < ir.samples.size(); ++i)
		peak = std::max(peak, std::abs(ir.samples[i]));
	if (peak < 1e-6f)
		return;
	for (size_t i = static_cast<size_t>(crossover); i < ir.samples.size(); ++i) {
		float t = static_cast<float>(i - crossover) / ir.sample_rate;
		float env = std::exp(-kDecayCoeff * t / rt60);
		ir.samples[i] += peak * env * 0.3f; // additive late tail
	}
}

} // namespace audio_rt
