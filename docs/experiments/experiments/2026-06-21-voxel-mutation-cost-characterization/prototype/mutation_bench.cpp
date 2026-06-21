// 2026-06-21-voxel-mutation-cost-characterization — standalone CPU mutation cost simulator
// Standalone C++26 prototype, NOT ProjectV mainline (per docs/experiments/AGENTS.md §2).
// Models per-chunk SVDAG-on-64-tree mutation strategies representative of mainline
// src/voxel/Sparse64Tree.hpp + src/voxel/VoxelWorld.cpp.
//
// Build: clang++ -O3 -march=native -std=c++26 -DNDEBUG -Wall -Wextra -Wpedantic
//        mutation_bench.cpp -o mutation_bench
// Run:   ./mutation_bench
// Output: results.csv (machine-readable) + stdout summary
//
// Reference: docs/experiments/experiments/2026-06-21-voxel-mutation-cost-characterization/

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---------------------------------------------------------------------------
// 1. SVDAG-on-64-tree (simplified model, faithful to mainline Sparse64Tree).
//    side=8 → 8^3 = 512 voxels per chunk; branching 4^3 = 64 children per node.
//    Per-node COW via refCount. Per-node dedup index via structural hash.
//    Mirrors mainline src/voxel/Sparse64Tree.hpp:523-567 SetCellRecursive.
// ---------------------------------------------------------------------------

namespace svdag {

constexpr int kNodeSide = 4;                   // 4^3 branching
constexpr int kChildrenPerNode = kNodeSide * kNodeSide * kNodeSide;
constexpr int kChunkSide = 8;                   // 8^3 chunk = 512 voxels
constexpr int kChunkSize = kChunkSide * kChunkSide * kChunkSide;
constexpr uint32_t kLeafFlag = 0x80000000u;
constexpr uint32_t kHomogeneousFlag = 0x40000000u;
constexpr uint32_t kNodeIndexMask = 0x3FFFFFFFu;
constexpr uint32_t kMaterialMask = 0xFFu;
constexpr uint32_t kInvalidNodeIndex = kNodeIndexMask;

inline constexpr uint32_t MakeLeaf(uint8_t material) noexcept { return kLeafFlag | static_cast<uint32_t>(material); }
inline constexpr uint32_t MakeHomogeneous(uint8_t material) noexcept { return kLeafFlag | kHomogeneousFlag | static_cast<uint32_t>(material); }
inline constexpr bool IsLeaf(uint32_t slot) noexcept { return (slot & (kLeafFlag | kHomogeneousFlag)) == kLeafFlag; }
inline constexpr bool IsHomogeneous(uint32_t slot) noexcept { return (slot & (kLeafFlag | kHomogeneousFlag)) == (kLeafFlag | kHomogeneousFlag); }
inline constexpr uint8_t LeafMaterial(uint32_t slot) noexcept { return static_cast<uint8_t>(slot & kMaterialMask); }
inline constexpr uint8_t HomogeneousMaterial(uint32_t slot) noexcept { return static_cast<uint8_t>(slot & kMaterialMask); }
inline constexpr uint32_t NodeIndex(uint32_t slot) noexcept { return slot & kNodeIndexMask; }
inline constexpr int ComputeChildSlotIndex(int sx, int sy, int sz) noexcept { return sx + kNodeSide * (sy + kNodeSide * sz); }

struct Node {
    uint64_t fillMask = 0;
    std::array<uint32_t, kChildrenPerNode> slots{};
    uint64_t structuralHash = 0;
    uint32_t refCount = 0;
    bool inDedupIndex = false;
};

struct VoxelSvdag64 {
    int depth = 0; // depth = 1 (chunk 8x8x8 = depth 1) + chunk_size/kNodeSide recursion = 1+0 = 1 if kChunkSide == kNodeSide; otherwise depth = ceil(log4(kChunkSide)). For kChunkSide=8, kNodeSide=4 → depth = ceil(log4(8)) = ceil(1.5) = 2.
    int maxDepth = 0;
    std::vector<Node> nodes;
    std::unordered_multimap<uint64_t, uint32_t> dedupIndex;
    bool dedupEnabled = false;
    uint32_t rootSlot = MakeLeaf(0);

    int ComputeDepth() const {
        // For kChunkSide=8, kNodeSide=4: depth = 2 (root + level at 4^3 + leaves at 4^3).
        // But we treat leaves as in-place materials, not separate nodes. So depth tracks only branching levels.
        // For our simplification: depth = ceil(log_kNodeSide(kChunkSide)).
        int d = 0;
        int n = 1;
        while (n < kChunkSide) { n *= kNodeSide; ++d; }
        return d;
    }

    void Reset() {
        depth = ComputeDepth();
        maxDepth = depth;
        nodes.clear();
        dedupIndex.clear();
        rootSlot = MakeLeaf(0);
    }

    VoxelSvdag64() { Reset(); }

    void SetDeduplicationEnabled(bool enabled) {
        if (enabled == dedupEnabled) return;
        dedupEnabled = enabled;
        for (auto& n : nodes) { n.refCount = (n.refCount > 0) ? 1 : 0; n.inDedupIndex = false; }
        dedupIndex.clear();
        if (enabled) {
            for (uint32_t i = 0; i < nodes.size(); ++i) {
                nodes[i].structuralHash = ComputeHash(nodes[i]);
                dedupIndex.emplace(nodes[i].structuralHash, i);
                nodes[i].inDedupIndex = true;
            }
        }
    }

    static uint64_t MixSplitMix64(uint64_t seed, uint64_t value) noexcept {
        uint64_t z = seed + value;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

    static uint64_t ComputeHash(const Node& node) noexcept {
        uint64_t h = MixSplitMix64(0x9E3779B97F4A7C15ull, node.fillMask);
        for (auto slot : node.slots) h = MixSplitMix64(h, slot);
        return h;
    }

    static int ExtractSubCoord(int coord, int level) noexcept {
        if (level <= 0) return 0;
        int shift = (level - 1) * 2;
        return (coord >> shift) & (kNodeSide - 1);
    }

    uint32_t AllocateNode(uint32_t fillMaterial) {
        Node newNode;
        if (IsLeaf(fillMaterial)) {
            uint8_t m = LeafMaterial(fillMaterial);
            for (int i = 0; i < kChildrenPerNode; ++i) newNode.slots[i] = MakeLeaf(m);
        } else {
            for (int i = 0; i < kChildrenPerNode; ++i) newNode.slots[i] = MakeLeaf(0);
        }
        newNode.refCount = 1;
        if (dedupEnabled) {
            newNode.structuralHash = ComputeHash(newNode);
            for (auto it = dedupIndex.equal_range(newNode.structuralHash).first; it != dedupIndex.equal_range(newNode.structuralHash).second; ++it) {
                if (nodes[it->second].fillMask == newNode.fillMask && nodes[it->second].slots == newNode.slots) {
                    ++nodes[it->second].refCount;
                    return it->second;
                }
            }
        }
        uint32_t idx = static_cast<uint32_t>(nodes.size());
        nodes.push_back(newNode);
        if (dedupEnabled) {
            nodes[idx].structuralHash = ComputeHash(nodes[idx]);
            dedupIndex.emplace(nodes[idx].structuralHash, idx);
            nodes[idx].inDedupIndex = true;
        }
        return idx;
    }

    uint32_t MarkNodeUnique(uint32_t nodeIndex) {
        if (!dedupEnabled || nodes[nodeIndex].refCount == 1) return nodeIndex;
        --nodes[nodeIndex].refCount;
        if (nodes[nodeIndex].inDedupIndex) {
            auto range = dedupIndex.equal_range(nodes[nodeIndex].structuralHash);
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second == nodeIndex) { dedupIndex.erase(it); break; }
            }
            nodes[nodeIndex].inDedupIndex = false;
        }
        Node copy = nodes[nodeIndex];
        copy.refCount = 1;
        uint32_t newIdx = static_cast<uint32_t>(nodes.size());
        nodes.push_back(copy);
        if (dedupEnabled) {
            nodes[newIdx].structuralHash = ComputeHash(nodes[newIdx]);
            dedupIndex.emplace(nodes[newIdx].structuralHash, newIdx);
            nodes[newIdx].inDedupIndex = true;
        }
        return newIdx;
    }

    bool CanCollapseToHomogeneous(const Node& node, uint8_t& outMaterial) const {
        if (node.fillMask != 0xFFFFFFFFFFFFFFFFull) return false;
        uint8_t material = 0;
        for (int i = 0; i < kChildrenPerNode; ++i) {
            uint32_t slot = node.slots[i];
            uint8_t m;
            if (IsHomogeneous(slot)) m = HomogeneousMaterial(slot);
            else if (IsLeaf(slot)) m = LeafMaterial(slot);
            else return false;
            if (i > 0 && m != material) return false;
            material = m;
        }
        outMaterial = material;
        return true;
    }

    uint32_t SetCellRecursive(uint32_t slot, int x, int y, int z, uint8_t material, int level) {
        if (level <= 0) return MakeLeaf(material);
        if (IsHomogeneous(slot)) {
            uint8_t existing = HomogeneousMaterial(slot);
            if (existing == material) return slot;
            uint32_t newIdx = AllocateNode(MakeLeaf(existing));
            nodes[newIdx].fillMask = 0xFFFFFFFFFFFFFFFFull;
            slot = newIdx;
        } else if (IsLeaf(slot)) {
            if (LeafMaterial(slot) == material) return slot;
            uint32_t idx = AllocateNode(slot);
            nodes[idx].fillMask = 0xFFFFFFFFFFFFFFFFull;
            slot = idx;
        }
        int sx = ExtractSubCoord(x, level);
        int sy = ExtractSubCoord(y, level);
        int sz = ExtractSubCoord(z, level);
        int ci = ComputeChildSlotIndex(sx, sy, sz);
        uint32_t nodeIndex = MarkNodeUnique(NodeIndex(slot));
        uint32_t existingSlot = nodes[nodeIndex].slots[ci];
        uint32_t updatedSlot = SetCellRecursive(existingSlot, x, y, z, material, level - 1);
        nodes[nodeIndex].slots[ci] = updatedSlot;
        nodes[nodeIndex].fillMask |= (1ull << ci);
        uint8_t collapse = 0;
        if (CanCollapseToHomogeneous(nodes[nodeIndex], collapse)) {
            if (dedupEnabled && nodes[nodeIndex].inDedupIndex) {
                auto range = dedupIndex.equal_range(nodes[nodeIndex].structuralHash);
                for (auto it = range.first; it != range.second; ++it) {
                    if (it->second == nodeIndex) { dedupIndex.erase(it); break; }
                }
                nodes[nodeIndex].inDedupIndex = false;
            }
            --nodes[nodeIndex].refCount;
            return MakeHomogeneous(collapse);
        }
        if (dedupEnabled) {
            auto range = dedupIndex.equal_range(nodes[nodeIndex].structuralHash);
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second == nodeIndex) { dedupIndex.erase(it); break; }
            }
            nodes[nodeIndex].structuralHash = ComputeHash(nodes[nodeIndex]);
            dedupIndex.emplace(nodes[nodeIndex].structuralHash, nodeIndex);
            nodes[nodeIndex].inDedupIndex = true;
        }
        return nodeIndex;
    }

    void SetCell(int x, int y, int z, uint8_t material) {
        rootSlot = SetCellRecursive(rootSlot, x, y, z, material, maxDepth);
    }

    uint8_t GetCell(int x, int y, int z) const {
        if (maxDepth == 0) {
            if (IsHomogeneous(rootSlot)) return HomogeneousMaterial(rootSlot);
            return IsLeaf(rootSlot) ? LeafMaterial(rootSlot) : 0;
        }
        uint32_t slot = rootSlot;
        for (int level = maxDepth; level > 0; --level) {
            if (IsHomogeneous(slot)) return HomogeneousMaterial(slot);
            if (IsLeaf(slot)) return LeafMaterial(slot);
            const Node& node = nodes[NodeIndex(slot)];
            int sx = ExtractSubCoord(x, level);
            int sy = ExtractSubCoord(y, level);
            int sz = ExtractSubCoord(z, level);
            int ci = ComputeChildSlotIndex(sx, sy, sz);
            if (((node.fillMask >> ci) & 1ull) == 0ull) return 0;
            slot = node.slots[ci];
        }
        if (IsHomogeneous(slot)) return HomogeneousMaterial(slot);
        if (IsLeaf(slot)) return LeafMaterial(slot);
        return 0;
    }

    size_t NodeCount() const { return nodes.size(); }

    // Snapshot: copy of rootSlot + nodes (for E_CopyOnWriteSnapshot).
    struct Snapshot {
        uint32_t rootSlot;
        std::vector<Node> nodes;
        std::unordered_multimap<uint64_t, uint32_t> dedupIndex;
        bool dedupEnabled;
        VoxelSvdag64 Restore() const {
            VoxelSvdag64 copy;
            copy.depth = 1; copy.maxDepth = 1;
            copy.rootSlot = rootSlot;
            copy.nodes = nodes;
            copy.dedupIndex = dedupIndex;
            copy.dedupEnabled = dedupEnabled;
            return copy;
        }
    };

    Snapshot TakeSnapshot() const {
        return {rootSlot, nodes, dedupIndex, dedupEnabled};
    }
};

} // namespace svdag

// ---------------------------------------------------------------------------
// 2. Mutations: SetCell calls represent gameplay mutations. We model both
//    single-frame and multi-frame burst patterns.
// ---------------------------------------------------------------------------

struct MutationOp {
    int x, y, z;
    uint8_t material;
};

using MutationBatch = std::vector<MutationOp>;

// ---------------------------------------------------------------------------
// 3. Strategies. Each strategy is a callable that takes (dag, batch, stats)
//    and applies the mutations + records metrics.
//    Stats: latency_ns (per edit), allocations (nodes added), peak_nodes.
// ---------------------------------------------------------------------------

struct EditStats {
    double latency_us = 0.0;       // total per-pattern wall time
    uint64_t allocations = 0;      // nodes added
    uint64_t peak_nodes = 0;       // peak nodes in tree
    uint64_t edit_count = 0;       // number of SetCell calls made
    uint64_t skip_count = 0;       // skipped (deduplicated / coalesced) edits
    uint64_t coalesced_rebuilds = 0; // # of times chunk tree was rebuilt
};

namespace strategy {

// A_NaiveInPlace: per-SetCell rebuild path. Equivalent to current mainline SetCellRecursive.
// COW node copies + dedup lookups happen per edit.
inline void A_NaiveInPlace(svdag::VoxelSvdag64& dag, const MutationBatch& batch, EditStats& s) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (const auto& m : batch) {
        size_t beforeNodes = dag.NodeCount();
        dag.SetCell(m.x, m.y, m.z, m.material);
        s.allocations += (dag.NodeCount() - beforeNodes);
        ++s.edit_count;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    s.latency_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    s.peak_nodes = std::max(s.peak_nodes, static_cast<uint64_t>(dag.NodeCount()));
}

// B_DirtyFlagDeferred: mainline has chunk-level dirty flag. We model: per-chunk
// edits during a single frame are coalesced — only the LAST SetCell per chunk
// actually mutates the tree; intermediate edits are buffered.
// Note: in mainline SetVoxelMaterial marks chunk dirty but still calls SetCell
// for EACH voxel — this strategy is the OPTIMIZATION where intermediate writes
// are skipped if the chunk was already edited this frame.
inline void B_DirtyFlagDeferred(svdag::VoxelSvdag64& dag, const MutationBatch& batch, EditStats& s) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::unordered_set<uint32_t> editedChunks;
    for (const auto& m : batch) {
        int cx = m.x / 8; int cy = m.y / 8; int cz = m.z / 8;
        uint32_t key = (cx * 1024) ^ (cy * 32) ^ cz;
        if (editedChunks.count(key)) { ++s.skip_count; continue; }
        editedChunks.insert(key);
        size_t beforeNodes = dag.NodeCount();
        dag.SetCell(m.x, m.y, m.z, m.material);
        s.allocations += (dag.NodeCount() - beforeNodes);
        ++s.edit_count;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    s.latency_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    s.peak_nodes = std::max(s.peak_nodes, static_cast<uint64_t>(dag.NodeCount()));
}

// C_BatchCoalesce: buffer N SetCells per chunk into a flat list, then rebuild
// chunk tree ONCE at end of frame from flat snapshot.
// Inspired by mathijs727 GPU-SVDAG-Editing Phase 1 (temp SVO construction).
inline void C_BatchCoalesce(svdag::VoxelSvdag64& dag, const MutationBatch& batch, EditStats& s) {
    auto t0 = std::chrono::high_resolution_clock::now();
    std::unordered_map<uint32_t, std::vector<MutationOp>> chunkEdits;
    for (const auto& m : batch) {
        int cx = m.x / 8; int cy = m.y / 8; int cz = m.z / 8;
        uint32_t key = (cx * 1024) ^ (cy * 32) ^ cz;
        chunkEdits[key].push_back(m);
    }
    for (auto& [key, edits] : chunkEdits) {
        for (const auto& m : edits) {
            size_t beforeNodes = dag.NodeCount();
            dag.SetCell(m.x, m.y, m.z, m.material);
            s.allocations += (dag.NodeCount() - beforeNodes);
        }
        ++s.coalesced_rebuilds;
        s.edit_count += edits.size();
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    s.latency_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    s.peak_nodes = std::max(s.peak_nodes, static_cast<uint64_t>(dag.NodeCount()));
}

// D_DoubleBufferSwap: clone current tree to staging; mutations go to staging;
// commit = swap root + nodes vec. Per-batch full clone of nodes vec is expensive
// in nodes count, but the COMMITTED tree is always atomic.
inline void D_DoubleBufferSwap(svdag::VoxelSvdag64& dag, const MutationBatch& batch, EditStats& s) {
    auto t0 = std::chrono::high_resolution_clock::now();
    // Take snapshot (clone full node vec).
    auto snap = dag.TakeSnapshot();
    // Apply edits in staging (rebuild via SetCell on a fresh staging from snapshot).
    svdag::VoxelSvdag64 staging = snap.Restore();
    for (const auto& m : batch) {
        size_t beforeNodes = staging.NodeCount();
        staging.SetCell(m.x, m.y, m.z, m.material);
        s.allocations += (staging.NodeCount() - beforeNodes);
        ++s.edit_count;
    }
    // Swap: dag ← staging.
    dag.rootSlot = staging.rootSlot;
    dag.nodes = std::move(staging.nodes);
    dag.dedupIndex = std::move(staging.dedupIndex);
    ++s.coalesced_rebuilds;
    auto t1 = std::chrono::high_resolution_clock::now();
    s.latency_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    s.peak_nodes = std::max(s.peak_nodes, static_cast<uint64_t>(dag.NodeCount()));
}

// E_CopyOnWriteSnapshot: per-node COW as mainline does, but with an explicit
// per-frame "commit" — readers always see a fully-formed immutable snapshot.
// Mutations modify in-place via MarkNodeUnique (per-node copy-on-write).
// This is essentially mainline semantics (already does MarkNodeUnique) but
// we measure pure per-edit cost without dirty-flag coalescing.
// Key difference vs A: no dirty flag — every edit goes through MarkNodeUnique.
inline void E_CopyOnWriteSnapshot(svdag::VoxelSvdag64& dag, const MutationBatch& batch, EditStats& s) {
    auto t0 = std::chrono::high_resolution_clock::now();
    for (const auto& m : batch) {
        size_t beforeNodes = dag.NodeCount();
        // Same as A but with dedup enabled to force per-node COW cost.
        bool saved = dag.dedupEnabled;
        dag.SetDeduplicationEnabled(true);
        dag.SetCell(m.x, m.y, m.z, m.material);
        dag.SetDeduplicationEnabled(saved);
        s.allocations += (dag.NodeCount() - beforeNodes);
        ++s.edit_count;
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    s.latency_us += std::chrono::duration<double, std::micro>(t1 - t0).count();
    s.peak_nodes = std::max(s.peak_nodes, static_cast<uint64_t>(dag.NodeCount()));
}

} // namespace strategy

// ---------------------------------------------------------------------------
// 4. Scenes: synthetic voxel scenes representative of ProjectV gameplay.
// ---------------------------------------------------------------------------

namespace scene {

enum class Id : uint8_t {
    UniformFloor = 0,   // 100% solid homogeneous at floor (collapse-friendly)
    SparseWorld  = 1,   // mostly air with scattered voxels
    MixedBiome   = 2,   // chunks with varied density
    CaveStress   = 3,   // hollow interior with surface voxels
    StackedSolid = 4    // solid stacked tower (best case for collapse)
};

inline constexpr const char* Name(Id id) {
    switch (id) {
        case Id::UniformFloor: return "uniform_floor";
        case Id::SparseWorld:  return "sparse_world";
        case Id::MixedBiome:   return "mixed_biome";
        case Id::CaveStress:   return "cave_stress";
        case Id::StackedSolid: return "stacked_solid";
    }
    return "unknown";
}

// Build a chunk (8^3 = 512 voxels) into a flat vector. Returns material per voxel index.
inline void Build(Id id, uint32_t seed, std::array<uint8_t, svdag::kChunkSize>& out) {
    out.fill(0); // Air
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> uniform(0, 99);
    switch (id) {
        case Id::UniformFloor: {
            // Floor at y=0..1, solid homogeneous.
            for (int y = 0; y < 2; ++y) for (int z = 0; z < 8; ++z) for (int x = 0; x < 8; ++x)
                out[x + 8 * (y + 8 * z)] = 3; // FloorWhite
            break;
        }
        case Id::SparseWorld: {
            // ~5% fill, scattered.
            for (int i = 0; i < svdag::kChunkSize / 20; ++i) {
                int idx = uniform(rng) % svdag::kChunkSize;
                out[idx] = 1; // Glass
            }
            break;
        }
        case Id::MixedBiome: {
            // 30% fill, varied.
            for (int i = 0; i < svdag::kChunkSize * 3 / 10; ++i) {
                int idx = uniform(rng) % svdag::kChunkSize;
                uint8_t m = 1 + (uniform(rng) % 4); // 1..4
                out[idx] = m;
            }
            break;
        }
        case Id::CaveStress: {
            // Hollow interior + thin shell.
            for (int y = 0; y < 8; ++y) for (int z = 0; z < 8; ++z) for (int x = 0; x < 8; ++x) {
                bool onShell = (x == 0 || x == 7 || y == 0 || y == 7 || z == 0 || z == 7);
                if (onShell) out[x + 8 * (y + 8 * z)] = 4; // FloorGray
            }
            // Sparse interior voxels.
            for (int i = 0; i < 8; ++i) {
                int idx = uniform(rng) % svdag::kChunkSize;
                out[idx] = 2; // Fluid
            }
            break;
        }
        case Id::StackedSolid: {
            // Solid tower y=0..7.
            for (int i = 0; i < svdag::kChunkSize; ++i) out[i] = 3;
            break;
        }
    }
}

// Mutation patterns: per-frame batch sizes representative of gameplay.
enum class Pattern : uint8_t {
    P1_SingleClick = 0,       // 1 SetCell
    P2_FillOperation = 1,      // 64 SetCells in 1 chunk (FillVoxelBox analog)
    P3_MultiChunkBuild = 2,    // 64 SetCells across 8 chunks
    P4_FloodFill = 3,          // ~128 SetCells in 1 chunk (BFS flood-fill analog)
    P5_StressBurst = 4         // 256 SetCells across chunks (burst / world gen)
};

inline constexpr const char* PatternName(Pattern p) {
    switch (p) {
        case Pattern::P1_SingleClick:     return "P1_SingleClick";
        case Pattern::P2_FillOperation:   return "P2_FillOperation";
        case Pattern::P3_MultiChunkBuild: return "P3_MultiChunkBuild";
        case Pattern::P4_FloodFill:       return "P4_FloodFill";
        case Pattern::P5_StressBurst:     return "P5_StressBurst";
    }
    return "?";
}

inline MutationBatch GenerateMutations(Pattern p, uint32_t seed) {
    MutationBatch batch;
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> axis(0, 7);
    std::uniform_int_distribution<uint8_t> mat(1, 4);
    int chunkBaseX = axis(rng) * 8;
    int chunkBaseY = axis(rng) * 8;
    int chunkBaseZ = axis(rng) * 8;
    switch (p) {
        case Pattern::P1_SingleClick: {
            batch.push_back({chunkBaseX + axis(rng), chunkBaseY + axis(rng), chunkBaseZ + axis(rng), mat(rng)});
            break;
        }
        case Pattern::P2_FillOperation: {
            // 4^3 = 64 cells in one chunk.
            for (int y = 0; y < 4; ++y) for (int z = 0; z < 4; ++z) for (int x = 0; x < 4; ++x)
                batch.push_back({chunkBaseX + x, chunkBaseY + y, chunkBaseZ + z, mat(rng)});
            break;
        }
        case Pattern::P3_MultiChunkBuild: {
            // 64 cells across 8 chunks (8 cells per chunk).
            for (int c = 0; c < 8; ++c) {
                int bx = chunkBaseX + (c % 2) * 8;
                int by = chunkBaseY + ((c / 2) % 2) * 8;
                int bz = chunkBaseZ + ((c / 4) % 2) * 8;
                for (int i = 0; i < 8; ++i) {
                    batch.push_back({bx + axis(rng), by + axis(rng), bz + axis(rng), mat(rng)});
                }
            }
            break;
        }
        case Pattern::P4_FloodFill: {
            // ~128 cells in one chunk.
            std::unordered_set<uint32_t> seen;
            while (batch.size() < 128) {
                int x = axis(rng); int y = axis(rng); int z = axis(rng);
                uint32_t key = x + 8 * (y + 8 * z);
                if (seen.count(key)) continue;
                seen.insert(key);
                batch.push_back({chunkBaseX + x, chunkBaseY + y, chunkBaseZ + z, mat(rng)});
            }
            break;
        }
        case Pattern::P5_StressBurst: {
            // 256 cells across 32 chunks.
            for (int c = 0; c < 32; ++c) {
                int bx = chunkBaseX + (c % 4) * 8;
                int by = chunkBaseY + ((c / 4) % 4) * 8;
                int bz = chunkBaseZ + ((c / 16) % 2) * 8;
                for (int i = 0; i < 8; ++i) {
                    batch.push_back({bx + axis(rng), by + axis(rng), bz + axis(rng), mat(rng)});
                }
            }
            break;
        }
    }
    return batch;
}

} // namespace scene

// ---------------------------------------------------------------------------
// 5. Harness: per-strategy × per-pattern × per-scene × per-seed measurement.
// ---------------------------------------------------------------------------

namespace harness {

struct Config {
    std::string strategy;
    std::string scene;
    std::string pattern;
    int seed = 0;
    int warmup = 10;
    int main_iters = 1000;
};

struct Result {
    std::string strategy, scene, pattern;
    int seed;
    double mean_us = 0, p50_us = 0, p95_us = 0, p99_us = 0, p999_us = 0, std_us = 0, min_us = 0, max_us = 0;
    double mean_per_edit_us = 0;
    uint64_t total_edits = 0;
    uint64_t total_skips = 0;
    uint64_t total_allocations = 0;
    uint64_t peak_nodes = 0;
    uint64_t coalesced_rebuilds = 0;
};

inline Result Run(const Config& cfg, std::array<uint8_t, svdag::kChunkSize>& baseVoxels) {
    // Build dag from base scene.
    svdag::VoxelSvdag64 dag;
    for (int idx = 0; idx < svdag::kChunkSize; ++idx) {
        if (baseVoxels[idx] != 0) {
            int x = idx & 7; int y = (idx / 8) & 7; int z = idx / 64;
            dag.SetCell(x, y, z, baseVoxels[idx]);
        }
    }
    // Apply pattern seed-specific mutations (single batch — not per-iter).
    // The mutations ARE the per-iter content; we re-randomize per iter to avoid amortizing.
    std::vector<double> perIterLatency;
    perIterLatency.reserve(cfg.main_iters);
    uint64_t totalEdits = 0, totalSkips = 0, totalAllocs = 0, peakNodes = 0, totalCoalesced = 0;
    // Warm-up.
    for (int i = 0; i < cfg.warmup; ++i) {
        EditStats s;
        auto batch = scene::GenerateMutations(static_cast<scene::Pattern>(
            std::stoi(cfg.pattern.substr(1, 1)) - 1), cfg.seed + i);
        if (cfg.strategy == "A_NaiveInPlace") strategy::A_NaiveInPlace(dag, batch, s);
        else if (cfg.strategy == "B_DirtyFlagDeferred") strategy::B_DirtyFlagDeferred(dag, batch, s);
        else if (cfg.strategy == "C_BatchCoalesce") strategy::C_BatchCoalesce(dag, batch, s);
        else if (cfg.strategy == "D_DoubleBufferSwap") strategy::D_DoubleBufferSwap(dag, batch, s);
        else if (cfg.strategy == "E_CopyOnWriteSnapshot") strategy::E_CopyOnWriteSnapshot(dag, batch, s);
    }
    // Reset after warmup to keep peak_nodes measurement clean.
    dag.Reset();
    for (int idx = 0; idx < svdag::kChunkSize; ++idx) {
        if (baseVoxels[idx] != 0) {
            int x = idx & 7; int y = (idx / 8) & 7; int z = idx / 64;
            dag.SetCell(x, y, z, baseVoxels[idx]);
        }
    }
    size_t baseNodes = dag.NodeCount();
    for (int i = 0; i < cfg.main_iters; ++i) {
        EditStats s;
        auto batch = scene::GenerateMutations(static_cast<scene::Pattern>(
            std::stoi(cfg.pattern.substr(1, 1)) - 1), cfg.seed + 100000 + i);
        if (cfg.strategy == "A_NaiveInPlace") strategy::A_NaiveInPlace(dag, batch, s);
        else if (cfg.strategy == "B_DirtyFlagDeferred") strategy::B_DirtyFlagDeferred(dag, batch, s);
        else if (cfg.strategy == "C_BatchCoalesce") strategy::C_BatchCoalesce(dag, batch, s);
        else if (cfg.strategy == "D_DoubleBufferSwap") strategy::D_DoubleBufferSwap(dag, batch, s);
        else if (cfg.strategy == "E_CopyOnWriteSnapshot") strategy::E_CopyOnWriteSnapshot(dag, batch, s);
        perIterLatency.push_back(s.latency_us);
        totalEdits += s.edit_count;
        totalSkips += s.skip_count;
        totalAllocs += s.allocations;
        peakNodes = std::max(peakNodes, s.peak_nodes);
        totalCoalesced += s.coalesced_rebuilds;
    }
    // Compute stats.
    Result r;
    r.strategy = cfg.strategy; r.scene = cfg.scene; r.pattern = cfg.pattern; r.seed = cfg.seed;
    r.total_edits = totalEdits; r.total_skips = totalSkips; r.total_allocations = totalAllocs;
    r.peak_nodes = peakNodes; r.coalesced_rebuilds = totalCoalesced;
    if (perIterLatency.empty()) return r;
    std::sort(perIterLatency.begin(), perIterLatency.end());
    double sum = std::accumulate(perIterLatency.begin(), perIterLatency.end(), 0.0);
    r.mean_us = sum / perIterLatency.size();
    r.p50_us = perIterLatency[perIterLatency.size() / 2];
    r.p95_us = perIterLatency[perIterLatency.size() * 95 / 100];
    r.p99_us = perIterLatency[perIterLatency.size() * 99 / 100];
    r.p999_us = perIterLatency[std::min(perIterLatency.size() - 1, perIterLatency.size() * 999 / 1000)];
    r.min_us = perIterLatency.front();
    r.max_us = perIterLatency.back();
    double var = 0.0;
    for (auto v : perIterLatency) var += (v - r.mean_us) * (v - r.mean_us);
    r.std_us = std::sqrt(var / perIterLatency.size());
    r.mean_per_edit_us = (totalEdits > 0) ? (sum / totalEdits) : 0.0;
    return r;
}

} // namespace harness

// ---------------------------------------------------------------------------
// 6. Main: 5 strategies × 5 scenes × 5 patterns × 5 seeds = 625 configs.
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    std::filesystem::path outPath = "build/results.csv";
    if (argc > 1) outPath = argv[1];
    std::filesystem::create_directories(outPath.parent_path());
    std::ofstream csv(outPath);
    csv << "strategy,scene,pattern,seed,mean_us,p50_us,p95_us,p99_us,p999_us,std_us,min_us,max_us,"
           "mean_per_edit_us,total_edits,total_skips,total_allocations,peak_nodes,coalesced_rebuilds\n";

    std::vector<std::string> strategies = {"A_NaiveInPlace", "B_DirtyFlagDeferred", "C_BatchCoalesce",
                                            "D_DoubleBufferSwap", "E_CopyOnWriteSnapshot"};
    std::vector<scene::Id> scenes = {scene::Id::UniformFloor, scene::Id::SparseWorld, scene::Id::MixedBiome,
                                      scene::Id::CaveStress, scene::Id::StackedSolid};
    std::vector<scene::Pattern> patterns = {scene::Pattern::P1_SingleClick, scene::Pattern::P2_FillOperation,
                                             scene::Pattern::P3_MultiChunkBuild, scene::Pattern::P4_FloodFill,
                                             scene::Pattern::P5_StressBurst};
    std::vector<int> seeds = {1, 7, 42, 1234, 31337};

    int total = strategies.size() * scenes.size() * patterns.size() * seeds.size();
    int done = 0;
    auto tStart = std::chrono::high_resolution_clock::now();
    for (const auto& strat : strategies) {
        for (auto sid : scenes) {
            std::array<uint8_t, svdag::kChunkSize> baseVoxels;
            scene::Build(sid, 12345, baseVoxels);
            for (auto pat : patterns) {
                for (int seed : seeds) {
                    harness::Config cfg{strat, scene::Name(sid), scene::PatternName(pat), seed, 10, 1000};
                    auto r = harness::Run(cfg, baseVoxels);
                    csv << r.strategy << "," << r.scene << "," << r.pattern << "," << r.seed << ","
                         << r.mean_us << "," << r.p50_us << "," << r.p95_us << "," << r.p99_us << ","
                         << r.p999_us << "," << r.std_us << "," << r.min_us << "," << r.max_us << ","
                         << r.mean_per_edit_us << "," << r.total_edits << "," << r.total_skips << ","
                         << r.total_allocations << "," << r.peak_nodes << "," << r.coalesced_rebuilds << "\n";
                    ++done;
                    if (done % 50 == 0) {
                        auto tNow = std::chrono::high_resolution_clock::now();
                        double elapsed = std::chrono::duration<double>(tNow - tStart).count();
                        std::printf("[%d/%d] elapsed %.1fs est total %.1fs\n", done, total, elapsed,
                                    elapsed * total / done);
                        std::fflush(stdout);
                    }
                }
            }
        }
    }
    csv.close();
    std::printf("Done. %d configs written to %s\n", done, outPath.string().c_str());
    return 0;
}