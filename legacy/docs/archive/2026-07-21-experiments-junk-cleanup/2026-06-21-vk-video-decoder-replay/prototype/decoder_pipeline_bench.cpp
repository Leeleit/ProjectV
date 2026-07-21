// 2026-06-21-vk-video-decoder-replay experiment.
// Standalone C++26 CPU analytical cost model для in-engine Vulkan Video decode pipeline.
// Per `AGENTS.md §13` — analytical model only, no Vulkan init, no real vkCmdDecodeVideoKHR dispatch.
//
// 3 strategies × 4 scenarios × 3 codecs × 2 bitrate × 3 seeds × 100 frames + 10 warmup
// = 216 configs × 100 frames = 21,600 main measurements.
//
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic decoder_pipeline_bench.cpp -o build/decoder_pipeline_bench

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace vdr {

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
};

Stats Compute(const std::vector<double> &samples)
{
	Stats s{};
	if (samples.empty())
		return s;
	std::vector<double> sorted = samples;
	std::sort(sorted.begin(), sorted.end());
	double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
	s.mean = sum / static_cast<double>(samples.size());
	s.median = sorted[sorted.size() / 2];
	s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
	s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
	s.min = sorted.front();
	s.max = sorted.back();
	return s;
}

enum class Codec {
	H264,
	H265,
	AV1,
};

enum class Scenario {
	Intro_720p30,
	Cinematic_1080p60,
	Replay_1080p60,
	Trailer_4K30,
};

enum class Bitrate {
	Low_2Mbps,
	High_8Mbps,
};

enum class Strategy {
	A_ExternalPlayer,
	B_FFmpegSWDecoder,
	C_VulkanVideoHWDecoder,
};

constexpr const char *CodecName(Codec c)
{
	switch (c) {
	case Codec::H264:
		return "H264";
	case Codec::H265:
		return "H265";
	case Codec::AV1:
		return "AV1";
	}
	return "?";
}

constexpr const char *ScenarioName(Scenario s)
{
	switch (s) {
	case Scenario::Intro_720p30:
		return "intro_720p30";
	case Scenario::Cinematic_1080p60:
		return "cinematic_1080p60";
	case Scenario::Replay_1080p60:
		return "replay_1080p60";
	case Scenario::Trailer_4K30:
		return "trailer_4k30";
	}
	return "?";
}

constexpr const char *BitrateName(Bitrate b)
{
	switch (b) {
	case Bitrate::Low_2Mbps:
		return "2mbps";
	case Bitrate::High_8Mbps:
		return "8mbps";
	}
	return "?";
}

constexpr const char *StrategyName(Strategy s)
{
	switch (s) {
	case Strategy::A_ExternalPlayer:
		return "A_ExternalPlayer";
	case Strategy::B_FFmpegSWDecoder:
		return "B_FFmpegSWDecoder";
	case Strategy::C_VulkanVideoHWDecoder:
		return "C_VulkanVideoHWDecoder";
	}
	return "?";
}

struct Resolution {
	int width;
	int height;
	int fps;
};

Resolution GetResolution(Scenario s)
{
	switch (s) {
	case Scenario::Intro_720p30:
		return {1280, 720, 30};
	case Scenario::Cinematic_1080p60:
		return {1920, 1080, 60};
	case Scenario::Replay_1080p60:
		return {1920, 1080, 60};
	case Scenario::Trailer_4K30:
		return {3840, 2160, 30};
	}
	return {0, 0, 0};
}

struct FrameCost {
	double cpu_time_us;
	double gpu_time_us;
	double upload_time_us;
	double vram_mb;
	double sysram_mb;
	double first_frame_latency_us;
};

double SWDecoderBase1080pUs(Codec c)
{
	switch (c) {
	case Codec::H264:
		return 3500.0;
	case Codec::H265:
		return 5000.0;
	case Codec::AV1:
		return 8000.0;
	}
	return 0.0;
}

double HWDecoderBase1080pUs(Codec c)
{
	switch (c) {
	case Codec::H264:
		return 100.0;
	case Codec::H265:
		return 80.0;
	case Codec::AV1:
		return 150.0;
	}
	return 0.0;
}

double ScenarioComplexityMultiplier(Scenario s)
{
	switch (s) {
	case Scenario::Intro_720p30:
		return 0.8;
	case Scenario::Cinematic_1080p60:
		return 1.0;
	case Scenario::Replay_1080p60:
		return 1.2;
	case Scenario::Trailer_4K30:
		return 2.0;
	}
	return 1.0;
}

double ResolutionScale(Scenario s)
{
	auto res = GetResolution(s);
	return (static_cast<double>(res.width) * static_cast<double>(res.height)) /
		   (1920.0 * 1080.0);
}

double BitrateComplexityMultiplier(Bitrate b)
{
	switch (b) {
	case Bitrate::Low_2Mbps:
		return 0.7;
	case Bitrate::High_8Mbps:
		return 1.3;
	}
	return 1.0;
}

double BufferToImageCostUs(Scenario s)
{
	return 150.0 * ResolutionScale(s);
}

double DPBMemoryMB(Scenario s)
{
	auto res = GetResolution(s);
	constexpr int kDPBFrames = 5;
	constexpr double kBytesPerPixel420 = 1.5;
	return (static_cast<double>(kDPBFrames) * static_cast<double>(res.width) *
			static_cast<double>(res.height) * kBytesPerPixel420) /
		   (1024.0 * 1024.0);
}

double OutputImageMB(Scenario s)
{
	auto res = GetResolution(s);
	return (static_cast<double>(res.width) * static_cast<double>(res.height) * 4.0) /
		   (1024.0 * 1024.0);
}

FrameCost A_Cost(Scenario s, Bitrate /*b*/)
{
	auto res = GetResolution(s);
	FrameCost c{};
	c.cpu_time_us = 250.0 + 50.0 * ResolutionScale(s);
	c.gpu_time_us = 50.0;
	c.upload_time_us = 0.0;
	c.vram_mb = 0.0;
	c.sysram_mb = (static_cast<double>(res.width) * static_cast<double>(res.height) * 4.0) /
				  (1024.0 * 1024.0);
	c.first_frame_latency_us = 100000.0;
	return c;
}

FrameCost B_Cost(Scenario s, Bitrate b, Codec codec)
{
	FrameCost fc{};
	double base_sw = SWDecoderBase1080pUs(codec);
	fc.cpu_time_us = base_sw * ResolutionScale(s) * ScenarioComplexityMultiplier(s) *
					 BitrateComplexityMultiplier(b);
	fc.gpu_time_us = 0.0;
	fc.upload_time_us = BufferToImageCostUs(s);
	fc.vram_mb = OutputImageMB(s);
	fc.sysram_mb = (1920.0 * 1080.0 * 4.0) / (1024.0 * 1024.0) * ResolutionScale(s);
	fc.first_frame_latency_us = 50000.0;
	return fc;
}

FrameCost C_Cost(Scenario s, Bitrate b, Codec codec)
{
	FrameCost fc{};
	double base_hw = HWDecoderBase1080pUs(codec);
	fc.cpu_time_us = 10.0 + 5.0 * ResolutionScale(s);
	fc.gpu_time_us = base_hw * ResolutionScale(s) * ScenarioComplexityMultiplier(s) *
					 BitrateComplexityMultiplier(b);
	fc.upload_time_us = 0.0;
	fc.vram_mb = DPBMemoryMB(s) + OutputImageMB(s);
	fc.sysram_mb = 0.0;
	fc.first_frame_latency_us = 1000.0;
	return fc;
}

FrameCost ComputeCost(Strategy strategy, Scenario s, Bitrate b, Codec codec)
{
	switch (strategy) {
	case Strategy::A_ExternalPlayer:
		return A_Cost(s, b);
	case Strategy::B_FFmpegSWDecoder:
		return B_Cost(s, b, codec);
	case Strategy::C_VulkanVideoHWDecoder:
		return C_Cost(s, b, codec);
	}
	return {};
}

std::vector<double> GenerateFrameSamples(double base_us, double stddev_pct, int n_frames,
										 std::mt19937 &rng)
{
	double stddev = stddev_pct * base_us;
	std::normal_distribution<double> dist(base_us, stddev);
	std::vector<double> samples;
	samples.reserve(n_frames);
	for (int i = 0; i < n_frames; ++i) {
		double sample = std::max(0.0, dist(rng));
		samples.push_back(sample);
	}
	return samples;
}

struct ConfigResult {
	std::string strategy;
	std::string scenario;
	std::string codec;
	std::string bitrate;
	int seed;
	int n_frames;
	double mean_total_us;
	double p95_total_us;
	double p99_total_us;
	double std_total_us;
	double peak_vram_mb;
	double peak_sysram_mb;
	double first_frame_us;
};

ConfigResult RunConfig(Strategy strategy, Scenario s, Bitrate b, Codec codec, int seed,
					   int n_warmup, int n_measure)
{
	std::mt19937 rng(seed);
	FrameCost cost = ComputeCost(strategy, s, b, codec);

	double stddev_pct = 0.15;
	if (cost.cpu_time_us > 1000.0)
		stddev_pct = 0.10;
	if (cost.gpu_time_us > 0.0) {
		stddev_pct = (cost.gpu_time_us > 100.0) ? 0.08 : 0.12;
	}

	auto warmup_cpu = GenerateFrameSamples(cost.cpu_time_us, stddev_pct, n_warmup, rng);
	auto warmup_gpu = GenerateFrameSamples(cost.gpu_time_us, stddev_pct, n_warmup, rng);
	(void)warmup_cpu;
	(void)warmup_gpu;

	auto cpu_samples = GenerateFrameSamples(cost.cpu_time_us, stddev_pct, n_measure, rng);
	auto gpu_samples = GenerateFrameSamples(cost.gpu_time_us, stddev_pct, n_measure, rng);

	std::vector<double> total_samples;
	total_samples.reserve(n_measure);
	for (size_t i = 0; i < cpu_samples.size(); ++i) {
		double total = cpu_samples[i] + cost.upload_time_us + gpu_samples[i];
		if (i == 0)
			total += cost.first_frame_latency_us;
		total_samples.push_back(total);
	}

	Stats s_stats = Compute(total_samples);

	ConfigResult r{};
	r.strategy = StrategyName(strategy);
	r.scenario = ScenarioName(s);
	r.codec = CodecName(codec);
	r.bitrate = BitrateName(b);
	r.seed = seed;
	r.n_frames = n_measure;
	r.mean_total_us = s_stats.mean;
	r.p95_total_us = s_stats.p95;
	r.p99_total_us = s_stats.p99;
	r.std_total_us = s_stats.stddev;
	r.peak_vram_mb = cost.vram_mb;
	r.peak_sysram_mb = cost.sysram_mb;
	r.first_frame_us = cost.first_frame_latency_us;
	return r;
}

int Main()
{
	constexpr int kWarmup = 10;
	constexpr int kMeasure = 100;
	constexpr int kSeeds[] = {1, 7, 42};

	std::vector<ConfigResult> results;

	for (auto strategy : {Strategy::A_ExternalPlayer, Strategy::B_FFmpegSWDecoder,
						  Strategy::C_VulkanVideoHWDecoder}) {
		for (auto scenario : {Scenario::Intro_720p30, Scenario::Cinematic_1080p60,
							  Scenario::Replay_1080p60, Scenario::Trailer_4K30}) {
			for (auto codec : {Codec::H264, Codec::H265, Codec::AV1}) {
				for (auto bitrate : {Bitrate::Low_2Mbps, Bitrate::High_8Mbps}) {
					for (int seed : kSeeds) {
						auto r = RunConfig(strategy, scenario, bitrate, codec, seed, kWarmup,
										   kMeasure);
						results.push_back(r);
					}
				}
			}
		}
	}

	std::filesystem::path out_path = "build/results.csv";
	std::filesystem::create_directories(out_path.parent_path());
	std::ofstream out(out_path);
	out << "strategy,scenario,codec,bitrate,seed,n_frames,mean_total_us,p95_total_us,p99_total_us,"
		<< "std_total_us,peak_vram_mb,peak_sysram_mb,first_frame_us\n";
	out << std::fixed;
	out.precision(4);
	for (const auto &r : results) {
		out << r.strategy << "," << r.scenario << "," << r.codec << "," << r.bitrate << ","
			<< r.seed << "," << r.n_frames << "," << r.mean_total_us << "," << r.p95_total_us
			<< "," << r.p99_total_us << "," << r.std_total_us << "," << r.peak_vram_mb << ","
			<< r.peak_sysram_mb << "," << r.first_frame_us << "\n";
	}
	out.close();

	std::printf("=== 2026-06-21-vk-video-decoder-replay analytical cost model ===\n");
	std::printf("Configurations: %zu (3 strategies x 4 scenarios x 3 codecs x 2 bitrate x 3 seeds)\n",
				results.size());
	std::printf("Per-config: %d warmup + %d measure frames\n", kWarmup, kMeasure);
	std::printf("Total measurements: %zu x %d = %d main measurements\n", results.size(),
				kMeasure, static_cast<int>(results.size() * kMeasure));

	std::map<std::string, std::vector<ConfigResult>> by_strategy;
	for (const auto &r : results) {
		by_strategy[r.strategy].push_back(r);
	}
	for (const auto &[strategy, configs] : by_strategy) {
		std::vector<double> means;
		for (const auto &c : configs)
			means.push_back(c.mean_total_us);
		std::vector<double> p99s;
		for (const auto &c : configs)
			p99s.push_back(c.p99_total_us);
		Stats s_mean = Compute(means);
		Stats s_p99 = Compute(p99s);
		double mean_vram = 0;
		for (const auto &c : configs)
			mean_vram += c.peak_vram_mb;
		mean_vram /= static_cast<double>(configs.size());
		std::printf("\n--- %s ---\n", strategy.c_str());
		std::printf("  configs: %zu\n", configs.size());
		std::printf("  per-config mean_total_us: mean=%.1f p95=%.1f p99=%.1f std=%.1f us\n",
					s_mean.mean, s_mean.p95, s_mean.p99, s_mean.stddev);
		std::printf("  per-config p99_total_us:  mean=%.1f p95=%.1f p99=%.1f us\n", s_p99.mean,
					s_p99.p95, s_p99.p99);
		std::printf("  peak_vram_mb avg: %.2f MiB\n", mean_vram);
	}
	std::printf("\nResults written to: %s\n", out_path.string().c_str());
	return 0;
}

} // namespace vdr

int main()
{
	return vdr::Main();
}