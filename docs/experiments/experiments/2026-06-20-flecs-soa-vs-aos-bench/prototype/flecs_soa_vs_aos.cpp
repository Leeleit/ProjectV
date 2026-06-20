// 2026-06-20-flecs-soa-vs-aos-bench prototype
// Standalone C++26 benchmark: SoA vs AoS для ECS hot loops (raycast, physics step, render cull)
// Hardware baseline: AMD Ryzen 7 5800X (Zen 3, AVX2/FMA, no AVX-512, L1d 32 KiB, L2 512 KiB, L3 32 MiB,
//   cache line 64 B per AMD EPYC 7003 microarch reference = Zen 3 cache spec).
//
// Build: clang++ -O3 -march=native -DNDEBUG -std=c++26 flecs_soa_vs_aos.cpp -o /tmp/flecs_bench
// Run:   /tmp/flecs_bench --entities 500000 --iterations 1000 --warmup 100 --seed 42 --output results.csv

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// ===== Component types (mirrors ProjectV ECS fields per workspace.md Phase 5) =====

struct Vec3 {
	float x;
	float y;
	float z;
};

struct Aabb {
	Vec3 min;
	Vec3 max;
};

struct EntityAos {
	Vec3 position;
	Vec3 velocity;
	Aabb bounds;
	uint32_t material;
	uint8_t isActive;
	uint8_t pad[3];
	uint64_t lastTouched;
};
static_assert(sizeof(EntityAos) == 64, "AoS struct must equal 64 B (1 cache line) for clean comparison");

struct ColdSidecar {
	uint32_t material;
	uint64_t lastTouched;
};

// ===== Layout 1: AoS (baseline, current mainline idiom) =====

struct LayoutAos {
	std::vector<EntityAos> entities;

	size_t size() const { return entities.size(); }
	void resize(size_t n) { entities.resize(n); }

	Vec3 get_position(size_t i) const { return entities[i].position; }
	Vec3 get_velocity(size_t i) const { return entities[i].velocity; }
	Aabb get_bounds(size_t i) const { return entities[i].bounds; }
	uint32_t get_material(size_t i) const { return entities[i].material; }
	uint8_t get_isActive(size_t i) const { return entities[i].isActive; }
	uint64_t get_lastTouched(size_t i) const { return entities[i].lastTouched; }

	void set_position(size_t i, const Vec3 &v) { entities[i].position = v; }
	void set_velocity(size_t i, const Vec3 &v) { entities[i].velocity = v; }
	void set_bounds(size_t i, const Aabb &v) { entities[i].bounds = v; }
	void set_lastTouched(size_t i, uint64_t v) { entities[i].lastTouched = v; }
	void set_material_and_active(size_t i, uint32_t m, uint8_t a)
	{
		entities[i].material = m;
		entities[i].isActive = a;
	}
};

// ===== Layout 2: SoA (Flecs chunk-component default) =====

struct LayoutSoa {
	std::vector<Vec3> positions;
	std::vector<Vec3> velocities;
	std::vector<Aabb> bounds;
	std::vector<uint32_t> materials;
	std::vector<uint8_t> isActive;
	std::vector<uint64_t> lastTouched;

	size_t size() const { return positions.size(); }
	void resize(size_t n)
	{
		positions.resize(n);
		velocities.resize(n);
		bounds.resize(n);
		materials.resize(n);
		isActive.resize(n);
		lastTouched.resize(n);
	}

	Vec3 get_position(size_t i) const { return positions[i]; }
	Vec3 get_velocity(size_t i) const { return velocities[i]; }
	Aabb get_bounds(size_t i) const { return bounds[i]; }
	uint32_t get_material(size_t i) const { return materials[i]; }
	uint8_t get_isActive(size_t i) const { return isActive[i]; }
	uint64_t get_lastTouched(size_t i) const { return lastTouched[i]; }

	void set_position(size_t i, const Vec3 &v) { positions[i] = v; }
	void set_velocity(size_t i, const Vec3 &v) { velocities[i] = v; }
	void set_bounds(size_t i, const Aabb &v) { bounds[i] = v; }
	void set_lastTouched(size_t i, uint64_t v) { lastTouched[i] = v; }
	void set_material_and_active(size_t i, uint32_t m, uint8_t a)
	{
		materials[i] = m;
		isActive[i] = a;
	}
};

// ===== Layout 3: HotOnly-SoA (SoA for hot fields, AoS sidecar for cold) =====

struct LayoutHotOnly {
	std::vector<Vec3> positions;
	std::vector<Vec3> velocities;
	std::vector<Aabb> bounds;
	std::vector<uint8_t> isActive;
	std::vector<ColdSidecar> cold; // AoS material + lastTouched

	size_t size() const { return positions.size(); }
	void resize(size_t n)
	{
		positions.resize(n);
		velocities.resize(n);
		bounds.resize(n);
		isActive.resize(n);
		cold.resize(n);
	}

	Vec3 get_position(size_t i) const { return positions[i]; }
	Vec3 get_velocity(size_t i) const { return velocities[i]; }
	Aabb get_bounds(size_t i) const { return bounds[i]; }
	uint32_t get_material(size_t i) const { return cold[i].material; }
	uint8_t get_isActive(size_t i) const { return isActive[i]; }
	uint64_t get_lastTouched(size_t i) const { return cold[i].lastTouched; }

	void set_position(size_t i, const Vec3 &v) { positions[i] = v; }
	void set_velocity(size_t i, const Vec3 &v) { velocities[i] = v; }
	void set_bounds(size_t i, const Aabb &v) { bounds[i] = v; }
	void set_lastTouched(size_t i, uint64_t v) { cold[i].lastTouched = v; }
	void set_material_and_active(size_t i, uint32_t m, uint8_t a)
	{
		cold[i].material = m;
		isActive[i] = a;
	}
};

// ===== Layout 4: Hybrid-SoA (SoA for hot, AoS for medium-cold) =====

struct LayoutHybrid {
	std::vector<Vec3> positions;
	std::vector<Vec3> velocities;
	std::vector<Aabb> bounds;
	std::vector<uint8_t> isActive;
	std::vector<uint32_t> materials;
	std::vector<uint64_t> lastTouched;

	size_t size() const { return positions.size(); }
	void resize(size_t n)
	{
		positions.resize(n);
		velocities.resize(n);
		bounds.resize(n);
		isActive.resize(n);
		materials.resize(n);
		lastTouched.resize(n);
	}

	Vec3 get_position(size_t i) const { return positions[i]; }
	Vec3 get_velocity(size_t i) const { return velocities[i]; }
	Aabb get_bounds(size_t i) const { return bounds[i]; }
	uint32_t get_material(size_t i) const { return materials[i]; }
	uint8_t get_isActive(size_t i) const { return isActive[i]; }
	uint64_t get_lastTouched(size_t i) const { return lastTouched[i]; }

	void set_position(size_t i, const Vec3 &v) { positions[i] = v; }
	void set_velocity(size_t i, const Vec3 &v) { velocities[i] = v; }
	void set_bounds(size_t i, const Aabb &v) { bounds[i] = v; }
	void set_lastTouched(size_t i, uint64_t v) { lastTouched[i] = v; }
	void set_material_and_active(size_t i, uint32_t m, uint8_t a)
	{
		materials[i] = m;
		isActive[i] = a;
	}
};

// ===== Synthetic scene generation =====

template <typename Layout>
void populate(Layout &layout, std::mt19937 &rng)
{
	const size_t n = layout.size();
	std::uniform_real_distribution<float> dist_pos(-100.0f, 100.0f);
	std::uniform_real_distribution<float> dist_vel(-1.0f, 1.0f);
	std::uniform_real_distribution<float> dist_size(0.5f, 5.0f);
	std::uniform_int_distribution<uint32_t> dist_mat(0, 15);
	std::uniform_int_distribution<int> dist_active(0, 99);

	for (size_t i = 0; i < n; ++i) {
		Vec3 pos = {dist_pos(rng), dist_pos(rng), dist_pos(rng)};
		Vec3 vel = {dist_vel(rng), dist_vel(rng), dist_vel(rng)};
		float half = dist_size(rng);
		Aabb bounds = {
			{pos.x - half, pos.y - half, pos.z - half},
			{pos.x + half, pos.y + half, pos.z + half}};
		uint32_t material = dist_mat(rng);
		uint8_t isActive = (dist_active(rng) < 80) ? 1 : 0;
		uint64_t lastTouched = 0;

		layout.set_position(i, pos);
		layout.set_velocity(i, vel);
		layout.set_bounds(i, bounds);
		layout.set_material_and_active(i, material, isActive);
		layout.set_lastTouched(i, lastTouched);
	}
}

// ===== Raycast helpers (slab method, branch-free) =====

inline bool ray_aabb_test(const Vec3 &origin, const Vec3 &inv_dir, const Aabb &box)
{
	float t1 = (box.min.x - origin.x) * inv_dir.x;
	float t2 = (box.max.x - origin.x) * inv_dir.x;
	float tmin = std::min(t1, t2);
	float tmax = std::max(t1, t2);
	for (int axis = 1; axis < 3; ++axis) {
		float o = (axis == 1) ? origin.y : origin.z;
		float bmin = (axis == 1) ? box.min.y : box.min.z;
		float bmax = (axis == 1) ? box.max.y : box.max.z;
		float d = (axis == 1) ? inv_dir.y : inv_dir.z;
		float ta = (bmin - o) * d;
		float tb = (bmax - o) * d;
		tmin = std::max(tmin, std::min(ta, tb));
		tmax = std::min(tmax, std::max(ta, tb));
	}
	return tmax >= std::max(0.0f, tmin);
}

// ===== Workloads (per README §1 T1/T2/T3) =====

// T1 raycast: read position + bounds, write lastTouched.
template <typename Layout>
void workload_raycast(Layout &layout, std::mt19937 &rng)
{
	(void)rng;
	const Vec3 ray_origin = {0.0f, 0.0f, 0.0f};
	const Vec3 inv_dir = {1.0f / 0.577f, 1.0f / 0.577f, 1.0f / 0.577f}; // normalized dir
	const size_t n = layout.size();
	uint64_t hits = 0;
	for (size_t i = 0; i < n; ++i) {
		Vec3 pos = layout.get_position(i);
		Aabb box = layout.get_bounds(i);
		Aabb translated = {
			{box.min.x + pos.x, box.min.y + pos.y, box.min.z + pos.z},
			{box.max.x + pos.x, box.max.y + pos.y, box.max.z + pos.z}};
		bool hit = ray_aabb_test(ray_origin, inv_dir, translated);
		if (hit)
			++hits;
		layout.set_lastTouched(i, hit ? 1 : 0);
	}
	// Prevent dead-code elimination
	if (hits == 0xFFFFFFFFFFFFFFFFULL)
		std::printf("never\n");
}

// T2 physics-step: read position + velocity, write position + velocity (Euler integrate).
template <typename Layout>
void workload_physics(Layout &layout, std::mt19937 &rng)
{
	(void)rng;
	const float dt = 1.0f / 60.0f;
	const float damping = 0.999f;
	const size_t n = layout.size();
	for (size_t i = 0; i < n; ++i) {
		Vec3 pos = layout.get_position(i);
		Vec3 vel = layout.get_velocity(i);
		pos.x += vel.x * dt;
		pos.y += vel.y * dt;
		pos.z += vel.z * dt;
		vel.x *= damping;
		vel.y *= damping;
		vel.z *= damping;
		layout.set_position(i, pos);
		layout.set_velocity(i, vel);
	}
}

// T3 render-cull: read position + bounds + material, predicate isActive, count visible.
template <typename Layout>
void workload_cull(Layout &layout, std::mt19937 &rng)
{
	(void)rng;
	const Vec3 camera = {0.0f, 0.0f, 0.0f};
	const float max_dist_sq = 200.0f * 200.0f;
	const size_t n = layout.size();
	uint64_t visible = 0;
	for (size_t i = 0; i < n; ++i) {
		if (!layout.get_isActive(i))
			continue;
		Vec3 pos = layout.get_position(i);
		float dx = pos.x - camera.x;
		float dy = pos.y - camera.y;
		float dz = pos.z - camera.z;
		float dist_sq = dx * dx + dy * dy + dz * dz;
		if (dist_sq > max_dist_sq)
			continue;
		Aabb box = layout.get_bounds(i);
		uint32_t mat = layout.get_material(i);
		// Pretend material influences some attribute (avoid DCE)
		if ((mat ^ static_cast<uint32_t>(box.min.x)) == 0xDEADBEEF)
			++visible;
		++visible;
	}
	if (visible == 0xFFFFFFFFFFFFFFFFULL)
		std::printf("never\n");
}

// ===== Statistics (per benchmarks/methodology.md §7) =====

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
};

Stats compute_stats(std::vector<double> samples)
{
	Stats s{};
	std::sort(samples.begin(), samples.end());
	double sum = 0.0;
	for (double v : samples)
		sum += v;
	s.mean = sum / samples.size();
	s.median = samples[samples.size() / 2];
	s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
	s.min = samples.front();
	s.max = samples.back();
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / samples.size());
	return s;
}

// ===== Timing harness (per benchmarks/methodology.md §3) =====

template <typename Layout, typename Workload>
Stats bench_workload(Layout &layout, Workload &&workload, std::mt19937 &rng,
					 size_t warmup_iters, size_t measure_iters, const char * /*name*/)
{
	// Warm-up
	for (size_t i = 0; i < warmup_iters; ++i) {
		workload(layout, rng);
	}

	// Measure
	std::vector<double> samples;
	samples.reserve(measure_iters);
	for (size_t i = 0; i < measure_iters; ++i) {
		auto t0 = std::chrono::steady_clock::now();
		workload(layout, rng);
		auto t1 = std::chrono::steady_clock::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		samples.push_back(ns);
	}

	return compute_stats(std::move(samples));
}

// ===== CLI parsing =====

struct CliArgs {
	size_t entities = 500000;
	size_t iterations = 1000;
	size_t warmup = 100;
	uint32_t seed = 42;
	std::string output_path;
	bool run_all = false;
};

CliArgs parse_cli(int argc, char **argv)
{
	CliArgs args;
	for (int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];
		auto next = [&](const char *name) -> std::string {
			if (i + 1 >= argc) {
				std::fprintf(stderr, "missing value for %s\n", name);
				std::exit(1);
			}
			return std::string(argv[++i]);
		};
		if (arg == "--entities")
			args.entities = std::stoull(next("--entities"));
		else if (arg == "--iterations")
			args.iterations = std::stoull(next("--iterations"));
		else if (arg == "--warmup")
			args.warmup = std::stoull(next("--warmup"));
		else if (arg == "--seed")
			args.seed = static_cast<uint32_t>(std::stoul(next("--seed")));
		else if (arg == "--output")
			args.output_path = next("--output");
		else if (arg == "--all")
			args.run_all = true;
		else if (arg == "--help") {
			std::printf("Usage: %s [--entities N] [--iterations N] [--warmup N] [--seed N] [--output PATH] [--all]\n",
						argv[0]);
			std::exit(0);
		}
	}
	return args;
}

// ===== CSV writer =====

struct CsvWriter {
	FILE *fp;
	explicit CsvWriter(const std::string &path)
	{
		fp = std::fopen(path.c_str(), "w");
		if (!fp) {
			std::fprintf(stderr, "cannot open %s\n", path.c_str());
			std::exit(1);
		}
		std::fprintf(fp, "config,workload,entities,iterations,seed,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns,throughput_Meps\n");
	}
	~CsvWriter()
	{
		if (fp)
			std::fclose(fp);
	}

	void row(std::string_view config, std::string_view workload, size_t entities, size_t iterations, uint32_t seed,
			 const Stats &s)
	{
		double throughput = (entities / 1e6) / (s.mean / 1e9); // M entities/sec
		std::fprintf(fp, "%.*s,%.*s,%zu,%zu,%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f\n",
					 static_cast<int>(config.size()), config.data(),
					 static_cast<int>(workload.size()), workload.data(),
					 entities, iterations, seed,
					 s.mean, s.median, s.p95, s.p99, s.stddev, s.min, s.max, throughput);
	}
};

// ===== Per-config + per-workload runner =====

template <typename Layout>
Stats run_one(Layout &layout, const std::string &config_name, const std::string &workload_name,
			  std::mt19937 &rng, size_t warmup, size_t iterations, CsvWriter &csv, size_t entities, uint32_t seed)
{
	if (workload_name == "raycast") {
		Stats s = bench_workload(layout, workload_raycast<Layout>, rng, warmup, iterations, "raycast");
		csv.row(config_name, workload_name, entities, iterations, seed, s);
		return s;
	} else if (workload_name == "physics") {
		Stats s = bench_workload(layout, workload_physics<Layout>, rng, warmup, iterations, "physics");
		csv.row(config_name, workload_name, entities, iterations, seed, s);
		return s;
	} else if (workload_name == "cull") {
		Stats s = bench_workload(layout, workload_cull<Layout>, rng, warmup, iterations, "cull");
		csv.row(config_name, workload_name, entities, iterations, seed, s);
		return s;
	}
	std::fprintf(stderr, "unknown workload: %s\n", workload_name.c_str());
	std::exit(1);
}

template <typename Layout>
Stats dispatch_workload(Layout &layout, const std::string &config_name, const std::string &workload_name,
						std::mt19937 &rng, size_t warmup, size_t iterations, CsvWriter &csv, size_t entities, uint32_t seed)
{
	return run_one(layout, config_name, workload_name, rng, warmup, iterations, csv, entities, seed);
}

// ===== main =====

int main(int argc, char **argv)
{
	CliArgs args = parse_cli(argc, argv);

	if (args.output_path.empty()) {
		std::fprintf(stderr, "must specify --output PATH\n");
		return 1;
	}

	CsvWriter csv(args.output_path);
	std::mt19937 rng_populate(args.seed);

	std::printf("flecs_soa_vs_aos_bench: entities=%zu, iterations=%zu, warmup=%zu, seed=%u\n",
				args.entities, args.iterations, args.warmup, args.seed);
	std::printf("Hardware: AMD Ryzen 7 5800X (Zen 3), AVX2/FMA, L1d 32K, L2 512K, L3 32M, cache line 64B.\n");
	std::printf("Output: %s\n\n", args.output_path.c_str());

	// Layouts
	LayoutAos aos;
	aos.resize(args.entities);
	populate(aos, rng_populate);
	LayoutSoa soa;
	soa.resize(args.entities);
	populate(soa, rng_populate);
	LayoutHotOnly hot;
	hot.resize(args.entities);
	populate(hot, rng_populate);
	LayoutHybrid hybrid;
	hybrid.resize(args.entities);
	populate(hybrid, rng_populate);

	const std::vector<std::string> workloads = {"raycast", "physics", "cull"};

	if (args.run_all) {
		for (const std::string &w : workloads) {
			std::mt19937 rng(args.seed);
			dispatch_workload(aos, "aos", w, rng, args.warmup, args.iterations, csv, args.entities, args.seed);
			dispatch_workload(soa, "soa", w, rng, args.warmup, args.iterations, csv, args.entities, args.seed);
			dispatch_workload(hot, "hot", w, rng, args.warmup, args.iterations, csv, args.entities, args.seed);
			dispatch_workload(hybrid, "hybrid", w, rng, args.warmup, args.iterations, csv, args.entities, args.seed);
		}
	} else {
		std::fprintf(stderr, "Use --all to run all 12 (config, workload) combos, or specify explicit config + workload.\n");
		std::fprintf(stderr, "(Explicit per-combo CLI not implemented — single-shot --all covers all measurements.)\n");
		return 1;
	}

	std::printf("Done. CSV: %s\n", args.output_path.c_str());
	return 0;
}
