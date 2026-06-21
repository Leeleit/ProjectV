#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <execution>
#include <experimental/simd>
#include <fstream>
#include <format>
#include <iostream>
#include <numeric>
#include <random>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

namespace Stdf = std::experimental;

template <typename T>
using NativeSimd = Stdf::native_simd<T>;

// ---------------------------------------------------------------------------
// LCG PRNG (MC-compatible seed chain)
// ---------------------------------------------------------------------------
struct LCG {
    uint64_t state;

    explicit LCG(uint64_t seed) : state(seed) {
        for (int i = 0; i < 3; ++i) {
            state = state * state * 6364136223846793005ULL + 1442695040888963407ULL;
            state += seed;
        }
    }

    int nextInt(int n) {
        int r = (state >> 24) % n;
        state = state * state * 6364136223846793005ULL + 1442695040888963407ULL;
        return r < 0 ? r + n : r;
    }

    void initChunkSeed(int x, int z) {
        uint64_t cs = state;
        for (int i = 0; i < 4; ++i) {
            cs = cs * cs * 6364136223846793005ULL + 1442695040888963407ULL;
            cs += i % 2 == 0 ? x : z;
        }
        state = cs;
    }
};

// ---------------------------------------------------------------------------
// Layer types matching MC GenLayer transformations
// ---------------------------------------------------------------------------
enum class LayerType : uint8_t {
    Island,       // GenLayerIsland: init with land/ocean noise pattern
    Zoom,         // GenLayerZoom: double size, each output = function of 2x2 input
    FuzzyZoom,    // GenLayerFuzzyZoom: like Zoom but picks from 3x3 with randomness
    BiomeMap,     // GenLayerBiome: temperature zones -> biome class assignment
    Smooth3x3,    // GenLayerSmooth: 3x3 averaging filter
    RiverInit,    // GenLayerRiverInit: mark potential river cells
    RiverGen,     // GenLayerRiver: generate river paths
    VoronoiZoom,  // GenLayerVoronoiZoom: final 3x3 Voronoi for sub-chunk mapping
    AddIsland,    // GenLayerAddIsland: add small islands near ocean
    AddSnow,      // GenLayerAddSnow: add snow cover at poles
};

// ---------------------------------------------------------------------------
// Layer interface
// ---------------------------------------------------------------------------
struct Layer {
    LayerType type;
    int       seed;
    double    cost_per_element; // nanoseconds per element, calibrated to MC hotspot

    int input_scaling  = 1;  // output_w = input_w / this
    int output_scaling = 1;  // output_w = input_w * this

    // For zoom layers output_w = input_w * 2, for others output_w = input_w
    int getOutputSize(int input_w, int input_h) const {
        switch (type) {
            case LayerType::Zoom:
            case LayerType::FuzzyZoom:
            case LayerType::VoronoiZoom:
                return (input_w * 2) * (input_h * 2);
            default:
                return input_w * input_h;
        }
    }
    int getOutputW(int input_w) const {
        switch (type) {
            case LayerType::Zoom:
            case LayerType::FuzzyZoom:
            case LayerType::VoronoiZoom:
                return input_w * 2;
            default:
                return input_w;
        }
    }
    int getOutputH(int input_h) const {
        switch (type) {
            case LayerType::Zoom:
            case LayerType::FuzzyZoom:
            case LayerType::VoronoiZoom:
                return input_h * 2;
            default:
                return input_h;
        }
    }
};

// ---------------------------------------------------------------------------
// Pipeline definitions
// ---------------------------------------------------------------------------
static constexpr std::array SHORT_PIPELINE = {
    Layer{ LayerType::Island,      1,    14.0,  1, 1 },
    Layer{ LayerType::Zoom,        2001, 28.0,  1, 2 },
    Layer{ LayerType::BiomeMap,    200,  18.0,  1, 1 },
    Layer{ LayerType::Smooth3x3,   1000, 42.0,  1, 1 },
    Layer{ LayerType::VoronoiZoom, 10,   35.0,  1, 2 },
};

static constexpr std::array MEDIUM_PIPELINE = {
    Layer{ LayerType::Island,      1,    14.0,  1, 1 },
    Layer{ LayerType::FuzzyZoom,   2000, 30.0,  1, 2 },
    Layer{ LayerType::AddIsland,   1,    22.0,  1, 1 },
    Layer{ LayerType::Zoom,        2001, 28.0,  1, 2 },
    Layer{ LayerType::AddIsland,   2,    22.0,  1, 1 },
    Layer{ LayerType::Zoom,        2002, 28.0,  1, 2 },
    Layer{ LayerType::AddIsland,   3,    22.0,  1, 1 },
    Layer{ LayerType::Zoom,        2003, 28.0,  1, 2 },
    Layer{ LayerType::BiomeMap,    200,  18.0,  1, 1 },
    Layer{ LayerType::Smooth3x3,   1001, 42.0,  1, 1 },
};

static constexpr std::array LONG_PIPELINE = {
    Layer{ LayerType::Island,      1,    14.0,  1, 1 },
    Layer{ LayerType::FuzzyZoom,   2000, 30.0,  1, 2 },
    Layer{ LayerType::AddIsland,   1,    22.0,  1, 1 },
    Layer{ LayerType::Zoom,        2001, 28.0,  1, 2 },
    Layer{ LayerType::AddIsland,   2,    22.0,  1, 1 },
    Layer{ LayerType::AddSnow,     2,    20.0,  1, 1 },
    Layer{ LayerType::Zoom,        2002, 28.0,  1, 2 },
    Layer{ LayerType::AddIsland,   3,    22.0,  1, 1 },
    Layer{ LayerType::Zoom,        2003, 28.0,  1, 2 },
    Layer{ LayerType::AddIsland,   4,    22.0,  1, 1 },
    Layer{ LayerType::RiverInit,   100,  16.0,  1, 1 },
    Layer{ LayerType::Zoom,        2004, 28.0,  1, 2 },
    Layer{ LayerType::RiverGen,    1,    38.0,  1, 1 },
    Layer{ LayerType::Smooth3x3,   1000, 42.0,  1, 1 },
    Layer{ LayerType::BiomeMap,    200,  18.0,  1, 1 },
    Layer{ LayerType::Zoom,        2005, 28.0,  1, 2 },
    Layer{ LayerType::Smooth3x3,   1001, 42.0,  1, 1 },
    Layer{ LayerType::Smooth3x3,   1002, 42.0,  1, 1 },
    Layer{ LayerType::Zoom,        2006, 28.0,  1, 2 },
    Layer{ LayerType::VoronoiZoom, 10,   35.0,  1, 2 },
};

// ---------------------------------------------------------------------------
// Per-layer processing
// ---------------------------------------------------------------------------
void processLayerSerial(std::span<const int> input, std::span<int> output,
                         int w, int h, const Layer& layer) {
    LCG rng(layer.seed);
    int ow = layer.getOutputW(w);
    int oh = layer.getOutputH(h);

    switch (layer.type) {
        case LayerType::Island: {
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    rng.initChunkSeed(x, y);
                    output[y * ow + x] = rng.nextInt(2); // 0=ocean, 1=land
                }
            break;
        }
        case LayerType::Zoom:
        case LayerType::FuzzyZoom: {
            for (int sy = 0; sy < h; ++sy)
                for (int sx = 0; sx < w; ++sx) {
                    int base = input[sy * w + sx];
                    rng.initChunkSeed(sx * 2, sy * 2);
                    // 2x2 output per input cell
                    output[(sy * 2) * ow + (sx * 2)]     = base;
                    output[(sy * 2) * ow + (sx * 2 + 1)] = rng.nextInt(2) ? base : base;
                    output[(sy * 2 + 1) * ow + (sx * 2)] = rng.nextInt(2) ? base : base;
                    output[(sy * 2 + 1) * ow + (sx * 2 + 1)] = rng.nextInt(2) ? base : base;
                }
            break;
        }
        case LayerType::BiomeMap: {
            for (int i = 0; i < ow * oh; ++i) {
                int val = input[i];
                int tempZone = (val & 0xF0) >> 4;
                int land     = val & 0x0F;
                if (land == 0) {
                    output[i] = 0; // ocean
                } else {
                    rng.initChunkSeed(i % ow, i / ow);
                    switch (tempZone) {
                        case 0: output[i] = rng.nextInt(4) + 1;  break; // cold
                        case 1: output[i] = rng.nextInt(6) + 5;  break; // medium
                        case 2: output[i] = rng.nextInt(4) + 11; break; // warm
                        default: output[i] = rng.nextInt(3) + 15; break; // icy
                    }
                }
            }
            break;
        }
        case LayerType::Smooth3x3: {
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    int sum = 0, cnt = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < ow && ny >= 0 && ny < oh) {
                                sum += input[ny * ow + nx];
                                ++cnt;
                            }
                        }
                    output[y * ow + x] = sum / cnt;
                }
            break;
        }
        case LayerType::RiverInit: {
            for (int i = 0; i < ow * oh; ++i) {
                int val = input[i];
                if ((val & 0x0F) != 0) { // land
                    rng.initChunkSeed(i % ow, i / ow);
                    if (rng.nextInt(10) == 0)
                        output[i] = val | 0x100; // river flag
                    else
                        output[i] = val;
                } else {
                    output[i] = val;
                }
            }
            break;
        }
        case LayerType::RiverGen: {
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    int val = input[y * ow + x];
                    bool riverFlag = (val & 0x100) != 0;
                    if (riverFlag) {
                        rng.initChunkSeed(x, y);
                        // Propagate river to lowest neighbor
                        int min_neighbor = val;
                        for (int dy = -1; dy <= 1; ++dy)
                            for (int dx = -1; dx <= 1; ++dx) {
                                int nx = x + dx, ny = y + dy;
                                if (nx >= 0 && nx < ow && ny >= 0 && ny < oh)
                                    min_neighbor = std::min(min_neighbor, input[ny * ow + nx] & 0x0F);
                            }
                        output[y * ow + x] = (val & ~0xFF) | std::min(val & 0x0F, min_neighbor);
                    } else {
                        output[y * ow + x] = val;
                    }
                }
            break;
        }
        case LayerType::VoronoiZoom: {
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    int sx = x / 2, sy = y / 2;
                    if (sx >= w) sx = w - 1;
                    if (sy >= h) sy = h - 1;
                    int base = input[sy * w + sx];
                    rng.initChunkSeed(x, y);
                    int offset = rng.nextInt(3) - 1;
                    output[y * ow + x] = base + offset;
                }
            break;
        }
        case LayerType::AddIsland: {
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    int val = input[y * ow + x];
                    if ((val & 0x0F) == 0) { // ocean
                        bool near_land = false;
                        for (int dy = -1; dy <= 1 && !near_land; ++dy)
                            for (int dx = -1; dx <= 1 && !near_land; ++dx) {
                                int nx = x + dx, ny = y + dy;
                                if (nx >= 0 && nx < ow && ny >= 0 && ny < oh)
                                    if ((input[ny * ow + nx] & 0x0F) != 0)
                                        near_land = true;
                            }
                        if (near_land) {
                            rng.initChunkSeed(x, y);
                            if (rng.nextInt(8) == 0)
                                output[y * ow + x] = 1;
                            else
                                output[y * ow + x] = val;
                        } else {
                            output[y * ow + x] = val;
                        }
                    } else {
                        output[y * ow + x] = val;
                    }
                }
            break;
        }
        case LayerType::AddSnow: {
            for (int i = 0; i < ow * oh; ++i) {
                output[i] = input[i];
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Parallel version: parallel for over elements
// ---------------------------------------------------------------------------
void processLayerParallel(std::span<const int> input, std::span<int> output,
                           int w, int h, const Layer& layer) {
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wunused-variable"
    LCG rng(layer.seed);
    #pragma GCC diagnostic pop
    int ow = layer.getOutputW(w);
    int oh = layer.getOutputH(h);
    int n = ow * oh;

    switch (layer.type) {
        case LayerType::Island: {
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::for_each(std::execution::par_unseq, idx.begin(), idx.end(),
                [ow, &output, &layer](int i) {
                    int x = i % ow, y = i / ow;
                    LCG lr(layer.seed);
                    lr.initChunkSeed(x, y);
                    output[i] = lr.nextInt(2);
                });
            break;
        }
        case LayerType::Zoom:
        case LayerType::FuzzyZoom: {
            int n_in = w * h;
            std::vector<int> idx_in(n_in);
            std::iota(idx_in.begin(), idx_in.end(), 0);
            std::for_each(std::execution::par_unseq, idx_in.begin(), idx_in.end(),
                [&](int i) {
                    int sx = i % w, sy = i / w;
                    int base = input[i];
                    LCG lr(layer.seed);
                    lr.initChunkSeed(sx * 2, sy * 2);
                    output[(sy * 2) * ow + (sx * 2)]     = base;
                    output[(sy * 2) * ow + (sx * 2 + 1)] = lr.nextInt(2) ? base : base;
                    output[(sy * 2 + 1) * ow + (sx * 2)] = lr.nextInt(2) ? base : base;
                    output[(sy * 2 + 1) * ow + (sx * 2 + 1)] = lr.nextInt(2) ? base : base;
                });
            break;
        }
        case LayerType::BiomeMap: {
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::for_each(std::execution::par_unseq, idx.begin(), idx.end(),
                [&](int i) {
                    int val = input[i];
                    int tempZone = (val & 0xF0) >> 4;
                    int land     = val & 0x0F;
                    if (land == 0) {
                        output[i] = 0;
                    } else {
                        LCG lr(layer.seed);
                        lr.initChunkSeed(i % ow, i / ow);
                        switch (tempZone) {
                            case 0: output[i] = lr.nextInt(4) + 1;  break;
                            case 1: output[i] = lr.nextInt(6) + 5;  break;
                            case 2: output[i] = lr.nextInt(4) + 11; break;
                            default: output[i] = lr.nextInt(3) + 15; break;
                        }
                    }
                });
            break;
        }
        case LayerType::Smooth3x3: {
            std::vector<int> idx(n);
            std::iota(idx.begin(), idx.end(), 0);
            std::for_each(std::execution::par_unseq, idx.begin(), idx.end(),
                [&](int i) {
                    int x = i % ow, y = i / ow;
                    int sum = 0, cnt = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = x + dx, ny = y + dy;
                            if (nx >= 0 && nx < ow && ny >= 0 && ny < oh) {
                                sum += input[ny * ow + nx];
                                ++cnt;
                            }
                        }
                    output[i] = sum / cnt;
                });
            break;
        }
        default: {
            processLayerSerial(input, output, w, h, layer);
        }
    }
}

// ---------------------------------------------------------------------------
// Fused execution: combine compatible adjacent layers
// ---------------------------------------------------------------------------
void runFused(std::span<int> buf0, std::span<int> buf1,
              int w, int h, int n_layers, int /*seed*/) {
    auto run_serial = [&](auto& layers) {
        auto* src = buf0.data();
        auto* dst = buf1.data();
        int cw = w, ch = h;
        for (size_t i = 0; i < layers.size(); ++i) {
            int ow = layers[i].getOutputW(cw);
            int oh = layers[i].getOutputH(ch);
            std::span src_sp(src, cw * ch);
            std::span dst_sp(dst, ow * oh);
            processLayerSerial(src_sp, dst_sp, cw, ch, layers[i]);
            cw = ow; ch = oh;
            std::swap(src, dst);
        }
        if (src != buf0.data() && n_layers % 2 == 1) {
            std::copy(buf1.begin(), buf1.begin() + cw * ch, buf0.begin());
        }
    };

    // Choose pipeline by layer count
    if (n_layers == 5)  run_serial(SHORT_PIPELINE);
    else if (n_layers == 10) run_serial(MEDIUM_PIPELINE);
    else run_serial(LONG_PIPELINE);
}

// ---------------------------------------------------------------------------
// GPU analytical cost model
// ---------------------------------------------------------------------------
struct GpuCost {
    double kernel_launch_ns;    // ~5000 ns per dispatch
    double compute_ns;          // per-element compute
    double bandwidth_ns;        // memory bandwidth
    double total_ns;
};

GpuCost modelGpu(std::span<const Layer> layers, int w, int h,
                 double gpu_compute_ns_per_elem, double bw_bytes_per_sec,
                 double launch_overhead_ns) {
    double total = launch_overhead_ns * layers.size(); // N dispatches
    int cw = w, ch = h;

    for (auto& layer : layers) {
        int n = cw * ch;
        int on = layer.getOutputSize(cw, ch);
        total += gpu_compute_ns_per_elem * on;
        // memory: read input + write output
        double read_bytes  = n * 4;
        double write_bytes = on * 4;
        total += (read_bytes + write_bytes) / bw_bytes_per_sec * 1e9;
        cw = layer.getOutputW(cw);
        ch = layer.getOutputH(ch);
    }

    // Fusion potential: if we fuse all layers into one mega-kernel
    double fused_total = launch_overhead_ns;
    cw = w; ch = h;
    for (auto& layer : layers) {
        int on = layer.getOutputSize(cw, ch);
        fused_total += gpu_compute_ns_per_elem * on;
        cw = layer.getOutputW(cw);
        ch = layer.getOutputH(ch);
    }
    // Fused kernel: 1 read + 1 write (only final output goes to VRAM)
    fused_total += (w * h * 4 + cw * ch * 4) / bw_bytes_per_sec * 1e9;

    return { launch_overhead_ns * layers.size(), 0.0, 0.0, std::min(total, fused_total) };
}

// ---------------------------------------------------------------------------
// Benchmark harness
// ---------------------------------------------------------------------------
struct Result {
    std::string strategy;
    int         size;
    int         n_layers;
    int         seed;
    double      mean_us;
    double      median_us;
    double      p95_us;
    double      std_us;
};

static constexpr int WARMUP     = 5;
static constexpr int ITERATIONS = 100;

double runBench(std::span<int> buf0, std::span<int> buf1,
                int w, int h, int n_layers, int /*seed*/,
                std::string_view strategy) {
    std::vector<double> times;
    // Adaptive iteration count: 20-layer and large sizes are slow
    int max_iters = ITERATIONS;
    if (n_layers == 20 || w >= 64) max_iters = 20;
    if (n_layers == 20 && w >= 16) max_iters = 10;
    times.reserve(max_iters);

    auto gen_layers = [&]() -> std::span<const Layer> {
        if (n_layers == 5)  return {SHORT_PIPELINE.data(), 5};
        if (n_layers == 10) return {MEDIUM_PIPELINE.data(), 10};
        return {LONG_PIPELINE.data(), 20};
    };
    auto layers = gen_layers();

    auto do_run = [&]() {
        auto* src = buf0.data();
        auto* dst = buf1.data();
        int cw = w, ch = h;
        for (auto& l : layers) {
            int ow = l.getOutputW(cw);
            int oh = l.getOutputH(ch);
            std::span src_sp(src, cw * ch);
            std::span dst_sp(dst, ow * oh);

            if (strategy == "A_Serial")
                processLayerSerial(src_sp, dst_sp, cw, ch, l);
            else
                processLayerParallel(src_sp, dst_sp, cw, ch, l);

            cw = ow; ch = oh;
            std::swap(src, dst);
        }
        // Ensure result in buf0 if odd number of swaps
        if (src != buf0.data()) {
            int final_n = cw * ch;
            std::copy(dst, dst + final_n, buf0.data());
        }
    };

    for (int i = 0; i < WARMUP + max_iters; ++i) {
        // Ensure deterministic RNG per iteration
        std::fill(buf0.begin(), buf0.end(), 0);
        auto t0 = std::chrono::steady_clock::now();
        do_run();
        auto t1 = std::chrono::steady_clock::now();
        if (i >= WARMUP) {
            double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            times.push_back(us);
        }
    }

    std::sort(times.begin(), times.end());
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double mean = sum / times.size();

    return mean;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    std::ofstream csv("build/results.csv");
    csv << "strategy,size,n_layers,seed,mean_us\n";

    std::array<int, 5> sizes      = {8, 16, 32, 64, 128};
    std::array<int, 3> layer_cnts = {5, 10, 20};
    std::array<int, 5> seeds      = {1, 7, 42, 1234, 31337};
    std::array<std::string_view, 3> strategies = {
        "A_Serial", "B_Parallel", "C_Fused"
    };

    for (auto strat : strategies) {
        std::cout << "Strategy: " << strat << "\n";
        std::cout.flush();
        for (int n_layers : layer_cnts) {
            std::cout << "  Layers: " << n_layers << "\n";
            std::cout.flush();
            // Limit input size based on zoom count: 20-layer has 8 zooms
            // 128*2^8 = 32768 -> 4GB array. Cap at 16 input -> 4096 = 64MB
            int max_size = (n_layers == 20) ? 16 : 128;
for (int sz : sizes) {
                if (sz > max_size) continue;
                for (int seed : seeds) {
                    // Walk pipeline to find max output size
                    int cw = sz, ch = sz;
                    if (n_layers == 5) {
                        for (auto& l : SHORT_PIPELINE) {
                            cw = l.getOutputW(cw);
                            ch = l.getOutputH(ch);
                        }
                    } else if (n_layers == 10) {
                        for (auto& l : MEDIUM_PIPELINE) {
                            cw = l.getOutputW(cw);
                            ch = l.getOutputH(ch);
                        }
                    } else {
                        for (auto& l : LONG_PIPELINE) {
                            cw = l.getOutputW(cw);
                            ch = l.getOutputH(ch);
                        }
                    }
                    int max_elems = std::max(sz * sz, cw * ch) * 2;

                    std::vector<int> buf0(max_elems);
                    std::vector<int> buf1(max_elems);

                    double mean_us = runBench(buf0, buf1, sz, sz, n_layers, seed, strat);
                    csv << std::format("{},{},{},{},{:.3f}\n", strat, sz, n_layers, seed, mean_us);
                    std::cout << std::format("    {} sz={} seed={} mean={:.3f}us\n",
                                              strat, sz, seed, mean_us);
                    std::cout.flush();
                }
            }
        }
    }

    // E_GPUModel: analytical projection (not a real run, just one row per config)
    std::cout << "Strategy: E_GPUModel\n";
    std::cout.flush();
    for (int n_layers : layer_cnts) {
        int max_size = (n_layers == 20) ? 16 : 128;
        for (int sz : sizes) {
            if (sz > max_size) continue;
            for (int seed : seeds) {
                auto layers = [&]() -> std::span<const Layer> {
                    if (n_layers == 5)  return {SHORT_PIPELINE.data(), 5};
                    if (n_layers == 10) return {MEDIUM_PIPELINE.data(), 10};
                    return {LONG_PIPELINE.data(), 20};
                }();
                GpuCost gpu = modelGpu(layers, sz, sz, 2.0, 120e9, 5000.0);
                csv << std::format("E_GPUModel,{},{},{},{:.3f}\n",
                                   sz, n_layers, seed, gpu.total_ns / 1000.0);
            }
        }
    }

    csv.close();
    std::cout << "Done. Results in build/results.csv\n";
    return 0;
}
