#include <iostream>
#include <vector>
#include <queue>
#include <chrono>
#include <random>
#include <algorithm>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

using namespace std;

// Union-Find (DSU) helper
class DSU {
public:
    vector<int> parent;
    vector<int> rank;

    DSU(int n) {
        parent.resize(n);
        rank.resize(n, 0);
        for (int i = 0; i < n; ++i) {
            parent[i] = i;
        }
    }

    int find(int i) {
        if (parent[i] == i)
            return i;
        return parent[i] = find(parent[i]); // Path compression
    }

    bool unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) {
            if (rank[root_i] < rank[root_j]) {
                parent[root_i] = root_j;
            } else if (rank[root_i] > rank[root_j]) {
                parent[root_j] = root_i;
            } else {
                parent[root_j] = root_i;
                rank[root_i]++;
            }
            return true;
        }
        return false;
    }
};

const int WORLD_SIZE = 32;
const int CHUNK_SIZE = 8;
const int CHUNKS_PER_AXIS = WORLD_SIZE / CHUNK_SIZE; // 4
const int NUM_CHUNKS = CHUNKS_PER_AXIS * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS; // 64
const int VOXELS_PER_CHUNK = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE; // 512

enum VoxelType : uint8_t {
    VOXEL_AIR = 0,
    VOXEL_WOOD = 1,
    VOXEL_CONCRETE = 2,
    VOXEL_STEEL = 3,
    VOXEL_GROUND = 4
};

struct VoxelProperties {
    int mass;
    int max_load;
};

const VoxelProperties PROPERTIES[] = {
    {0, 0},        // Air
    {10, 40},      // Wood
    {20, 120},     // Concrete
    {50, 300},     // Steel
    {0, 9999999}   // Ground
};

struct Coords {
    int x, y, z;
};

inline int get_voxel_index(int x, int y, int z) {
    return x * WORLD_SIZE * WORLD_SIZE + y * WORLD_SIZE + z;
}

inline Coords get_voxel_coords(int idx) {
    int x = idx / (WORLD_SIZE * WORLD_SIZE);
    int rem = idx % (WORLD_SIZE * WORLD_SIZE);
    int y = rem / WORLD_SIZE;
    int z = rem % WORLD_SIZE;
    return {x, y, z};
}

inline int get_chunk_id(int x, int y, int z) {
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    int cz = z / CHUNK_SIZE;
    return cx * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + cy * CHUNKS_PER_AXIS + cz;
}

struct ChunkTopology {
    int local_labels[VOXELS_PER_CHUNK];
    int num_components;
};

// Compute local CCL for a single chunk using 6-connectivity
void compute_local_ccl(int chunk_id, const uint8_t* world_voxels, ChunkTopology& topo) {
    int cx = (chunk_id / (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) * CHUNK_SIZE;
    int cy = ((chunk_id % (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) / CHUNKS_PER_AXIS) * CHUNK_SIZE;
    int cz = (chunk_id % CHUNKS_PER_AXIS) * CHUNK_SIZE;

    DSU dsu(VOXELS_PER_CHUNK);
    
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int gx = cx + lx;
                int gy = cy + ly;
                int gz = cz + lz;
                int g_idx = get_voxel_index(gx, gy, gz);
                int l_idx = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                
                if (world_voxels[g_idx] == VOXEL_AIR) {
                    topo.local_labels[l_idx] = -1;
                    continue;
                }
                
                if (lx > 0) {
                    int neighbor_l_idx = (lx - 1) * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                    int neighbor_g_idx = get_voxel_index(cx + lx - 1, gy, gz);
                    if (world_voxels[neighbor_g_idx] != VOXEL_AIR) {
                        dsu.unite(l_idx, neighbor_l_idx);
                    }
                }
                if (ly > 0) {
                    int neighbor_l_idx = lx * CHUNK_SIZE * CHUNK_SIZE + (ly - 1) * CHUNK_SIZE + lz;
                    int neighbor_g_idx = get_voxel_index(gx, cy + ly - 1, gz);
                    if (world_voxels[neighbor_g_idx] != VOXEL_AIR) {
                        dsu.unite(l_idx, neighbor_l_idx);
                    }
                }
                if (lz > 0) {
                    int neighbor_l_idx = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + (lz - 1);
                    int neighbor_g_idx = get_voxel_index(gx, gy, cz + lz - 1);
                    if (world_voxels[neighbor_g_idx] != VOXEL_AIR) {
                        dsu.unite(l_idx, neighbor_l_idx);
                    }
                }
            }
        }
    }
    
    unordered_map<int, int> root_to_comp;
    topo.num_components = 0;
    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
            for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                int l_idx = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                int gx = cx + lx;
                int gy = cy + ly;
                int gz = cz + lz;
                int g_idx = get_voxel_index(gx, gy, gz);
                
                if (world_voxels[g_idx] == VOXEL_AIR) {
                    topo.local_labels[l_idx] = -1;
                    continue;
                }
                
                int root = dsu.find(l_idx);
                if (root_to_comp.find(root) == root_to_comp.end()) {
                    root_to_comp[root] = topo.num_components++;
                }
                topo.local_labels[l_idx] = root_to_comp[root];
            }
        }
    }
}

// Build global DSU mapping components across chunks
void build_global_dsu(const uint8_t* world_voxels, const ChunkTopology* chunk_topologies, DSU& global_dsu) {
    int ground_node = NUM_CHUNKS * 256;
    
    // Merge components along boundaries
    for (int cx = 0; cx < CHUNKS_PER_AXIS; ++cx) {
        for (int cy = 0; cy < CHUNKS_PER_AXIS; ++cy) {
            for (int cz = 0; cz < CHUNKS_PER_AXIS; ++cz) {
                int chunk_id = cx * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + cy * CHUNKS_PER_AXIS + cz;
                
                // 1. Boundary +X
                if (cx < CHUNKS_PER_AXIS - 1) {
                    int neighbor_chunk = (cx + 1) * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + cy * CHUNKS_PER_AXIS + cz;
                    int lx1 = CHUNK_SIZE - 1;
                    int lx2 = 0;
                    for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                            int gx1 = cx * CHUNK_SIZE + lx1;
                            int gy = cy * CHUNK_SIZE + ly;
                            int gz = cz * CHUNK_SIZE + lz;
                            int gx2 = (cx + 1) * CHUNK_SIZE + lx2;
                            
                            int g_idx1 = get_voxel_index(gx1, gy, gz);
                            int g_idx2 = get_voxel_index(gx2, gy, gz);
                            
                            if (world_voxels[g_idx1] != VOXEL_AIR && world_voxels[g_idx2] != VOXEL_AIR) {
                                int l_idx1 = lx1 * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                                int l_idx2 = lx2 * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                                
                                int comp1 = chunk_topologies[chunk_id].local_labels[l_idx1];
                                int comp2 = chunk_topologies[neighbor_chunk].local_labels[l_idx2];
                                
                                if (comp1 != -1 && comp2 != -1) {
                                    global_dsu.unite(chunk_id * 256 + comp1, neighbor_chunk * 256 + comp2);
                                }
                            }
                        }
                    }
                }
                
                // 2. Boundary +Y
                if (cy < CHUNKS_PER_AXIS - 1) {
                    int neighbor_chunk = cx * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + (cy + 1) * CHUNKS_PER_AXIS + cz;
                    int ly1 = CHUNK_SIZE - 1;
                    int ly2 = 0;
                    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                            int gx = cx * CHUNK_SIZE + lx;
                            int gy1 = cy * CHUNK_SIZE + ly1;
                            int gz = cz * CHUNK_SIZE + lz;
                            int gy2 = (cy + 1) * CHUNK_SIZE + ly2;
                            
                            int g_idx1 = get_voxel_index(gx, gy1, gz);
                            int g_idx2 = get_voxel_index(gx, gy2, gz);
                            
                            if (world_voxels[g_idx1] != VOXEL_AIR && world_voxels[g_idx2] != VOXEL_AIR) {
                                int l_idx1 = lx * CHUNK_SIZE * CHUNK_SIZE + ly1 * CHUNK_SIZE + lz;
                                int l_idx2 = lx * CHUNK_SIZE * CHUNK_SIZE + ly2 * CHUNK_SIZE + lz;
                                
                                int comp1 = chunk_topologies[chunk_id].local_labels[l_idx1];
                                int comp2 = chunk_topologies[neighbor_chunk].local_labels[l_idx2];
                                
                                if (comp1 != -1 && comp2 != -1) {
                                    global_dsu.unite(chunk_id * 256 + comp1, neighbor_chunk * 256 + comp2);
                                }
                            }
                        }
                    }
                }
                
                // 3. Boundary +Z
                if (cz < CHUNKS_PER_AXIS - 1) {
                    int neighbor_chunk = cx * CHUNKS_PER_AXIS * CHUNKS_PER_AXIS + cy * CHUNKS_PER_AXIS + (cz + 1);
                    int lz1 = CHUNK_SIZE - 1;
                    int lz2 = 0;
                    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
                        for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                            int gx = cx * CHUNK_SIZE + lx;
                            int gy = cy * CHUNK_SIZE + ly;
                            int gz1 = cz * CHUNK_SIZE + lz1;
                            int gz2 = (cz + 1) * CHUNK_SIZE + lz2;
                            
                            int g_idx1 = get_voxel_index(gx, gy, gz1);
                            int g_idx2 = get_voxel_index(gx, gy, gz2);
                            
                            if (world_voxels[g_idx1] != VOXEL_AIR && world_voxels[g_idx2] != VOXEL_AIR) {
                                int l_idx1 = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz1;
                                int l_idx2 = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz2;
                                
                                int comp1 = chunk_topologies[chunk_id].local_labels[l_idx1];
                                int comp2 = chunk_topologies[neighbor_chunk].local_labels[l_idx2];
                                
                                if (comp1 != -1 && comp2 != -1) {
                                    global_dsu.unite(chunk_id * 256 + comp1, neighbor_chunk * 256 + comp2);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Connect VOXEL_GROUND containing components to ground_node
    for (int chunk_id = 0; chunk_id < NUM_CHUNKS; ++chunk_id) {
        int cx = (chunk_id / (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) * CHUNK_SIZE;
        int cy = ((chunk_id % (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) / CHUNKS_PER_AXIS) * CHUNK_SIZE;
        int cz = (chunk_id % CHUNKS_PER_AXIS) * CHUNK_SIZE;
        
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                    int gx = cx + lx;
                    int gy = cy + ly;
                    int gz = cz + lz;
                    int g_idx = get_voxel_index(gx, gy, gz);
                    
                    if (world_voxels[g_idx] == VOXEL_GROUND) {
                        int l_idx = lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz;
                        int comp = chunk_topologies[chunk_id].local_labels[l_idx];
                        if (comp != -1) {
                            global_dsu.unite(chunk_id * 256 + comp, ground_node);
                        }
                    }
                }
            }
        }
    }
}

// Strategy A: Naive Global BFS
vector<int> run_naive_global_bfs(const uint8_t* world_voxels) {
    vector<bool> visited(WORLD_SIZE * WORLD_SIZE * WORLD_SIZE, false);
    queue<int> q;
    
    // Find all ground anchors as roots
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        if (world_voxels[i] == VOXEL_GROUND) {
            q.push(i);
            visited[i] = true;
        }
    }
    
    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        
        Coords c = get_voxel_coords(curr);
        int dx[] = {-1, 1, 0, 0, 0, 0};
        int dy[] = {0, 0, -1, 1, 0, 0};
        int dz[] = {0, 0, 0, 0, -1, 1};
        
        for (int i = 0; i < 6; ++i) {
            int nx = c.x + dx[i];
            int ny = c.y + dy[i];
            int nz = c.z + dz[i];
            
            if (nx >= 0 && nx < WORLD_SIZE && ny >= 0 && ny < WORLD_SIZE && nz >= 0 && nz < WORLD_SIZE) {
                int n_idx = get_voxel_index(nx, ny, nz);
                if (world_voxels[n_idx] != VOXEL_AIR && !visited[n_idx]) {
                    visited[n_idx] = true;
                    q.push(n_idx);
                }
            }
        }
    }
    
    // All solid voxels that were NOT visited are marked as collapsed
    vector<int> collapsed;
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        if (world_voxels[i] != VOXEL_AIR && !visited[i]) {
            collapsed.push_back(i);
        }
    }
    return collapsed;
}

// Strategy B: Hierarchical DSU Check
vector<int> run_hierarchical_dsu(const uint8_t* world_voxels, const ChunkTopology* chunk_topologies) {
    DSU global_dsu(NUM_CHUNKS * 256 + 1);
    build_global_dsu(world_voxels, chunk_topologies, global_dsu);
    
    int ground_node = NUM_CHUNKS * 256;
    vector<int> collapsed;
    
    for (int chunk_id = 0; chunk_id < NUM_CHUNKS; ++chunk_id) {
        int cx = (chunk_id / (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) * CHUNK_SIZE;
        int cy = ((chunk_id % (CHUNKS_PER_AXIS * CHUNKS_PER_AXIS)) / CHUNKS_PER_AXIS) * CHUNK_SIZE;
        int cz = (chunk_id % CHUNKS_PER_AXIS) * CHUNK_SIZE;
        
        for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
            for (int ly = 0; ly < CHUNK_SIZE; ++ly) {
                for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
                    int gx = cx + lx;
                    int gy = cy + ly;
                    int gz = cz + lz;
                    int g_idx = get_voxel_index(gx, gy, gz);
                    
                    if (world_voxels[g_idx] != VOXEL_AIR && world_voxels[g_idx] != VOXEL_GROUND) {
                        int comp = chunk_topologies[chunk_id].local_labels[lx * CHUNK_SIZE * CHUNK_SIZE + ly * CHUNK_SIZE + lz];
                        if (comp != -1) {
                            int g_comp = chunk_id * 256 + comp;
                            if (global_dsu.find(g_comp) != global_dsu.find(ground_node)) {
                                collapsed.push_back(g_idx);
                            }
                        }
                    }
                }
            }
        }
    }
    return collapsed;
}

// Strategy C: Local Split BFS from neighbor points of mutated voxel
vector<int> run_local_split_bfs(int deleted_idx, const uint8_t* world_voxels) {
    Coords dc = get_voxel_coords(deleted_idx);
    int dx[] = {-1, 1, 0, 0, 0, 0};
    int dy[] = {0, 0, -1, 1, 0, 0};
    int dz[] = {0, 0, 0, 0, -1, 1};
    
    unordered_set<int> known_stable;
    unordered_set<int> known_collapsed;
    vector<int> collapsed_result;
    
    // Find all ground anchors to seed known stables
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        if (world_voxels[i] == VOXEL_GROUND) {
            known_stable.insert(i);
        }
    }
    
    for (int i = 0; i < 6; ++i) {
        int nx = dc.x + dx[i];
        int ny = dc.y + dy[i];
        int nz = dc.z + dz[i];
        
        if (nx >= 0 && nx < WORLD_SIZE && ny >= 0 && ny < WORLD_SIZE && nz >= 0 && nz < WORLD_SIZE) {
            int n_idx = get_voxel_index(nx, ny, nz);
            if (world_voxels[n_idx] != VOXEL_AIR && known_stable.find(n_idx) == known_stable.end() && known_collapsed.find(n_idx) == known_collapsed.end()) {
                // Run a BFS to check if this neighbor is connected to Ground
                vector<int> visited_nodes;
                vector<int> q;
                unordered_set<int> visited;
                
                q.push_back(n_idx);
                visited.insert(n_idx);
                int head = 0;
                bool is_stable = false;
                
                while (head < (int)q.size()) {
                    int curr = q[head++];
                    visited_nodes.push_back(curr);
                    
                    if (known_stable.count(curr)) {
                        is_stable = true;
                        break;
                    }
                    
                    Coords c = get_voxel_coords(curr);
                    for (int j = 0; j < 6; ++j) {
                        int nnx = c.x + dx[j];
                        int nny = c.y + dy[j];
                        int nnz = c.z + dz[j];
                        
                        if (nnx >= 0 && nnx < WORLD_SIZE && nny >= 0 && nny < WORLD_SIZE && nnz >= 0 && nnz < WORLD_SIZE) {
                            int nn_idx = get_voxel_index(nnx, nny, nnz);
                            if (world_voxels[nn_idx] != VOXEL_AIR && visited.find(nn_idx) == visited.end()) {
                                visited.insert(nn_idx);
                                q.push_back(nn_idx);
                            }
                        }
                    }
                }
                
                if (is_stable) {
                    for (int node : visited_nodes) {
                        known_stable.insert(node);
                    }
                } else {
                    for (int node : visited_nodes) {
                        known_collapsed.insert(node);
                        collapsed_result.push_back(node);
                    }
                }
            }
        }
    }
    
    // Sort to match baseline validation
    sort(collapsed_result.begin(), collapsed_result.end());
    return collapsed_result;
}

// Strategy D: Stress Propagation
// Returns collapsed voxels where stress > max_load
vector<int> run_stress_propagation(const uint8_t* world_voxels, int iterations) {
    vector<float> loads(WORLD_SIZE * WORLD_SIZE * WORLD_SIZE, 0.0f);
    
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        loads[i] = PROPERTIES[world_voxels[i]].mass;
    }
    
    for (int iter = 0; iter < iterations; ++iter) {
        for (int y = WORLD_SIZE - 1; y >= 0; --y) {
            for (int x = 0; x < WORLD_SIZE; ++x) {
                for (int z = 0; z < WORLD_SIZE; ++z) {
                    int g_idx = get_voxel_index(x, y, z);
                    if (world_voxels[g_idx] == VOXEL_AIR || world_voxels[g_idx] == VOXEL_GROUND) {
                        continue;
                    }
                    
                    float cur_load = loads[g_idx];
                    if (cur_load <= 0.0f) continue;
                    
                    // Distribute to supports below or sideways
                    int support_indices[6];
                    int num_supports = 0;
                    
                    // Downward support
                    if (y > 0) {
                        int down_idx = get_voxel_index(x, y - 1, z);
                        if (world_voxels[down_idx] != VOXEL_AIR) {
                            support_indices[num_supports++] = down_idx;
                        }
                    }
                    
                    // Sideways supports
                    int dx[] = {-1, 1, 0, 0};
                    int dz[] = {0, 0, -1, 1};
                    for (int i = 0; i < 4; ++i) {
                        int nx = x + dx[i];
                        int nz = z + dz[i];
                        if (nx >= 0 && nx < WORLD_SIZE && nz >= 0 && nz < WORLD_SIZE) {
                            int side_idx = get_voxel_index(nx, y, nz);
                            if (world_voxels[side_idx] != VOXEL_AIR) {
                                support_indices[num_supports++] = side_idx;
                            }
                        }
                    }
                    
                    if (num_supports > 0) {
                        bool has_down = (y > 0 && world_voxels[get_voxel_index(x, y - 1, z)] != VOXEL_AIR);
                        if (has_down && num_supports > 1) {
                            int down_idx = get_voxel_index(x, y - 1, z);
                            loads[down_idx] += cur_load * 0.8f;
                            float side_share = (cur_load * 0.2f) / (num_supports - 1);
                            for (int i = 0; i < num_supports; ++i) {
                                if (support_indices[i] != down_idx) {
                                    loads[support_indices[i]] += side_share;
                                }
                            }
                        } else {
                            float share = cur_load / num_supports;
                            for (int i = 0; i < num_supports; ++i) {
                                loads[support_indices[i]] += share;
                            }
                        }
                    }
                }
            }
        }
    }
    
    vector<int> collapsed;
    for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
        if (world_voxels[i] != VOXEL_AIR && world_voxels[i] != VOXEL_GROUND) {
            if (loads[i] > PROPERTIES[world_voxels[i]].max_load) {
                collapsed.push_back(i);
            }
        }
    }
    return collapsed;
}

// Strategy E: Hybrid bounded AABB check
vector<int> run_hybrid_aabb(int deleted_idx, const uint8_t* world_voxels, const ChunkTopology* chunk_topologies) {
    Coords dc = get_voxel_coords(deleted_idx);
    int ccx = dc.x / CHUNK_SIZE;
    int ccy = dc.y / CHUNK_SIZE;
    int ccz = dc.z / CHUNK_SIZE;
    
    // Bounds of 3x3x3 chunks AABB around the mutation
    int min_cx = max(0, ccx - 1);
    int max_cx = min(CHUNKS_PER_AXIS - 1, ccx + 1);
    int min_cy = max(0, ccy - 1);
    int max_cy = min(CHUNKS_PER_AXIS - 1, ccy + 1);
    int min_cz = max(0, ccz - 1);
    int max_cz = min(CHUNKS_PER_AXIS - 1, ccz + 1);
    
    int min_vx = min_cx * CHUNK_SIZE;
    int max_vx = (max_cx + 1) * CHUNK_SIZE - 1;
    int min_vy = min_cy * CHUNK_SIZE;
    int max_vy = (max_cy + 1) * CHUNK_SIZE - 1;
    int min_vz = min_cz * CHUNK_SIZE;
    int max_vz = (max_cz + 1) * CHUNK_SIZE - 1;
    
    // Run BFS strictly within these voxel bounds
    vector<int> dx = {-1, 1, 0, 0, 0, 0};
    vector<int> dy = {0, 0, -1, 1, 0, 0};
    vector<int> dz = {0, 0, 0, 0, -1, 1};
    
    unordered_set<int> visited;
    vector<int> q;
    
    // Neighbors of deleted voxel inside AABB
    for (int i = 0; i < 6; ++i) {
        int nx = dc.x + dx[i];
        int ny = dc.y + dy[i];
        int nz = dc.z + dz[i];
        
        if (nx >= min_vx && nx <= max_vx && ny >= min_vy && ny <= max_vy && nz >= min_vz && nz <= max_vz) {
            int n_idx = get_voxel_index(nx, ny, nz);
            if (world_voxels[n_idx] != VOXEL_AIR && visited.find(n_idx) == visited.end()) {
                q.push_back(n_idx);
                visited.insert(n_idx);
            }
        }
    }
    
    int head = 0;
    bool touched_aabb_boundary = false;
    bool found_ground = false;
    
    while (head < (int)q.size()) {
        int curr = q[head++];
        if (world_voxels[curr] == VOXEL_GROUND) {
            found_ground = true;
        }
        
        Coords c = get_voxel_coords(curr);
        
        // Check if we touch the bounds of our AABB
        if (c.x == min_vx || c.x == max_vx || c.y == min_vy || c.y == max_vy || c.z == min_vz || c.z == max_vz) {
            touched_aabb_boundary = true;
        }
        
        for (int i = 0; i < 6; ++i) {
            int nx = c.x + dx[i];
            int ny = c.y + dy[i];
            int nz = c.z + dz[i];
            
            if (nx >= min_vx && nx <= max_vx && ny >= min_vy && ny <= max_vy && nz >= min_vz && nz <= max_vz) {
                int n_idx = get_voxel_index(nx, ny, nz);
                if (world_voxels[n_idx] != VOXEL_AIR && visited.find(n_idx) == visited.end()) {
                    visited.insert(n_idx);
                    q.push_back(n_idx);
                }
            }
        }
    }
    
    // If we touched boundary and didn't find ground locally, fall back to global DSU (Strategy B)
    if (touched_aabb_boundary && !found_ground) {
        return run_hierarchical_dsu(world_voxels, chunk_topologies);
    }
    
    // Otherwise, we can determine stability locally.
    // Solid voxels within AABB that are not connected to ground (and didn't touch boundary) are collapsed.
    vector<int> collapsed;
    if (!found_ground) {
        // If ground was not found and we didn't touch boundary, all visited solids must fall
        for (int idx : q) {
            if (world_voxels[idx] != VOXEL_GROUND) {
                collapsed.push_back(idx);
            }
        }
    }
    sort(collapsed.begin(), collapsed.end());
    return collapsed;
}

// Scene Generation
void build_scene_small_house(uint8_t* voxels) {
    memset(voxels, VOXEL_AIR, WORLD_SIZE * WORLD_SIZE * WORLD_SIZE);
    
    // Ground anchor floor at y = 0..1
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int z = 0; z < WORLD_SIZE; ++z) {
            voxels[get_voxel_index(x, 0, z)] = VOXEL_GROUND;
            voxels[get_voxel_index(x, 1, z)] = VOXEL_GROUND;
        }
    }
    
    // House walls in y = 2..12, from x,z = 8..24 (concrete)
    for (int y = 2; y <= 12; ++y) {
        for (int x = 8; x <= 24; ++x) {
            for (int z = 8; z <= 24; ++z) {
                // Wall outline
                if (x == 8 || x == 24 || z == 8 || z == 24) {
                    // Door opening at x = 16, z = 8, y = 2..5
                    if (z == 8 && x >= 14 && x <= 18 && y >= 2 && y <= 6) {
                        continue;
                    }
                    // Window openings at side walls
                    if ((x == 8 || x == 24) && (z == 12 || z == 20) && (y >= 5 && y <= 8)) {
                        continue;
                    }
                    voxels[get_voxel_index(x, y, z)] = VOXEL_CONCRETE;
                }
            }
        }
    }
    
    // Ceiling/roof at y = 13..14 (wood)
    for (int x = 7; x <= 25; ++x) {
        for (int z = 7; z <= 25; ++z) {
            voxels[get_voxel_index(x, 13, z)] = VOXEL_WOOD;
            if (x >= 9 && x <= 23 && z >= 9 && z <= 23) {
                voxels[get_voxel_index(x, 14, z)] = VOXEL_WOOD;
            }
        }
    }
}

void build_scene_bridge(uint8_t* voxels) {
    memset(voxels, VOXEL_AIR, WORLD_SIZE * WORLD_SIZE * WORLD_SIZE);
    
    // Ground anchor floor at y = 0
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int z = 0; z < WORLD_SIZE; ++z) {
            voxels[get_voxel_index(x, 0, z)] = VOXEL_GROUND;
        }
    }
    
    // Two concrete pillars: left (x = 4..7) and right (x = 24..27) from y = 1 to 15
    for (int y = 1; y <= 15; ++y) {
        for (int z = 12; z <= 19; ++z) {
            for (int x = 4; x <= 7; ++x) {
                voxels[get_voxel_index(x, y, z)] = VOXEL_CONCRETE;
            }
            for (int x = 24; x <= 27; ++x) {
                voxels[get_voxel_index(x, y, z)] = VOXEL_CONCRETE;
            }
        }
    }
    
    // Steel span connecting pillars at y = 16..17, from x = 4 to 27
    for (int y = 16; y <= 17; ++y) {
        for (int z = 13; z <= 18; ++z) {
            for (int x = 4; x <= 27; ++x) {
                voxels[get_voxel_index(x, y, z)] = VOXEL_STEEL;
            }
        }
    }
}

void build_scene_tower(uint8_t* voxels) {
    memset(voxels, VOXEL_AIR, WORLD_SIZE * WORLD_SIZE * WORLD_SIZE);
    
    // Ground anchor at y = 0
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int z = 0; z < WORLD_SIZE; ++z) {
            voxels[get_voxel_index(x, 0, z)] = VOXEL_GROUND;
        }
    }
    
    // Vertical column at x = 12..19, z = 12..19, y = 1..31
    for (int y = 1; y < WORLD_SIZE; ++y) {
        for (int x = 12; x <= 19; ++x) {
            for (int z = 12; z <= 19; ++z) {
                // Steel core in center (14..17)
                if (x >= 14 && x <= 17 && z >= 14 && z <= 17) {
                    voxels[get_voxel_index(x, y, z)] = VOXEL_STEEL;
                } else {
                    voxels[get_voxel_index(x, y, z)] = VOXEL_CONCRETE;
                }
            }
        }
    }
}

void build_scene_stressed_arch(uint8_t* voxels) {
    memset(voxels, VOXEL_AIR, WORLD_SIZE * WORLD_SIZE * WORLD_SIZE);
    
    // Ground anchor floor at y = 0..1
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int z = 0; z < WORLD_SIZE; ++z) {
            voxels[get_voxel_index(x, 0, z)] = VOXEL_GROUND;
            voxels[get_voxel_index(x, 1, z)] = VOXEL_GROUND;
        }
    }
    
    // Arch spanning from x = 4 to 28 at z = 14..17
    // Equation of arch height: y = 2 + 12 * sin((x - 4) * PI / 24)
    for (int x = 4; x <= 28; ++x) {
        float angle = (x - 4) * M_PI / 24.0f;
        int arch_y = 2 + (int)(13.0f * sin(angle));
        
        for (int z = 14; z <= 17; ++z) {
            // Pillars touching ground
            if (x == 4 || x == 28) {
                for (int y = 2; y <= arch_y; ++y) {
                    voxels[get_voxel_index(x, y, z)] = VOXEL_CONCRETE;
                }
            } else {
                voxels[get_voxel_index(x, arch_y, z)] = VOXEL_CONCRETE;
                voxels[get_voxel_index(x, arch_y - 1, z)] = VOXEL_CONCRETE;
            }
        }
    }
}

void build_scene_random_scaffolding(uint8_t* voxels, unsigned int seed) {
    memset(voxels, VOXEL_AIR, WORLD_SIZE * WORLD_SIZE * WORLD_SIZE);
    mt19937 rng(seed);
    
    // Ground floor at y = 0
    for (int x = 0; x < WORLD_SIZE; ++x) {
        for (int z = 0; z < WORLD_SIZE; ++z) {
            voxels[get_voxel_index(x, 0, z)] = VOXEL_GROUND;
        }
    }
    
    // Create random vertical columns (steel)
    const int num_pillars = 12;
    vector<pair<int, int>> pillar_locs;
    for (int i = 0; i < num_pillars; ++i) {
        int px = uniform_int_distribution<int>(4, 27)(rng);
        int pz = uniform_int_distribution<int>(4, 27)(rng);
        pillar_locs.push_back({px, pz});
        
        int height = uniform_int_distribution<int>(15, 28)(rng);
        for (int y = 1; y <= height; ++y) {
            voxels[get_voxel_index(px, y, pz)] = VOXEL_STEEL;
            voxels[get_voxel_index(px+1, y, pz)] = VOXEL_STEEL;
            voxels[get_voxel_index(px, y, pz+1)] = VOXEL_STEEL;
        }
    }
    
    // Add random horizontal beams connecting pillars
    const int num_beams = 25;
    for (int i = 0; i < num_beams; ++i) {
        int p1 = uniform_int_distribution<int>(0, num_pillars - 1)(rng);
        int p2 = uniform_int_distribution<int>(0, num_pillars - 1)(rng);
        if (p1 == p2) continue;
        
        int y = uniform_int_distribution<int>(5, 25)(rng);
        int x1 = pillar_locs[p1].first;
        int z1 = pillar_locs[p1].second;
        int x2 = pillar_locs[p2].first;
        int z2 = pillar_locs[p2].second;
        
        // Draw voxel line
        int dx = abs(x2 - x1);
        int dz = abs(z2 - z1);
        int steps = max(dx, dz);
        for (int s = 0; s <= steps; ++s) {
            float t = (float)s / steps;
            int lx = x1 + (int)(t * (x2 - x1));
            int lz = z1 + (int)(t * (z2 - z1));
            voxels[get_voxel_index(lx, y, lz)] = VOXEL_WOOD;
        }
    }
}

// Statistical helper
struct Stats {
    double mean;
    double median;
    double p95;
    double p99;
    double std_dev;
};

Stats compute_stats(vector<double>& times) {
    Stats s;
    int n = times.size();
    if (n == 0) return {0, 0, 0, 0, 0};
    
    sort(times.begin(), times.end());
    
    double sum = 0;
    for (double t : times) sum += t;
    s.mean = sum / n;
    
    s.median = times[n / 2];
    s.p95 = times[(int)(n * 0.95)];
    s.p99 = times[(int)(n * 0.99)];
    
    double sq_sum = 0;
    for (double t : times) {
        sq_sum += (t - s.mean) * (t - s.mean);
    }
    s.std_dev = sqrt(sq_sum / n);
    
    return s;
}

int main() {
    cout << "Starting Voxel Building Stability and Collapse Physics Benchmark..." << endl;
    
    uint8_t* world_voxels = new uint8_t[WORLD_SIZE * WORLD_SIZE * WORLD_SIZE];
    ChunkTopology* chunk_topologies = new ChunkTopology[NUM_CHUNKS];
    
    // Warmup chunk topologies memory
    memset(chunk_topologies, 0, NUM_CHUNKS * sizeof(ChunkTopology));
    
    vector<string> scenes = {"small_house", "bridge", "tower", "stressed_arch", "random_scaffolding"};
    vector<string> strategies = {"A_NaiveGlobalBFS", "B_HierarchicalCclDsu", "C_LocalSplitBFS", "D_StressPropagation", "E_Hybrid_AABB"};
    
    // CSV output file
    ofstream csv("results.csv");
    csv << "Scene,Strategy,Seed,Mean_us,Median_us,p95_us,p99_us,Std_us,Accuracy,VoxelCount\n";
    
    const int num_seeds = 5;
    unsigned int seeds[num_seeds] = {1, 7, 42, 1234, 31337};
    
    const int num_mutations = 50; // number of deleted voxels
    const int iter_per_mutation = 50; // repeat for stats
    
    for (const string& scene : scenes) {
        cout << "\n--------------------------------------------" << endl;
        cout << "Scene: " << scene << endl;
        cout << "--------------------------------------------" << endl;
        
        for (int seed_idx = 0; seed_idx < num_seeds; ++seed_idx) {
            unsigned int seed = seeds[seed_idx];
            
            // Build scene
            if (scene == "small_house") build_scene_small_house(world_voxels);
            else if (scene == "bridge") build_scene_bridge(world_voxels);
            else if (scene == "tower") build_scene_tower(world_voxels);
            else if (scene == "stressed_arch") build_scene_stressed_arch(world_voxels);
            else if (scene == "random_scaffolding") build_scene_random_scaffolding(world_voxels, seed);
            
            // Count starting solid voxels
            int start_solids = 0;
            for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
                if (world_voxels[i] != VOXEL_AIR && world_voxels[i] != VOXEL_GROUND) {
                    start_solids++;
                }
            }
            
            // Find valid mutation candidates (solid, non-ground voxels)
            vector<int> mutation_candidates;
            for (int i = 0; i < WORLD_SIZE * WORLD_SIZE * WORLD_SIZE; ++i) {
                if (world_voxels[i] != VOXEL_AIR && world_voxels[i] != VOXEL_GROUND) {
                    mutation_candidates.push_back(i);
                }
            }
            
            if (mutation_candidates.empty()) continue;
            
            // Randomly select mutations
            mt19937 mut_rng(seed);
            shuffle(mutation_candidates.begin(), mutation_candidates.end(), mut_rng);
            int actual_mutations = min(num_mutations, (int)mutation_candidates.size());
            vector<int> target_mutations(mutation_candidates.begin(), mutation_candidates.begin() + actual_mutations);
            
            // For each strategy, we run mutations and track timings
            for (const string& strategy : strategies) {
                // Restore original scene
                if (scene == "small_house") build_scene_small_house(world_voxels);
                else if (scene == "bridge") build_scene_bridge(world_voxels);
                else if (scene == "tower") build_scene_tower(world_voxels);
                else if (scene == "stressed_arch") build_scene_stressed_arch(world_voxels);
                else if (scene == "random_scaffolding") build_scene_random_scaffolding(world_voxels, seed);
                
                // Initialize chunk topologies if Strategy B or E
                if (strategy == "B_HierarchicalCclDsu" || strategy == "E_Hybrid_AABB") {
                    for (int chunk_id = 0; chunk_id < NUM_CHUNKS; ++chunk_id) {
                        compute_local_ccl(chunk_id, world_voxels, chunk_topologies[chunk_id]);
                    }
                }
                
                vector<double> timings;
                double total_mismatches = 0;
                double total_checks = 0;
                
                for (int m = 0; m < actual_mutations; ++m) {
                    int delete_idx = target_mutations[m];
                    
                    // Pre-mutation setup
                    uint8_t old_type = world_voxels[delete_idx];
                    
                    // We run multiple iterations for timing precision
                    vector<double> mut_timings;
                    vector<int> collapsed_output;
                    
                    for (int iter = 0; iter < iter_per_mutation; ++iter) {
                        // Apply deletion
                        world_voxels[delete_idx] = VOXEL_AIR;
                        
                        // Local updates if B or E
                        int changed_chunk = -1;
                        if (strategy == "B_HierarchicalCclDsu" || strategy == "E_Hybrid_AABB") {
                            Coords c = get_voxel_coords(delete_idx);
                            changed_chunk = get_chunk_id(c.x, c.y, c.z);
                        }
                        
                        auto start = chrono::high_resolution_clock::now();
                        
                        // Strategy Execution
                        if (strategy == "A_NaiveGlobalBFS") {
                            collapsed_output = run_naive_global_bfs(world_voxels);
                        } 
                        else if (strategy == "B_HierarchicalCclDsu") {
                            compute_local_ccl(changed_chunk, world_voxels, chunk_topologies[changed_chunk]);
                            collapsed_output = run_hierarchical_dsu(world_voxels, chunk_topologies);
                        } 
                        else if (strategy == "C_LocalSplitBFS") {
                            collapsed_output = run_local_split_bfs(delete_idx, world_voxels);
                        } 
                        else if (strategy == "D_StressPropagation") {
                            collapsed_output = run_stress_propagation(world_voxels, 16); // 16 iterations
                        } 
                        else if (strategy == "E_Hybrid_AABB") {
                            compute_local_ccl(changed_chunk, world_voxels, chunk_topologies[changed_chunk]);
                            collapsed_output = run_hybrid_aabb(delete_idx, world_voxels, chunk_topologies);
                        }
                        
                        auto end = chrono::high_resolution_clock::now();
                        double duration = chrono::duration_cast<chrono::nanoseconds>(end - start).count() / 1000.0; // us
                        mut_timings.push_back(duration);
                        
                        // Restore world voxel for next iteration of same mutation
                        world_voxels[delete_idx] = old_type;
                    }
                    
                    // Actually apply deletion and update cache for future mutations
                    world_voxels[delete_idx] = VOXEL_AIR;
                    if (strategy == "B_HierarchicalCclDsu" || strategy == "E_Hybrid_AABB") {
                        Coords c = get_voxel_coords(delete_idx);
                        int changed_chunk = get_chunk_id(c.x, c.y, c.z);
                        compute_local_ccl(changed_chunk, world_voxels, chunk_topologies[changed_chunk]);
                    }
                    
                    // Validate correctness against baseline A_NaiveGlobalBFS
                    // (Note: Strategy D is stress-based, so it will differ naturally in model,
                    // we don't count D's different model as "mismatch" but we check it for crash).
                    if (strategy != "D_StressPropagation") {
                        vector<int> baseline_output = run_naive_global_bfs(world_voxels);
                        sort(baseline_output.begin(), baseline_output.end());
                        sort(collapsed_output.begin(), collapsed_output.end());
                        
                        total_checks++;
                        if (collapsed_output != baseline_output) {
                            total_mismatches++;
                        }
                    }
                    
                    // Remove collapsed voxels from the world to simulate physical collapse cascading
                    for (int idx : collapsed_output) {
                        world_voxels[idx] = VOXEL_AIR;
                        if (strategy == "B_HierarchicalCclDsu" || strategy == "E_Hybrid_AABB") {
                            Coords c = get_voxel_coords(idx);
                            int changed_chunk = get_chunk_id(c.x, c.y, c.z);
                            compute_local_ccl(changed_chunk, world_voxels, chunk_topologies[changed_chunk]);
                        }
                    }
                    
                    // Accumulate timings
                    for (double t : mut_timings) {
                        timings.push_back(t);
                    }
                }
                
                // Compute statistics
                Stats s = compute_stats(timings);
                double accuracy = 1.0;
                if (strategy != "D_StressPropagation" && total_checks > 0) {
                    accuracy = 1.0 - (total_mismatches / total_checks);
                }
                
                cout << "  " << strategy << ": Mean = " << s.mean << " us, Acc = " << (accuracy * 100.0) << "%" << endl;
                csv << scene << "," << strategy << "," << seed << ","
                    << s.mean << "," << s.median << "," << s.p95 << "," << s.p99 << "," << s.std_dev << ","
                    << accuracy << "," << start_solids << "\n";
            }
        }
    }
    
    delete[] world_voxels;
    delete[] chunk_topologies;
    csv.close();
    cout << "\nBenchmark complete. Results written to results.csv." << endl;
    return 0;
}
