#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <chrono>

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

constexpr int kChunkSize = 8;
constexpr int kFragmentsPerChunk = kChunkSize * kChunkSize * kChunkSize;
constexpr int kSeeds[] = {1, 7, 42, 1234, 31337};
constexpr int kWarmup = 10;
constexpr int kIter = 1000;
constexpr int kMaxMaterials = 256;

// RTX 3060 Ti Ampere GA104: 38 SMs × 1665 MHz boost
constexpr double kClockGhz = 1.665;
constexpr double kCycleNs = 1.0 / kClockGhz;
constexpr int kWarpSize = 32;

// Pipeline creation overhead (Ampere, Vulkan driver)
constexpr double kPipelineCreateUs = 850.0; // ~850 µs per vkCreateGraphicsPipeline
constexpr double kShaderCompileMs = 7.0;    // ~7 ms per GLSL→SPIR-V via libshaderc

// ---------------------------------------------------------------------------
// Scene definitions
// ---------------------------------------------------------------------------

struct SceneConfig {
    const char *name;
    int numMaterials;
    int numCustomShaders;       // how many materials have custom shaders
    double customShaderComplexity; // relative ALU multiplier vs baseline (1.0 = same as baseline)
};

constexpr SceneConfig kScenes[] = {
    {"uniform_floor",    1,  0, 0.0},
    {"mixed_biome",      6,  2, 2.5},
    {"forest_floor",     4,  2, 4.0},
    {"cave_stress",      3,  1, 3.0},
    {"custom_shader_heavy", 16, 16, 3.0},
};

// ---------------------------------------------------------------------------
// Material shader complexity model (ALU cost in cycles per fragment)
// ---------------------------------------------------------------------------

struct MaterialShader {
    std::string name;
    // ALU cycle cost per fragment
    int aluCycles;
    // Register count (affects occupancy)
    int registers;
    // Whether shader uses divergent control flow internally
    bool hasDivergence;
};

MaterialShader makeMaterialShader(const std::string &name, int aluCycles, int regs, bool div) {
    return {name, aluCycles, regs, div};
}

// Baseline PBR material evaluation (current mainline)
MaterialShader baselineMaterial() {
    // materials[inMaterialIndex] SSBO load (4-8 cycles) + unpack (4 cycles) + ~10 ALU
    return makeMaterialShader("baseline", 20, 16, false);
}

// Custom shader variants
MaterialShader customSimple() {
    // Flat color override + tint (minimal custom shader)
    return makeMaterialShader("custom_simple", 30, 18, false);
}

MaterialShader customEmissivePulse() {
    // sin(time) * emissiveStrength + baseColor modulation
    return makeMaterialShader("custom_emissive", 60, 24, false);
}

MaterialShader customAnimatedUV() {
    // UV scrolling + noise-based perturbation + PBR modulation
    return makeMaterialShader("custom_animated", 110, 32, true);
}

MaterialShader customProceduralTexture() {
    // Procedural checker/gradient/wood texture generation (~20 ALU ops for simple proc tex)
    return makeMaterialShader("custom_procedural", 140, 28, true);
}

MaterialShader customComplexPBR() {
    // Custom lighting model: clearcoat + anisotropy + subsurface
    return makeMaterialShader("custom_complex", 200, 40, true);
}

// ---------------------------------------------------------------------------
// Analytical cost models
// ---------------------------------------------------------------------------

struct StrategyResult {
    std::string strategy;
    std::string scene;
    int seed;
    double meanUs;           // mean time per fragment (microseconds)
    double divergencePenalty; // warp divergence penalty
    double vramBytes;        // shader storage VRAM
    double pipelineCreateAmortized; // pipeline/build overhead amortized per-frame
    double compileMsPerUnique; // shader compilation time
    int numPipelines;        // number of unique pipelines needed
    int numFunctionSlots;    // for uber-shader: number of function table slots
    double aluCost;          // raw ALU cost in cycles
};

// Shader binary size estimate (compiled SPIR-V) by complexity
double estimateShaderBinaryBytes(int aluCycles) {
    // Approx: ~4 bytes per SPIR-V word, ~3 words per ALU operation + overhead
    return std::max(512.0, aluCycles * 12.0);
}

// Pipeline object VRAM (VkPipeline + shader module + descriptor set templates)
double estimatePipelineVramBytes() {
    return 8.0 * 1024.0; // ~8 KiB per pipeline object in driver
}

// Uber-shader: single shader with function table
// Cost = function table dispatch (indirect call through array) + selected shader body
double evaluateUberShader(const SceneConfig &scene, MaterialShader (&shaders)[6], int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));
    std::uniform_int_distribution<int> matDist(0, scene.numMaterials - 1);

    // Assign custom shaders to some materials
    // Materials 0..(n-1) = baseline, last kCustom = custom
    int firstCustom = scene.numMaterials - scene.numCustomShaders;

    double totalCycles = 0.0;
    int totalFragments = kFragmentsPerChunk * kWarmup; // representative batch

    // Uber-shader overhead: function table index load (2 cycles) + indirect call (4 cycles)
    constexpr int kUberDispatchOverhead = 6;

    for (int f = 0; f < totalFragments; ++f) {
        int mat = matDist(rng);
        const MaterialShader *shader = &shaders[0]; // baseline
        if (mat >= firstCustom) {
            int customIdx = mat - firstCustom;
            int shaderIdx = customIdx % 5; // cycle through custom types
            shader = &shaders[1 + shaderIdx];
        }
        totalCycles += kUberDispatchOverhead + shader->aluCycles;
    }

    double meanCycles = totalCycles / totalFragments;
    return meanCycles * kCycleNs / 1000.0; // convert to µs
}

// Custom pipeline: N unique pipelines, sorted by shader handle per-frame
// Cost = per-fragment shader ALU (no dispatch overhead) + pipeline state change overhead
double evaluateCustomPipeline(const SceneConfig &scene, MaterialShader (&shaders)[6], int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));
    std::uniform_int_distribution<int> matDist(0, scene.numMaterials - 1);

    int firstCustom = scene.numMaterials - scene.numCustomShaders;

    // Pipeline state change frequency depends on sorting
    // Assuming chunks are sorted by shader handle: 1 state change per unique shader per frame
    int uniqueShaders = 1 + scene.numCustomShaders; // 1 baseline + N custom
    // Pipeline switch cost: ~5 µs on Ampere Vulkan driver (bind pipeline + descriptors)
    constexpr double kPipelineSwitchUs = 5.0;
    // Amortize over chunks rendered per frame (assume ~100 chunks visible = 51200 fragments)
    constexpr int kVisibleChunks = 100;
    double pipelineSwitchPerFragment = (kPipelineSwitchUs * uniqueShaders) / (kVisibleChunks * kFragmentsPerChunk);

    double totalCycles = 0.0;
    int totalFragments = kFragmentsPerChunk * kWarmup;

    for (int f = 0; f < totalFragments; ++f) {
        int mat = matDist(rng);
        const MaterialShader *shader = &shaders[0];
        if (mat >= firstCustom) {
            int customIdx = mat - firstCustom;
            int shaderIdx = customIdx % 5;
            shader = &shaders[1 + shaderIdx];
        }
        totalCycles += shader->aluCycles;
    }

    double meanCycles = totalCycles / totalFragments;
    double shaderCostUs = meanCycles * kCycleNs / 1000.0;
    return shaderCostUs + pipelineSwitchPerFragment;
}

// Hybrid: uber-shader for N <= 4 unique, per-pipeline for > 4 unique
double evaluateHybrid(const SceneConfig &scene, MaterialShader (&shaders)[6], int seed) {
    if (scene.numCustomShaders <= 4) {
        return evaluateUberShader(scene, shaders, seed);
    }
    return evaluateCustomPipeline(scene, shaders, seed);
}

// Baseline: SSBO lookup only
double evaluateBaseline(const SceneConfig &, MaterialShader (&shaders)[6], int) {
    auto &base = shaders[0];
    return base.aluCycles * kCycleNs / 1000.0;
}

// ---------------------------------------------------------------------------
// Divergence model
// ---------------------------------------------------------------------------

double computeDivergenceForScene(const SceneConfig &scene, int seed) {
    std::mt19937 rng(static_cast<unsigned>(seed));
    std::uniform_int_distribution<int> matDist(0, scene.numMaterials - 1);

    int firstCustom = scene.numMaterials - scene.numCustomShaders;
    int totalWarps = (kFragmentsPerChunk * kWarmup + kWarpSize - 1) / kWarpSize;
    double totalDivPenalty = 0.0;

    for (int w = 0; w < totalWarps; ++w) {
        int uniqueInWarp = 0;
        int seen[kMaxMaterials] = {0};
        int minCost = 1000000, maxCost = 0;

        for (int t = 0; t < kWarpSize; ++t) {
            int idx = w * kWarpSize + t;
            if (idx >= kFragmentsPerChunk * kWarmup) break;
            int mat = matDist(rng);
            if (!seen[mat]++) uniqueInWarp++;
            // cost depends on shader type
            int cost = 20; // baseline
            if (mat >= firstCustom) {
                int customIdx = mat - firstCustom;
                int shaderIdx = customIdx % 5;
                constexpr int customCosts[5] = {30, 60, 110, 140, 200};
                cost = customCosts[shaderIdx];
            }
            minCost = std::min(minCost, cost);
            maxCost = std::max(maxCost, cost);
        }

        double warpCost = 0.0;
        if (uniqueInWarp > 1) {
            // Divergent warp: worst-case all threads take both paths
            warpCost = static_cast<double>(uniqueInWarp - 1) / kWarpSize * (maxCost - minCost) * 0.5;
        }
        totalDivPenalty += warpCost;
    }

    double meanDivPenalty = totalDivPenalty / totalWarps * kCycleNs / 1000.0;
    return meanDivPenalty;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    MaterialShader shaders[6] = {
        baselineMaterial(),
        customSimple(),
        customEmissivePulse(),
        customAnimatedUV(),
        customProceduralTexture(),
        customComplexPBR(),
    };

    std::vector<StrategyResult> results;
    results.reserve(4 * 5 * 5 * kIter);

    for (const auto &scene : kScenes) {
        for (int seed : kSeeds) {
            std::mt19937 rng(static_cast<unsigned>(seed));

            // Warmup
            for (int w = 0; w < kWarmup; ++w) {
                (void)evaluateBaseline(scene, shaders, seed);
                (void)evaluateUberShader(scene, shaders, seed);
                (void)evaluateCustomPipeline(scene, shaders, seed);
                (void)evaluateHybrid(scene, shaders, seed);
            }

            for (int i = 0; i < kIter; ++i) {
                auto measure = [&](auto fn, const std::string &name, int numPipes, int numSlots) {
                    double us = fn(scene, shaders, seed);

                    // Divergence penalty (applies to uber-shader and hybrid-uber mode)
                    double divergence = 0.0;
                    if (name == "B_UberShader" || name == "D_Hybrid") {
                        divergence = computeDivergenceForScene(scene, seed);
                    }

                    // Pipeline overhead amortization
                    double pipelineAmort = 0.0;
                    if (name == "C_CustomPipeline") {
                        // Amortize pipeline creation over expected lifetime (assume 1000 frames before rebuild)
                        pipelineAmort = (numPipes * kPipelineCreateUs) / 1000.0 / (100 * kFragmentsPerChunk);
                    }

                    // Shader compilation time (once per unique shader, not per-frame)
                    double compileMs = numPipes * kShaderCompileMs;

                    // VRAM for shader storage
                    double vram = 0.0;
                    if (name == "B_UberShader") {
                        // Single uber-shader binary
                        vram = estimateShaderBinaryBytes(200) + estimatePipelineVramBytes();
                    } else if (name == "C_CustomPipeline") {
                        // N shader binaries + N pipeline objects
                        vram = 0.0;
                        int matIdx = 0;
                        for (int m = 0; m < scene.numMaterials; ++m) {
                            if (m >= scene.numMaterials - scene.numCustomShaders) {
                                int si = matIdx++ % 5;
                                vram += estimateShaderBinaryBytes(shaders[1 + si].aluCycles) +
                                        estimatePipelineVramBytes();
                            }
                        }
                        // Baseline shader too
                        vram += estimateShaderBinaryBytes(shaders[0].aluCycles) + estimatePipelineVramBytes();
                    } else if (name == "D_Hybrid") {
                        if (scene.numCustomShaders <= 4) {
                            vram = estimateShaderBinaryBytes(200) + estimatePipelineVramBytes();
                        } else {
                            vram = 0.0;
                            for (int m = 0; m < scene.numMaterials; ++m) {
                                int si = (m >= scene.numMaterials - scene.numCustomShaders) ? (m % 5) : -1;
                                if (si >= 0) {
                                    vram += estimateShaderBinaryBytes(shaders[1 + si].aluCycles) +
                                            estimatePipelineVramBytes();
                                }
                            }
                            vram += estimateShaderBinaryBytes(shaders[0].aluCycles) + estimatePipelineVramBytes();
                        }
                    }

                    StrategyResult r;
                    r.strategy = name;
                    r.scene = scene.name;
                    r.seed = seed;
                    r.meanUs = us + divergence + pipelineAmort;
                    r.divergencePenalty = divergence;
                    r.vramBytes = vram;
                    r.pipelineCreateAmortized = pipelineAmort;
                    r.compileMsPerUnique = compileMs;
                    r.numPipelines = numPipes;
                    r.numFunctionSlots = numSlots;
                    r.aluCost = (us / kCycleNs) * 1000.0; // cycles
                    return r;
                };

                results.push_back(measure(evaluateBaseline, "A_Baseline", 1, 0));
                results.push_back(measure(evaluateUberShader, "B_UberShader", 1, 1 + scene.numCustomShaders));
                results.push_back(measure(evaluateCustomPipeline, "C_CustomPipeline", 1 + scene.numCustomShaders, 0));
                results.push_back(measure(evaluateHybrid, "D_Hybrid",
                    scene.numCustomShaders <= 4 ? 1 : (1 + scene.numCustomShaders),
                    scene.numCustomShaders <= 4 ? (1 + scene.numCustomShaders) : 0));
            }
        }
    }

    // Write CSV
    std::FILE *f = std::fopen("build/results.csv", "w");
    if (!f) { std::perror("fopen"); return 1; }

    std::fprintf(f, "strategy,scene,seed,meanUs,divergencePenaltyUs,vramBytes,pipelineAmortUs,"
                    "compileMsPerUnique,numPipelines,numFunctionSlots,aluCycles\n");
    for (const auto &r : results) {
        std::fprintf(f, "%s,%s,%d,%.6f,%.6f,%.0f,%.6f,%.2f,%d,%d,%.0f\n",
            r.strategy.c_str(), r.scene.c_str(), r.seed,
            r.meanUs, r.divergencePenalty, r.vramBytes,
            r.pipelineCreateAmortized, r.compileMsPerUnique,
            r.numPipelines, r.numFunctionSlots, r.aluCost);
    }
    std::fclose(f);

    // Print human-readable summary
    std::printf("\n=== Voxel GPU Shader Editor — Benchmark Summary ===\n");
    std::printf("Config: 4 strategies × 5 scenes × %d seeds × %d iter\n\n", (int)std::size(kSeeds), kIter);

    // Aggregate per-strategy
    struct Agg {
        std::string name;
        double meanUs = 0, divergence = 0, vram = 0, pipelineAmort = 0, compileMs = 0;
        int count = 0, numPipes = 0, numSlots = 0;
    };
    Agg aggs[4];
    for (const auto &r : results) {
        int idx = 0;
        if (r.strategy == "A_Baseline") idx = 0;
        else if (r.strategy == "B_UberShader") idx = 1;
        else if (r.strategy == "C_CustomPipeline") idx = 2;
        else if (r.strategy == "D_Hybrid") idx = 3;
        else continue;
        auto &a = aggs[idx];
        a.name = r.strategy;
        a.meanUs += r.meanUs;
        a.divergence += r.divergencePenalty;
        a.vram += r.vramBytes;
        a.pipelineAmort += r.pipelineCreateAmortized;
        a.compileMs += r.compileMsPerUnique;
        a.numPipes += r.numPipelines;
        a.numSlots += r.numFunctionSlots;
        a.count++;
    }

    std::printf("%-20s | %10s | %10s | %10s | %10s | %8s | %6s\n",
        "Strategy", "Mean (µs)", "Div (µs)", "VRAM (KiB)", "Amort (µs)", "Compile", "#Pipes");
    std::printf("%s\n", std::string(90, '-').c_str());
    for (auto &a : aggs) {
        if (a.count == 0) continue;
        double n = a.count;
        std::printf("%-20s | %10.4f | %10.4f | %10.1f | %10.6f | %7.2fms | %4d\n",
            a.name.c_str(),
            a.meanUs / n,
            a.divergence / n,
            a.vram / n / 1024.0,
            a.pipelineAmort / n,
            a.compileMs / n,
            (int)(a.numPipes / n));
    }

    // Per-scene breakdown for best vs worst
    std::printf("\n--- Per-scene worst-case (custom_shader_heavy) ---\n");
    std::printf("%-20s | %10s | %10s | %10s\n", "Strategy", "Mean (µs)", "Div (µs)", "VRAM (KiB)");
    std::printf("%s\n", std::string(55, '-').c_str());
    for (const auto &r : results) {
        if (r.scene != "custom_shader_heavy") continue;
        if (r.seed != 42) continue; // show only seed 42 for brevity
        std::printf("%-20s | %10.4f | %10.4f | %10.1f\n",
            r.strategy.c_str(), r.meanUs, r.divergencePenalty, r.vramBytes / 1024.0);
    }

    std::printf("\n--- Key observations ---\n");
    std::printf("Baseline (SSBO lookup): ~%.1f cycles per fragment\n", aggs[0].meanUs / aggs[0].count / kCycleNs * 1000.0);
    std::printf("UberShader overhead: +%.1f%% over baseline (mean)\n",
        (aggs[1].meanUs / aggs[1].count - aggs[0].meanUs / aggs[0].count) /
        (aggs[0].meanUs / aggs[0].count) * 100.0);
    std::printf("CustomPipeline max pipelines: %d (custom_shader_heavy)\n",
        (int)(aggs[2].numPipes / aggs[2].count));
    std::printf("\nDone. Results written to build/results.csv\n");

    return 0;
}
