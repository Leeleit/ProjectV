#include "voxel/NanoVdb.hpp"

#include <cassert>

namespace projectv::voxel::nanovdb {

namespace {

constexpr uint32_t GetBitsPerAxis() noexcept
{
	return static_cast<uint32_t>(kSparse64BitsPerAxis);
}

constexpr uint32_t GetChildrenPerNode() noexcept
{
	return static_cast<uint32_t>(kSparse64ChildrenPerNode);
}

bool DecodeSlot(
	const Sparse64Tree &tree,
	const uint32_t slot,
	const uint8_t *materialLookup,
	bool &outIsLeaf,
	uint8_t &outMaterial,
	uint32_t &outNodeIndex)
{
	const uint32_t flagBits = slot & (kSparse64LeafFlag | kSparse64HomogeneousFlag);
	if ((flagBits & kSparse64LeafFlag) != 0u) {
		outIsLeaf = true;
		const uint8_t materialIndex = static_cast<uint8_t>(slot & kSparse64MaterialMask);
		outMaterial = (materialLookup != nullptr) ? materialLookup[materialIndex] : materialIndex;
		outNodeIndex = kNanoVdbInvalidIndex;
		return true;
	}
	outIsLeaf = false;
	outMaterial = 0u;
	outNodeIndex = slot & kSparse64NodeIndexMask;
	if (outNodeIndex >= tree.RawNodeCount()) {
		return false;
	}
	return true;
}

void EmitEmptyLower(NanoVdbLower &lower)
{
	lower.childMask = 0u;
	lower.valueMask32 = 0u;
	lower.firstLeaf = kNanoVdbInvalidIndex;
}

}  // namespace

bool BuildNanoVdbFlatten(
	const Sparse64Tree &tree,
	const uint8_t *materialLookup,
	NanoVdbFlattenResult &outResult)
{
	outResult = {};
	if (tree.SideX() == 0 || tree.SideY() == 0 || tree.SideZ() == 0) {
		return false;
	}

	const uint32_t childrenPerNode = GetChildrenPerNode();
	const uint64_t allChildrenMask = (childrenPerNode == 64u)
		? ~uint64_t{0}
		: ((uint64_t{1} << childrenPerNode) - 1u);

	outResult.uppers.reserve(1u);
	outResult.lowers.resize(childrenPerNode);
	outResult.leaves.reserve(childrenPerNode);
	outResult.materials.reserve(64u);

	for (uint32_t i = 0; i < childrenPerNode; ++i) {
		EmitEmptyLower(outResult.lowers[i]);
	}

	NanoVdbUpper upper{};
	upper.childMask = 0u;
	upper.firstLower = 0u;

	const uint32_t rootSlot = tree.RootSlot();
	bool rootIsLeaf = false;
	uint8_t rootMaterial = 0u;
	uint32_t rootNodeIndex = kNanoVdbInvalidIndex;
	if (!DecodeSlot(tree, rootSlot, materialLookup, rootIsLeaf, rootMaterial, rootNodeIndex)) {
		return false;
	}

	if (rootIsLeaf) {
		if (rootMaterial != 0u) {
			const uint32_t totalVoxels = static_cast<uint32_t>(tree.SideX()) *
				static_cast<uint32_t>(tree.SideY()) *
				static_cast<uint32_t>(tree.SideZ());
			const uint64_t valueMask = (totalVoxels >= 64u)
				? ~uint64_t{0}
				: ((uint64_t{1} << totalVoxels) - 1u);

			NanoVdbLower lower{};
			lower.childMask = valueMask;
			lower.valueMask32 = static_cast<uint32_t>(valueMask & 0xFFFFFFFFu);
			lower.firstLeaf = 0u;

			NanoVdbLeaf leaf{};
			leaf.valueMask = valueMask;
			leaf.firstMaterial = 0u;
			leaf.materialCount = 1u;
			leaf.homogeneousMaterial = rootMaterial;
			leaf.homogeneousFlag = 1u;

			outResult.materials.push_back(rootMaterial);
			outResult.lowers[0] = lower;
			outResult.leaves.push_back(leaf);
			upper.firstLower = 0u;
			upper.childMask = 1u;
		}
	} else {
		const Sparse64Tree::Node &rootNode = tree.NodeAt(rootNodeIndex);
		const uint64_t rootChildMask = rootNode.fillMask & allChildrenMask;

		uint32_t leafBase = 0u;

		for (uint32_t childSlotIndex = 0; childSlotIndex < childrenPerNode; ++childSlotIndex) {
			if (((rootChildMask >> childSlotIndex) & 1ull) == 0ull) {
				continue;
			}
			const uint32_t childSlot = rootNode.slots[childSlotIndex];
			bool childIsLeaf = false;
			uint8_t childMaterial = 0u;
			uint32_t childNodeIndex = kNanoVdbInvalidIndex;
			if (!DecodeSlot(tree, childSlot, materialLookup, childIsLeaf, childMaterial, childNodeIndex)) {
				return false;
			}

			if (childIsLeaf) {
				if (childMaterial != 0u) {
					const uint64_t valueMask = (childrenPerNode == 64u)
						? ~uint64_t{0}
						: ((uint64_t{1} << childrenPerNode) - 1u);

					NanoVdbLower lower{};
					lower.childMask = valueMask;
					lower.valueMask32 = static_cast<uint32_t>(valueMask & 0xFFFFFFFFu);
					lower.firstLeaf = leafBase;

					NanoVdbLeaf leaf{};
					leaf.valueMask = valueMask;
					leaf.firstMaterial = static_cast<uint32_t>(outResult.materials.size());
					leaf.materialCount = 1u;
					leaf.homogeneousMaterial = childMaterial;
					leaf.homogeneousFlag = 1u;

					outResult.materials.push_back(childMaterial);
					outResult.lowers[childSlotIndex] = lower;
					outResult.leaves.push_back(leaf);
					++leafBase;
					upper.childMask |= (uint32_t{1} << childSlotIndex);
				}
			} else {
				const Sparse64Tree::Node &childNode = tree.NodeAt(childNodeIndex);
				const uint64_t childChildMask = childNode.fillMask & allChildrenMask;

				uint64_t effectiveChildMask = 0u;
				uint64_t valueMaskAggregate = 0u;
				uint32_t childCountLocal = 0u;
				for (uint32_t leafChildSlotIndex = 0;
					leafChildSlotIndex < childrenPerNode;
					++leafChildSlotIndex) {
					if (((childChildMask >> leafChildSlotIndex) & 1ull) == 0ull) {
						continue;
					}
					const uint32_t leafChildSlot = childNode.slots[leafChildSlotIndex];
					bool leafChildIsLeaf = false;
					uint8_t leafChildMaterial = 0u;
					uint32_t leafChildNodeIndex = kNanoVdbInvalidIndex;
					if (!DecodeSlot(
							tree,
							leafChildSlot,
							materialLookup,
							leafChildIsLeaf,
							leafChildMaterial,
							leafChildNodeIndex)) {
						return false;
					}
					if (!leafChildIsLeaf) {
						return false;
					}
					if (leafChildMaterial == 0u) {
						continue;
					}
					const uint64_t leafBit = uint64_t{1} << leafChildSlotIndex;
					valueMaskAggregate |= leafBit;
					effectiveChildMask |= leafBit;

					NanoVdbLeaf leaf{};
					leaf.valueMask = leafBit;
					leaf.firstMaterial = static_cast<uint32_t>(outResult.materials.size());
					leaf.materialCount = 1u;
					leaf.homogeneousMaterial = leafChildMaterial;
					leaf.homogeneousFlag = 1u;

					outResult.materials.push_back(leafChildMaterial);
					outResult.leaves.push_back(leaf);
					++childCountLocal;
				}
				if (childCountLocal > 0u) {
					NanoVdbLower lower{};
					lower.childMask = effectiveChildMask;
					lower.valueMask32 = static_cast<uint32_t>(valueMaskAggregate & 0xFFFFFFFFu);
					lower.firstLeaf = leafBase;

					outResult.lowers[childSlotIndex] = lower;
					leafBase += childCountLocal;
					upper.childMask |= (uint32_t{1} << childSlotIndex);
				}
			}
		}

		upper.firstLower = 0u;
		outResult.uppers.push_back(upper);
	}

	outResult.upperCount = static_cast<uint32_t>(outResult.uppers.size());
	outResult.lowerCount = static_cast<uint32_t>(outResult.lowers.size());
	outResult.leafCount = static_cast<uint32_t>(outResult.leaves.size());
	outResult.materialCount = static_cast<uint32_t>(outResult.materials.size());
	outResult.rootUpperIndex = 0u;
	outResult.rootLevelDepth = tree.MaxDepth();
	return true;
}

uint8_t ReadNanoVdbVoxelMaterial(
	const NanoVdbFlattenResult &result,
	const uint32_t rootUpperIndex,
	const uint32_t localX,
	const uint32_t localY,
	const uint32_t localZ)
{
	if (rootUpperIndex >= result.uppers.size()) {
		return 0u;
	}
	const NanoVdbUpper &upper = result.uppers[rootUpperIndex];
	if (upper.firstLower == kNanoVdbInvalidIndex) {
		return 0u;
	}
	const uint32_t childrenPerNode = GetChildrenPerNode();
	const uint32_t bitsPerAxis = GetBitsPerAxis();
	const uint32_t mask = (1u << bitsPerAxis) - 1u;
	const uint32_t rootDepth = (result.rootLevelDepth >= 2)
		? static_cast<uint32_t>(result.rootLevelDepth - 1)
		: 0u;
	const uint32_t upperShift = bitsPerAxis * rootDepth;
	const uint32_t upperChildX = (localX >> upperShift) & mask;
	const uint32_t upperChildY = (localY >> upperShift) & mask;
	const uint32_t upperChildZ = (localZ >> upperShift) & mask;
	const uint32_t upperChildIndex = upperChildX +
		(mask + 1u) * (upperChildY + (mask + 1u) * upperChildZ);
	if (upperChildIndex >= childrenPerNode) {
		return 0u;
	}
	if (((upper.childMask >> upperChildIndex) & 1u) == 0u) {
		return 0u;
	}
	const uint32_t lowerIndex = upper.firstLower + upperChildIndex;
	if (lowerIndex >= result.lowers.size()) {
		return 0u;
	}
	const NanoVdbLower &lower = result.lowers[lowerIndex];
	if (lower.firstLeaf == kNanoVdbInvalidIndex) {
		return 0u;
	}
	const uint32_t lowerChildX = localX & mask;
	const uint32_t lowerChildY = localY & mask;
	const uint32_t lowerChildZ = localZ & mask;
	const uint32_t lowerChildIndex = lowerChildX +
		(mask + 1u) * (lowerChildY + (mask + 1u) * lowerChildZ);
	if (lowerChildIndex >= childrenPerNode) {
		return 0u;
	}
	if (((lower.childMask >> lowerChildIndex) & 1ull) == 0ull) {
		return 0u;
	}
	const uint64_t lowerBitsBelow = lower.childMask & ((uint64_t{1} << lowerChildIndex) - 1ull);
	const uint32_t leafOffset = static_cast<uint32_t>(__builtin_popcountll(lowerBitsBelow));
	const uint32_t leafIndex = lower.firstLeaf + leafOffset;
	if (leafIndex >= result.leaves.size()) {
		return 0u;
	}
	const NanoVdbLeaf &leaf = result.leaves[leafIndex];
	if (leaf.homogeneousFlag != 0u) {
		return leaf.homogeneousMaterial;
	}
	const uint32_t materialIndex = leaf.firstMaterial + leafOffset;
	if (materialIndex >= result.materials.size()) {
		return 0u;
	}
	return result.materials[materialIndex];
}

}  // namespace projectv::voxel::nanovdb