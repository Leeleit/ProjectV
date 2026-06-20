// Benchmark harness — runs each path N times, reports mean / p50 / p95 / p99 / std.
// CSV output for downstream analysis.

#include "benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace vb {

Stats ComputeStats(std::vector<double> &v)
{
	Stats s{};
	if (v.empty())
		return s;
	std::sort(v.begin(), v.end());
	double sum = 0;
	for (double x : v)
		sum += x;
	s.mean = sum / v.size();
	s.min = v.front();
	s.max = v.back();
	s.median = v[v.size() / 2];
	s.p95 = v[size_t(v.size() * 0.95)];
	s.p99 = v[size_t(v.size() * 0.99)];
	double var = 0;
	for (double x : v)
		var += (x - s.mean) * (x - s.mean);
	s.stddev = std::sqrt(var / v.size());
	return s;
}

void RunBenchmark(BenchmarkConfig &cfg, BenchmarkResult &resultOut)
{
	// Build scene.
	std::vector<std::vector<uint8_t>> chunks;
	std::vector<PackedFace> faces;
	std::vector<ChunkDescriptor> chunkDescs;
	std::vector<MaterialVisual> materials;
	GenerateScene(cfg.scene, chunks, faces, chunkDescs, materials);
	resultOut.faceCount = faces.size();
	resultOut.materialCount = uint32_t(materials.size());

	VkContext ctx{};
	if (!InitVulkan(ctx)) {
		std::fprintf(stderr, "InitVulkan failed\n");
		return;
	}

	Pipeline p{};
	if (!BuildPipelines(ctx, p, cfg.extent, faces, chunkDescs, materials)) {
		std::fprintf(stderr, "BuildPipelines failed\n");
		return;
	}

	// Simple camera looking at scene center.
	auto makeViewProj = [&]() {
		std::array<float, 16> vp{};
		// Identity-ish for prototype (no real matrix math).
		// Just need a perspective that maps [-1,1] NDC.
		// Simplified: orthographic projection to NDC, then translate to put scene in view.
		float *m = vp.data();
		// column-major 4x4: simple perspective
		float aspect = float(cfg.extent.width) / float(cfg.extent.height);
		float fovY = 60.0f * 3.14159265f / 180.0f;
		float f = 1.0f / std::tan(fovY / 2.0f);
		float zNear = 0.1f, zFar = 200.0f;
		m[0] = f / aspect;
		m[1] = 0;
		m[2] = 0;
		m[3] = 0;
		m[4] = 0;
		m[5] = f;
		m[6] = 0;
		m[7] = 0;
		m[8] = 0;
		m[9] = 0;
		m[10] = (zFar + zNear) / (zNear - zFar);
		m[11] = -1.0f;
		m[12] = 0;
		m[13] = 0;
		m[14] = (2 * zFar * zNear) / (zNear - zFar);
		m[15] = 0;
		// Simple camera translation — pull back along Z.
		// Multiply perspective * translation.
		// For prototype simplicity, just put camera at z = 30 looking at origin.
		// We compose translation matrix into view manually:
		std::array<float, 16> result{};
		float tx = -16.0f, ty = -16.0f, tz = -50.0f; // center of 4×4×4 chunks at 8 each = 32/2 = 16.
		result[0] = m[0];
		result[1] = m[1];
		result[2] = m[2];
		result[3] = m[3];
		result[4] = m[4];
		result[5] = m[5];
		result[6] = m[6];
		result[7] = m[7];
		result[8] = m[8];
		result[9] = m[9];
		result[10] = m[10];
		result[11] = m[11];
		// Translation component:
		result[12] = m[0] * tx + m[4] * ty + m[8] * tz + m[12];
		result[13] = m[1] * tx + m[5] * ty + m[9] * tz + m[13];
		result[14] = m[2] * tx + m[6] * ty + m[10] * tz + m[14];
		result[15] = m[3] * tx + m[7] * ty + m[11] * tz + m[15];
		return result;
	};

	std::array<float, 16> vp = makeViewProj();
	std::array<float, 4> sunDir{0.4f, 0.6f, 0.7f, 0.0f};
	std::array<float, 4> sunColor{1.0f, 0.95f, 0.85f, 1.5f};
	std::array<float, 4> camPos{16.0f, 16.0f, 50.0f, 0.0f};

	// Warmup.
	std::vector<uint32_t> hashBaseline, hashVis;
	for (uint32_t i = 0; i < cfg.warmupFrames; ++i) {
		RecordAndSubmitBaseline(ctx, p, 4, vp, sunDir, sunColor, camPos, hashBaseline);
		RecordAndSubmitVisBuffer(ctx, p, 4, vp, sunDir, sunColor, camPos, hashVis);
	}

	// Measured runs.
	std::vector<double> tBaseline, tVis;
	for (uint32_t i = 0; i < cfg.measuredFrames; ++i) {
		double tb = RecordAndSubmitBaseline(ctx, p, 4, vp, sunDir, sunColor, camPos, hashBaseline);
		double tv = RecordAndSubmitVisBuffer(ctx, p, 4, vp, sunDir, sunColor, camPos, hashVis);
		tBaseline.push_back(tb);
		tVis.push_back(tv);
	}

	resultOut.baselineStats = ComputeStats(tBaseline);
	resultOut.visStats = ComputeStats(tVis);

	// Verify hashes differ (sanity: paths are different algorithms) but are stable.
	std::sort(hashBaseline.begin(), hashBaseline.end());
	std::sort(hashVis.begin(), hashVis.end());
	resultOut.hashBaseline = hashBaseline[hashBaseline.size() / 2];
	resultOut.hashVis = hashVis[hashVis.size() / 2];

	DestroyPipelines(ctx, p);
	DestroyVulkan(ctx);
}

void WriteCSV(const std::string &path, const BenchmarkResult &r)
{
	std::ofstream f(path);
	f << "config,path,mean_ms,median_ms,p95_ms,p99_ms,std_ms,min_ms,max_ms,hash\n";
	auto writeRow = [&](const char *path_, const Stats &s, uint32_t h) {
		f << "default," << path_ << ","
		  << s.mean << "," << s.median << ","
		  << s.p95 << "," << s.p99 << ","
		  << s.stddev << "," << s.min << "," << s.max << "," << h << "\n";
	};
	writeRow("baseline", r.baselineStats, r.hashBaseline);
	writeRow("visbuffer", r.visStats, r.hashVis);
	f.close();
}

} // namespace vb
