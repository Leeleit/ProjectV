#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace projectv::voxel {

inline constexpr int kSparse64BitsPerAxis = 2;
inline constexpr int kSparse64NodeSide = 1 << kSparse64BitsPerAxis;
inline constexpr int kSparse64ChildrenPerNode = 1 << (3 * kSparse64BitsPerAxis);

inline constexpr uint32_t kSparse64LeafFlag = 0x80000000u;
inline constexpr uint32_t kSparse64NodeIndexMask = 0x7FFFFFFFu;
inline constexpr uint32_t kSparse64MaterialMask = 0xFFu;

inline constexpr uint32_t MakeSparse64Leaf(uint8_t material) noexcept
{
	return kSparse64LeafFlag | static_cast<uint32_t>(material);
}

inline constexpr bool IsSparse64Leaf(uint32_t slot) noexcept
{
	return (slot & kSparse64LeafFlag) != 0u;
}

inline constexpr uint8_t Sparse64LeafMaterial(uint32_t slot) noexcept
{
	return static_cast<uint8_t>(slot & kSparse64MaterialMask);
}

inline constexpr uint32_t Sparse64NodeIndex(uint32_t slot) noexcept
{
	return slot & kSparse64NodeIndexMask;
}

inline constexpr int ComputeSparse64ChildSlotIndex(int subX, int subY, int subZ) noexcept
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
		rootSlot_ = MakeSparse64Leaf(0);
	}

	int SideX() const noexcept { return sideX_; }
	int SideY() const noexcept { return sideY_; }
	int SideZ() const noexcept { return sideZ_; }
	int DepthX() const noexcept { return depthX_; }
	int DepthY() const noexcept { return depthY_; }
	int DepthZ() const noexcept { return depthZ_; }
	int MaxDepth() const noexcept { return maxDepth_; }

	uint8_t GetCell(int x, int y, int z) const noexcept
	{
		if (!Contains(x, y, z)) {
			return 0;
		}
		if (maxDepth_ == 0) {
			return IsSparse64Leaf(rootSlot_) ? Sparse64LeafMaterial(rootSlot_) : 0;
		}
		uint32_t slot = rootSlot_;
		for (int level = maxDepth_; level > 0; --level) {
			if (IsSparse64Leaf(slot)) {
				return Sparse64LeafMaterial(slot);
			}
			const Node &node = nodes_[Sparse64NodeIndex(slot)];
			const int subX = ExtractSubCoord(x, level);
			const int subY = ExtractSubCoord(y, level);
			const int subZ = ExtractSubCoord(z, level);
			const int childIndex = ComputeSparse64ChildSlotIndex(subX, subY, subZ);
			if (((node.fillMask >> childIndex) & 1ull) == 0ull) {
				return 0;
			}
			slot = node.slots[childIndex];
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

	bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < sideX_ && y >= 0 && y < sideY_ && z >= 0 && z < sideZ_;
	}

	size_t NodeCount() const noexcept
	{
		return nodes_.size();
	}

	size_t NonAirCount() const noexcept
	{
		return CountNonAirRecursive(rootSlot_, maxDepth_);
	}

	bool IsEmpty() const noexcept
	{
		return NonAirCount() == 0;
	}

	void Clear() noexcept
	{
		nodes_.clear();
		rootSlot_ = MakeSparse64Leaf(0);
	}

private:
	struct Node {
		uint64_t fillMask = 0;
		std::array<uint32_t, kSparse64ChildrenPerNode> slots{};
	};

	int sideX_ = 0;
	int sideY_ = 0;
	int sideZ_ = 0;
	int depthX_ = 0;
	int depthY_ = 0;
	int depthZ_ = 0;
	int maxDepth_ = 0;
	std::vector<Node> nodes_;
	uint32_t rootSlot_ = MakeSparse64Leaf(0);

	int ExtractSubCoord(int coord, int level) const noexcept
	{
		if (level <= 0 || level > maxDepth_) {
			return 0;
		}
		int shift = (level - 1) * kSparse64BitsPerAxis;
		return (coord >> shift) & (kSparse64NodeSide - 1);
	}

	size_t CountNonAirRecursive(uint32_t slot, int level) const noexcept
	{
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
			if (((node.fillMask >> i) & 1ull) != 0ull) {
				count += CountNonAirRecursive(node.slots[i], level - 1);
			}
		}
		return count;
	}

	uint32_t AllocateNode(uint32_t fillMaterial) noexcept
	{
		const uint32_t index = static_cast<uint32_t>(nodes_.size());
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
		nodes_.push_back(newNode);
		return index;
	}

	uint32_t SetCellRecursive(uint32_t slot, int x, int y, int z, uint8_t material, int level) noexcept
	{
		if (level <= 0) {
			return MakeSparse64Leaf(material);
		}
		const int subX = ExtractSubCoord(x, level);
		const int subY = ExtractSubCoord(y, level);
		const int subZ = ExtractSubCoord(z, level);
		const int childIndex = ComputeSparse64ChildSlotIndex(subX, subY, subZ);

		if (IsSparse64Leaf(slot)) {
			const uint32_t nodeIndex = AllocateNode(slot);
			slot = nodeIndex;
		}

		const uint32_t nodeIndex = Sparse64NodeIndex(slot);
		const uint32_t existingSlot = nodes_[nodeIndex].slots[childIndex];
		const uint32_t updatedSlot = SetCellRecursive(existingSlot, x, y, z, material, level - 1);
		nodes_[nodeIndex].slots[childIndex] = updatedSlot;
		nodes_[nodeIndex].fillMask |= (1ull << childIndex);
		return slot;
	}
};

} // namespace projectv::voxel
