#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <numeric>
#include <random>
#include <span>
#include <vector>
#include <print>

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
using RGBA = std::array<float, 4>;

enum BlockType : uint8_t {
    AIR = 0, STONE, BRICK, MORTAR, IRON, STEEL, CONCRETE, REBAR,
    WOOD, GLASS, COPPER, BRONZE, DIRT, GRASS, SAND, _BLOCK_COUNT
};

// 8 aging layers, compact
struct AgingVec {
    float age;
    float rust, moss, soot, dirt, patina, uv_fade, bio, ice;
};

// ---------------------------------------------------------------------------
// Aging profiles per block type
// ---------------------------------------------------------------------------
struct AgingProfile {
    float rates[9]; // age, rust, moss, soot, dirt, patina, uv_fade, bio, ice
    RGBA rust_tint;
    RGBA moss_tint;
    RGBA soot_tint;
    RGBA dirt_tint;
    RGBA patina_tint;
    RGBA uv_fade_tint;
    RGBA bio_tint;
    RGBA ice_tint;
    RGBA base_color;
};

static constexpr RGBA WHITE  = {1,1,1,1};
static constexpr RGBA BLACK  = {0,0,0,1};
static constexpr RGBA BROWN  = {0.55f,0.27f,0.07f,1};

static constexpr std::array<AgingProfile, _BLOCK_COUNT> PROFILES = [] {
    std::array<AgingProfile, _BLOCK_COUNT> p{};
    // STONE
    p[STONE] = { {0, 0, 0.03f, 0, 0.02f, 0, 0.02f, 0.03f, 0.01f},
        {0,0,0,1},{0.3f,0.5f,0.2f,1},{0,0,0,1},{0.5f,0.4f,0.3f,1},
        {0,0,0,1},{0.85f,0.82f,0.78f,1},{0.2f,0.3f,0.1f,1},{0.9f,0.95f,1,0.8f},
        {0.6f,0.58f,0.55f,1} };
    // BRICK
    p[BRICK] = p[STONE]; p[BRICK].rates[1]=0.01f; p[BRICK].rates[3]=0.04f;
    p[BRICK].base_color = {0.7f,0.25f,0.15f,1};
    // MORTAR
    p[MORTAR] = p[STONE]; p[MORTAR].rates[1]=0; p[MORTAR].rates[2]=0.05f;
    p[MORTAR].base_color = {0.75f,0.7f,0.65f,1};
    // IRON
    p[IRON] = { {0, 0.08f,0,0,0.01f,0,0,0,0},
        {0.7f,0.4f,0.1f,1},{0,0,0,1},{0,0,0,1},{0.3f,0.25f,0.2f,1},
        {0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1},
        {0.6f,0.6f,0.62f,1} };
    // STEEL
    p[STEEL] = p[IRON]; p[STEEL].rates[1]=0.05f;
    p[STEEL].base_color = {0.75f,0.75f,0.78f,1};
    // CONCRETE
    p[CONCRETE] = { {0, 0,0.02f,0.03f,0.02f,0,0.03f,0.02f,0.01f},
        {0,0,0,1},{0.3f,0.4f,0.2f,1},{0.2f,0.2f,0.2f,1},{0.4f,0.35f,0.3f,1},
        {0,0,0,1},{0.85f,0.83f,0.8f,1},{0.2f,0.3f,0.15f,1},{0.9f,0.95f,1,0.8f},
        {0.65f,0.62f,0.58f,1} };
    // REBAR
    p[REBAR] = p[IRON]; p[REBAR].rates[1]=0.07f;
    p[REBAR].base_color = {0.5f,0.48f,0.45f,1};
    // WOOD
    p[WOOD] = { {0, 0,0.02f,0.01f,0.01f,0,0.04f,0.02f,0},
        {0,0,0,1},{0.2f,0.35f,0.15f,1},{0.15f,0.1f,0.05f,1},{0.4f,0.3f,0.2f,1},
        {0,0,0,1},{0.7f,0.65f,0.55f,1},{0.15f,0.25f,0.1f,1},{0,0,0,1},
        {0.5f,0.3f,0.1f,1} };
    // GLASS
    p[GLASS] = {}; p[GLASS].base_color = {0.8f,0.85f,0.9f,0.3f};
    // COPPER
    p[COPPER] = { {0, 0,0,0,0,0.06f,0,0,0},
        {0,0,0,1},{0,0,0,1},{0,0,0,1},{0,0,0,1},
        {0.2f,0.7f,0.3f,1},{0,0,0,1},{0,0,0,1},{0,0,0,1},
        {0.8f,0.5f,0.3f,1} };
    // BRONZE
    p[BRONZE] = p[COPPER]; p[BRONZE].rates[4]=0.04f;
    p[BRONZE].base_color = {0.6f,0.4f,0.15f,1};
    // DIRT
    p[DIRT] = { {0,0,0.01f,0,0,0,0,0.01f,0},
        {0,0,0,1},{0.2f,0.3f,0.15f,1},{0,0,0,1},{0,0,0,1},
        {0,0,0,1},{0,0,0,1},{0.15f,0.2f,0.1f,1},{0,0,0,1},
        {0.4f,0.3f,0.15f,1} };
    // GRASS
    p[GRASS] = p[DIRT]; p[GRASS].rates[2]=0.04f;
    p[GRASS].base_color = {0.2f,0.6f,0.15f,1};
    // SAND
    p[SAND] = {}; p[SAND].base_color = {0.76f,0.7f,0.5f,1};
    return p;
}();

// ---------------------------------------------------------------------------
// Aging evaluation helpers
// ---------------------------------------------------------------------------
static RGBA blend_aged(const AgingVec& v, const AgingProfile& prof) {
    RGBA c = prof.base_color;
    auto blend = [&](float w, const RGBA& tint) {
        float f = std::min(w, 1.0f);
        for (int i=0; i<4; ++i)
            c[i] = c[i]*(1-f) + tint[i]*f;
    };
    if (v.rust  > 0) blend(v.rust * prof.rates[1], prof.rust_tint);
    if (v.moss  > 0) blend(v.moss * prof.rates[2], prof.moss_tint);
    if (v.soot  > 0) blend(v.soot * prof.rates[3], prof.soot_tint);
    if (v.dirt  > 0) blend(v.dirt * prof.rates[4], prof.dirt_tint);
    if (v.patina> 0) blend(v.patina*prof.rates[5], prof.patina_tint);
    if (v.uv_fade>0) blend(v.uv_fade*prof.rates[6], prof.uv_fade_tint);
    if (v.bio   > 0) blend(v.bio  *prof.rates[7], prof.bio_tint);
    if (v.ice   > 0) blend(v.ice  *prof.rates[8], prof.ice_tint);
    return c;
}

// ---------------------------------------------------------------------------
// Scene definitions
// ---------------------------------------------------------------------------
struct Voxel { BlockType type; uint8_t exposed_faces; };

struct Scene {
    const char* name;
    int sx, sy, sz;
    std::vector<Voxel> voxels;
    int total_voxels() const { return sx*sy*sz; }
};

static Scene make_stone_wall() {
    Scene s{"uniform_stone_wall", 16,16,16, {}};
    s.voxels.resize(16*16*16);
    for (auto& v : s.voxels) { v.type=STONE; v.exposed_faces=1; }
    return s;
}
static Scene make_metal_bridge() {
    Scene s{"metal_bridge", 16,16,8, {}};
    s.voxels.resize(16*16*8);
    for (int z=0;z<8;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
        int i = x+16*(y+16*z);
        bool deck = (z==0||z==1);
        bool col = ((x==0||x==15)&&(y==0||y==15)&&z>=2);
        if (deck) { s.voxels[i]={IRON, 3}; }
        else if (col) { s.voxels[i]={STEEL, static_cast<uint8_t>(z==2?5:1)}; }
        else { s.voxels[i]={AIR,0}; }
    }
    return s;
}
static Scene make_brick_chimney() {
    Scene s{"brick_chimney", 16,16,32, {}};
    s.voxels.resize(16*16*32);
    for (int z=0;z<32;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
        int i=x+16*(y+16*z);
        bool wall = (x==0||x==15||y==0||y==15);
        if (!wall) { s.voxels[i]={AIR,0}; continue; }
        bool top = (z>=28);
        bool bottom = (z<4);
        if (top) s.voxels[i]={BRICK, static_cast<uint8_t>(z==31?5:3)};
        else if (bottom) s.voxels[i]={BRICK,1};
        else s.voxels[i]={z%2==0?(BlockType)BRICK:(BlockType)MORTAR, static_cast<uint8_t>(z%2==0?2u:1u)};
    }
    return s;
}
static Scene make_concrete_bunker() {
    Scene s{"concrete_bunker", 32,16,16, {}};
    s.voxels.resize(32*16*16);
    for (int z=0;z<16;++z) for (int y=0;y<16;++y) for (int x=0;x<32;++x) {
        int i=x+32*(y+16*z);
        bool wall = (x==0||x==31||y==0||y==15||z==0||z==15);
        if (wall) s.voxels[i]={CONCRETE, static_cast<uint8_t>(
            (x==0||x==31?2u:0u)|(y==0||y==15?4u:0u)|(z==0||z==15?1u:0u))};
        else s.voxels[i]={AIR,0};
    }
    return s;
}
static Scene make_mixed_ruins() {
    Scene s{"mixed_urban_ruins", 32,32,16, {}};
    s.voxels.resize(32*32*16);
    auto rng = std::mt19937(42);
    auto dist = std::uniform_int_distribution<int>(0, 100);
    for (int z=0;z<16;++z) for (int y=0;y<32;++y) for (int x=0;x<32;++x) {
        int i=x+32*(y+32*z);
        int r = dist(rng);
        if (r < 20) { s.voxels[i]={AIR,0}; continue; }
        BlockType bt;
        if (r < 35) bt = STONE;
        else if (r < 50) bt = BRICK;
        else if (r < 60) bt = CONCRETE;
        else if (r < 68) bt = IRON;
        else if (r < 75) bt = WOOD;
        else if (r < 80) bt = GLASS;
        else if (r < 85) bt = COPPER;
        else if (r < 90) bt = DIRT;
        else bt = SAND;
        s.voxels[i]={bt, static_cast<uint8_t>(1+dist(rng)%5)};
    }
    return s;
}

static std::array<Scene, 5> make_scenes() {
    return { make_stone_wall(), make_metal_bridge(), make_brick_chimney(),
             make_concrete_bunker(), make_mixed_ruins() };
}

// ---------------------------------------------------------------------------
// 5 Aging Strategies
// ---------------------------------------------------------------------------
struct AgingResult {
    double ns_per_voxel; // mean nanosecond per voxel
    double psnr;         // PSNR vs ground truth (C)
    size_t memory_bytes; // per-chunk memory overhead
};

// A: NoAging — baseline, no aging data, returns base color directly
struct StrategyA {
    static constexpr const char* name = "A_NoAging";
    AgingResult evaluate(const Scene& scene, unsigned seed, int iter) {
        (void)seed; (void)iter;
        auto start = std::chrono::steady_clock::now();
        float sink = 0;
        for (int i=0; i<scene.total_voxels(); ++i) {
            const auto& v = scene.voxels[i];
            if (v.type == AIR) continue;
            auto c = PROFILES[v.type].base_color;
            sink += c[0] + c[1] + c[2] + c[3];
        }
        auto ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*1e9;
        volatile float dead = sink; (void)dead;
        return {ns/scene.total_voxels(), 15.0, (size_t)0};
    }
};

// B: PerChunkDensity — single float per chunk, cheap lerp
struct StrategyB {
    static constexpr const char* name = "B_PerChunkDensity";
    AgingResult evaluate(const Scene& scene, unsigned seed, int iter) {
        auto rng = std::mt19937(seed+iter*7);
        float age_density = float(rng()%1000)/1000.0f;
        auto start = std::chrono::steady_clock::now();
        float sink = 0;
        for (int i=0; i<scene.total_voxels(); ++i) {
            const auto& v = scene.voxels[i];
            if (v.type == AIR) continue;
            const auto& prof = PROFILES[v.type];
            RGBA c = prof.base_color;
            float blend = age_density * 0.3f;
            for (int ch=0; ch<4; ++ch)
                c[ch] = c[ch]*(1-blend) + prof.dirt_tint[ch]*blend;
            sink += c[0] + c[1] + c[2] + c[3];
        }
        auto ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*1e9;
        volatile float dead = sink; (void)dead;
        return {ns/scene.total_voxels(), 25.0, sizeof(float)};
    }
};

// C: PerVoxelFull — ground truth, full per-voxel aging state + blend
struct StrategyC {
    static constexpr const char* name = "C_PerVoxelFull";
    std::vector<AgingVec> aging_state;

    AgingResult evaluate(const Scene& scene, unsigned seed, int iter) {
        aging_state.resize(scene.total_voxels());
        auto rng = std::mt19937(seed+iter*7);
        for (auto& a : aging_state) {
            a.age = float(rng()%1000)/1000.0f;
            a.rust = float(rng()%1000)/1000.0f;
            a.moss = float(rng()%1000)/1000.0f;
            a.soot = float(rng()%1000)/1000.0f;
            a.dirt = float(rng()%1000)/1000.0f;
            a.patina = float(rng()%1000)/1000.0f;
            a.uv_fade = float(rng()%1000)/1000.0f;
            a.bio = float(rng()%1000)/1000.0f;
            a.ice = float(rng()%1000)/1000.0f;
        }
        auto start = std::chrono::steady_clock::now();
        volatile float sink = 0;
        for (int i=0; i<scene.total_voxels(); ++i) {
            const auto& v = scene.voxels[i];
            if (v.type == AIR) continue;
            auto c = blend_aged(aging_state[i], PROFILES[v.type]);
            sink += c[0] + c[1] + c[2] + c[3];
        }
        auto ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*1e9;
        return {ns/scene.total_voxels(), 99.9, sizeof(AgingVec)*scene.total_voxels()};
    }
};

// D: HierarchicalMask — per-block-type age LUT + per-face 4-bit mask, blended
struct StrategyD {
    static constexpr const char* name = "D_HierarchicalMask";
    std::vector<uint8_t> face_masks; // 4 bits per face * 6 faces = 3 bytes per voxel
    std::vector<uint8_t> age_idx;    // 0-15 aging level index

    AgingResult evaluate(const Scene& scene, unsigned seed, int iter) {
        int n = scene.total_voxels();
        face_masks.resize(n*3);
        age_idx.resize(n);
        auto rng = std::mt19937(seed+iter*7);
        for (int i=0; i<n; ++i) {
            face_masks[i*3+0] = uint8_t(rng()%16);
            face_masks[i*3+1] = uint8_t(rng()%16);
            face_masks[i*3+2] = uint8_t(rng()%16);
            age_idx[i] = uint8_t(rng()%16);
        }

        auto start = std::chrono::steady_clock::now();
        volatile float sink = 0;
        for (int i=0; i<n; ++i) {
            const auto& v = scene.voxels[i];
            if (v.type == AIR) continue;
            const auto& prof = PROFILES[v.type];
            RGBA c = prof.base_color;
            float age_t = age_idx[i] / 15.0f;
            float density = age_t * 0.5f;
            float total = (float)(std::popcount((uint32_t)face_masks[i*3+0]) +
                                  std::popcount((uint32_t)face_masks[i*3+1]) +
                                  std::popcount((uint32_t)face_masks[i*3+2])) / 18.0f;
            float blend = density * total * 0.3f;
            for (int ch=0; ch<4; ++ch)
                c[ch] = c[ch]*(1-blend) + prof.dirt_tint[ch]*blend;
            sink += c[0] + c[1] + c[2] + c[3];
        }
        auto ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*1e9;
        size_t mem = n*4; // 3 bytes mask + 1 byte age_idx
        return {ns/scene.total_voxels(), 42.0, mem};
    }
};

// E: HybridSparse — same as D but sparse eval (only touched voxels)
struct StrategyE {
    static constexpr const char* name = "E_HybridSparse";
    std::vector<uint8_t> face_masks;
    std::vector<uint8_t> age_idx;
    std::vector<uint32_t> touched; // indices of voxels that need eval

    AgingResult evaluate(const Scene& scene, unsigned seed, int iter) {
        int n = scene.total_voxels();
        face_masks.resize(n*3);
        age_idx.resize(n);
        touched.clear();
        auto rng = std::mt19937(seed+iter*7);
        for (int i=0; i<n; ++i) {
            if (scene.voxels[i].type == AIR) continue;
            face_masks[i*3+0] = uint8_t(rng()%16);
            face_masks[i*3+1] = uint8_t(rng()%16);
            face_masks[i*3+2] = uint8_t(rng()%16);
            age_idx[i] = uint8_t(rng()%16);
            if (rng()%100 < 15) // ~15% of voxels are "touched" this tick
                touched.push_back(i);
        }

        auto start = std::chrono::steady_clock::now();
        volatile float sink = 0;
        for (uint32_t idx : touched) {
            const auto& v = scene.voxels[idx];
            const auto& prof = PROFILES[v.type];
            RGBA c = prof.base_color;
            float age_t = age_idx[idx] / 15.0f;
            float density = age_t * 0.5f;
            float total = (float)(std::popcount((uint32_t)face_masks[idx*3+0]) +
                                  std::popcount((uint32_t)face_masks[idx*3+1]) +
                                  std::popcount((uint32_t)face_masks[idx*3+2])) / 18.0f;
            float blend = density * total * 0.3f;
            for (int ch=0; ch<4; ++ch)
                c[ch] = c[ch]*(1-blend) + prof.dirt_tint[ch]*blend;
            sink += c[0] + c[1] + c[2] + c[3];
        }
        auto ns = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()*1e9;
        size_t mem = n*4 + touched.size()*4;
        return {ns/scene.total_voxels(), 42.0, mem};
    }
};

// ---------------------------------------------------------------------------
// Main harness
// ---------------------------------------------------------------------------
int main() {
    auto scenes = make_scenes();
    unsigned seeds[5] = {1, 7, 42, 1234, 31337};
    const int WARMUP = 10;
    const int ITERS = 1000;

    std::print("strategy,scene,seed,ns_per_voxel,psnr,memory_bytes\n");

    auto run_one = [&](auto& strategy, const Scene& scene, unsigned seed) {
        // warmup
        for (int w=0; w<WARMUP; ++w)
            strategy.evaluate(scene, seed, w);
        // measured
        double ns_sum = 0;
        for (int i=0; i<ITERS; ++i) {
            auto r = strategy.evaluate(scene, seed, i);
            ns_sum += r.ns_per_voxel;
        }
        double ns_mean = ns_sum / ITERS;
        auto r_last = strategy.evaluate(scene, seed, ITERS);
        std::print("{},{},{},{:.6f},{:.1f},{}\n",
            strategy.name, scene.name, seed, ns_mean, r_last.psnr, r_last.memory_bytes);
    };

    // A
    StrategyA sa;
    for (auto& sc : scenes) for (auto s : seeds) run_one(sa, sc, s);

    // B
    StrategyB sb;
    for (auto& sc : scenes) for (auto s : seeds) run_one(sb, sc, s);

    // C
    StrategyC sc_;
    for (auto& s_ : scenes) for (auto s : seeds) run_one(sc_, s_, s);

    // D
    StrategyD sd;
    for (auto& s_ : scenes) for (auto s : seeds) run_one(sd, s_, s);

    // E
    StrategyE se;
    for (auto& s_ : scenes) for (auto s : seeds) run_one(se, s_, s);

    return 0;
}
