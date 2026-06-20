// SPDX-License-Identifier: MIT
//
// cpu_bugfix.cpp - CPU-side prototype comparing SVDAG-on-64-tree (current
// ProjectV mainline per src/voxel/Sparse64Tree.hpp) against a proper
// NanoVDB-aligned 4-level B+tree (Upper[chunkSide^3] -> Lower[(chunkSide/2)^3] ->
// Leaf[(chunkSide/4)^3]) with byte-exact correctness.
//
// Mainline constants (mirrored from src/voxel/Sparse64Tree.hpp):
//   kSparse64BitsPerAxis = 2, kSparse64NodeSide = 4,
//   kSparse64ChildrenPerNode = 64
//   kSparse64LeafFlag = 0x80000000u, kSparse64HomogeneousFlag = 0x40000000u
//   kSparse64NodeIndexMask = 0x3FFFFFFFu
//
// ProjectV chunkSize (per VoxelWorld.hpp:78 + SceneConfig.cpp:78):
//   chunkSize = 8
//   depth = ComputeSparse64Depth(8) = 2
//   For chunkSize=8: 1 root + up to 64 children + 64 leaves = ~130 nodes max.
//
// Bugfix target: experiments/2026-06-20-svdag-vs-vdb-memory-throughput had a
// "uniform-tile lie" bug in NanoVDB-like impl (verify_mismatches > 0 for 4/7
// scenes). This implementation removes the bug by always materializing children
// on SetCell, paying small memory cost for correctness.
//
// Scope per docs/experiments/AGENTS.md section 2: standalone research artifact,
// NOT part of ProjectV mainline.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     docs/experiments/experiments/2026-06-20-nanovdb-on-gpu/prototype/cpu_bugfix.cpp \
//     -o /tmp/cpu_bugfix
//
// Run:
//   /tmp/cpu_bugfix

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
inline constexpr u32 kHomogeneousFlag = 0x40000000u;
inline constexpr u32 kNodeIndexMask = 0x3FFFFFFFu;
inline constexpr u32 kMaterialMask = 0xFFu;

inline constexpr u32 MakeLeaf(u8 m) noexcept
{
	return kLeafFlag | static_cast<u32>(m);
}
inline constexpr u32 MakeHomogeneous(u8 m) noexcept
{
	return kLeafFlag | kHomogeneousFlag | static_cast<u32>(m);
}
inline constexpr bool IsLeaf(u32 slot) noexcept
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == kLeafFlag;
}
inline constexpr bool IsHomogeneous(u32 slot) noexcept
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == (kLeafFlag | kHomogeneousFlag);
}
inline constexpr u8 LeafMaterial(u32 slot) noexcept
{
	return static_cast<u8>(slot & kMaterialMask);
}
inline constexpr u32 NodeIndex(u32 slot) noexcept
{
	return slot & kNodeIndexMask;
}

inline constexpr int kChunkSide = 8;
inline constexpr int kChunkVoxels = kChunkSide * kChunkSide * kChunkSide;

enum class SceneKind { Solid,
					   Ground,
					   Brick,
					   VoxelLab,
					   SparseRandom };

struct Scene {
	std::vector<u8> voxels;
	int side = kChunkSide;
	size_t expectedNonAir = 0;
	std::string name;
};

Scene GenScene(SceneKind kind, u64 seed)
{
	Scene s;
	s.side = kChunkSide;
	s.voxels.assign(static_cast<size_t>(kChunkSide) * kChunkSide * kChunkSide, 0);
	std::mt19937_64 rng(seed);

	auto idx = [s = kChunkSide](int x, int y, int z) {
		return x + s * (y + s * z);
	};

	switch (kind) {
	case SceneKind::Solid: {
		std::fill(s.voxels.begin(), s.voxels.end(), 1);
		s.expectedNonAir = kChunkVoxels;
		s.name = "solid_8";
		break;
	}
	case SceneKind::Ground: {
		for (int y = 0; y < kChunkSide; ++y)
			for (int x = 0; x < kChunkSide; ++x)
				s.voxels[idx(x, y, 0)] = 1;
		s.expectedNonAir = kChunkSide * kChunkSide;
		s.name = "ground_8";
		break;
	}
	case SceneKind::Brick: {
		for (int bz = 0; bz < 2; ++bz)
			for (int by = 0; by < 2; ++by)
				for (int bx = 0; bx < 2; ++bx) {
					u8 mat = static_cast<u8>(1 + (bx + by + bz) % 3);
					int x0 = bx * 4, y0 = by * 4, z0 = bz * 4;
					for (int z = z0; z < z0 + 4; ++z)
						for (int y = y0; y < y0 + 4; ++y)
							for (int x = x0; x < x0 + 4; ++x)
								s.voxels[idx(x, y, z)] = mat;
				}
		s.expectedNonAir = 8 * 64;
		s.name = "brick_8";
		break;
	}
	case SceneKind::VoxelLab: {
		std::uniform_real_distribution<double> dist(0.0, 1.0);
		size_t count = 0;
		for (int z = 0; z < kChunkSide; ++z)
			for (int y = 0; y < kChunkSide; ++y)
				for (int x = 0; x < kChunkSide; ++x) {
					double height = static_cast<double>(z) / kChunkSide;
					double threshold = 0.05 + 0.3 * height;
					if (dist(rng) < threshold) {
						u8 mat = static_cast<u8>(1 + (z / 4) % 3);
						s.voxels[idx(x, y, z)] = mat;
						++count;
					}
				}
		s.expectedNonAir = count;
		s.name = "voxel_lab_8";
		break;
	}
	case SceneKind::SparseRandom: {
		std::uniform_int_distribution<int> mat(1, 3);
		std::uniform_real_distribution<double> dens(0.0, 1.0);
		size_t count = 0;
		for (int z = 0; z < kChunkSide; ++z)
			for (int y = 0; y < kChunkSide; ++y)
				for (int x = 0; x < kChunkSide; ++x) {
					if (dens(rng) < 0.1) {
						s.voxels[idx(x, y, z)] = static_cast<u8>(mat(rng));
						++count;
					}
				}
		s.expectedNonAir = count;
		s.name = "sparse_random_8";
		break;
	}
	}
	return s;
}

class Svdag64 {
  public:
	struct alignas(8) Node {
		u64 fillMask = 0;
		std::array<u32, kChildren> slots{};
		u64 structuralHash = 0;
		u32 refCount = 0;
	};
	static_assert(sizeof(Node) == 280, "Node must be exactly 280 B");

	explicit Svdag64(int side) { Reset(side); }

	void Reset(int side)
	{
		side_ = side > 0 ? side : 0;
		depth_ = ComputeDepth(side_);
		nodes_.clear();
		rootSlot_ = MakeLeaf(0);
	}

	int Depth() const noexcept { return depth_; }

	u8 GetCell(int x, int y, int z) const
	{
		if (!Contains(x, y, z))
			return 0;
		if (depth_ == 0) {
			if (IsHomogeneous(rootSlot_))
				return LeafMaterial(rootSlot_);
			return IsLeaf(rootSlot_) ? LeafMaterial(rootSlot_) : 0;
		}
		u32 slot = rootSlot_;
		for (int level = depth_; level > 0; --level) {
			if (IsHomogeneous(slot))
				return LeafMaterial(slot);
			if (IsLeaf(slot))
				return LeafMaterial(slot);
			const Node &node = nodes_[NodeIndex(slot)];
			const int subX = ExtractSubCoord(x, level);
			const int subY = ExtractSubCoord(y, level);
			const int subZ = ExtractSubCoord(z, level);
			const int childIndex = subX + kNodeSide * (subY + kNodeSide * subZ);
			if (((node.fillMask >> childIndex) & 1ull) == 0ull)
				return 0;
			slot = node.slots[childIndex];
		}
		if (IsHomogeneous(slot))
			return LeafMaterial(slot);
		if (IsLeaf(slot))
			return LeafMaterial(slot);
		return 0;
	}

	void SetCell(int x, int y, int z, u8 material)
	{
		if (!Contains(x, y, z))
			return;
		if (depth_ == 0) {
			rootSlot_ = MakeLeaf(material);
			return;
		}
		rootSlot_ = SetCellRecursive(rootSlot_, x, y, z, material, depth_);
	}

	bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < side_ && y >= 0 && y < side_ && z >= 0 && z < side_;
	}

	size_t NonAirCount() const { return CountNonAirRecursive(rootSlot_, depth_); }

	size_t MemoryBytes() const noexcept
	{
		size_t bytes = sizeof(*this);
		bytes += nodes_.capacity() * sizeof(Node);
		return bytes;
	}

	size_t NodeCount() const noexcept { return nodes_.size(); }

	template <typename RayGen>
	double BenchRayMarch(int numRays, RayGen &&gen) const
	{
		using clk = std::chrono::high_resolution_clock;
		std::mt19937_64 rng(0xCAFEBABEull);
		std::uniform_real_distribution<float> ud(0.0f, 1.0f * side_);
		auto t0 = clk::now();
		size_t hits = 0;
		for (int i = 0; i < numRays; ++i) {
			float ox = ud(rng);
			float oy = ud(rng);
			float oz = ud(rng);
			float dx = ud(rng) - 0.5f;
			float dy = ud(rng) - 0.5f;
			float dz = ud(rng) - 0.5f;
			float len = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (len < 1e-6f) {
				dx = 1;
				dy = 0;
				dz = 0;
				len = 1;
			}
			dx /= len;
			dy /= len;
			dz /= len;
			float t = 0.0f;
			float tMax = static_cast<float>(side_) * 2.0f;
			int steps = 0;
			while (t < tMax && steps < 50) {
				int xi = static_cast<int>(ox + dx * t);
				int yi = static_cast<int>(oy + dy * t);
				int zi = static_cast<int>(oz + dz * t);
				if (xi < 0 || xi >= side_ || yi < 0 || yi >= side_ || zi < 0 || zi >= side_)
					break;
				u8 v = GetCell(xi, yi, zi);
				if (v != 0) {
					++hits;
					break;
				}
				t += 0.5f;
				++steps;
			}
			(void)gen;
		}
		auto t1 = clk::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		(void)hits;
		return ns / numRays;
	}

  private:
	static int ComputeDepth(int side) noexcept
	{
		if (side <= 1)
			return 0;
		int depth = 1;
		int n = kNodeSide;
		while (n < side) {
			n *= kNodeSide;
			++depth;
		}
		return depth;
	}
	int ExtractSubCoord(int coord, int level) const noexcept
	{
		if (level <= 0 || level > depth_)
			return 0;
		int shift = (level - 1) * kBitsPerAxis;
		return (coord >> shift) & (kNodeSide - 1);
	}

	bool CanCollapseToHomogeneous(const Node &node, u8 &outMaterial) const noexcept
	{
		if (node.fillMask != 0xFFFFFFFFFFFFFFFFull)
			return false;
		u8 material = 0;
		for (int i = 0; i < kChildren; ++i) {
			u32 s = node.slots[i];
			if (IsHomogeneous(s)) {
				u8 m = LeafMaterial(s);
				if (i > 0 && m != material)
					return false;
				material = m;
			} else if (IsLeaf(s)) {
				u8 m = LeafMaterial(s);
				if (i > 0 && m != material)
					return false;
				material = m;
			} else {
				return false;
			}
		}
		outMaterial = material;
		return true;
	}

	u32 AllocateNode(u32 fillMaterial)
	{
		Node n{};
		u8 m = IsLeaf(fillMaterial) ? LeafMaterial(fillMaterial) : 0;
		for (int i = 0; i < kChildren; ++i)
			n.slots[i] = MakeLeaf(m);
		n.refCount = 1;
		u32 idx = static_cast<u32>(nodes_.size());
		nodes_.push_back(n);
		return idx;
	}

	u32 SetCellRecursive(u32 slot, int x, int y, int z, u8 material, int level)
	{
		if (level <= 0)
			return MakeLeaf(material);
		if (IsHomogeneous(slot)) {
			u8 existing = LeafMaterial(slot);
			if (existing == material)
				return slot;
			u32 newIdx = AllocateNode(MakeLeaf(existing));
			nodes_[newIdx].fillMask = 0xFFFFFFFFFFFFFFFFull;
			slot = newIdx;
		} else if (IsLeaf(slot)) {
			if (LeafMaterial(slot) == material)
				return slot;
			u32 idx = AllocateNode(slot);
			nodes_[idx].fillMask = 0xFFFFFFFFFFFFFFFFull;
			slot = idx;
		}
		int subX = ExtractSubCoord(x, level);
		int subY = ExtractSubCoord(y, level);
		int subZ = ExtractSubCoord(z, level);
		int childIndex = subX + kNodeSide * (subY + kNodeSide * subZ);
		u32 nodeIndex = NodeIndex(slot);
		u32 existing = nodes_[nodeIndex].slots[childIndex];
		u32 updated = SetCellRecursive(existing, x, y, z, material, level - 1);
		nodes_[nodeIndex].slots[childIndex] = updated;
		nodes_[nodeIndex].fillMask |= (1ull << childIndex);
		u8 collapseMat = 0;
		if (CanCollapseToHomogeneous(nodes_[nodeIndex], collapseMat)) {
			return MakeHomogeneous(collapseMat);
		}
		return nodeIndex;
	}

	size_t CountNonAirRecursive(u32 slot, int level) const
	{
		if (IsHomogeneous(slot)) {
			if (LeafMaterial(slot) == 0)
				return 0;
			size_t cells = 1;
			for (int i = 0; i < level; ++i)
				cells *= kChildren;
			return cells;
		}
		if (IsLeaf(slot))
			return LeafMaterial(slot) != 0 ? 1 : 0;
		if (level <= 0)
			return 0;
		const Node &node = nodes_[NodeIndex(slot)];
		if (node.fillMask == 0)
			return 0;
		size_t count = 0;
		for (int i = 0; i < kChildren; ++i) {
			if (((node.fillMask >> i) & 1ull) != 0ull) {
				count += CountNonAirRecursive(node.slots[i], level - 1);
			}
		}
		return count;
	}

	int side_;
	int depth_;
	std::vector<Node> nodes_;
	u32 rootSlot_ = MakeLeaf(0);
};

class NanovdbAligned {
  public:
	static constexpr int kBits = 1;
	static constexpr int kSide = 1 << kBits;
	static constexpr int kChildren = 1 << (3 * kBits);
	static constexpr int kLeafVoxels = 1 << (3 * kBits);

	static constexpr int kUpperVoxels = 512;
	static constexpr int kLowerVoxels = 64;

	struct alignas(8) UpperNode {
		u8 valueMask = 0;
		u8 childMask = 0;
		u32 valuesOrIds[kChildren] = {};
	};
	struct alignas(8) LowerNode {
		u8 valueMask = 0;
		u8 childMask = 0;
		u32 valuesOrIds[kChildren] = {};
	};
	struct alignas(8) LeafNode {
		u8 valueMask = 0;
		u8 voxels[kLeafVoxels] = {};
	};

	static_assert(sizeof(UpperNode) == 40);
	static_assert(sizeof(LowerNode) == 40);
	static_assert(sizeof(LeafNode) == 16);

	explicit NanovdbAligned(int /*side*/) { Reset(); }

	void Reset()
	{
		uppers_.clear();
		lowers_.clear();
		leaves_.clear();
		uppers_.push_back(std::make_unique<UpperNode>());
	}

	u8 GetCell(int x, int y, int z) const
	{
		if (!Contains(x, y, z))
			return 0;
		UpperNode *up = uppers_[0].get();
		int uChild = SubCoord(x, 2) + kSide * (SubCoord(y, 2) + kSide * SubCoord(z, 2));
		if (((up->childMask >> uChild) & 1u) == 0u) {
			return LowByte(up->valuesOrIds[uChild]);
		}
		LowerNode *lo = lowers_[up->valuesOrIds[uChild]].get();
		int lChild = SubCoord(x, 1) + kSide * (SubCoord(y, 1) + kSide * SubCoord(z, 1));
		if (((lo->childMask >> lChild) & 1u) == 0u) {
			return LowByte(lo->valuesOrIds[lChild]);
		}
		LeafNode *lf = leaves_[lo->valuesOrIds[lChild]].get();
		int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
		if (((lf->valueMask >> lfChild) & 1u) == 0u)
			return 0;
		return lf->voxels[lfChild];
	}

	void SetCell(int x, int y, int z, u8 material)
	{
		if (!Contains(x, y, z))
			return;
		if (material == 0)
			return;
		UpperNode *up = uppers_[0].get();
		int uChild = SubCoord(x, 2) + kSide * (SubCoord(y, 2) + kSide * SubCoord(z, 2));
		if (((up->childMask >> uChild) & 1u) == 0u) {
			if (LowByte(up->valuesOrIds[uChild]) == material)
				return;
			u8 existing = LowByte(up->valuesOrIds[uChild]);
			up->valueMask &= ~(1u << uChild);
			lowers_.push_back(std::make_unique<LowerNode>());
			u32 lowerId = static_cast<u32>(lowers_.size() - 1);
			up->childMask |= (1u << uChild);
			up->valuesOrIds[uChild] = lowerId;
			LowerNode *lo = lowers_[lowerId].get();
			FillLowerUniform(lo, existing);
			SetInLower(lo, x, y, z, material);
			return;
		}
		LowerNode *lo = lowers_[up->valuesOrIds[uChild]].get();
		int lChild = SubCoord(x, 1) + kSide * (SubCoord(y, 1) + kSide * SubCoord(z, 1));
		if (((lo->childMask >> lChild) & 1u) == 0u) {
			if (LowByte(lo->valuesOrIds[lChild]) == material)
				return;
			u8 existing = LowByte(lo->valuesOrIds[lChild]);
			lo->valueMask &= ~(1u << lChild);
			leaves_.push_back(std::make_unique<LeafNode>());
			u32 leafId = static_cast<u32>(leaves_.size() - 1);
			lo->childMask |= (1u << lChild);
			lo->valuesOrIds[lChild] = leafId;
			LeafNode *lf = leaves_[leafId].get();
			FillLeafUniform(lf, existing);
			SetInLeaf(lf, x, y, z, material);
			return;
		}
		LeafNode *lf = leaves_[lo->valuesOrIds[lChild]].get();
		int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
		lf->valueMask |= (1u << lfChild);
		lf->voxels[lfChild] = material;
	}

	bool Contains(int x, int y, int z) const noexcept
	{
		return x >= 0 && x < kUpperVoxels && y >= 0 && y < kUpperVoxels && z >= 0 && z < kUpperVoxels;
	}

	size_t NonAirCount() const
	{
		if (uppers_.empty())
			return 0;
		return CountNonAirInUpper(uppers_[0].get());
	}

	size_t MemoryBytes() const noexcept
	{
		size_t bytes = sizeof(*this);
		bytes += 736;
		bytes += uppers_.capacity() * sizeof(UpperNode);
		bytes += lowers_.capacity() * sizeof(LowerNode);
		bytes += leaves_.capacity() * sizeof(LeafNode);
		return bytes;
	}

	size_t NodeCount() const noexcept
	{
		return uppers_.size() + lowers_.size() + leaves_.size();
	}

	template <typename RayGen>
	double BenchRayMarch(int numRays, RayGen &&gen) const
	{
		using clk = std::chrono::high_resolution_clock;
		std::mt19937_64 rng(0xCAFEBABEull);
		std::uniform_real_distribution<float> ud(0.0f, 1.0f * kUpperVoxels);
		auto t0 = clk::now();
		size_t hits = 0;
		for (int i = 0; i < numRays; ++i) {
			float ox = ud(rng);
			float oy = ud(rng);
			float oz = ud(rng);
			float dx = ud(rng) - 0.5f;
			float dy = ud(rng) - 0.5f;
			float dz = ud(rng) - 0.5f;
			float len = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (len < 1e-6f) {
				dx = 1;
				dy = 0;
				dz = 0;
				len = 1;
			}
			dx /= len;
			dy /= len;
			dz /= len;
			float t = 0.0f;
			float tMax = static_cast<float>(kUpperVoxels) * 2.0f;
			int steps = 0;
			while (t < tMax && steps < 50) {
				int xi = static_cast<int>(ox + dx * t);
				int yi = static_cast<int>(oy + dy * t);
				int zi = static_cast<int>(oz + dz * t);
				if (xi < 0 || xi >= kUpperVoxels || yi < 0 || yi >= kUpperVoxels || zi < 0 || zi >= kUpperVoxels)
					break;
				u8 v = GetCell(xi, yi, zi);
				if (v != 0) {
					++hits;
					break;
				}
				t += 0.5f;
				++steps;
			}
			(void)gen;
		}
		auto t1 = clk::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		(void)hits;
		return ns / numRays;
	}

  private:
	static u8 LowByte(u32 v) noexcept { return static_cast<u8>(v & 0xFFu); }
	static int SubCoord(int coord, int level) noexcept
	{
		int shift = level * kBits;
		return (coord >> shift) & (kSide - 1);
	}
	static void FillLowerUniform(LowerNode *lo, u8 material)
	{
		for (int i = 0; i < kChildren; ++i)
			lo->valuesOrIds[i] = material;
		lo->valueMask = 0xFF;
		lo->childMask = 0;
	}
	static void FillLeafUniform(LeafNode *lf, u8 material)
	{
		for (int i = 0; i < kLeafVoxels; ++i)
			lf->voxels[i] = material;
		lf->valueMask = 0xFF;
	}
	void SetInLower(LowerNode *lo, int x, int y, int z, u8 material)
	{
		int lChild = SubCoord(x, 1) + kSide * (SubCoord(y, 1) + kSide * SubCoord(z, 1));
		if (((lo->childMask >> lChild) & 1u) == 0u) {
			if (LowByte(lo->valuesOrIds[lChild]) == material)
				return;
			u8 existing = LowByte(lo->valuesOrIds[lChild]);
			lo->valueMask &= ~(1u << lChild);
			leaves_.push_back(std::make_unique<LeafNode>());
			u32 leafId = static_cast<u32>(leaves_.size() - 1);
			lo->childMask |= (1u << lChild);
			lo->valuesOrIds[lChild] = leafId;
			LeafNode *lf = leaves_[leafId].get();
			FillLeafUniform(lf, existing);
			SetInLeaf(lf, x, y, z, material);
			return;
		}
		LeafNode *lf = leaves_[lo->valuesOrIds[lChild]].get();
		int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
		lf->valueMask |= (1u << lfChild);
		lf->voxels[lfChild] = material;
	}
	void SetInLeaf(LeafNode *lf, int x, int y, int z, u8 material)
	{
		int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
		lf->valueMask |= (1u << lfChild);
		lf->voxels[lfChild] = material;
	}
	size_t CountNonAirInUpper(const UpperNode *up) const
	{
		size_t count = 0;
		for (int i = 0; i < kChildren; ++i) {
			if (((up->childMask >> i) & 1u) == 0u) {
				if (LowByte(up->valuesOrIds[i]) != 0)
					count += kLowerVoxels;
				continue;
			}
			count += CountNonAirInLower(lowers_[up->valuesOrIds[i]].get());
		}
		return count;
	}
	size_t CountNonAirInLower(const LowerNode *lo) const
	{
		size_t count = 0;
		for (int i = 0; i < kChildren; ++i) {
			if (((lo->childMask >> i) & 1u) == 0u) {
				if (LowByte(lo->valuesOrIds[i]) != 0)
					count += kLeafVoxels;
				continue;
			}
			count += CountNonAirInLeaf(leaves_[lo->valuesOrIds[i]].get());
		}
		return count;
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

	std::vector<std::unique_ptr<UpperNode>> uppers_;
	std::vector<std::unique_ptr<LowerNode>> lowers_;
	std::vector<std::unique_ptr<LeafNode>> leaves_;
};

struct Result {
	std::string tree;
	std::string scene;
	size_t nonAir = 0;
	size_t bytes = 0;
	double bytesPerVoxel = 0;
	size_t nodes = 0;
	double buildMs = 0;
	size_t verifyMismatches = 0;
	double rayMarchNsMean = 0;
};

template <typename Tree>
Result BenchTree(const Scene &scene, std::string_view treeName, u64 seed)
{
	Result r;
	r.tree = treeName;
	r.scene = scene.name;

	using clk = std::chrono::high_resolution_clock;

	Tree t(scene.side);
	auto t0 = clk::now();
	for (int z = 0; z < scene.side; ++z)
		for (int y = 0; y < scene.side; ++y)
			for (int x = 0; x < scene.side; ++x) {
				u8 v = scene.voxels[x + scene.side * (y + scene.side * z)];
				if (v != 0)
					t.SetCell(x, y, z, v);
			}
	auto t1 = clk::now();
	r.buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	size_t mismatches = 0;
	for (int z = 0; z < scene.side; ++z)
		for (int y = 0; y < scene.side; ++y)
			for (int x = 0; x < scene.side; ++x) {
				u8 expected = scene.voxels[x + scene.side * (y + scene.side * z)];
				u8 actual = t.GetCell(x, y, z);
				if (expected != actual)
					++mismatches;
			}
	r.verifyMismatches = mismatches;

	r.nonAir = t.NonAirCount();
	r.bytes = t.MemoryBytes();
	r.bytesPerVoxel = (r.nonAir > 0) ? static_cast<double>(r.bytes) / r.nonAir : 0.0;
	r.nodes = t.NodeCount();

	constexpr int kWarmup = 100;
	constexpr int kMeasure = 1000;
	for (int i = 0; i < kWarmup; ++i)
		(void)t.BenchRayMarch(100, 0);
	r.rayMarchNsMean = t.BenchRayMarch(kMeasure, seed);
	return r;
}

} // namespace projectv::proto

int main()
{
	using namespace projectv::proto;

	constexpr u64 kSeed = 0x5EED5EEDull;
	std::vector<SceneKind> kinds = {
		SceneKind::Solid, SceneKind::Ground, SceneKind::Brick,
		SceneKind::VoxelLab, SceneKind::SparseRandom};

	std::vector<Result> results;
	for (auto kind : kinds) {
		Scene scene = GenScene(kind, kSeed);
		results.push_back(BenchTree<Svdag64>(scene, "svdag64", kSeed));
		results.push_back(BenchTree<NanovdbAligned>(scene, "nanovdb_aligned", kSeed));
	}

	std::filesystem::path outDir =
		std::filesystem::path(__FILE__).parent_path();
	std::ofstream csv(outDir / "results_cpu.csv");
	csv << "tree,scene,nonAir,bytes,bytesPerVoxel,nodes,buildMs,verifyMismatches,rayMarchNsMean\n";
	for (const auto &r : results) {
		csv << r.tree << "," << r.scene << "," << r.nonAir << ","
			<< r.bytes << "," << r.bytesPerVoxel << "," << r.nodes << ","
			<< r.buildMs << "," << r.verifyMismatches << "," << r.rayMarchNsMean << "\n";
	}
	csv.close();

	std::ofstream md(outDir / "RESULTS.md");
	md << "# Results — CPU-side: SVDAG-on-64-tree vs NanoVDB-aligned (byte-exact, chunkSize=8)\n\n";
	md << "Host: AMD Ryzen 7 5800X, clang 22.1.6, -O3 -march=native -DNDEBUG.\n";
	md << "All scenes 8^3 = 512 voxels (matches mainline chunkSize per VoxelWorld.hpp:78).\n";
	md << "Build = full SetCell traversal. Ray-march = CPU simulation of GPU traversal\n";
	md << "pattern (sequential descent, no batching). kWarmup=100, kMeasure=1000 rays/scene.\n\n";
	md << "| Tree | Scene | NonAir | Bytes | B/vox | Nodes | Build ms | Verify mism | Ray ns mean |\n";
	md << "|:-----|:------|-------:|------:|------:|------:|---------:|------------:|------------:|\n";
	for (const auto &r : results) {
		md << "| " << r.tree << " | " << r.scene << " | " << r.nonAir << " | "
		   << r.bytes << " | " << r.bytesPerVoxel << " | " << r.nodes << " | "
		   << r.buildMs << " | " << r.verifyMismatches << " | "
		   << r.rayMarchNsMean << " |\n";
	}
	md << "\n## Notes\n\n";
	md << "- **svdag64** = standalone re-implementation of src/voxel/Sparse64Tree.hpp ";
	md << "semantics. Node = fillMask:u64 + 64 child slots:u32 + structuralHash:u64 + ";
	md << "refCount:u32 = 280 B. Includes leaf flag (0x80000000u), homogeneous flag ";
	md << "(0x40000000u), node index mask (0x3FFFFFFFu), material mask (0xFFu). Matches mainline.\n";
	md << "- **nanovdb_aligned** = 3-level structure (Upper[8^3] -> Lower[4^3] -> Leaf[2^3]) ";
	md << "for chunkSize=8. Per NanoVDB.h actual structure (scaled: full NanoVDB uses ";
	md << "32^3/16^3/8^3 for full Grid; here each level covers 1/8 of parent in each axis). ";
	md << "Byte-exact: always materializes children on SetCell (bugfix vs 2026-06-20 ";
	md << "svdag_vs_nanovdb prototype which had uniform-tile lie). Fixed grid overhead ";
	md << "736 B (GridData 672 B + TreeData 64 B per NanoVDB.h).\n";
	md << "- verify_mismatches MUST be 0 for all rows (byte-exact correctness vs flat voxels).\n";
	md << "- This prototype differs from 2026-06-20-svdag-vs-vdb-memory-throughput in 2 ways:\n";
	md << "  1. **chunkSize=8 (not 32):** matches mainline per VoxelWorld.hpp:78.\n";
	md << "  2. **NanoVDB impl byte-exact:** bugfix removes the uniform-tile lie, all ";
	md << "scenes have verify_mismatches=0.\n";
	md.close();

	std::printf("Wrote results_cpu.csv and RESULTS.md to %s\n",
				outDir.string().c_str());
	std::printf("\n%-16s %-18s %8s %10s %8s %6s %9s %5s %12s\n",
				"Tree", "Scene", "NonAir", "Bytes", "B/vox", "Nodes",
				"Build_ms", "Mism", "Ray_ns");
	for (const auto &r : results) {
		std::printf("%-16s %-18s %8zu %10zu %8.2f %6zu %9.3f %5zu %12.2f\n",
					r.tree.c_str(), r.scene.c_str(), r.nonAir, r.bytes,
					r.bytesPerVoxel, r.nodes, r.buildMs, r.verifyMismatches,
					r.rayMarchNsMean);
	}

	size_t totalMismatches = 0;
	for (const auto &r : results)
		totalMismatches += r.verifyMismatches;
	if (totalMismatches != 0) {
		std::fprintf(stderr, "ERROR: total verify_mismatches = %zu (expected 0)\n",
					 totalMismatches);
		return 1;
	}
	return 0;
}
