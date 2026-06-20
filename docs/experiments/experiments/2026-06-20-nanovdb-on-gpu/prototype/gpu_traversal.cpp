// SPDX-License-Identifier: MIT
//
// gpu_traversal.cpp - Standalone Vulkan compute prototype measuring GPU
// ray-march throughput (Mrays/s) for SVDAG-on-64-tree vs NanoVDB-aligned
// GPU buffer layouts. RTX 3060 Ti target (Vulkan 1.4.341+, subgroupSize=32).
//
// Compiled SPIR-V shaders embedded:
//   svdag64_spv.h     - SVDAG-on-64-tree traversal (depth=2 for chunkSize=8)
//   nanovdb_spv.h     - NanoVDB-aligned traversal (Upper->Lower->Leaf)
//
// Scope per docs/experiments/AGENTS.md section 2: standalone research artifact,
// NOT part of ProjectV mainline.
//
// Build:
//   clang++ -std=c++26 -O3 -march=native -DNDEBUG \
//     docs/experiments/experiments/2026-06-20-nanovdb-on-gpu/prototype/gpu_traversal.cpp \
//     -o /tmp/gpu_traversal -lvulkan
//
// Run:
//   /tmp/gpu_traversal

#include <vulkan/vulkan.h>

// Workaround: this system's vulkan_core.h is missing several pipeline stage bits
// (COMPUTE, TOP/BOTTOM_PIPE, DRAW_INDIRECT, VERTEX_INPUT) that are core in
// Vulkan 1.0+. Define what we need and cast at call sites.
static constexpr VkPipelineStageFlagBits kPipelineStageCompute =
	static_cast<VkPipelineStageFlagBits>(0x00000002u);

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "svdag64_spv.h"
#include "nanovdb_spv.h"

#define VK_CHECK(call)                                              \
	do {                                                            \
		VkResult _r = (call);                                       \
		if (_r != VK_SUCCESS) {                                     \
			std::fprintf(stderr, "Vulkan error: %d at %s:%d\n",     \
						 static_cast<int>(_r), __FILE__, __LINE__); \
			std::exit(1);                                           \
		}                                                           \
	} while (0)

namespace projectv::proto {

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;

inline constexpr int kBitsPerAxis = 2;
inline constexpr int kNodeSide = 1 << kBitsPerAxis;
inline constexpr int kChildren = 1 << (3 * kBitsPerAxis);
inline constexpr u32 kLeafFlag = 0x80000000u;
inline constexpr u32 kHomogeneousFlag = 0x40000000u;
inline constexpr u32 kNodeIndexMask = 0x3FFFFFFFu;
inline constexpr u32 kMaterialMask = 0xFFu;
inline constexpr int kChunkSide = 8;
inline constexpr int kChunkVoxels = kChunkSide * kChunkSide * kChunkSide;
inline constexpr int kGpuRays = 65536; // 64k rays per dispatch (1 workgroup = 64 invocations)

inline u32 MakeLeaf(u8 m)
{
	return kLeafFlag | static_cast<u32>(m);
}
inline u32 MakeHomogeneous(u8 m)
{
	return kLeafFlag | kHomogeneousFlag | static_cast<u32>(m);
}
inline bool IsLeaf(u32 slot)
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == kLeafFlag;
}
inline bool IsHomogeneous(u32 slot)
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == (kLeafFlag | kHomogeneousFlag);
}
inline u32 NodeIndexFromSlot(u32 slot)
{
	return slot & kNodeIndexMask;
}

// =============================================================================
// Scene generators.
// =============================================================================

enum class SceneKind { Solid,
					   Ground,
					   Brick,
					   VoxelLab,
					   SparseRandom };

struct Scene {
	std::vector<u8> voxels;
	std::string name;
};

Scene GenScene(SceneKind kind, u64 seed)
{
	Scene s;
	s.voxels.assign(kChunkVoxels, 0);
	std::mt19937_64 rng(seed);

	auto idx = [](int x, int y, int z) {
		return x + kChunkSide * (y + kChunkSide * z);
	};

	switch (kind) {
	case SceneKind::Solid: {
		std::fill(s.voxels.begin(), s.voxels.end(), 1);
		s.name = "solid_8";
		break;
	}
	case SceneKind::Ground: {
		for (int y = 0; y < kChunkSide; ++y)
			for (int x = 0; x < kChunkSide; ++x)
				s.voxels[idx(x, y, 0)] = 1;
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
		s.name = "brick_8";
		break;
	}
	case SceneKind::VoxelLab: {
		std::uniform_real_distribution<double> dist(0.0, 1.0);
		for (int z = 0; z < kChunkSide; ++z)
			for (int y = 0; y < kChunkSide; ++y)
				for (int x = 0; x < kChunkSide; ++x) {
					double height = static_cast<double>(z) / kChunkSide;
					if (dist(rng) < 0.05 + 0.3 * height)
						s.voxels[idx(x, y, z)] = static_cast<u8>(1 + (z / 4) % 3);
				}
		s.name = "voxel_lab_8";
		break;
	}
	case SceneKind::SparseRandom: {
		std::uniform_int_distribution<int> mat(1, 3);
		std::uniform_real_distribution<double> dens(0.0, 1.0);
		for (int z = 0; z < kChunkSide; ++z)
			for (int y = 0; y < kChunkSide; ++y)
				for (int x = 0; x < kChunkSide; ++x) {
					if (dens(rng) < 0.1)
						s.voxels[idx(x, y, z)] = static_cast<u8>(mat(rng));
				}
		s.name = "sparse_random_8";
		break;
	}
	}
	return s;
}

// =============================================================================
// SVDAG-on-64-tree CPU builder -> GPU buffer.
// =============================================================================

struct SvdagGpuData {
	std::vector<u32> nodeData; // packed: fillMask + children[64] per node, 264 B per node
	u32 rootNodeIndex = 0;
	int depth = 2;
};

struct alignas(8) CpuNode {
	u64 fillMask = 0;
	std::array<u32, kChildren> slots{};
	u32 refCount = 0;
};

bool CanCollapse(CpuNode &n, u8 &outMat)
{
	if (n.fillMask != 0xFFFFFFFFFFFFFFFFull)
		return false;
	u8 m = 0;
	for (int i = 0; i < kChildren; ++i) {
		u32 s = n.slots[i];
		u8 sm = static_cast<u8>(s & 0xFFu);
		if ((s & 0xC0000000u) == 0xC0000000u || (s & 0x80000000u) != 0u) {
			if (i > 0 && sm != m)
				return false;
			m = sm;
		} else
			return false;
	}
	outMat = m;
	return true;
}

u32 SetCellRec(std::vector<CpuNode> &nodes, u32 slot, int x, int y, int z,
			   u8 material, int level)
{
	if (level <= 0)
		return MakeLeaf(material);
	if (IsHomogeneous(slot)) {
		u8 existing = static_cast<u8>(slot & 0xFFu);
		if (existing == material)
			return slot;
		CpuNode n{};
		n.fillMask = 0xFFFFFFFFFFFFFFFFull;
		for (int i = 0; i < kChildren; ++i)
			n.slots[i] = MakeLeaf(existing);
		n.refCount = 1;
		nodes.push_back(n);
		slot = static_cast<u32>(nodes.size() - 1);
	} else if (IsLeaf(slot)) {
		if (static_cast<u8>(slot & 0xFFu) == material)
			return slot;
		CpuNode n{};
		n.fillMask = 0xFFFFFFFFFFFFFFFFull;
		for (int i = 0; i < kChildren; ++i)
			n.slots[i] = slot;
		n.refCount = 1;
		nodes.push_back(n);
		slot = static_cast<u32>(nodes.size() - 1);
	}
	int subX = (x >> ((level - 1) * kBitsPerAxis)) & (kNodeSide - 1);
	int subY = (y >> ((level - 1) * kBitsPerAxis)) & (kNodeSide - 1);
	int subZ = (z >> ((level - 1) * kBitsPerAxis)) & (kNodeSide - 1);
	int childIndex = subX + kNodeSide * (subY + kNodeSide * subZ);
	u32 existing = nodes[slot].slots[childIndex];
	u32 updated = SetCellRec(nodes, existing, x, y, z, material, level - 1);
	nodes[slot].slots[childIndex] = updated;
	nodes[slot].fillMask |= (1ull << childIndex);
	u8 collapseMat = 0;
	if (CanCollapse(nodes[slot], collapseMat)) {
		return MakeHomogeneous(collapseMat);
	}
	return slot;
}

SvdagGpuData BuildSvdagGpu(const Scene &scene)
{
	SvdagGpuData out;
	out.depth = 2;				   // chunkSide=8 -> depth=2
	std::vector<CpuNode> nodes(1); // 1 root node
	nodes[0].refCount = 1;
	u32 rootSlot = MakeLeaf(0);
	int side = kChunkSide;
	for (int z = 0; z < side; ++z)
		for (int y = 0; y < side; ++y)
			for (int x = 0; x < side; ++x) {
				u8 v = scene.voxels[x + side * (y + side * z)];
				if (v != 0) {
					rootSlot = SetCellRec(nodes, rootSlot, x, y, z, v, out.depth);
				}
			}
	// If rootSlot is a leaf/homogeneous (no internal nodes needed), create an empty root.
	if (IsLeaf(rootSlot) || IsHomogeneous(rootSlot)) {
		CpuNode empty{};
		empty.refCount = 1;
		if (IsHomogeneous(rootSlot)) {
			empty.fillMask = 0xFFFFFFFFFFFFFFFFull;
			for (int i = 0; i < kChildren; ++i)
				empty.slots[i] = rootSlot;
		}
		nodes.push_back(empty);
		out.rootNodeIndex = static_cast<u32>(nodes.size() - 1);
	} else {
		out.rootNodeIndex = rootSlot;
	}
	// Pack nodes into GPU buffer: [fillMask:u32, fillMask_hi:u32, children[64]]
	// For simplicity, we use fillMask as 1 u32 (only lower 32 bits used since kChildren=64 fits in u64).
	// Actually, fillMask needs u64. Layout: fillMask_lo, fillMask_hi, then children[64].
	// Each child is u32, so total per node = 2 + 64 = 66 u32s = 264 B.
	out.nodeData.reserve(nodes.size() * 66);
	for (const auto &n : nodes) {
		out.nodeData.push_back(static_cast<u32>(n.fillMask & 0xFFFFFFFFu));
		out.nodeData.push_back(static_cast<u32>((n.fillMask >> 32) & 0xFFFFFFFFu));
		for (u32 c : n.slots)
			out.nodeData.push_back(c);
	}
	return out;
}

// =============================================================================
// NanoVDB-aligned CPU builder -> GPU buffers.
// =============================================================================

struct NanovdbGpuData {
	std::vector<u32> upperData; // [valueMask, childMask, valuesOrIds[8]] = 10 u32 = 40 B per Upper
	std::vector<u32> lowerData; // [valueMask, childMask, valuesOrIds[8]] = 10 u32 = 40 B per Lower
	std::vector<u32> leafData;	// [valueMask, voxels_packed[2]] = 3 u32 = 12 B per Leaf
};

struct CpuUpper {
	u8 valueMask = 0;
	u8 childMask = 0;
	u32 valuesOrIds[8] = {};
};
struct CpuLower {
	u8 valueMask = 0;
	u8 childMask = 0;
	u32 valuesOrIds[8] = {};
};
struct CpuLeaf {
	u8 valueMask = 0;
	u8 voxels[8] = {};
};

static u8 LowByte(u32 v)
{
	return static_cast<u8>(v & 0xFFu);
}
static int SubC(int c, int level)
{
	return (c >> (level * 1)) & 1;
}

void SetInLower(std::vector<CpuLower> &lo, std::vector<CpuLeaf> &lf,
				u32 lowerId, int x, int y, int z, u8 material)
{
	int lChild = SubC(x, 1) + 2 * (SubC(y, 1) + 2 * SubC(z, 1));
	if (((lo[lowerId].childMask >> lChild) & 1u) == 0u) {
		if (LowByte(lo[lowerId].valuesOrIds[lChild]) == material)
			return;
		u8 existing = LowByte(lo[lowerId].valuesOrIds[lChild]);
		lo[lowerId].valueMask &= ~(1u << lChild);
		CpuLeaf newLf{};
		for (int i = 0; i < 8; ++i)
			newLf.voxels[i] = existing;
		newLf.valueMask = 0xFF;
		lf.push_back(newLf);
		u32 leafId = static_cast<u32>(lf.size() - 1);
		lo[lowerId].childMask |= (1u << lChild);
		lo[lowerId].valuesOrIds[lChild] = leafId;
		int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
		lf[leafId].valueMask |= (1u << lfChild);
		lf[leafId].voxels[lfChild] = material;
		return;
	}
	u32 leafId = lo[lowerId].valuesOrIds[lChild];
	int lfChild = (x & 1) + 2 * ((y & 1) + 2 * (z & 1));
	lf[leafId].valueMask |= (1u << lfChild);
	lf[leafId].voxels[lfChild] = material;
}

void SetInUpper(std::vector<CpuUpper> &up, std::vector<CpuLower> &lo,
				std::vector<CpuLeaf> &lf, int x, int y, int z, u8 material)
{
	int uChild = SubC(x, 2) + 2 * (SubC(y, 2) + 2 * SubC(z, 2));
	if (((up[0].childMask >> uChild) & 1u) == 0u) {
		if (LowByte(up[0].valuesOrIds[uChild]) == material)
			return;
		u8 existing = LowByte(up[0].valuesOrIds[uChild]);
		up[0].valueMask &= ~(1u << uChild);
		CpuLower newLo{};
		newLo.valueMask = 0xFF;
		for (int i = 0; i < 8; ++i)
			newLo.valuesOrIds[i] = existing;
		lo.push_back(newLo);
		u32 lowerId = static_cast<u32>(lo.size() - 1);
		up[0].childMask |= (1u << uChild);
		up[0].valuesOrIds[uChild] = lowerId;
		SetInLower(lo, lf, lowerId, x, y, z, material);
		return;
	}
	u32 lowerId = up[0].valuesOrIds[uChild];
	SetInLower(lo, lf, lowerId, x, y, z, material);
}

NanovdbGpuData BuildNanovdbGpu(const Scene &scene)
{
	NanovdbGpuData out;
	std::vector<CpuUpper> up(1);
	std::vector<CpuLower> lo;
	std::vector<CpuLeaf> lf;
	for (int z = 0; z < kChunkSide; ++z)
		for (int y = 0; y < kChunkSide; ++y)
			for (int x = 0; x < kChunkSide; ++x) {
				u8 v = scene.voxels[x + kChunkSide * (y + kChunkSide * z)];
				if (v != 0)
					SetInUpper(up, lo, lf, x, y, z, v);
			}
	// Pack uppers.
	out.upperData.reserve(up.size() * 10);
	for (const auto &u : up) {
		out.upperData.push_back(u.valueMask);
		out.upperData.push_back(u.childMask);
		for (int i = 0; i < 8; ++i)
			out.upperData.push_back(u.valuesOrIds[i]);
	}
	// Pack lowers.
	out.lowerData.reserve(lo.size() * 10);
	for (const auto &l : lo) {
		out.lowerData.push_back(l.valueMask);
		out.lowerData.push_back(l.childMask);
		for (int i = 0; i < 8; ++i)
			out.lowerData.push_back(l.valuesOrIds[i]);
	}
	// Pack leaves: [valueMask, voxels[0..3] packed, voxels[4..7] packed]
	out.leafData.reserve(lf.size() * 3);
	for (const auto &l : lf) {
		out.leafData.push_back(l.valueMask);
		u32 lo_packed = l.voxels[0] | (l.voxels[1] << 8) | (l.voxels[2] << 16) | (l.voxels[3] << 24);
		u32 hi_packed = l.voxels[4] | (l.voxels[5] << 8) | (l.voxels[6] << 16) | (l.voxels[7] << 24);
		out.leafData.push_back(lo_packed);
		out.leafData.push_back(hi_packed);
	}
	return out;
}

// =============================================================================
// Ray generator (consistent across both kernels).
// =============================================================================

std::vector<float> GenerateRays(u64 seed)
{
	std::mt19937_64 rng(seed);
	std::uniform_real_distribution<float> ud(0.0f, 1.0f * kChunkSide);
	std::vector<float> rays;
	rays.reserve(kGpuRays * 4);
	for (int i = 0; i < kGpuRays; ++i) {
		float ox = ud(rng);
		float oy = ud(rng);
		float oz = ud(rng);
		float t = 0.0f;
		rays.push_back(ox);
		rays.push_back(oy);
		rays.push_back(oz);
		rays.push_back(t);
	}
	return rays;
}

// =============================================================================
// Vulkan boilerplate (minimal: instance, device, queue, command pool, buffers).
// =============================================================================

struct VkContext {
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physDev = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	uint32_t computeFamily = 0;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties props{};
};

void CreateContext(VkContext &ctx)
{
	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "nanovdb-on-gpu-prototype";
	app.apiVersion = VK_API_VERSION_1_4;
	VkInstanceCreateInfo ic{};
	ic.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ic.pApplicationInfo = &app;
	VK_CHECK(vkCreateInstance(&ic, nullptr, &ctx.instance));

	uint32_t devCount = 0;
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
	std::vector<VkPhysicalDevice> devs(devCount);
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, devs.data());
	ctx.physDev = devs[0]; // first device = RTX 3060 Ti
	vkGetPhysicalDeviceProperties(ctx.physDev, &ctx.props);

	// Find compute queue family.
	uint32_t qfCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physDev, &qfCount, nullptr);
	std::vector<VkQueueFamilyProperties> qfs(qfCount);
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physDev, &qfCount, qfs.data());
	ctx.computeFamily = 0;
	for (uint32_t i = 0; i < qfCount; ++i) {
		if ((qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
			ctx.computeFamily = i;
			break;
		}
	}
	float prio = 1.0f;
	VkDeviceQueueCreateInfo qc{};
	qc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qc.queueFamilyIndex = ctx.computeFamily;
	qc.queueCount = 1;
	qc.pQueuePriorities = &prio;
	VkDeviceCreateInfo dc{};
	dc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dc.queueCreateInfoCount = 1;
	dc.pQueueCreateInfos = &qc;
	VK_CHECK(vkCreateDevice(ctx.physDev, &dc, nullptr, &ctx.device));
	vkGetDeviceQueue(ctx.device, ctx.computeFamily, 0, &ctx.computeQueue);

	VkCommandPoolCreateInfo pc{};
	pc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pc.queueFamilyIndex = ctx.computeFamily;
	VK_CHECK(vkCreateCommandPool(ctx.device, &pc, nullptr, &ctx.cmdPool));
}

void DestroyContext(VkContext &ctx)
{
	vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
	vkDestroyDevice(ctx.device, nullptr);
	vkDestroyInstance(ctx.instance, nullptr);
}

struct DeviceBuffer {
	VkBuffer buf = VK_NULL_HANDLE;
	VkDeviceMemory mem = VK_NULL_HANDLE;
	VkDeviceSize size = 0;
};

uint32_t FindMemoryType(VkPhysicalDevice physDev, uint32_t typeBits, VkMemoryPropertyFlags props)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(physDev, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
		if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) {
			return i;
		}
	}
	return UINT32_MAX;
}

DeviceBuffer CreateBuffer(VkDevice dev, VkPhysicalDevice physDev, VkDeviceSize size,
						  VkBufferUsageFlags usage, const void *data = nullptr)
{
	DeviceBuffer b;
	b.size = size;
	VkBufferCreateInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = size;
	bi.usage = usage;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(dev, &bi, nullptr, &b.buf));
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, b.buf, &req);
	VkMemoryAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	ai.allocationSize = req.size;
	ai.memoryTypeIndex = FindMemoryType(physDev, req.memoryTypeBits,
										VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
											VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &b.mem));
	VK_CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
	if (data) {
		void *mapped;
		VK_CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &mapped));
		std::memcpy(mapped, data, size);
		vkUnmapMemory(dev, b.mem);
	}
	return b;
}

void DestroyBuffer(VkDevice dev, DeviceBuffer &b)
{
	if (b.buf)
		vkDestroyBuffer(dev, b.buf, nullptr);
	if (b.mem)
		vkFreeMemory(dev, b.mem, nullptr);
	b = {};
}

// =============================================================================
// Compute dispatch + timestamp query helper.
// =============================================================================

double RunCompute(VkContext &ctx, VkPipeline pipeline, VkPipelineLayout layout,
				  VkDescriptorSet descriptorSet,
				  const void *pushConstants, uint32_t pushSize,
				  uint32_t groupCount)
{
	VkCommandBufferAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	ai.commandPool = ctx.cmdPool;
	ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	ai.commandBufferCount = 1;
	VkCommandBuffer cmd;
	VK_CHECK(vkAllocateCommandBuffers(ctx.device, &ai, &cmd));

	VkQueryPoolCreateInfo qci{};
	qci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	qci.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qci.queryCount = 2;
	VkQueryPool qp;
	VK_CHECK(vkCreateQueryPool(ctx.device, &qci, nullptr, &qp));

	VkCommandBufferBeginInfo bi{};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(cmd, &bi));
	vkCmdResetQueryPool(cmd, qp, 0, 2);
	vkCmdWriteTimestamp(cmd, kPipelineStageCompute, qp, 0);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout,
							0, 1, &descriptorSet, 0, nullptr);
	vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, pushSize, pushConstants);
	vkCmdDispatch(cmd, groupCount, 1, 1);
	vkCmdWriteTimestamp(cmd, kPipelineStageCompute, qp, 1);
	VK_CHECK(vkEndCommandBuffer(cmd));

	VkSubmitInfo si{};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cmd;
	VK_CHECK(vkQueueSubmit(ctx.computeQueue, 1, &si, VK_NULL_HANDLE));
	VK_CHECK(vkQueueWaitIdle(ctx.computeQueue));

	uint64_t ts[2];
	vkGetQueryPoolResults(ctx.device, qp, 0, 2, sizeof(ts), ts, sizeof(uint64_t),
						  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	double ns = static_cast<double>(ts[1] - ts[0]) *
				ctx.props.limits.timestampPeriod;
	vkDestroyQueryPool(ctx.device, qp, nullptr);
	vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1, &cmd);
	return ns;
}

// =============================================================================
// Pipeline factory.
// =============================================================================

struct Pipeline {
	VkPipeline pipe = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsLayout = VK_NULL_HANDLE;
	VkDescriptorPool descPool = VK_NULL_HANDLE;
};

Pipeline CreatePipeline(VkContext &ctx, const unsigned char *spirv, size_t spirvSize,
						uint32_t numBindings)
{
	Pipeline p;
	VkDescriptorSetLayoutBinding bindings[8] = {};
	for (uint32_t i = 0; i < numBindings; ++i) {
		bindings[i].binding = i;
		bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	VkDescriptorSetLayoutCreateInfo dlci{};
	dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dlci.bindingCount = numBindings;
	dlci.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(ctx.device, &dlci, nullptr, &p.dsLayout));

	VkPushConstantRange pcr{};
	pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcr.offset = 0;
	pcr.size = 8; // 2 x uint32: SVDAG uses {rootSlotPacked, chunkSideDepth}; NanoVDB uses 1 x uint32
	VkPipelineLayoutCreateInfo plci{};
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &p.dsLayout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	VK_CHECK(vkCreatePipelineLayout(ctx.device, &plci, nullptr, &p.layout));

	VkShaderModuleCreateInfo smci{};
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = spirvSize;
	smci.pCode = reinterpret_cast<const uint32_t *>(spirv);
	VkShaderModule sm;
	VK_CHECK(vkCreateShaderModule(ctx.device, &smci, nullptr, &sm));

	VkComputePipelineCreateInfo cpci{};
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = sm;
	cpci.stage.pName = "main";
	cpci.layout = p.layout;
	VK_CHECK(vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &cpci, nullptr, &p.pipe));
	vkDestroyShaderModule(ctx.device, sm, nullptr);

	// Descriptor pool for 1 set with numBindings storage buffers.
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = numBindings;
	VkDescriptorPoolCreateInfo dpci{};
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &poolSize;
	VK_CHECK(vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &p.descPool));
	return p;
}

VkDescriptorSet AllocateAndUpdateDescriptorSet(VkContext &ctx, Pipeline &p,
											   const std::vector<VkBuffer> &buffers)
{
	VkDescriptorSetAllocateInfo ai{};
	ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	ai.descriptorPool = p.descPool;
	ai.descriptorSetCount = 1;
	VkDescriptorSetLayout layout = p.dsLayout;
	ai.pSetLayouts = &layout;
	VkDescriptorSet set;
	VK_CHECK(vkAllocateDescriptorSets(ctx.device, &ai, &set));
	std::vector<VkDescriptorBufferInfo> infos;
	std::vector<VkWriteDescriptorSet> writes;
	infos.reserve(buffers.size());
	writes.reserve(buffers.size());
	for (size_t i = 0; i < buffers.size(); ++i) {
		VkDescriptorBufferInfo info{};
		info.buffer = buffers[i];
		info.offset = 0;
		info.range = VK_WHOLE_SIZE;
		infos.push_back(info);
		VkWriteDescriptorSet w{};
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.dstSet = set;
		w.dstBinding = static_cast<uint32_t>(i);
		w.descriptorCount = 1;
		w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		w.pBufferInfo = &infos[i];
		writes.push_back(w);
	}
	vkUpdateDescriptorSets(ctx.device, static_cast<uint32_t>(writes.size()),
						   writes.data(), 0, nullptr);
	return set;
}

// =============================================================================
// Verify GPU hits vs CPU reference.
// =============================================================================

u32 VerifyHits(const std::vector<u32> &hits, const Scene &scene)
{
	// Reconstruct ray origins deterministically and compare to CPU GetCell.
	std::mt19937_64 rng(0xCAFEBABEull);
	std::uniform_real_distribution<float> ud(0.0f, 1.0f * kChunkSide);
	u32 mismatches = 0;
	for (int i = 0; i < kGpuRays; ++i) {
		float ox = ud(rng);
		float oy = ud(rng);
		float oz = ud(rng);
		float t = 0.0f;
		float tMax = static_cast<float>(kChunkSide) * 1.5f;
		bool cpuHit = false;
		int steps = 0;
		while (t < tMax && steps < 50) {
			int xi = static_cast<int>(ox + t);
			int yi = static_cast<int>(oy + t);
			int zi = static_cast<int>(oz + t);
			if (xi < 0 || xi >= kChunkSide || yi < 0 || yi >= kChunkSide || zi < 0 || zi >= kChunkSide)
				break;
			if (scene.voxels[xi + kChunkSide * (yi + kChunkSide * zi)] != 0) {
				cpuHit = true;
				break;
			}
			t += 0.5f;
			++steps;
		}
		if ((hits[i] != 0) != cpuHit)
			++mismatches;
	}
	return mismatches;
}

// =============================================================================
// Main benchmark.
// =============================================================================

struct GpuResult {
	std::string scene;
	std::string kernel;
	double meanMs = 0;
	double mraysPerSec = 0;
	uint32_t verifyMismatches = 0;
	size_t gpuBytes = 0;
};

double BenchKernel(VkContext &ctx, Pipeline &pipe,
				   const std::vector<VkBuffer> &buffers,
				   const void *pushConstants, uint32_t pushSize)
{
	VkDescriptorSet ds = AllocateAndUpdateDescriptorSet(ctx, pipe, buffers);
	constexpr int kWarmup = 10;
	constexpr int kMeasure = 50;
	for (int i = 0; i < kWarmup; ++i) {
		(void)RunCompute(ctx, pipe.pipe, pipe.layout, ds, pushConstants, pushSize,
						 kGpuRays / 64);
	}
	std::vector<double> samples;
	samples.reserve(kMeasure);
	for (int i = 0; i < kMeasure; ++i) {
		double ns = RunCompute(ctx, pipe.pipe, pipe.layout, ds, pushConstants, pushSize,
							   kGpuRays / 64);
		samples.push_back(ns);
	}
	std::sort(samples.begin(), samples.end());
	double sum = 0;
	for (double s : samples)
		sum += s;
	vkFreeDescriptorSets(ctx.device, pipe.descPool, 1, &ds);
	return sum / samples.size();
}

} // namespace projectv::proto

int main()
{
	using namespace projectv::proto;

	VkContext ctx;
	CreateContext(ctx);
	std::printf("GPU: %s (Vulkan %d.%d, driver %s)\n",
				ctx.props.deviceName,
				VK_VERSION_MAJOR(ctx.props.apiVersion),
				VK_VERSION_MINOR(ctx.props.apiVersion),
				"n/a");

	Pipeline svdagPipe = CreatePipeline(ctx, svdag64_spv, sizeof(svdag64_spv), 3);
	Pipeline nanovdbPipe = CreatePipeline(ctx, nanovdb_spv, sizeof(nanovdb_spv), 5);

	// Generate rays once (shared input across all dispatches).
	auto rays = GenerateRays(0xCAFEBABEull);
	DeviceBuffer rayBuf = CreateBuffer(ctx.device, ctx.physDev,
									   sizeof(float) * rays.size(),
									   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
									   rays.data());

	std::vector<SceneKind> kinds = {
		SceneKind::Solid, SceneKind::Ground, SceneKind::Brick,
		SceneKind::VoxelLab, SceneKind::SparseRandom};

	std::vector<GpuResult> gpuResults;

	for (auto kind : kinds) {
		Scene scene = GenScene(kind, 0x5EED5EEDull);

		// Build both GPU layouts.
		SvdagGpuData svdag = BuildSvdagGpu(scene);
		NanovdbGpuData nanovdb = BuildNanovdbGpu(scene);

		// Create buffers.
		DeviceBuffer svdagNodeBuf = CreateBuffer(ctx.device, ctx.physDev,
												 sizeof(u32) * svdag.nodeData.size(),
												 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, svdag.nodeData.data());
		DeviceBuffer nanovdbUpperBuf = CreateBuffer(ctx.device, ctx.physDev,
													sizeof(u32) * std::max<size_t>(10, nanovdb.upperData.size()),
													VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nanovdb.upperData.data());
		DeviceBuffer nanovdbLowerBuf = CreateBuffer(ctx.device, ctx.physDev,
													sizeof(u32) * std::max<size_t>(10, nanovdb.lowerData.size()),
													VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nanovdb.lowerData.data());
		DeviceBuffer nanovdbLeafBuf = CreateBuffer(ctx.device, ctx.physDev,
												   sizeof(u32) * std::max<size_t>(3, nanovdb.leafData.size()),
												   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nanovdb.leafData.data());
		DeviceBuffer hitBuf = CreateBuffer(ctx.device, ctx.physDev,
										   sizeof(u32) * kGpuRays,
										   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, nullptr);

		// Run SVDAG-on-64-tree kernel.
		// Push constants: binding 3 = {rootSlotPacked (node index), chunkSideDepth (low8=side, high8=depth)}.
		uint32_t pcSvdag[2] = {
			svdag.rootNodeIndex,
			static_cast<uint32_t>(kChunkSide) | (static_cast<uint32_t>(svdag.depth) << 8)};
		double nsSvdag = BenchKernel(ctx, svdagPipe,
									 {svdagNodeBuf.buf, rayBuf.buf, hitBuf.buf},
									 pcSvdag, sizeof(pcSvdag));
		// Read back hits for verify.
		std::vector<u32> hitsSvdag(kGpuRays);
		void *mapped;
		VK_CHECK(vkMapMemory(ctx.device, hitBuf.mem, 0, sizeof(u32) * kGpuRays, 0, &mapped));
		std::memcpy(hitsSvdag.data(), mapped, sizeof(u32) * kGpuRays);
		vkUnmapMemory(ctx.device, hitBuf.mem);
		uint32_t mismSvdag = VerifyHits(hitsSvdag, scene);
		GpuResult rs;
		rs.scene = scene.name;
		rs.kernel = "svdag64";
		rs.meanMs = nsSvdag / 1e6;
		rs.mraysPerSec = static_cast<double>(kGpuRays) / (nsSvdag / 1e9) / 1e6;
		rs.verifyMismatches = mismSvdag;
		rs.gpuBytes = svdagNodeBuf.size;
		gpuResults.push_back(rs);

		// Run NanoVDB-aligned kernel.
		uint32_t pcNanovdb = 0; // unused for NanoVDB kernel
		double nsNanovdb = BenchKernel(ctx, nanovdbPipe,
									   {nanovdbUpperBuf.buf, nanovdbLowerBuf.buf, nanovdbLeafBuf.buf,
										rayBuf.buf, hitBuf.buf},
									   &pcNanovdb, sizeof(pcNanovdb));
		std::vector<u32> hitsNanovdb(kGpuRays);
		VK_CHECK(vkMapMemory(ctx.device, hitBuf.mem, 0, sizeof(u32) * kGpuRays, 0, &mapped));
		std::memcpy(hitsNanovdb.data(), mapped, sizeof(u32) * kGpuRays);
		vkUnmapMemory(ctx.device, hitBuf.mem);
		uint32_t mismNanovdb = VerifyHits(hitsNanovdb, scene);
		GpuResult rn;
		rn.scene = scene.name;
		rn.kernel = "nanovdb_aligned";
		rn.meanMs = nsNanovdb / 1e6;
		rn.mraysPerSec = static_cast<double>(kGpuRays) / (nsNanovdb / 1e9) / 1e6;
		rn.verifyMismatches = mismNanovdb;
		rn.gpuBytes = nanovdbUpperBuf.size + nanovdbLowerBuf.size + nanovdbLeafBuf.size;
		gpuResults.push_back(rn);

		// Cleanup per scene.
		DestroyBuffer(ctx.device, svdagNodeBuf);
		DestroyBuffer(ctx.device, nanovdbUpperBuf);
		DestroyBuffer(ctx.device, nanovdbLowerBuf);
		DestroyBuffer(ctx.device, nanovdbLeafBuf);
		DestroyBuffer(ctx.device, hitBuf);

		std::printf("%-18s svdag64: %.3f ms, %.1f Mrays/s, verify mism=%u, bytes=%zu\n",
					scene.name.c_str(), rs.meanMs, rs.mraysPerSec, mismSvdag, rs.gpuBytes);
		std::printf("%-18s nanovdb: %.3f ms, %.1f Mrays/s, verify mism=%u, bytes=%zu\n",
					scene.name.c_str(), rn.meanMs, rn.mraysPerSec, mismNanovdb, rn.gpuBytes);
	}

	// Cleanup.
	DestroyBuffer(ctx.device, rayBuf);
	vkDestroyPipeline(ctx.device, svdagPipe.pipe, nullptr);
	vkDestroyPipelineLayout(ctx.device, svdagPipe.layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.device, svdagPipe.dsLayout, nullptr);
	vkDestroyDescriptorPool(ctx.device, svdagPipe.descPool, nullptr);
	vkDestroyPipeline(ctx.device, nanovdbPipe.pipe, nullptr);
	vkDestroyPipelineLayout(ctx.device, nanovdbPipe.layout, nullptr);
	vkDestroyDescriptorSetLayout(ctx.device, nanovdbPipe.dsLayout, nullptr);
	vkDestroyDescriptorPool(ctx.device, nanovdbPipe.descPool, nullptr);
	DestroyContext(ctx);

	// Write CSV.
	std::filesystem::path outDir =
		std::filesystem::path(__FILE__).parent_path();
	std::ofstream csv(outDir / "results_gpu.csv");
	csv << "kernel,scene,meanMs,mraysPerSec,verifyMismatches,gpuBytes\n";
	for (const auto &r : gpuResults) {
		csv << r.kernel << "," << r.scene << "," << r.meanMs << ","
			<< r.mraysPerSec << "," << r.verifyMismatches << "," << r.gpuBytes << "\n";
	}
	csv.close();

	std::ofstream md(outDir / "GPU_RESULTS.md");
	md << "# Results — GPU-side: SVDAG-on-64-tree vs NanoVDB-aligned (chunkSize=8, RTX 3060 Ti)\n\n";
	md << "GPU: " << ctx.props.deviceName << " (Vulkan "
	   << VK_VERSION_MAJOR(ctx.props.apiVersion) << "."
	   << VK_VERSION_MINOR(ctx.props.apiVersion) << ").\n";
	md << "Rays per dispatch: " << kGpuRays << ". Workgroup size 64 (= 2x subgroupSize=32).\n";
	md << "Warm-up=10, Measure=50 dispatches per scene. Median ns reported.\n";
	md << "Mrays/s = rays / (ns / 1e9) / 1e6.\n\n";
	md << "| Kernel | Scene | Mean ms | Mrays/s | Verify mism | GPU bytes |\n";
	md << "|:-------|:------|--------:|--------:|------------:|----------:|\n";
	for (const auto &r : gpuResults) {
		md << "| " << r.kernel << " | " << r.scene << " | "
		   << r.meanMs << " | " << r.mraysPerSec << " | "
		   << r.verifyMismatches << " | " << r.gpuBytes << " |\n";
	}
	md.close();

	std::printf("\nWrote results_gpu.csv and GPU_RESULTS.md\n");
	return 0;
}
