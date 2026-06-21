// Entry point for frame-flight-allocator-budget prototype.
//
// Per `docs/experiments/benchmarks/methodology.md` §3:
//   - warmup: 50 frames
//   - measured: N=1000 frames per strategy
//   - 4 strategies, same workload
//   - output: results.csv + human-readable summary
//
// Hardware baseline: dev host per `docs/experiments/hardware-profile.md` §3.
// Single-threaded harness (no cross-thread contention).

#include "harness.hpp"
#include "strategies.hpp"
#include "benchmark.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <cstdio>
#include <vector>
#include <string>

using namespace prototype;

static const uint32_t kWarmupFrames = 50;
static const uint32_t kMeasuredFrames = 1000;

struct StrategyResult {
    std::string name;
    Stats allocLatencyStats;
    size_t totalFailures = 0;
    Stats heapUsageMBStats;
    Stats heapBudgetMBStats;
    VkDeviceSize peakHeapUsageBytes = 0;
};

template <typename Strategy>
static StrategyResult runStrategy(DeviceContext& ctx, WorkloadParams p, const char* name) {
    Strategy strat(ctx);
    std::vector<double> allocLatencies;
    std::vector<double> heapUsagesMB;
    std::vector<double> heapBudgetsMB;
    std::vector<FrameSample> samples;
    samples.reserve(kWarmupFrames + kMeasuredFrames);

    for (uint32_t i = 0; i < kWarmupFrames + kMeasuredFrames; ++i) {
        FrameSample s = strat.runFrame(i, p);
        if (i >= kWarmupFrames) {
            allocLatencies.push_back(s.allocLatencyUs);
            heapUsagesMB.push_back((double)s.heapUsageBytes / (1024.0 * 1024.0));
            heapBudgetsMB.push_back((double)s.heapBudgetBytes / (1024.0 * 1024.0));
        }
        samples.push_back(s);
    }

    StrategyResult r;
    r.name = name;
    r.allocLatencyStats = computeStats(allocLatencies);
    r.heapUsageMBStats = computeStats(heapUsagesMB);
    r.heapBudgetMBStats = computeStats(heapBudgetsMB);
    for (auto& s : samples) {
        if (s.heapUsageBytes > r.peakHeapUsageBytes)
            r.peakHeapUsageBytes = s.heapUsageBytes;
        r.totalFailures += s.failedCount;
    }
    return r;
}

int main(int /*argc*/, char** /*argv*/) {
    VkResult vr = volkInitialize();
    if (vr != VK_SUCCESS) {
        fprintf(stderr, "volkInitialize failed: %d\n", (int)vr);
        return 1;
    }

    DeviceContext ctx = createDeviceContext(/*enableMemoryBudgetExt=*/true);

    // Persistent resources (created once for all strategies). On RTX 3060 Ti
    // these are tiny (~283 KiB) so they don't materially affect VRAM budget.
    VkBuffer matBuf, cluBuf;
    VmaAllocation matAlloc, cluAlloc;
    createPersistent(ctx, matBuf, matAlloc, cluBuf, cluAlloc);
    fprintf(stderr, "[main] persistent resources allocated (material=%p cluster=%p)\n",
            (void*)matBuf, (void*)cluBuf);

    WorkloadParams wp{};
    fprintf(stderr, "[main] workload: smallSsboCount=%u worldEditEveryK=%u worldEditExtra=%zu MiB\n",
            wp.smallSsboCount, wp.worldEditEveryKFrames,
            wp.worldEditExtraBytes / (1024 * 1024));

    std::vector<StrategyResult> results;
    results.push_back(runStrategy<StrategyA_Default>(ctx, wp, "A_Default"));
    results.push_back(runStrategy<StrategyB_BudgetTrack>(ctx, wp, "B_BudgetTrack"));
    results.push_back(runStrategy<StrategyC_LinearPool>(ctx, wp, "C_LinearPool"));
    results.push_back(runStrategy<StrategyD_DoubleBuffer>(ctx, wp, "D_DoubleBuffer"));
    results.push_back(runStrategy<StrategyE_PreCreatedRing>(ctx, wp, "E_PreCreatedRing"));

    // Stress pass: huge world-edit spike (256 MiB) every 50 frames.
    // Tests hard-cap behavior of WITHIN_BUDGET_BIT (Strategy B + D).
    fprintf(stderr, "\n[main] === STRESS PASS: 256 MiB world-edit spike ===\n");
    WorkloadParams stressWp{};
    stressWp.worldEditEveryKFrames = 50;
    stressWp.worldEditExtraBytes = 256ULL * 1024 * 1024;
    std::vector<StrategyResult> stress;
    stress.push_back(runStrategy<StrategyA_Default>(ctx, stressWp, "A_Default"));
    stress.push_back(runStrategy<StrategyB_BudgetTrack>(ctx, stressWp, "B_BudgetTrack"));
    stress.push_back(runStrategy<StrategyD_DoubleBuffer>(ctx, stressWp, "D_DoubleBuffer"));
    for (auto& r : stress) results.push_back(r);

    // Free persistent.
    vmaDestroyBuffer(ctx.allocator, matBuf, matAlloc);
    vmaDestroyBuffer(ctx.allocator, cluBuf, cluAlloc);

    // Output: human-readable + CSV.
    printf("\n=== Frame-Flight Allocator Budget — Results ===\n");
    printf("Workload: %u small SSBOs + 1 MiB + 4 MiB + 4 MiB image per frame; "
           "8 MiB world-edit spike every %u frames\n",
           wp.smallSsboCount, wp.worldEditEveryKFrames);
    printf("Warmup=%u Measured=%u frames per strategy\n\n", kWarmupFrames, kMeasuredFrames);

    for (auto& r : results) {
        printf("[%s]\n", r.name.c_str());
        printStatsRow("frameAllocLatency", r.allocLatencyStats);
        printStatsRow("heapUsageMiB",      r.heapUsageMBStats);
        printStatsRow("heapBudgetMiB",     r.heapBudgetMBStats);
        printf("  peakHeapUsage        %.2f MiB\n",
               (double)r.peakHeapUsageBytes / (1024.0 * 1024.0));
        printf("  totalFailures        %zu\n\n", r.totalFailures);
    }

    // CSV output for offline analysis.
    FILE* csv = fopen("results.csv", "w");
    if (csv) {
        fprintf(csv, "strategy,metric,mean,median,p95,p99,stddev,min,max,totalFailures,peakHeapUsageMiB\n");
        for (auto& r : results) {
            auto row = [&](const char* metric, const Stats& s) {
                fprintf(csv, "%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%zu,%.4f\n",
                        r.name.c_str(), metric,
                        s.mean, s.median, s.p95, s.p99, s.stddev, s.minv, s.maxv,
                        r.totalFailures,
                        (double)r.peakHeapUsageBytes / (1024.0 * 1024.0));
            };
            row("frameAllocLatencyUs", r.allocLatencyStats);
            row("heapUsageMiB", r.heapUsageMBStats);
            row("heapBudgetMiB", r.heapBudgetMBStats);
        }
        fclose(csv);
        fprintf(stderr, "[main] CSV written to results.csv\n");
    }

    destroyDeviceContext(ctx);
    return 0;
}
