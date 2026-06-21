#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <print>
#include <random>
#include <span>
#include <vector>

// ProjectV: chunkSize=8 → 512 voxels per chunk
constexpr int CHUNK_VOXELS = 512;
constexpr int SCENES = 5;
constexpr int STRATEGIES = 4;
constexpr int SEEDS = 5;
constexpr int ITER = 200;
constexpr int WARMUP = 5;
constexpr int GLOBAL_MAX_ID = 65535;
constexpr int MIN_BITS = 4;
constexpr int MAX_PALETTE_BITS = 13;

// ---------- scenes (voxel ID arrays) ----------
struct Scene {
    const char *name;
    std::array<uint16_t, CHUNK_VOXELS> data;
    int unique_types;
};

static Scene make_scene(const char *name, std::span<const uint16_t> ids, int count) {
    Scene s;
    s.name = name;
    std::fill(s.data.begin(), s.data.end(), uint16_t{0});
    for (int i = 0; i < count; ++i)
        s.data[i] = ids[i % ids.size()];
    // deterministically mix to avoid trivial RLE
    std::mt19937 rng(42);
    std::shuffle(s.data.begin(), s.data.end(), rng);
    auto tmp = s.data;
    std::ranges::sort(tmp);
    s.unique_types = (int)(std::unique(tmp.begin(), tmp.end()) - tmp.begin());
    return s;
}

static std::array<Scene, SCENES> make_scenes() {
    std::array<Scene, SCENES> scenes;
    scenes[0] = make_scene("uniform_air",    std::array<uint16_t,1>{0},                             512);
    scenes[1] = make_scene("uniform_floor",  std::array<uint16_t,2>{0, 1},                          256);
    scenes[2] = make_scene("forest_floor",   std::array<uint16_t,7>{0,1,2,3,4,5,6},                 73);
    scenes[3] = make_scene("cave_stress",    std::array<uint16_t,12>{0,1,2,3,4,5,6,7,8,9,10,11},   42);
    scenes[4] = make_scene("mixed_biome",    std::array<uint16_t,25>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24}, 20);
    for (auto &s : scenes) std::print("scene {}: {} unique types\n", s.name, s.unique_types);
    return scenes;
}

// ---------- Strategy A: Fixed16 (current mainline baseline) ----------
struct Fixed16 {
    uint16_t data[CHUNK_VOXELS];
    static constexpr auto name = "A_Fixed16";
    void build(std::span<const uint16_t> src) { memcpy(data, src.data(), CHUNK_VOXELS * 2); }
    int bytes() const { return CHUNK_VOXELS * 2; }
    uint16_t get(int i) const { return data[i]; }
    void set(int i, uint16_t v) { data[i] = v; }
};

// ---------- Strategy B: AdaptivePalette (Minecraft 1.12 style) ----------
struct AdaptivePalette {
    static constexpr auto name = "B_AdaptivePalette";
    uint16_t palette[1 << MAX_PALETTE_BITS];
    int palette_size = 0;
    int bits = MIN_BITS;
    uint64_t storage[CHUNK_VOXELS * MAX_PALETTE_BITS / 64 + 1]{};

    void build(std::span<const uint16_t> src) {
        palette_size = 0;
        bits = MIN_BITS;
        memset(storage, 0, sizeof(storage));
        for (int i = 0; i < CHUNK_VOXELS; ++i) {
            int idx = find_or_add(src[i]);
            set_index(i, idx);
        }
    }

    int find_or_add(uint16_t v) {
        for (int i = 0; i < palette_size; ++i)
            if (palette[i] == v) return i;
        if (palette_size >= (1 << bits)) grow();
        palette[palette_size] = v;
        return palette_size++;
    }

    void grow() {
        int old_bits = bits;
        bits = std::min(bits + 1, MAX_PALETTE_BITS);
        if (bits == old_bits) return;
        uint64_t new_storage[CHUNK_VOXELS * MAX_PALETTE_BITS / 64 + 1]{};
        for (int i = 0; i < CHUNK_VOXELS; ++i) {
            int idx = get_index(i, old_bits);
            set_index_in(i, idx, bits, new_storage);
        }
        memcpy(storage, new_storage, sizeof(storage));
    }

    int bytes() const {
        int pal_bytes = palette_size * 2;
        int data_bytes = (CHUNK_VOXELS * bits + 7) / 8;
        return pal_bytes + data_bytes;
    }

    uint16_t get(int i) const {
        int idx = get_index(i, bits);
        return idx < palette_size ? palette[idx] : 0;
    }

    void set(int i, uint16_t v) {
        int idx = find_or_add(v);
        set_index(i, idx);
    }

    int get_index(int i, int b) const {
        int bit = i * b;
        int word = bit / 64;
        int off = bit % 64;
        uint64_t mask = (1ULL << b) - 1;
        uint64_t val = storage[word] >> off;
        if (off + b > 64)
            val |= storage[word + 1] << (64 - off);
        return (int)(val & mask);
    }

    void set_index(int i, int idx) { set_index_in(i, idx, bits, storage); }
    void set_index_in(int i, int idx, int b, uint64_t *store) const {
        int bit = i * b;
        int word = bit / 64;
        int off = bit % 64;
        uint64_t mask = (1ULL << b) - 1;
        store[word] = (store[word] & ~(mask << off)) | ((uint64_t)idx & mask) << off;
        if (off + b > 64)
            store[word + 1] = (store[word + 1] & ~(mask >> (64 - off))) | ((uint64_t)idx & mask) >> (64 - off);
    }
};

// ---------- Strategy C: SingleStateOpt ----------
struct SingleStateOpt {
    static constexpr auto name = "C_SingleStateOpt";
    AdaptivePalette inner;
    bool single = false;
    uint16_t single_val = 0;

    void build(std::span<const uint16_t> src) {
        uint16_t first = src[0];
        bool all_same = true;
        for (auto v : src) if (v != first) { all_same = false; break; }
        if (all_same) {
            single = true;
            single_val = first;
        } else {
            single = false;
            inner.build(src);
        }
    }

    int bytes() const {
        if (single) return 2; // just the value
        return inner.bytes();
    }

    uint16_t get(int) const {
        if (single) return single_val;
        return inner.get(0);
    }
    void set(int, uint16_t) { /* no-op for bench */ }
};

// ---------- Strategy D: Direct8 (fixed 8-bit palette, up to 256 types) ----------
struct Direct8 {
    static constexpr auto name = "D_Direct8";
    uint16_t palette[256];
    int palette_size = 0;
    uint8_t data[CHUNK_VOXELS]{};

    void build(std::span<const uint16_t> src) {
        palette_size = 0;
        memset(data, 0, CHUNK_VOXELS);
        for (int i = 0; i < CHUNK_VOXELS; ++i) {
            int idx = -1;
            for (int j = 0; j < palette_size; ++j)
                if (palette[j] == src[i]) { idx = j; break; }
            if (idx < 0) {
                idx = palette_size;
                palette[palette_size++] = src[i];
            }
            data[i] = (uint8_t)idx;
        }
    }

    int bytes() const {
        return palette_size * 2 + CHUNK_VOXELS;
    }

    uint16_t get(int i) const {
        int idx = data[i];
        return idx < palette_size ? palette[idx] : 0;
    }

    void set(int i, uint16_t v) {
        int idx = -1;
        for (int j = 0; j < palette_size; ++j)
            if (palette[j] == v) { idx = j; break; }
        if (idx < 0) {
            if (palette_size >= 256) return;
            idx = palette_size++;
            palette[idx] = v;
        }
        data[i] = (uint8_t)idx;
    }
};

// ---------- harness ----------
template<typename S>
static void bench_strategy(const Scene &scene, int seed, std::vector<double> &out_bytes,
                           std::vector<double> &out_lookup, std::vector<double> &out_mutate)
{
    std::mt19937 rng(seed);
    S strat;
    strat.build(scene.data);

    // bytes
    out_bytes.push_back((double)strat.bytes());

    // lookup × 500
    std::array<int, 500> indices;
    for (auto &ix : indices) ix = rng() % CHUNK_VOXELS;
    auto t0 = std::chrono::steady_clock::now();
    for (int iter = 0; iter < ITER; ++iter)
        for (auto ix : indices) volatile uint16_t v = strat.get(ix);
    auto t1 = std::chrono::steady_clock::now();
    double ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / (ITER * 500.0);
    out_lookup.push_back(ns);

    // mutate: replace each voxel, cycling palette to test set perf
    std::array<uint16_t, CHUNK_VOXELS> new_vals;
    for (auto &v : new_vals) v = (uint16_t)(rng() % 100 + 50);
    S ms;
    ms.build(scene.data);
    t0 = std::chrono::steady_clock::now();
    for (int iter = 0; iter < 20; ++iter) {
        for (int i = 0; i < CHUNK_VOXELS; ++i)
            ms.set(i, (uint16_t)(new_vals[(i + iter * 37) % CHUNK_VOXELS]));
    }
    t1 = std::chrono::steady_clock::now();
    double mut_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / (20.0 * CHUNK_VOXELS);
    out_mutate.push_back(mut_ns);
}

int main() {
    auto scenes = make_scenes();
    auto out = std::ofstream("results.csv");
    out << "scene,unique_types,strategy,seed,bytes,lookup_ns,mutate_ns\n";

    for (auto &scene : scenes) {
        for (int seed = 0; seed < SEEDS; ++seed) {
            auto bench = [&](auto &strat_instance, const char *sname) {
                std::vector<double> b, l, m;
                bench_strategy<std::decay_t<decltype(strat_instance)>>(scene, seed, b, l, m);
                out << std::format("{},{},{},{},{:.1f},{:.2f},{:.2f}\n", scene.name, scene.unique_types, sname, seed, b[0], l[0], m[0]);
            };
            Fixed16 f; bench(f, Fixed16::name);
            AdaptivePalette a; bench(a, AdaptivePalette::name);
            SingleStateOpt s; bench(s, SingleStateOpt::name);
            Direct8 d; bench(d, Direct8::name);
        }
    }
    out.close();
    std::print("done. results written to results.csv\n");
    return 0;
}
