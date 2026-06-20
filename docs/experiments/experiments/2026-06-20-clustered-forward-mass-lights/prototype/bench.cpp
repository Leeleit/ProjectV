// Standalone CPU prototype: cluster grid build + light culling for Forward+.
//
// Models the core algorithm of Forward+ (Harada 2012) on CPU: split view frustum
// into a 3D grid of clusters (linear XY + exponential Z), then for each cluster
// compute its AABB and assign all lights whose bounding sphere intersects it.
//
// This is the per-frame compute-shader work that the GPU prototype would do; we
// measure it on CPU (Zen 3 5800X, dev host `obvium`) so the numbers approximate
// the GPU compute pass cost for small-light counts. For high light counts the
// CPU becomes the bottleneck itself, but we report the algorithm shape and
// compare against published GPU numbers (sources.md S3) to validate the cost
// model.
//
// Build / run:
//   cd docs/experiments/experiments/2026-06-20-clustered-forward-mass-lights/prototype/
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -o bench bench.cpp
//   taskset -c 2 ./bench
// Output: results.csv (per-iter) + stdout summary table.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace {

struct Vec3 {
	float x, y, z;
};

struct Light {
	Vec3 position;
	float radius;
	Vec3 color;		 // unused in build; kept for completeness
	float intensity; // unused in build
};

struct Cluster {
	Vec3 aabbMin;
	Vec3 aabbMax;
	uint32_t lightOffset;
	uint32_t lightCount;
};

struct Stats {
	double mean = 0.0;
	double median = 0.0;
	double p95 = 0.0;
	double p99 = 0.0;
	double stddev = 0.0;
	double minv = 0.0;
	double maxv = 0.0;
};

struct Camera {
	Vec3 position{0.0f, 0.0f, 0.0f};
	Vec3 forward{0.0f, 0.0f, -1.0f}; // looking -Z by convention
	Vec3 up{0.0f, 1.0f, 0.0f};
	float fovYRad = 60.0f * 3.14159265f / 180.0f;
	float aspect = 16.0f / 9.0f;
	float nearZ = 0.1f;
	float farZ = 200.0f;
};

struct GridDims {
	uint32_t nx, ny, nz;
};

// Compute the 8 frustum corners at a given Z (in view space, -Z = forward).
// Returns corners in WORLD space (camera basis).
void ComputeFrustumCornersAtViewZ(const Camera &cam, float viewZ, Vec3 out[8])
{
	const float halfHeight = std::abs(viewZ) * std::tan(cam.fovYRad * 0.5f);
	const float halfWidth = halfHeight * cam.aspect;
	// 8 corners at given Z (Z=viewZ): 4 at near, 4 at far
	// For 2D split, we use 4 corners per slice.
	// Here we compute the 4 corner rays of the frustum at the given viewZ.
	// Convention: viewZ is negative (forward direction).
	// The corners of the frustum cross-section at viewZ are:
	// (-halfWidth, -halfHeight, viewZ), (+halfWidth, -halfHeight, viewZ), ...
	// We use the camera basis to transform to world space.
	Vec3 fwd = cam.forward;
	Vec3 up = cam.up;
	Vec3 right{up.z * fwd.y - up.y * fwd.z,
			   up.x * fwd.z - up.z * fwd.x,
			   up.y * fwd.x - up.x * fwd.y};
	const float rlen = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
	right = {right.x / rlen, right.y / rlen, right.z / rlen};
	up = {fwd.z * right.y - fwd.y * right.z,
		  fwd.x * right.z - fwd.z * right.x,
		  fwd.y * right.x - fwd.x * right.z};
	// 4 corners at viewZ
	const float hw = halfWidth, hh = halfHeight;
	const float cz = viewZ;
	Vec3 local[4] = {
		{-hw, -hh, cz}, {+hw, -hh, cz}, {+hw, +hh, cz}, {-hw, +hh, cz}};
	for (int i = 0; i < 4; ++i) {
		const Vec3 &p = local[i];
		out[i] = {
			cam.position.x + right.x * p.x + up.x * p.y - fwd.x * p.z,
			cam.position.y + right.y * p.x + up.y * p.y - fwd.y * p.z,
			cam.position.z + right.z * p.x + up.z * p.y - fwd.z * p.z};
	}
}

// Build cluster AABBs. XY is linear (uniform screen-space tile size),
// Z is exponential (Naughty Dog 2016 / id Tech 6 logarithmic depth).
std::vector<Cluster> BuildClusters(const Camera &cam, const GridDims &g)
{
	std::vector<Cluster> clusters(g.nx * g.ny * g.nz);
	// Compute viewZ per slice (exponential)
	// z_k = near * (far/near)^(k/nz) for k in [0, nz]
	// We store slices in [z_k, z_{k+1}] (k = 0..nz-1).
	// Convert to view space (Z negative).
	std::vector<float> zView(g.nz + 1);
	const float ratio = cam.farZ / cam.nearZ;
	for (uint32_t k = 0; k <= g.nz; ++k) {
		const float t = static_cast<float>(k) / static_cast<float>(g.nz);
		const float dist = cam.nearZ * std::pow(ratio, t);
		zView[k] = -dist; // view space: forward is -Z
	}
	// For each (slice, tile) compute 4 corner rays and take AABB.
	for (uint32_t kz = 0; kz < g.nz; ++kz) {
		// corners at z = zView[kz] and z = zView[kz+1]
		Vec3 near4[4], far4[4];
		ComputeFrustumCornersAtViewZ(cam, zView[kz], near4);
		ComputeFrustumCornersAtViewZ(cam, zView[kz + 1], far4);
		for (uint32_t ky = 0; ky < g.ny; ++ky) {
			for (uint32_t kx = 0; kx < g.nx; ++kx) {
				// 8-corner AABB of the cluster sub-frustum
				Vec3 mn{+1e30f, +1e30f, +1e30f};
				Vec3 mx{-1e30f, -1e30f, -1e30f};
				// 4 near + 4 far, then subdivide by tile X/Y linearly between
				// the 4 near and 4 far corners.
				const float u0 = static_cast<float>(kx) / static_cast<float>(g.nx);
				const float u1 = static_cast<float>(kx + 1) / static_cast<float>(g.nx);
				const float v0 = static_cast<float>(ky) / static_cast<float>(g.ny);
				const float v1 = static_cast<float>(ky + 1) / static_cast<float>(g.ny);
				for (int ci = 0; ci < 4; ++ci) {
					const float u = (ci == 0 || ci == 3) ? u0 : u1;
					const float v = (ci == 0 || ci == 1) ? v0 : v1;
					// Bilinear interp: corner at (u,v) is
					//   lerp(lerp(n00, n10, u), lerp(n01, n11, u), v)
					//   where n00=near4[0], n10=near4[1], n01=near4[3], n11=near4[2]
					// (assuming corner order: BL,BR,TR,TL)
					Vec3 c0{near4[0].x + (near4[1].x - near4[0].x) * u,
							near4[0].y + (near4[1].y - near4[0].y) * u,
							near4[0].z + (near4[1].z - near4[0].z) * u};
					Vec3 c1{near4[3].x + (near4[2].x - near4[3].x) * u,
							near4[3].y + (near4[2].y - near4[3].y) * u,
							near4[3].z + (near4[2].z - near4[3].z) * u};
					Vec3 cn{c0.x + (c1.x - c0.x) * v, c0.y + (c1.y - c0.y) * v, c0.z + (c1.z - c0.z) * v};
					Vec3 c0f{far4[0].x + (far4[1].x - far4[0].x) * u,
							 far4[0].y + (far4[1].y - far4[0].y) * u,
							 far4[0].z + (far4[1].z - far4[0].z) * u};
					Vec3 c1f{far4[3].x + (far4[2].x - far4[3].x) * u,
							 far4[3].y + (far4[2].y - far4[3].y) * u,
							 far4[3].z + (far4[2].z - far4[3].z) * u};
					Vec3 cf{c0f.x + (c1f.x - c0f.x) * v, c0f.y + (c1f.y - c0f.y) * v, c0f.z + (c1f.z - c0f.z) * v};
					for (const Vec3 &p : {cn, cf}) {
						mn = {std::min(mn.x, p.x), std::min(mn.y, p.y), std::min(mn.z, p.z)};
						mx = {std::max(mx.x, p.x), std::max(mx.y, p.y), std::max(mx.z, p.z)};
					}
				}
				const uint32_t idx = (kz * g.ny + ky) * g.nx + kx;
				clusters[idx] = {mn, mx, 0u, 0u};
			}
		}
	}
	return clusters;
}

// Sphere-AABB intersection (point sphere center at `c` with radius `r` vs
// AABB [mn, mx]). Returns true if they overlap.
inline bool SphereAabbOverlap(const Vec3 &c, float r, const Vec3 &mn, const Vec3 &mx)
{
	float d2 = 0.0f;
	if (c.x < mn.x)
		d2 += (mn.x - c.x) * (mn.x - c.x);
	else if (c.x > mx.x)
		d2 += (c.x - mx.x) * (c.x - mx.x);
	if (c.y < mn.y)
		d2 += (mn.y - c.y) * (mn.y - c.y);
	else if (c.y > mx.y)
		d2 += (c.y - mx.y) * (c.y - mx.y);
	if (c.z < mn.z)
		d2 += (mn.z - c.z) * (mn.z - c.z);
	else if (c.z > mx.z)
		d2 += (c.z - mx.z) * (c.z - mx.z);
	return d2 <= r * r;
}

// Assign lights to clusters. Two-pass: count per cluster, then write index array
// (so we know exact offsets without over-allocation).
struct AssignResult {
	std::vector<uint32_t> lightIndices; // flat, [cluster.lightOffset..+lightCount)
	std::vector<uint32_t> lightCountPerCluster;
	uint64_t totalTests = 0;
	double seconds = 0.0;
};

AssignResult AssignLightsToClusters(std::span<const Light> lights,
									std::vector<Cluster> &clusters)
{
	AssignResult res;
	res.lightCountPerCluster.assign(clusters.size(), 0u);
	// Pass 1: count
	auto t0 = std::chrono::high_resolution_clock::now();
	for (const Light &L : lights) {
		for (size_t ci = 0; ci < clusters.size(); ++ci) {
			++res.totalTests;
			if (SphereAabbOverlap(L.position, L.radius,
								  clusters[ci].aabbMin, clusters[ci].aabbMax)) {
				++res.lightCountPerCluster[ci];
			}
		}
	}
	// Compute offsets
	uint32_t total = 0;
	for (size_t ci = 0; ci < clusters.size(); ++ci) {
		clusters[ci].lightOffset = total;
		clusters[ci].lightCount = res.lightCountPerCluster[ci];
		total += res.lightCountPerCluster[ci];
	}
	res.lightIndices.assign(total, 0u);
	// Pass 2: write indices
	std::vector<uint32_t> cursor(clusters.size(), 0u);
	for (uint32_t li = 0; li < lights.size(); ++li) {
		const Light &L = lights[li];
		for (size_t ci = 0; ci < clusters.size(); ++ci) {
			if (SphereAabbOverlap(L.position, L.radius,
								  clusters[ci].aabbMin, clusters[ci].aabbMax)) {
				const uint32_t off = clusters[ci].lightOffset + cursor[ci]++;
				res.lightIndices[off] = li;
			}
		}
	}
	auto t1 = std::chrono::high_resolution_clock::now();
	res.seconds = std::chrono::duration<double>(t1 - t0).count();
	return res;
}

Stats ComputeStats(std::span<const double> samples)
{
	Stats s{};
	if (samples.empty())
		return s;
	std::vector<double> sorted(samples.begin(), samples.end());
	std::sort(sorted.begin(), sorted.end());
	double sum = 0.0;
	for (double v : samples)
		sum += v;
	s.mean = sum / static_cast<double>(samples.size());
	s.median = sorted[sorted.size() / 2];
	s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
	s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
	s.minv = sorted.front();
	s.maxv = sorted.back();
	return s;
}

std::vector<Light> GenerateLightsSparse(uint32_t count, uint32_t seed)
{
	// Sparse: lights in [-50, 50]^3 (representing ambient/VPL distribution).
	// Radius 1-10 m (small VPLs / torches / neon blocks).
	std::mt19937 rng(seed);
	std::uniform_real_distribution<float> pos(-50.0f, 50.0f);
	std::uniform_real_distribution<float> rad(1.0f, 10.0f);
	std::vector<Light> lights(count);
	for (uint32_t i = 0; i < count; ++i) {
		lights[i].position = {pos(rng), pos(rng), pos(rng) * 0.5f - 30.0f};
		lights[i].radius = rad(rng);
		lights[i].color = {1.0f, 1.0f, 1.0f};
		lights[i].intensity = 1.0f;
	}
	return lights;
}

std::vector<Light> GenerateLightsDense(uint32_t count, uint32_t seed)
{
	// Dense: lights concentrated in 20m sphere around camera (representing
	// lava cluster / magical effects / torches cluster). Radius 4-15 m
	// (lava = 10-15 m influence, torches = 4-8 m).
	std::mt19937 rng(seed);
	std::uniform_real_distribution<float> radPos(0.0f, 20.0f);
	std::uniform_real_distribution<float> theta(0.0f, 6.2831853f);
	std::uniform_real_distribution<float> phi(0.0f, 3.14159265f);
	std::uniform_real_distribution<float> radLight(4.0f, 15.0f);
	std::vector<Light> lights(count);
	for (uint32_t i = 0; i < count; ++i) {
		// Uniform sphere via rejection (simplified: theta/phi, biased to -Z)
		const float r = radPos(rng);
		const float t = theta(rng);
		const float p = phi(rng);
		const float sinp = std::sin(p);
		lights[i].position = {r * sinp * std::cos(t),
							  r * std::cos(p),
							  -r * sinp * std::sin(t) - 10.0f}; // -Z bias
		lights[i].radius = radLight(rng);
		lights[i].color = {1.0f, 1.0f, 1.0f};
		lights[i].intensity = 1.0f;
	}
	return lights;
}

struct Config {
	std::string scenario;
	std::string name;
	GridDims grid;
	uint32_t lightCount;
	uint32_t seed;
};

} // namespace

int main()
{
	const std::vector<Config> configs = {
		// Sparse scenario (lights spread in [-50,50]^3, VPL-like distribution)
		{"sparse", "8x4x12", {8, 4, 12}, 100, 1},
		{"sparse", "8x4x12", {8, 4, 12}, 1000, 3},
		{"sparse", "16x9x24", {16, 9, 24}, 100, 5},
		{"sparse", "16x9x24", {16, 9, 24}, 1000, 7},
		{"sparse", "32x18x64", {32, 18, 64}, 100, 9},
		{"sparse", "32x18x64", {32, 18, 64}, 1000, 11},
		// Dense scenario (lights in 20m sphere around camera, lava/torches)
		{"dense", "8x4x12", {8, 4, 12}, 100, 101},
		{"dense", "8x4x12", {8, 4, 12}, 1000, 103},
		{"dense", "16x9x24", {16, 9, 24}, 100, 105},
		{"dense", "16x9x24", {16, 9, 24}, 1000, 107},
		{"dense", "16x9x24", {16, 9, 24}, 5000, 108},
		{"dense", "32x18x64", {32, 18, 64}, 100, 109},
		{"dense", "32x18x64", {32, 18, 64}, 1000, 111},
	};

	const Camera cam; // defaults
	const uint32_t kWarmup = 10;

	std::ofstream csv("results.csv");
	csv << "scenario,config,lights,iter,build_ms,totalTests,avgLights,medianLights,maxLights,pctEmpty,pctOverflow\n";

	std::printf("ProjectV-ClusteredForward Cluster Build CPU Benchmark\n");
	std::printf("CPU: Zen 3 5800X, governor=powersave, single-core (taskset -c 2)\n");
	std::printf("Camera: pos=(0,0,0), forward=(0,0,-1), FOV=60deg, near=0.1, far=200\n");
	std::printf("Cluster build: adaptive iters per config (10 warmup, target ~5s)\n");
	std::printf("Two scenarios: sparse (lights in [-50,50]^3, VPL-like) + dense (20m sphere, lava/torches)\n\n");
	std::printf("%-8s %-10s %6s %10s %10s %10s %10s %10s %10s %10s\n",
				"scenario", "config", "lights", "mean_ms", "median_ms", "p95_ms", "p99_ms", "std_ms", "min_ms", "max_ms");
	std::printf("%-8s %-10s %6s %10s %10s %10s %10s %10s %10s %10s\n",
				"--------", "------", "------", "-------", "---------", "------", "------", "------", "------", "------");

	for (const Config &cfg : configs) {
		const auto lights = (cfg.scenario == "dense")
								? GenerateLightsDense(cfg.lightCount, cfg.seed)
								: GenerateLightsSparse(cfg.lightCount, cfg.seed);
		std::vector<double> samples;
		// Per-iter occupancy stats: aggregate across all iters at the end
		// (assignments are deterministic for fixed seed).
		// We only need a single occupancy measurement (iter=0 below).
		double avgL = 0.0, medL = 0.0, maxL = 0.0, pctEmpty = 0.0, pctOver = 0.0;

		// Pre-build clusters ONCE per config (they are static per camera).
		// In a real renderer this would be ~O(N_clusters) work per frame,
		// which is cheap (~0.1 ms even for 32x18x64 = 36K clusters).
		// We measure only AssignLights (the per-frame hot path).
		auto baseClusters = BuildClusters(cam, cfg.grid);

		// Adaptive iteration count: target ~5 sec measurement time per config,
		// min 5 iters, max 1000. Pilot iter below estimates per-iter cost.
		AssignResult pilot = AssignLightsToClusters(lights, baseClusters);
		const double pilotMs = pilot.seconds * 1000.0;
		uint32_t targetIters = 0;
		if (pilotMs < 0.001)
			targetIters = 1000; // sub-µs → max iterations
		else if (pilotMs > 1000.0)
			targetIters = 3; // >1 sec/iter → 3 iters only
		else
			targetIters = static_cast<uint32_t>(std::clamp(5000.0 / pilotMs, 5.0, 1000.0));
		samples.reserve(targetIters);

		for (uint32_t it = 0; it < targetIters + kWarmup; ++it) {
			// Copy cluster AABBs (the offset/count get reset each iter).
			std::vector<Cluster> clusters = baseClusters;
			AssignResult res = AssignLightsToClusters(lights, clusters);
			if (it == kWarmup) {
				// Occupancy stats on first post-warmup iter
				std::vector<uint32_t> counts;
				counts.reserve(clusters.size());
				const uint32_t softCap = 1024;
				for (const auto &c : clusters)
					counts.push_back(c.lightCount);
				std::vector<uint32_t> sorted = counts;
				std::sort(sorted.begin(), sorted.end());
				double sum = 0.0;
				uint32_t empty = 0, over = 0;
				for (uint32_t v : counts) {
					sum += v;
					if (v == 0)
						++empty;
					if (v > softCap)
						++over;
				}
				avgL = sum / static_cast<double>(counts.size());
				medL = static_cast<double>(sorted[sorted.size() / 2]);
				maxL = static_cast<double>(sorted.back());
				pctEmpty = 100.0 * empty / static_cast<double>(counts.size());
				pctOver = 100.0 * over / static_cast<double>(counts.size());
			}
			if (it >= kWarmup) {
				samples.push_back(res.seconds * 1000.0); // ms
				csv << cfg.name << "," << cfg.lightCount << "," << (it - kWarmup)
					<< "," << (res.seconds * 1000.0) << "," << res.totalTests
					<< "," << avgL << "," << medL << "," << maxL
					<< "," << pctEmpty << "," << pctOver << "\n";
			}
		}

		const Stats s = ComputeStats(samples);
		std::printf("%-8s %-10s %6u %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f %10.3f  (occ avg=%.1f med=%.0f max=%.0f empty=%.1f%% over=%.2f%%, n=%u)\n",
					cfg.scenario.c_str(), cfg.name.c_str(), cfg.lightCount,
					s.mean, s.median, s.p95, s.p99, s.stddev, s.minv, s.maxv,
					avgL, medL, maxL, pctEmpty, pctOver,
					static_cast<uint32_t>(samples.size()));
	}

	csv.close();
	std::printf("\nresults.csv: per-iter rows (warmup excluded).\n");
	return 0;
}
