// Stream benchmark: 5 chunk streaming strategies x 5 movement patterns x 5 seeds.
// Synthetic voxel world: 16x16x16 chunk grid = 4096 chunks max,
// 1.7 KiB/compressed per chunk, 3-tier memory hierarchy (VRAM/RAM/SSD).
// Single-session standalone prototype per `benchmarks/methodology.md §7` Stats harness.
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic -o stream_bench stream_bench.cpp
// Run:   ./stream_bench --warmup 10 --frames 1000 --output results.csv

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// Stats harness (per `benchmarks/methodology.md §7`)
// ============================================================================

struct Stats {
    double mean{};
    double median{};
    double p95{};
    double p99{};
    double stddev{};
    double minv{};
    double maxv{};
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    if (samples.empty()) return s;
    std::ranges::sort(samples);
    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    s.mean = sum / static_cast<double>(samples.size());
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

// ============================================================================
// World model
// ============================================================================

// Chunk identifier (xyz in chunk-space; chunkSize=8 voxels, 16 chunks/dim = 128 m world).
struct ChunkId {
    int x, y, z;
    bool operator==(const ChunkId& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
    bool operator<(const ChunkId& o) const noexcept {
        return std::tie(x, y, z) < std::tie(o.x, o.y, o.z);
    }
};
struct ChunkIdHash {
    size_t operator()(const ChunkId& c) const noexcept {
        return std::hash<int64_t>{}(
            (static_cast<int64_t>(c.x) * 73856093) ^
            (static_cast<int64_t>(c.y) * 19349663) ^
            (static_cast<int64_t>(c.z) * 83492791));
    }
};

// 3-tier memory hierarchy (latency in microseconds per chunk load).
struct MemoryTiers {
    double vram_us = 0.0;         // VRAM-resident: 0 µs (VMA heap, 448 GB/s peak)
    double ram_us = 0.035;        // RAM: 1.7 KiB @ 50 GB/s ≈ 35 ns
    double ssd_us = 0.6;          // SSD: 1.7 KiB @ 3 GB/s ≈ 0.6 µs
    double decompress_us = 0.5;   // LZ4-like fast decompress, single-thread Zen 3
    std::size_t vram_capacity_bytes = 8ull * 1024 * 1024 * 1024;   // 8 GiB RTX 3060 Ti
    std::size_t ram_capacity_bytes  = 8ull * 1024 * 1024 * 1024;   // 8 GiB RAM cache (assumed)
    std::size_t chunk_bytes_compressed = 1700;                      // 1.7 KiB/chunk (nanovdb-on-gpu representative)
};

// Chunk tier (current storage location).
enum class ChunkTier : uint8_t { SSD = 0, RAM = 1, VRAM = 2, Count = 3 };

struct Chunk {
    ChunkTier tier = ChunkTier::SSD;
    std::uint64_t last_access_frame = 0;
    std::uint32_t priority_score = 0;  // for predictive prefetch
};

// ============================================================================
// 3-tier cache store
// ============================================================================

struct TierStore {
    MemoryTiers tiers;
    std::unordered_map<ChunkId, Chunk, ChunkIdHash> chunks;
    std::size_t tier_bytes[3] = {0, 0, 0};

    bool ensure_capacity(ChunkTier tier, std::size_t bytes_needed) {
        std::size_t cap = (tier == ChunkTier::VRAM) ? tiers.vram_capacity_bytes :
                          (tier == ChunkTier::RAM) ? tiers.ram_capacity_bytes :
                                                    std::numeric_limits<std::size_t>::max();
        return tier_bytes[static_cast<int>(tier)] + bytes_needed <= cap;
    }

    void evict_until(ChunkTier tier, std::size_t target_bytes) {
        // Simple LRU eviction: drop oldest-accessed chunks until tier usage <= target.
        std::vector<std::pair<ChunkId, std::uint64_t>> access_list;
        for (auto& [id, c] : chunks) {
            if (c.tier == tier) access_list.emplace_back(id, c.last_access_frame);
        }
        std::ranges::sort(access_list,
            [](auto& a, auto& b) { return a.second < b.second; });  // oldest first
        for (auto& [id, _] : access_list) {
            if (tier_bytes[static_cast<int>(tier)] <= target_bytes) break;
            auto it = chunks.find(id);
            if (it == chunks.end()) continue;
            // Demote tier: VRAM->RAM, RAM->SSD, SSD->remove (gen-world will recreate)
            if (tier == ChunkTier::VRAM) {
                if (ensure_capacity(ChunkTier::RAM, tiers.chunk_bytes_compressed)) {
                    it->second.tier = ChunkTier::RAM;
                    tier_bytes[static_cast<int>(ChunkTier::VRAM)] -= tiers.chunk_bytes_compressed;
                    tier_bytes[static_cast<int>(ChunkTier::RAM)] += tiers.chunk_bytes_compressed;
                } else {
                    // RAM full: drop to SSD
                    it->second.tier = ChunkTier::SSD;
                    tier_bytes[static_cast<int>(ChunkTier::VRAM)] -= tiers.chunk_bytes_compressed;
                    // tier_bytes[SSD] unchanged (no allocation cost)
                }
            } else if (tier == ChunkTier::RAM) {
                it->second.tier = ChunkTier::SSD;
                tier_bytes[static_cast<int>(ChunkTier::RAM)] -= tiers.chunk_bytes_compressed;
            }
        }
    }

    double load_chunk(ChunkId id, std::uint64_t frame) {
        // Returns latency in microseconds. Creates chunk if missing.
        auto it = chunks.find(id);
        if (it == chunks.end()) {
            // First-time load: must come from "world gen" (modeled as 5 µs avg).
            chunks[id] = Chunk{ChunkTier::SSD, frame, 0};
            return 5.0;  // world-gen cost
        }
        Chunk& c = it->second;
        c.last_access_frame = frame;
        switch (c.tier) {
            case ChunkTier::VRAM: return tiers.vram_us;
            case ChunkTier::RAM:  return tiers.ram_us;
            case ChunkTier::SSD:  {
                // Promote to RAM, then VRAM (if VRAM has space).
                double cost = tiers.ssd_us + tiers.decompress_us;
                if (ensure_capacity(ChunkTier::VRAM, tiers.chunk_bytes_compressed)) {
                    // Promote straight to VRAM (single-step).
                    c.tier = ChunkTier::VRAM;
                    tier_bytes[static_cast<int>(ChunkTier::VRAM)] += tiers.chunk_bytes_compressed;
                } else {
                    // Evict VRAM to make room, then promote.
                    evict_until(ChunkTier::VRAM,
                                tier_bytes[static_cast<int>(ChunkTier::VRAM)] - tiers.chunk_bytes_compressed);
                    c.tier = ChunkTier::VRAM;
                    tier_bytes[static_cast<int>(ChunkTier::VRAM)] += tiers.chunk_bytes_compressed;
                }
                return cost;
            }
            default: return 0.0;
        }
    }

    void record_predictive_hit(ChunkId id, std::uint32_t priority, std::uint64_t frame) {
        auto it = chunks.find(id);
        if (it == chunks.end()) {
            chunks[id] = Chunk{ChunkTier::RAM, frame, priority};
            tier_bytes[static_cast<int>(ChunkTier::RAM)] += tiers.chunk_bytes_compressed;
            if (!ensure_capacity(ChunkTier::RAM, 0)) {
                evict_until(ChunkTier::RAM, tier_bytes[static_cast<int>(ChunkTier::RAM)] - tiers.chunk_bytes_compressed);
            }
        } else {
            Chunk& c = it->second;
            c.last_access_frame = frame;
            c.priority_score = priority;
            if (c.tier == ChunkTier::SSD) {
                // Promote to RAM in background (predictive load).
                c.tier = ChunkTier::RAM;
                tier_bytes[static_cast<int>(ChunkTier::RAM)] += tiers.chunk_bytes_compressed;
                if (!ensure_capacity(ChunkTier::RAM, 0)) {
                    evict_until(ChunkTier::RAM, tier_bytes[static_cast<int>(ChunkTier::RAM)] - tiers.chunk_bytes_compressed);
                }
            }
        }
    }

    void reset() {
        chunks.clear();
        std::memset(tier_bytes, 0, sizeof(tier_bytes));
    }
};

// ============================================================================
// Streaming strategy interface
// ============================================================================

enum class StrategyKind : uint8_t {
    A_PrebakeAll = 0,
    B_FixedRing,
    C_PredictiveStreaming,
    D_DemandPaging,
    E_HybridDemandPredictive,
    Count
};

std::string_view StrategyName(StrategyKind k) {
    switch (k) {
        case StrategyKind::A_PrebakeAll: return "A_PrebakeAll";
        case StrategyKind::B_FixedRing: return "B_FixedRing";
        case StrategyKind::C_PredictiveStreaming: return "C_PredictiveStreaming";
        case StrategyKind::D_DemandPaging: return "D_DemandPaging";
        case StrategyKind::E_HybridDemandPredictive: return "E_HybridDemandPredictive";
        default: return "?";
    }
}

struct CameraState {
    ChunkId pos{};      // current chunk position
    ChunkId velocity{}; // chunks-per-frame direction (signed)
};

// On-access: read current camera, return list of chunk IDs to load this frame.
// Returns latency budget used in microseconds (the strategy must NOT block the main thread
// — main-thread blocking = frame stutter; background-thread budget tracked separately).
struct FrameResult {
    double main_thread_block_us = 0.0;  // stutter budget
    double background_load_us = 0.0;   // background thread budget
    std::size_t chunks_loaded = 0;
};

struct Strategy {
    StrategyKind kind;
    TierStore store;
    // For B_FixedRing: ring size = fraction of VRAM.
    double ring_vram_fraction = 0.5;  // B = 50% of VRAM ring (vs unbounded A)

    void init(StrategyKind k, MemoryTiers tiers) {
        kind = k;
        store.tiers = tiers;
        store.reset();
    }

    // Called once at startup.
    FrameResult on_startup() {
        FrameResult r{};
        if (kind == StrategyKind::A_PrebakeAll) {
            // Prebake = load all chunks at startup (worst-case: full world).
            for (int x = 0; x < 16; ++x)
                for (int y = 0; y < 16; ++y)
                    for (int z = 0; z < 16; ++z) {
                        store.load_chunk({x, y, z}, 0);
                        ++r.chunks_loaded;
                    }
            r.main_thread_block_us = 5.0 * 16 * 16 * 16;  // 20 ms startup cost
        }
        return r;
    }

    // Called every frame: decide which chunks to load + measure latency.
    // cam.pos = camera chunk, cam.velocity = chunks/frame direction.
    FrameResult on_frame(CameraState cam, std::uint64_t frame_num) {
        FrameResult r{};
        // Determine visible chunks (3x3x3 cube around camera = 27 chunks max per frame).
        std::vector<ChunkId> visible;
        for (int dx = -1; dx <= 1; ++dx)
            for (int dy = -1; dy <= 1; ++dy)
                for (int dz = -1; dz <= 1; ++dz) {
                    visible.push_back({cam.pos.x + dx, cam.pos.y + dy, cam.pos.z + dz});
                }

        // Step 1: load visible chunks (must complete this frame for 0 stutter).
        for (auto& id : visible) {
            double cost = store.load_chunk(id, frame_num);
            // Cost is added to whichever budget it belongs to (sync vs async).
            switch (kind) {
                case StrategyKind::A_PrebakeAll:
                case StrategyKind::B_FixedRing:
                    // Sync loads (block on miss).
                    r.main_thread_block_us += cost;
                    break;
                case StrategyKind::C_PredictiveStreaming:
                    // Sync for visible (must be in VRAM) + background for predictive.
                    r.main_thread_block_us += cost;
                    break;
                case StrategyKind::D_DemandPaging:
                case StrategyKind::E_HybridDemandPredictive:
                    // Async load: visible chunk must be ready, but we assume
                    // background pre-fetched it (predictive) or page-fault on first access
                    // (returns cached low-latency result).
                    if (cost <= store.tiers.ram_us) {
                        // Already in RAM/VRAM (prefetched by background thread).
                        r.main_thread_block_us += cost;
                    } else {
                        // SSD miss on first access — register as stutter (real-world path).
                        r.main_thread_block_us += cost;  // D + E still pay for first miss
                    }
                    break;
                default: break;
            }
            ++r.chunks_loaded;
        }

        // Step 2: background-thread work (predictive prefetch for C/E).
        if (kind == StrategyKind::C_PredictiveStreaming ||
            kind == StrategyKind::E_HybridDemandPredictive) {
            // Predict 3-chunk shell ahead of velocity direction.
            ChunkId ahead = {cam.pos.x + cam.velocity.x * 3,
                             cam.pos.y + cam.velocity.y * 3,
                             cam.pos.z + cam.velocity.z * 3};
            // Load 3x3 shell around ahead (predictive).
            int prefetched = 0;
            for (int dx = -1; dx <= 1 && prefetched < 9; ++dx)
                for (int dy = -1; dy <= 1 && prefetched < 9; ++dy)
                    for (int dz = -1; dz <= 1 && prefetched < 9; ++dz) {
                        ChunkId id = {ahead.x + dx, ahead.y + dy, ahead.z + dz};
                        store.record_predictive_hit(id, 100, frame_num);
                        r.background_load_us += store.tiers.ssd_us + store.tiers.decompress_us;
                        ++prefetched;
                    }
        }

        return r;
    }

    // For B_FixedRing: cap VRAM usage after each frame.
    void post_frame() {
        if (kind == StrategyKind::B_FixedRing) {
            std::size_t cap = static_cast<std::size_t>(
                store.tiers.vram_capacity_bytes * ring_vram_fraction);
            if (store.tier_bytes[static_cast<int>(ChunkTier::VRAM)] > cap) {
                store.evict_until(ChunkTier::VRAM, cap);
            }
        }
    }
};

// ============================================================================
// Movement pattern generators
// ============================================================================

enum class SceneKind : uint8_t {
    linear_walk = 0,
    teleport_stress,
    orbit_center,
    fly_vertical,
    spiral_in,
    Count
};

std::string_view SceneName(SceneKind s) {
    switch (s) {
        case SceneKind::linear_walk: return "linear_walk";
        case SceneKind::teleport_stress: return "teleport_stress";
        case SceneKind::orbit_center: return "orbit_center";
        case SceneKind::fly_vertical: return "fly_vertical";
        case SceneKind::spiral_in: return "spiral_in";
        default: return "?";
    }
}

struct MovementState {
    double t = 0.0;
    ChunkId pos{};
    ChunkId velocity{};
    double speed_chunks_per_frame = 0.5;  // typical walking speed
};

ChunkId ClampToWorld(ChunkId c) {
    auto clamp = [](int v) { return std::clamp(v, 0, 15); };
    return {clamp(c.x), clamp(c.y), clamp(c.z)};
}

MovementState StepMovement(SceneKind scene, MovementState s, std::uint64_t frame, std::mt19937& rng) {
    s.t = static_cast<double>(frame);
    switch (scene) {
        case SceneKind::linear_walk: {
            // Constant velocity along +X axis.
            s.velocity = {1, 0, 0};
            s.pos = ClampToWorld({static_cast<int>(s.t * s.speed_chunks_per_frame) % 14 + 1, 8, 8});
            break;
        }
        case SceneKind::teleport_stress: {
            // Random teleport every 1-5 seconds (30-150 frames @ 30 Hz).
            if (frame % 60 == 0 || s.velocity == ChunkId{0, 0, 0}) {
                std::uniform_int_distribution<int> dist(0, 15);
                s.pos = {dist(rng), dist(rng), dist(rng)};
                // Brief settling period: stay still for a few frames.
                s.velocity = {0, 0, 0};
            } else if (frame % 60 < 5) {
                s.velocity = {0, 0, 0};  // post-teleport settle
            } else {
                s.velocity = {0, 0, 0};  // stay in place most frames
            }
            break;
        }
        case SceneKind::orbit_center: {
            // Camera circles around center (8, 8, 8) at constant radius.
            double angle = s.t * 0.02;  // slow orbit
            double radius = 6.0;
            int cx = static_cast<int>(8.0 + radius * std::cos(angle));
            int cz = static_cast<int>(8.0 + radius * std::sin(angle));
            s.pos = ClampToWorld({cx, 8, cz});
            // Velocity = tangential to circle.
            s.velocity = {static_cast<int>(-std::sin(angle) * 0.5),
                          0,
                          static_cast<int>(std::cos(angle) * 0.5)};
            break;
        }
        case SceneKind::fly_vertical: {
            // Vertical movement along Y axis (fly mode).
            s.velocity = {0, 1, 0};
            s.pos = ClampToWorld({8, static_cast<int>(s.t * 0.3) % 14 + 1, 8});
            break;
        }
        case SceneKind::spiral_in: {
            // Spiral toward center (8, 8, 8), radius decreasing over time.
            double angle = s.t * 0.04;
            double radius = std::max(1.0, 8.0 - s.t * 0.01);
            int cx = static_cast<int>(8.0 + radius * std::cos(angle));
            int cz = static_cast<int>(8.0 + radius * std::sin(angle));
            s.pos = ClampToWorld({cx, 8, cz});
            s.velocity = {static_cast<int>(-radius * std::sin(angle) * 0.04),
                          0,
                          static_cast<int>(radius * std::cos(angle) * 0.04)};
            break;
        }
        default: break;
    }
    return s;
}

// ============================================================================
// Simulation harness
// ============================================================================

struct SimConfig {
    StrategyKind strategy;
    SceneKind scene;
    std::uint32_t seed;
    std::uint64_t warmup_frames;
    std::uint64_t measure_frames;
};

struct SimResult {
    std::string strategy;
    std::string scene;
    std::uint32_t seed;
    // Per-frame metrics.
    std::vector<double> stutter_us_per_frame;  // main thread block time
    std::vector<double> background_us_per_frame;
    std::vector<double> peak_vram_bytes_per_frame;
    std::vector<double> peak_ram_bytes_per_frame;
    std::vector<double> ssd_load_count_per_frame;
    std::size_t total_chunks_loaded = 0;
};

SimResult RunSimulation(const SimConfig& cfg, MemoryTiers tiers) {
    Strategy strat;
    strat.init(cfg.strategy, tiers);
    strat.on_startup();

    std::mt19937 rng(cfg.seed);
    MovementState mov{};
    SimResult res{};
    res.strategy = std::string(StrategyName(cfg.strategy));
    res.scene = std::string(SceneName(cfg.scene));
    res.seed = cfg.seed;

    std::uint64_t total_frames = cfg.warmup_frames + cfg.measure_frames;
    for (std::uint64_t f = 0; f < total_frames; ++f) {
        mov = StepMovement(cfg.scene, mov, f, rng);
        CameraState cam{mov.pos, mov.velocity};
        FrameResult fr = strat.on_frame(cam, f);
        strat.post_frame();
        res.total_chunks_loaded += fr.chunks_loaded;

        if (f >= cfg.warmup_frames) {
            res.stutter_us_per_frame.push_back(fr.main_thread_block_us);
            res.background_us_per_frame.push_back(fr.background_load_us);
            res.peak_vram_bytes_per_frame.push_back(
                static_cast<double>(strat.store.tier_bytes[static_cast<int>(ChunkTier::VRAM)]));
            res.peak_ram_bytes_per_frame.push_back(
                static_cast<double>(strat.store.tier_bytes[static_cast<int>(ChunkTier::RAM)]));
            res.ssd_load_count_per_frame.push_back(
                fr.chunks_loaded > 0 && fr.main_thread_block_us > 5.0 ? 1.0 : 0.0);
        }
    }
    return res;
}

// ============================================================================
// Main + CSV output
// ============================================================================

struct Args {
    std::uint64_t warmup = 10;
    std::uint64_t frames = 1000;
    fs::path output = "results.csv";
    bool verbose = false;
};

Args ParseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--warmup" && i + 1 < argc) a.warmup = std::stoull(argv[++i]);
        else if (arg == "--frames" && i + 1 < argc) a.frames = std::stoull(argv[++i]);
        else if (arg == "--output" && i + 1 < argc) a.output = argv[++i];
        else if (arg == "--verbose") a.verbose = true;
    }
    return a;
}

int main(int argc, char** argv) {
    Args args = ParseArgs(argc, argv);
    MemoryTiers tiers{};

    std::ofstream csv(args.output);
    csv << "strategy,scene,seed,stutter_us_mean,stutter_us_p99,stutter_us_max,"
        << "bg_us_mean,bg_us_p99,"
        << "vram_mb_mean,vram_mb_max,"
        << "ram_mb_mean,ram_mb_max,"
        << "ssd_loads_total,chunks_loaded_total,frames\n";

    constexpr std::array<StrategyKind, 5> strategies = {
        StrategyKind::A_PrebakeAll,
        StrategyKind::B_FixedRing,
        StrategyKind::C_PredictiveStreaming,
        StrategyKind::D_DemandPaging,
        StrategyKind::E_HybridDemandPredictive,
    };
    constexpr std::array<SceneKind, 5> scenes = {
        SceneKind::linear_walk,
        SceneKind::teleport_stress,
        SceneKind::orbit_center,
        SceneKind::fly_vertical,
        SceneKind::spiral_in,
    };
    constexpr std::array<std::uint32_t, 5> seeds = {1, 7, 42, 1234, 31337};

    std::size_t total_configs = strategies.size() * scenes.size() * seeds.size();
    std::size_t config_idx = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (auto strat : strategies) {
        for (auto scene : scenes) {
            for (auto seed : seeds) {
                ++config_idx;
                SimConfig cfg{strat, scene, seed, args.warmup, args.frames};
                SimResult res = RunSimulation(cfg, tiers);

                Stats st_stutter = ComputeStats(res.stutter_us_per_frame);
                Stats st_bg = ComputeStats(res.background_us_per_frame);
                Stats st_vram = ComputeStats(res.peak_vram_bytes_per_frame);
                Stats st_ram = ComputeStats(res.peak_ram_bytes_per_frame);

                double ssd_total = 0;
                for (double v : res.ssd_load_count_per_frame) ssd_total += v;
                double vram_mb = 1024.0 * 1024.0;
                double ram_mb = 1024.0 * 1024.0;

                csv << std::format("{},{},{},{:.3f},{:.3f},{:.3f},"
                                   "{:.3f},{:.3f},"
                                   "{:.3f},{:.3f},"
                                   "{:.3f},{:.3f},"
                                   "{:.0f},{},{}\n",
                                   res.strategy, res.scene, res.seed,
                                   st_stutter.mean, st_stutter.p99, st_stutter.maxv,
                                   st_bg.mean, st_bg.p99,
                                   st_vram.mean / vram_mb, st_vram.maxv / vram_mb,
                                   st_ram.mean / ram_mb, st_ram.maxv / ram_mb,
                                   ssd_total, res.total_chunks_loaded,
                                   args.frames);

                if (args.verbose) {
                    std::printf("[%zu/%zu] %s x %s seed=%u: "
                                "stutter_mean=%.2f us p99=%.2f us max=%.2f us | "
                                "bg_mean=%.2f us | "
                                "vram_max=%.1f MiB ram_max=%.1f MiB | "
                                "ssd=%zu chunks=%zu\n",
                                config_idx, total_configs,
                                res.strategy.c_str(), res.scene.c_str(), res.seed,
                                st_stutter.mean, st_stutter.p99, st_stutter.maxv,
                                st_bg.mean,
                                st_vram.maxv / vram_mb, st_ram.maxv / ram_mb,
                                static_cast<std::size_t>(ssd_total), res.total_chunks_loaded);
                }
            }
        }
    }

    auto t_end = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t_end - t_start).count();

    std::printf("Done: %zu configs x %zu frames = %zu main measurements in %.2f sec\n",
                total_configs, args.frames, total_configs * args.frames, wall_sec);
    std::printf("Output: %s\n", args.output.string().c_str());
    return 0;
}
