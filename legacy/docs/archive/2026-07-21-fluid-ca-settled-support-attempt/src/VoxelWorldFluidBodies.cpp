#include "voxel/VoxelWorldFluidInternal.hpp"

#include "voxel/VoxelWorld.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <queue>
#include <tuple>
#include <vector>

namespace {

constexpr int kFluidChainBudgetPerBody = 8; // K: max chains per body per tick
constexpr int kFluidChainMaxPath = 64;		// L: max BFS path length per chain

constexpr int kNeighbor6[6][3] = {
	// fixed order: down, lateral, up last (determinism)
	{0, -1, 0},
	{1, 0, 0},
	{-1, 0, 0},
	{0, 0, 1},
	{0, 0, -1},
	{0, 1, 0},
};

constexpr uint8_t kAirMat = static_cast<uint8_t>(VoxelMaterial::Air);
constexpr uint8_t kFluidMat = static_cast<uint8_t>(VoxelMaterial::Fluid);

bool IsSolidMaterial(const uint8_t material)
{
	return material != kAirMat && material != kFluidMat; // Glass, FloorWhite, FloorGray, …
}

} // namespace

// Per-BASIN hydrostatics (operator contract: one uniform disc per unobstructed basin).
// Bodies sharing a reachable basin (their floods touch) are unioned; each basin gets
// ONE target disc anchored at its largest body. Isolated fragments (strays) get a
// D-gradient toward the disc and crawl back to it — stickiness is granted only to
// cells of the anchor body. Chains: sources only from a body's current top layer,
// guard (Σy, ΣD) strictly decreases → oscillation impossible by construction.
uint32_t ProcessFluidBodyChains(const int width, const int height, const int depth, std::vector<uint8_t> &next)
{
	const auto index = [width, height](const int lx, const int ly, const int lz) -> size_t {
		return static_cast<size_t>(lx) + static_cast<size_t>(ly) * static_cast<size_t>(width) +
			   static_cast<size_t>(lz) * static_cast<size_t>(width) * static_cast<size_t>(height);
	};
	const auto isInsideLocal = [width, height, depth](const int lx, const int ly, const int lz) -> bool {
		return lx >= 0 && ly >= 0 && lz >= 0 && lx < width && ly < height && lz < depth;
	};
	const auto decode = [width, height](const int32_t flat, int &lx, int &ly, int &lz) {
		lx = flat % width;
		ly = (flat / width) % height;
		lz = flat / (width * height);
	};

	const size_t totalCells = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth);
	uint32_t moved = 0u;

	// 6-connected bodies (scan order)
	std::vector<int32_t> bodyId(totalCells, -1);
	std::vector<std::vector<int32_t>> bodyCells;
	std::vector<int32_t> floodStack;
	for (int y = 0; y < height; ++y) {
		for (int z = 0; z < depth; ++z) {
			for (int x = 0; x < width; ++x) {
				const int32_t startFlat = static_cast<int32_t>(index(x, y, z));
				if (next[startFlat] != kFluidMat || bodyId[startFlat] != -1) {
					continue;
				}
				const int32_t id = static_cast<int32_t>(bodyCells.size());
				bodyCells.emplace_back();
				bodyId[startFlat] = id;
				floodStack.push_back(startFlat);
				while (!floodStack.empty()) {
					const int32_t cur = floodStack.back();
					floodStack.pop_back();
					bodyCells[id].push_back(cur);
					int cx = 0;
					int cy = 0;
					int cz = 0;
					decode(cur, cx, cy, cz);
					for (const auto &nb : kNeighbor6) {
						const int nx = cx + nb[0];
						const int ny = cy + nb[1];
						const int nz = cz + nb[2];
						if (!isInsideLocal(nx, ny, nz)) {
							continue;
						}
						const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
						if (next[nFlat] == kFluidMat && bodyId[nFlat] == -1) {
							bodyId[nFlat] = id;
							floodStack.push_back(nFlat);
						}
					}
				}
			}
		}
	}
	const auto columnKey = [width, height](const int32_t flat) -> int32_t { // (x,z) column key for the 1-chain-per-column rule
		return (flat % width) + (flat / (width * height)) * width;
	};
	const auto scanKey = [width, height](const int32_t flat) -> int64_t { // (y,z,x) scan order key
		const int64_t lx = flat % width;
		const int64_t ly = (flat / width) % height;
		const int64_t lz = flat / (width * height);
		return (ly << 40) | (lz << 20) | lx;
	};
	for (auto &cells : bodyCells) {
		std::sort(cells.begin(), cells.end(), [&](const int32_t a, const int32_t b) { return scanKey(a) < scanKey(b); });
	}
	const int bodyCount = static_cast<int>(bodyCells.size());
	if (bodyCount == 0) {
		return 0u;
	}

	// Scratch (generation-tagged — no O(world) clears between floods)
	std::vector<int32_t> targetMark(totalCells, -1); // == gen → in T
	std::vector<int32_t> pushMark(totalCells, -1);	 // == gen → already a candidate
	std::vector<int32_t> reachMark(totalCells, -1);	 // == gen → reachable Air node (flood)
	std::vector<int32_t> reachDist(totalCells, -1);	 // flood BFS depth
	std::vector<int32_t> distMark(totalCells, -1);	 // == gen → distVal valid
	std::vector<int32_t> distVal(totalCells, -1);	 // distance to nearest T cell
	std::vector<int32_t> groundMark(totalCells, -1); // == gen → supported column down to Solid / world bottom
	std::vector<int32_t> columnMark(totalCells, -1); // == basinGen → column (x,z) already consumed a chain this tick
	std::vector<int32_t> bfsSeen(totalCells, -1);	 // == bfsGen → visited in current chain BFS
	std::vector<int32_t> bfsParent(totalCells, -1);
	std::vector<int32_t> bfsDepth(totalCells, -1);
	std::vector<int32_t> bfsQueue;
	std::vector<int32_t> tCells;
	int32_t targetGen = 0;
	int32_t bfsGen = 0;

	struct Cand { // target candidates: (y, stickiness, rad) + octant round-robin — uniform disc by radius, not path length
		int32_t y;
		int32_t stick;
		int32_t rad;
		int32_t z;
		int32_t x;
		int32_t flat;
	};
	struct CandGreater {
		int rotation = 0; // 0..3 — cycles the preferred quadrant each tick (no fixed-direction bias)
		bool operator()(const Cand &a, const Cand &b) const
		{
			const auto key = [this](const Cand &c) {
				switch (rotation & 3) {
				case 1:
					return std::tuple{c.y, c.stick, c.rad, c.x, -c.z};
				case 2:
					return std::tuple{c.y, c.stick, c.rad, -c.z, -c.x};
				case 3:
					return std::tuple{c.y, c.stick, c.rad, -c.x, c.z};
				default:
					return std::tuple{c.y, c.stick, c.rad, c.z, c.x};
				}
			};
			return key(a) > key(b);
		}
	};

	// Union-find over bodies (same basin ⇔ their floods touch each other's fluid)
	std::vector<int32_t> parent(static_cast<size_t>(bodyCount));
	for (int i = 0; i < bodyCount; ++i) {
		parent[static_cast<size_t>(i)] = i;
	}
	const auto findRoot = [&parent](int32_t a) {
		int32_t r = a;
		while (parent[static_cast<size_t>(r)] != r) {
			r = parent[static_cast<size_t>(r)];
		}
		while (parent[static_cast<size_t>(a)] != r) {
			const int32_t nextUp = parent[static_cast<size_t>(a)];
			parent[static_cast<size_t>(a)] = r;
			a = nextUp;
		}
		return r;
	};
	const auto unite = [&parent, &findRoot](const int32_t a, const int32_t b) {
		const int32_t ra = findRoot(a);
		const int32_t rb = findRoot(b);
		if (ra != rb) { // deterministic: lower index wins
			parent[static_cast<size_t>(std::max(ra, rb))] = std::min(ra, rb);
		}
	};

	const auto fillable = [&](const int32_t flat, const int32_t gen) -> bool { // support below: world bottom, Solid, Fluid, or T
		int x = 0;
		int y = 0;
		int z = 0;
		decode(flat, x, y, z);
		if (y == 0) {
			return true;
		}
		const int32_t belowFlat = flat - width;
		if (targetMark[belowFlat] == gen) {
			return true;
		}
		const uint8_t below = next[belowFlat];
		return below == kFluidMat || IsSolidMaterial(below);
	};

	struct CandList { // fillable flooded candidates of one flood: flat + dist
		std::vector<std::pair<int32_t, int32_t>> items;
	};
	// Flood from a set of seed bodies; collects fillable candidates and unites touching bodies
	const auto basinFlood = [&](const std::vector<int32_t> &seedBodies, const bool doUnite, CandList &out) {
		for (const int32_t seed : seedBodies) {
			for (const int32_t cell : bodyCells[static_cast<size_t>(seed)]) {
				int x = 0;
				int y = 0;
				int z = 0;
				decode(cell, x, y, z);
				for (int d = 0; d < 5; ++d) { // down + 4 lateral
					const int nx = x + kNeighbor6[d][0];
					const int ny = y + kNeighbor6[d][1];
					const int nz = z + kNeighbor6[d][2];
					if (!isInsideLocal(nx, ny, nz)) {
						continue;
					}
					const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
					if (next[nFlat] == kAirMat && reachMark[nFlat] != targetGen) {
						reachMark[nFlat] = targetGen;
						reachDist[nFlat] = 1;
						bfsQueue.push_back(nFlat);
						if (fillable(nFlat, targetGen)) {
							out.items.emplace_back(nFlat, 1);
						}
					}
				}
			}
		}
		size_t reachHead = 0;
		while (reachHead < bfsQueue.size()) {
			const int32_t cur = bfsQueue[reachHead++];
			int cx = 0;
			int cy = 0;
			int cz = 0;
			decode(cur, cx, cy, cz);
			for (const auto &nb : kNeighbor6) {
				const int nx = cx + nb[0];
				const int ny = cy + nb[1];
				const int nz = cz + nb[2];
				if (!isInsideLocal(nx, ny, nz) || ny > cy) { // down + lateral; water never climbs
					continue;
				}
				const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
				if (next[nFlat] == kFluidMat && doUnite) {
					const int32_t other = bodyId[nFlat];
					if (other >= 0) {
						bool member = false;
						for (const int32_t seed : seedBodies) {
							member = member || other == seed;
						}
						if (!member) {
							unite(seedBodies.front(), other);
						}
					}
					continue;
				}
				if (next[nFlat] != kAirMat || reachMark[nFlat] == targetGen) {
					continue;
				}
				reachMark[nFlat] = targetGen;
				reachDist[nFlat] = reachDist[cur] + 1;
				bfsQueue.push_back(nFlat);
				if (fillable(nFlat, targetGen)) {
					out.items.emplace_back(nFlat, reachDist[nFlat]);
				}
			}
		}
	};

	// Phase A: per-body floods (union detection + per-body candidate lists)
	std::vector<CandList> bodyCands(static_cast<size_t>(bodyCount));
	std::vector<int32_t> bodyFloodGen(static_cast<size_t>(bodyCount), -1);
	for (int32_t b = 0; b < bodyCount; ++b) {
		++targetGen;
		bodyFloodGen[static_cast<size_t>(b)] = targetGen;
		basinFlood(std::vector<int32_t>{b}, true, bodyCands[static_cast<size_t>(b)]);
	}

	// Phase B: basins (groups of body indices, ordered by lowest member)
	std::vector<std::vector<int32_t>> basinMembers(static_cast<size_t>(bodyCount));
	for (int32_t b = 0; b < bodyCount; ++b) {
		basinMembers[static_cast<size_t>(findRoot(b))].push_back(b);
	}
	std::vector<std::vector<int32_t>> basins;
	for (int32_t b = 0; b < bodyCount; ++b) {
		if (!basinMembers[static_cast<size_t>(b)].empty()) {
			basins.push_back(basinMembers[static_cast<size_t>(b)]);
		}
	}

	const auto lateralBodyNeighbours = [&](const int32_t flat, const int32_t id) -> int {
		int x = 0;
		int y = 0;
		int z = 0;
		decode(flat, x, y, z);
		int count = 0;
		for (int d = 1; d <= 4; ++d) {
			const int nx = x + kNeighbor6[d][0];
			const int nz = z + kNeighbor6[d][2];
			if (isInsideLocal(nx, y, nz) && bodyId[index(nx, y, nz)] == id) {
				++count;
			}
		}
		return count;
	};

	// Phase C: one hydrostatic disc per basin
	for (const auto &members : basins) {
		int32_t anchor = members.front(); // largest body anchors the disc (stickiness only for its cells)
		for (const int32_t m : members) {
			if (bodyCells[static_cast<size_t>(m)].size() > bodyCells[static_cast<size_t>(anchor)].size()) {
				anchor = m;
			}
		}
		int64_t sumX = 0; // anchor centroid — octant anchor for the balanced take
		int64_t sumZ = 0;
		for (const int32_t cell : bodyCells[static_cast<size_t>(anchor)]) {
			int x = 0;
			int y = 0;
			int z = 0;
			decode(cell, x, y, z);
			sumX += x;
			sumZ += z;
		}
		const int32_t centroidX = static_cast<int32_t>(sumX / static_cast<int64_t>(bodyCells[static_cast<size_t>(anchor)].size()));
		const int32_t centroidZ = static_cast<int32_t>(sumZ / static_cast<int64_t>(bodyCells[static_cast<size_t>(anchor)].size()));
		int32_t volume = 0;
		for (const int32_t m : members) {
			volume += static_cast<int32_t>(bodyCells[static_cast<size_t>(m)].size());
		}

		CandList cands;
		int32_t basinGen = 0;
		if (members.size() == 1) { // reuse the phase-A flood (marks live under its gen)
			basinGen = bodyFloodGen[static_cast<size_t>(members.front())];
			cands = bodyCands[static_cast<size_t>(members.front())];
			targetGen = std::max(targetGen, basinGen);
		} else { // unified basin flood with a fresh gen
			basinGen = ++targetGen;
			basinFlood(members, false, cands);
		}

		// PQ keys: (y, stickiness, rad); stickiness 0 = anchor body cells (settled shapes stay put)
		std::priority_queue<Cand, std::vector<Cand>, CandGreater> pq(CandGreater{static_cast<int>(basinGen & 3)});
		const auto pushCand = [&](const int32_t flat, const int32_t stick) {
			if (pushMark[flat] == basinGen) {
				return;
			}
			pushMark[flat] = basinGen;
			int x = 0;
			int y = 0;
			int z = 0;
			decode(flat, x, y, z);
			const int32_t rad = (x - centroidX) * (x - centroidX) + (z - centroidZ) * (z - centroidZ);
			pq.push(Cand{y, stick, rad, z, x, flat});
		};
		for (const int32_t m : members) {
			const int32_t stick = m == anchor ? 0 : 1; // stickiness belongs to the anchor body only
			for (const int32_t cell : bodyCells[static_cast<size_t>(m)]) {
				pushCand(cell, stick);
			}
		}
		for (const auto &[flat, dist] : cands.items) {
			(void)dist;
			pushCand(flat, 1);
		}

		// Take target T: bottom-up by (y, dist) shells; each shell is taken round-robin over the 8
		// compass octants around the anchor centroid — every unobstructed direction fills evenly
		tCells.clear();
		const auto tryTake = [&](const Cand &cand) -> bool {
			if (targetMark[cand.flat] == basinGen || !fillable(cand.flat, basinGen)) {
				return false;
			}
			targetMark[cand.flat] = basinGen;
			tCells.push_back(cand.flat);
			int x = 0;
			int y = 0;
			int z = 0;
			decode(cand.flat, x, y, z);
			for (const auto &nb : kNeighbor6) {
				const int nx = x + nb[0];
				const int ny = y + nb[1];
				const int nz = z + nb[2];
				if (!isInsideLocal(nx, ny, nz)) {
					continue;
				}
				const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
				if (next[nFlat] != kAirMat || bodyId[nFlat] >= 0 || pushMark[nFlat] == basinGen || reachMark[nFlat] != basinGen) {
					continue;
				}
				if (fillable(nFlat, basinGen)) { // newly supportable via T (stacking on the fill)
					pushCand(nFlat, 1);
				}
			}
			return true;
		};
		const auto octantOf = [&](const Cand &cand) -> int { // 0=E 1=SE 2=S 3=SW 4=W 5=NW 6=N 7=NE
			const int dx = cand.x - centroidX;
			const int dz = cand.z - centroidZ;
			const int adx = std::abs(dx);
			const int adz = std::abs(dz);
			if (adx >= 2 * adz) {
				return dx >= 0 ? 0 : 4;
			}
			if (adz >= 2 * adx) {
				return dz >= 0 ? 2 : 6;
			}
			if (dx >= 0) {
				return dz >= 0 ? 1 : 7;
			}
			return dz >= 0 ? 3 : 5;
		};
		const int startOctant = static_cast<int>(basinGen & 7); // rotate the starting sector per tick
		while (static_cast<int32_t>(tCells.size()) < volume && !pq.empty()) {
			const int groupY = pq.top().y; // pull the whole current (y, stick) group, then share the budget across octants
			const int groupStick = pq.top().stick;
			std::vector<Cand> buckets[8];
			while (!pq.empty() && pq.top().y == groupY && pq.top().stick == groupStick) {
				const Cand cand = pq.top();
				pq.pop();
				buckets[octantOf(cand)].push_back(cand);
			}
			for (auto &bucket : buckets) { // each octant fills by radius from the anchor
				std::sort(bucket.begin(), bucket.end(), [](const Cand &a, const Cand &b) {
					return std::tuple{a.rad, a.z, a.x} < std::tuple{b.rad, b.z, b.x};
				});
			}
			for (size_t round = 0; static_cast<int32_t>(tCells.size()) < volume; ++round) {
				bool any = false;
				for (int o = 0; o < 8 && static_cast<int32_t>(tCells.size()) < volume; ++o) {
					auto &bucket = buckets[(startOctant + o) & 7];
					if (round < bucket.size() && tryTake(bucket[round])) {
						any = true;
					}
				}
				if (!any) {
					break;
				}
			}
		}

		// Distance field D to nearest T cell over member fluid + flooded Air (reverse legal moves)
		bfsQueue.clear();
		for (const int32_t t : tCells) {
			distMark[t] = basinGen;
			distVal[t] = 0;
			bfsQueue.push_back(t);
		}
		size_t head = 0;
		while (head < bfsQueue.size()) {
			const int32_t cur = bfsQueue[head++];
			int cx = 0;
			int cy = 0;
			int cz = 0;
			decode(cur, cx, cy, cz);
			for (const auto &nb : kNeighbor6) {
				const int nx = cx + nb[0];
				const int ny = cy + nb[1];
				const int nz = cz + nb[2];
				if (!isInsideLocal(nx, ny, nz)) {
					continue;
				}
				const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
				if (distMark[nFlat] == basinGen) {
					continue;
				}
				const int32_t owner = bodyId[nFlat];
				const bool isNode = (owner >= 0 && findRoot(owner) == findRoot(members.front())) ||
									(next[nFlat] == kAirMat && reachMark[nFlat] == basinGen);
				if (!isNode) {
					continue;
				}
				bool legal = false;
				if (ny == cy + 1) { // forward move: n falls into cur
					legal = true;
				} else if (ny == cy) { // forward move: n slides into cur — cur supportable or a live fall node
					legal = cy == 0;
					if (!legal) {
						const int32_t belowCur = index(cx, cy - 1, cz);
						legal = next[belowCur] != kAirMat || targetMark[belowCur] == basinGen || distMark[belowCur] == basinGen;
					}
				}
				if (!legal) {
					continue;
				}
				distMark[nFlat] = basinGen;
				distVal[nFlat] = distVal[cur] + 1;
				bfsQueue.push_back(nFlat);
			}
		}

		// Grounded marks per member body (cells sorted y asc)
		for (const int32_t m : members) {
			for (const int32_t cell : bodyCells[static_cast<size_t>(m)]) {
				int x = 0;
				int y = 0;
				int z = 0;
				decode(cell, x, y, z);
				bool grounded = y == 0;
				if (!grounded) {
					const int32_t belowFlat = cell - width;
					const uint8_t below = next[belowFlat];
					grounded = IsSolidMaterial(below) || (below == kFluidMat && groundMark[belowFlat] == basinGen);
				}
				if (grounded) {
					groundMark[cell] = basinGen;
				}
			}
		}

		// ---- Chains per member body ----
		const auto applyChainTo = [&](const int32_t source, const int32_t dest, const int32_t id, std::vector<int32_t> &cells) -> bool {
			++bfsGen;
			bfsQueue.clear();
			bfsSeen[source] = bfsGen;
			bfsParent[source] = source;
			bfsDepth[source] = 0;
			bfsQueue.push_back(source);
			size_t chainHead = 0;
			while (chainHead < bfsQueue.size()) {
				const int32_t cur = bfsQueue[chainHead++];
				if (bfsDepth[cur] >= kFluidChainMaxPath) {
					continue;
				}
				int cx = 0;
				int cy = 0;
				int cz = 0;
				decode(cur, cx, cy, cz);
				for (const auto &nb : kNeighbor6) {
					const int nx = cx + nb[0];
					const int ny = cy + nb[1];
					const int nz = cz + nb[2];
					if (!isInsideLocal(nx, ny, nz)) {
						continue;
					}
					const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
					if (nFlat == dest) { // goal reached — shift along the path: dest ← frontier ← … ← source
						next[dest] = kFluidMat;
						int32_t walk = cur;
						while (walk != source) {
							next[walk] = next[bfsParent[walk]];
							walk = bfsParent[walk];
						}
						next[source] = kAirMat;
						bodyId[dest] = id;
						bodyId[source] = -1;
						// Column settle: same-body fluid directly above the source drops one cell into the vacancy
						// (bottom-first drain — the column's top shrinks, streams stay contiguous)
						int32_t above = source + width;
						while (above < static_cast<int32_t>(totalCells) && next[above] == kFluidMat && bodyId[above] == id) {
							next[above - width] = next[above];
							bodyId[above - width] = id;
							next[above] = kAirMat;
							bodyId[above] = -1;
							above += width;
						}
						const int32_t sourceX = source % width;
						const int32_t sourceZ = source / (width * height);
						for (auto &cell : cells) { // source→dest; settled cells above the source drop by one
							if (cell == source) {
								cell = dest;
							} else if (cell > source && cell < above && cell % width == sourceX && cell / (width * height) == sourceZ) {
								cell -= width;
							}
						}
						std::sort(cells.begin(), cells.end(), [&](const int32_t a, const int32_t b) { return scanKey(a) < scanKey(b); });
						return true;
					}
					if (next[nFlat] == kFluidMat && bodyId[nFlat] == id && groundMark[nFlat] == basinGen && bfsSeen[nFlat] != bfsGen) {
						bfsSeen[nFlat] = bfsGen;
						bfsParent[nFlat] = cur;
						bfsDepth[nFlat] = bfsDepth[cur] + 1;
						bfsQueue.push_back(nFlat);
					}
				}
			}
			return false;
		};

		for (const int32_t m : members) {
			auto &cells = bodyCells[static_cast<size_t>(m)];
			const int32_t id = m;
			// Destination candidates (static this tick): Air neighbors (lateral or below) of the body with finite D
			struct Dest {
				int32_t dist;
				int32_t y;
				int32_t z;
				int32_t x;
				int32_t flat;
				bool exit; // fall-entry: unsupported Air (outflow point of the body)
			};
			std::vector<Dest> dests;
			for (const int32_t cell : cells) {
				int cx = 0;
				int cy = 0;
				int cz = 0;
				decode(cell, cx, cy, cz);
				for (const auto &nb : kNeighbor6) {
					const int nx = cx + nb[0];
					const int ny = cy + nb[1];
					const int nz = cz + nb[2];
					if (ny > cy || !isInsideLocal(nx, ny, nz)) { // destinations never climb
						continue;
					}
					const int32_t nFlat = static_cast<int32_t>(index(nx, ny, nz));
					if (next[nFlat] != kAirMat || bodyId[nFlat] >= 0 || distMark[nFlat] != basinGen) {
						continue;
					}
					const bool isExit = ny > 0 && next[index(nx, ny - 1, nz)] == kAirMat;
					dests.push_back(Dest{distVal[nFlat], ny, nz, nx, nFlat, isExit});
				}
			}
			const int destRotation = static_cast<int>(basinGen & 3); // same compass rotation as the target fill
			std::sort(dests.begin(), dests.end(), [destRotation](const Dest &a, const Dest &b) {
				const auto key = [destRotation](const Dest &d) {
					switch (destRotation) {
					case 1:
						return std::tuple{d.dist, d.y, d.x, -d.z};
					case 2:
						return std::tuple{d.dist, d.y, -d.z, -d.x};
					case 3:
						return std::tuple{d.dist, d.y, -d.x, d.z};
					default:
						return std::tuple{d.dist, d.y, d.z, d.x};
					}
				};
				return key(a) < key(b);
			});
			int budget = kFluidChainBudgetPerBody;
			int chainOrdinal = 0;
			// Excess bottom on settled support: grounded, ∉T, below Solid or Fluid∈T (bottom-first drain).
			const auto refreshSources = [&](std::vector<int32_t> &sources, int &maxSourceY) {
				sources.clear();
				maxSourceY = -1;
				for (const int32_t cell : cells) {
					if (groundMark[cell] != basinGen || targetMark[cell] == basinGen) {
						continue; // only unsettled excess
					}
					int cx = 0;
					int cy = 0;
					int cz = 0;
					decode(cell, cx, cy, cz);
					if (cy > 0) {
						const int32_t belowFlat = cell - width;
						const uint8_t below = next[belowFlat];
						if (IsSolidMaterial(below)) {
							; // rests on Solid
						} else if (below == kFluidMat && targetMark[belowFlat] == basinGen) {
							; // rests on T (settled water)
						} else {
							continue; // rests on transit fluid / Air — not the excess bottom
						}
					}
					sources.push_back(cell);
					maxSourceY = std::max(maxSourceY, cy);
				}
			};
			const auto filterSourcesForDest = [&](const Dest &dest, const std::vector<int32_t> &sources,
												  std::vector<int32_t> &out) {
				out.clear();
				for (const int32_t source : sources) {
					const int sy = static_cast<int>((source / width) % height);
					if (sy > dest.y) {
						out.push_back(source); // dest below source
					} else if (sy == dest.y && distMark[source] == basinGen && distVal[source] > dest.dist) {
						out.push_back(source); // same level: only farther from T
					}
				}
			};
			const auto serveOne = [&](const Dest &dest) -> bool { // guard + one chain into dest
				std::vector<int32_t> sources;
				int maxSourceY = -1;
				refreshSources(sources, maxSourceY);
				if (dest.y > maxSourceY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) {
					return false;
				}
				std::vector<int32_t> eligible;
				filterSourcesForDest(dest, sources, eligible);
				const int len = static_cast<int>(eligible.size());
				if (len == 0) {
					return false;
				}
				for (int j = 0; j < len; ++j) {
					const int32_t source = eligible[(static_cast<size_t>(chainOrdinal + j)) % static_cast<size_t>(len)];
					if (columnMark[columnKey(source)] == basinGen) {
						continue; // one chain per column per tick
					}
					if (applyChainTo(source, dest.flat, id, cells)) {
						columnMark[columnKey(source)] = basinGen;
						++chainOrdinal;
						return true;
					}
				}
				return false;
			};
			{ // active outflow points emit each tick, stream heads (highest y) first — continuous streams, rate ∝ opening
				std::vector<const Dest *> exits;
				for (const Dest &dest : dests) {
					if (dest.exit) {
						exits.push_back(&dest);
					}
				}
				std::sort(exits.begin(), exits.end(), [](const Dest *a, const Dest *b) {
					return std::tuple{b->y, a->dist, a->z, a->x} < std::tuple{a->y, a->dist, a->z, a->x};
				});
				for (const Dest *dest : exits) {
					if (serveOne(*dest)) {
						++moved;
					}
				}
			}
			bool progressed = false;
			while (budget > 0) {
				std::vector<int32_t> sources;
				int maxSourceY = -1;
				refreshSources(sources, maxSourceY);
				progressed = false;
				size_t destIndex = 0;
				while (destIndex < dests.size() && !progressed) { // equal-D groups, rotated within a group — uniform overflow
					size_t groupEnd = destIndex + 1;
					while (groupEnd < dests.size() && dests[groupEnd].dist == dests[destIndex].dist) {
						++groupEnd;
					}
					const size_t groupSize = groupEnd - destIndex;
					for (size_t j = 0; j < groupSize && !progressed; ++j) {
						const Dest &dest = dests[destIndex + (static_cast<size_t>(chainOrdinal) + j) % groupSize];
						if (dest.exit) {
							continue; // already served above
						}
						if (dest.y > maxSourceY || next[dest.flat] != kAirMat || bodyId[dest.flat] >= 0) {
							continue;
						}
						std::vector<int32_t> eligible;
						filterSourcesForDest(dest, sources, eligible);
						const int len = static_cast<int>(eligible.size());
						if (len == 0) {
							continue;
						}
						const int stride = std::max(1, std::min(budget, len)); // spread successive chains across the layer
						for (int j2 = 0; j2 < len; ++j2) {
							const int32_t source = eligible[(static_cast<int64_t>(chainOrdinal + j2) * stride) % len];
							if (columnMark[columnKey(source)] == basinGen) {
								continue; // one chain per column per tick
							}
							if (applyChainTo(source, dest.flat, id, cells)) {
								columnMark[columnKey(source)] = basinGen;
								++chainOrdinal;
								progressed = true;
								break;
							}
						}
					}
					destIndex = groupEnd;
				}
				if (!progressed) { // bay fallback: fillable Air hole with ≥3 lateral body neighbours, source loosely bound (≤1)
					std::vector<int32_t> bays;
					for (const int32_t cell : cells) {
						int cx = 0;
						int cy = 0;
						int cz = 0;
						decode(cell, cx, cy, cz);
						for (int d = 1; d <= 4; ++d) {
							const int nx = cx + kNeighbor6[d][0];
							const int nz = cz + kNeighbor6[d][2];
							if (!isInsideLocal(nx, cy, nz)) {
								continue;
							}
							const int32_t nFlat = static_cast<int32_t>(index(nx, cy, nz));
							if (next[nFlat] == kAirMat && bodyId[nFlat] != id && targetMark[nFlat] != basinGen && fillable(nFlat, basinGen) &&
								lateralBodyNeighbours(nFlat, id) >= 3) {
								bays.push_back(nFlat);
							}
						}
					}
					std::sort(bays.begin(), bays.end());
					bays.erase(std::unique(bays.begin(), bays.end()), bays.end());
					std::vector<int32_t> loose; // bay heal may pull ∈T protrusions (T prefers stick-0 fluid over the Air hole)
					for (const int32_t cell : cells) {
						if (groundMark[cell] == basinGen && lateralBodyNeighbours(cell, id) <= 1) {
							loose.push_back(cell);
						}
					}
					for (const int32_t bay : bays) {
						for (const int32_t source : loose) {
							if (columnMark[columnKey(source)] == basinGen) {
								continue;
							}
							if (applyChainTo(source, bay, id, cells)) {
								columnMark[columnKey(source)] = basinGen;
								progressed = true;
								break;
							}
						}
						if (progressed) {
							break;
						}
					}
				}
				if (!progressed) {
					break;
				}
				--budget;
				++moved;
			}
		}
	}

	return moved;
}
