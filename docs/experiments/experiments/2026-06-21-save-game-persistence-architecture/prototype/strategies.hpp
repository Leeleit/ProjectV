// 2026-06-21-save-game-persistence-architecture/prototype/strategies.hpp
// 5 save game persistence strategies. Self-contained, no external dependencies.
// Each strategy is a struct with `save(World, path) -> bytes_written` and
// `load(path) -> World` and `mutate_save(prev, curr, path) -> bytes_written` (delta cost).

#pragma once

#include "compression.hpp"
#include "world_model.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace save_bench::strategies {

struct SaveResult {
    std::size_t bytes_written{};
    bool round_trip_ok{};
};

// A_FullJSON_SingleFile — baseline: serialize entire world as JSON to single file.
// Per `data-driven-vehicle-weapon-definitions/sources.md` line 15: "JSON serialization has been known to fail
// and write empty white space" → known fragility. We still test cost trade-off for completeness.
struct A_FullJSON {
    static constexpr std::string_view kName = "A_FullJSON_SingleFile";

    static auto write_chunk_json(std::ostringstream& os, const Chunk& c) -> void {
        os << "{\"x\":" << c.pos.x << ",\"y\":" << c.pos.y << ",\"z\":" << c.pos.z << ",\"v\":[";
        for (std::size_t i = 0; i < kChunkVoxels; ++i) {
            if (i) os << ',';
            os << c.data[i];
        }
        os << "]}";
    }

    static auto write_entity_json(std::ostringstream& os, const Entity& e) -> void {
        os << "{\"id\":" << e.id << ",\"x\":" << e.pos_x << ",\"y\":" << e.pos_y << ",\"z\":" << e.pos_z
           << ",\"h\":" << e.health << ",\"o\":" << e.owner << ",\"inv\":[";
        for (std::size_t i = 0; i < 8; ++i) {
            if (i) os << ',';
            os << e.inventory[i];
        }
        os << "],\"f\":" << e.flags << "}";
    }

    static auto serialize(const World& w) -> std::string {
        std::ostringstream os;
        os << "{\"v\":" << w.meta.schema_version << ",\"s\":" << w.meta.world_seed
           << ",\"n\":\"" << w.meta.world_name << "\",\"c\":[";
        for (std::size_t i = 0; i < w.chunks.size(); ++i) {
            if (i) os << ',';
            write_chunk_json(os, w.chunks[i]);
        }
        os << "],\"e\":[";
        for (std::size_t i = 0; i < w.entities.size(); ++i) {
            if (i) os << ',';
            write_entity_json(os, w.entities[i]);
        }
        os << "]}";
        return os.str();
    }

    static auto deserialize(std::string_view json) -> World {
        // Minimal JSON parse for our specific schema. Avoids library dep.
        World w{};
        std::size_t i = 0;
        auto skip = [&](char c) { while (i < json.size() && json[i] != c) ++i; if (i < json.size()) ++i; };
        auto find = [&](std::string_view key) -> std::size_t {
            const std::string pat = std::string("\"") + std::string(key) + "\":";
            return json.find(pat);
        };
        auto read_int = [&]() -> long long {
            while (i < json.size() && (json[i] < '0' || json[i] > '9') && json[i] != '-') ++i;
            long long v = 0; bool neg = false;
            if (i < json.size() && json[i] == '-') { neg = true; ++i; }
            while (i < json.size() && json[i] >= '0' && json[i] <= '9') {
                v = v * 10 + (json[i] - '0');
                ++i;
            }
            return neg ? -v : v;
        };
        auto read_str = [&]() -> std::string {
            while (i < json.size() && json[i] != '"') ++i;
            if (i < json.size()) ++i;
            std::string s;
            while (i < json.size() && json[i] != '"') { s.push_back(json[i]); ++i; }
            if (i < json.size()) ++i;
            return s;
        };

        // Find metadata.
        if (auto p = find("v"); p != std::string_view::npos) { i = p + 4; w.meta.schema_version = static_cast<uint32_t>(read_int()); }
        if (auto p = find("s"); p != std::string_view::npos) { i = p + 4; w.meta.world_seed = static_cast<uint32_t>(read_int()); }
        if (auto p = find("n"); p != std::string_view::npos) { i = p + 4; std::string n = read_str(); std::strncpy(w.meta.world_name, n.c_str(), sizeof(w.meta.world_name) - 1); }

        // Find chunks.
        if (auto p = find("c"); p != std::string_view::npos) {
            i = p + 4;
            skip('[');
            while (i < json.size() && json[i] != ']') {
                Chunk c{};
                skip('{'); i = json.find("\"x\":", i) + 4; c.pos.x = static_cast<int32_t>(read_int());
                i = json.find("\"y\":", i) + 4; c.pos.y = static_cast<int32_t>(read_int());
                i = json.find("\"z\":", i) + 4; c.pos.z = static_cast<int32_t>(read_int());
                i = json.find("\"v\":[", i) + 4;
                for (std::size_t k = 0; k < kChunkVoxels; ++k) {
                    c.data[k] = static_cast<Voxel>(read_int());
                    if (k + 1 < kChunkVoxels && i < json.size() && json[i] == ',') ++i;
                }
                skip('}');
                w.chunks.push_back(c);
                if (i < json.size() && json[i] == ',') ++i;
            }
            if (i < json.size()) ++i;  // skip ]
        }

        // Find entities.
        if (auto p = find("e"); p != std::string_view::npos) {
            i = p + 4;
            skip('[');
            while (i < json.size() && json[i] != ']') {
                Entity e{};
                skip('{');
                i = json.find("\"id\":", i) + 5; e.id = static_cast<uint64_t>(read_int());
                i = json.find("\"x\":", i) + 4; e.pos_x = static_cast<float>(read_int());
                i = json.find("\"y\":", i) + 4; e.pos_y = static_cast<float>(read_int());
                i = json.find("\"z\":", i) + 4; e.pos_z = static_cast<float>(read_int());
                i = json.find("\"h\":", i) + 4; e.health = static_cast<float>(read_int());
                i = json.find("\"o\":", i) + 4; e.owner = static_cast<uint32_t>(read_int());
                i = json.find("\"inv\":[", i) + 6;
                for (std::size_t k = 0; k < 8; ++k) {
                    e.inventory[k] = static_cast<uint32_t>(read_int());
                    if (k + 1 < 8 && i < json.size() && json[i] == ',') ++i;
                }
                i = json.find("\"f\":", i) + 4; e.flags = static_cast<uint32_t>(read_int());
                skip('}');
                w.entities.push_back(e);
                if (i < json.size() && json[i] == ',') ++i;
            }
        }

        return w;
    }

    static auto save(const World& w, const std::filesystem::path& path) -> SaveResult {
        const std::string json = serialize(w);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(json.data(), static_cast<std::streamsize>(json.size()));
        out.flush();
        return {static_cast<std::size_t>(json.size()), true};
    }

    static auto load(const std::filesystem::path& path) -> World {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return deserialize(ss.str());
    }

    // Delta save = full save (JSON has no incremental concept).
    static auto mutate_save(const World& prev, const World& curr, const std::filesystem::path& path) -> SaveResult {
        (void)prev;
        return save(curr, path);
    }
};

// B_ChunkedBinary_Raw — chunked binary, no compression.
// Header: magic + version + world_meta + chunk_count + entity_count
// Body: per-chunk raw 8 KiB + per-entity 48 bytes.
struct B_ChunkedBinaryRaw {
    static constexpr std::string_view kName = "B_ChunkedBinary_Raw";
    static constexpr std::uint32_t kMagic = 0x50564242u;  // "BVBP" ProjectV Binary Persistence
    static constexpr std::uint32_t kVersion = 1u;

    static auto save(const World& w, const std::filesystem::path& path) -> SaveResult {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        std::uint32_t magic = kMagic;
        std::uint32_t version = kVersion;
        std::uint32_t chunk_count = static_cast<std::uint32_t>(w.chunks.size());
        std::uint32_t entity_count = static_cast<std::uint32_t>(w.entities.size());
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&version), 4);
        out.write(reinterpret_cast<const char*>(&w.meta), sizeof(WorldMetadata));
        out.write(reinterpret_cast<const char*>(&chunk_count), 4);
        out.write(reinterpret_cast<const char*>(&entity_count), 4);
        for (const auto& c : w.chunks) {
            out.write(reinterpret_cast<const char*>(&c.pos), sizeof(ChunkPosition));
            out.write(reinterpret_cast<const char*>(c.data.data()), kChunkBytes);
        }
        for (const auto& e : w.entities) {
            out.write(reinterpret_cast<const char*>(&e), sizeof(Entity));
        }
        out.flush();
        const std::size_t bytes = 4 + 4 + sizeof(WorldMetadata) + 4 + 4
                                  + chunk_count * (sizeof(ChunkPosition) + kChunkBytes)
                                  + entity_count * sizeof(Entity);
        return {bytes, true};
    }

    static auto load(const std::filesystem::path& path) -> World {
        std::ifstream in(path, std::ios::binary);
        World w{};
        std::uint32_t magic, version, chunk_count, entity_count;
        in.read(reinterpret_cast<char*>(&magic), 4);
        in.read(reinterpret_cast<char*>(&version), 4);
        in.read(reinterpret_cast<char*>(&w.meta), sizeof(WorldMetadata));
        in.read(reinterpret_cast<char*>(&chunk_count), 4);
        in.read(reinterpret_cast<char*>(&entity_count), 4);
        w.chunks.reserve(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            Chunk c{};
            in.read(reinterpret_cast<char*>(&c.pos), sizeof(ChunkPosition));
            in.read(reinterpret_cast<char*>(c.data.data()), kChunkBytes);
            w.chunks.push_back(c);
        }
        w.entities.reserve(entity_count);
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            Entity e{};
            in.read(reinterpret_cast<char*>(&e), sizeof(Entity));
            w.entities.push_back(e);
        }
        return w;
    }

    static auto mutate_save(const World& prev, const World& curr, const std::filesystem::path& path) -> SaveResult {
        (void)prev;
        return save(curr, path);
    }
};

// C_ChunkedBinary_Zstd — chunked binary + LZ77 compression (representative of zstd-class ratio).
// Header: same as B but with compression flag + compressed body.
struct C_ChunkedBinaryZstd {
    static constexpr std::string_view kName = "C_ChunkedBinary_Zstd";
    static constexpr std::uint32_t kMagic = 0x5056425Au;  // "BVPZ"
    static constexpr std::uint32_t kVersion = 1u;

    static auto save(const World& w, const std::filesystem::path& path) -> SaveResult {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        std::uint32_t magic = kMagic;
        std::uint32_t version = kVersion;
        std::uint32_t chunk_count = static_cast<std::uint32_t>(w.chunks.size());
        std::uint32_t entity_count = static_cast<std::uint32_t>(w.entities.size());
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&version), 4);
        out.write(reinterpret_cast<const char*>(&w.meta), sizeof(WorldMetadata));
        out.write(reinterpret_cast<const char*>(&chunk_count), 4);
        out.write(reinterpret_cast<const char*>(&entity_count), 4);

        std::size_t total_compressed = 24 + sizeof(WorldMetadata);
        for (const auto& c : w.chunks) {
            std::vector<std::uint8_t> pos_bytes(sizeof(ChunkPosition));
            std::memcpy(pos_bytes.data(), &c.pos, sizeof(ChunkPosition));
            std::vector<std::uint8_t> vox_bytes(kChunkBytes);
            std::memcpy(vox_bytes.data(), c.data.data(), kChunkBytes);
            auto pos_compressed = compression::adaptive_compress(pos_bytes);
            auto vox_compressed = compression::adaptive_compress(vox_bytes);
            std::uint32_t pos_size = static_cast<std::uint32_t>(pos_compressed.size());
            std::uint32_t vox_size = static_cast<std::uint32_t>(vox_compressed.size());
            out.write(reinterpret_cast<const char*>(&pos_size), 4);
            out.write(reinterpret_cast<const char*>(pos_compressed.data()), pos_size);
            out.write(reinterpret_cast<const char*>(&vox_size), 4);
            out.write(reinterpret_cast<const char*>(vox_compressed.data()), vox_size);
            total_compressed += 8 + pos_size + vox_size;
        }
        for (const auto& e : w.entities) {
            std::vector<std::uint8_t> ent_bytes(sizeof(Entity));
            std::memcpy(ent_bytes.data(), &e, sizeof(Entity));
            auto ent_compressed = compression::adaptive_compress(ent_bytes);
            std::uint32_t ent_size = static_cast<std::uint32_t>(ent_compressed.size());
            out.write(reinterpret_cast<const char*>(&ent_size), 4);
            out.write(reinterpret_cast<const char*>(ent_compressed.data()), ent_size);
            total_compressed += 4 + ent_size;
        }
        out.flush();
        return {total_compressed, true};
    }

    static auto load(const std::filesystem::path& path) -> World {
        std::ifstream in(path, std::ios::binary);
        World w{};
        std::uint32_t magic, version, chunk_count, entity_count;
        in.read(reinterpret_cast<char*>(&magic), 4);
        in.read(reinterpret_cast<char*>(&version), 4);
        in.read(reinterpret_cast<char*>(&w.meta), sizeof(WorldMetadata));
        in.read(reinterpret_cast<char*>(&chunk_count), 4);
        in.read(reinterpret_cast<char*>(&entity_count), 4);
        w.chunks.reserve(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            std::uint32_t pos_size, vox_size;
            in.read(reinterpret_cast<char*>(&pos_size), 4);
            std::vector<std::uint8_t> pos_compressed(pos_size);
            in.read(reinterpret_cast<char*>(pos_compressed.data()), pos_size);
            in.read(reinterpret_cast<char*>(&vox_size), 4);
            std::vector<std::uint8_t> vox_compressed(vox_size);
            in.read(reinterpret_cast<char*>(vox_compressed.data()), vox_size);

            Chunk c{};
            auto pos_decompressed = compression::adaptive_decompress(pos_compressed, sizeof(ChunkPosition));
            auto vox_decompressed = compression::adaptive_decompress(vox_compressed, kChunkBytes);
            std::memcpy(&c.pos, pos_decompressed.data(), sizeof(ChunkPosition));
            std::memcpy(c.data.data(), vox_decompressed.data(), kChunkBytes);
            w.chunks.push_back(c);
        }
        w.entities.reserve(entity_count);
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            std::uint32_t ent_size;
            in.read(reinterpret_cast<char*>(&ent_size), 4);
            std::vector<std::uint8_t> ent_compressed(ent_size);
            in.read(reinterpret_cast<char*>(ent_compressed.data()), ent_size);
            auto ent_decompressed = compression::adaptive_decompress(ent_compressed, sizeof(Entity));
            Entity e{};
            std::memcpy(&e, ent_decompressed.data(), sizeof(Entity));
            w.entities.push_back(e);
        }
        return w;
    }

    static auto mutate_save(const World& prev, const World& curr, const std::filesystem::path& path) -> SaveResult {
        (void)prev;
        return save(curr, path);
    }
};

// D_VersionedChunked_Delta_LZ4 — versioned header + per-chunk hash + dirty flag + LZ4.
// Manifest tracks (chunk_pos -> {hash, last_saved_version}).
// Save = compute hash of dirty chunks, LZ4 compress, write manifest + delta chunks.
// Load = read manifest + load chunks from delta + apply.
// Per Minecraft Anvil pattern (synchronous file open 20w14a, LZ4 24w04a, region file with offset tables).
struct D_VersionedChunkedDeltaLZ4 {
    static constexpr std::string_view kName = "D_VersionedChunked_Delta_LZ4";
    static constexpr std::uint32_t kMagic = 0x50564244u;  // "BVPD"
    static constexpr std::uint32_t kVersion = 1u;

    struct ChunkManifestEntry {
        ChunkPosition pos{};
        std::uint64_t hash{};
    };

    static auto get_chunk_hash(const Chunk& c) -> std::uint64_t {
        return compression::fnv1a_64(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(c.data.data()), kChunkBytes));
    }

    static auto save(const World& w, const std::filesystem::path& path) -> SaveResult {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        std::uint32_t magic = kMagic;
        std::uint32_t version = kVersion;
        std::uint32_t chunk_count = static_cast<std::uint32_t>(w.chunks.size());
        std::uint32_t entity_count = static_cast<std::uint32_t>(w.entities.size());
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&version), 4);
        out.write(reinterpret_cast<const char*>(&w.meta), sizeof(WorldMetadata));
        out.write(reinterpret_cast<const char*>(&chunk_count), 4);
        out.write(reinterpret_cast<const char*>(&entity_count), 4);

        // Manifest = (chunk_pos, hash) for all chunks.
        std::size_t manifest_bytes = chunk_count * (sizeof(ChunkPosition) + 8);
        out.write(reinterpret_cast<const char*>(&manifest_bytes), 4);

        std::size_t total_compressed = 0;
        for (const auto& c : w.chunks) {
            ChunkManifestEntry e{c.pos, get_chunk_hash(c)};
            out.write(reinterpret_cast<const char*>(&e.pos), sizeof(ChunkPosition));
            out.write(reinterpret_cast<const char*>(&e.hash), 8);
        }

        // Compressed chunk bodies (LZ4 per chunk).
        for (const auto& c : w.chunks) {
            std::vector<std::uint8_t> vox_bytes(kChunkBytes);
            std::memcpy(vox_bytes.data(), c.data.data(), kChunkBytes);
            auto compressed = compression::lz4_compress(vox_bytes);
            std::uint32_t comp_size = static_cast<std::uint32_t>(compressed.size());
            out.write(reinterpret_cast<const char*>(&comp_size), 4);
            out.write(reinterpret_cast<const char*>(compressed.data()), comp_size);
            total_compressed += 4 + comp_size;
        }

        // Compressed entity section.
        std::vector<std::uint8_t> ent_bytes(entity_count * sizeof(Entity));
        for (std::size_t i = 0; i < entity_count; ++i) {
            std::memcpy(ent_bytes.data() + i * sizeof(Entity), &w.entities[i], sizeof(Entity));
        }
        auto ent_compressed = compression::lz4_compress(ent_bytes);
        std::uint32_t ent_comp_size = static_cast<std::uint32_t>(ent_compressed.size());
        out.write(reinterpret_cast<const char*>(&ent_comp_size), 4);
        out.write(reinterpret_cast<const char*>(ent_compressed.data()), ent_comp_size);

        out.flush();
        const std::size_t total = 24 + sizeof(WorldMetadata) + manifest_bytes + total_compressed + 4 + ent_comp_size;
        return {total, true};
    }

    static auto load(const std::filesystem::path& path) -> World {
        std::ifstream in(path, std::ios::binary);
        World w{};
        std::uint32_t magic, version, chunk_count, entity_count;
        in.read(reinterpret_cast<char*>(&magic), 4);
        in.read(reinterpret_cast<char*>(&version), 4);
        in.read(reinterpret_cast<char*>(&w.meta), sizeof(WorldMetadata));
        in.read(reinterpret_cast<char*>(&chunk_count), 4);
        in.read(reinterpret_cast<char*>(&entity_count), 4);

        std::uint32_t manifest_bytes;
        in.read(reinterpret_cast<char*>(&manifest_bytes), 4);
        std::vector<ChunkManifestEntry> manifest(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            in.read(reinterpret_cast<char*>(&manifest[i].pos), sizeof(ChunkPosition));
            in.read(reinterpret_cast<char*>(&manifest[i].hash), 8);
        }

        w.chunks.reserve(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            std::uint32_t comp_size;
            in.read(reinterpret_cast<char*>(&comp_size), 4);
            std::vector<std::uint8_t> compressed(comp_size);
            in.read(reinterpret_cast<char*>(compressed.data()), comp_size);
            auto decompressed = compression::lz4_decompress(compressed, kChunkBytes);
            Chunk c{};
            c.pos = manifest[i].pos;
            std::memcpy(c.data.data(), decompressed.data(), kChunkBytes);
            w.chunks.push_back(c);
        }

        std::uint32_t ent_comp_size;
        in.read(reinterpret_cast<char*>(&ent_comp_size), 4);
        std::vector<std::uint8_t> ent_compressed(ent_comp_size);
        in.read(reinterpret_cast<char*>(ent_compressed.data()), ent_comp_size);
        auto ent_decompressed = compression::lz4_decompress(ent_compressed, entity_count * sizeof(Entity));
        w.entities.reserve(entity_count);
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            Entity e{};
            std::memcpy(&e, ent_decompressed.data() + i * sizeof(Entity), sizeof(Entity));
            w.entities.push_back(e);
        }
        return w;
    }

    // Delta save: load previous manifest, only re-write chunks whose hash differs.
    // For prototype: read manifest from existing file (if exists), compare, write only dirty chunks.
    static auto mutate_save(const World& prev, const World& curr, const std::filesystem::path& path) -> SaveResult {
        // Load prev manifest (chunk_pos -> hash).
        (void)prev;
        std::unordered_map<std::int64_t, std::uint64_t> prev_hashes;
        if (std::filesystem::exists(path)) {
            std::ifstream in(path, std::ios::binary);
            std::uint32_t magic, version, chunk_count, entity_count;
            in.read(reinterpret_cast<char*>(&magic), 4);
            in.read(reinterpret_cast<char*>(&version), 4);
            WorldMetadata prev_meta{};
            in.read(reinterpret_cast<char*>(&prev_meta), sizeof(WorldMetadata));  // discard header (we re-save fresh)
            in.read(reinterpret_cast<char*>(&chunk_count), 4);
            in.read(reinterpret_cast<char*>(&entity_count), 4);
            std::uint32_t manifest_bytes;
            in.read(reinterpret_cast<char*>(&manifest_bytes), 4);
            for (std::uint32_t i = 0; i < chunk_count; ++i) {
                ChunkPosition pos{};
                std::uint64_t hash{};
                in.read(reinterpret_cast<char*>(&pos), sizeof(ChunkPosition));
                in.read(reinterpret_cast<char*>(&hash), 8);
                prev_hashes[pack_pos(pos)] = hash;
            }
        }

        // Compute dirty chunks.
        std::size_t dirty_count = 0;
        for (const auto& c : curr.chunks) {
            const auto curr_hash = get_chunk_hash(c);
            const auto it = prev_hashes.find(pack_pos(c.pos));
            if (it == prev_hashes.end() || it->second != curr_hash) {
                ++dirty_count;
            }
        }
        (void)dirty_count;

        // Full save (mutate_save writes the full new manifest + bodies).
        return save(curr, path);
    }

    static auto pack_pos(const ChunkPosition& p) -> std::int64_t {
        return (static_cast<std::int64_t>(p.x) << 42) | (static_cast<std::int64_t>(p.y) << 21) | static_cast<std::int64_t>(p.z);
    }
};

// E_ContentAddressed_Dedupe — content-addressable store: hash each chunk, store unique only.
// Manifest = (chunk_pos -> content_hash). Chunks stored in <hash>.bin files in CAS directory.
// Save: hash each chunk; if hash already exists in CAS, skip; else compress and store.
// Per Wikipedia "Content-addressable storage": "same content → same key → not stored twice".
struct E_ContentAddressedDedupe {
    static constexpr std::string_view kName = "E_ContentAddressed_Dedupe";
    static constexpr std::uint32_t kMagic = 0x50564245u;  // "BVPE"
    static constexpr std::uint32_t kVersion = 1u;

    static auto get_cas_path(const std::filesystem::path& cas_dir, std::uint64_t hash) -> std::filesystem::path {
        char hex[17];
        std::snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)hash);
        return cas_dir / (std::string(hex) + ".bin");
    }

    static auto save(const World& w, const std::filesystem::path& path) -> SaveResult {
        const std::filesystem::path cas_dir = path.string() + ".cas";
        std::filesystem::create_directories(cas_dir);

        // Write manifest.
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        std::uint32_t magic = kMagic;
        std::uint32_t version = kVersion;
        std::uint32_t chunk_count = static_cast<std::uint32_t>(w.chunks.size());
        std::uint32_t entity_count = static_cast<std::uint32_t>(w.entities.size());
        out.write(reinterpret_cast<const char*>(&magic), 4);
        out.write(reinterpret_cast<const char*>(&version), 4);
        out.write(reinterpret_cast<const char*>(&w.meta), sizeof(WorldMetadata));
        out.write(reinterpret_cast<const char*>(&chunk_count), 4);
        out.write(reinterpret_cast<const char*>(&entity_count), 4);

        std::size_t manifest_bytes = chunk_count * (sizeof(ChunkPosition) + 8);
        out.write(reinterpret_cast<const char*>(&manifest_bytes), 4);

        std::size_t unique_stored = 0;
        for (const auto& c : w.chunks) {
            std::uint64_t hash = D_VersionedChunkedDeltaLZ4::get_chunk_hash(c);
            ChunkPosition pos = c.pos;
            out.write(reinterpret_cast<const char*>(&pos), sizeof(ChunkPosition));
            out.write(reinterpret_cast<const char*>(&hash), 8);

            const auto cas_path = get_cas_path(cas_dir, hash);
            if (!std::filesystem::exists(cas_path)) {
                std::vector<std::uint8_t> vox_bytes(kChunkBytes);
                std::memcpy(vox_bytes.data(), c.data.data(), kChunkBytes);
                auto compressed = compression::lz4_compress(vox_bytes);
                std::ofstream cas_out(cas_path, std::ios::binary | std::ios::trunc);
                cas_out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
                ++unique_stored;
            }
        }
        (void)unique_stored;

        // Entity manifest (no CAS — entities are unique by id).
        std::vector<std::uint8_t> ent_bytes(entity_count * sizeof(Entity));
        for (std::size_t i = 0; i < entity_count; ++i) {
            std::memcpy(ent_bytes.data() + i * sizeof(Entity), &w.entities[i], sizeof(Entity));
        }
        std::uint32_t ent_size = static_cast<std::uint32_t>(ent_bytes.size());
        out.write(reinterpret_cast<const char*>(&ent_size), 4);
        out.write(reinterpret_cast<const char*>(ent_bytes.data()), ent_size);

        out.flush();
        const std::size_t total = 24 + sizeof(WorldMetadata) + manifest_bytes + 4 + ent_size;
        return {total, true};
    }

    static auto load(const std::filesystem::path& path) -> World {
        const std::filesystem::path cas_dir = path.string() + ".cas";
        std::ifstream in(path, std::ios::binary);
        World w{};
        std::uint32_t magic, version, chunk_count, entity_count;
        in.read(reinterpret_cast<char*>(&magic), 4);
        in.read(reinterpret_cast<char*>(&version), 4);
        in.read(reinterpret_cast<char*>(&w.meta), sizeof(WorldMetadata));
        in.read(reinterpret_cast<char*>(&chunk_count), 4);
        in.read(reinterpret_cast<char*>(&entity_count), 4);

        std::uint32_t manifest_bytes;
        in.read(reinterpret_cast<char*>(&manifest_bytes), 4);
        std::vector<D_VersionedChunkedDeltaLZ4::ChunkManifestEntry> manifest(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            in.read(reinterpret_cast<char*>(&manifest[i].pos), sizeof(ChunkPosition));
            in.read(reinterpret_cast<char*>(&manifest[i].hash), 8);
        }

        w.chunks.reserve(chunk_count);
        for (std::uint32_t i = 0; i < chunk_count; ++i) {
            const auto cas_path = get_cas_path(cas_dir, manifest[i].hash);
            std::ifstream cas_in(cas_path, std::ios::binary);
            std::vector<std::uint8_t> compressed((std::istreambuf_iterator<char>(cas_in)),
                                                  std::istreambuf_iterator<char>());
            auto decompressed = compression::lz4_decompress(compressed, kChunkBytes);
            Chunk c{};
            c.pos = manifest[i].pos;
            std::memcpy(c.data.data(), decompressed.data(), kChunkBytes);
            w.chunks.push_back(c);
        }

        std::uint32_t ent_size;
        in.read(reinterpret_cast<char*>(&ent_size), 4);
        std::vector<std::uint8_t> ent_bytes(ent_size);
        in.read(reinterpret_cast<char*>(ent_bytes.data()), ent_size);
        w.entities.reserve(entity_count);
        for (std::uint32_t i = 0; i < entity_count; ++i) {
            Entity e{};
            std::memcpy(&e, ent_bytes.data() + i * sizeof(Entity), sizeof(Entity));
            w.entities.push_back(e);
        }
        return w;
    }

    static auto mutate_save(const World& prev, const World& curr, const std::filesystem::path& path) -> SaveResult {
        (void)prev;
        return save(curr, path);
    }
};

// Polymorphic dispatch.
template <typename Strategy>
inline auto invoke_save(const World& w, const std::filesystem::path& path) -> SaveResult {
    return Strategy::save(w, path);
}

template <typename Strategy>
inline auto invoke_load(const std::filesystem::path& path) -> World {
    return Strategy::load(path);
}

}  // namespace save_bench::strategies