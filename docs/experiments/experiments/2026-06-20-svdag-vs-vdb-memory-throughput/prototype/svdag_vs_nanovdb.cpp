// SPDX-License-Identifier: MIT
//
// svdag_vs_nanovdb.cpp - Standalone C++26 prototype comparing SVDAG-on-64-tree
// (current ProjectV mainline storage, mirrors src/voxel/Sparse64Tree.hpp semantics)
// against a NanoVDB-like 4-level B+tree (Root[8] -> Upper[8] -> Lower[8] -> Leaf[8]).
// 4 levels of 2 = 16 cells per axis per root child; 8 root children cover 32^3.
//
// This is a structural comparison with the *essence* of openvdb/nanovdb/NanoVDB.h
// (multi-level fixed-depth B+tree with bitmask skips for uniform children).
// NanoVDB's actual layout uses 32^3/16^3/8^3 branching which is unfavorable
// for 32^3 chunks; this prototype uses 2^3 branching (octree-style) so the tree
// depth matches our chunk size and all bitmasks fit in u8. The trade-off
// (fixed-depth vs variable-depth) is the same as NanoVDB's.
//
// Scope per docs/experiments/AGENTS.md section 2: standalone research artifact,
// NOT part of ProjectV mainline.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     docs/experiments/experiments/2026-06-20-svdag-vs-vdb-memory-throughput/prototype/svdag_vs_nanovdb.cpp \
//     -o /tmp/svdag_vs_nanovdb
//
// Run:
//   /tmp/svdag_vs_nanovdb

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace projectv::proto {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

inline constexpr int kBitsPerAxis = 2;
inline constexpr int kNodeSide = 1 << kBitsPerAxis;
inline constexpr int kChildren = 1 << (3 * kBitsPerAxis);
inline constexpr u32 kLeafFlag = 0x80000000u;
inline constexpr u32 kNodeIndexMask = 0x7FFFFFFFu;
inline constexpr u32 kMaterialMask = 0xFFu;
inline constexpr u32 kInvalidIndex = kNodeIndexMask;

inline constexpr u32 MakeLeaf(u8 material) noexcept
{
	return kLeafFlag | static_cast<u32>(material);
}
inline constexpr bool IsLeaf(u32 slot) noexcept
{
	return (slot & kLeafFlag) != 0u;
}
inline constexpr u8 LeafMaterial(u32 slot) noexcept
{
	return static_cast<u8>(slot & kMaterialMask);
}
inline constexpr u32 NodeIndex(u32 slot) noexcept
{
	return slot & kNodeIndexMask;
}
inline constexpr int ChildSlotIndex(int sx, int sy, int sz) noexcept
{
	return sx + kNodeSide * (sy + kNodeSide * sz);
}

// =============================================================================
// SVDAG-on-64-tree (current ProjectV mainline storage).
// Mirror of src/voxel/Sparse64Tree.hpp semantics; standalone re-implementation.
// =============================================================================

class Svdag64 {
  public:
	struct alignas(8) Node {
		u64 fillMask = 0;
		std::array<u32, kChildren> slots{};
		u64 structuralHash = 0;
		u32 refCount = 0;
	};
	static_assert(sizeof(Node) == 280, "Node must be exactly 280 B");

	explicit Svdag64(int side) : side_(side) { Reset(); }

	void Reset()
	{
		nodes_.clear();
		dedupIndex_.clear();
		dedupEnabled_ = false;
		rootSlot_ = MakeLeaf(0);
	}

	bool IsDedupEnabled() const noexcept { return dedupEnabled_; }

	void SetDedupEnabled(bool enabled)
	{
		if (enabled == dedupEnabled_)
			return;
		dedupEnabled_ = enabled;
		for (auto &n : nodes_)
			n.refCount = (n.refCount > 0) ? 1 : 0;
		dedupIndex_.clear();
		if (enabled) {
			for (u32 i = 0; i < nodes_.size(); ++i) {
				RebuildIndex(i);
			}
		}
	}

	u8 GetCell(int x, int y, int z) const noexcept
	{
		if (!Contains(x, y, z))
			return 0;
		int maxDepth = MaxDepth();
		if (maxDepth == 0)
			return IsLeaf(rootSlot_) ? LeafMaterial(rootSlot_) : 0;
		u32 slot = rootSlot_;
		for (int level = maxDepth; level > 0; --level) {
			if (IsLeaf(slot))
				return LeafMaterial(slot);
			const Node &node = nodes_[NodeIndex(slot)];
			int sx = SubCoord(x, level);
			int sy = SubCoord(y, level);
			int sz = SubCoord(z, level);
			int child = ChildSlotIndex(sx, sy, sz);
			if (((node.fillMask >> child) & 1ull) == 0)
				return 0;
			slot = node.slots[child];
		}
		return IsLeaf(slot) ? LeafMaterial(slot) : 0;
	}

	void SetCell(int x, int y, int z, u8 material)
	{
		if (!Contains(x, y, z))
			return;
		int maxDepth = MaxDepth();
		if (maxDepth == 0) {
			rootSlot_ = MakeLeaf(material);
			return;
		}
		rootSlot_ = SetCellRec(rootSlot_, x, y, z, material, maxDepth);
	}

	bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < side_ && y >= 0 && y < side_ && z >= 0 && z < side_;
	}

	size_t NodeCount() const noexcept { return nodes_.size(); }
	size_t NonAirCount() const { return CountNonAirRec(rootSlot_, MaxDepth()); }

	size_t MemoryBytes() const noexcept
	{
		size_t bytes = sizeof(*this);
		bytes += nodes_.capacity() * sizeof(Node);
		for (const auto &kv : dedupIndex_) {
			bytes += sizeof(kv);
		}
		return bytes;
	}

  private:
	int MaxDepth() const noexcept
	{
		if (side_ <= 1)
			return 0;
		int depth = 1;
		int n = kNodeSide;
		while (n < side_) {
			n *= kNodeSide;
			++depth;
		}
		return depth;
	}

	int SubCoord(int coord, int level) const noexcept
	{
		int shift = (level - 1) * kBitsPerAxis;
		return (coord >> shift) & (kNodeSide - 1);
	}

	static u64 MixSplitMix(u64 seed, u64 value) noexcept
	{
		u64 z = seed + value;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
		return z ^ (z >> 31);
	}

	u64 ComputeHash(const Node &node) const noexcept
	{
		u64 h = MixSplitMix(0x9E3779B97F4A7C15ull, node.fillMask);
		for (u32 s : node.slots)
			h = MixSplitMix(h, s);
		return h;
	}

	void RebuildIndex(u32 idx)
	{
		Node &node = nodes_[idx];
		node.structuralHash = ComputeHash(node);
		dedupIndex_.emplace(node.structuralHash, idx);
	}

	bool Equal(const Node &a, const Node &b) const noexcept
	{
		if (a.fillMask != b.fillMask)
			return false;
		if (a.structuralHash != b.structuralHash)
			return false;
		for (size_t i = 0; i < a.slots.size(); ++i) {
			if (a.slots[i] != b.slots[i])
				return false;
		}
		return true;
	}

	u32 FindEquivalent(const Node &candidate)
	{
		if (!dedupEnabled_)
			return kInvalidIndex;
		u64 h = ComputeHash(candidate);
		auto range = dedupIndex_.equal_range(h);
		for (auto it = range.first; it != range.second; ++it) {
			if (Equal(nodes_[it->second], candidate))
				return it->second;
		}
		return kInvalidIndex;
	}

	void RemoveFromIndex(u32 nodeIndex)
	{
		const u64 h = nodes_[nodeIndex].structuralHash;
		auto range = dedupIndex_.equal_range(h);
		for (auto it = range.first; it != range.second; ++it) {
			if (it->second == nodeIndex) {
				dedupIndex_.erase(it);
				return;
			}
		}
	}

	u32 MarkUnique(u32 nodeIndex)
	{
		if (!dedupEnabled_ || nodes_[nodeIndex].refCount == 1) {
			return nodeIndex;
		}
		--nodes_[nodeIndex].refCount;
		RemoveFromIndex(nodeIndex);
		Node copy = nodes_[nodeIndex];
		copy.refCount = 1;
		const u32 newIndex = static_cast<u32>(nodes_.size());
		nodes_.push_back(copy);
		RebuildIndex(newIndex);
		return newIndex;
	}

	u32 AllocateNode()
	{
		Node n{};
		for (u32 &s : n.slots)
			s = MakeLeaf(0);
		n.refCount = 1;
		if (dedupEnabled_) {
			n.structuralHash = ComputeHash(n);
			u32 existing = FindEquivalent(n);
			if (existing != kInvalidIndex) {
				++nodes_[existing].refCount;
				return existing;
			}
		}
		u32 idx = static_cast<u32>(nodes_.size());
		nodes_.push_back(n);
		if (dedupEnabled_)
			RebuildIndex(idx);
		return idx;
	}

	u32 SetCellRec(u32 slot, int x, int y, int z, u8 material, int level)
	{
		if (level <= 0)
			return MakeLeaf(material);
		int sx = SubCoord(x, level);
		int sy = SubCoord(y, level);
		int sz = SubCoord(z, level);
		int child = ChildSlotIndex(sx, sy, sz);

		if (IsLeaf(slot)) {
			slot = AllocateNode();
		}
		u32 nodeIdx = MarkUnique(NodeIndex(slot));
		u32 newSlot = SetCellRec(nodes_[nodeIdx].slots[child],
								 x, y, z, material, level - 1);
		nodes_[nodeIdx].slots[child] = newSlot;
		nodes_[nodeIdx].fillMask |= (1ull << child);

		if (dedupEnabled_) {
			RemoveFromIndex(nodeIdx);
			nodes_[nodeIdx].structuralHash = ComputeHash(nodes_[nodeIdx]);
			dedupIndex_.emplace(nodes_[nodeIdx].structuralHash, nodeIdx);
		}
		return nodeIdx;
	}

	size_t CountNonAirRec(u32 slot, int level) const noexcept
	{
		if (IsLeaf(slot))
			return LeafMaterial(slot) != 0 ? 1 : 0;
		if (level <= 0)
			return 0;
		const Node &node = nodes_[NodeIndex(slot)];
		if (node.fillMask == 0)
			return 0;
		size_t count = 0;
		for (int i = 0; i < kChildren; ++i) {
			if (((node.fillMask >> i) & 1ull) != 0) {
				count += CountNonAirRec(node.slots[i], level - 1);
			}
		}
		return count;
	}

	int side_;
	std::vector<Node> nodes_;
	std::unordered_multimap<u64, u32> dedupIndex_;
	bool dedupEnabled_ = false;
	u32 rootSlot_ = MakeLeaf(0);
};

// =============================================================================
// NanoVDB-like 4-level B+tree with 2^3 = 8 children per node (octree-style).
// 4 levels of 2 = 16 cells per axis per root child; 8 root children cover 32^3.
// Same essential structure as openvdb/nanovdb/NanoVDB.h but with octree
// branching (vs NanoVDB's 32^3/16^3/8^3) so all bitmasks fit in u8.
//
// Per-node sizes (this implementation):
//   Root:    1 B childMap + 8*4 B = 36 B (padded to 40 B with alignment)
//   Upper:   1 B valueMask + 1 B tileMask + 8*4 B = 34 B (padded to 40 B)
//   Lower:   1 B valueMask + 1 B tileMask + 8*4 B = 34 B (padded to 40 B)
//   Leaf:    1 B valueMask + 8 B voxels = 9 B (padded to 16 B)
// + fixed grid overhead 736 B (GridData 672 B + TreeData 64 B per NanoVDB.h).
// =============================================================================

class NanovdbLike {
  public:
	static constexpr int kBitsPerAxis = 1;
	static constexpr int kNodeSide = 1 << kBitsPerAxis;
	static constexpr int kTilesPerNode = 1 << (3 * kBitsPerAxis);
	static constexpr int kLeafVoxels = 1 << (3 * kBitsPerAxis);

	struct alignas(8) RootNode {
		u8 childMap = 0;
		u32 childrenOrValues[kTilesPerNode] = {};
	};
	static_assert(sizeof(RootNode) == 40);

	struct alignas(8) InnerNode {
		u8 valueMask = 0;
		u8 tileMask = 0;
		u32 valuesOrIds[kTilesPerNode] = {};
	};
	static_assert(sizeof(InnerNode) == 40);

	struct alignas(8) LeafNode {
		u8 valueMask = 0;
		u8 voxels[kLeafVoxels] = {};
	};
	static_assert(sizeof(LeafNode) == 16);

	explicit NanovdbLike(int side) : side_(side) { Reset(); }

	void Reset()
	{
		root_ = std::make_unique<RootNode>();
		uppers_.clear();
		lowers_.clear();
		leaves_.clear();
	}

	u8 GetCell(int x, int y, int z) const
	{
		if (!Contains(x, y, z))
			return 0;
		const RootNode *root = root_.get();
		int uChild = SubCoord(x, 3) + 2 * (SubCoord(y, 3) + 2 * SubCoord(z, 3));
		if (root->childMap == 0) {
			return LowByte(root->childrenOrValues[0]);
		}
		if (((root->childMap >> uChild) & 1u) == 0) {
			return LowByte(root->childrenOrValues[uChild]);
		}
		u32 upperId = root->childrenOrValues[uChild];
		return GetInUpper(uppers_[upperId].get(), x, y, z);
	}

	void SetCell(int x, int y, int z, u8 material)
	{
		if (!Contains(x, y, z))
			return;
		if (material == 0)
			return; // 0 = background, no-op
		RootNode *root = root_.get();
		int uChild = SubCoord(x, 3) + 2 * (SubCoord(y, 3) + 2 * SubCoord(z, 3));
		if (((root->childMap >> uChild) & 1u) == 0) {
			// Tile is not a sub-tree yet. Promote to a uniform-0 sub-tree to
			// avoid "uniform-tile lie" (we can't represent a single non-zero
			// voxel within a uniform sub-cube without descending).
			root->childMap |= (1u << uChild);
			uppers_.push_back(std::make_unique<InnerNode>());
			u32 upperId = static_cast<u32>(uppers_.size() - 1);
			FillInnerWithValue(uppers_[upperId].get(), 0);
			root->childrenOrValues[uChild] = upperId;
		}
		u32 upperId = root->childrenOrValues[uChild];
		SetInUpper(uppers_[upperId].get(), x, y, z, material);
	}

	bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < side_ && y >= 0 && y < side_ && z >= 0 && z < side_;
	}

	size_t NodeCount() const noexcept
	{
		return 1 + uppers_.size() + lowers_.size() + leaves_.size();
	}
	size_t NonAirCount() const
	{
		if (!root_)
			return 0;
		const RootNode *root = root_.get();
		size_t count = 0;
		if (root->childMap == 0) {
			size_t per = static_cast<size_t>(side_) * side_ * side_;
			return (LowByte(root->childrenOrValues[0]) != 0) ? per : 0;
		}
		for (int i = 0; i < kTilesPerNode; ++i) {
			if (((root->childMap >> i) & 1u) == 0) {
				if (LowByte(root->childrenOrValues[i]) != 0)
					count += SidePerRootChild();
				continue;
			}
			count += CountNonAirInUpper(uppers_[root->childrenOrValues[i]].get());
		}
		return count;
	}

	size_t SidePerRootChild() const noexcept
	{
		return (side_ * side_ * side_) / kTilesPerNode;
	}
	size_t MemoryBytes() const noexcept
	{
		size_t bytes = sizeof(*this);
		bytes += sizeof(RootNode);
		bytes += 736;
		bytes += uppers_.capacity() * sizeof(InnerNode);
		bytes += lowers_.capacity() * sizeof(InnerNode);
		bytes += leaves_.capacity() * sizeof(LeafNode);
		return bytes;
	}

  private:
	static u8 LowByte(u32 v) noexcept { return static_cast<u8>(v & 0xFFu); }
	static int SubCoord(int coord, int level) noexcept
	{
		int shift = (level - 1) * kBitsPerAxis;
		return (coord >> shift) & (kNodeSide - 1);
	}
	static int ChildSlot(int x, int y, int z, int level) noexcept
	{
		return SubCoord(x, level) + kNodeSide * (SubCoord(y, level) + kNodeSide * SubCoord(z, level));
	}
	static int VoxelInLeaf(int x, int y, int z) noexcept
	{
		return (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
	}

	static void FillInnerWithValue(InnerNode *in, u8 material)
	{
		u32 v = static_cast<u32>(material);
		for (int i = 0; i < kTilesPerNode; ++i)
			in->valuesOrIds[i] = v;
		in->valueMask = 0xFF;
	}

	u8 GetInUpper(const InnerNode *up, int x, int y, int z) const
	{
		int lChild = ChildSlot(x, y, z, 2);
		if (((up->valueMask >> lChild) & 1u) != 0) {
			return LowByte(up->valuesOrIds[lChild]);
		}
		if (((up->tileMask >> lChild) & 1u) == 0)
			return 0;
		u32 lowerId = up->valuesOrIds[lChild];
		return GetInLower(lowers_[lowerId].get(), x, y, z);
	}

	u8 GetInLower(const InnerNode *lo, int x, int y, int z) const
	{
		int lfChild = ChildSlot(x, y, z, 1);
		if (((lo->valueMask >> lfChild) & 1u) != 0) {
			return LowByte(lo->valuesOrIds[lfChild]);
		}
		if (((lo->tileMask >> lfChild) & 1u) == 0)
			return 0;
		u32 leafId = lo->valuesOrIds[lfChild];
		const LeafNode *lf = leaves_[leafId].get();
		int vIdx = VoxelInLeaf(x, y, z);
		return lf->voxels[vIdx];
	}

	void SetInUpper(InnerNode *up, int x, int y, int z, u8 material)
	{
		int lChild = ChildSlot(x, y, z, 2);
		if (((up->valueMask >> lChild) & 1u) != 0) {
			u8 existing = LowByte(up->valuesOrIds[lChild]);
			if (existing == material)
				return;
			// Tile was uniform; promote to sub-tree to represent mixed values.
			up->valueMask &= ~(1u << lChild);
			lowers_.push_back(std::make_unique<InnerNode>());
			u32 lowerId = static_cast<u32>(lowers_.size() - 1);
			FillInnerWithValue(lowers_[lowerId].get(), existing);
			SetInLower(lowers_[lowerId].get(), x, y, z, material);
			up->tileMask |= (1u << lChild);
			up->valuesOrIds[lChild] = lowerId;
			return;
		}
		if (((up->tileMask >> lChild) & 1u) == 0) {
			// No sub-tree yet. Promote to a uniform-0 sub-tree.
			up->tileMask |= (1u << lChild);
			lowers_.push_back(std::make_unique<InnerNode>());
			u32 lowerId = static_cast<u32>(lowers_.size() - 1);
			FillInnerWithValue(lowers_[lowerId].get(), 0);
			SetInLower(lowers_[lowerId].get(), x, y, z, material);
			up->valuesOrIds[lChild] = lowerId;
			return;
		}
		u32 lowerId = up->valuesOrIds[lChild];
		SetInLower(lowers_[lowerId].get(), x, y, z, material);
	}

	void SetInLower(InnerNode *lo, int x, int y, int z, u8 material)
	{
		int lfChild = ChildSlot(x, y, z, 1);
		if (((lo->valueMask >> lfChild) & 1u) != 0) {
			u8 existing = LowByte(lo->valuesOrIds[lfChild]);
			if (existing == material)
				return;
			lo->valueMask &= ~(1u << lfChild);
			leaves_.push_back(std::make_unique<LeafNode>());
			u32 leafId = static_cast<u32>(leaves_.size() - 1);
			for (int i = 0; i < kLeafVoxels; ++i)
				leaves_[leafId]->voxels[i] = existing;
			int vIdx = VoxelInLeaf(x, y, z);
			leaves_[leafId]->voxels[vIdx] = material;
			leaves_[leafId]->valueMask = 0xFF;
			lo->tileMask |= (1u << lfChild);
			lo->valuesOrIds[lfChild] = leafId;
			return;
		}
		if (((lo->tileMask >> lfChild) & 1u) == 0) {
			lo->tileMask |= (1u << lfChild);
			leaves_.push_back(std::make_unique<LeafNode>());
			u32 leafId = static_cast<u32>(leaves_.size() - 1);
			for (int i = 0; i < kLeafVoxels; ++i)
				leaves_[leafId]->voxels[i] = 0;
			int vIdx = VoxelInLeaf(x, y, z);
			leaves_[leafId]->voxels[vIdx] = material;
			leaves_[leafId]->valueMask = 1u << vIdx;
			lo->valuesOrIds[lfChild] = leafId;
			return;
		}
		u32 leafId = lo->valuesOrIds[lfChild];
		LeafNode *lf = leaves_[leafId].get();
		int vIdx = VoxelInLeaf(x, y, z);
		lf->voxels[vIdx] = material;
		lf->valueMask |= (1u << vIdx);
	}

	size_t CountNonAirInUpper(const InnerNode *up) const
	{
		size_t count = 0;
		for (int i = 0; i < kTilesPerNode; ++i) {
			if (((up->valueMask >> i) & 1u) != 0) {
				if (LowByte(up->valuesOrIds[i]) != 0)
					count += SidePerUpperChild();
				continue;
			}
			if (((up->tileMask >> i) & 1u) == 0)
				continue;
			count += CountNonAirInLower(lowers_[up->valuesOrIds[i]].get());
		}
		return count;
	}
	size_t CountNonAirInLower(const InnerNode *lo) const
	{
		size_t count = 0;
		for (int i = 0; i < kLeafVoxels; ++i) {
			if (((lo->valueMask >> i) & 1u) != 0) {
				if (LowByte(lo->valuesOrIds[i]) != 0)
					count += SidePerLowerChild();
				continue;
			}
			if (((lo->tileMask >> i) & 1u) == 0)
				continue;
			count += CountNonAirInLeaf(leaves_[lo->valuesOrIds[i]].get());
		}
		return count;
	}
	size_t SidePerUpperChild() const noexcept
	{
		return (side_ * side_ * side_) / (kTilesPerNode * kTilesPerNode);
	}
	size_t SidePerLowerChild() const noexcept
	{
		return kLeafVoxels;
	}
	size_t CountNonAirInLeaf(const LeafNode *lf) const
	{
		size_t count = 0;
		for (int i = 0; i < kLeafVoxels; ++i) {
			if (lf->voxels[i] != 0)
				++count;
		}
		return count;
	}

	int side_;
	std::unique_ptr<RootNode> root_;
	std::vector<std::unique_ptr<InnerNode>> uppers_;
	std::vector<std::unique_ptr<InnerNode>> lowers_;
	std::vector<std::unique_ptr<LeafNode>> leaves_;
};

// =============================================================================
// Scene generators.
// =============================================================================

struct Scene {
	std::string name;
	int side;
	std::vector<u8> voxels;
};

inline Scene MakeEmpty(int side, std::string name = "empty_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	return s;
}

inline Scene MakeSolid(int side, u8 material, std::string name = "solid_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, material);
	return s;
}

inline Scene MakeGround(int side, int layers, std::string name = "ground_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	for (int y = 0; y < layers; ++y) {
		for (int z = 0; z < side; ++z) {
			for (int x = 0; x < side; ++x) {
				s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)] = 1;
			}
		}
	}
	return s;
}

inline Scene MakeCheckered(int side, std::string name = "checkered_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	for (int z = 0; z < side; ++z) {
		for (int y = 0; y < side; ++y) {
			for (int x = 0; x < side; ++x) {
				if ((x + y + z) % 2 == 0) {
					s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)] = 1;
				}
			}
		}
	}
	return s;
}

inline Scene MakeBrickPattern(int side, std::string name = "brick_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	const int brickSide = 4;
	auto placeBrick = [&](int ox, int oy, int oz, u8 material) {
		for (int z = 0; z < brickSide; ++z)
			for (int y = 0; y < brickSide; ++y)
				for (int x = 0; x < brickSide; ++x)
					s.voxels[static_cast<size_t>(ox + x) + side * (static_cast<size_t>(oy + y) + side * (oz + z))] = material;
	};
	int step = side / 4;
	for (int zi = 0; zi < 4; ++zi)
		for (int yi = 0; yi < 4; ++yi)
			for (int xi = 0; xi < 4; ++xi)
				placeBrick(xi * step, yi * step, zi * step, 1 + ((xi + yi + zi) % 3));
	return s;
}

inline Scene MakeVoxelLabLike(int side, unsigned seed, std::string name = "voxel_lab_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> dist(0.0, 1.0);
	for (int z = 0; z < side; ++z) {
		for (int y = 0; y < side; ++y) {
			double heightFactor = 1.0 - (static_cast<double>(y) / side);
			double baseDensity = 0.20 * heightFactor + 0.02;
			for (int x = 0; x < side; ++x) {
				double r = dist(rng);
				if (r < baseDensity) {
					int material = 1 + static_cast<int>(r / baseDensity * 6.0);
					if (material > 6)
						material = 6;
					s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)] = static_cast<u8>(material);
				}
			}
		}
	}
	return s;
}

inline Scene MakeSparseRandom(int side, double density, unsigned seed, std::string name = "sparse_random_32")
{
	Scene s{std::move(name), side, {}};
	s.voxels.assign(static_cast<size_t>(side) * side * side, 0);
	std::mt19937 rng(seed);
	std::uniform_real_distribution<double> dist(0.0, 1.0);
	std::uniform_int_distribution<int> mat(1, 3);
	for (size_t i = 0; i < s.voxels.size(); ++i) {
		if (dist(rng) < density)
			s.voxels[i] = static_cast<u8>(mat(rng));
	}
	return s;
}

// =============================================================================
// Build tree from scene + correctness check.
// =============================================================================

template <typename Tree>
Tree BuildTree(const Scene &s)
{
	Tree t(s.side);
	int side = s.side;
	for (int z = 0; z < side; ++z) {
		for (int y = 0; y < side; ++y) {
			for (int x = 0; x < side; ++x) {
				u8 v = s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)];
				if (v != 0)
					t.SetCell(x, y, z, v);
			}
		}
	}
	return t;
}

template <typename Tree>
size_t VerifyTree(const Tree &t, const Scene &s)
{
	int side = s.side;
	size_t mismatches = 0;
	for (int z = 0; z < side; ++z) {
		for (int y = 0; y < side; ++y) {
			for (int x = 0; x < side; ++x) {
				u8 expected = s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)];
				u8 actual = t.GetCell(x, y, z);
				if (expected != actual)
					++mismatches;
			}
		}
	}
	return mismatches;
}

// =============================================================================
// Benchmark harness.
// =============================================================================

struct BenchResult {
	std::string tree_name;
	std::string scene_name;
	size_t side;
	size_t total_bytes;
	size_t non_air_voxels;
	double bytes_per_non_air;
	size_t unique_nodes;
	double set_cell_us_mean;
	double set_cell_us_p95;
	double set_cell_us_p99;
	double get_cell_ns_mean;
	double get_cell_ns_p95;
	double get_cell_ns_p99;
	double verify_mismatches;
	double tree_build_ms;
};

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
};

inline Stats ComputeStats(std::vector<double> samples)
{
	std::sort(samples.begin(), samples.end());
	double sum = 0.0;
	for (double v : samples)
		sum += v;
	Stats s{};
	s.mean = sum / static_cast<double>(samples.size());
	s.median = samples[samples.size() / 2];
	s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
	return s;
}

template <typename Tree>
BenchResult BenchTree(const Scene &s, std::string_view tree_name, bool dedup, unsigned seed)
{
	int side = s.side;

	{
		Tree warm(side);
		if constexpr (requires { warm.SetDedupEnabled(true); }) {
			warm.SetDedupEnabled(dedup);
		}
		for (int z = 0; z < side; z += 4) {
			for (int y = 0; y < side; y += 4) {
				for (int x = 0; x < side; x += 4) {
					u8 v = s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)];
					if (v != 0)
						warm.SetCell(x, y, z, v);
				}
			}
		}
		volatile u8 sink = warm.GetCell(side / 2, side / 2, side / 2);
		(void)sink;
	}

	auto tBuildStart = std::chrono::steady_clock::now();
	Tree t(side);
	if constexpr (requires { t.SetDedupEnabled(true); }) {
		t.SetDedupEnabled(dedup);
	}
	for (int z = 0; z < side; ++z) {
		for (int y = 0; y < side; ++y) {
			for (int x = 0; x < side; ++x) {
				u8 v = s.voxels[static_cast<size_t>(x) + side * (static_cast<size_t>(y) + side * z)];
				if (v != 0)
					t.SetCell(x, y, z, v);
			}
		}
	}
	auto tBuildEnd = std::chrono::steady_clock::now();
	double buildMs = std::chrono::duration<double, std::milli>(tBuildEnd - tBuildStart).count();

	size_t mismatches = VerifyTree(t, s);

	constexpr int kSetIters = 1000;
	std::vector<double> setUsSamples;
	setUsSamples.reserve(kSetIters);
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> coord(0, side - 1);
	std::uniform_int_distribution<int> mat(1, 6);
	for (int i = 0; i < kSetIters; ++i) {
		int x = coord(rng), y = coord(rng), z = coord(rng);
		u8 m = static_cast<u8>(mat(rng));
		auto t0 = std::chrono::steady_clock::now();
		t.SetCell(x, y, z, m);
		auto t1 = std::chrono::steady_clock::now();
		double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
		setUsSamples.push_back(us);
	}
	Stats setStats = ComputeStats(std::move(setUsSamples));

	constexpr int kGetIters = 10000;
	std::vector<double> getNsSamples;
	getNsSamples.reserve(kGetIters);
	volatile u8 sink = 0;
	for (int i = 0; i < kGetIters; ++i) {
		int x = coord(rng), y = coord(rng), z = coord(rng);
		auto t0 = std::chrono::steady_clock::now();
		u8 v = t.GetCell(x, y, z);
		auto t1 = std::chrono::steady_clock::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		getNsSamples.push_back(ns);
		sink ^= v;
	}
	Stats getStats = ComputeStats(std::move(getNsSamples));
	(void)sink;

	size_t bytes = t.MemoryBytes();
	size_t nonAir = t.NonAirCount();

	BenchResult r{};
	r.tree_name = std::string(tree_name);
	r.scene_name = s.name;
	r.side = static_cast<size_t>(side);
	r.total_bytes = bytes;
	r.non_air_voxels = nonAir;
	r.bytes_per_non_air = nonAir > 0 ? static_cast<double>(bytes) / static_cast<double>(nonAir) : 0.0;
	r.unique_nodes = t.NodeCount();
	r.set_cell_us_mean = setStats.mean;
	r.set_cell_us_p95 = setStats.p95;
	r.set_cell_us_p99 = setStats.p99;
	r.get_cell_ns_mean = getStats.mean;
	r.get_cell_ns_p95 = getStats.p95;
	r.get_cell_ns_p99 = getStats.p99;
	r.verify_mismatches = static_cast<double>(mismatches);
	r.tree_build_ms = buildMs;
	return r;
}

// =============================================================================
// Output.
// =============================================================================

inline void WriteCsv(const std::vector<BenchResult> &rs, const std::filesystem::path &path)
{
	std::ofstream out(path);
	out << "tree,scene,side,total_bytes,non_air_voxels,bytes_per_non_air,"
		<< "unique_nodes,set_us_mean,set_us_p95,set_us_p99,"
		<< "get_ns_mean,get_ns_p95,get_ns_p99,verify_mismatches,build_ms\n";
	for (const auto &r : rs) {
		out << r.tree_name << ',' << r.scene_name << ',' << r.side << ','
			<< r.total_bytes << ',' << r.non_air_voxels << ','
			<< (r.non_air_voxels > 0 ? std::to_string(r.bytes_per_non_air) : "inf") << ','
			<< r.unique_nodes << ','
			<< r.set_cell_us_mean << ',' << r.set_cell_us_p95 << ',' << r.set_cell_us_p99 << ','
			<< r.get_cell_ns_mean << ',' << r.get_cell_ns_p95 << ',' << r.get_cell_ns_p99 << ','
			<< r.verify_mismatches << ',' << r.tree_build_ms << '\n';
	}
}

inline void WriteResultsMd(const std::vector<BenchResult> &rs, const std::filesystem::path &path)
{
	std::ofstream out(path);
	out << "# Results — SVDAG-on-64-tree vs NanoVDB-like (32^3 regions)\n\n";
	out << "Host: AMD Ryzen 7 5800X, clang 22.1.6, -O3 -march=native -DNDEBUG.\n";
	out << "All scenes are 32^3 = 32768 voxels. Build = SetCell over full scene.\n";
	out << "SetCell bench = 1000 random SetCell calls after build (microseconds).\n";
	out << "GetCell bench = 10000 random GetCell calls after build (nanoseconds).\n\n";

	out << "## Per-tree summary\n\n";
	out << "| Tree | Scene | Non-air | Total bytes | B/non-air | SetCell mean us | SetCell p99 us | GetCell mean ns | GetCell p99 ns | Build ms | Verify mism |\n";
	out << "|:-----|:------|--------:|------------:|----------:|----------------:|---------------:|----------------:|---------------:|---------:|------------:|\n";
	for (const auto &r : rs) {
		out << "| " << r.tree_name
			<< " | " << r.scene_name
			<< " | " << r.non_air_voxels
			<< " | " << r.total_bytes
			<< " | " << (r.non_air_voxels > 0 ? std::to_string(static_cast<int>(r.bytes_per_non_air * 100) / 100.0) : std::string("inf"))
			<< " | " << static_cast<int>(r.set_cell_us_mean * 100) / 100.0
			<< " | " << static_cast<int>(r.set_cell_us_p99 * 100) / 100.0
			<< " | " << static_cast<int>(r.get_cell_ns_mean * 100) / 100.0
			<< " | " << static_cast<int>(r.get_cell_ns_p99 * 100) / 100.0
			<< " | " << static_cast<int>(r.tree_build_ms * 100) / 100.0
			<< " | " << static_cast<int>(r.verify_mismatches) << " |\n";
	}
	out << "\n## Notes\n\n";
	out << "- SVDAG-on-64-tree (no dedup) = current ProjectV mainline per-chunk storage baseline.\n";
	out << "- SVDAG-on-64-tree (dedup ON) = Stage 1.2 lazy SVDAG (SetDeduplicationEnabled(true)).\n";
	out << "- NanoVDB-like = 4-level B+tree (Root[8] -> Upper[8] -> Lower[8] -> Leaf[8]).\n";
	out << "  Same essential structure as openvdb/nanovdb/NanoVDB.h (multi-level fixed-depth\n";
	out << "  B+tree with bitmask skips for uniform children). This prototype uses 2^3=8\n";
	out << "  branching (octree-style, 4 levels cover 32^3 chunks exactly) so all bitmasks\n";
	out << "  fit in u8 and the tree depth matches our chunk size. NanoVDB's actual layout\n";
	out << "  uses 32^3/16^3/8^3 branching which is unfavorable for 32^3 chunks (each upper\n";
	out << "  covers 64^3 cells, way more than needed).\n";
	out << "  Per-node sizes: Root=40 B, Upper=40 B, Lower=40 B, Leaf=16 B +\n";
	out << "  fixed 736 B grid overhead (GridData 672 B + TreeData 64 B per NanoVDB.h).\n";
	out << "- verify_mismatches MUST be 0 for all rows (byte-exact correctness vs flat voxels).\n";
}

} // namespace projectv::proto

int main()
{
	using namespace projectv::proto;

	constexpr int kSide = 32;
	constexpr unsigned kSeed = 0xC0FFEEu;

	std::vector<Scene> scenes = {
		MakeEmpty(kSide),
		MakeSolid(kSide, 1, "solid_32"),
		MakeGround(kSide, 4, "ground_32"),
		MakeCheckered(kSide),
		MakeBrickPattern(kSide),
		MakeVoxelLabLike(kSide, kSeed),
		MakeSparseRandom(kSide, 0.10, kSeed),
	};

	std::vector<BenchResult> results;
	results.reserve(scenes.size() * 3);

	for (const auto &s : scenes) {
		std::printf("  scene %-18s nonAir=%zu\n", s.name.c_str(),
					static_cast<size_t>(std::count_if(s.voxels.begin(), s.voxels.end(), [](u8 v) { return v != 0; })));
		std::fflush(stdout);

		std::printf("    -> svdag64_no_dedup ... ");
		std::fflush(stdout);
		results.push_back(BenchTree<Svdag64>(s, "svdag64_no_dedup", false, kSeed));
		std::printf("done\n");
		std::fflush(stdout);

		std::printf("    -> svdag64_dedup_on  ... ");
		std::fflush(stdout);
		results.push_back(BenchTree<Svdag64>(s, "svdag64_dedup_on", true, kSeed));
		std::printf("done\n");
		std::fflush(stdout);

		std::printf("    -> nanovdb_like      ... ");
		std::fflush(stdout);
		results.push_back(BenchTree<NanovdbLike>(s, "nanovdb_like", false, kSeed));
		std::printf("done\n");
		std::fflush(stdout);
	}

	std::filesystem::path outDir = std::filesystem::path(__FILE__).parent_path();
	std::filesystem::path csvPath = outDir / "results.csv";
	std::filesystem::path mdPath = outDir / "RESULTS.md";
	WriteCsv(results, csvPath);
	WriteResultsMd(results, mdPath);

	std::printf("\nResults written:\n  %s\n  %s\n", csvPath.c_str(), mdPath.c_str());
	std::printf("Verify mismatches: %s\n",
				std::all_of(results.begin(), results.end(),
							[](const BenchResult &r) { return r.verify_mismatches == 0; })
					? "all 0 (PASS)"
					: "SOME > 0 (FAIL)");
	return 0;
}
