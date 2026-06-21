// defrag_bench.cpp — standalone C++26 CPU VMA fragmentation simulator v2.
//
// v2: realistic random alloc/free pattern with size variance + forced
// fragmentation scenario. v1 used first-fit append which prevented any
// fragmentation. v2 uses worst-fit (defrag-friendly) baseline to show
// fragmentation cost savings.
//
// Models a synthetic 8 GiB VRAM heap (matches dev host `obvium` RTX 3060 Ti per
// docs/experiments/hardware-profile.md §3) with 5 allocation strategies.
//
// Measures:
//   - peak_VRAM_MiB        (in-use bytes peak; fragmentation increases
//                           this if heap can't compact back)
//   - heap_used_MiB        (mean bytes actually used)
//   - fragmentation_ratio  (1 - largest_contiguous_block / total_free, 0..1)
//   - per-frame_defrag_ms  (simulated cost based on bytes_moved * mov_factor)
//   - stutter_frames       (frames with defrag cost > 2 ms threshold)
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//     -o build/defrag_bench defrag_bench.cpp
//
// Run:
//   ./build/defrag_bench
//
// Output:
//   build/results.csv — 500 rows = 1 header + 500 main measurements
//     (5 strategies × 5 scenes × 4 alloc patterns × 5 seeds × 1 = 500)
//
// Reference: docs/experiments/experiments/2026-06-21-vulkan-defragmentation-compaction/
// License: standalone prototype, NOT ProjectV mainline.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr size_t HEAP_BYTES              = 2ULL * 1024 * 1024 * 1024;  // 2 GiB (Stage 4.3 projected)
constexpr size_t ALIGN_BYTES             = 256;
constexpr double DEFRAG_MOV_MS_PER_GIB   = 2.0;   // realistic GPU copy cost
constexpr double STUTTER_MS_THRESHOLD    = 2.0;   // 6% of 33.3 ms frame budget
constexpr double FRAG_THRESHOLD          = 0.4;
constexpr int    PERIODIC_FULL_INTERVAL  = 300;
constexpr size_t INCREMENTAL_BUDGET_BYTES = 8ULL * 1024 * 1024;  // 8 MiB cap

enum class Strategy : int {
    A_None                = 0,
    B_PeriodicFull        = 1,
    C_IncrementalBudgeted = 2,
    D_OnDemandThreshold   = 3,
    E_BudgetedOnDemand    = 4,
};
constexpr int STRATEGY_COUNT = 5;

enum class AllocPattern : int {
    ChunkPersistent = 0,
    TransientRing   = 1,
    JITLoadedChunk  = 2,
    BlasPoolAlloc   = 3,
};
constexpr int ALLOC_PATTERN_COUNT = 4;
constexpr int SCENE_COUNT = 5;

struct LiveBlock {
    int       id;
    int       gen;          // generation when allocated (for matching frees)
    size_t    size;
    size_t    heap_offset;  // byte offset within heap
    bool      live;
};

struct Heap {
    std::vector<LiveBlock> blocks;
    size_t total_bytes_moved = 0;
    int    stutter_frames    = 0;
    int    alloc_failures    = 0;  // count of OOM-skip allocs (heap_below_budget=false)
    int    alloc_attempts    = 0;  // total alloc attempts made by workload
};

size_t align_up(size_t v, size_t a) { return ((v + a - 1) / a) * a; }

// Place allocation in any free gap, choosing BEST-FIT (VMA default per
// VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT) to produce realistic
// fragmentation pattern: best-fit leaves small holes between allocations,
// defrag can compact them. Worst-fit would always pick the largest gap and
// produce zero fragmentation.
size_t place_alloc_worst_fit(Heap& h, int id, int gen, size_t size) {
    size_t aligned = align_up(size, ALIGN_BYTES);
    if (aligned == 0 || aligned > HEAP_BYTES) return SIZE_MAX;

    std::vector<size_t> starts;
    starts.reserve(h.blocks.size() + 2);
    starts.push_back(0);
    for (const auto& b : h.blocks) {
        if (b.live) starts.push_back(b.heap_offset);
    }
    std::sort(starts.begin(), starts.end());

    struct Gap { size_t off; size_t sz; };
    std::vector<Gap> gaps;
    for (size_t i = 0; i + 1 < starts.size(); ++i) {
        size_t off = starts[i];
        size_t end = starts[i + 1];
        if (end > off) gaps.push_back({off, end - off});
    }
    if (starts.back() < HEAP_BYTES) {
        gaps.push_back({starts.back(), HEAP_BYTES - starts.back()});
    }

    // Best-fit: pick smallest gap that still fits (VMA default).
    // This produces realistic fragmentation: small leftovers between allocations.
    const Gap* best = nullptr;
    for (const auto& g : gaps) {
        if (g.sz < aligned) continue;
        if (!best || g.sz < best->sz) best = &g;
    }
    if (!best) return SIZE_MAX;

    h.blocks.push_back({id, gen, size, best->off, true});
    return best->off;
}

void free_alloc(Heap& h, int id, int gen) {
    for (auto& b : h.blocks) {
        if (b.id == id && b.gen == gen && b.live) { b.live = false; return; }
    }
}

size_t bytes_in_use(const Heap& h) {
    size_t s = 0;
    for (const auto& b : h.blocks) if (b.live) s += b.size;
    return s;
}

size_t total_free_bytes(const Heap& h) {
    return HEAP_BYTES - bytes_in_use(h);
}

size_t largest_contiguous_free(const Heap& h) {
    std::vector<size_t> starts;
    starts.reserve(h.blocks.size() + 1);
    starts.push_back(0);
    for (const auto& b : h.blocks) if (b.live) starts.push_back(b.heap_offset);
    std::sort(starts.begin(), starts.end());
    size_t largest = 0;
    size_t prev = 0;
    for (size_t off : starts) {
        if (off > prev) largest = std::max(largest, off - prev);
        prev = std::max(prev, off);
    }
    if (prev < HEAP_BYTES) largest = std::max(largest, HEAP_BYTES - prev);
    return largest;
}

double fragmentation_ratio(const Heap& h) {
    size_t free = total_free_bytes(h);
    if (free == 0) return 0.0;
    size_t largest = largest_contiguous_free(h);
    if (largest > free) largest = free;
    return 1.0 - static_cast<double>(largest) / static_cast<double>(free);
}

// Defrag pass: compact live blocks to lowest offsets. Returns bytes moved.
size_t defrag_pass(Heap& h, size_t max_bytes_to_move, size_t& bytes_moved_out) {
    bytes_moved_out = 0;
    // Collect live blocks sorted by current offset (stable).
    std::vector<LiveBlock*> live;
    for (auto& b : h.blocks) if (b.live) live.push_back(&b);
    std::sort(live.begin(), live.end(),
        [](const LiveBlock* a, const LiveBlock* b) {
            return a->heap_offset < b->heap_offset;
        });
    size_t cursor = 0;
    for (auto* b : live) {
        size_t aligned_size = align_up(b->size, ALIGN_BYTES);
        if (b->heap_offset != cursor && (bytes_moved_out + aligned_size) <= max_bytes_to_move) {
            bytes_moved_out += aligned_size;
            b->heap_offset = cursor;
        }
        cursor = std::max(cursor, b->heap_offset + aligned_size);
    }
    h.total_bytes_moved += bytes_moved_out;
    return bytes_moved_out;
}

double ms_for_moved_bytes(size_t bytes_moved) {
    return static_cast<double>(bytes_moved) / (1024.0 * 1024.0 * 1024.0)
           * DEFRAG_MOV_MS_PER_GIB;
}

// Build per-frame ops for a given pattern + RNG.
// Creates realistic fragmentation pattern: alloc random small + large blocks,
// free random older blocks to create holes, then allocate new ones.
void build_frame_ops(
    AllocPattern pat, std::mt19937_64& rng, int frame, int& next_id,
    std::vector<std::tuple<int, int, size_t>>& allocs,   // id, gen, size
    std::vector<std::pair<int, int>>& frees              // id, gen
) {
    std::uniform_int_distribution<int> coin(0, 99);
    std::uniform_int_distribution<int> small_kb(4, 64);
    std::uniform_int_distribution<int> med_kb(64, 1024);
    std::uniform_int_distribution<int> large_kb(1024, 4096);

    auto alloc_one = [&](size_t sz) {
        allocs.emplace_back(next_id, frame, sz);
        ++next_id;
    };
    auto free_one = [&](int back) {
        if (next_id > back) {
            frees.emplace_back(next_id - back, frame - back);
        }
    };

    if (pat == AllocPattern::ChunkPersistent) {
        // Rare adds, never freed.
        if (frame % 50 == 0 && coin(rng) < 40) alloc_one(8 * 1024);
    } else if (pat == AllocPattern::TransientRing) {
        // Per-frame: alloc 1-4 random + free same count older (creates churn).
        int n = 1 + coin(rng) % 4;
        for (int i = 0; i < n; ++i) {
            int r = coin(rng);
            if (r < 60) alloc_one(static_cast<size_t>(small_kb(rng)) * 1024);
            else if (r < 95) alloc_one(static_cast<size_t>(med_kb(rng)) * 1024);
            else alloc_one(static_cast<size_t>(large_kb(rng)) * 1024);
        }
        for (int i = 0; i < n; ++i) free_one(2 + coin(rng) % 5);
    } else if (pat == AllocPattern::JITLoadedChunk) {
        // Bursty: 0-3 chunks per frame.
        int n = coin(rng) % 4;
        for (int i = 0; i < n; ++i) alloc_one(64 * 1024);
    } else if (pat == AllocPattern::BlasPoolAlloc) {
        // Alloc per chunk rebuild.
        if (frame % 5 == 0 && coin(rng) < 50) {
            alloc_one(512 * 1024);
            if (frame > 50) free_one(60);
        }
    }
}

void run_simulation(
    Strategy strat, AllocPattern pat, int scene_idx, int seed,
    int warmup_frames, int measure_frames,
    double& peak_vram_mib_out, double& heap_used_mean_mib_out,
    double& frag_ratio_mean_out, double& defrag_p99_ms_out,
    int& stutter_frames_out, double& alloc_failure_rate_out
) {
    Heap h;
    std::mt19937_64 rng(static_cast<uint64_t>(seed) * 7919
                        + static_cast<int>(pat) * 131 + scene_idx * 9973);
    int next_id = 1;

    size_t peak_in_use = 0;
    size_t used_sum = 0;
    double frag_sum = 0.0;
    std::vector<double> per_frame_defrag_ms;
    per_frame_defrag_ms.reserve(measure_frames);

    auto apply_frame = [&](int frame) {
        std::vector<std::tuple<int, int, size_t>> allocs;
        std::vector<std::pair<int, int>> frees;
        build_frame_ops(pat, rng, frame, next_id, allocs, frees);

        // Apply frees FIRST (creates holes).
        for (auto& [id, gen] : frees) free_alloc(h, id, gen);

        // Apply defrag BEFORE alloc attempts (so defrag can compact holes for
        // new allocations, which is the realistic scenario).
        size_t bytes_moved = 0;
        double defrag_ms = 0.0;
        switch (strat) {
            case Strategy::A_None: break;
            case Strategy::B_PeriodicFull:
                if (frame > 0 && frame % PERIODIC_FULL_INTERVAL == 0) {
                    defrag_pass(h, SIZE_MAX, bytes_moved);
                    defrag_ms = ms_for_moved_bytes(bytes_moved);
                }
                break;
            case Strategy::C_IncrementalBudgeted:
                defrag_pass(h, INCREMENTAL_BUDGET_BYTES, bytes_moved);
                defrag_ms = ms_for_moved_bytes(bytes_moved);
                break;
            case Strategy::D_OnDemandThreshold:
                if (fragmentation_ratio(h) > FRAG_THRESHOLD) {
                    defrag_pass(h, SIZE_MAX, bytes_moved);
                    defrag_ms = ms_for_moved_bytes(bytes_moved);
                }
                break;
            case Strategy::E_BudgetedOnDemand:
                if (fragmentation_ratio(h) > FRAG_THRESHOLD) {
                    defrag_pass(h, INCREMENTAL_BUDGET_BYTES, bytes_moved);
                    defrag_ms = ms_for_moved_bytes(bytes_moved);
                }
                break;
        }

        // Then attempt allocations. Real OOM = no contiguous hole of required
        // size (place_alloc returns SIZE_MAX). No early budget gate — let
        // fragmentation show through naturally.
        for (auto& [id, gen, sz] : allocs) {
            h.alloc_attempts++;
            if (place_alloc_worst_fit(h, id, gen, sz) == SIZE_MAX) {
                h.alloc_failures++;
            }
        }

        size_t in_use = bytes_in_use(h);
        peak_in_use = std::max(peak_in_use, in_use);
        used_sum += in_use;

        per_frame_defrag_ms.push_back(defrag_ms);
        if (defrag_ms > STUTTER_MS_THRESHOLD) h.stutter_frames++;
        frag_sum += fragmentation_ratio(h);
    };

    for (int f = 0; f < warmup_frames; ++f) apply_frame(f);
    per_frame_defrag_ms.clear();
    for (int f = 0; f < measure_frames; ++f) apply_frame(warmup_frames + f);

    peak_vram_mib_out   = static_cast<double>(peak_in_use) / (1024.0 * 1024.0);
    heap_used_mean_mib_out = static_cast<double>(used_sum) / static_cast<double>(measure_frames)
                              / (1024.0 * 1024.0);
    frag_ratio_mean_out = frag_sum / static_cast<double>(measure_frames);

    std::sort(per_frame_defrag_ms.begin(), per_frame_defrag_ms.end());
    if (per_frame_defrag_ms.empty()) {
        defrag_p99_ms_out = 0.0;
    } else {
        size_t idx = per_frame_defrag_ms.size() * 99 / 100;
        defrag_p99_ms_out = per_frame_defrag_ms[idx];
    }
    stutter_frames_out = h.stutter_frames;
    if (h.alloc_attempts > 0) {
        alloc_failure_rate_out = static_cast<double>(h.alloc_failures)
                                  / static_cast<double>(h.alloc_attempts);
    } else {
        alloc_failure_rate_out = 0.0;
    }
}

}  // namespace

int main() {
    const int warmup_frames  = 10;
    const int measure_frames = 1000;
    const int seeds[] = {1, 7, 42, 1234, 31337};

    std::ofstream csv("build/results.csv");
    csv << "strategy,scene,alloc_pattern,seed,peak_vram_mib,heap_used_mean_mib,"
           "frag_ratio_mean,defrag_p99_ms,stutter_frames,alloc_failure_rate\n";

    const char* strat_names[STRATEGY_COUNT] = {
        "A_None", "B_PeriodicFull", "C_IncrementalBudgeted",
        "D_OnDemandThreshold", "E_BudgetedOnDemand"
    };
    const char* scene_names[SCENE_COUNT] = {
        "uniform_floor", "forest_floor", "cave_stress", "mixed_biome", "uniform_air"
    };
    const char* alloc_names[ALLOC_PATTERN_COUNT] = {
        "ChunkPersistent", "TransientRing", "JITLoadedChunk", "BlasPoolAlloc"
    };

    auto t0 = std::chrono::steady_clock::now();
    int row = 0;
    for (int si = 0; si < STRATEGY_COUNT; ++si) {
        for (int scene = 0; scene < SCENE_COUNT; ++scene) {
            for (int ai = 0; ai < ALLOC_PATTERN_COUNT; ++ai) {
                for (int seed : seeds) {
                    double peak, used, frag, p99, fail_rate;
                    int stutter;
                    run_simulation(
                        static_cast<Strategy>(si), static_cast<AllocPattern>(ai),
                        scene, seed, warmup_frames, measure_frames,
                        peak, used, frag, p99, stutter, fail_rate
                    );
                    csv << strat_names[si] << "," << scene_names[scene] << ","
                        << alloc_names[ai] << "," << seed << ","
                        << peak << "," << used << "," << frag << ","
                        << p99 << "," << stutter << "," << fail_rate << "\n";
                    ++row;
                }
            }
        }
    }
    csv.close();
    auto t1 = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t1 - t0).count();
    std::printf("Wrote build/results.csv with %d rows in %.2f s wall.\n", row, wall_sec);
    return 0;
}
