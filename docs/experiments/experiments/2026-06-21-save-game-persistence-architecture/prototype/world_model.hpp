// 2026-06-21-save-game-persistence-architecture/prototype/world_model.hpp
// Synthetic world model representative of ProjectV Stage 6+ persistent sandbox.
// CPU-only analytical model — no Vulkan, no Flecs dependency, no ProjectV mainline types.
//
// Design rationale:
// - Voxel = uint16_t material id (65535 materials, enough for ProjectV modding).
// - Chunk = 8x8x8 voxels = 4096 voxels = 8 KiB raw (matches ProjectV mainline VoxelChunk).
// - World = NxNxN chunks (default N=8 small, N=16 medium, N=32 large).
// - Entity = id + components (Position, Health, Owner, Inventory).
// - WorldSnapshot = header + chunk array + entity array + metadata.

#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace save_bench {

inline constexpr std::size_t kChunkDim = 8;
inline constexpr std::size_t kChunkVoxels = kChunkDim * kChunkDim * kChunkDim;
inline constexpr std::size_t kChunkBytes = kChunkVoxels * sizeof(uint16_t);

using Voxel = uint16_t;
using ChunkData = std::array<Voxel, kChunkVoxels>;

struct ChunkPosition {
    int32_t x{};
    int32_t y{};
    int32_t z{};

    auto operator<=>(const ChunkPosition&) const noexcept = default;
};

struct Chunk {
    ChunkPosition pos{};
    ChunkData data{};
};

struct Entity {
    uint64_t id{};
    float pos_x{};
    float pos_y{};
    float pos_z{};
    float health{};
    uint32_t owner{};
    uint32_t inventory[8]{};
    uint32_t flags{};
};

struct WorldMetadata {
    uint32_t schema_version{1};
    uint32_t world_seed{};
    uint64_t created_at_unix{};
    uint64_t total_playtime_ticks{};
    char world_name[64]{};
    char mod_dependencies[256]{};
};

struct World {
    WorldMetadata meta{};
    std::vector<Chunk> chunks;
    std::vector<Entity> entities;

    auto raw_bytes() const noexcept -> std::size_t {
        return chunks.size() * kChunkBytes + entities.size() * sizeof(Entity);
    }
};

// Synthesise a representative world from a seed.
// fill_density = 0.0-1.0 (chance a voxel is non-zero).
// entity_density = average entities per chunk.
inline auto synthesize_world(std::uint64_t seed,
                             int chunk_dim,
                             float fill_density = 0.3f,
                             float entity_density = 0.5f) -> World {
    World w;
    w.meta.schema_version = 1;
    w.meta.world_seed = static_cast<uint32_t>(seed & 0xFFFFFFFFu);
    w.meta.created_at_unix = 1700000000 + seed;
    w.meta.total_playtime_ticks = seed * 100;
    std::snprintf(w.meta.world_name, sizeof(w.meta.world_name), "world_%llx", (unsigned long long)seed);
    std::snprintf(w.meta.mod_dependencies, sizeof(w.meta.mod_dependencies),
                  "core,base_game,test_pack_%llx", (unsigned long long)seed);

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint32_t> material_dist(1, 4095);
    std::uniform_real_distribution<float> fill_dist(0.0f, 1.0f);

    const int total_chunks = chunk_dim * chunk_dim * chunk_dim;
    w.chunks.reserve(total_chunks);

    for (int cx = 0; cx < chunk_dim; ++cx) {
        for (int cy = 0; cy < chunk_dim; ++cy) {
            for (int cz = 0; cz < chunk_dim; ++cz) {
                Chunk c{};
                c.pos = {cx, cy, cz};
                for (std::size_t i = 0; i < kChunkVoxels; ++i) {
                    if (fill_dist(rng) < fill_density) {
                        c.data[i] = static_cast<Voxel>(material_dist(rng));
                    }
                }
                w.chunks.push_back(c);
            }
        }
    }

    const std::size_t entity_count = static_cast<std::size_t>(total_chunks * entity_density);
    w.entities.reserve(entity_count);
    for (std::size_t i = 0; i < entity_count; ++i) {
        Entity e{};
        e.id = (seed << 32) | i;
        e.pos_x = static_cast<float>(rng() % 1000);
        e.pos_y = static_cast<float>(rng() % 256);
        e.pos_z = static_cast<float>(rng() % 1000);
        e.health = static_cast<float>(rng() % 100);
        e.owner = static_cast<uint32_t>(rng() % 16);
        for (auto& inv : e.inventory) inv = static_cast<uint32_t>(rng() % 100000);
        e.flags = static_cast<uint32_t>(rng() & 0xFFFFu);
        w.entities.push_back(e);
    }

    return w;
}

// Mutate a copy of the world: change `mutation_pct` of chunks (0-100).
inline auto mutate_world(const World& src, std::uint64_t seed, float mutation_pct) -> World {
    World dst = src;
    if (mutation_pct <= 0.0f) return dst;
    std::mt19937_64 rng(seed ^ 0xDEADBEEFu);
    std::uniform_int_distribution<uint32_t> material_dist(1, 4095);

    const std::size_t num_to_mutate = static_cast<std::size_t>(dst.chunks.size() * (mutation_pct / 100.0f));
    for (std::size_t i = 0; i < num_to_mutate; ++i) {
        const std::size_t idx = rng() % dst.chunks.size();
        for (std::size_t j = 0; j < kChunkVoxels; ++j) {
            if (rng() & 1u) {
                dst.chunks[idx].data[j] = static_cast<Voxel>(material_dist(rng));
            }
        }
        dst.chunks[idx].pos.x += static_cast<int32_t>(rng() & 1u);
    }

    const std::size_t ents_to_mutate = static_cast<std::size_t>(dst.entities.size() * (mutation_pct / 100.0f));
    for (std::size_t i = 0; i < ents_to_mutate; ++i) {
        const std::size_t idx = rng() % dst.entities.size();
        dst.entities[idx].health = static_cast<float>(rng() % 100);
        dst.entities[idx].pos_x = static_cast<float>(rng() % 1000);
    }

    return dst;
}

// Compare two worlds for round-trip fidelity (bit-exact voxel + entity compare).
inline auto worlds_equal(const World& a, const World& b) noexcept -> bool {
    if (a.chunks.size() != b.chunks.size()) return false;
    if (a.entities.size() != b.entities.size()) return false;
    for (std::size_t i = 0; i < a.chunks.size(); ++i) {
        if (a.chunks[i].data != b.chunks[i].data) return false;
    }
    for (std::size_t i = 0; i < a.entities.size(); ++i) {
        if (a.entities[i].id != b.entities[i].id) return false;
        if (a.entities[i].health != b.entities[i].health) return false;
        if (a.entities[i].pos_x != b.entities[i].pos_x) return false;
        if (a.entities[i].pos_y != b.entities[i].pos_y) return false;
        if (a.entities[i].pos_z != b.entities[i].pos_z) return false;
        if (a.entities[i].owner != b.entities[i].owner) return false;
        for (std::size_t k = 0; k < 8; ++k) {
            if (a.entities[i].inventory[k] != b.entities[i].inventory[k]) return false;
        }
    }
    return true;
}

}  // namespace save_bench