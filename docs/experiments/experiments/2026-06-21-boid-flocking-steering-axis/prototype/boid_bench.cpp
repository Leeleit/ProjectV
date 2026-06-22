// boid_bench.cpp — Boid/Flocking steering benchmark
// 2026-06-21-boid-flocking-steering-axis experiment
//
// Standalone C++26 CPU prototype. Implements 5 strategies for neighbor query + force calculation
// of Reynolds 1987 boid model (separation/alignment/cohesion).
//
// Build: clang++ -std=c++26 -O3 -march=native -DNDEBUG -Wall -Wextra -Wpedantic \
//            -o build/boid_bench boid_bench.cpp
// Run:   ./build/boid_bench
//
// Hardware baseline: Zen 3 5800X (AVX2 + FMA + BMI2, no AVX-512) per hardware-profile.md §1.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// --------------------------------------------------------------------------
// Vec3 helpers (avoid std::array verbosity)
// --------------------------------------------------------------------------

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    Vec3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }
    Vec3 operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    Vec3 operator*(float s) const { return {x*s, y*s, z*s}; }
    Vec3 operator/(float s) const { return {x/s, y/s, z/s}; }
};

inline float dot3(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline float length3(const Vec3& a) { return std::sqrt(dot3(a, a)); }
inline float dist3(const Vec3& a, const Vec3& b) { return length3(a - b); }
inline Vec3 normalize3(const Vec3& a, float eps = 1e-9f) {
    float l = length3(a);
    return l > eps ? a * (1.0f / l) : Vec3{};
}
inline Vec3 clamp_mag3(const Vec3& a, float max_mag) {
    float l = length3(a);
    return l > max_mag ? a * (max_mag / l) : a;
}

// --------------------------------------------------------------------------
// Constants — Reynolds 1987 canonical boid model
// --------------------------------------------------------------------------

struct World {
    Vec3 bounds;             // world size in each dimension
    float perception_r = 2.0f;   // neighborhood radius
    float separation_r = 1.0f;   // separation-only radius (within perception)
    float max_speed = 5.0f;
    float max_force = 10.0f;
    float dt = 0.1f;             // 10 Hz tick
    float w_sep = 1.5f;          // separation weight (highest — anti-collision)
    float w_align = 1.0f;        // alignment weight
    float w_cohes = 1.0f;        // cohesion weight
};

// --------------------------------------------------------------------------
// Stats computation per benchmarks/methodology.md §7
// --------------------------------------------------------------------------

struct Stats {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
    double minv = 0.0;
    double maxv = 0.0;
    int n = 0;
};

Stats ComputeStats(std::vector<double> samples) {
    Stats s{};
    s.n = static_cast<int>(samples.size());
    if (samples.empty()) return s;
    std::sort(samples.begin(), samples.end());
    double sum = 0.0;
    for (double v : samples) sum += v;
    s.mean = sum / samples.size();
    s.median = samples[samples.size() / 2];
    s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
    s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
    double var = 0.0;
    for (double v : samples) var += (v - s.mean) * (v - s.mean);
    s.stddev = std::sqrt(var / samples.size());
    s.minv = samples.front();
    s.maxv = samples.back();
    return s;
}

// --------------------------------------------------------------------------
// xoshiro256** PRNG (fast, deterministic, good statistical properties)
// --------------------------------------------------------------------------

struct Xoshiro256 {
    uint64_t s[4]{};

    explicit Xoshiro256(uint64_t seed) {
        // SplitMix64 init
        for (int i = 0; i < 4; i++) {
            uint64_t z = (seed += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            s[i] = z ^ (z >> 31);
        }
    }
    static uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }
    uint64_t next() {
        uint64_t result = rotl(s[1] * 5, 7) * 9;
        uint64_t t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
        s[2] ^= t; s[3] = rotl(s[3], 45);
        return result;
    }
    float uniform() { return static_cast<float>(next() >> 11) * (1.0f / 9007199254740992.0f); }
    Vec3 uniform_vec3_in_box(const Vec3& bounds) {
        return {uniform() * bounds.x, uniform() * bounds.y, uniform() * bounds.z};
    }
    Vec3 random_unit_vec3() {
        // Marsaglia 1972 method
        float z = uniform() * 2.0f - 1.0f;
        float a = uniform() * 6.2831853f;
        float r = std::sqrt(1.0f - z * z);
        return {r * std::cos(a), r * std::sin(a), z};
    }
};

// --------------------------------------------------------------------------
// Boid SoA storage
// --------------------------------------------------------------------------

struct BoidArray {
    std::vector<Vec3> pos;     // x[N], y[N], z[N] stored in SoA-friendly array-of-struct
    std::vector<Vec3> vel;
    std::vector<Vec3> force;  // accumulator per tick
    int n = 0;

    void resize(int n_) {
        n = n_;
        pos.assign(n, {});
        vel.assign(n, {});
        force.assign(n, {});
    }
    void init_random(Xoshiro256& rng, const World& w) {
        for (int i = 0; i < n; i++) {
            pos[i] = rng.uniform_vec3_in_box(w.bounds);
            vel[i] = rng.random_unit_vec3() * (w.max_speed * 0.5f);
            force[i] = {};
        }
    }
    void integrate(const World& w) {
        for (int i = 0; i < n; i++) {
            vel[i] += force[i] * w.dt;
            vel[i] = clamp_mag3(vel[i], w.max_speed);
            pos[i] += vel[i] * w.dt;
            // wrap-around boundary
            auto wrap = [](float& p, float b) {
                if (p < 0.0f) p += b;
                else if (p >= b) p -= b;
            };
            wrap(pos[i].x, w.bounds.x);
            wrap(pos[i].y, w.bounds.y);
            wrap(pos[i].z, w.bounds.z);
            force[i] = {};
        }
    }
};

// --------------------------------------------------------------------------
// Strategy A — Naive O(N²) double loop
// --------------------------------------------------------------------------

void StepNaive(BoidArray& ba, const World& w) {
    for (int i = 0; i < ba.n; i++) {
        Vec3 sep{}, ali{}, coh{};
        int sep_n = 0, ali_n = 0, coh_n = 0;
        for (int j = 0; j < ba.n; j++) {
            if (j == i) continue;
            Vec3 d = ba.pos[i] - ba.pos[j];
            float d2 = dot3(d, d);
            if (d2 > w.perception_r * w.perception_r) continue;
            float d_len = std::sqrt(d2);
            if (d_len < w.separation_r && d_len > 1e-6f) {
                // separation: push away, inverse-distance weighted
                sep += d * (1.0f / std::max(d_len, 1e-3f));
                sep_n++;
            }
            ali += ba.vel[j];
            ali_n++;
            coh += ba.pos[j];
            coh_n++;
        }
        Vec3 total{};
        if (sep_n > 0) total += sep * (w.w_sep / sep_n);
        if (ali_n > 0) total += (ali / static_cast<float>(ali_n) - ba.vel[i]) * w.w_align;
        if (coh_n > 0) total += (coh / static_cast<float>(coh_n) - ba.pos[i]) * w.w_cohes * 0.05f;
        ba.force[i] = clamp_mag3(total, w.max_force);
    }
    ba.integrate(w);
}

// --------------------------------------------------------------------------
// Strategy B — Spatial hash grid (Reynolds 1987 "spatial data structure")
// --------------------------------------------------------------------------

struct SpatialHash {
    float cell_size = 0.0f;
    std::unordered_map<int64_t, std::vector<int>> cells;

    void clear() { cells.clear(); }
    void reserve(int n) { cells.reserve(n / 4); }

    static int64_t key(int x, int y, int z) {
        // Cantor-like 3D hash
        return (static_cast<int64_t>(x) * 73856093LL) ^
               (static_cast<int64_t>(y) * 19349663LL) ^
               (static_cast<int64_t>(z) * 83492791LL);
    }

    void build(const BoidArray& ba, const World& w) {
        clear();
        cell_size = w.perception_r;
        for (int i = 0; i < ba.n; i++) {
            int cx = static_cast<int>(std::floor(ba.pos[i].x / cell_size));
            int cy = static_cast<int>(std::floor(ba.pos[i].y / cell_size));
            int cz = static_cast<int>(std::floor(ba.pos[i].z / cell_size));
            cells[key(cx, cy, cz)].push_back(i);
        }
    }

    void query_neighbors(const Vec3& p, std::vector<int>& out, const World& w) const {
        out.clear();
        int cx = static_cast<int>(std::floor(p.x / cell_size));
        int cy = static_cast<int>(std::floor(p.y / cell_size));
        int cz = static_cast<int>(std::floor(p.z / cell_size));
        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            auto it = cells.find(key(cx+dx, cy+dy, cz+dz));
            if (it == cells.end()) continue;
            for (int j : it->second) {
                Vec3 d = p - ba_pos_lookup(j);  // small trick
                if (dot3(d, d) <= w.perception_r * w.perception_r) out.push_back(j);
            }
        }
    }

    // Provide a function pointer to ba.pos for neighbor lookup
    const Vec3* ba_pos = nullptr;
    Vec3 ba_pos_lookup(int j) const { return ba_pos[j]; }
    void set_boids(const BoidArray& ba) { ba_pos = ba.pos.data(); }
};

void StepSpatialHash(BoidArray& ba, const World& w) {
    static SpatialHash sh;
    static std::vector<int> neighbors;
    sh.build(ba, w);
    sh.set_boids(ba);
    for (int i = 0; i < ba.n; i++) {
        Vec3 sep{}, ali{}, coh{};
        int sep_n = 0, ali_n = 0, coh_n = 0;
        // Query 27 cells around i
        int cx = static_cast<int>(std::floor(ba.pos[i].x / w.perception_r));
        int cy = static_cast<int>(std::floor(ba.pos[i].y / w.perception_r));
        int cz = static_cast<int>(std::floor(ba.pos[i].z / w.perception_r));
        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            auto it = sh.cells.find(SpatialHash::key(cx+dx, cy+dy, cz+dz));
            if (it == sh.cells.end()) continue;
            for (int j : it->second) {
                if (j == i) continue;
                Vec3 d = ba.pos[i] - ba.pos[j];
                float d2 = dot3(d, d);
                if (d2 > w.perception_r * w.perception_r) continue;
                float d_len = std::sqrt(d2);
                if (d_len < w.separation_r && d_len > 1e-6f) {
                    sep += d * (1.0f / std::max(d_len, 1e-3f));
                    sep_n++;
                }
                ali += ba.vel[j];
                ali_n++;
                coh += ba.pos[j];
                coh_n++;
            }
        }
        Vec3 total{};
        if (sep_n > 0) total += sep * (w.w_sep / sep_n);
        if (ali_n > 0) total += (ali / static_cast<float>(ali_n) - ba.vel[i]) * w.w_align;
        if (coh_n > 0) total += (coh / static_cast<float>(coh_n) - ba.pos[i]) * w.w_cohes * 0.05f;
        ba.force[i] = clamp_mag3(total, w.max_force);
    }
    ba.integrate(w);
}

// --------------------------------------------------------------------------
// Strategy C — KD-tree (simple recursive, range query bounded depth)
// --------------------------------------------------------------------------

struct KDNode {
    int idx;
    int axis;  // 0=x, 1=y, 2=z
    int left = -1, right = -1;
};

struct KDTree {
    std::vector<KDNode> nodes;
    int root = -1;

    void build(const BoidArray& ba) {
        nodes.clear();
        std::vector<int> ids(ba.n);
        std::iota(ids.begin(), ids.end(), 0);
        root = build_rec(ba, ids, 0, static_cast<int>(ids.size()), 0);
    }
    int build_rec(const BoidArray& ba, std::vector<int>& ids, int lo, int hi, int depth) {
        if (lo >= hi) return -1;
        int axis = depth % 3;
        int mid = (lo + hi) / 2;
        std::nth_element(ids.begin() + lo, ids.begin() + mid, ids.begin() + hi,
            [&](int a, int b) {
                float aa = axis == 0 ? ba.pos[a].x : axis == 1 ? ba.pos[a].y : ba.pos[a].z;
                float bb = axis == 0 ? ba.pos[b].x : axis == 1 ? ba.pos[b].y : ba.pos[b].z;
                return aa < bb;
            });
        int id = static_cast<int>(nodes.size());
        nodes.push_back({ids[mid], axis, -1, -1});
        nodes[id].left = build_rec(ba, ids, lo, mid, depth + 1);
        nodes[id].right = build_rec(ba, ids, mid + 1, hi, depth + 1);
        return id;
    }
    void range_query(int node, const Vec3& p, float r2,
                     std::vector<int>& out, const BoidArray& ba) const {
        if (node < 0) return;
        int j = nodes[node].idx;
        Vec3 d = p - ba.pos[j];
        if (dot3(d, d) <= r2) out.push_back(j);
        int axis = nodes[node].axis;
        float diff = axis == 0 ? d.x : axis == 1 ? d.y : d.z;
        int first = diff < 0 ? nodes[node].left : nodes[node].right;
        int second = diff < 0 ? nodes[node].right : nodes[node].left;
        range_query(first, p, r2, out, ba);
        if (diff * diff < r2) range_query(second, p, r2, out, ba);
    }
};

void StepKDTree(BoidArray& ba, const World& w) {
    static KDTree tree;
    static std::vector<int> neighbors;
    tree.build(ba);
    float r2 = w.perception_r * w.perception_r;
    for (int i = 0; i < ba.n; i++) {
        neighbors.clear();
        tree.range_query(tree.root, ba.pos[i], r2, neighbors, ba);
        Vec3 sep{}, ali{}, coh{};
        int sep_n = 0, ali_n = 0, coh_n = 0;
        for (int j : neighbors) {
            if (j == i) continue;
            Vec3 d = ba.pos[i] - ba.pos[j];
            float d_len = std::sqrt(dot3(d, d));
            if (d_len < w.separation_r && d_len > 1e-6f) {
                sep += d * (1.0f / std::max(d_len, 1e-3f));
                sep_n++;
            }
            ali += ba.vel[j];
            ali_n++;
            coh += ba.pos[j];
            coh_n++;
        }
        Vec3 total{};
        if (sep_n > 0) total += sep * (w.w_sep / sep_n);
        if (ali_n > 0) total += (ali / static_cast<float>(ali_n) - ba.vel[i]) * w.w_align;
        if (coh_n > 0) total += (coh / static_cast<float>(coh_n) - ba.pos[i]) * w.w_cohes * 0.05f;
        ba.force[i] = clamp_mag3(total, w.max_force);
    }
    ba.integrate(w);
}

// --------------------------------------------------------------------------
// Strategy D — Spatial hash + SIMD AVX2 batch processing
// (8 floats/cycle FMA accumulation per neighbor scan)
// --------------------------------------------------------------------------

#if defined(__AVX2__)
#include <immintrin.h>

// Horizontal sum of __m256 — defined before use
inline float _mm256_reduce_add_ps(__m256 v) {
    __m128 vlow = _mm256_castps256_ps128(v);
    __m128 vhigh = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(vlow, vhigh);
    __m128 shuf = _mm_movehdup_ps(sum);
    __m128 sums = _mm_add_ps(sum, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
}
#endif

void StepSIMD_AVX2(BoidArray& ba, const World& w) {
    // Same query structure as B_SpatialHash, but use AVX2 to batch-process neighbor positions
    // into force accumulators.
    static SpatialHash sh;
    sh.build(ba, w);
    sh.set_boids(ba);
    const float r2 = w.perception_r * w.perception_r;
    const float sep_r2 = w.separation_r * w.separation_r;
    const float w_sep = w.w_sep;
    const float w_align = w.w_align;
    const float w_coh = w.w_cohes * 0.05f;

    for (int i = 0; i < ba.n; i++) {
        Vec3 sep_acc{};
        Vec3 ali_acc{};
        Vec3 coh_acc{};
        int sep_n = 0, ali_n = 0, coh_n = 0;

        int cx = static_cast<int>(std::floor(ba.pos[i].x / w.perception_r));
        int cy = static_cast<int>(std::floor(ba.pos[i].y / w.perception_r));
        int cz = static_cast<int>(std::floor(ba.pos[i].z / w.perception_r));

        // For each cell, gather 8 neighbors at a time (if available) and process with AVX2
        for (int dx = -1; dx <= 1; dx++)
        for (int dy = -1; dy <= 1; dy++)
        for (int dz = -1; dz <= 1; dz++) {
            auto it = sh.cells.find(SpatialHash::key(cx+dx, cy+dy, cz+dz));
            if (it == sh.cells.end()) continue;
            const std::vector<int>& cell = it->second;
            int cn = static_cast<int>(cell.size());
            int j = 0;
            // Process 8-at-a-time with AVX2
#if defined(__AVX2__)
            for (; j + 8 <= cn; j += 8) {
                // Load 8 neighbor positions
                __m256 px = _mm256_setr_ps(
                    ba.pos[cell[j+0]].x, ba.pos[cell[j+1]].x, ba.pos[cell[j+2]].x, ba.pos[cell[j+3]].x,
                    ba.pos[cell[j+4]].x, ba.pos[cell[j+5]].x, ba.pos[cell[j+6]].x, ba.pos[cell[j+7]].x);
                __m256 py = _mm256_setr_ps(
                    ba.pos[cell[j+0]].y, ba.pos[cell[j+1]].y, ba.pos[cell[j+2]].y, ba.pos[cell[j+3]].y,
                    ba.pos[cell[j+4]].y, ba.pos[cell[j+5]].y, ba.pos[cell[j+6]].y, ba.pos[cell[j+7]].y);
                __m256 pz = _mm256_setr_ps(
                    ba.pos[cell[j+0]].z, ba.pos[cell[j+1]].z, ba.pos[cell[j+2]].z, ba.pos[cell[j+3]].z,
                    ba.pos[cell[j+4]].z, ba.pos[cell[j+5]].z, ba.pos[cell[j+6]].z, ba.pos[cell[j+7]].z);

                // dx, dy, dz = pos[i] - pos[j]
                __m256 dxv = _mm256_sub_ps(_mm256_set1_ps(ba.pos[i].x), px);
                __m256 dyv = _mm256_sub_ps(_mm256_set1_ps(ba.pos[i].y), py);
                __m256 dzv = _mm256_sub_ps(_mm256_set1_ps(ba.pos[i].z), pz);
                // d2 = dx^2 + dy^2 + dz^2
                __m256 d2 = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dxv, dxv), _mm256_mul_ps(dyv, dyv)),
                                          _mm256_mul_ps(dzv, dzv));
                // mask: d2 < r2
                __m256 mask_r = _mm256_cmp_ps(d2, _mm256_set1_ps(r2), _CMP_LT_OQ);
                // For ali/coh: apply mask_r, accumulate vel and pos
                __m256 vx = _mm256_setr_ps(
                    ba.vel[cell[j+0]].x, ba.vel[cell[j+1]].x, ba.vel[cell[j+2]].x, ba.vel[cell[j+3]].x,
                    ba.vel[cell[j+4]].x, ba.vel[cell[j+5]].x, ba.vel[cell[j+6]].x, ba.vel[cell[j+7]].x);
                __m256 vy = _mm256_setr_ps(
                    ba.vel[cell[j+0]].y, ba.vel[cell[j+1]].y, ba.vel[cell[j+2]].y, ba.vel[cell[j+3]].y,
                    ba.vel[cell[j+4]].y, ba.vel[cell[j+5]].y, ba.vel[cell[j+6]].y, ba.vel[cell[j+7]].y);
                __m256 vz = _mm256_setr_ps(
                    ba.vel[cell[j+0]].z, ba.vel[cell[j+1]].z, ba.vel[cell[j+2]].z, ba.vel[cell[j+3]].z,
                    ba.vel[cell[j+4]].z, ba.vel[cell[j+5]].z, ba.vel[cell[j+6]].z, ba.vel[cell[j+7]].z);
                ali_acc.x += _mm256_reduce_add_ps(_mm256_and_ps(vx, mask_r));
                ali_acc.y += _mm256_reduce_add_ps(_mm256_and_ps(vy, mask_r));
                ali_acc.z += _mm256_reduce_add_ps(_mm256_and_ps(vz, mask_r));
                coh_acc.x += _mm256_reduce_add_ps(_mm256_and_ps(px, mask_r));
                coh_acc.y += _mm256_reduce_add_ps(_mm256_and_ps(py, mask_r));
                coh_acc.z += _mm256_reduce_add_ps(_mm256_and_ps(pz, mask_r));
                // For separation: extract d, accumulate (1/d) weighted
                // For prototype: simple accumulator, ignore SIMD for separation (subtle to vectorize)
                float d2_arr[8];
                _mm256_storeu_ps(d2_arr, d2);
                for (int k = 0; k < 8; k++) {
                    int jj = cell[j + k];
                    if (jj == i) continue;
                    if (d2_arr[k] < r2) {
                        ali_n++;
                        coh_n++;
                        if (d2_arr[k] < sep_r2 && d2_arr[k] > 1e-6f) {
                            float d_len = std::sqrt(d2_arr[k]);
                            sep_acc += (ba.pos[i] - ba.pos[jj]) * (1.0f / std::max(d_len, 1e-3f));
                            sep_n++;
                        }
                    }
                }
                _mm256_zeroupper();  // avoid AVX-SSE transition penalty
            }
#endif
            // tail (less than 8)
            for (; j < cn; j++) {
                int jj = cell[j];
                if (jj == i) continue;
                Vec3 d = ba.pos[i] - ba.pos[jj];
                float d2v = dot3(d, d);
                if (d2v > r2) continue;
                ali_acc += ba.vel[jj];
                coh_acc += ba.pos[jj];
                ali_n++;
                coh_n++;
                float d_len = std::sqrt(d2v);
                if (d_len < w.separation_r && d_len > 1e-6f) {
                    sep_acc += d * (1.0f / std::max(d_len, 1e-3f));
                    sep_n++;
                }
            }
        }
        Vec3 total{};
        if (sep_n > 0) total += sep_acc * (w_sep / sep_n);
        if (ali_n > 0) total += (ali_acc / static_cast<float>(ali_n) - ba.vel[i]) * w_align;
        if (coh_n > 0) total += (coh_acc / static_cast<float>(coh_n) - ba.pos[i]) * w_coh;
        ba.force[i] = clamp_mag3(total, w.max_force);
    }
    ba.integrate(w);
}

// Horizontal sum of __m256 — only with AVX2 + helper
// (moved above StepSIMD_AVX2)

// --------------------------------------------------------------------------
// Strategy E — GPU compute analytical projection
//   CPU synthetic w/ GPU latency model.
//   Per-boid cost = α + β·k where k = neighbor count (estimated from B_SpatialHash).
//   + GPU launch overhead (1 ms = typical Vulkan dispatch).
//   Speedup = 1.5-2.5× over B for parallel reduction (typical).
// --------------------------------------------------------------------------

void StepGPUComputeAnalytical(BoidArray& ba, const World& w) {
    // Analytical: use B_SpatialHash result, project cost to GPU model.
    // For prototype, just run B_SpatialHash but apply a constant scaling factor (1.5x speedup).
    // Real cost = (B cost) / GPU_SPEEDUP + LAUNCH_OVERHEAD
    // Since we're CPU, this is a measurement of the underlying algorithm (B),
    // with a documented GPU projection in the output.
    StepSpatialHash(ba, w);
}

// --------------------------------------------------------------------------
// Strategy enum + dispatch
// --------------------------------------------------------------------------

enum class Strategy {
    A_Naive = 0,
    B_SpatialHash = 1,
    C_KDTree = 2,
    D_SIMD_AVX2 = 3,
    E_GPUCompute = 4
};

const char* StrategyName(Strategy s) {
    switch (s) {
        case Strategy::A_Naive: return "A_Naive";
        case Strategy::B_SpatialHash: return "B_SpatialHash";
        case Strategy::C_KDTree: return "C_KDTree";
        case Strategy::D_SIMD_AVX2: return "D_SIMD_AVX2";
        case Strategy::E_GPUCompute: return "E_GPUCompute";
    }
    return "?";
}

void StepStrategy(Strategy s, BoidArray& ba, const World& w) {
    switch (s) {
        case Strategy::A_Naive: StepNaive(ba, w); break;
        case Strategy::B_SpatialHash: StepSpatialHash(ba, w); break;
        case Strategy::C_KDTree: StepKDTree(ba, w); break;
        case Strategy::D_SIMD_AVX2: StepSIMD_AVX2(ba, w); break;
        case Strategy::E_GPUCompute: StepGPUComputeAnalytical(ba, w); break;
    }
}

// --------------------------------------------------------------------------
// Scene configs
// --------------------------------------------------------------------------

struct Scene {
    const char* name;
    int n_boids;
    Vec3 bounds;
};

const std::vector<Scene> SCENES = {
    {"small_drone_squad",   100,  { 50,  50,  50}},
    {"medium_drone_swarm",  1000, {100, 100,  50}},
    {"large_battle_drones", 5000, {200, 200, 100}},
    {"xlarge_swarm",        10000,{200, 200, 100}},
    {"mega_flock",          50000,{500, 500, 200}},
};

const std::vector<int> SEEDS = {1, 7, 42, 1234, 31337};
const std::vector<Strategy> STRATEGIES = {
    Strategy::A_Naive, Strategy::B_SpatialHash, Strategy::C_KDTree,
    Strategy::D_SIMD_AVX2
    // E_GPUCompute removed: StepGPUComputeAnalytical literally calls StepSpatialHash
    // (analytical projection only — no real GPU dispatch); provides no new data.
};
constexpr int WARMUP = 10;
constexpr int MAIN_ITER = 1000;

// --------------------------------------------------------------------------
// Main benchmark loop
// --------------------------------------------------------------------------

int main() {
    std::printf("boid_bench — Boid/Flocking steering benchmark (2026-06-21)\n");
    std::printf("Hardware: Zen 3 5800X (AVX2 + FMA, no AVX-512) per hardware-profile.md §1\n");
    std::printf("Compiler: Clang -O3 -march=native -std=c++26\n");
    std::printf("Configs: %zu strategies x %zu scenes x %zu seeds x %d iter + %d warmup = %d measurements\n",
                STRATEGIES.size(), SCENES.size(), SEEDS.size(), MAIN_ITER, WARMUP,
                static_cast<int>(STRATEGIES.size() * SCENES.size() * SEEDS.size() * MAIN_ITER));
    std::printf("(E_GPUCompute = analytical projection only, same as B_SpatialHash — excluded)\n");
    std::printf("Output: build/results.csv\n\n");

    std::ofstream csv("build/results.csv");
    if (!csv) {
        std::fprintf(stderr, "ERROR: cannot open build/results.csv\n");
        return 1;
    }
    csv << "strategy,scene,n_boids,seed,iter,ns_per_tick,flock_order,mean_dist_to_centroid\n";

    World w;
    w.bounds = {200, 200, 100};

    int total_configs = static_cast<int>(STRATEGIES.size() * SCENES.size() * SEEDS.size());
    int cfg = 0;
    auto t_start_total = std::chrono::high_resolution_clock::now();

    for (Strategy strat : STRATEGIES) {
        for (const Scene& scene : SCENES) {
            // Skip A_Naive for N > 1000 (impractical: O(N²) × 1000 iter × 5 seeds = too slow)
            if (strat == Strategy::A_Naive && scene.n_boids > 1000) {
                std::printf("[skip] A_Naive @ N=%d (impractical, >100s/iter expected)\n", scene.n_boids);
                continue;
            }
            for (int seed : SEEDS) {
                cfg++;
                // Adjust world bounds per scene
                World wlocal = w;
                wlocal.bounds = scene.bounds;
                // Adjust max_speed for larger worlds to keep visual flow
                wlocal.max_speed = std::min(20.0f, 0.1f * std::min({scene.bounds.x, scene.bounds.y, scene.bounds.z}));

                BoidArray ba;
                ba.resize(scene.n_boids);
                Xoshiro256 rng(static_cast<uint64_t>(seed) * 1000003ULL);
                ba.init_random(rng, wlocal);

                // Warmup
                for (int it = 0; it < WARMUP; it++) {
                    StepStrategy(strat, ba, wlocal);
                }

                // Main loop
                std::vector<double> samples;
                samples.reserve(MAIN_ITER);
                double last_order = 0.0;
                double last_centroid_dist = 0.0;

                for (int it = 0; it < MAIN_ITER; it++) {
                    auto t0 = std::chrono::high_resolution_clock::now();
                    StepStrategy(strat, ba, wlocal);
                    auto t1 = std::chrono::high_resolution_clock::now();
                    double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
                    samples.push_back(ns);

                    // Compute flock order parameter (polarization) and cohesion
                    if (it == MAIN_ITER - 1) {
                        Vec3 avg_v{};
                        Vec3 centroid{};
                        for (int i = 0; i < ba.n; i++) {
                            avg_v += ba.vel[i];
                            centroid += ba.pos[i];
                        }
                        avg_v /= static_cast<float>(ba.n);
                        centroid /= static_cast<float>(ba.n);
                        last_order = length3(avg_v) / wlocal.max_speed;
                        double d_sum = 0.0;
                        for (int i = 0; i < ba.n; i++) {
                            d_sum += dist3(ba.pos[i], centroid);
                        }
                        last_centroid_dist = d_sum / ba.n;
                    }
                }

                Stats st = ComputeStats(std::move(samples));
                std::printf("[%3d/%d] %s | %-22s | N=%5d | seed=%5d | mean=%8.0f ns/iter | p99=%8.0f | order=%.3f\n",
                            cfg, total_configs, StrategyName(strat), scene.name, scene.n_boids, seed,
                            st.mean, st.p99, last_order);

                csv << StrategyName(strat) << ","
                    << scene.name << ","
                    << scene.n_boids << ","
                    << seed << ","
                    << MAIN_ITER << ","
                    << st.mean << ","
                    << last_order << ","
                    << last_centroid_dist << "\n";
                csv.flush();  // ensure data persists on abort
            }
        }
    }

    auto t_end_total = std::chrono::high_resolution_clock::now();
    double total_sec = std::chrono::duration<double>(t_end_total - t_start_total).count();
    std::printf("\nTotal wall time: %.2f sec\n", total_sec);
    csv.close();
    std::printf("Results written to build/results.csv\n");
    return 0;
}
