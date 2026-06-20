// Cache-oblivious layout benchmark for Sparse64Tree-like chunk storage.
//
// Standalone C++26 prototype for `docs/experiments/experiments/2026-06-20-cache-oblivious-chunk-tree/`.
// No Vulkan, no ProjectV dependencies. Reuses only std containers + chrono.
//
// Compares two layouts of the same node pool:
//   - baseline: insertion order (mirrors `Sparse64Tree::SetCellRecursive`
//     allocate-then-fill semantics — depth-first descent allocates top of
//     path before bottom, so per-step accesses scatter through `nodes_[]`).
//   - morton:   post-construction reorder by 3D Morton (Z-order) curve over
//     subtree spatial centers. Touches only `nodes_[]` ordering, not structure.
//
// Measurement: random-walk voxel traversal over a synthetic VoxelLab-like scene
// (kChunkGrid^3 chunks x kChunkSide^3 voxels, ~30% non-air fill rate).
// Each "step" walks the tree root -> mid -> leaf (3 node accesses minimum).
//
// Output: CSV per (layout, cold/warm) row, mean/median/p95/p99/std in ns/step.
//
// Reproducibility: scene_seed and walk_seed are explicit arguments; default
// values give a stable baseline.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Geometry / layout constants (mirror `Sparse64Tree.hpp`).
// ---------------------------------------------------------------------------

inline constexpr std::uint32_t kLeafFlag = 0x80000000u;
inline constexpr std::uint32_t kHomogeneousFlag = 0x40000000u;
inline constexpr std::uint32_t kNodeIndexMask = 0x3FFFFFFFu;
inline constexpr std::uint32_t kMaterialMask = 0xFFu;

inline constexpr int kBitsPerAxis = 2;
inline constexpr int kNodeSide = 1 << kBitsPerAxis;				 // 4
inline constexpr int kChildrenPerNode = 1 << (3 * kBitsPerAxis); // 64

struct Node {
	std::uint64_t fillMask = 0;
	std::array<std::uint32_t, kChildrenPerNode> slots{};
	std::uint64_t structuralHash = 0;
	std::uint32_t refCount = 0;
};

static_assert(sizeof(Node) == 280, "Node size must match Sparse64Tree::Node layout");

inline constexpr std::uint32_t MakeLeaf(std::uint8_t material) noexcept
{
	return kLeafFlag | material;
}

inline constexpr bool IsLeaf(std::uint32_t slot) noexcept
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == kLeafFlag;
}

inline constexpr bool IsHomogeneous(std::uint32_t slot) noexcept
{
	return (slot & (kLeafFlag | kHomogeneousFlag)) == (kLeafFlag | kHomogeneousFlag);
}

inline constexpr std::uint32_t NodeIndex(std::uint32_t slot) noexcept
{
	return slot & kNodeIndexMask;
}

// ---------------------------------------------------------------------------
// Morton (Z-order) 3D encode.
// ---------------------------------------------------------------------------

inline std::uint64_t MortonEncode3D(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
{
	std::uint64_t result = 0;
	for (int i = 0; i < 21; ++i) {
		result |= (static_cast<std::uint64_t>(x & (1u << i)) << (2 * i)) | (static_cast<std::uint64_t>(y & (1u << i)) << (2 * i + 1)) | (static_cast<std::uint64_t>(z & (1u << i)) << (2 * i + 2));
	}
	return result;
}

// ---------------------------------------------------------------------------
// Scene constants.
// ---------------------------------------------------------------------------

inline constexpr int kChunkGrid = 24;					   // 24^3 = 13824 chunks (working set > L3)
inline constexpr int kChunkSide = 8;					   // 8^3 voxels per chunk
inline constexpr int kTotalSide = kChunkGrid * kChunkSide; // 128 voxels per axis

struct Chunk {
	std::uint32_t rootSlot = MakeLeaf(0);
	std::uint32_t spatialCenterMorton = 0; // precomputed for chunk-level reorder
};

struct Scene {
	std::vector<Node> nodes;
	std::vector<Chunk> chunks;
	// Sidecar: spatial center (world voxel coords) of each node by oldIdx,
	// computed during construction. Used for Morton reorder.
	std::vector<std::array<int, 3>> nodeCenters;
};

// ---------------------------------------------------------------------------
// Set cell: promotes leaf chunks to internal on demand. No collapse-to-
// homogeneous (would create orphan nodes that complicate layout comparison).
// Spatial center tracked for each new node.
// ---------------------------------------------------------------------------

std::uint32_t AllocNode(Scene &scene, std::array<int, 3> center)
{
	Node n{};
	n.refCount = 1;
	const std::uint32_t idx = static_cast<std::uint32_t>(scene.nodes.size());
	scene.nodes.push_back(n);
	scene.nodeCenters.push_back(center);
	return idx;
}

void SetCell(Scene &scene, int cx, int cy, int cz, int vx, int vy, int vz, std::uint8_t material)
{
	const std::size_t chunkIdx = static_cast<std::size_t>(cx) + static_cast<std::size_t>(kChunkGrid) * (static_cast<std::size_t>(cy) + static_cast<std::size_t>(kChunkGrid) * static_cast<std::size_t>(cz));
	auto &chunk = scene.chunks[chunkIdx];

	// Promote chunk root if needed.
	if (IsLeaf(chunk.rootSlot)) {
		const std::uint8_t oldMat = static_cast<std::uint8_t>(chunk.rootSlot & kMaterialMask);
		std::array<int, 3> center = {cx * kChunkSide + kChunkSide / 2,
									 cy * kChunkSide + kChunkSide / 2,
									 cz * kChunkSide + kChunkSide / 2};
		const std::uint32_t newRoot = AllocNode(scene, center);
		for (int i = 0; i < kChildrenPerNode; ++i) {
			scene.nodes[newRoot].slots[i] = MakeLeaf(oldMat);
		}
		chunk.rootSlot = newRoot;
	}

	const int subX = (vx >> kBitsPerAxis) & (kNodeSide - 1);
	const int subY = (vy >> kBitsPerAxis) & (kNodeSide - 1);
	const int subZ = (vz >> kBitsPerAxis) & (kNodeSide - 1);
	const int childIdx = subX + kNodeSide * (subY + kNodeSide * subZ);

	Node &root = scene.nodes[chunk.rootSlot];
	std::uint32_t midSlot = root.slots[childIdx];

	if (IsLeaf(midSlot)) {
		const std::uint8_t oldMat = static_cast<std::uint8_t>(midSlot & kMaterialMask);
		const int midX = cx * kChunkSide + (subX << kBitsPerAxis) + kNodeSide / 2;
		const int midY = cy * kChunkSide + (subY << kBitsPerAxis) + kNodeSide / 2;
		const int midZ = cz * kChunkSide + (subZ << kBitsPerAxis) + kNodeSide / 2;
		const std::uint32_t newMid = AllocNode(scene, {midX, midY, midZ});
		for (int i = 0; i < kChildrenPerNode; ++i) {
			scene.nodes[newMid].slots[i] = MakeLeaf(oldMat);
		}
		midSlot = newMid;
		root.slots[childIdx] = midSlot;
		root.fillMask |= (1ull << childIdx);
	}

	const int leafX = vx & (kNodeSide - 1);
	const int leafY = vy & (kNodeSide - 1);
	const int leafZ = vz & (kNodeSide - 1);
	const int leafIdx = leafX + kNodeSide * (leafY + kNodeSide * leafZ);

	Node &mid = scene.nodes[NodeIndex(midSlot)];
	mid.slots[leafIdx] = MakeLeaf(material);
	mid.fillMask |= (1ull << leafIdx);
}

Scene BuildSyntheticScene(std::uint32_t seed)
{
	Scene s;
	s.chunks.resize(static_cast<std::size_t>(kChunkGrid) * kChunkGrid * kChunkGrid);
	// Reserve: worst case = 1 root + 64 mid per chunk = 65 * chunk_count.
	const std::size_t maxNodes = static_cast<std::size_t>(kChunkGrid) * kChunkGrid * kChunkGrid * 65;
	s.nodes.reserve(maxNodes);
	s.nodeCenters.reserve(maxNodes);

	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> materialDist(1, 4);
	std::uniform_real_distribution<float> fillDist(0.0f, 1.0f);

	for (int cz = 0; cz < kChunkGrid; ++cz) {
		for (int cy = 0; cy < kChunkGrid; ++cy) {
			for (int cx = 0; cx < kChunkGrid; ++cx) {
				for (int vz = 0; vz < kChunkSide; ++vz) {
					for (int vy = 0; vy < kChunkSide; ++vy) {
						for (int vx = 0; vx < kChunkSide; ++vx) {
							if (fillDist(rng) > 0.30f)
								continue;
							const std::uint8_t m = static_cast<std::uint8_t>(materialDist(rng));
							SetCell(s, cx, cy, cz, vx, vy, vz, m);
						}
					}
				}
			}
		}
	}

	// Compute chunk-level Morton centers.
	for (int cz = 0; cz < kChunkGrid; ++cz) {
		for (int cy = 0; cy < kChunkGrid; ++cy) {
			for (int cx = 0; cx < kChunkGrid; ++cx) {
				const std::size_t idx = static_cast<std::size_t>(cx) + static_cast<std::size_t>(kChunkGrid) * (static_cast<std::size_t>(cy) + static_cast<std::size_t>(kChunkGrid) * static_cast<std::size_t>(cz));
				s.chunks[idx].spatialCenterMorton = static_cast<std::uint32_t>(MortonEncode3D(
					static_cast<std::uint32_t>(cx),
					static_cast<std::uint32_t>(cy),
					static_cast<std::uint32_t>(cz)));
			}
		}
	}
	return s;
}

// ---------------------------------------------------------------------------
// Morton reorder: for each node, compute its spatial center Morton; sort
// nodes; remap all internal slots and chunk roots to new indices.
// ---------------------------------------------------------------------------

Scene MortonReorder(const Scene &src)
{
	Scene dst;
	dst.chunks = src.chunks;

	struct Meta {
		std::uint32_t oldIdx;
		std::uint64_t morton;
	};
	std::vector<Meta> meta;
	meta.reserve(src.nodes.size());

	for (std::uint32_t i = 0; i < src.nodes.size(); ++i) {
		const auto &c = src.nodeCenters[i];
		meta.push_back({i, MortonEncode3D(static_cast<std::uint32_t>(c[0]),
										  static_cast<std::uint32_t>(c[1]),
										  static_cast<std::uint32_t>(c[2]))});
	}

	std::sort(meta.begin(), meta.end(), [](const Meta &a, const Meta &b) {
		return a.morton < b.morton;
	});

	// Two-pass: first build full remap table, then rebuild nodes with remapped slots.
	std::unordered_map<std::uint32_t, std::uint32_t> remap;
	remap.reserve(meta.size());
	for (std::size_t j = 0; j < meta.size(); ++j) {
		remap[meta[j].oldIdx] = static_cast<std::uint32_t>(j);
	}

	dst.nodes.reserve(meta.size());

	for (const auto &m : meta) {
		Node copy = src.nodes[m.oldIdx];
		for (std::uint32_t &slot : copy.slots) {
			if (!IsLeaf(slot) && !IsHomogeneous(slot)) {
				const std::uint32_t oldChild = NodeIndex(slot);
				const auto it = remap.find(oldChild);
				if (it != remap.end()) {
					slot = it->second;
				} else {
					// Should not happen if all referenced nodes are reachable.
					slot = 0; // fallback; will likely return air
				}
			}
		}
		dst.nodes.push_back(copy);
		dst.nodeCenters.push_back(src.nodeCenters[m.oldIdx]);
	}
	for (auto &c : dst.chunks) {
		if (!IsLeaf(c.rootSlot) && !IsHomogeneous(c.rootSlot)) {
			const std::uint32_t oldRoot = NodeIndex(c.rootSlot);
			const auto it = remap.find(oldRoot);
			if (it != remap.end()) {
				c.rootSlot = it->second;
			}
		}
	}
	return dst;
}

// ---------------------------------------------------------------------------
// Traversal.
// ---------------------------------------------------------------------------

inline std::uint8_t GetCell(const Scene &scene, int cx, int cy, int cz, int vx, int vy, int vz)
{
	const std::size_t chunkIdx = static_cast<std::size_t>(cx) + static_cast<std::size_t>(kChunkGrid) * (static_cast<std::size_t>(cy) + static_cast<std::size_t>(kChunkGrid) * static_cast<std::size_t>(cz));
	const auto &chunk = scene.chunks[chunkIdx];
	std::uint32_t slot = chunk.rootSlot;
	if (IsLeaf(slot) || IsHomogeneous(slot)) {
		return static_cast<std::uint8_t>(slot & kMaterialMask);
	}
	const int subX = (vx >> kBitsPerAxis) & (kNodeSide - 1);
	const int subY = (vy >> kBitsPerAxis) & (kNodeSide - 1);
	const int subZ = (vz >> kBitsPerAxis) & (kNodeSide - 1);
	const int childIdx = subX + kNodeSide * (subY + kNodeSide * subZ);

	const Node &root = scene.nodes[NodeIndex(slot)];
	if (((root.fillMask >> childIdx) & 1ull) == 0ull)
		return 0;
	slot = root.slots[childIdx];
	if (IsLeaf(slot) || IsHomogeneous(slot)) {
		return static_cast<std::uint8_t>(slot & kMaterialMask);
	}
	const Node &mid = scene.nodes[NodeIndex(slot)];
	const int leafX = vx & (kNodeSide - 1);
	const int leafY = vy & (kNodeSide - 1);
	const int leafZ = vz & (kNodeSide - 1);
	const int leafIdx = leafX + kNodeSide * (leafY + kNodeSide * leafZ);
	if (((mid.fillMask >> leafIdx) & 1ull) == 0ull)
		return 0;
	slot = mid.slots[leafIdx];
	return static_cast<std::uint8_t>(slot & kMaterialMask);
}

struct Stats {
	double mean, median, p50, p95, p99, p99_9, stddev, min, max;
	std::size_t n;
};

Stats ComputeStats(std::vector<double> samples)
{
	Stats s{};
	s.n = samples.size();
	std::sort(samples.begin(), samples.end());
	double sum = 0.0;
	for (double v : samples)
		sum += v;
	s.mean = sum / static_cast<double>(samples.size());
	s.median = samples[samples.size() / 2];
	s.p50 = samples[samples.size() / 2];
	s.p95 = samples[static_cast<std::size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<std::size_t>(samples.size() * 0.99)];
	s.p99_9 = samples[static_cast<std::size_t>(samples.size() * 0.999)];
	s.min = samples.front();
	s.max = samples.back();
	double var = 0.0;
	for (double v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
	return s;
}

std::vector<double> RunRandomWalk(const Scene &scene, std::uint32_t seed, std::size_t steps,
								  std::size_t warmup, bool coldCache)
{
	std::mt19937 rng(seed);
	std::uniform_int_distribution<int> chunkDist(0, kChunkGrid - 1);
	std::uniform_int_distribution<int> voxelDist(0, kChunkSide - 1);
	std::uniform_int_distribution<int> deltaDist(-1, 1);

	int cx = chunkDist(rng), cy = chunkDist(rng), cz = chunkDist(rng);
	int vx = voxelDist(rng), vy = voxelDist(rng), vz = voxelDist(rng);

	auto step = [&]() {
		const std::uint8_t m = GetCell(scene, cx, cy, cz, vx, vy, vz);
		cx += deltaDist(rng);
		cy += deltaDist(rng);
		cz += deltaDist(rng);
		vx += deltaDist(rng);
		vy += deltaDist(rng);
		vz += deltaDist(rng);
		cx = (cx + kChunkGrid) % kChunkGrid;
		cy = (cy + kChunkGrid) % kChunkGrid;
		cz = (cz + kChunkGrid) % kChunkGrid;
		vx = (vx + kChunkSide) % kChunkSide;
		vy = (vy + kChunkSide) % kChunkSide;
		vz = (vz + kChunkSide) % kChunkSide;
		return m;
	};

	std::uint64_t sink = 0;
	for (std::size_t i = 0; i < warmup; ++i)
		sink += step();
	if (coldCache) {
		std::vector<std::uint8_t> evict(8 * 1024 * 1024, 0);
		for (auto &e : evict)
			sink += e;
	}

	std::vector<double> latencies;
	latencies.reserve(steps);
	for (std::size_t i = 0; i < steps; ++i) {
		const auto t0 = std::chrono::steady_clock::now();
		sink += step();
		const auto t1 = std::chrono::steady_clock::now();
		const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
		latencies.push_back(static_cast<double>(delta));
	}
	if (sink == 0xDEADBEEF)
		std::printf(""); // prevent dead-code elim
	return latencies;
}

void PrintStats(const std::string &label, const Stats &s)
{
	std::printf("%-40s  mean=%9.2f  p50=%9.2f  p95=%9.2f  p99=%9.2f  p99.9=%9.2f  std=%9.2f  min=%9.0f  max=%9.0f  N=%zu\n",
				label.c_str(), s.mean, s.median, s.p95, s.p99, s.p99_9, s.stddev, s.min, s.max, s.n);
}

void WriteCsv(const std::string &path, const std::string &layout, bool cold, const Stats &s)
{
	std::ofstream out(path, std::ios::app);
	out << layout << "," << (cold ? "cold" : "warm") << ","
		<< s.mean << "," << s.median << "," << s.p50 << ","
		<< s.p95 << "," << s.p99 << "," << s.p99_9 << ","
		<< s.stddev << "," << s.min << "," << s.max << "," << s.n << "\n";
}

} // namespace

int main(int argc, char **argv)
{
	std::string csvPath = "/tmp/cobl_results.csv";
	std::string layout = "all";
	std::size_t steps = 1000;
	std::size_t warmup = 1000;
	std::uint32_t sceneSeed = 42;

	for (int i = 1; i < argc; ++i) {
		const std::string a = argv[i];
		if (a == "--output")
			csvPath = argv[++i];
		else if (a == "--layout")
			layout = argv[++i];
		else if (a == "--iterations")
			steps = std::stoull(argv[++i]);
		else if (a == "--warmup")
			warmup = std::stoull(argv[++i]);
		else if (a == "--seed")
			sceneSeed = static_cast<std::uint32_t>(std::stoul(argv[++i]));
	}

	std::printf("Building synthetic scene (%d^3 chunks x %d^3 voxels, 30%% fill, seed=%u)...\n",
				kChunkGrid, kChunkSide, sceneSeed);
	auto scene = BuildSyntheticScene(sceneSeed);
	std::printf("  baseline nodes: %zu, total bytes: %zu (~%.1f MiB)\n",
				scene.nodes.size(), scene.nodes.size() * sizeof(Node),
				static_cast<double>(scene.nodes.size() * sizeof(Node)) / (1024.0 * 1024.0));

	auto morton = MortonReorder(scene);
	std::printf("  morton-reorder nodes: %zu, ~%.1f MiB\n",
				morton.nodes.size(),
				static_cast<double>(morton.nodes.size() * sizeof(Node)) / (1024.0 * 1024.0));

	std::printf("Host: L1d=32 KiB, L2=512 KiB, L3=32 MiB; node=280 B (~5 cache lines).\n");
	std::printf("Governor=powersave (per-run note in RESULTS.md).\n");

	std::ofstream csv(csvPath);
	csv << "layout,cache,mean,median,p50,p95,p99,p99.9,stddev,min,max,n\n";
	csv.close();

	const std::uint32_t walkSeeds[] = {1, 2, 3};

	for (const std::uint32_t ws : walkSeeds) {
		std::printf("\n--- walk seed %u ---\n", ws);

		if (layout == "all" || layout == "baseline") {
			auto warm = RunRandomWalk(scene, ws, steps, warmup, false);
			Stats sW = ComputeStats(warm);
			PrintStats("baseline warm", sW);
			WriteCsv(csvPath, "baseline", false, sW);

			auto cold = RunRandomWalk(scene, ws, steps, warmup, true);
			Stats sC = ComputeStats(cold);
			PrintStats("baseline cold", sC);
			WriteCsv(csvPath, "baseline", true, sC);
		}

		if (layout == "all" || layout == "morton") {
			auto warm = RunRandomWalk(morton, ws, steps, warmup, false);
			Stats sW = ComputeStats(warm);
			PrintStats("morton warm", sW);
			WriteCsv(csvPath, "morton", false, sW);

			auto cold = RunRandomWalk(morton, ws, steps, warmup, true);
			Stats sC = ComputeStats(cold);
			PrintStats("morton cold", sC);
			WriteCsv(csvPath, "morton", true, sC);
		}
	}
	std::printf("\nResults written to %s\n", csvPath.c_str());
	return 0;
}
