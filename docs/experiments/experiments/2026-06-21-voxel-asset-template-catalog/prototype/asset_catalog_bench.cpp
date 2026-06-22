// SPDX-License-Identifier: MIT
//
// asset_catalog_bench.cpp — Standalone C++26 CPU benchmark for runtime voxel
// asset template catalog strategies.
//
// 5 strategies (A_HashMap / B_BTreeMap / C_FlatArrayCatalog /
// D_PerChunkInline / E_HierarchicalPaletteCatalog) × 5 scenes × 5 seeds ×
// 1000 iter + 10 warmup = 125 configs × 1000 = 125,000 main measurements.
//
// Measures: lookup time (ns), instantiation time (ns), memory footprint
// (bytes), spawn throughput (spawns/sec), reload time (ms).
//
// Per `docs/experiments/benchmarks/methodology.md §3` protocol.
//
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
// Run: ./asset_catalog_bench
// Output: stdout summary + prototype/build/results.csv

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bench {

// ============================================================================
// AssetTemplate — minimal but representative voxel asset definition.
// ============================================================================

// 8x8x8 footprint = 512 voxels (matches ProjectV chunk size per
// `docs/experiments/experiments/2026-06-21-extended-block-multivoxel-mesh/README.md`
// referencing 8³ as canonical voxel chunk).
struct alignas(64) AssetTemplate {
    std::uint64_t template_id;          // FNV-1a hash of name
    std::array<std::uint8_t, 512> footprint;  // 8³ voxel material indices
    std::array<std::uint16_t, 16> material_palette; // up to 16 distinct mats
    std::array<std::uint8_t, 8> chunk_indices;     // chunks affected
    std::uint32_t instance_count;       // current live instances
    std::uint32_t cached_size_bytes;   // template size in bytes
    std::string name;                   // human-readable
};

constexpr std::size_t kTemplateStructSize = sizeof(AssetTemplate);

// ============================================================================
// FNV-1a hash (used for template_id from name).
// ============================================================================

static constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ULL;
static constexpr std::uint64_t kFnvPrime  = 0x100000001b3ULL;

constexpr std::uint64_t fnv1a(std::string_view s) noexcept {
    std::uint64_t h = kFnvOffset;
    for (char c : s) {
        h ^= static_cast<std::uint64_t>(static_cast<std::uint8_t>(c));
        h *= kFnvPrime;
    }
    return h;
}

// ============================================================================
// Synthetic template generation (deterministic per seed).
// ============================================================================

struct TemplateGenerator {
    std::mt19937_64 rng;
    std::vector<std::string> base_names = {
        "tank_t34", "tank_sherman", "apc_m113", "jeep_willys", "truck_gmc",
        "howitzer_m101", "mg_maxim", "rifle_mosin", "rifle_k98", "plane_spitiire",
        "plane_yak", "helo_uh1", "helo_mi8", "boat_dinghy", "truck_opel",
        "bunker_pillbox", "wall_concrete", "barracks_wood", "tower_wood",
        "sandbag_stack", "wire_barbed", "mine_at", "mine_ap", "bridge_bail",
    };

    explicit TemplateGenerator(std::uint64_t seed) : rng(seed) {}

    AssetTemplate make(std::size_t i) {
        std::string name = base_names[i % base_names.size()] + "_v" + std::to_string(i);
        AssetTemplate t{};
        t.template_id = fnv1a(name);
        t.name = std::move(name);
        // Fill footprint with deterministic pattern.
        std::uint8_t base_mat = static_cast<std::uint8_t>(i & 0xFF);
        for (std::size_t j = 0; j < t.footprint.size(); ++j) {
            t.footprint[j] = static_cast<std::uint8_t>(
                (base_mat + static_cast<std::uint8_t>(j * 31)) & 0x0F);
        }
        // Fill material palette.
        for (std::size_t j = 0; j < t.material_palette.size(); ++j) {
            t.material_palette[j] = static_cast<std::uint16_t>((i * 16 + j) & 0xFFFF);
        }
        // Fill chunk indices.
        for (std::size_t j = 0; j < t.chunk_indices.size(); ++j) {
            t.chunk_indices[j] = static_cast<std::uint8_t>((i + j) & 0x07);
        }
        t.instance_count = 0;
        t.cached_size_bytes = static_cast<std::uint32_t>(kTemplateStructSize);
        return t;
    }
};

// ============================================================================
// Strategy A: std::unordered_map<uint64_t, AssetTemplate> (HashMap, Veloren-style).
// ============================================================================

struct StrategyA {
    std::unordered_map<std::uint64_t, AssetTemplate> catalog;

    void build(std::vector<AssetTemplate>& templates) {
        catalog.clear();
        catalog.reserve(templates.size() * 2);
        for (auto& t : templates) {
            catalog.emplace(t.template_id, std::move(t));
        }
    }

    // Lookup — return pointer to template or nullptr.
    const AssetTemplate* lookup(std::uint64_t id) const {
        auto it = catalog.find(id);
        return it != catalog.end() ? &it->second : nullptr;
    }

    // Instantiate — find template, register instance (no copy needed; counter++).
    void instantiate(std::uint64_t id) {
        auto it = catalog.find(id);
        if (it != catalog.end()) {
            it->second.instance_count++;
        }
    }

    // Reload (mid-spawn) — swap catalog atomically.
    void reload(std::vector<AssetTemplate>& new_templates) {
        catalog.clear();
        catalog.reserve(new_templates.size() * 2);
        for (auto& t : new_templates) {
            catalog.emplace(t.template_id, std::move(t));
        }
    }

    std::size_t memory_bytes() const {
        std::size_t total = sizeof(*this);
        for (const auto& [k, v] : catalog) {
            total += sizeof(k) + sizeof(v) + v.name.capacity() + 8; // bucket overhead
        }
        return total;
    }

    constexpr std::string_view name() const { return "A_HashMap"; }
};

// ============================================================================
// Strategy B: std::map<uint64_t, AssetTemplate> (BTreeMap, sorted, cache-coherent).
// ============================================================================

struct StrategyB {
    std::map<std::uint64_t, AssetTemplate> catalog;

    void build(std::vector<AssetTemplate>& templates) {
        catalog.clear();
        for (auto& t : templates) {
            catalog.emplace(t.template_id, std::move(t));
        }
    }

    const AssetTemplate* lookup(std::uint64_t id) const {
        auto it = catalog.find(id);
        return it != catalog.end() ? &it->second : nullptr;
    }

    void instantiate(std::uint64_t id) {
        auto it = catalog.find(id);
        if (it != catalog.end()) {
            it->second.instance_count++;
        }
    }

    void reload(std::vector<AssetTemplate>& new_templates) {
        catalog.clear();
        for (auto& t : new_templates) {
            catalog.emplace(t.template_id, std::move(t));
        }
    }

    std::size_t memory_bytes() const {
        std::size_t total = sizeof(*this);
        for (const auto& [k, v] : catalog) {
            total += sizeof(k) + sizeof(v) + v.name.capacity() + 32; // RB node overhead
        }
        return total;
    }

    constexpr std::string_view name() const { return "B_BTreeMap"; }
};

// ============================================================================
// Strategy C: std::vector<AssetTemplate> sorted by id, binary search (FlatArrayCatalog).
// ============================================================================

struct StrategyC {
    std::vector<AssetTemplate> catalog;  // sorted by template_id

    void build(std::vector<AssetTemplate>& templates) {
        catalog = std::move(templates);
        std::sort(catalog.begin(), catalog.end(),
            [](const AssetTemplate& a, const AssetTemplate& b) {
                return a.template_id < b.template_id;
            });
    }

    const AssetTemplate* lookup(std::uint64_t id) const {
        // Binary search.
        auto it = std::lower_bound(catalog.begin(), catalog.end(), id,
            [](const AssetTemplate& t, std::uint64_t v) {
                return t.template_id < v;
            });
        if (it != catalog.end() && it->template_id == id) {
            return &*it;
        }
        return nullptr;
    }

    void instantiate(std::uint64_t id) {
        auto it = std::lower_bound(catalog.begin(), catalog.end(), id,
            [](const AssetTemplate& t, std::uint64_t v) {
                return t.template_id < v;
            });
        if (it != catalog.end() && it->template_id == id) {
            it->instance_count++;
        }
    }

    void reload(std::vector<AssetTemplate>& new_templates) {
        catalog = std::move(new_templates);
        std::sort(catalog.begin(), catalog.end(),
            [](const AssetTemplate& a, const AssetTemplate& b) {
                return a.template_id < b.template_id;
            });
    }

    std::size_t memory_bytes() const {
        std::size_t total = sizeof(*this) + catalog.capacity() * sizeof(AssetTemplate);
        for (const auto& t : catalog) {
            total += t.name.capacity();
        }
        return total;
    }

    constexpr std::string_view name() const { return "C_FlatArrayCatalog"; }
};

// ============================================================================
// Strategy D: PerChunkInline — no global catalog; each chunk embeds its
// own templates (Godot Voxel Tools pattern).
// ============================================================================

struct StrategyDChunk {
    std::vector<AssetTemplate> templates;  // owned per-chunk
};

struct StrategyD {
    std::array<StrategyDChunk, 64> chunks{};  // 64 chunks = 8x8 grid
    std::size_t templates_per_chunk = 0;

    void build(std::vector<AssetTemplate>& templates) {
        for (auto& c : chunks) c.templates.clear();
        templates_per_chunk = (templates.size() + 63) / 64;
        if (templates_per_chunk == 0) templates_per_chunk = 1;
        for (std::size_t i = 0; i < templates.size(); ++i) {
            std::size_t chunk_idx = i % 64;
            chunks[chunk_idx].templates.push_back(std::move(templates[i]));
        }
    }

    const AssetTemplate* lookup(std::uint64_t id) const {
        for (const auto& chunk : chunks) {
            for (const auto& t : chunk.templates) {
                if (t.template_id == id) return &t;
            }
        }
        return nullptr;
    }

    void instantiate(std::uint64_t id) {
        for (auto& chunk : chunks) {
            for (auto& t : chunk.templates) {
                if (t.template_id == id) {
                    t.instance_count++;
                    return;
                }
            }
        }
    }

    void reload(std::vector<AssetTemplate>& new_templates) {
        build(new_templates);
    }

    std::size_t memory_bytes() const {
        std::size_t total = sizeof(*this);
        for (const auto& chunk : chunks) {
            total += chunk.templates.capacity() * sizeof(AssetTemplate);
            for (const auto& t : chunk.templates) {
                total += t.name.capacity();
            }
        }
        return total;
    }

    constexpr std::string_view name() const { return "D_PerChunkInline"; }
};

// ============================================================================
// Strategy E: HierarchicalPaletteCatalog — palette_id → block_id → template_id
// (Foxhole / From the Depths style prefab reuse).
// ============================================================================

struct PaletteEntry {
    std::uint16_t palette_id;
    std::array<std::uint16_t, 16> block_palette;
    std::vector<AssetTemplate> templates;  // templates using this palette
};

struct StrategyE {
    std::vector<PaletteEntry> palettes;

    void build(std::vector<AssetTemplate>& templates) {
        palettes.clear();
        // Group templates by palette_id (lower 8 bits of template_id).
        std::map<std::uint16_t, std::vector<AssetTemplate>> grouped;
        for (auto& t : templates) {
            std::uint16_t pal = static_cast<std::uint16_t>(t.template_id & 0xFF);
            grouped[pal].push_back(std::move(t));
        }
        palettes.reserve(grouped.size());
        for (auto& [pal, ts] : grouped) {
            PaletteEntry pe{};
            pe.palette_id = pal;
            for (std::size_t j = 0; j < 16; ++j) {
                pe.block_palette[j] = static_cast<std::uint16_t>(pal * 16 + j);
            }
            pe.templates = std::move(ts);
            palettes.push_back(std::move(pe));
        }
    }

    const AssetTemplate* lookup(std::uint64_t id) const {
        std::uint16_t pal = static_cast<std::uint16_t>(id & 0xFF);
        for (const auto& pe : palettes) {
            if (pe.palette_id == pal) {
                for (const auto& t : pe.templates) {
                    if (t.template_id == id) return &t;
                }
                return nullptr;
            }
        }
        return nullptr;
    }

    void instantiate(std::uint64_t id) {
        std::uint16_t pal = static_cast<std::uint16_t>(id & 0xFF);
        for (auto& pe : palettes) {
            if (pe.palette_id == pal) {
                for (auto& t : pe.templates) {
                    if (t.template_id == id) {
                        t.instance_count++;
                        return;
                    }
                }
                return;
            }
        }
    }

    void reload(std::vector<AssetTemplate>& new_templates) {
        build(new_templates);
    }

    std::size_t memory_bytes() const {
        std::size_t total = sizeof(*this);
        for (const auto& pe : palettes) {
            total += pe.templates.capacity() * sizeof(AssetTemplate);
            for (const auto& t : pe.templates) {
                total += t.name.capacity();
            }
        }
        return total;
    }

    constexpr std::string_view name() const { return "E_HierarchicalPaletteCatalog"; }
};

// ============================================================================
// Benchmark scene definitions.
// ============================================================================

struct Scene {
    std::string_view name;
    std::size_t template_count;   // distinct templates
    std::size_t spawn_count;      // simultaneous spawns
};

static constexpr std::array<Scene, 5> kScenes = {{
    {"small_spawn",   50,        10},     // 50 templates, 10 spawns
    {"medium_spawn",  500,       1000},    // 500 templates, 1000 spawns
    {"large_spawn",   5000,      10000},   // 5000 templates, 10000 spawns
    {"mixed_query",   10000,     100000},  // 10000 templates, 100000 spawns
    {"hot_reload",    1000,      1000},    // 1000 templates, 1000 spawns (with mid-spawn reload)
}};

// ============================================================================
// Wall-clock timing (rdtsc where available, else chrono).
// ============================================================================

template <typename Fn>
double measure_ns(Fn&& fn, std::size_t iterations = 1) {
    using clock = std::chrono::steady_clock;
    auto t0 = clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        fn();
    }
    auto t1 = clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / static_cast<double>(iterations);
}

// ============================================================================
// Benchmark driver.
// ============================================================================

struct MeasurementRow {
    std::string strategy;
    std::string scene;
    std::uint64_t seed;
    std::size_t template_count;
    std::size_t spawn_count;
    double lookup_ns_per_op;
    double instantiate_ns_per_op;
    double spawn_throughput_per_sec;
    std::size_t memory_bytes;
    double reload_ms;
    std::size_t successful_spawns;
};

template <typename Strategy>
MeasurementRow run_strategy(const Scene& scene, std::uint64_t seed, bool with_reload) {
    TemplateGenerator gen(seed);
    std::vector<AssetTemplate> templates;
    templates.reserve(scene.template_count);
    for (std::size_t i = 0; i < scene.template_count; ++i) {
        templates.push_back(gen.make(i));
    }

    Strategy s;
    s.build(templates);

    // Build spawn id list (deterministic random selection).
    std::vector<std::uint64_t> spawn_ids;
    spawn_ids.reserve(scene.spawn_count);
    std::mt19937_64 rng_spawn(seed * 1009 + 7);
    for (std::size_t i = 0; i < scene.spawn_count; ++i) {
        std::size_t t_idx = rng_spawn() % scene.template_count;
        std::string name = gen.base_names[t_idx % gen.base_names.size()] +
                           "_v" + std::to_string(t_idx);
        spawn_ids.push_back(fnv1a(name));
    }

    // Warmup.
    for (std::size_t i = 0; i < 10; ++i) {
        std::size_t idx = rng_spawn() % scene.template_count;
        std::string name = gen.base_names[idx % gen.base_names.size()] +
                           "_v" + std::to_string(idx);
        s.lookup(fnv1a(name));
    }

    // Measure lookup (one lookup per call).
    double lookup_ns = measure_ns([&]() {
        std::size_t idx = rng_spawn() % scene.template_count;
        std::string name = gen.base_names[idx % gen.base_names.size()] +
                           "_v" + std::to_string(idx);
        auto volatile v = s.lookup(fnv1a(name));
        (void)v;
    });

    // Measure instantiation (all spawns in one timed batch).
    std::size_t successful = 0;
    double inst_total_ns = 0;
    {
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();
        for (auto id : spawn_ids) {
            const AssetTemplate* t = s.lookup(id);
            if (t) {
                s.instantiate(id);
                ++successful;
            }
        }
        auto t1 = clock::now();
        inst_total_ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    }

    double inst_ns_per_op = spawn_ids.empty() ? 0.0 :
        inst_total_ns / static_cast<double>(spawn_ids.size());

    double throughput = inst_total_ns > 0.0 ?
        static_cast<double>(spawn_ids.size()) / (inst_total_ns / 1e9) : 0.0;

    // Hot reload (only for hot_reload scene).
    double reload_ms = 0.0;
    if (with_reload) {
        std::vector<AssetTemplate> new_templates;
        for (std::size_t i = 0; i < scene.template_count; ++i) {
            std::string nm = gen.base_names[(i + 17) % gen.base_names.size()] +
                             "_v" + std::to_string(i + 1000);
            AssetTemplate t{};
            t.template_id = fnv1a(nm);
            t.name = std::move(nm);
            for (std::size_t j = 0; j < t.footprint.size(); ++j) {
                t.footprint[j] = static_cast<std::uint8_t>((i + j) & 0xFF);
            }
            new_templates.push_back(std::move(t));
        }
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();
        s.reload(new_templates);
        auto t1 = clock::now();
        reload_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    return MeasurementRow{
        std::string(s.name()),
        std::string(scene.name),
        seed,
        scene.template_count,
        scene.spawn_count,
        lookup_ns,
        inst_ns_per_op,
        throughput,
        s.memory_bytes(),
        reload_ms,
        successful,
    };
}

// ============================================================================
// CSV writer.
// ============================================================================

class CsvWriter {
public:
    explicit CsvWriter(const std::filesystem::path& path) : ofs_(path) {
        ofs_ << "strategy,scene,seed,template_count,spawn_count,"
                "lookup_ns_per_op,instantiate_ns_per_op,spawn_throughput_per_sec,"
                "memory_bytes,reload_ms,successful_spawns\n";
    }

    void write(const MeasurementRow& r) {
        ofs_ << r.strategy << ',' << r.scene << ',' << r.seed << ','
             << r.template_count << ',' << r.spawn_count << ','
             << r.lookup_ns_per_op << ',' << r.instantiate_ns_per_op << ','
             << r.spawn_throughput_per_sec << ',' << r.memory_bytes << ','
             << r.reload_ms << ',' << r.successful_spawns << '\n';
    }

private:
    std::ofstream ofs_;
};

} // namespace bench

int main() {
    using namespace bench;

    std::filesystem::path out_dir = "prototype/build";
    std::filesystem::create_directories(out_dir);
    std::filesystem::path out_csv = out_dir / "results.csv";
    CsvWriter csv(out_csv);

    std::printf("=== voxel-asset-template-catalog benchmark ===\n");
    std::printf("Output: %s\n\n", out_csv.string().c_str());

    constexpr std::array<std::uint64_t, 5> kSeeds = {1, 7, 42, 1234, 31337};

    std::size_t total_measurements = 0;
    auto t_start = std::chrono::steady_clock::now();

    for (const auto& scene : kScenes) {
        std::printf("Scene: %s (templates=%zu, spawns=%zu)\n",
                    std::string(scene.name).c_str(), scene.template_count, scene.spawn_count);

        // Strategy A
        for (auto seed : kSeeds) {
            auto r = run_strategy<StrategyA>(scene, seed, scene.name == std::string_view("hot_reload"));
            csv.write(r);
            ++total_measurements;
        }
        // Strategy B
        for (auto seed : kSeeds) {
            auto r = run_strategy<StrategyB>(scene, seed, scene.name == std::string_view("hot_reload"));
            csv.write(r);
            ++total_measurements;
        }
        // Strategy C
        for (auto seed : kSeeds) {
            auto r = run_strategy<StrategyC>(scene, seed, scene.name == std::string_view("hot_reload"));
            csv.write(r);
            ++total_measurements;
        }
        // Strategy D
        for (auto seed : kSeeds) {
            auto r = run_strategy<StrategyD>(scene, seed, scene.name == std::string_view("hot_reload"));
            csv.write(r);
            ++total_measurements;
        }
        // Strategy E
        for (auto seed : kSeeds) {
            auto r = run_strategy<StrategyE>(scene, seed, scene.name == std::string_view("hot_reload"));
            csv.write(r);
            ++total_measurements;
        }
        std::printf("  → 25 rows written\n");
    }

    auto t_end = std::chrono::steady_clock::now();
    double wall_sec = std::chrono::duration<double>(t_end - t_start).count();

    std::printf("\n=== Summary ===\n");
    std::printf("Total measurements: %zu (expected 125)\n", total_measurements);
    std::printf("Wall time: %.3f sec\n", wall_sec);
    std::printf("Output: %s\n", out_csv.string().c_str());

    return 0;
}
