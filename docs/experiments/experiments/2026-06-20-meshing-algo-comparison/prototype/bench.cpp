// bench.cpp — 2026-06-20-meshing-algo-comparison prototype
// Standalone C++20 prototype. 3 algorithms × 3 scenes × 1000 iter.
// Per docs/experiments/benchmarks/methodology.md.
//
// Algorithms:
//   1. Naive Greedy (current ProjectV mainline pattern, per-axis dispatch)
//   2. Surface Nets (naive — vertex at cell center if any edge crossing)
//   3. Dual Contouring (simplified QEF per cell)
//
// Scenes (32^3 chunks):
//   - solid_cube: full 32^3 uniform solid (max faces)
//   - hollow_shell: solid border, air interior
//   - sphere: sphere-discretized (smooth surface)
//
// Build: clang++ -std=c++20 -O3 -march=native -DNDEBUG -o bench bench.cpp
// Run:   ./bench --all > results.csv && ./bench --all | tee RESULTS.md

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

constexpr int CHUNK = 32;
constexpr int CHUNK_VOL = CHUNK * CHUNK * CHUNK;

static inline size_t idx3(int x, int y, int z)
{
	return (size_t)(z * CHUNK + y) * CHUNK + x;
}

// ====================== Data structures ======================

struct VoxelBuffer {
	uint8_t data[CHUNK_VOL];

	VoxelBuffer() { std::memset(data, 0, sizeof(data)); }

	uint8_t &at(int x, int y, int z) { return data[idx3(x, y, z)]; }
	uint8_t at(int x, int y, int z) const { return data[idx3(x, y, z)]; }

	// 0 outside chunk = air (mimics cross-chunk OOB, matches ProjectV greedy contract
	// per agent/knowledge.md §25: ReadVoxelMaterial handles OOB returning Air).
	bool is_solid(int x, int y, int z) const
	{
		if (x < 0 || x >= CHUNK || y < 0 || y >= CHUNK || z < 0 || z >= CHUNK)
			return false;
		return data[idx3(x, y, z)] != 0;
	}
};

struct Vertex {
	float x, y, z;
	float nx, ny, nz;
};

struct MeshData {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	void clear()
	{
		vertices.clear();
		indices.clear();
	}
	size_t triangle_count() const { return indices.size() / 3; }
	size_t memory_bytes() const
	{
		return vertices.size() * sizeof(Vertex) + indices.size() * sizeof(uint32_t);
	}
};

// ====================== Statistics ======================

struct Stats {
	double mean_ns = 0, median_ns = 0, p95_ns = 0, p99_ns = 0, stddev_ns = 0, min_ns = 0, max_ns = 0;
};

Stats compute_stats(std::vector<double> samples)
{
	Stats s;
	if (samples.empty())
		return s;
	std::sort(samples.begin(), samples.end());
	double sum = 0;
	for (double v : samples)
		sum += v;
	s.mean_ns = sum / samples.size();
	s.median_ns = samples[samples.size() / 2];
	s.p95_ns = samples[(size_t)(samples.size() * 0.95)];
	s.p99_ns = samples[(size_t)(samples.size() * 0.99)];
	s.min_ns = samples.front();
	s.max_ns = samples.back();
	double var = 0;
	for (double v : samples)
		var += (v - s.mean_ns) * (v - s.mean_ns);
	s.stddev_ns = std::sqrt(var / samples.size());
	return s;
}

// ====================== Scene generators ======================

VoxelBuffer scene_solid_cube()
{
	VoxelBuffer v;
	for (int i = 0; i < CHUNK_VOL; i++)
		v.data[i] = 1;
	return v;
}

VoxelBuffer scene_hollow_shell()
{
	VoxelBuffer v;
	for (int z = 0; z < CHUNK; z++)
		for (int y = 0; y < CHUNK; y++)
			for (int x = 0; x < CHUNK; x++) {
				bool on_shell =
					(x == 0) || (x == CHUNK - 1) ||
					(y == 0) || (y == CHUNK - 1) ||
					(z == 0) || (z == CHUNK - 1);
				v.at(x, y, z) = on_shell ? 1 : 0;
			}
	return v;
}

VoxelBuffer scene_sphere()
{
	VoxelBuffer v;
	constexpr float R = 14.0f;
	constexpr float CX = (CHUNK - 1) * 0.5f;
	constexpr float CY = (CHUNK - 1) * 0.5f;
	constexpr float CZ = (CHUNK - 1) * 0.5f;
	for (int z = 0; z < CHUNK; z++)
		for (int y = 0; y < CHUNK; y++)
			for (int x = 0; x < CHUNK; x++) {
				float dx = x - CX, dy = y - CY, dz = z - CZ;
				v.at(x, y, z) = (dx * dx + dy * dy + dz * dz <= R * R) ? 1 : 0;
			}
	return v;
}

// Scene D: layered terrain — density varies per Y slice (10% at bottom → 90% at top).
// Tests surface continuity on coplanar layers (greedy should excel at quad extension).
VoxelBuffer scene_layered_terrain()
{
	VoxelBuffer v;
	std::mt19937 rng(0xCAFE);
	for (int y = 0; y < CHUNK; y++) {
		float density = 0.10f + 0.80f * (float)y / (CHUNK - 1);
		std::bernoulli_distribution bd(density);
		for (int z = 0; z < CHUNK; z++)
			for (int x = 0; x < CHUNK; x++)
				v.at(x, y, z) = bd(rng) ? 1 : 0;
	}
	return v;
}

// Scene E: sparse random — 1% density scattered cubes (smallest fill, fewest triangles).
VoxelBuffer scene_sparse_random()
{
	VoxelBuffer v;
	std::mt19937 rng(0xBEEF);
	std::bernoulli_distribution bd(0.01f);
	for (int z = 0; z < CHUNK; z++)
		for (int y = 0; y < CHUNK; y++)
			for (int x = 0; x < CHUNK; x++)
				v.at(x, y, z) = bd(rng) ? 1 : 0;
	return v;
}

// Scene F: projectv_mix — solid platform + 3x3x3 scattered solid cubes + aabb cavity.
// Mimics ProjectV's typical chunk: ground + decoration + interior structure.
VoxelBuffer scene_projectv_mix()
{
	VoxelBuffer v;
	// Ground platform: y in [0, 3].
	for (int y = 0; y < 4; y++)
		for (int z = 0; z < CHUNK; z++)
			for (int x = 0; x < CHUNK; x++)
				v.at(x, y, z) = 1;
	// 3x3x3 scattered cubes.
	int positions[5][3] = {
		{4, 5, 4}, {12, 5, 4}, {20, 5, 4}, {4, 5, 20}, {20, 5, 20}};
	for (int p = 0; p < 5; p++) {
		int bx = positions[p][0], by = positions[p][1], bz = positions[p][2];
		for (int dy = 0; dy < 3; dy++)
			for (int dz = 0; dz < 3; dz++)
				for (int dx = 0; dx < 3; dx++)
					v.at(bx + dx, by + dy, bz + dz) = 1;
	}
	// AABB cavity: hollow box from (10,10,10) to (20,20,20) — interior air, walls solid.
	for (int y = 10; y < 20; y++)
		for (int z = 10; z < 20; z++)
			for (int x = 10; x < 20; x++) {
				bool on_shell = (x == 10) || (x == 19) ||
								(y == 10) || (y == 19) ||
								(z == 10) || (z == 19);
				if (on_shell)
					v.at(x, y, z) = 1;
			}
	return v;
}

// ====================== Algorithm 1: Naive Greedy ======================
// Per-axis dispatch, 6 faces (X+/X-/Y+/Y-/Z+/Z-). Uses a "consumed" mask
// to avoid re-emitting the same face. Matches current ProjectV pattern in
// `voxel_mesh.comp::GreedyFacePass` (per agent/knowledge.md §25).

MeshData algo_naive_greedy(const VoxelBuffer &src)
{
	MeshData mesh;
	mesh.vertices.reserve(8192);
	mesh.indices.reserve(8192);
	bool consumed[CHUNK_VOL]; // per-direction mask, reset between sign iterations

	auto emit_quad = [&](float ax, float ay, float az,
						 float bx, float by, float bz,
						 float cx, float cy, float cz,
						 float dx_, float dy_, float dz_,
						 float nx, float ny, float nz) {
		uint32_t base = (uint32_t)mesh.vertices.size();
		mesh.vertices.push_back({ax, ay, az, nx, ny, nz});
		mesh.vertices.push_back({bx, by, bz, nx, ny, nz});
		mesh.vertices.push_back({cx, cy, cz, nx, ny, nz});
		mesh.vertices.push_back({dx_, dy_, dz_, nx, ny, nz});
		mesh.indices.push_back(base + 0);
		mesh.indices.push_back(base + 1);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 0);
		mesh.indices.push_back(base + 2);
		mesh.indices.push_back(base + 3);
	};

	// Helper: test if (x,y,z) is on shell facing positive direction along axis.
	// For axis=0 (X), positive face: voxel solid at (x,y,z), air at (x+1,y,z).
	// For axis=0 (X), negative face: voxel solid at (x,y,z), air at (x-1,y,z).

	// We iterate 6 directions: (axis, sign)
	// For each direction:
	//   - The "slice" coord is the one orthogonal to axis AND the layer depth coord.
	//   - The "u, v" coords are the other two.
	//   - Actually: per-axis pass. We project 3D into 2D (the two axes perpendicular to axis),
	//     at each "depth" (along axis). For each 2D cell, check if solid (matches sign direction).
	//     Greedy-extend in U then V. Mark consumed.

	// For brevity, we use a unified triple-nested approach:
	// For each axis in {0,1,2} (X,Y,Z):
	//   For each sign in {+1, -1}:
	//     For each depth d (where sign*axis-coordinate axis_pos):
	//       For each (u, v) in 2D plane perpendicular to axis:
	//         If cell at (u,v,d) is solid (sign=+1: voxel_pos; sign=-1: voxel_pos = d-1)
	//            and adjacent cell is air:
	//            Greedy-extend in U, V; emit quad; mark consumed.

	// Iteration order: for axis=0 (X), u=Y, v=Z, depth=X.
	for (int axis = 0; axis < 3; axis++) {
		int u_axis = (axis + 1) % 3;
		int v_axis = (axis + 2) % 3;

		for (int sign = 0; sign < 2; sign++) {
			std::memset(consumed, 0, sizeof(consumed));
			// For each depth
			for (int d = 0; d < CHUNK; d++) {
				// Plane at depth d along axis, perpendicular = (u_axis, v_axis)
				for (int u = 0; u < CHUNK; u++) {
					for (int v = 0; v < CHUNK; v++) {
						// Cell coords
						int cell[3] = {0, 0, 0};
						int neighbor[3] = {0, 0, 0};
						cell[axis] = d;
						neighbor[axis] = d + (sign == 0 ? 1 : -1);
						cell[u_axis] = u;
						cell[v_axis] = v;
						neighbor[u_axis] = u;
						neighbor[v_axis] = v;

						size_t cell_i = idx3(cell[0], cell[1], cell[2]);
						if (consumed[cell_i])
							continue;

						if (src.is_solid(cell[0], cell[1], cell[2]) &&
							!src.is_solid(neighbor[0], neighbor[1], neighbor[2])) {
							// Boundary — greedy extend in u, then v.
							int u_extent = 1;
							while (u + u_extent < CHUNK) {
								int cn[3] = {cell[0], cell[1], cell[2]};
								int nn[3] = {neighbor[0], neighbor[1], neighbor[2]};
								cn[u_axis] = u + u_extent;
								nn[u_axis] = u + u_extent;
								if (!src.is_solid(cn[0], cn[1], cn[2]))
									break;
								if (src.is_solid(nn[0], nn[1], nn[2]))
									break;
								u_extent++;
							}

							int v_extent = 1;
							while (v + v_extent < CHUNK) {
								bool ok = true;
								for (int du = 0; du < u_extent && ok; du++) {
									int cn[3] = {cell[0], cell[1], cell[2]};
									int nn[3] = {neighbor[0], neighbor[1], neighbor[2]};
									cn[u_axis] = u + du;
									nn[u_axis] = u + du;
									cn[v_axis] = v + v_extent;
									nn[v_axis] = v + v_extent;
									if (!src.is_solid(cn[0], cn[1], cn[2])) {
										ok = false;
										break;
									}
									if (src.is_solid(nn[0], nn[1], nn[2])) {
										ok = false;
										break;
									}
								}
								if (!ok)
									break;
								v_extent++;
							}

							// Mark consumed (per-direction mask)
							for (int du = 0; du < u_extent; du++)
								for (int dv = 0; dv < v_extent; dv++) {
									int cn[3] = {cell[0], cell[1], cell[2]};
									cn[u_axis] = u + du;
									cn[v_axis] = v + dv;
									consumed[idx3(cn[0], cn[1], cn[2])] = true;
								}

							// Emit quad. Quad plane position:
							//   - positive face: at cell[axis] + 1 in axis direction
							//   - negative face: at cell[axis] in axis direction
							float face_pos = (float)(cell[axis] + (sign == 0 ? 1 : 0));
							float n_axis = (sign == 0) ? 1.0f : -1.0f;

							float a[3] = {(float)cell[0], (float)cell[1], (float)cell[2]};
							float b[3] = {(float)cell[0], (float)cell[1], (float)cell[2]};
							float c[3] = {(float)cell[0], (float)cell[1], (float)cell[2]};
							float dd[3] = {(float)cell[0], (float)cell[1], (float)cell[2]};

							a[axis] = face_pos;
							b[axis] = face_pos;
							c[axis] = face_pos;
							dd[axis] = face_pos;

							b[u_axis] = (float)(u + u_extent);
							c[u_axis] = (float)(u + u_extent);
							c[v_axis] = (float)(v + v_extent);
							dd[v_axis] = (float)(v + v_extent);

							float nx = 0, ny = 0, nz = 0;
							if (axis == 0)
								nx = n_axis;
							else if (axis == 1)
								ny = n_axis;
							else
								nz = n_axis;

							emit_quad(a[0], a[1], a[2],
									  b[0], b[1], b[2],
									  c[0], c[1], c[2],
									  dd[0], dd[1], dd[2],
									  nx, ny, nz);
						}
					}
				}
			}
		}
	}

	return mesh;
}

// ====================== Algorithm 2: Surface Nets (naive) ======================
// For each cell, check 12 edges. If any edge is sign-changing, place vertex at cell center.
// For each sign-changing edge, emit a quad connecting 4 surrounding cell vertices.
// Naive = no Laplacian smoothing (post-pass not part of core algorithm).

struct SNVertex {
	float x, y, z;
};

MeshData algo_surface_nets(const VoxelBuffer &v)
{
	MeshData mesh;
	// Vertex grid: (CHUNK+1) per axis — vertices at cell centers (slight offset).
	// For SN naive, vertex at cell center if any edge crossing.
	constexpr int V = CHUNK; // one vertex per cell
	std::vector<int> cell_vertex(V * V * V, -1);
	mesh.vertices.reserve(8192);
	mesh.indices.reserve(8192);

	auto cell_idx = [&](int x, int y, int z) {
		return (size_t)(z * V + y) * V + x;
	};

	// Edge endpoints: 12 edges per cell. For each, check sign change.
	// Vertex position: average of edge intersection points (8 corners of cube → 12 edges).
	// For naive: just use cell center.

	// Edges in a unit cube (delta direction): for each of 3 axes, two opposite directions.
	// We'll iterate over all edges in the grid (each edge shared by up to 4 cells, but for naive SN,
	// we just iterate per-cell).

	// Naive approach: for each cell, if any of its 12 edges has sign change, create vertex at center.
	for (int z = 0; z < V; z++) {
		for (int y = 0; y < V; y++) {
			for (int x = 0; x < V; x++) {
				// 12 edges: 4 along each axis (each face of cube has 4 edges)
				// Easier: for each of 3 axes, check 4 edges.
				bool has_crossing = false;
				// Edges along X (varying x, at corners (y±0.5, z±0.5)): actually let's enumerate.
				// 12 edges of a cube:
				//  X-edges (4): (x, y, z)-(x+1, y, z), (x, y+1, z)-(x+1, y+1, z),
				//                (x, y, z+1)-(x+1, y, z+1), (x, y+1, z+1)-(x+1, y+1, z+1)
				//  Y-edges (4), Z-edges (4)
				float corners[8] = {
					v.is_solid(x, y, z) ? 1.0f : -1.0f,				// 0: (0,0,0)
					v.is_solid(x + 1, y, z) ? 1.0f : -1.0f,			// 1: (1,0,0)
					v.is_solid(x, y + 1, z) ? 1.0f : -1.0f,			// 2: (0,1,0)
					v.is_solid(x + 1, y + 1, z) ? 1.0f : -1.0f,		// 3: (1,1,0)
					v.is_solid(x, y, z + 1) ? 1.0f : -1.0f,			// 4: (0,0,1)
					v.is_solid(x + 1, y, z + 1) ? 1.0f : -1.0f,		// 5: (1,0,1)
					v.is_solid(x, y + 1, z + 1) ? 1.0f : -1.0f,		// 6: (0,1,1)
					v.is_solid(x + 1, y + 1, z + 1) ? 1.0f : -1.0f, // 7: (1,1,1)
				};
				// 12 edges: pairs of corner indices
				int edges[12][2] = {
					{0, 1}, {2, 3}, {4, 5}, {6, 7}, // X
					{0, 2},
					{1, 3},
					{4, 6},
					{5, 7}, // Y
					{0, 4},
					{1, 5},
					{2, 6},
					{3, 7} // Z
				};
				for (int e = 0; e < 12; e++) {
					if (corners[edges[e][0]] * corners[edges[e][1]] < 0) {
						has_crossing = true;
						break;
					}
				}
				if (has_crossing) {
					// Vertex at mean of intersection points (naive: cell center + 0.5).
					float vx = (float)x + 0.5f;
					float vy = (float)y + 0.5f;
					float vz = (float)z + 0.5f;

					// Normal: gradient of occupancy at cell center.
					float gx = (v.is_solid(x + 1, y, z) ? 1.0f : -1.0f) - (v.is_solid(x - 1, y, z) ? 1.0f : -1.0f);
					float gy = (v.is_solid(x, y + 1, z) ? 1.0f : -1.0f) - (v.is_solid(x, y - 1, z) ? 1.0f : -1.0f);
					float gz = (v.is_solid(x, y, z + 1) ? 1.0f : -1.0f) - (v.is_solid(x, y, z - 1) ? 1.0f : -1.0f);
					float gl = std::sqrt(gx * gx + gy * gy + gz * gz);
					if (gl > 1e-6f) {
						gx /= gl;
						gy /= gl;
						gz /= gl;
					} else {
						gx = 1;
						gy = 0;
						gz = 0;
					}

					mesh.vertices.push_back({vx, vy, vz, gx, gy, gz});
					cell_vertex[cell_idx(x, y, z)] = (int)mesh.vertices.size() - 1;
				}
			}
		}
	}

	// For each sign-changing edge, emit quad connecting 4 surrounding cells.
	// 12 edges per cell — but each shared by 4 cells. We iterate per-cell and emit quads only
	// when the "lower" cell along the edge owns it (to avoid duplicates). Use: only emit if
	// cell < neighbor along edge axis.
	auto try_emit_quad = [&](int x0, int y0, int z0,
							 int x1, int y1, int z1,
							 int x2, int y2, int z2,
							 int x3, int y3, int z3) {
		int i0 = cell_vertex[cell_idx(x0, y0, z0)];
		int i1 = cell_vertex[cell_idx(x1, y1, z1)];
		int i2 = cell_vertex[cell_idx(x2, y2, z2)];
		int i3 = cell_vertex[cell_idx(x3, y3, z3)];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0)
			return;
		mesh.indices.push_back((uint32_t)i0);
		mesh.indices.push_back((uint32_t)i1);
		mesh.indices.push_back((uint32_t)i2);
		mesh.indices.push_back((uint32_t)i0);
		mesh.indices.push_back((uint32_t)i2);
		mesh.indices.push_back((uint32_t)i3);
	};

	auto edge_sign = [&](int x0, int y0, int z0, int x1, int y1, int z1) -> bool {
		bool s0 = v.is_solid(x0, y0, z0);
		bool s1 = v.is_solid(x1, y1, z1);
		return s0 != s1;
	};

	for (int z = 0; z < V; z++) {
		for (int y = 0; y < V; y++) {
			for (int x = 0; x < V; x++) {
				// X-edge from (x,y,z) to (x+1,y,z): cells around = (x,y,z), (x,y-1,z), (x,y-1,z-1), (x,y,z-1)
				// (if edge is sign-changing AND we own it = edge at lower-x)
				if (x < V - 1 && edge_sign(x, y, z, x + 1, y, z)) {
					try_emit_quad(x, y, z, x, y - 1, z, x, y - 1, z - 1, x, y, z - 1);
				}
				// Y-edge from (x,y,z) to (x,y+1,z)
				if (y < V - 1 && edge_sign(x, y, z, x, y + 1, z)) {
					try_emit_quad(x, y, z, x, y, z - 1, x + 1, y, z - 1, x + 1, y, z);
				}
				// Z-edge from (x,y,z) to (x,y,z+1)
				if (z < V - 1 && edge_sign(x, y, z, x, y, z + 1)) {
					try_emit_quad(x, y, z, x + 1, y, z, x + 1, y, z - 1, x, y, z - 1);
				}
			}
		}
	}

	return mesh;
}

// ====================== Algorithm 3: Dual Contouring (simplified) ======================
// For each cell with at least one sign-changing edge:
//   - Collect Hermite data: per-edge (intersection point, normal from gradient).
//   - Build QEF: M = sum n n^T, b = sum (n n^T) p.
//   - Solve 3x3 system M v = b.
//   - Place vertex at v.
// For each sign-changing edge, emit quad connecting 4 cell vertices.
//
// Note: simplified — no octree, no manifold guarantee. Per nickgildea/fast_dual_contouring.

struct DCVertex {
	float x, y, z;
};

static bool solve_qef(float M[3][3], float b[3], float out[3])
{
	// Solve 3x3 symmetric system via Cramer's rule (sufficient for prototype).
	float det =
		M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
		M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
		M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
	if (std::fabs(det) < 1e-9f)
		return false;
	float inv = 1.0f / det;
	out[0] = inv * (b[0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
					M[0][1] * (b[1] * M[2][2] - M[1][2] * b[2]) +
					M[0][2] * (b[1] * M[2][1] - M[1][1] * b[2]));
	out[1] = inv * (M[0][0] * (b[1] * M[2][2] - M[1][2] * b[2]) -
					b[0] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) +
					M[0][2] * (M[1][0] * b[2] - b[1] * M[2][0]));
	out[2] = inv * (M[0][0] * (M[1][1] * b[2] - b[1] * M[2][1]) -
					M[0][1] * (M[1][0] * b[2] - b[1] * M[2][0]) +
					b[0] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]));
	return true;
}

MeshData algo_dual_contouring(const VoxelBuffer &v)
{
	MeshData mesh;
	constexpr int V = CHUNK;
	std::vector<int> cell_vertex(V * V * V, -1);
	mesh.vertices.reserve(8192);
	mesh.indices.reserve(8192);

	auto cell_idx = [&](int x, int y, int z) {
		return (size_t)(z * V + y) * V + x;
	};

	// For each cell, collect Hermite data and solve QEF.
	for (int z = 0; z < V; z++) {
		for (int y = 0; y < V; y++) {
			for (int x = 0; x < V; x++) {
				// 12 edges, each with (intersection point, normal).
				// Edge endpoints: corner pairs. For each edge, midpoint is intersection (for
				// quantized input where sign crosses exactly at midpoint).
				// Normal: gradient of occupancy sampled at edge midpoint.

				float M[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
				float b[3] = {0, 0, 0};

				int corners[8][3] = {
					{x, y, z}, {x + 1, y, z}, {x, y + 1, z}, {x + 1, y + 1, z}, {x, y, z + 1}, {x + 1, y, z + 1}, {x, y + 1, z + 1}, {x + 1, y + 1, z + 1}};
				int edges[12][2] = {
					{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

				bool has_crossing = false;
				for (int e = 0; e < 12; e++) {
					int a = edges[e][0], b_ = edges[e][1];
					int ax = corners[a][0], ay = corners[a][1], az = corners[a][2];
					int bx = corners[b_][0], by = corners[b_][1], bz = corners[b_][2];
					bool sa = v.is_solid(ax, ay, az);
					bool sb = v.is_solid(bx, by, bz);
					if (sa != sb) {
						has_crossing = true;
						// Intersection point: midpoint
						float px = (corners[a][0] + corners[b_][0]) * 0.5f;
						float py = (corners[a][1] + corners[b_][1]) * 0.5f;
						float pz = (corners[a][2] + corners[b_][2]) * 0.5f;
						// Normal: gradient at edge midpoint.
						// Approximate: outward from solid side.
						// For quantized: use direction from solid center → midpoint is roughly normal.
						// We sample gradient by sampling occupancy at ±1 along each axis at the midpoint.
						float gx = (v.is_solid((int)px + 1, (int)py, (int)pz) ? 1.0f : -1.0f) -
								   (v.is_solid((int)px - 1, (int)py, (int)pz) ? 1.0f : -1.0f);
						float gy = (v.is_solid((int)px, (int)py + 1, (int)pz) ? 1.0f : -1.0f) -
								   (v.is_solid((int)px, (int)py - 1, (int)pz) ? 1.0f : -1.0f);
						float gz = (v.is_solid((int)px, (int)py, (int)pz + 1) ? 1.0f : -1.0f) -
								   (v.is_solid((int)px, (int)py, (int)pz - 1) ? 1.0f : -1.0f);
						float gl = std::sqrt(gx * gx + gy * gy + gz * gz);
						if (gl < 1e-6f) {
							gx = 0;
							gy = 0;
							gz = 0;
						} else {
							gx /= gl;
							gy /= gl;
							gz /= gl;
						}

						// Add (n n^T) to M and (n n^T p) to b.
						for (int i = 0; i < 3; i++)
							for (int j = 0; j < 3; j++)
								M[i][j] += (i == 0 ? gx : i == 1 ? gy
																 : gz) *
										   (j == 0 ? gx : j == 1 ? gy
																 : gz);
						b[0] += gx * (gx * px + gy * py + gz * pz);
						b[1] += gy * (gx * px + gy * py + gz * pz);
						b[2] += gz * (gx * px + gy * py + gz * pz);
					}
				}

				if (has_crossing) {
					float v_pos[3];
					if (!solve_qef(M, b, v_pos)) {
						// Singular — fallback to cell center.
						v_pos[0] = (float)x + 0.5f;
						v_pos[1] = (float)y + 0.5f;
						v_pos[2] = (float)z + 0.5f;
					}
					mesh.vertices.push_back({v_pos[0], v_pos[1], v_pos[2], 0, 1, 0});
					cell_vertex[cell_idx(x, y, z)] = (int)mesh.vertices.size() - 1;
				}
			}
		}
	}

	// Emit quads on sign-changing edges (4 cells around each edge).
	auto try_emit_quad = [&](int x0, int y0, int z0,
							 int x1, int y1, int z1,
							 int x2, int y2, int z2,
							 int x3, int y3, int z3) {
		int i0 = cell_vertex[cell_idx(x0, y0, z0)];
		int i1 = cell_vertex[cell_idx(x1, y1, z1)];
		int i2 = cell_vertex[cell_idx(x2, y2, z2)];
		int i3 = cell_vertex[cell_idx(x3, y3, z3)];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i3 < 0)
			return;
		mesh.indices.push_back((uint32_t)i0);
		mesh.indices.push_back((uint32_t)i1);
		mesh.indices.push_back((uint32_t)i2);
		mesh.indices.push_back((uint32_t)i0);
		mesh.indices.push_back((uint32_t)i2);
		mesh.indices.push_back((uint32_t)i3);
	};

	auto edge_sign = [&](int x0, int y0, int z0, int x1, int y1, int z1) -> bool {
		return v.is_solid(x0, y0, z0) != v.is_solid(x1, y1, z1);
	};

	for (int z = 0; z < V; z++) {
		for (int y = 0; y < V; y++) {
			for (int x = 0; x < V; x++) {
				if (x < V - 1 && edge_sign(x, y, z, x + 1, y, z))
					try_emit_quad(x, y, z, x, y - 1, z, x, y - 1, z - 1, x, y, z - 1);
				if (y < V - 1 && edge_sign(x, y, z, x, y + 1, z))
					try_emit_quad(x, y, z, x, y, z - 1, x + 1, y, z - 1, x + 1, y, z);
				if (z < V - 1 && edge_sign(x, y, z, x, y, z + 1))
					try_emit_quad(x, y, z, x + 1, y, z, x + 1, y, z - 1, x, y, z - 1);
			}
		}
	}

	return mesh;
}

// ====================== Algorithm 4: Marching Cubes (standard table) ======================
// Standard Lorensen-Cline 1987 table. 8 corners = 256 cases.
// For binary voxel input, intersection is always at edge midpoint (iso = 0.5).
// Edge cache shared across cells so adjacent cells reuse edge vertices.
//
// Edge ordering (Bourke convention):
//   0:(0-1) 1:(1-2) 2:(2-3) 3:(3-0)
//   4:(4-5) 5:(5-6) 6:(6-7) 7:(7-4)
//   8:(0-4) 9:(1-5) 10:(2-6) 11:(3-7)

static const int MC_EDGE_TABLE[256] = {
	0x0, 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
	0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
	0x190, 0x99, 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
	0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
	0x230, 0x339, 0x33, 0x13a, 0x636, 0x73f, 0x435, 0x53c,
	0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
	0x3a0, 0x2a9, 0x1a3, 0xaa, 0x7a6, 0x6af, 0x5a5, 0x4ac,
	0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
	0x460, 0x569, 0x663, 0x76a, 0x66, 0x16f, 0x265, 0x36c,
	0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
	0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff, 0x3f5, 0x2fc,
	0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
	0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55, 0x15c,
	0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
	0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc,
	0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
	0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
	0xcc, 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
	0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
	0x15c, 0x55, 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
	0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
	0x2fc, 0x3f5, 0xff, 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
	0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
	0x36c, 0x265, 0x16f, 0x66, 0x76a, 0x663, 0x569, 0x460,
	0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
	0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa, 0x1a3, 0x2a9, 0x3a0,
	0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
	0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33, 0x339, 0x230,
	0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
	0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99, 0x190,
	0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
	0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0};

// Each row: edge indices for triangles, terminated by -1.
// Up to 5 triangles per case (max 15 edge indices + terminator).
static const int MC_TRI_TABLE[256][16] = {
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 8, 3, 9, 8, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 2, 10, 0, 2, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 8, 3, 2, 10, 8, 10, 9, 8, -1, -1, -1, -1, -1, -1, -1},
	{3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 11, 2, 8, 11, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 11, 2, 1, 9, 11, 9, 8, 11, -1, -1, -1, -1, -1, -1, -1},
	{3, 10, 1, 11, 10, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 10, 1, 0, 8, 10, 8, 11, 10, -1, -1, -1, -1, -1, -1, -1},
	{3, 9, 0, 3, 11, 9, 11, 10, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 3, 0, 7, 3, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 1, 9, 4, 7, 1, 7, 3, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 4, 7, 3, 0, 4, 1, 2, 10, -1, -1, -1, -1, -1, -1, -1},
	{9, 2, 10, 9, 0, 2, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
	{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4, -1, -1, -1, -1},
	{8, 4, 7, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 4, 7, 11, 2, 4, 2, 0, 4, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 1, 8, 4, 7, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
	{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1, -1, -1, -1, -1},
	{3, 10, 1, 3, 11, 10, 7, 8, 4, -1, -1, -1, -1, -1, -1, -1},
	{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4, -1, -1, -1, -1},
	{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3, -1, -1, -1, -1},
	{4, 7, 11, 4, 11, 9, 9, 11, 10, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 5, 4, 1, 5, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 5, 4, 8, 3, 5, 3, 1, 5, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 1, 2, 10, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
	{5, 2, 10, 5, 4, 2, 4, 0, 2, -1, -1, -1, -1, -1, -1, -1},
	{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8, -1, -1, -1, -1},
	{9, 5, 4, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 11, 2, 0, 8, 11, 4, 9, 5, -1, -1, -1, -1, -1, -1, -1},
	{0, 5, 4, 0, 1, 5, 2, 3, 11, -1, -1, -1, -1, -1, -1, -1},
	{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5, -1, -1, -1, -1},
	{10, 3, 11, 10, 1, 3, 9, 5, 4, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10, -1, -1, -1, -1},
	{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3, -1, -1, -1, -1},
	{5, 4, 8, 5, 8, 10, 10, 8, 11, -1, -1, -1, -1, -1, -1, -1},
	{9, 7, 8, 5, 7, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 3, 0, 9, 5, 3, 5, 7, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 7, 8, 0, 1, 7, 1, 5, 7, -1, -1, -1, -1, -1, -1, -1},
	{1, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 7, 8, 9, 5, 7, 10, 1, 2, -1, -1, -1, -1, -1, -1, -1},
	{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3, -1, -1, -1, -1},
	{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2, -1, -1, -1, -1},
	{2, 10, 5, 2, 5, 3, 3, 5, 7, -1, -1, -1, -1, -1, -1, -1},
	{7, 9, 5, 7, 8, 9, 3, 11, 2, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11, -1, -1, -1, -1},
	{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7, -1, -1, -1, -1},
	{11, 2, 1, 11, 1, 7, 7, 1, 5, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11, -1, -1, -1, -1},
	{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0, -1},
	{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0, -1},
	{11, 10, 5, 7, 11, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 1, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 8, 3, 1, 9, 8, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 5, 2, 6, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 5, 1, 2, 6, 3, 0, 8, -1, -1, -1, -1, -1, -1, -1},
	{9, 6, 5, 9, 0, 6, 0, 2, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8, -1, -1, -1, -1},
	{2, 3, 11, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 0, 8, 11, 2, 0, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11, -1, -1, -1, -1},
	{6, 3, 11, 6, 5, 3, 5, 1, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6, -1, -1, -1, -1},
	{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9, -1, -1, -1, -1},
	{6, 5, 9, 6, 9, 11, 11, 9, 8, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 3, 0, 4, 7, 3, 6, 5, 10, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 5, 10, 6, 8, 4, 7, -1, -1, -1, -1, -1, -1, -1},
	{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4, -1, -1, -1, -1},
	{6, 1, 2, 6, 5, 1, 4, 7, 8, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7, -1, -1, -1, -1},
	{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6, -1, -1, -1, -1},
	{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9, -1},
	{3, 11, 2, 7, 8, 4, 10, 6, 5, -1, -1, -1, -1, -1, -1, -1},
	{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11, -1, -1, -1, -1},
	{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6, -1, -1, -1, -1},
	{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6, -1},
	{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6, -1, -1, -1, -1},
	{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11, -1},
	{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7, -1},
	{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9, -1, -1, -1, -1},
	{10, 4, 9, 6, 4, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 10, 6, 4, 9, 10, 0, 8, 3, -1, -1, -1, -1, -1, -1, -1},
	{10, 0, 1, 10, 6, 0, 6, 4, 0, -1, -1, -1, -1, -1, -1, -1},
	{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10, -1, -1, -1, -1},
	{1, 4, 9, 1, 2, 4, 2, 6, 4, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4, -1, -1, -1, -1},
	{0, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 3, 2, 8, 2, 4, 4, 2, 6, -1, -1, -1, -1, -1, -1, -1},
	{10, 4, 9, 10, 6, 4, 11, 2, 3, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6, -1, -1, -1, -1},
	{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10, -1, -1, -1, -1},
	{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1, -1},
	{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3, -1, -1, -1, -1},
	{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1, -1},
	{3, 11, 6, 3, 6, 0, 0, 6, 4, -1, -1, -1, -1, -1, -1, -1},
	{6, 4, 8, 11, 6, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 10, 6, 7, 8, 10, 8, 9, 10, -1, -1, -1, -1, -1, -1, -1},
	{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10, -1, -1, -1, -1},
	{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0, -1, -1, -1, -1},
	{10, 6, 7, 10, 7, 1, 1, 7, 3, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7, -1, -1, -1, -1},
	{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9, -1},
	{7, 8, 0, 7, 0, 6, 6, 0, 2, -1, -1, -1, -1, -1, -1, -1},
	{7, 3, 2, 6, 7, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7, -1, -1, -1, -1},
	{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7, -1},
	{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11, -1},
	{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1, -1, -1, -1, -1},
	{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6, -1},
	{0, 9, 1, 11, 6, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0, -1, -1, -1, -1},
	{7, 11, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 8, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 9, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 1, 9, 8, 3, 1, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
	{10, 1, 2, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 3, 0, 8, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
	{2, 9, 0, 2, 10, 9, 6, 11, 7, -1, -1, -1, -1, -1, -1, -1},
	{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8, -1, -1, -1, -1},
	{7, 2, 3, 6, 2, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{7, 0, 8, 7, 6, 0, 6, 2, 0, -1, -1, -1, -1, -1, -1, -1},
	{2, 7, 6, 2, 3, 7, 0, 1, 9, -1, -1, -1, -1, -1, -1, -1},
	{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6, -1, -1, -1, -1},
	{10, 7, 6, 10, 1, 7, 1, 3, 7, -1, -1, -1, -1, -1, -1, -1},
	{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8, -1, -1, -1, -1},
	{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7, -1, -1, -1, -1},
	{7, 6, 10, 7, 10, 8, 8, 10, 9, -1, -1, -1, -1, -1, -1, -1},
	{6, 8, 4, 11, 8, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 6, 11, 3, 0, 6, 0, 4, 6, -1, -1, -1, -1, -1, -1, -1},
	{8, 6, 11, 8, 4, 6, 9, 0, 1, -1, -1, -1, -1, -1, -1, -1},
	{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6, -1, -1, -1, -1},
	{6, 8, 4, 6, 11, 8, 2, 10, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6, -1, -1, -1, -1},
	{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9, -1, -1, -1, -1},
	{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3, -1},
	{8, 2, 3, 8, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1},
	{0, 4, 2, 4, 6, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8, -1, -1, -1, -1},
	{1, 9, 4, 1, 4, 2, 2, 4, 6, -1, -1, -1, -1, -1, -1, -1},
	{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1, -1, -1, -1, -1},
	{10, 1, 0, 10, 0, 6, 6, 0, 4, -1, -1, -1, -1, -1, -1, -1},
	{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3, -1},
	{10, 9, 4, 6, 10, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 5, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 4, 9, 5, 11, 7, 6, -1, -1, -1, -1, -1, -1, -1},
	{5, 0, 1, 5, 4, 0, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
	{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5, -1, -1, -1, -1},
	{9, 5, 4, 10, 1, 2, 7, 6, 11, -1, -1, -1, -1, -1, -1, -1},
	{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5, -1, -1, -1, -1},
	{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2, -1, -1, -1, -1},
	{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6, -1},
	{7, 2, 3, 7, 6, 2, 5, 4, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7, -1, -1, -1, -1},
	{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0, -1, -1, -1, -1},
	{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8, -1},
	{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7, -1, -1, -1, -1},
	{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4, -1},
	{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10, -1},
	{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10, -1, -1, -1, -1},
	{6, 9, 5, 6, 11, 9, 11, 8, 9, -1, -1, -1, -1, -1, -1, -1},
	{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5, -1, -1, -1, -1},
	{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11, -1, -1, -1, -1},
	{6, 11, 3, 6, 3, 5, 5, 3, 1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6, -1, -1, -1, -1},
	{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10, -1},
	{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5, -1},
	{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3, -1, -1, -1, -1},
	{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2, -1, -1, -1, -1},
	{9, 5, 6, 9, 6, 0, 0, 6, 2, -1, -1, -1, -1, -1, -1, -1},
	{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8, -1},
	{1, 5, 6, 2, 1, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6, -1},
	{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0, -1, -1, -1, -1},
	{0, 3, 8, 5, 6, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{10, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 5, 10, 7, 5, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{11, 5, 10, 11, 7, 5, 8, 3, 0, -1, -1, -1, -1, -1, -1, -1},
	{5, 11, 7, 5, 10, 11, 1, 9, 0, -1, -1, -1, -1, -1, -1, -1},
	{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1, -1, -1, -1, -1},
	{11, 1, 2, 11, 7, 1, 7, 5, 1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11, -1, -1, -1, -1},
	{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7, -1, -1, -1, -1},
	{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2, -1},
	{2, 5, 10, 2, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1},
	{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5, -1, -1, -1, -1},
	{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2, -1, -1, -1, -1},
	{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2, -1},
	{1, 3, 5, 3, 7, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 7, 0, 7, 1, 1, 7, 5, -1, -1, -1, -1, -1, -1, -1},
	{9, 0, 3, 9, 3, 5, 5, 3, 7, -1, -1, -1, -1, -1, -1, -1},
	{9, 8, 7, 5, 9, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{5, 8, 4, 5, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1},
	{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0, -1, -1, -1, -1},
	{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5, -1, -1, -1, -1},
	{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4, -1},
	{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8, -1, -1, -1, -1},
	{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11, -1},
	{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5, -1},
	{9, 4, 5, 2, 11, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4, -1, -1, -1, -1},
	{5, 10, 2, 5, 2, 4, 4, 2, 0, -1, -1, -1, -1, -1, -1, -1},
	{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9, -1},
	{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2, -1, -1, -1, -1},
	{8, 4, 5, 8, 5, 3, 3, 5, 1, -1, -1, -1, -1, -1, -1, -1},
	{0, 4, 5, 1, 0, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5, -1, -1, -1, -1},
	{9, 4, 5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 11, 7, 4, 9, 11, 9, 10, 11, -1, -1, -1, -1, -1, -1, -1},
	{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11, -1, -1, -1, -1},
	{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11, -1, -1, -1, -1},
	{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4, -1},
	{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2, -1, -1, -1, -1},
	{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3, -1},
	{11, 7, 4, 11, 4, 2, 2, 4, 0, -1, -1, -1, -1, -1, -1, -1},
	{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4, -1, -1, -1, -1},
	{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9, -1, -1, -1, -1},
	{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7, -1},
	{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10, -1},
	{1, 10, 2, 8, 7, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 1, 4, 1, 7, 7, 1, 3, -1, -1, -1, -1, -1, -1, -1},
	{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1, -1, -1, -1, -1},
	{4, 0, 3, 7, 4, 3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{4, 8, 7, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{9, 10, 8, 10, 11, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 9, 3, 9, 11, 11, 9, 10, -1, -1, -1, -1, -1, -1, -1},
	{0, 1, 10, 0, 10, 8, 8, 10, 11, -1, -1, -1, -1, -1, -1, -1},
	{3, 1, 10, 11, 3, 10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 2, 11, 1, 11, 9, 9, 11, 8, -1, -1, -1, -1, -1, -1, -1},
	{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9, -1, -1, -1, -1},
	{0, 2, 11, 8, 0, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{3, 2, 11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 8, 2, 8, 10, 10, 8, 9, -1, -1, -1, -1, -1, -1, -1},
	{9, 10, 2, 0, 9, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8, -1, -1, -1, -1},
	{1, 10, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{1, 3, 8, 9, 1, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 9, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{0, 3, 8, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}};

// Edge endpoint pairs (Bourke convention).
// edge i connects corners (a, b); vertex on edge is midpoint since binary input → iso=0.5.
static const int MC_EDGE_CORNERS[12][2] = {
	{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

static const int MC_CORNER_OFFSET[8][3] = {
	{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

MeshData algo_marching_cubes(const VoxelBuffer &v)
{
	MeshData mesh;
	// Edge vertex cache: 3D grid of (CHUNK+1) per axis × 3 edges per axis = total edges.
	// Edges indexed as: axis-major (x=0, y=1, z=2), then position. Use flat array.
	// X-edges: (CHUNK+1) × CHUNK × CHUNK (varying along x)
	// Y-edges: CHUNK × (CHUNK+1) × CHUNK
	// Z-edges: CHUNK × CHUNK × (CHUNK+1)
	constexpr int X_EDGES = (CHUNK + 1) * CHUNK * CHUNK;
	constexpr int Y_EDGES = CHUNK * (CHUNK + 1) * CHUNK;
	constexpr int Z_EDGES = CHUNK * CHUNK * (CHUNK + 1);
	std::vector<int> x_edge_v(X_EDGES, -1);
	std::vector<int> y_edge_v(Y_EDGES, -1);
	std::vector<int> z_edge_v(Z_EDGES, -1);

	auto x_idx = [&](int x, int y, int z) { return (size_t)(z * CHUNK + y) * (CHUNK + 1) + x; };
	auto y_idx = [&](int x, int y, int z) { return (size_t)(z * (CHUNK + 1) + y) * CHUNK + x; };
	auto z_idx = [&](int x, int y, int z) { return (size_t)((z + 1) * CHUNK * CHUNK + y * CHUNK) + x; };

	auto get_edge_vertex = [&](int edge_id, int cx, int cy, int cz) -> int {
		int a = MC_EDGE_CORNERS[edge_id][0];
		int b = MC_EDGE_CORNERS[edge_id][1];
		int ax = cx + MC_CORNER_OFFSET[a][0];
		int ay = cy + MC_CORNER_OFFSET[a][1];
		int az = cz + MC_CORNER_OFFSET[a][2];
		int bx = cx + MC_CORNER_OFFSET[b][0];
		int by = cy + MC_CORNER_OFFSET[b][1];
		int bz = cz + MC_CORNER_OFFSET[b][2];
		// Midpoint (binary → iso=0.5).
		float vx = (ax + bx) * 0.5f;
		float vy = (ay + by) * 0.5f;
		float vz = (az + bz) * 0.5f;
		// Lookup based on edge direction.
		if (ax != bx)
			return x_edge_v[x_idx(ax, ay, az)]; // edge along X
		if (ay != by)
			return y_edge_v[y_idx(ax, ay, az)]; // edge along Y
		return z_edge_v[z_idx(ax, ay, az)];		// edge along Z
	};

	auto set_edge_vertex = [&](int edge_id, int cx, int cy, int cz, float vx, float vy, float vz) -> int {
		int a = MC_EDGE_CORNERS[edge_id][0];
		int ax = cx + MC_CORNER_OFFSET[a][0];
		int ay = cy + MC_CORNER_OFFSET[a][1];
		int az = cz + MC_CORNER_OFFSET[a][2];
		int idx;
		if (edge_id < 4)
			idx = x_idx(ax, ay, az); // X edges 0-3
		else if (edge_id < 8)
			idx = y_idx(ax, ay, az); // Y edges 4-7
		else
			idx = z_idx(ax, ay, az); // Z edges 8-11
		if (edge_id < 4 && x_edge_v[idx] >= 0)
			return x_edge_v[idx];
		if (edge_id < 8 && edge_id >= 4 && y_edge_v[idx] >= 0)
			return y_edge_v[idx];
		if (edge_id >= 8 && z_edge_v[idx] >= 0)
			return z_edge_v[idx];

		mesh.vertices.push_back({vx, vy, vz, 0, 1, 0});
		int vid = (int)mesh.vertices.size() - 1;
		if (edge_id < 4)
			x_edge_v[idx] = vid;
		else if (edge_id < 8)
			y_edge_v[idx] = vid;
		else
			z_edge_v[idx] = vid;
		return vid;
	};

	for (int z = 0; z < CHUNK - 1; z++) {
		for (int y = 0; y < CHUNK - 1; y++) {
			for (int x = 0; x < CHUNK - 1; x++) {
				// 8 corners.
				bool corners[8];
				int cube_idx = 0;
				for (int i = 0; i < 8; i++) {
					corners[i] = v.is_solid(x + MC_CORNER_OFFSET[i][0],
											y + MC_CORNER_OFFSET[i][1],
											z + MC_CORNER_OFFSET[i][2]);
					if (corners[i])
						cube_idx |= (1 << i);
				}
				int edge_mask = MC_EDGE_TABLE[cube_idx];
				if (edge_mask == 0)
					continue;

				// Emit triangles per tri_table.
				const int *tris = MC_TRI_TABLE[cube_idx];
				for (int t = 0; tris[t] != -1; t += 3) {
					int e0 = tris[t], e1 = tris[t + 1], e2 = tris[t + 2];
					int i0, i1, i2;
					auto emit_edge = [&](int eid, int &out) {
						if (edge_mask & (1 << eid)) {
							int a = MC_EDGE_CORNERS[eid][0];
							int b = MC_EDGE_CORNERS[eid][1];
							int ax = x + MC_CORNER_OFFSET[a][0];
							int ay = y + MC_CORNER_OFFSET[a][1];
							int az = z + MC_CORNER_OFFSET[a][2];
							int bx = x + MC_CORNER_OFFSET[b][0];
							int by = y + MC_CORNER_OFFSET[b][1];
							int bz = z + MC_CORNER_OFFSET[b][2];
							out = set_edge_vertex(eid, x, y, z,
												  (ax + bx) * 0.5f,
												  (ay + by) * 0.5f,
												  (az + bz) * 0.5f);
						}
					};
					emit_edge(e0, i0);
					emit_edge(e1, i1);
					emit_edge(e2, i2);
					if (i0 >= 0 && i1 >= 0 && i2 >= 0) {
						mesh.indices.push_back((uint32_t)i0);
						mesh.indices.push_back((uint32_t)i1);
						mesh.indices.push_back((uint32_t)i2);
					}
				}
			}
		}
	}
	return mesh;
}

// ====================== Benchmark harness ======================

struct BenchResult {
	std::string scene;
	std::string algo;
	Stats stats;
	size_t triangles;
	size_t memory_bytes;
};

template <typename Algo, typename Scene>
BenchResult run_bench(const std::string &scene_name, const std::string &algo_name,
					  Algo algo, Scene scene_gen, int warmup = 30, int iters = 1000)
{
	VoxelBuffer voxels = scene_gen();
	MeshData mesh;

	// Warmup
	for (int i = 0; i < warmup; i++) {
		mesh = algo(voxels);
	}

	// Timed iterations
	std::vector<double> samples;
	samples.reserve(iters);
	for (int i = 0; i < iters; i++) {
		auto t0 = std::chrono::high_resolution_clock::now();
		mesh = algo(voxels);
		auto t1 = std::chrono::high_resolution_clock::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
		samples.push_back(ns);
	}

	BenchResult r;
	r.scene = scene_name;
	r.algo = algo_name;
	r.stats = compute_stats(std::move(samples));
	r.triangles = mesh.triangle_count();
	r.memory_bytes = mesh.memory_bytes();
	return r;
}

// ====================== Main ======================

struct AlgoEntry {
	const char *name;
	MeshData (*fn)(const VoxelBuffer &);
};

struct SceneEntry {
	const char *name;
	VoxelBuffer (*fn)();
};

static const AlgoEntry ALGOS[] = {
	{"naive_greedy", algo_naive_greedy},
	{"surface_nets", algo_surface_nets},
	{"dual_contouring", algo_dual_contouring},
	{"marching_cubes", algo_marching_cubes}};
static const SceneEntry SCENES[] = {
	{"solid_cube", scene_solid_cube},
	{"hollow_shell", scene_hollow_shell},
	{"sphere", scene_sphere},
	{"layered_terrain", scene_layered_terrain},
	{"sparse_random", scene_sparse_random},
	{"projectv_mix", scene_projectv_mix}};

static std::vector<std::string> split_csv(const std::string &s)
{
	std::vector<std::string> out;
	size_t i = 0, j;
	while (i < s.size()) {
		j = s.find(',', i);
		if (j == std::string::npos)
			j = s.size();
		out.emplace_back(s.substr(i, j - i));
		i = j + 1;
	}
	return out;
}

static bool contains(const std::vector<std::string> &v, const std::string &s)
{
	for (auto &x : v)
		if (x == s)
			return true;
	return false;
}

int main(int argc, char **argv)
{
	int warmup = 30, iters = 1000;
	bool run_all = true;
	std::vector<std::string> sel_scenes, sel_algos;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		if (a == "--all") {
			run_all = true;
		} else if (a.rfind("--scene=", 0) == 0) {
			sel_scenes = split_csv(a.substr(8));
			run_all = false;
		} else if (a.rfind("--algo=", 0) == 0) {
			sel_algos = split_csv(a.substr(7));
			run_all = false;
		} else if (a.rfind("--iters=", 0) == 0) {
			iters = std::atoi(a.substr(8).c_str());
		} else if (a.rfind("--warmup=", 0) == 0) {
			warmup = std::atoi(a.substr(9).c_str());
		} else if (a == "--help" || a == "-h") {
			printf("Usage: %s [--all] [--scene=s1,s2,...] [--algo=a1,a2,...] [--iters=N] [--warmup=N]\n", argv[0]);
			printf("Scenes: solid_cube, hollow_shell, sphere, layered_terrain, sparse_random, projectv_mix\n");
			printf("Algos:  naive_greedy, surface_nets, dual_contouring, marching_cubes\n");
			return 0;
		}
	}

	printf("# Meshing algorithm comparison — 2026-06-20-meshing-algo-comparison\n");
	printf("# Chunk: %d^3 = %d voxels\n", CHUNK, CHUNK_VOL);
	printf("# Algorithms: naive_greedy, surface_nets (naive), dual_contouring (simplified), marching_cubes (standard table)\n");
	printf("# Scenes: solid_cube, hollow_shell, sphere, layered_terrain, sparse_random, projectv_mix\n");
	printf("# Per benchmarks/methodology.md: warmup=%d, iters=%d, mean/median/p95/p99/std\n", warmup, iters);
	printf("# Hardware baseline: AMD Ryzen 7 5800X, Clang 22.1.6, -O3 -march=native -DNDEBUG\n");
	printf("\n");

	printf("scene,algo,triangles,memory_bytes,mean_ns,median_ns,p95_ns,p99_ns,stddev_ns,min_ns,max_ns\n");

	std::vector<BenchResult> results;
	for (const auto &sc : SCENES) {
		if (!run_all && !contains(sel_scenes, sc.name))
			continue;
		for (const auto &al : ALGOS) {
			if (!run_all && !contains(sel_algos, al.name))
				continue;
			results.push_back(run_bench(sc.name, al.name, al.fn, sc.fn, warmup, iters));
		}
	}

	// Output CSV
	for (auto &r : results) {
		printf("%s,%s,%zu,%zu,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n",
			   r.scene.c_str(), r.algo.c_str(),
			   r.triangles, r.memory_bytes,
			   r.stats.mean_ns, r.stats.median_ns,
			   r.stats.p95_ns, r.stats.p99_ns, r.stats.stddev_ns,
			   r.stats.min_ns, r.stats.max_ns);
	}

	printf("\n# Done.\n");
	return 0;
}
