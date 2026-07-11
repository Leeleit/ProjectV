#pragma once

#include <array> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace projectv::voxel {

inline constexpr int kSparse64BitsPerAxis = 2;
inline constexpr int kSparse64NodeSide = 1 << kSparse64BitsPerAxis;
inline constexpr int kSparse64ChildrenPerNode = 1 << (3 * kSparse64BitsPerAxis);

inline constexpr uint32_t kSparse64LeafFlag = 0x80000000u;
inline constexpr uint32_t kSparse64HomogeneousFlag = 0x40000000u;
inline constexpr uint32_t kSparse64NodeIndexMask = 0x3FFFFFFFu;
inline constexpr uint32_t kSparse64MaterialMask = 0xFFu;
inline constexpr uint32_t kSparse64InvalidNodeIndex = kSparse64NodeIndexMask;

constexpr uint32_t MakeSparse64Leaf(uint8_t material) noexcept
{
	return kSparse64LeafFlag | static_cast<uint32_t>(material);
}

constexpr uint32_t MakeSparse64Homogeneous(uint8_t material) noexcept
{
	return kSparse64LeafFlag | kSparse64HomogeneousFlag | static_cast<uint32_t>(material);
}

constexpr bool IsSparse64Leaf(uint32_t slot) noexcept
{
	return (slot & (kSparse64LeafFlag | kSparse64HomogeneousFlag)) == kSparse64LeafFlag;
}

constexpr bool IsSparse64Homogeneous(uint32_t slot) noexcept
{
	return (slot & (kSparse64LeafFlag | kSparse64HomogeneousFlag)) ==
		   (kSparse64LeafFlag | kSparse64HomogeneousFlag);
}

constexpr uint8_t Sparse64LeafMaterial(uint32_t slot) noexcept
{
	return static_cast<uint8_t>(slot & kSparse64MaterialMask);
}

constexpr uint8_t Sparse64HomogeneousMaterial(uint32_t slot) noexcept
{
	return static_cast<uint8_t>(slot & kSparse64MaterialMask);
}

constexpr uint32_t Sparse64NodeIndex(uint32_t slot) noexcept
{
	return slot & kSparse64NodeIndexMask;
}

constexpr int ComputeSparse64ChildSlotIndex(int subX, int subY, int subZ) noexcept
{
	return subX + kSparse64NodeSide * (subY + kSparse64NodeSide * subZ);
}

inline int ComputeSparse64Depth(int side) noexcept
{
	if (side <= 1) {
		return 0;
	}
	int depth = 1;
	int n = kSparse64NodeSide;
	while (n < side) {
		n *= kSparse64NodeSide;
		++depth;
	}
	return depth;
}

class Sparse64Tree {
public:
	struct Node {
		uint64_t fillMask = 0;
		std::array<uint32_t, kSparse64ChildrenPerNode> slots{};
		uint64_t structuralHash = 0;
		uint32_t refCount = 0;
	};

	Sparse64Tree() noexcept = default;

	explicit Sparse64Tree(int sideX, int sideY, int sideZ)
	{
		Reset(sideX, sideY, sideZ);
	}

	void Reset(int sideX, int sideY, int sideZ)
	{
		sideX_ = sideX > 0 ? sideX : 0;
		sideY_ = sideY > 0 ? sideY : 0;
		sideZ_ = sideZ > 0 ? sideZ : 0;
		depthX_ = ComputeSparse64Depth(sideX_);
		depthY_ = ComputeSparse64Depth(sideY_);
		depthZ_ = ComputeSparse64Depth(sideZ_);
		maxDepth_ = depthX_;
		if (depthY_ > maxDepth_) {
			maxDepth_ = depthY_;
		}
		if (depthZ_ > maxDepth_) {
			maxDepth_ = depthZ_;
		}
		nodes_.clear();
		dedupIndex_.clear();
		rootSlot_ = MakeSparse64Leaf(0);
	}

	[[nodiscard]] int SideX() const noexcept { return sideX_; }
	[[nodiscard]] int SideY() const noexcept { return sideY_; }
	[[nodiscard]] int SideZ() const noexcept { return sideZ_; }
	[[nodiscard]] int DepthX() const noexcept { return depthX_; }
	[[nodiscard]] int DepthY() const noexcept { return depthY_; }
	[[nodiscard]] int DepthZ() const noexcept { return depthZ_; }
	[[nodiscard]] int MaxDepth() const noexcept { return maxDepth_; }

	[[nodiscard]] bool IsDeduplicationEnabled() const noexcept
	{
		return deduplicationEnabled_;
	}

	void SetDeduplicationEnabled(const bool enabled) noexcept
	{
		if (enabled == deduplicationEnabled_) {
			return;
		}
		deduplicationEnabled_ = enabled;
		for (std::size_t i = 0; i < nodes_.size(); ++i) {
			nodes_[i].refCount = nodes_[i].refCount > 0 ? 1 : 0;
		}
		if (!enabled) {
			dedupIndex_.clear();
			for (std::size_t i = 0; i < nodes_.size(); ++i) {
				nodes_[i].structuralHash = 0;
			}
		} else {
			dedupIndex_.clear();
			for (std::size_t i = 0; i < nodes_.size(); ++i) {
				RebuildHashAndIndexForNode(static_cast<uint32_t>(i));
			}
		}
	}

	[[nodiscard]] uint8_t GetCell(int x, int y, int z) const noexcept
	{
		if (!Contains(x, y, z)) {
			return 0;
		}
		if (maxDepth_ == 0) {
			if (IsSparse64Homogeneous(rootSlot_)) {
				return Sparse64HomogeneousMaterial(rootSlot_);
			}
			return IsSparse64Leaf(rootSlot_) ? Sparse64LeafMaterial(rootSlot_) : 0;
		}
		uint32_t slot = rootSlot_;
		for (int level = maxDepth_; level > 0; --level) {
			if (IsSparse64Homogeneous(slot)) {
				return Sparse64HomogeneousMaterial(slot);
			}
			if (IsSparse64Leaf(slot)) {
				return Sparse64LeafMaterial(slot);
			}
			const Node &node = nodes_[Sparse64NodeIndex(slot)];
			const int subX = ExtractSubCoord(x, level);
			const int subY = ExtractSubCoord(y, level);
			const int subZ = ExtractSubCoord(z, level);
			const int childIndex = ComputeSparse64ChildSlotIndex(subX, subY, subZ);
			if ((node.fillMask >> childIndex & 1ull) == 0ull) {
				return 0;
			}
			slot = node.slots[childIndex];
		}
		if (IsSparse64Homogeneous(slot)) {
			return Sparse64HomogeneousMaterial(slot);
		}
		if (IsSparse64Leaf(slot)) {
			return Sparse64LeafMaterial(slot);
		}
		return 0;
	}

	void SetCell(int x, int y, int z, uint8_t material) noexcept
	{
		if (!Contains(x, y, z)) {
			return;
		}
		if (maxDepth_ == 0) {
			rootSlot_ = MakeSparse64Leaf(material);
			return;
		}
		rootSlot_ = SetCellRecursive(rootSlot_, x, y, z, material, maxDepth_);
	}

	[[nodiscard]] bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < sideX_ && y >= 0 && y < sideY_ && z >= 0 && z < sideZ_;
	}

	[[nodiscard]] size_t NodeCount() const noexcept
	{
		return nodes_.size();
	}

	[[nodiscard]] size_t RawNodeCount() const noexcept
	{
		return nodes_.size();
	}

	[[nodiscard]] const Node &NodeAt(uint32_t nodeIndex) const noexcept
	{
		return nodes_[nodeIndex];
	}

	[[nodiscard]] size_t LiveNodeCount() const noexcept
	{
		size_t live = 0;
		for (const Node &node : nodes_) {
			if (node.refCount > 0) {
				++live;
			}
		}
		return live;
	}

	[[nodiscard]] const Node *GetNodes() const noexcept
	{
		return nodes_.data();
	}

	[[nodiscard]] uint32_t RootSlot() const noexcept
	{
		return rootSlot_;
	}

	void RestoreFrom(uint32_t rootSlot, std::vector<Node> nodes) noexcept
	{
		rootSlot_ = rootSlot;
		nodes_ = std::move(nodes);
		dedupIndex_.clear();
		for (std::size_t i = 0; i < nodes_.size(); ++i) {
			nodes_[i].refCount = 0;
		}
		if (deduplicationEnabled_) {
			for (std::size_t i = 0; i < nodes_.size(); ++i) {
				RebuildHashAndIndexForNode(static_cast<uint32_t>(i));
			}
		}
	}

	[[nodiscard]] size_t NonAirCount() const noexcept
	{
		return CountNonAirRecursive(rootSlot_, maxDepth_);
	}

	[[nodiscard]] bool IsEmpty() const noexcept
	{
		return NonAirCount() == 0;
	}

	void Clear() noexcept
	{
		nodes_.clear();
		dedupIndex_.clear();
		rootSlot_ = MakeSparse64Leaf(0);
	}

private:
	int sideX_ = 0;
	int sideY_ = 0;
	int sideZ_ = 0;
	int depthX_ = 0;
	int depthY_ = 0;
	int depthZ_ = 0;
	int maxDepth_ = 0;
	std::vector<Node> nodes_;
	std::unordered_multimap<uint64_t, uint32_t> dedupIndex_;
	bool deduplicationEnabled_ = false;
	uint32_t rootSlot_ = MakeSparse64Leaf(0);

	static uint64_t MixSplitMix64(uint64_t seed, uint64_t value) noexcept
	{
		uint64_t z = seed + value;
		z = (z ^ z >> 30) * 0xbf58476d1ce4e5b9ull;
		z = (z ^ z >> 27) * 0x94d049bb133111ebull;
		return z ^ z >> 31;
	}

	[[nodiscard]] uint64_t ComputeNodeStructuralHash(const Node &node) const noexcept
	{
		uint64_t h = MixSplitMix64(0x9E3779B97F4A7C15ull, node.fillMask);
		for (std::size_t i = 0; i < node.slots.size(); ++i) {
			h = MixSplitMix64(h, node.slots[i]);
		}
		return h;
	}

	void RebuildHashAndIndexForNode(uint32_t nodeIndex) noexcept // NOLINT(bugprone-exception-escape): hash index growth is bounded
	{
		Node &node = nodes_[nodeIndex];
		node.structuralHash = ComputeNodeStructuralHash(node);
		dedupIndex_.emplace(node.structuralHash, nodeIndex);
	}

	[[nodiscard]] bool NodesStructurallyEqual(const Node &a, const Node &b) const noexcept
	{
		if (a.fillMask != b.fillMask) {
			return false;
		}
		if (a.structuralHash != b.structuralHash) {
			return false;
		}
		for (std::size_t i = 0; i < a.slots.size(); ++i) {
			if (a.slots[i] != b.slots[i]) {
				return false;
			}
		}
		return true;
	}

	uint32_t FindEquivalentNode(const Node &candidate) noexcept
	{
		if (!deduplicationEnabled_) {
			return kSparse64InvalidNodeIndex;
		}
		const uint64_t hash = ComputeNodeStructuralHash(candidate);
		const auto [begin, end] = dedupIndex_.equal_range(hash);
		for (auto it = begin; it != end; ++it) {
			if (NodesStructurallyEqual(nodes_[it->second], candidate)) {
				return it->second;
			}
		}
		return kSparse64InvalidNodeIndex;
	}

	void AddNodeToDedupIndex(uint32_t nodeIndex) noexcept
	{
		if (!deduplicationEnabled_) {
			return;
		}
		RebuildHashAndIndexForNode(nodeIndex);
	}

	void RemoveNodeFromDedupIndex(uint32_t nodeIndex) noexcept
	{
		if (!deduplicationEnabled_) {
			return;
		}
		const uint64_t hash = nodes_[nodeIndex].structuralHash;
		const auto [begin, end] = dedupIndex_.equal_range(hash);
		for (auto it = begin; it != end; ++it) {
			if (it->second == nodeIndex) {
				dedupIndex_.erase(it);
				return;
			}
		}
	}

	void UpdateNodeInDedupIndex(uint32_t nodeIndex) noexcept
	{
		if (!deduplicationEnabled_) {
			return;
		}
		RemoveNodeFromDedupIndex(nodeIndex);
		AddNodeToDedupIndex(nodeIndex);
	}

	[[nodiscard]] int ExtractSubCoord(int coord, int level) const noexcept
	{
		if (level <= 0 || level > maxDepth_) {
			return 0;
		}
		const int shift = (level - 1) * kSparse64BitsPerAxis;
		return coord >> shift & kSparse64NodeSide - 1;
	}

	[[nodiscard]] size_t CountNonAirRecursive(uint32_t slot, int level) const noexcept // NOLINT(misc-no-recursion): recursive tree traversal is intentional
	{
		if (IsSparse64Homogeneous(slot)) {
			if (Sparse64HomogeneousMaterial(slot) == 0) {
				return 0;
			}
			size_t cells = 1;
			for (int i = 0; i < level; ++i) {
				cells *= kSparse64ChildrenPerNode;
			}
			return cells;
		}
		if (IsSparse64Leaf(slot)) {
			return Sparse64LeafMaterial(slot) != 0 ? 1 : 0;
		}
		if (level <= 0) {
			return 0;
		}
		const Node &node = nodes_[Sparse64NodeIndex(slot)];
		if (node.fillMask == 0) {
			return 0;
		}
		size_t count = 0;
		for (int i = 0; i < kSparse64ChildrenPerNode; ++i) {
			if ((node.fillMask >> i & 1ull) != 0ull) {
				count += CountNonAirRecursive(node.slots[i], level - 1);
			}
		}
		return count;
	}

	bool CanCollapseToHomogeneous(const Node &node, uint8_t &outMaterial) const noexcept
	{
		if (node.fillMask != 0xFFFFFFFFFFFFFFFFull) {
			return false;
		}
		uint8_t material = 0;
		for (int i = 0; i < kSparse64ChildrenPerNode; ++i) {
			const uint32_t slot = node.slots[i];
			if (IsSparse64Homogeneous(slot)) {
				const uint8_t slotMaterial = Sparse64HomogeneousMaterial(slot);
				if (i > 0 && slotMaterial != material) {
					return false;
				}
				material = slotMaterial;
			} else if (IsSparse64Leaf(slot)) {
				const uint8_t slotMaterial = Sparse64LeafMaterial(slot);
				if (i > 0 && slotMaterial != material) {
					return false;
				}
				material = slotMaterial;
			} else {
				return false;
			}
		}
		outMaterial = material;
		return true;
	}

	uint32_t AllocateNode(uint32_t fillMaterial) noexcept // NOLINT(bugprone-exception-escape): node vector growth is bounded
	{
		Node newNode{};
		if (IsSparse64Leaf(fillMaterial)) {
			const uint8_t material = Sparse64LeafMaterial(fillMaterial);
			for (int i = 0; i < kSparse64ChildrenPerNode; ++i) {
				newNode.slots[i] = MakeSparse64Leaf(material);
			}
		} else {
			for (int i = 0; i < kSparse64ChildrenPerNode; ++i) {
				newNode.slots[i] = MakeSparse64Leaf(0);
			}
		}
		newNode.refCount = 1;
		if (deduplicationEnabled_) {
			newNode.structuralHash = ComputeNodeStructuralHash(newNode);
			const uint32_t existing = FindEquivalentNode(newNode);
			if (existing != kSparse64InvalidNodeIndex) {
				++nodes_[existing].refCount;
				return existing;
			}
		}
		const uint32_t index = static_cast<uint32_t>(nodes_.size());
		nodes_.push_back(newNode);
		if (deduplicationEnabled_) {
			AddNodeToDedupIndex(index);
		}
		return index;
	}

	uint32_t MarkNodeUnique(uint32_t nodeIndex) noexcept // NOLINT(bugprone-exception-escape): node vector growth is bounded
	{
		if (!deduplicationEnabled_ || nodes_[nodeIndex].refCount == 1) {
			return nodeIndex;
		}
		--nodes_[nodeIndex].refCount;
		RemoveNodeFromDedupIndex(nodeIndex);
		Node copy = nodes_[nodeIndex];
		copy.refCount = 1;
		const uint32_t newIndex = static_cast<uint32_t>(nodes_.size());
		nodes_.push_back(copy);
		AddNodeToDedupIndex(newIndex);
		return newIndex;
	}

	void DecrementRefCount(uint32_t nodeIndex) noexcept
	{
		if (nodes_[nodeIndex].refCount > 0) {
			--nodes_[nodeIndex].refCount;
		}
	}

	[[nodiscard]] uint32_t GetRefCount(uint32_t nodeIndex) const noexcept
	{
		return nodes_[nodeIndex].refCount;
	}

	[[nodiscard]] bool IsHomogeneousRoot() const noexcept
	{
		return IsSparse64Homogeneous(rootSlot_);
	}

public:
	void DedupPass() noexcept
	{
		if (!deduplicationEnabled_ || IsSparse64Leaf(rootSlot_)) {
			return;
		}
		rootSlot_ = DedupSubtree(rootSlot_, maxDepth_);
	}

	uint32_t DedupSubtree(uint32_t slot, int level) noexcept // NOLINT(misc-no-recursion): bounded-depth recursive tree traversal
	{
		// Tree depth bounded by chunkSize (default 8 = depth 2; max chunkSize 32 = depth 3).
		// Max recursion: log2(32) + 1 = 6 levels. Safe from stack overflow.
		if (level <= 0 || IsSparse64Leaf(slot)) {
			return slot;
		}
		const uint32_t nodeIndex = Sparse64NodeIndex(slot);
		Node &node = nodes_[nodeIndex];
		for (std::size_t i = 0; i < node.slots.size(); ++i) {
			node.slots[i] = DedupSubtree(node.slots[i], level - 1);
		}
		UpdateNodeInDedupIndex(nodeIndex);
		return slot;
	}

	uint32_t SetCellRecursive(uint32_t slot, int x, int y, int z, uint8_t material, int level) noexcept // NOLINT(misc-no-recursion): bounded-depth recursive tree traversal
	{
		if (level <= 0) {
			return MakeSparse64Leaf(material);
		}
		if (IsSparse64Homogeneous(slot)) {
			const uint8_t existingMaterial = Sparse64HomogeneousMaterial(slot);
			if (existingMaterial == material) {
				return slot;
			}
			const uint32_t newNodeIndex = AllocateNode(MakeSparse64Leaf(existingMaterial));
			nodes_[newNodeIndex].fillMask = 0xFFFFFFFFFFFFFFFFull;
			slot = newNodeIndex;
		} else if (IsSparse64Leaf(slot)) {
			if (Sparse64LeafMaterial(slot) == material) {
				return slot;
			}
			const uint32_t nodeIndex = AllocateNode(slot);
			nodes_[nodeIndex].fillMask = 0xFFFFFFFFFFFFFFFFull;
			slot = nodeIndex;
		}

		const int subX = ExtractSubCoord(x, level);
		const int subY = ExtractSubCoord(y, level);
		const int subZ = ExtractSubCoord(z, level);
		const int childIndex = ComputeSparse64ChildSlotIndex(subX, subY, subZ);

		const uint32_t nodeIndex = MarkNodeUnique(Sparse64NodeIndex(slot));
		const uint32_t existingSlot = nodes_[nodeIndex].slots[childIndex];
		const uint32_t updatedSlot = SetCellRecursive(existingSlot, x, y, z, material, level - 1);
		nodes_[nodeIndex].slots[childIndex] = updatedSlot;
		nodes_[nodeIndex].fillMask |= 1ull << childIndex;

		uint8_t collapseMaterial = 0;
		if (CanCollapseToHomogeneous(nodes_[nodeIndex], collapseMaterial)) {
			if (deduplicationEnabled_) {
				RemoveNodeFromDedupIndex(nodeIndex);
			}
			DecrementRefCount(nodeIndex);
			return MakeSparse64Homogeneous(collapseMaterial);
		}

		UpdateNodeInDedupIndex(nodeIndex);
		return nodeIndex;
	}
};

} // namespace projectv::voxel
