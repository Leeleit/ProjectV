// bench.cpp - Standalone benchmark comparing simple std::thread pool vs BS::thread_pool
// (work stealing) on synthetic ProjectV chunk generation workload.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     bench.cpp -o /tmp/bench_pool
//
// Run:
//   /tmp/bench_pool                    # default: 30 iters per config, all configs
//   /tmp/bench_pool 100                # 100 iters per config (slower, more accurate)
//
// Output:
//   stdout: human-readable summary
//   ../results.csv: machine-readable per-config stats
//
// Scope per docs/experiments/AGENTS.md section 2: standalone research artifact,
// NOT part of ProjectV mainline. Zero ProjectV source dependencies (only stdlib
// + vendored BS::thread_pool.hpp single header from upstream v5.0.0).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sched.h>
#include <string>
#include <thread>
#include <vector>

#include "pool_simple.hpp"
#include "pool_bs.hpp"
#include "stats.hpp"
#include "workload.hpp"

namespace {

// Pin the calling thread to a single core for low-jitter harness timing.
// Per benchmarks/methodology.md §4 (CPU isolation).
void PinToCore(int core)
{
	cpu_set_t set;
	CPU_ZERO(&set);
	CPU_SET(core, &set);
	sched_setaffinity(0, sizeof(set), &set);
}

// Config: workload size (chunks) × pool impl × thread count.
struct Config {
	int numChunks;
	const char *poolName;
	int numThreads;
	// lambda to run measurement; populated by RunXxx
	// ...
};

using stats::Result;
using workload::ChunkGen;

// -----------------------------------------------------------------------------
// Measurement: serial baseline (1 thread, no pool).
// -----------------------------------------------------------------------------
Result BenchSerial(int numChunks, int iters)
{
	std::vector<ChunkGen> chunks(numChunks);
	PinToCore(0);

	// Warm-up
	for (int i = 0; i < 10; ++i) {
		for (int c = 0; c < numChunks; ++c) {
			chunks[c].generate(static_cast<uint32_t>(c) + 0xC0FFEEu);
		}
	}

	std::vector<double> samples;
	samples.reserve(iters);
	for (int i = 0; i < iters; ++i) {
		auto t0 = std::chrono::high_resolution_clock::now();
		for (int c = 0; c < numChunks; ++c) {
			chunks[c].generate(static_cast<uint32_t>(c) + 0xC0FFEEu);
		}
		auto t1 = std::chrono::high_resolution_clock::now();
		samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
	}

	// Verify output is non-zero (sanity: workload actually ran).
	uint64_t checksum = 0;
	for (const auto &c : chunks)
		checksum ^= c.rootHash;
	if (checksum == 0)
		std::fprintf(stderr, "WARN: serial checksum=0 (impossible)\n");

	return stats::Compute(samples);
}

// -----------------------------------------------------------------------------
// Measurement: pool_simple (mutex + condvar + shared deque, no work stealing).
// -----------------------------------------------------------------------------
Result BenchSimplePool(int numChunks, int numThreads, int iters)
{
	std::vector<ChunkGen> chunks(numChunks);
	PinToCore(0);

	pool_simple::ThreadPool pool(static_cast<std::size_t>(numThreads));

	auto runOnce = [&] {
		for (int c = 0; c < numChunks; ++c) {
			pool.Submit([c, &chunks] {
				chunks[c].generate(static_cast<uint32_t>(c) + 0xC0FFEEu);
			});
		}
		pool.WaitIdle();
	};

	// Warm-up
	for (int i = 0; i < 10; ++i)
		runOnce();

	std::vector<double> samples;
	samples.reserve(iters);
	for (int i = 0; i < iters; ++i) {
		auto t0 = std::chrono::high_resolution_clock::now();
		runOnce();
		auto t1 = std::chrono::high_resolution_clock::now();
		samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
	}

	return stats::Compute(samples);
}

// -----------------------------------------------------------------------------
// Measurement: BS::thread_pool (work stealing).
// -----------------------------------------------------------------------------
Result BenchBsPool(int numChunks, int numThreads, int iters)
{
	std::vector<ChunkGen> chunks(numChunks);
	PinToCore(0);

	pool_bs::ThreadPool pool(static_cast<std::size_t>(numThreads));

	auto runOnce = [&] {
		std::vector<std::future<void>> futures;
		futures.reserve(static_cast<std::size_t>(numChunks));
		for (int c = 0; c < numChunks; ++c) {
			futures.push_back(pool.submit_task([c, &chunks] {
				chunks[c].generate(static_cast<uint32_t>(c) + 0xC0FFEEu);
			}));
		}
		for (auto &f : futures)
			f.wait();
	};

	// Warm-up
	for (int i = 0; i < 10; ++i)
		runOnce();

	std::vector<double> samples;
	samples.reserve(iters);
	for (int i = 0; i < iters; ++i) {
		auto t0 = std::chrono::high_resolution_clock::now();
		runOnce();
		auto t1 = std::chrono::high_resolution_clock::now();
		samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
	}

	return stats::Compute(samples);
}

// -----------------------------------------------------------------------------
// Print single config result + return CSV row.
// -----------------------------------------------------------------------------
void PrintRow(FILE *out, const char *poolName, int numChunks, int numThreads,
			  const Result &r, double throughput)
{
	std::fprintf(out, "%-7s | chunks=%-6d threads=%-2d | mean=%8.3f ms | p95=%8.3f | "
					  "p99=%8.3f | std=%6.3f | min=%7.3f | max=%8.3f | throughput=%8.1f Mops/s\n",
				 poolName, numChunks, numThreads, r.mean, r.p95, r.p99, r.stddev,
				 r.min, r.max, throughput);
}

} // namespace

int main(int argc, char **argv)
{
	int iters = 30;
	if (argc >= 2)
		iters = std::atoi(argv[1]);
	if (iters < 1)
		iters = 1;

	std::printf("=== ProjectV work-stealing-job-system benchmark ===\n");
	std::printf("Hardware: AMD Ryzen 7 5800X (Zen 3, 8C/16T), governor=performance\n");
	std::printf("Compiler: clang++ 22.1.6, -O3 -march=native -std=c++26\n");
	std::printf("Workload: synthetic chunk gen (%d voxels/chunk, 1024 voxels = 8³ typical)\n",
				512);
	std::printf("Iters per config: %d (10 warm-up excluded)\n\n", iters);

	const int workloads[] = {256, 1024, 4096, 16384};
	const int threadCounts[] = {1, 4, 16};

	// CSV file
	std::ofstream csv(std::string("..") + "/results.csv");
	csv << "pool,num_chunks,num_threads,mean_ms,median_ms,p95_ms,p99_ms,std_ms,min_ms,max_ms,throughput_mops\n";

	// Header
	std::printf("%-7s | %-23s | %-25s | %-15s\n",
				"pool", "workload", "latency (ms)", "throughput");
	std::printf("--------+-------------------------+---------------------------+----------------\n");

	for (int n : workloads) {
		for (int t : threadCounts) {
			// Serial baseline only for t==1.
			if (t == 1) {
				Result r = BenchSerial(n, iters);
				double ops = static_cast<double>(n) * 512.0;
				double throughput = ops / (r.mean * 1e-3) / 1e6; // Mops/sec
				PrintRow(stdout, "serial", n, t, r, throughput);
				csv << "serial," << n << "," << t << ","
					<< r.mean << "," << r.median << "," << r.p95 << "," << r.p99 << ","
					<< r.stddev << "," << r.min << "," << r.max << "," << throughput << "\n";
			}

			// Simple pool
			{
				Result r = BenchSimplePool(n, t, iters);
				double ops = static_cast<double>(n) * 512.0;
				double throughput = ops / (r.mean * 1e-3) / 1e6;
				PrintRow(stdout, "simple", n, t, r, throughput);
				csv << "simple," << n << "," << t << ","
					<< r.mean << "," << r.median << "," << r.p95 << "," << r.p99 << ","
					<< r.stddev << "," << r.min << "," << r.max << "," << throughput << "\n";
			}

			// BS::thread_pool
			{
				Result r = BenchBsPool(n, t, iters);
				double ops = static_cast<double>(n) * 512.0;
				double throughput = ops / (r.mean * 1e-3) / 1e6;
				PrintRow(stdout, "bs", n, t, r, throughput);
				csv << "bs," << n << "," << t << ","
					<< r.mean << "," << r.median << "," << r.p95 << "," << r.p99 << ","
					<< r.stddev << "," << r.min << "," << r.max << "," << throughput << "\n";
			}
		}
		std::printf("\n");
	}

	csv.close();

	std::printf("=== done. results.csv written. ===\n");
	return 0;
}
