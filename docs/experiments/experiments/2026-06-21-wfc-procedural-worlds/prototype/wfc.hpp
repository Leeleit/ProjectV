#pragma once
// WFC (Wave Function Collapse) engine — AC-3 algorithm + bitmask tileset.
// Standalone, header-only, C++26, no external deps.
// Reference: Maxim Gumin 2016 (https://github.com/mxgmn/WaveFunctionCollapse),
//            N-WFC arXiv 2308.07307 (nested sub-grids → polynomial time).
#include <array>
#include <bit>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace wfc {

using TileID = uint8_t;
using Bitmask = uint32_t;
static constexpr TileID INVALID = 255;
static constexpr Bitmask ALL_TILES_BITMASK = 0xFFu;

struct Tileset {
    static constexpr int MAX_TILES = 8;
    int tile_count = 0;
    // adjacency[tile][axis][tile] = 1 if tile can have `tile` as +axis neighbor.
    // axis: 0=+X, 1=+Y, 2=+Z. Negative axis uses mirror.
    uint8_t adjacency[MAX_TILES][3][MAX_TILES] = {};
    int weights[MAX_TILES] = {};
};

struct WFCConfig {
    int sx = 32, sy = 32, sz = 32;
    int max_propagation_passes = 32;
    int max_backtracks = 16;
    int max_solve_time_ms = 1000;
    uint64_t rng_seed = 42;
};

struct WFCStats {
    int propagation_passes = 0;
    int propagation_iterations = 0;
    int backtracks = 0;
    bool success = false;
    double solve_time_us = 0.0;
    size_t peak_working_set_bytes = 0;
};

// Inline helper: popcount for entropy (number of possible tiles in a cell).
inline int popcount(Bitmask m) { return std::popcount(m); }

// Inline helper: lowest set bit (used for picking tile from weighted options).
inline TileID lowest_set_bit(Bitmask m) { return static_cast<TileID>(std::countr_zero(m)); }

class WFCEngine {
public:
    WFCEngine(const Tileset& ts, WFCConfig cfg)
        : ts_(ts), cfg_(cfg),
          wave_(static_cast<size_t>(cfg.sx) * cfg.sy * cfg.sz, ALL_TILES_BITMASK),
          collapsed_(static_cast<size_t>(cfg.sx) * cfg.sy * cfg.sz, INVALID),
          entropy_(static_cast<size_t>(cfg.sx) * cfg.sy * cfg.sz, 0),
          rng_(cfg.rng_seed) {
        for (size_t i = 0; i < collapsed_.size(); ++i) {
            entropy_[i] = static_cast<int>(popcount(wave_[i]));
        }
    }

    WFCStats solve() {
        auto t0 = std::chrono::steady_clock::now();
        auto solve_deadline = t0 + std::chrono::milliseconds(cfg_.max_solve_time_ms);
        WFCStats stats;
        stats.peak_working_set_bytes = working_set_bytes();

        int total_cells = cfg_.sx * cfg_.sy * cfg_.sz;
        int cells_collapsed = 0;

        while (cells_collapsed < total_cells && stats.backtracks <= cfg_.max_backtracks) {
            if (std::chrono::steady_clock::now() > solve_deadline) {
                stats.success = false;
                stats.solve_time_us = cfg_.max_solve_time_ms * 1000.0;
                return stats;
            }
            // 1. Find cell with minimum entropy > 1 (lowest_set_bit wave != 0).
            size_t min_idx = SIZE_MAX;
            int min_entropy = INT_MAX;
            for (size_t i = 0; i < wave_.size(); ++i) {
                if (collapsed_[i] != INVALID) continue;
                int e = entropy_[i];
                if (e > 1 && e < min_entropy) {
                    min_entropy = e;
                    min_idx = i;
                } else if (e == 1 && min_entropy > 1) {
                    // Found singleton but prefer multi; skip unless no multi found.
                    if (min_idx == SIZE_MAX) { min_idx = i; min_entropy = e; }
                }
            }
            if (min_idx == SIZE_MAX) break; // All collapsed.

            // 2. Collapse: weighted random pick.
            Bitmask options = wave_[min_idx];
            TileID chosen = weighted_pick(options);
            wave_[min_idx] = (Bitmask{1} << chosen);
            collapsed_[min_idx] = chosen;
            entropy_[min_idx] = 1;
            ++cells_collapsed;

            // 3. Propagate via AC-3 (queue-based).
            std::vector<size_t> queue;
            queue.push_back(min_idx);
            int pass = 0;
            while (!queue.empty() && pass < cfg_.max_propagation_passes) {
                ++pass;
                ++stats.propagation_passes;
                size_t curr = queue.back();
                queue.pop_back();
                ++stats.propagation_iterations;

                int x, y, z;
                idx_to_xyz(curr, x, y, z);
                for (int axis = 0; axis < 3; ++axis) {
                    int nx = x, ny = y, nz = z;
                    if (axis == 0) { nx = x + 1; }
                    else if (axis == 1) { ny = y + 1; }
                    else { nz = z + 1; }
                    if (nx >= cfg_.sx || ny >= cfg_.sy || nz >= cfg_.sz) continue;
                    size_t nidx = xyz_to_idx(nx, ny, nz);
                    if (collapsed_[nidx] != INVALID) continue;
                    if (propagate_to(nidx, axis, +1)) {
                        if (entropy_[nidx] == 0) {
                            // Contradiction.
                            ++stats.backtracks;
                            stats.success = false;
                            auto t1 = std::chrono::steady_clock::now();
                            stats.solve_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
                            return stats;
                        }
                        queue.push_back(nidx);
                    }
                }
            }
        }

        stats.success = (cells_collapsed == total_cells);
        auto t1 = std::chrono::steady_clock::now();
        stats.solve_time_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        stats.peak_working_set_bytes = working_set_bytes();
        return stats;
    }

    const std::vector<TileID>& collapsed() const { return collapsed_; }
    const std::vector<Bitmask>& wave() const { return wave_; }

private:
    bool propagate_to(size_t nidx, int axis, int sign) {
        Bitmask& cell = wave_[nidx];
        Bitmask before = cell;
        Bitmask allowed = 0;
        // For each tile in self's wave, gather allowed neighbor tiles.
        Bitmask self_wave = wave_[xyz_to_idx_from_cell(nidx, axis, -sign)]; // previous cell
        if (self_wave == 0) return false;
        for (int t = 0; t < ts_.tile_count; ++t) {
            if (!(self_wave & (Bitmask{1} << t))) continue;
            for (int nb = 0; nb < ts_.tile_count; ++nb) {
                if (ts_.adjacency[t][axis][nb]) {
                    allowed |= (Bitmask{1} << nb);
                }
            }
        }
        cell = cell & allowed;
        if (cell != before) {
            entropy_[nidx] = popcount(cell);
            return true;
        }
        return false;
    }

    size_t xyz_to_idx_from_cell(size_t nidx, int axis, int sign) {
        int x, y, z;
        idx_to_xyz(static_cast<int>(nidx), x, y, z);
        if (axis == 0) x -= sign;
        else if (axis == 1) y -= sign;
        else z -= sign;
        return xyz_to_idx(x, y, z);
    }

    size_t xyz_to_idx(int x, int y, int z) const {
        return static_cast<size_t>((z * cfg_.sy + y) * cfg_.sx + x);
    }

    void idx_to_xyz(size_t idx, int& x, int& y, int& z) const {
        x = static_cast<int>(idx % cfg_.sx);
        y = static_cast<int>((idx / cfg_.sx) % cfg_.sy);
        z = static_cast<int>(idx / (cfg_.sx * cfg_.sy));
    }

    TileID weighted_pick(Bitmask options) {
        int total_weight = 0;
        for (int t = 0; t < ts_.tile_count; ++t) {
            if (options & (Bitmask{1} << t)) total_weight += ts_.weights[t];
        }
        if (total_weight == 0) return lowest_set_bit(options);
        std::uniform_int_distribution<int> dist(0, total_weight - 1);
        int r = dist(rng_);
        for (int t = 0; t < ts_.tile_count; ++t) {
            if (!(options & (Bitmask{1} << t))) continue;
            r -= ts_.weights[t];
            if (r < 0) return static_cast<TileID>(t);
        }
        return lowest_set_bit(options);
    }

    size_t working_set_bytes() const {
        return wave_.size() * sizeof(Bitmask)
             + collapsed_.size() * sizeof(TileID)
             + entropy_.size() * sizeof(int);
    }

    const Tileset& ts_;
    WFCConfig cfg_;
    std::vector<Bitmask> wave_;
    std::vector<TileID> collapsed_;
    std::vector<int> entropy_;
    std::mt19937_64 rng_;
};

// Tile-transitions consistency score: ratio of non-conflicting neighbor pairs / total pairs.
// Higher = better local coherence.
inline double transitions_consistency_score(const std::vector<TileID>& grid,
                                             int sx, int sy, int sz,
                                             const Tileset& ts) {
    int total = 0, ok = 0;
    auto get = [&](int x, int y, int z) -> TileID {
        return grid[static_cast<size_t>((z * sy + y) * sx + x)];
    };
    for (int z = 0; z < sz; ++z)
    for (int y = 0; y < sy; ++y)
    for (int x = 0; x < sx; ++x) {
        TileID t = get(x, y, z);
        if (t == INVALID) continue;
        for (int axis = 0; axis < 3; ++axis) {
            int nx = x, ny = y, nz = z;
            if (axis == 0) ++nx; else if (axis == 1) ++ny; else ++nz;
            if (nx >= sx || ny >= sy || nz >= sz) continue;
            TileID nb = get(nx, ny, nz);
            if (nb == INVALID) continue;
            ++total;
            if (ts.adjacency[t][axis][nb]) ++ok;
        }
    }
    return total ? static_cast<double>(ok) / total : 1.0;
}

} // namespace wfc
