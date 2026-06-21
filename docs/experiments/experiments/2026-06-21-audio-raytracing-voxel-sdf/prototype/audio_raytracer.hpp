// audio_raytracer.hpp — geometric audio ray tracer through VoxelGrid
//
// Hybrid strategy:
//   - Direct path: occlusion test (1 ray per source-listener pair)
//   - Early reflections: specular bouncing (N rays per source, M reflection orders)
//   - Late reverberation: statistical, see reverb.{hpp,cpp}
//
// Standalone — no miniaudio / Vulkan / ProjectV mainline dependency.

#pragma once

#include "voxel_grid.hpp"
#include <cstdint>
#include <vector>

namespace audio_rt {

struct AudioSource {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float power = 1.0f; // 0..1
};

struct AudioListener {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct ReflectionPath {
	float total_distance = 0.0f;
	float energy = 0.0f; // product of reflection coeffs * 1/r² attenuation
	int bounces = 0;
};

struct ImpulseResponse {
	std::vector<float> samples;
	int sample_rate = 44100;
	float rt60_seconds = 0.0f;
	int source_count = 0;
	int reflection_count = 0;
};

class AudioRaytracer {
  public:
	explicit AudioRaytracer(const VoxelGrid &grid) noexcept;

	// === Configurations per README.md §3 ===

	// (A) baseline — no geometric processing, returns true (always direct).
	bool traceNoGeometry(const AudioSource &src, const AudioListener &lst) const noexcept;

	// (B) occlusion-only — single ray from source to listener. Returns true if line-of-sight.
	bool traceOcclusionOnly(const AudioSource &src, const AudioListener &lst) const noexcept;

	// (C) full hybrid — N rays per source, M reflection orders.
	std::vector<ReflectionPath> traceReflections(const AudioSource &src, const AudioListener &lst,
											   int max_reflections, int rays_per_source) noexcept;

	// (D) full hybrid + temporal cache — skip rays if (src,lst) haven't moved > epsilon.
	std::vector<ReflectionPath> traceReflectionsCached(const AudioSource &src, const AudioListener &lst,
													   int max_reflections, int rays_per_source) noexcept;

	// Generate impulse response buffer from reflection paths (time-domain).
	[[nodiscard]] ImpulseResponse generateIR(const std::vector<ReflectionPath> &paths, int source_count,
											 float max_ir_seconds = 1.5f) const noexcept;

	// Reset temporal cache (e.g. when scene geometry changes).
	void invalidateCache() noexcept { cache_valid_ = false; }
	void notifySceneChange() noexcept { cache_valid_ = false; }

	// Counters (for verification + accounting)
	[[nodiscard]] uint64_t raysTraced() const noexcept { return rays_traced_; }
	[[nodiscard]] uint64_t voxelsTraversed() const noexcept { return voxels_traversed_; }
	[[nodiscard]] uint64_t cacheHits() const noexcept { return cache_hits_; }

  private:
	const VoxelGrid &grid_;

	// Per-ray tracer with reflection bounces (recursive-style loop).
	// Returns path energy and distance, or std::nullopt if ray escaped / max_dist exceeded.
	std::optional<ReflectionPath> traceBounceRay(float ox, float oy, float oz, float dx, float dy, float dz,
												 int max_bounces) noexcept;

	// Counters
	mutable uint64_t rays_traced_ = 0;
	mutable uint64_t voxels_traversed_ = 0;
	mutable uint64_t cache_hits_ = 0;

	// Temporal cache
	bool cache_valid_ = false;
	float last_src_x_ = 0, last_src_y_ = 0, last_src_z_ = 0;
	float last_lst_x_ = 0, last_lst_y_ = 0, last_lst_z_ = 0;
	int last_rays_ = 0;
	int last_reflections_ = 0;
	std::vector<ReflectionPath> cached_paths_;

	// Cheap deterministic RNG (splitmix64) — uniform sphere sampling.
	mutable uint64_t rng_state_ = 0xDEADBEEFCAFEBABEULL;
	[[nodiscard]] float randUnit() noexcept;
	void randUnitVec3(float &x, float &y, float &z) noexcept;

	// Reflection coefficient lookup
	[[nodiscard]] static float reflectionCoeff(Material m) noexcept;
	[[nodiscard]] static float absorptionCoeff(Material m) noexcept;
};

} // namespace audio_rt
