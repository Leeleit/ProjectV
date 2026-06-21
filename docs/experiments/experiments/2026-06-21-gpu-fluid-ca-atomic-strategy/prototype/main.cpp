// gpu-fluid-ca-atomic-strategy prototype main
// Stage 3.1 GPU Fluid CA atomic strategy benchmark
// Measures 6 strategies (A, B, C-2stage, D, E, F) × 5 scene configs × 3 seeds × N=1000
// Per benchmarks/methodology.md §3
// Build: see CMakeLists.txt + README.md

#define VMA_IMPLEMENTATION
#define VMA_VULKAN_VERSION 1004000
// VMA_STATIC_VULKAN_FUNCTIONS=1: VMA uses vulkan.h prototypes directly (linked from libvulkan.so).
// This is the simplest mode — works without VmaVulkanFunctions struct.
// Compatible with regular vulkan.h prototypes (no VK_NO_PROTOTYPES).
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#include "harness.hpp"   // includes vulkan.h + vk_mem_alloc.h
#include "scenes.hpp"

#include <vector>
#include <span>
#include <string>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <stdexcept>

using harness::VulkanContext;
using harness::Stats;
using harness::ComputeStats;
using harness::Buffer;
using harness::QueryPool;
using scenes::SceneType;
using scenes::Cell;
using scenes::GenerateScene;
using scenes::CountFluid;
using scenes::SceneName;
using scenes::SceneDimensions;

constexpr uint32_t kStrategyCount = 6;   // A, B, C, D, E, F
constexpr uint32_t kSceneCount = 5;
constexpr uint32_t kSeedCount = 3;
constexpr uint32_t kDefaultFrames = 1000;
constexpr uint32_t kDefaultWarmup = 30;
constexpr uint32_t kWorkgroupSize = 8u * 8u * 4u; // 256 (matches 8×8×4 workgroup in GLSL)
constexpr uint32_t kCheckerboardMasks = 8u;

// Strategy identifiers
enum StrategyId : uint32_t {
    S_ATOMIC_OR_BLIND = 0,
    S_CAS = 1,
    S_SHARED_MEM_2STAGE = 2,
    S_SUBGROUP_BALLOT = 3,
    S_HIER_LOCK = 4,
    S_CHECKERBOARD = 5
};

const char* StrategyName(StrategyId s) {
    switch (s) {
        case S_ATOMIC_OR_BLIND: return "A_AtomicOr_Blind";
        case S_CAS: return "B_CAS";
        case S_SHARED_MEM_2STAGE: return "C_SharedMem_2Stage";
        case S_SUBGROUP_BALLOT: return "D_SubgroupBallot";
        case S_HIER_LOCK: return "E_HierLock";
        case S_CHECKERBOARD: return "F_Checkerboard";
    }
    return "unknown";
}

const char* StrategySpvFile(StrategyId s, uint32_t stage = 0) {
    static char buf[64];
    if (s == S_SHARED_MEM_2STAGE) {
        return stage == 0 ? "C_collect.spv" : "C_writeback.spv";
    }
    snprintf(buf, sizeof(buf), "S%u.spv", s);
    return buf;
}

struct ConfigResult {
    std::string strategy;
    std::string scene;
    uint32_t seed;
    Stats stats;
    uint32_t conservation_violations;
    uint32_t fluid_before;
    uint32_t fluid_after;
    uint32_t atomic_ops_count;
};

void PrintUsage(const char* prog) {
    std::printf("Usage: %s [--scene=NAME] [--strategy=ID] [--frames=N] [--warmup=N] [--csv=path]\n", prog);
    std::printf("\nScenes: empty, sparse, vertical_column, water_tower, lava_pool\n");
    std::printf("Strategies: 0=A_AtomicOr_Blind, 1=B_CAS, 2=C_SharedMem_2Stage,\n"
                "             3=D_SubgroupBallot, 4=E_HierLock, 5=F_Checkerboard\n");
    std::printf("Special: --scene=all --strategy=all — run full matrix (90 configs × N frames)\n");
}

// Load SPIR-V file
std::vector<uint32_t> LoadSPV(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Failed to open " + path);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0);
    std::vector<uint32_t> code(sz / 4);
    f.read(reinterpret_cast<char*>(code.data()), sz);
    return code;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<uint32_t>& spv) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spv.size() * 4;
    info.pCode = spv.data();
    VkShaderModule m = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &info, nullptr, &m) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateShaderModule failed");
    }
    return m;
}

// Per-config buffers
struct ConfigBuffers {
    Buffer source;          // read-only input cells
    Buffer destination;     // output cells (ping-pong)
    Buffer stats;           // 16 bytes (4 × uint32_t)
    Buffer locks;           // 1 uint per chunk (Strategy E)
    Buffer claim_slots;     // Strategy D
    Buffer wg_counters;     // Strategy C: 1 uint per workgroup
    Buffer claim_destinations;  // Strategy C: per-claim destination
    Buffer claim_source_indices; // Strategy C: per-claim source
};

ConfigBuffers AllocateBuffers(VulkanContext& ctx, uint32_t total_cells, uint32_t total_workgroups) {
    ConfigBuffers b;
    b.source = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
    b.destination = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
    b.stats = ctx.CreateStorageBuffer(16);
    b.locks = ctx.CreateStorageBuffer(4);
    b.claim_slots = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(uint32_t)));
    b.wg_counters = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_workgroups * sizeof(uint32_t)));
    b.claim_destinations = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_workgroups * kWorkgroupSize * sizeof(uint32_t)));
    b.claim_source_indices = ctx.CreateStorageBuffer(static_cast<VkDeviceSize>(total_workgroups * kWorkgroupSize * sizeof(uint32_t)));
    return b;
}

// Build descriptor set for a strategy (returns bindings count)
struct DescriptorSet {
    VkDescriptorSetLayout layout{VK_NULL_HANDLE};
    VkDescriptorPool pool{VK_NULL_HANDLE};
    VkDescriptorSet set{VK_NULL_HANDLE};
};

DescriptorSet CreateDescriptorSet(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
    DescriptorSet ds;
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
    layout_info.pBindings = bindings.data();
    vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &ds.layout);

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount = static_cast<uint32_t>(bindings.size());
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = 1;
    vkCreateDescriptorPool(device, &pool_info, nullptr, &ds.pool);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = ds.pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &ds.layout;
    vkAllocateDescriptorSets(device, &alloc_info, &ds.set);
    return ds;
}

void DestroyDescriptorSet(VkDevice device, DescriptorSet& ds) {
    vkDestroyDescriptorPool(device, ds.pool, nullptr);
    vkDestroyDescriptorSetLayout(device, ds.layout, nullptr);
}

VkPipelineLayout CreatePipelineLayout(VkDevice device, VkDescriptorSetLayout dsl, uint32_t push_size) {
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = push_size;
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = 1;
    info.pSetLayouts = &dsl;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &range;
    VkPipelineLayout pl = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &info, nullptr, &pl);
    return pl;
}

VkPipeline CreateComputePipeline(VkDevice device, VkShaderModule sm, VkPipelineLayout pl, const char* entry = "main") {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = sm;
    stage.pName = entry;
    VkComputePipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage = stage;
    info.layout = pl;
    VkPipeline p = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &p) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateComputePipelines failed");
    }
    return p;
}

int main(int argc, char** argv) {
    // Parse args
    std::string scene_filter = "all";
    std::string strategy_filter = "all";
    uint32_t frames = kDefaultFrames;
    uint32_t warmup = kDefaultWarmup;
    std::string csv_path = "results.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--scene=") == 0) scene_filter = arg.substr(8);
        else if (arg.find("--strategy=") == 0) strategy_filter = arg.substr(11);
        else if (arg.find("--frames=") == 0) frames = std::stoul(arg.substr(9));
        else if (arg.find("--warmup=") == 0) warmup = std::stoul(arg.substr(9));
        else if (arg.find("--csv=") == 0) csv_path = arg.substr(6);
        else { PrintUsage(argv[0]); return 1; }
    }

    // Init Vulkan
    VulkanContext ctx;
    try { ctx.Init(); }
    catch (const std::exception& e) { std::fprintf(stderr, "Vulkan init failed: %s\n", e.what()); return 1; }
    ctx.PrintDeviceInfo();

    // Load all 7 SPIR-V files
    std::vector<VkShaderModule> shader_modules(kStrategyCount + 1);  // index 0..5 single, 6 = C writeback
    try {
        for (uint32_t s = 0; s < kStrategyCount; ++s) {
            if (s == S_SHARED_MEM_2STAGE) continue;  // C handled separately
            const std::string spv_path = std::string(StrategySpvFile(static_cast<StrategyId>(s)));
            auto spv = LoadSPV(spv_path);
            shader_modules[s] = CreateShaderModule(ctx.device_, spv);
            std::printf("Loaded shader S%u → %s\n", s, spv_path.c_str());
        }
        // Strategy C: 2 shader modules
        {
            auto spv_collect = LoadSPV("C_collect.spv");
            shader_modules[S_SHARED_MEM_2STAGE] = CreateShaderModule(ctx.device_, spv_collect);
            std::printf("Loaded shader C_collect.spv\n");
        }
        {
            auto spv_writeback = LoadSPV("C_writeback.spv");
            shader_modules[kStrategyCount] = CreateShaderModule(ctx.device_, spv_writeback);
            std::printf("Loaded shader C_writeback.spv\n");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Shader load failed: %s\n", e.what());
        return 1;
    }

    // Build output CSV
    std::ofstream csv(csv_path);
    csv << "strategy,scene,seed,frames,warmup,mean_us,median_us,p95_us,p99_us,stddev_us,min_us,max_us,"
        << "conservation_violations,fluid_before,fluid_after,atomic_ops_count\n";

    // Per-strategy descriptor set layouts + pipeline layouts (cached for reuse)
    // All strategies share: bindings 0 (source), 1 (destination), 2 (stats), 3 (locks), 4 (claim_slots)
    // Strategy C additionally uses: 5 (wg_counters), 6 (claim_destinations), 7 (claim_source_indices)
    std::vector<VkDescriptorSetLayoutBinding> base_bindings(5);
    for (uint32_t b = 0; b < 5; ++b) {
        base_bindings[b].binding = b;
        base_bindings[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        base_bindings[b].descriptorCount = 1;
        base_bindings[b].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    std::vector<VkDescriptorSetLayoutBinding> c_bindings(8);
    for (uint32_t b = 0; b < 8; ++b) {
        c_bindings[b].binding = b;
        c_bindings[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        c_bindings[b].descriptorCount = 1;
        c_bindings[b].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    // Per-config query pool (2 timestamps: start + end)
    QueryPool query_pool(ctx.device_, 2);

    // Iterate strategies
    for (uint32_t s = 0; s < kStrategyCount; ++s) {
        if (strategy_filter != "all" && std::to_string(s) != strategy_filter) continue;
        const StrategyId sid = static_cast<StrategyId>(s);

        // Setup pipeline(s) for this strategy
        const std::vector<VkDescriptorSetLayoutBinding>& bindings = (sid == S_SHARED_MEM_2STAGE) ? c_bindings : base_bindings;
        DescriptorSet ds = CreateDescriptorSet(ctx.device_, bindings);
        VkPipelineLayout pl = CreatePipelineLayout(ctx.device_, ds.layout, 32);

        std::vector<VkPipeline> pipelines;
        if (sid == S_SHARED_MEM_2STAGE) {
            // 2 pipelines: collect + writeback
            pipelines.push_back(CreateComputePipeline(ctx.device_, shader_modules[sid], pl, "main"));
            pipelines.push_back(CreateComputePipeline(ctx.device_, shader_modules[kStrategyCount], pl, "main"));
        } else {
            pipelines.push_back(CreateComputePipeline(ctx.device_, shader_modules[sid], pl, "main"));
        }
        const uint32_t pipeline_count = static_cast<uint32_t>(pipelines.size());

        // Iterate scenes
        for (uint32_t sc_idx = 0; sc_idx < kSceneCount; ++sc_idx) {
            SceneType scene_type = static_cast<SceneType>(sc_idx);
            if (scene_filter != "all" && scene_filter != SceneName(scene_type)) continue;

            for (uint32_t seed = 0; seed < kSeedCount; ++seed) {
                if (scene_type == SceneType::Empty && seed > 0) continue;

                // Generate scene
                std::vector<Cell> initial = GenerateScene(scene_type, seed);
                const scenes::SceneDims dims = SceneDimensions(scene_type);
                const uint32_t total_cells = static_cast<uint32_t>(initial.size());
                const uint32_t total_workgroups = (total_cells + kWorkgroupSize - 1) / kWorkgroupSize;
                const uint32_t fluid_before = CountFluid(initial);

                // Allocate + upload buffers
                ConfigBuffers bufs = AllocateBuffers(ctx, total_cells, total_workgroups);
                // Upload initial source
                {
                    Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
                    void* m = staging.Map();
                    std::memcpy(m, initial.data(), total_cells * sizeof(Cell));
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.source.handle(), staging.size());
                }
                // Zero initialize destination + stats + locks + claim buffers
                std::vector<Cell> zero_cells(total_cells);
                for (auto& c : zero_cells) c = Cell{0, 0, 0, 0};
                {
                    Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
                    void* m = staging.Map();
                    std::memcpy(m, zero_cells.data(), total_cells * sizeof(Cell));
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.destination.handle(), staging.size());
                }
                uint32_t zero_4[4] = {0, 0, 0, 0};
                {
                    Buffer staging = ctx.CreateStagingBuffer(16);
                    void* m = staging.Map();
                    std::memcpy(m, zero_4, 16);
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.stats.handle(), 16);
                }
                {
                    Buffer staging = ctx.CreateStagingBuffer(4);
                    void* m = staging.Map();
                    uint32_t z = 0;
                    std::memcpy(m, &z, 4);
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.locks.handle(), 4);
                }
                // Zero claim destinations + source indices
                std::vector<uint32_t> zero_claims(total_workgroups * kWorkgroupSize);
                {
                    Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(zero_claims.size() * sizeof(uint32_t)));
                    void* m = staging.Map();
                    std::memcpy(m, zero_claims.data(), zero_claims.size() * sizeof(uint32_t));
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.claim_destinations.handle(), staging.size());
                }
                {
                    Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(zero_claims.size() * sizeof(uint32_t)));
                    void* m = staging.Map();
                    std::memcpy(m, zero_claims.data(), zero_claims.size() * sizeof(uint32_t));
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.claim_source_indices.handle(), staging.size());
                }
                {
                    Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(total_workgroups * sizeof(uint32_t)));
                    void* m = staging.Map();
                    std::memset(m, 0, total_workgroups * sizeof(uint32_t));
                    staging.Unmap();
                    ctx.CopyBuffer(staging.handle(), bufs.wg_counters.handle(), staging.size());
                }

                // Write descriptor set
                std::vector<VkWriteDescriptorSet> writes(bindings.size());
                std::vector<VkDescriptorBufferInfo> infos(bindings.size());
                std::array<VkBuffer, 8> buf_handles = {
                    bufs.source.handle(), bufs.destination.handle(), bufs.stats.handle(),
                    bufs.locks.handle(), bufs.claim_slots.handle(), bufs.wg_counters.handle(),
                    bufs.claim_destinations.handle(), bufs.claim_source_indices.handle()
                };
                for (uint32_t b = 0; b < bindings.size(); ++b) {
                    infos[b].buffer = buf_handles[b];
                    infos[b].offset = 0;
                    infos[b].range = VK_WHOLE_SIZE;
                    writes[b].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    writes[b].dstSet = ds.set;
                    writes[b].dstBinding = b;
                    writes[b].dstArrayElement = 0;
                    writes[b].descriptorCount = 1;
                    writes[b].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    writes[b].pBufferInfo = &infos[b];
                }
                vkUpdateDescriptorSets(ctx.device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

                // Allocate command buffer
                VkCommandBuffer cmd = ctx.AllocateOneShotCommandBuffer();

                // Push constants
                struct PushData {
                    uint32_t grid_x, grid_y, grid_z, pad;
                    uint32_t cell_count, flags;
                    uint32_t mask_x, mask_y, mask_z, mask_w;
                    float tick_interval;
                    uint32_t rsv0, rsv1, rsv2;
                };
                const PushData push_template{dims.width, dims.height, dims.depth, 0,
                                              total_cells, 0,
                                              0, 8, 0, 0,
                                              0.05f, 0, 0, 0};

                // Number of dispatches per tick
                uint32_t dispatches_per_tick = 1;
                if (sid == S_CHECKERBOARD) dispatches_per_tick = kCheckerboardMasks;

                // Bench loop
                std::vector<double> tick_times;
                tick_times.reserve(frames);

                for (uint32_t i = 0; i < warmup + frames; ++i) {
                    // Reset destination + stats + locks + claim buffers per tick
                    // (simulates ping-pong state from previous tick)
                    {
                        Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
                        void* m = staging.Map();
                        std::memcpy(m, zero_cells.data(), total_cells * sizeof(Cell));
                        staging.Unmap();
                        ctx.CopyBuffer(staging.handle(), bufs.destination.handle(), staging.size());
                    }
                    {
                        Buffer staging = ctx.CreateStagingBuffer(16);
                        void* m = staging.Map();
                        std::memcpy(m, zero_4, 16);
                        staging.Unmap();
                        ctx.CopyBuffer(staging.handle(), bufs.stats.handle(), 16);
                    }
                    {
                        Buffer staging = ctx.CreateStagingBuffer(4);
                        void* m = staging.Map();
                        uint32_t z = 0;
                        std::memcpy(m, &z, 4);
                        staging.Unmap();
                        ctx.CopyBuffer(staging.handle(), bufs.locks.handle(), 4);
                    }
                    if (sid == S_SHARED_MEM_2STAGE) {
                        // Reset wg_counters + claim_destinations + claim_source_indices
                        Buffer staging = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(zero_claims.size() * sizeof(uint32_t)));
                        void* m = staging.Map();
                        std::memcpy(m, zero_claims.data(), zero_claims.size() * sizeof(uint32_t));
                        staging.Unmap();
                        ctx.CopyBuffer(staging.handle(), bufs.claim_destinations.handle(), staging.size());
                        Buffer staging2 = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(zero_claims.size() * sizeof(uint32_t)));
                        void* m2 = staging2.Map();
                        std::memcpy(m2, zero_claims.data(), zero_claims.size() * sizeof(uint32_t));
                        staging2.Unmap();
                        ctx.CopyBuffer(staging2.handle(), bufs.claim_source_indices.handle(), staging2.size());
                        Buffer staging3 = ctx.CreateStagingBuffer(static_cast<VkDeviceSize>(total_workgroups * sizeof(uint32_t)));
                        void* m3 = staging3.Map();
                        std::memset(m3, 0, total_workgroups * sizeof(uint32_t));
                        staging3.Unmap();
                        ctx.CopyBuffer(staging3.handle(), bufs.wg_counters.handle(), staging3.size());
                    }

                    // Record command buffer
                    VkCommandBufferBeginInfo begin_info{};
                    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    vkBeginCommandBuffer(cmd, &begin_info);
                    vkCmdResetQueryPool(cmd, query_pool.handle(), 0, 2);
                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, query_pool.handle(), 0);

                    // Dispatch loop
                    for (uint32_t d = 0; d < dispatches_per_tick; ++d) {
                        PushData push_data = push_template;
                        if (sid == S_CHECKERBOARD) {
                            push_data.mask_x = d;  // current mask 0..7
                        }
                        VkPipeline pipeline;
                        if (sid == S_SHARED_MEM_2STAGE) {
                            pipeline = (d == 0) ? pipelines[0] : pipelines[1];
                        } else {
                            pipeline = pipelines[0];
                        }
                        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &ds.set, 0, nullptr);
                        vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushData), &push_data);
                        vkCmdDispatch(cmd, total_workgroups, 1, 1);

                        // Inter-stage barrier for Strategy C between collect + writeback
                        if (sid == S_SHARED_MEM_2STAGE && d == 0) {
                            VkMemoryBarrier barrier{};
                            barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                            vkCmdPipelineBarrier(cmd,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                0, 1, &barrier, 0, nullptr, 0, nullptr);
                        }
                    }

                    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, query_pool.handle(), 1);
                    vkEndCommandBuffer(cmd);

                    VkSubmitInfo submit_info{};
                    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submit_info.commandBufferCount = 1;
                    submit_info.pCommandBuffers = &cmd;
                    vkQueueSubmit(ctx.compute_queue_, 1, &submit_info, VK_NULL_HANDLE);
                    vkQueueWaitIdle(ctx.compute_queue_);

                    // Get GPU timestamp (start_idx=0, end_idx=1)
                    double gpu_time_ns;
                    try {
                        gpu_time_ns = ctx.ComputeTimestampDelta(query_pool, 0, 1);
                    } catch (const std::exception& e) {
                        std::fprintf(stderr, "Timestamp query failed: %s\n", e.what());
                        return 1;
                    }

                    if (i >= warmup) {
                        tick_times.push_back(gpu_time_ns / 1000.0);  // ns → µs
                    }
                }

                // Readback stats + destination for validation
                Buffer stats_rb = ctx.ReadbackBuffer(bufs.stats.handle(), 16);
                uint32_t stats_readback[4] = {0, 0, 0, 0};
                std::memcpy(stats_readback, stats_rb.Map(), 16);
                stats_rb.Unmap();
                Buffer dest_rb = ctx.ReadbackBuffer(bufs.destination.handle(),
                                                     static_cast<VkDeviceSize>(total_cells * sizeof(Cell)));
                std::vector<Cell> dest_readback(total_cells);
                std::memcpy(dest_readback.data(), dest_rb.Map(), total_cells * sizeof(Cell));
                dest_rb.Unmap();
                const uint32_t fluid_after = CountFluid(dest_readback);
                const uint32_t conservation_violations = (fluid_after > fluid_before + 1 || fluid_after + 1 < fluid_before) ? 1 : 0;

                Stats s = ComputeStats(tick_times);
                ConfigResult result;
                result.strategy = StrategyName(sid);
                result.scene = SceneName(scene_type);
                result.seed = seed;
                result.stats = s;
                result.conservation_violations = conservation_violations;
                result.fluid_before = fluid_before;
                result.fluid_after = fluid_after;
                result.atomic_ops_count = stats_readback[2];

                csv << result.strategy << "," << result.scene << "," << result.seed << ","
                    << frames << "," << warmup << ","
                    << s.mean << "," << s.median << "," << s.p95 << "," << s.p99 << ","
                    << s.stddev << "," << s.min << "," << s.max << ","
                    << conservation_violations << "," << fluid_before << "," << fluid_after << ","
                    << stats_readback[2] << "\n";
                csv.flush();

                std::printf("[%s/%s/seed%u] mean=%.2f µs, p99=%.2f µs, fluid %u→%u, atomic_ops=%u, violations=%u\n",
                            StrategyName(sid), SceneName(scene_type), seed,
                            s.mean, s.p99, fluid_before, fluid_after,
                            stats_readback[2], conservation_violations);

                ctx.FreeCommandBuffer(cmd);
                // Ensure GPU is idle before destroying descriptor sets / buffers
                // (prevents use-after-free in NVIDIA driver when descriptors reference freed buffers).
                vkDeviceWaitIdle(ctx.device_);
                DestroyDescriptorSet(ctx.device_, ds);
            }
        }

        // Cleanup per-strategy
        for (auto p : pipelines) vkDestroyPipeline(ctx.device_, p, nullptr);
        vkDestroyPipelineLayout(ctx.device_, pl, nullptr);
    }

    // Cleanup
    for (auto sm : shader_modules) if (sm != VK_NULL_HANDLE) vkDestroyShaderModule(ctx.device_, sm, nullptr);
    csv.close();
    ctx.Shutdown();
    std::printf("Done. Results written to %s\n", csv_path.c_str());
    return 0;
}
