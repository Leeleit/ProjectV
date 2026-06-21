#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#if defined(PROFILE_TRACY)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#pragma clang diagnostic pop
#endif

// ---------------------------------------------------------------------------
// Stats (per benchmarks/methodology.md §7)
// ---------------------------------------------------------------------------

struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double stddev;
    double min_v;
    double max_v;
};

Stats Compute(const std::vector<double>& samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / static_cast<double>(samples.size());
    s.median = sorted[sorted.size() / 2];
    s.p95 = sorted[static_cast<size_t>(sorted.size() * 0.95)];
    s.p99 = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.min_v = sorted.front();
    s.max_v = sorted.back();
    return s;
}

// ---------------------------------------------------------------------------
// Vulkan helpers
// ---------------------------------------------------------------------------

#define VK_CHECK(expr)                                                        \
    do {                                                                      \
        VkResult _r = (expr);                                                 \
        if (_r != VK_SUCCESS) {                                               \
            std::fprintf(stderr, "VK error %d at %s:%d (%s)\n",               \
                         static_cast<int>(_r), __FILE__, __LINE__, #expr);    \
            std::abort();                                                     \
        }                                                                     \
    } while (false)

struct VkContext {
    VkInstance instance{};
    VkPhysicalDevice phys{};
    VkDevice device{};
    VkQueue graphicsQueue{};
    VkQueue computeQueue{};
    uint32_t graphicsFamily{};
    uint32_t computeFamily{};
    VkCommandPool cmdPool{};
    VkCommandBuffer cmd{};
    VkPhysicalDeviceProperties props{};

    // Memory-bound synthetic workload (1 MiB SSBO).
    VkBuffer ssbo{};
    VkDeviceMemory ssboMem{};
    void* ssboMapped{};

    // Manual timestamp query pool (config C only).
    VkQueryPool tsPool{};
    static constexpr uint32_t kTsCount = 2;  // frame TOP + BOTTOM

    // Per-pass query pool (config B Tracy GPU only — Tracy owns its pool).
    static constexpr uint32_t kPassQueryCount = 32;

#if defined(PROFILE_TRACY)
    tracy::VkCtx* tracyCtx{nullptr};
#endif

    uint64_t frameIndex = 0;
};

void InitVulkan(VkContext& ctx) {
    VK_CHECK(volkInitialize());

    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "tracy-gpu-vs-manual";
    app.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo ic{};
    ic.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ic.pApplicationInfo = &app;
    VK_CHECK(vkCreateInstance(&ic, nullptr, &ctx.instance));

    volkLoadInstance(ctx.instance);

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(ctx.instance, &devCount, devs.data());
    ctx.phys = devs[0];
    vkGetPhysicalDeviceProperties(ctx.phys, &ctx.props);

    // Pick graphics + compute families.
    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.phys, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.phys, &qfCount, qf.data());

    ctx.graphicsFamily = UINT32_MAX;
    ctx.computeFamily = UINT32_MAX;
    for (uint32_t i = 0; i < qfCount; ++i) {
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && ctx.graphicsFamily == UINT32_MAX)
            ctx.graphicsFamily = i;
        if ((qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            ctx.computeFamily == UINT32_MAX)
            ctx.computeFamily = i;
    }
    if (ctx.computeFamily == UINT32_MAX) ctx.computeFamily = ctx.graphicsFamily;

    float prio = 1.0f;
    VkDeviceQueueCreateInfo qcis[2]{};
    qcis[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qcis[0].queueFamilyIndex = ctx.graphicsFamily;
    qcis[0].queueCount = 1;
    qcis[0].pQueuePriorities = &prio;
    qcis[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qcis[1].queueFamilyIndex = ctx.computeFamily;
    qcis[1].queueCount = 1;
    qcis[1].pQueuePriorities = &prio;

    VkPhysicalDeviceFeatures feats{};
    vkGetPhysicalDeviceFeatures(ctx.phys, &feats);

    VkDeviceCreateInfo dc{};
    dc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dc.queueCreateInfoCount = (ctx.computeFamily != ctx.graphicsFamily) ? 2u : 1u;
    dc.pQueueCreateInfos = qcis;
    dc.pEnabledFeatures = &feats;
    VK_CHECK(vkCreateDevice(ctx.phys, &dc, nullptr, &ctx.device));
    volkLoadDevice(ctx.device);

    vkGetDeviceQueue(ctx.device, ctx.graphicsFamily, 0, &ctx.graphicsQueue);
    if (ctx.computeFamily != ctx.graphicsFamily)
        vkGetDeviceQueue(ctx.device, ctx.computeFamily, 0, &ctx.computeQueue);

    VkCommandPoolCreateInfo pc{};
    pc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pc.queueFamilyIndex = ctx.graphicsFamily;
    pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(ctx.device, &pc, nullptr, &ctx.cmdPool));

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = ctx.cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(ctx.device, &cbai, &ctx.cmd));

    // 1 MiB SSBO for synthetic compute work.
    VkBufferCreateInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = 1024 * 1024;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(ctx.device, &bi, nullptr, &ctx.ssbo));

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(ctx.device, ctx.ssbo, &memReq);

    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(ctx.phys, &mp);
    uint32_t memType = UINT32_MAX;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
            (mp.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            memType = i;
            break;
        }
    }
    if (memType == UINT32_MAX) {
        std::fprintf(stderr, "No host-visible memory type found\n");
        std::abort();
    }

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = memType;
    VK_CHECK(vkAllocateMemory(ctx.device, &mai, nullptr, &ctx.ssboMem));
    VK_CHECK(vkBindBufferMemory(ctx.device, ctx.ssbo, ctx.ssboMem, 0));
    VK_CHECK(vkMapMemory(ctx.device, ctx.ssboMem, 0, memReq.size, 0, &ctx.ssboMapped));

    // Timestamp query pool (config C — manual aggregate).
    VkQueryPoolCreateInfo qp{};
    qp.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qp.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qp.queryCount = VkContext::kTsCount;
    VK_CHECK(vkCreateQueryPool(ctx.device, &qp, nullptr, &ctx.tsPool));

#if defined(PROFILE_TRACY)
    // Calibrated Tracy context via symbol table (TRACY_VK_USE_SYMBOL_TABLE=ON):
    // Tracy resolves ALL Vulkan symbols (including KHR-promoted calibrated
    // timestamps) through the instance/device procaddrs we pass. Without
    // symbol table, Tracy would call non-existent global
    // `vkGetPhysicalDeviceCalibrateableTimeDomainsEXT` (not in Vulkan 1.4
    // core, only KHR-suffixed version exists).
    PFN_vkGetInstanceProcAddr instProc = vkGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr devProc = vkGetDeviceProcAddr;

    // 8-arg calibrated symbol-table overload.
    ctx.tracyCtx = tracy::CreateVkContext(
        ctx.instance, ctx.phys, ctx.device, ctx.graphicsQueue, ctx.cmd,
        instProc, devProc, /*calibrated=*/true);
    if (!ctx.tracyCtx) {
        std::fprintf(stderr, "Tracy context creation returned null\n");
        std::abort();
    }
#endif
}

// ---------------------------------------------------------------------------
// Synthetic compute pass: 1 MiB SSBO clear (memory-bandwidth-bound, ~5 µs)
// ---------------------------------------------------------------------------

void SyntheticPass(VkContext& ctx, uint32_t passIndex, const std::string& label,
                   bool useTracyZone, bool useManualTimestamp) {
#if defined(PROFILE_TRACY)
    if (useTracyZone) {
        // Use TracyVkZoneTransient for dynamic (runtime) string names — the
        // non-transient TracyVkZone requires a constexpr string literal.
        TracyVkZoneTransient(ctx.tracyCtx, ___tracy_pass_zone, ctx.cmd,
                             label.c_str(), true);
    }
#else
    (void)useTracyZone;
#endif

    // 1 dispatch x 64 workgroups x 256 threads = 16384 threads.
    // Each writes one float — minimal ALU.
    vkCmdFillBuffer(ctx.cmd, ctx.ssbo, 0, 1024 * 1024, 0xDEADBEEFu);

    if (useManualTimestamp) {
        vkCmdWriteTimestamp(ctx.cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            ctx.tsPool, passIndex % VkContext::kTsCount);
    }
}

void SubmitAndWait(VkContext& ctx) {
    VK_CHECK(vkEndCommandBuffer(ctx.cmd));

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &ctx.cmd;
    VK_CHECK(vkQueueSubmit(ctx.graphicsQueue, 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(ctx.graphicsQueue));

    // Re-enter recording state BEFORE TracyVkCollect, because Tracy may record
    // vkCmdResetQueryPool into the command buffer.
    VK_CHECK(vkResetCommandBuffer(ctx.cmd, 0));
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(ctx.cmd, &bi));

#if defined(PROFILE_TRACY)
    if (ctx.tracyCtx) {
        // Issue TracyVkCollect at end of frame after new recording started.
        TracyVkCollect(ctx.tracyCtx, ctx.cmd);
    }
#endif
}

// ---------------------------------------------------------------------------
// Configs (per §3 Method)
// ---------------------------------------------------------------------------

enum class Config { A, B, C, D };

Config ParseConfig(const char* s) {
    if (std::strcmp(s, "A") == 0) return Config::A;
    if (std::strcmp(s, "B") == 0) return Config::B;
    if (std::strcmp(s, "C") == 0) return Config::C;
    if (std::strcmp(s, "D") == 0) return Config::D;
    std::fprintf(stderr, "Unknown config %s (use A/B/C/D)\n", s);
    std::abort();
}

void RunFrame(VkContext& ctx, Config cfg, uint32_t nPasses) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(ctx.cmd, &bi));

    bool useManual = false;
#if defined(PROFILE_TRACY)
    bool useTracy = false;
#endif

    switch (cfg) {
        case Config::A:
            useManual = false;
#if defined(PROFILE_TRACY)
            useTracy = false;
#endif
            break;
        case Config::B:
            useManual = false;
#if defined(PROFILE_TRACY)
            useTracy = true;
#endif
            break;
        case Config::C:
            useManual = true;
#if defined(PROFILE_TRACY)
            useTracy = false;
#endif
            break;
        case Config::D:
            useManual = true;
#if defined(PROFILE_TRACY)
            // Top-3 hot-path passes use Tracy; rest manual.
            useTracy = false;
#endif
            break;
    }

    for (uint32_t i = 0; i < nPasses; ++i) {
        char label[64];
        std::snprintf(label, sizeof(label), "Pass_%u", i);

        bool thisPassTracy = false;
#if defined(PROFILE_TRACY)
        if (cfg == Config::D) {
            // Top-3 passes: voxel.mesh (i=0), voxel.frag (i=1), hzb_cull (i=2).
            thisPassTracy = (i < 3);
        } else {
            thisPassTracy = useTracy;
        }
#endif

        SyntheticPass(ctx, i, label, thisPassTracy, useManual);
    }

    SubmitAndWait(ctx);
    ++ctx.frameIndex;
}

void WriteCsv(const std::string& path, const std::vector<std::vector<double>>& frames,
              const std::string& label) {
    std::ofstream out(path, std::ios::trunc);
    out << "config,passes,frame_index,wall_ms\n";
    for (size_t f = 0; f < frames.size(); ++f) {
        for (size_t s = 0; s < frames[f].size(); ++s) {
            out << label << "," << f << "," << s << "," << frames[f][s] << "\n";
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 5) {
        std::fprintf(stderr,
                     "Usage: %s --config=A|B|C|D --passes=N [--warmup=60] [--frames=1000] [--drift-test] [--out=results.csv]\n"
                     "  --drift-test    Run 10K frames with per-1K-window mean for Issue #663 drift detection.\n",
                     argv[0]);
        return 1;
    }

    Config cfg = Config::A;
    uint32_t nPasses = 3;
    uint32_t warmup = 60;
    uint32_t nFrames = 1000;
    std::string outPath = "results.csv";
    bool driftTest = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--config=", 0) == 0) cfg = ParseConfig(a.substr(9).c_str());
        else if (a.rfind("--passes=", 0) == 0) nPasses = std::stoul(a.substr(9));
        else if (a.rfind("--warmup=", 0) == 0) warmup = std::stoul(a.substr(9));
        else if (a.rfind("--frames=", 0) == 0) nFrames = std::stoul(a.substr(9));
        else if (a.rfind("--out=", 0) == 0) outPath = a.substr(6);
        else if (a == "--drift-test") {
            driftTest = true;
            nFrames = 10000;
            warmup = 60;
        }
    }

    std::printf("Config=%c Passes=%u Warmup=%u Frames=%u%s Out=%s\n",
                static_cast<char>('A' + static_cast<int>(cfg)), nPasses, warmup, nFrames,
                driftTest ? " (DRIFT TEST)" : "", outPath.c_str());

    VkContext ctx;
    InitVulkan(ctx);

    std::printf("GPU: %s (Vulkan %u.%u.%u)\n", ctx.props.deviceName,
                VK_VERSION_MAJOR(ctx.props.apiVersion),
                VK_VERSION_MINOR(ctx.props.apiVersion),
                VK_VERSION_PATCH(ctx.props.apiVersion));
    std::printf("timestampPeriod = %f ns/tick\n", ctx.props.limits.timestampPeriod);

    // Warmup.
    for (uint32_t i = 0; i < warmup; ++i) RunFrame(ctx, cfg, nPasses);

    // Measure.
    std::vector<double> samples;
    samples.reserve(nFrames);
    for (uint32_t i = 0; i < nFrames; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        RunFrame(ctx, cfg, nPasses);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        samples.push_back(ms);
    }

    Stats s = Compute(samples);
    std::printf("\n[Config %c / %u passes / %u frames]\n",
                static_cast<char>('A' + static_cast<int>(cfg)), nPasses, nFrames);
    std::printf("  mean=%.3f ms  median=%.3f  p95=%.3f  p99=%.3f  std=%.3f  min=%.3f  max=%.3f\n",
                s.mean, s.median, s.p95, s.p99, s.stddev, s.min_v, s.max_v);

    WriteCsv(outPath, {samples},
             std::string(1, static_cast<char>('A' + static_cast<int>(cfg))) +
                 "_p" + std::to_string(nPasses));

    // Drift test (Issue #663 verification): per-1K-window mean to detect calibration
    // drift over time. Config B (Tracy GPU all) and high passes (15) are the
    // critical case.
    if (driftTest) {
        std::string driftPath = outPath;
        auto pos = driftPath.find_last_of('.');
        if (pos != std::string::npos) driftPath = driftPath.substr(0, pos) + "_drift.csv";
        else driftPath += "_drift.csv";

        std::ofstream dout(driftPath, std::ios::trunc);
        dout << "config,passes,window_index,window_start_frame,window_end_frame,window_mean_ms,window_max_ms\n";
        constexpr uint32_t kWindow = 1000;
        uint32_t nWindows = nFrames / kWindow;
        for (uint32_t w = 0; w < nWindows; ++w) {
            double sum = 0.0;
            double mx = 0.0;
            for (uint32_t i = w * kWindow; i < (w + 1) * kWindow; ++i) {
                sum += samples[i];
                if (samples[i] > mx) mx = samples[i];
            }
            double mean = sum / static_cast<double>(kWindow);
            dout << static_cast<char>('A' + static_cast<int>(cfg)) << "," << nPasses << ","
                 << w << "," << (w * kWindow) << "," << ((w + 1) * kWindow - 1) << ","
                 << mean << "," << mx << "\n";

            if (w == 0 || w == nWindows / 2 || w == nWindows - 1) {
                std::printf("  drift window %2u (frames %5u-%5u): mean=%.3f ms max=%.3f ms\n",
                            w, w * kWindow, (w + 1) * kWindow - 1, mean, mx);
            }
        }
        std::printf("Drift data written: %s\n", driftPath.c_str());

        // Drift verdict.
        double first = 0.0, last = 0.0;
        for (uint32_t i = 0; i < kWindow; ++i) first += samples[i];
        first /= static_cast<double>(kWindow);
        for (uint32_t i = (nWindows - 1) * kWindow; i < nWindows * kWindow; ++i)
            last += samples[i];
        last /= static_cast<double>(kWindow);
        double driftPct = (last - first) / first * 100.0;
        std::printf("Drift verdict: first-1K mean=%.3f ms, last-1K mean=%.3f ms, drift=%+.1f%%\n",
                    first, last, driftPct);
        std::printf("Issue #663 alert threshold: +20%% drift → MEASURED %+.1f%% (%s)\n",
                    driftPct, std::abs(driftPct) > 20.0 ? "ALERT" : "OK");
    }

#if defined(PROFILE_TRACY)
    if (ctx.tracyCtx) tracy::DestroyVkContext(ctx.tracyCtx);
#endif
    vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1, &ctx.cmd);
    vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
    vkDestroyQueryPool(ctx.device, ctx.tsPool, nullptr);
    vkUnmapMemory(ctx.device, ctx.ssboMem);
    vkFreeMemory(ctx.device, ctx.ssboMem, nullptr);
    vkDestroyBuffer(ctx.device, ctx.ssbo, nullptr);
    vkDestroyDevice(ctx.device, nullptr);
    vkDestroyInstance(ctx.instance, nullptr);

    return 0;
}
