// audio_raytracer.cpp — implementation

#include "audio_raytracer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace audio_rt {

AudioRaytracer::AudioRaytracer(const VoxelGrid &grid) noexcept : grid_(grid) {}

float AudioRaytracer::reflectionCoeff(Material m) noexcept {
	switch (m) {
	case Material::Stone: return 0.70f;
	case Material::Wood:  return 0.40f;
	case Material::Glass: return 0.90f;
	case Material::Water: return 0.50f;
	case Material::Sand:  return 0.20f;
	case Material::Air:
	default:              return 0.0f;
	}
}

float AudioRaytracer::absorptionCoeff(Material m) noexcept {
	return 1.0f - reflectionCoeff(m);
}

float AudioRaytracer::randUnit() noexcept {
	rng_state_ += 0x9E3779B97F4A7C15ULL;
	uint64_t z = rng_state_;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	z = z ^ (z >> 31);
	return static_cast<float>(z >> 11) * (1.0f / static_cast<float>(1ULL << 53));
}

void AudioRaytracer::randUnitVec3(float &x, float &y, float &z) noexcept {
	// Marsaglia 1972 — uniform on unit sphere.
	float u1 = randUnit();
	float u2 = randUnit();
	float r = std::sqrt(std::max(0.0f, 1.0f - u1 * u1));
	float phi = 6.2831853f * u2;
	x = r * std::cos(phi);
	y = u1;
	z = r * std::sin(phi);
}

bool AudioRaytracer::traceNoGeometry(const AudioSource &src, const AudioListener &lst) const noexcept {
	(void)src;
	(void)lst;
	return true;
}

bool AudioRaytracer::traceOcclusionOnly(const AudioSource &src, const AudioListener &lst) const noexcept {
	float dx = lst.x - src.x;
	float dy = lst.y - src.y;
	float dz = lst.z - src.z;
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (dist < 1e-3f)
		return true;
	float nx = dx / dist, ny = dy / dist, nz = dz / dist;
	rays_traced_ += 1;
	auto hit = grid_.traceRay(src.x, src.y, src.z, nx, ny, nz, dist - 0.01f);
	voxels_traversed_ += static_cast<uint64_t>(dist); // rough proxy
	return !hit.has_value();
}

std::optional<ReflectionPath> AudioRaytracer::traceBounceRay(float ox, float oy, float oz, float dx, float dy, float dz,
															 int max_bounces) noexcept {
	ReflectionPath path{};
	float cur_x = ox, cur_y = oy, cur_z = oz;
	float cur_dx = dx, cur_dy = dy, cur_dz = dz;
	path.energy = 1.0f;
	for (int bounce = 0; bounce <= max_bounces; ++bounce) {
		auto hit = grid_.traceRay(cur_x, cur_y, cur_z, cur_dx, cur_dy, cur_dz);
		rays_traced_ += 1;
		if (!hit.has_value())
			return std::nullopt; // escaped
		path.total_distance += hit->t;
		float rc = reflectionCoeff(hit->mat);
		path.energy *= rc;
		// inverse-square attenuation (simplified — energy ∝ 1/r²)
		path.energy *= 1.0f / (1.0f + hit->t * hit->t * 0.01f);
		path.bounces = bounce;
		// reflect: d - 2(d·n)n
		float dot = cur_dx * hit->normal.x + cur_dy * hit->normal.y + cur_dz * hit->normal.z;
		cur_dx -= 2.0f * dot * hit->normal.x;
		cur_dy -= 2.0f * dot * hit->normal.y;
		cur_dz -= 2.0f * dot * hit->normal.z;
		// normalise
		float m = std::sqrt(cur_dx * cur_dx + cur_dy * cur_dy + cur_dz * cur_dz);
		if (m < 1e-6f)
			return std::nullopt;
		cur_dx /= m;
		cur_dy /= m;
		cur_dz /= m;
		// offset slightly off surface
		cur_x = static_cast<float>(hit->x) + 0.001f * (-hit->normal.x) + cur_dx * 0.01f;
		cur_y = static_cast<float>(hit->y) + 0.001f * (-hit->normal.y) + cur_dy * 0.01f;
		cur_z = static_cast<float>(hit->z) + 0.001f * (-hit->normal.z) + cur_dz * 0.01f;
		if (path.energy < 0.001f)
			return std::nullopt; // energy decayed
	}
	return path;
}

std::vector<ReflectionPath> AudioRaytracer::traceReflections(const AudioSource &src, const AudioListener &lst,
															 int max_reflections, int rays_per_source) noexcept {
	std::vector<ReflectionPath> paths;
	paths.reserve(static_cast<size_t>(rays_per_source) * (max_reflections + 1));
	// Direct path with occlusion (bounce=0)
	float dx = lst.x - src.x;
	float dy = lst.y - src.y;
	float dz = lst.z - src.z;
	float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
	if (dist > 1e-3f) {
		float nx = dx / dist, ny = dy / dist, nz = dz / dist;
		auto direct_hit = grid_.traceRay(src.x, src.y, src.z, nx, ny, nz, dist - 0.01f);
		rays_traced_ += 1;
		if (!direct_hit.has_value()) {
			ReflectionPath direct{};
			direct.total_distance = dist;
			direct.energy = src.power * (1.0f / (1.0f + dist * dist * 0.01f));
			direct.bounces = 0;
			paths.push_back(direct);
		}
	}
	// Specular reflections from random directions aimed at source
	// (Image-source lite: from listener, shoot ray that hits source after N bounces via Monte Carlo).
	for (int r = 0; r < rays_per_source; ++r) {
		// shoot ray from source in random direction; trace bounces; check if it ends near listener
		float rdx, rdy, rdz;
		randUnitVec3(rdx, rdy, rdz);
		auto p = traceBounceRay(src.x, src.y, src.z, rdx, rdy, rdz, max_reflections);
		if (!p.has_value())
			continue;
		// verify listener proximity
		float last_x = src.x, last_y = src.y, last_z = src.z;
		float cur_dx = rdx, cur_dy = rdy, cur_dz = rdz;
		for (int b = 0; b <= p->bounces; ++b) {
			auto hit = grid_.traceRay(last_x, last_y, last_z, cur_dx, cur_dy, cur_dz);
			if (!hit.has_value())
				break;
			last_x = static_cast<float>(hit->x) + 0.001f;
			last_y = static_cast<float>(hit->y) + 0.001f;
			last_z = static_cast<float>(hit->z) + 0.001f;
			float dot = cur_dx * hit->normal.x + cur_dy * hit->normal.y + cur_dz * hit->normal.z;
			cur_dx -= 2.0f * dot * hit->normal.x;
			cur_dy -= 2.0f * dot * hit->normal.y;
			cur_dz -= 2.0f * dot * hit->normal.z;
			float m = std::sqrt(cur_dx * cur_dx + cur_dy * cur_dy + cur_dz * cur_dz);
			if (m < 1e-6f)
				break;
			cur_dx /= m;
			cur_dy /= m;
			cur_dz /= m;
		}
		float ex = lst.x - last_x;
		float ey = lst.y - last_y;
		float ez = lst.z - last_z;
		float er = std::sqrt(ex * ex + ey * ey + ez * ez);
		if (er < 1.0f) {
			p->energy *= src.power;
			paths.push_back(*p);
		}
	}
	return paths;
}

std::vector<ReflectionPath> AudioRaytracer::traceReflectionsCached(const AudioSource &src, const AudioListener &lst,
																	int max_reflections, int rays_per_source) noexcept {
	float dx = src.x - last_src_x_;
	float dy = src.y - last_src_y_;
	float dz = src.z - last_src_z_;
	float lx = lst.x - last_lst_x_;
	float ly = lst.y - last_lst_y_;
	float lz = lst.z - last_lst_z_;
	float moved = std::sqrt(dx * dx + dy * dy + dz * dz) + std::sqrt(lx * lx + ly * ly + lz * lz);
	constexpr float kCacheEpsilon = 0.01f; // 1 cm
	if (cache_valid_ && moved < kCacheEpsilon && last_rays_ == rays_per_source && last_reflections_ == max_reflections) {
		cache_hits_ += 1;
		return cached_paths_;
	}
	auto paths = traceReflections(src, lst, max_reflections, rays_per_source);
	cached_paths_ = paths;
	last_src_x_ = src.x;
	last_src_y_ = src.y;
	last_src_z_ = src.z;
	last_lst_x_ = lst.x;
	last_lst_y_ = lst.y;
	last_lst_z_ = lst.z;
	last_rays_ = rays_per_source;
	last_reflections_ = max_reflections;
	cache_valid_ = true;
	return paths;
}

ImpulseResponse AudioRaytracer::generateIR(const std::vector<ReflectionPath> &paths, int source_count,
										   float max_ir_seconds) const noexcept {
	ImpulseResponse ir;
	ir.sample_rate = 44100;
	ir.source_count = source_count;
	ir.reflection_count = static_cast<int>(paths.size());
	int max_samples = static_cast<int>(max_ir_seconds * ir.sample_rate);
	ir.samples.assign(static_cast<size_t>(max_samples), 0.0f);
	const float kSpeedOfSound = 343.0f; // m/s at 20°C
	for (const auto &p : paths) {
		float time_s = p.total_distance / kSpeedOfSound;
		int sample_idx = static_cast<int>(time_s * ir.sample_rate);
		if (sample_idx < 0 || sample_idx >= max_samples)
			continue;
		// Spread energy across a few samples for the impulse (anti-aliased).
		int spread = 2;
		for (int s = -spread; s <= spread; ++s) {
			int idx = sample_idx + s;
			if (idx < 0 || idx >= max_samples)
				continue;
			float window = (s == 0) ? 1.0f : 0.5f;
			ir.samples[static_cast<size_t>(idx)] += p.energy * window;
		}
	}
	// Estimate RT60 from energy decay (simplified — Schroeder integration).
	ir.rt60_seconds = 0.0f;
	if (!paths.empty()) {
		// Approximate: find sample where energy decays to 1/1000 of peak.
		float peak = 0.0f;
		for (float s : ir.samples)
			peak = std::max(peak, std::abs(s));
		if (peak > 1e-6f) {
			float threshold = peak * 0.001f;
			for (int i = max_samples - 1; i >= 0; --i) {
				if (std::abs(ir.samples[static_cast<size_t>(i)]) > threshold) {
					ir.rt60_seconds = static_cast<float>(i) / ir.sample_rate;
					break;
				}
			}
		}
	}
	return ir;
}

} // namespace audio_rt
