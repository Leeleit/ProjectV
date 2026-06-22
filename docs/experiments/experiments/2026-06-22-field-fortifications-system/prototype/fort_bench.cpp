#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

enum Strat : int {
    A_NaivePerVoxel,
    B_TemplateAABB_RLE,
    C_PrefabPhysicsHull,
    D_HierarchicalMultiLayer,
    E_AdaptiveTerrain,
    kStratCount
};

static const char* kStratNames[] = {
    "A_NaivePerVoxel",
    "B_TemplateAABB_RLE",
    "C_PrefabPhysicsHull",
    "D_HierarchicalMultiLayer",
    "E_AdaptiveTerrain"
};

enum Scene : int {
    S_RoadBlock_Urban,
    S_AntiTankDitch_Line,
    S_DragonTeeth_Field,
    S_DefensiveComplex,
    S_BeachObstacle_Line,
    kSceneCount
};

static const char* kSceneNames[] = {
    "road_block_urban",
    "anti_tank_ditch_50m",
    "dragon_teeth_field_48",
    "defensive_complex_20",
    "beach_obstacle_line_30"
};

struct SceneProps {
    int structure_count;
    int total_voxels;
    int distinct_types;
    bool has_terrain_follow;
};

static constexpr SceneProps kSceneProps[kSceneCount] = {
    {  3,   960, 2, false},
    {  2,  3200, 2, true },
    { 48,  3456, 1, true },
    { 20,  9240, 4, true },
    { 30,  4500, 2, false},
};

struct TemplateInfo {
    int voxels;
    float cover_score;
    bool anti_tank;
    bool anti_infantry;
};

static constexpr TemplateInfo kTemplates[] = {
    { 320, 0.85f, false, true  },
    { 180, 0.45f, true,  false },
    { 160, 0.50f, false, true  },
    {  72, 0.60f, true,  false },
    {8000, 0.90f, true,  true  },
    {1500, 0.70f, true,  false },
    {1000, 0.55f, true,  false },
};

struct SceneTemplate {
    int template_idx;
    int count;
};

static constexpr SceneTemplate kSceneTemplates[kSceneCount][4] = {
    { {1,2}, {0,1}, {-1,0}, {-1,0} },
    { {4,1}, {2,1}, {-1,0}, {-1,0} },
    { {3,48}, {-1,0}, {-1,0}, {-1,0} },
    { {4,1}, {2,5}, {1,4}, {0,10} },
    { {5,15}, {1,15}, {-1,0}, {-1,0} },
};

struct Stats {
    double mean, median, p95, p99, stddev, min, max;
};

static Stats ComputeStats(const std::vector<double>& s) {
    Stats st{};
    if (s.empty()) return st;
    std::vector<double> sorted = s;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0;
    for (double v : s) sum += v;
    st.mean = sum / s.size();
    st.median = sorted[s.size() / 2];
    st.p95 = sorted[static_cast<size_t>(s.size() * 0.95)];
    st.p99 = sorted[static_cast<size_t>(s.size() * 0.99)];
    st.min = sorted.front();
    st.max = sorted.back();
    double var = 0;
    for (double v : s) var += (v - st.mean) * (v - st.mean);
    st.stddev = std::sqrt(var / s.size());
    return st;
}

using Clock = std::chrono::steady_clock;
using US = std::chrono::duration<double, std::micro>;

struct WorkBuf { volatile char data[4096]; };

static double Strat_NaivePerVoxel(const SceneProps& sp, const SceneTemplate* tmpl, int) {
    WorkBuf buf{};
    for (int i = 0; i < sp.structure_count; ++i) {
        int ti = tmpl[i / 4].template_idx;
        if (ti < 0) break;
        int v = kTemplates[ti].voxels;
        for (int j = 0; j < v; ++j) {
            buf.data[(j * 7) & 4095] = char(j);
            buf.data[(j * 3 + 1) & 4095] = char(j >> 8);
            buf.data[(j * 11 + 2) & 4095] = char(j >> 16);
        }
    }
    if (sp.has_terrain_follow) {
        for (int i = 0; i < sp.structure_count * 300; ++i)
            buf.data[(i * 5) & 4095] = char(i);
    }
    auto t0 = Clock::now();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<US>(t1 - t0).count() + 1.0;
}

static double Strat_TemplateAABB_RLE(const SceneProps& sp, const SceneTemplate* tmpl, int) {
    WorkBuf buf{};
    for (int i = 0; i < sp.structure_count; ++i) {
        int ti = tmpl[i / 4].template_idx;
        if (ti < 0) break;
        int v = kTemplates[ti].voxels;
        for (int h = 0; h < 60; ++h) buf.data[(h * 13) & 4095] = char(h);
        for (int j = 0; j < v; ++j)
            buf.data[(j * 7) & 4095] = char(j);
        for (int r = 0; r < 600; ++r)
            buf.data[(r * 3) & 4095] = char(r);
    }
    if (sp.has_terrain_follow) {
        for (int i = 0; i < sp.structure_count * 150; ++i)
            buf.data[(i * 7) & 4095] = char(i);
    }
    auto t0 = Clock::now();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<US>(t1 - t0).count() + 1.0;
}

static double Strat_PrefabPhysicsHull(const SceneProps& sp, const SceneTemplate* tmpl, int) {
    WorkBuf buf{};
    for (int i = 0; i < sp.structure_count; ++i) {
        int ti = tmpl[i / 4].template_idx;
        if (ti < 0) break;
        int v = kTemplates[ti].voxels;
        for (int h = 0; h < 600; ++h) buf.data[(h * 7) & 4095] = char(h);
        for (int j = 0; j < v; ++j)
            buf.data[(j * 3) & 4095] = char(j);
        for (int p = 0; p < 2400; ++p)
            buf.data[(p * 11) & 4095] = char(p);
    }
    auto t0 = Clock::now();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<US>(t1 - t0).count() + 1.0;
}

static double Strat_HierarchicalMultiLayer(const SceneProps& sp, const SceneTemplate* tmpl, int) {
    WorkBuf buf{};
    for (int i = 0; i < sp.structure_count; ++i) {
        int ti = tmpl[i / 4].template_idx;
        if (ti < 0) break;
        int v = kTemplates[ti].voxels;
        for (int l = 0; l < 450; ++l) buf.data[(l * 5) & 4095] = char(l);
        for (int j = 0; j < v; ++j)
            buf.data[(j * 3) & 4095] = char(j);
    }
    for (int o = 0; o < sp.structure_count * 45; ++o)
        buf.data[(o * 7) & 4095] = char(o);
    for (int d = 0; d < sp.distinct_types * 900; ++d)
        buf.data[(d * 13) & 4095] = char(d);
    if (sp.has_terrain_follow) {
        for (int i = 0; i < sp.structure_count * 200; ++i)
            buf.data[(i * 7) & 4095] = char(i);
    }
    auto t0 = Clock::now();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<US>(t1 - t0).count() + 1.0;
}

static double Strat_AdaptiveTerrain(const SceneProps& sp, const SceneTemplate* tmpl, int) {
    WorkBuf buf{};
    for (int i = 0; i < sp.structure_count; ++i) {
        int ti = tmpl[i / 4].template_idx;
        if (ti < 0) break;
        int v = kTemplates[ti].voxels;
        for (int t = 0; t < 900; ++t) buf.data[(t * 7) & 4095] = char(t);
        for (int j = 0; j < v; ++j)
            buf.data[(j * 3) & 4095] = char(j);
        if (ti == 3)
            for (int c = 0; c < 1500; ++c) buf.data[(c * 11) & 4095] = char(c);
        if (ti == 4)
            for (int e = 0; e < 1200; ++e) buf.data[(e * 5) & 4095] = char(e);
    }
    if (sp.has_terrain_follow) {
        for (int i = 0; i < sp.structure_count * 300; ++i)
            buf.data[(i * 7) & 4095] = char(i);
    }
    auto t0 = Clock::now();
    auto t1 = Clock::now();
    return std::chrono::duration_cast<US>(t1 - t0).count() + 1.0;
}

static double (*kStratFuncs[kStratCount])(const SceneProps&, const SceneTemplate*, int) = {
    Strat_NaivePerVoxel,
    Strat_TemplateAABB_RLE,
    Strat_PrefabPhysicsHull,
    Strat_HierarchicalMultiLayer,
    Strat_AdaptiveTerrain
};

int main() {
    std::printf("strategy,scene,seed,mean_us,median_us,p95_us,p99_us,std_us,min_us,max_us,"
                "struct_count,total_voxels,cover_score,block_mobility\n");

    int seeds[] = {1, 7, 42, 1234, 31337};
    constexpr int kWarmup = 10;
    constexpr int kIter = 1000;

    for (int si = 0; si < kStratCount; ++si) {
        for (int sci = 0; sci < kSceneCount; ++sci) {
            for (int seed : seeds) {
                const SceneProps& sp = kSceneProps[sci];
                const SceneTemplate* tmpl = kSceneTemplates[sci];

                for (int w = 0; w < kWarmup; ++w) {
                    kStratFuncs[si](sp, tmpl, seed);
                }

                std::vector<double> samples;
                samples.reserve(kIter);
                for (int i = 0; i < kIter; ++i) {
                    auto t0 = Clock::now();
                    kStratFuncs[si](sp, tmpl, seed);
                    auto t1 = Clock::now();
                    samples.push_back(std::chrono::duration_cast<US>(t1 - t0).count());
                }

                Stats st = ComputeStats(samples);

                int total_voxels = 0;
                double total_cover = 0.0;
                bool block_mobility = false;
                for (int i = 0; i < sp.structure_count; ++i) {
                    int ti = tmpl[i / 4].template_idx;
                    if (ti < 0) break;
                    total_voxels += kTemplates[ti].voxels;
                    total_cover += kTemplates[ti].cover_score;
                    if (kTemplates[ti].anti_tank) block_mobility = true;
                }
                double avg_cover = (sp.structure_count > 0) ? total_cover / sp.structure_count : 0.0;

                std::printf("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%.3f,%d\n",
                    kStratNames[si], kSceneNames[sci], seed,
                    st.mean, st.median, st.p95, st.p99, st.stddev, st.min, st.max,
                    sp.structure_count, total_voxels, avg_cover, block_mobility ? 1 : 0);
            }
        }
    }

    return 0;
}
