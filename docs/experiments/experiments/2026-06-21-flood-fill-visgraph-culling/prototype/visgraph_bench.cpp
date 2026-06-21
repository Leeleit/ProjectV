#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <print>
#include <queue>
#include <random>
#include <span>
#include <vector>

constexpr int CHUNK_8 = 8;
constexpr int CHUNK_16 = 16;
constexpr int VOXELS_8 = CHUNK_8 * CHUNK_8 * CHUNK_8;   // 512
constexpr int VOXELS_16 = CHUNK_16 * CHUNK_16 * CHUNK_16; // 4096
constexpr int SEEDS = 5;
constexpr int ITER = 500;

enum Face : int { NEG_X, POS_X, NEG_Y, POS_Y, NEG_Z, POS_Z, FACE_COUNT };

static int face_mask(Face f) { return 1 << f; }

struct VisGraph {
    int size;
    uint64_t matrix; // 6 bits: reachable[src_face*6+dst_face]
    double flood_us;
    int non_opaque;

    VisGraph() : size(0), matrix(0), flood_us(0), non_opaque(0) {}
};

// 1D index from 3D
static int idx3(int x, int y, int z, int s) { return (y * s + z) * s + x; }

// minecraft 1.12 visgraph flood-fill
// starting from each non-opaque block on a face, BFS through non-opaque,
// record which faces the flood can exit through.
static VisGraph compute_visgraph(std::span<const uint8_t> chunk, int sz) {
    VisGraph vg;
    vg.size = sz;
    int voxels = sz * sz * sz;
    std::vector<int> visited(voxels, -1);
    std::vector<uint8_t> opaque(voxels);
    vg.non_opaque = 0;
    for (int i = 0; i < voxels; ++i) {
        opaque[i] = chunk[i];
        if (!chunk[i]) vg.non_opaque++;
    }

    auto t0 = std::chrono::steady_clock::now();

    // for each non-opaque block on each face, start a BFS flood-fill
    // We iterate over all 6 faces
    for (int face = 0; face < 6; ++face) {
        // generate start positions for this face
        std::vector<int> starts;
        int sx = 0, sy = 0, szz = 0, dx = 0, dy = 0, dz = 0;
        switch (face) {
            case NEG_X: sx = 0;      dx = 0; dy = 1; dz = 1; break;
            case POS_X: sx = sz-1;   dx = 0; dy = 1; dz = 1; break;
            case NEG_Y: sy = 0;      dx = 1; dy = 0; dz = 1; break;
            case POS_Y: sy = sz-1;   dx = 1; dy = 0; dz = 1; break;
            case NEG_Z: szz = 0;     dx = 1; dy = 1; dz = 0; break;
            case POS_Z: szz = sz-1;  dx = 1; dy = 1; dz = 0; break;
        }

        auto add_start = [&](int x, int y, int z) {
            if (x >= 0 && x < sz && y >= 0 && y < sz && z >= 0 && z < sz) {
                if (!opaque[idx3(x, y, z, sz)])
                    starts.push_back(idx3(x, y, z, sz));
            }
        };

        if (dx == 0 && dy == 1 && dz == 1) { // X faces: iterate Y,Z
            for (int y = 0; y < sz; ++y)
                for (int z = 0; z < sz; ++z) add_start(sx, y, z);
        } else if (dx == 1 && dy == 0 && dz == 1) { // Y faces
            for (int x = 0; x < sz; ++x)
                for (int z = 0; z < sz; ++z) add_start(x, sy, z);
        } else { // Z faces
            for (int x = 0; x < sz; ++x)
                for (int y = 0; y < sz; ++y) add_start(x, y, szz);
        }

        for (int start : starts) {
            if (visited[start] == face) continue; // already covered by this face
            // BFS
            std::queue<int> q;
            q.push(start);
            visited[start] = face;
            uint8_t reached_faces = 0;

            while (!q.empty()) {
                int cur = q.front(); q.pop();
                int zz = cur / (sz * sz);
                int rem = cur % (sz * sz);
                int yy = rem / sz;
                int xx = rem % sz;

                // check 6 neighbors
                static const int ndx[] = {-1,1,0,0,0,0};
                static const int ndy[] = {0,0,-1,1,0,0};
                static const int ndz[] = {0,0,0,0,-1,1};
                for (int d = 0; d < 6; ++d) {
                    int nx = xx + ndx[d];
                    int ny = yy + ndy[d];
                    int nz = zz + ndz[d];

                    // exited chunk through a face
                    if (nx < 0) { reached_faces |= face_mask(NEG_X); continue; }
                    if (nx >= sz) { reached_faces |= face_mask(POS_X); continue; }
                    if (ny < 0) { reached_faces |= face_mask(NEG_Y); continue; }
                    if (ny >= sz) { reached_faces |= face_mask(POS_Y); continue; }
                    if (nz < 0) { reached_faces |= face_mask(NEG_Z); continue; }
                    if (nz >= sz) { reached_faces |= face_mask(POS_Z); continue; }

                    int nidx = idx3(nx, ny, nz, sz);
                    if (opaque[nidx]) continue;
                    if (visited[nidx] == face) continue;
                    visited[nidx] = face;
                    q.push(nidx);
                }
            }

            // connect all reached faces
            for (int a = 0; a < 6; ++a) {
                if (!(reached_faces & (1 << a))) continue;
                for (int b = a + 1; b < 6; ++b) {
                    if (!(reached_faces & (1 << b))) continue;
                    int src = std::min(face, a);
                    int dst = std::max(face, a);
                    vg.matrix |= 1ULL << (src * 6 + dst);
                    src = std::min(face, b);
                    dst = std::max(face, b);
                    vg.matrix |= 1ULL << (src * 6 + dst);
                }
            }
            // connect entry face to all reached faces
            for (int a = 0; a < 6; ++a) {
                if (!(reached_faces & (1 << a))) continue;
                int src = std::min(face, a);
                int dst = std::max(face, a);
                vg.matrix |= 1ULL << (src * 6 + dst);
            }
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    vg.flood_us = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1000.0;
    return vg;
}

// scenes
struct Scene {
    const char *name;
    std::vector<uint8_t> data8;
    std::vector<uint8_t> data16;
    int desc;
};

static Scene make_scene(const char *name, std::span<const std::pair<int,int>> spans_8, std::span<const std::pair<int,int>> spans_16, int desc) {
    Scene s;
    s.name = name;
    s.data8.resize(VOXELS_8, 0);
    s.data16.resize(VOXELS_16, 0);
    s.desc = desc;

    auto fill = [](std::span<uint8_t> d, int sz, std::span<const std::pair<int,int>> sp) {
        for (auto [start, len] : sp) {
            for (int i = 0; i < len && start + i < sz*sz*sz; ++i)
                d[start + i] = 1;
        }
    };
    fill(s.data8, CHUNK_8, spans_8);
    fill(s.data16, CHUNK_16, spans_16);
    return s;
}

// utility: random opaque blocks with density
static Scene make_random_scene(const char *name, int sz, double density, int desc) {
    Scene s;
    s.name = name;
    s.desc = desc;
    int voxels = sz * sz * sz;
    if (sz == 8) {
        s.data8.resize(voxels, 0);
        s.data16.resize(VOXELS_16, 0);
        std::mt19937 rng(42);
        for (int i = 0; i < voxels; ++i)
            s.data8[i] = (rng() < density * 4294967295.0) ? 1 : 0;
    } else {
        s.data8.resize(VOXELS_8, 0);
        s.data16.resize(voxels, 0);
        std::mt19937 rng(42);
        for (int i = 0; i < voxels; ++i)
            s.data16[i] = (rng() < density * 4294967295.0) ? 1 : 0;
    }
    return s;
}

int main() {
    // 2D scenes for reference:
    // open_plane: all air
    // cave_network: BFS-friendly tunnels (random 30% density opaque)
    // dense_cave: 50% opaque
    // nearly_solid: 80% opaque
    // full_solid: all opaque
    std::vector<Scene> scenes;
    scenes.push_back(make_random_scene("open_plane", CHUNK_8, 0.0, 0));
    scenes.push_back(make_random_scene("cave_network", CHUNK_8, 0.3, 1));
    scenes.push_back(make_random_scene("dense_cave", CHUNK_8, 0.5, 2));
    scenes.push_back(make_random_scene("nearly_solid", CHUNK_8, 0.8, 3));
    scenes.push_back(make_random_scene("full_solid", CHUNK_8, 1.0, 4));

    // also 16³ for comparison
    std::vector<Scene> scenes16;
    scenes16.push_back(make_random_scene("open_plane_16", CHUNK_16, 0.0, 0));
    scenes16.push_back(make_random_scene("cave_network_16", CHUNK_16, 0.3, 1));
    scenes16.push_back(make_random_scene("dense_cave_16", CHUNK_16, 0.5, 2));
    scenes16.push_back(make_random_scene("nearly_solid_16", CHUNK_16, 0.8, 3));
    scenes16.push_back(make_random_scene("full_solid_16", CHUNK_16, 1.0, 4));

    auto out = std::ofstream("results.csv");
    out << "scene,size,opaque_pct,seed,flood_us,matrix,non_opaque\n";

    auto bench_scenes = [&](auto &ss, int sz) {
        for (auto &sc : ss) {
            auto &data = (sz == 8) ? sc.data8 : sc.data16;
            int opaque_count = 0;
            for (auto v : data) if (v) opaque_count++;
            double opaque_pct = 100.0 * opaque_count / data.size();

            for (int seed = 0; seed < SEEDS; ++seed) {
                // re-seed deterministic variation
                std::mt19937 rng(seed);
                std::vector<uint8_t> d2 = data;
                if (seed > 0) {
                    // perturb 5% of blocks
                    int changes = std::max(1, (int)(d2.size() * 0.05));
                    for (int c = 0; c < changes; ++c) {
                        int p = rng() % d2.size();
                        d2[p] = d2[p] ? 0 : 1;
                    }
                }

                double total_us = 0;
                uint64_t last_matrix = 0;
                int last_nonopaque = 0;
                for (int iter = 0; iter < ITER; ++iter) {
                    auto vg = compute_visgraph(d2, sz);
                    total_us += vg.flood_us;
                    last_matrix = vg.matrix;
                    last_nonopaque = vg.non_opaque;
                }
                out << std::format("{},{},{:.1f},{},{:.3f},{},{}\n",
                    sc.name, sz, opaque_pct, seed, total_us / ITER, last_matrix, last_nonopaque);
            }
        }
    };

    bench_scenes(scenes, CHUNK_8);
    bench_scenes(scenes16, CHUNK_16);

    out.close();
    std::print("done. results written to results.csv\n");
    return 0;
}
