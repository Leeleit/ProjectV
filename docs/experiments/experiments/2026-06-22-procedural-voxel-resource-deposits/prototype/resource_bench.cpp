// resource_bench.cpp — procedural voxel resource deposit benchmark
// 5 strategies × 5 scenes × 5 seeds × 50 iterations = 6250 measurements
// 8³ chunk (512 voxels) = canonical ProjectV chunk

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <random>
#include <span>
#include <string_view>
#include <vector>

// ============================================================
// Simplex noise (3D) — simplified for geology context
// ============================================================
static const int grad3[12][3] = {
    {1,1,0},{-1,1,0},{1,-1,0},{-1,-1,0},
    {1,0,1},{-1,0,1},{1,0,-1},{-1,0,-1},
    {0,1,1},{0,-1,1},{0,1,-1},{0,-1,-1}
};
static const unsigned char perm[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

static float dot3(const int g[3], float x, float y, float z) {
    return g[0]*x + g[1]*y + g[2]*z;
}

static float simplex3(float x, float y, float z) {
    float n0=0,n1=0,n2=0,n3=0;
    float s = (x+y+z)*(1.f/3.f);
    int i = int(std::floor(x+s)), j = int(std::floor(y+s)), k = int(std::floor(z+s));
    float t = (i+j+k)*(1.f/6.f);
    float X0 = i-t, Y0 = j-t, Z0 = k-t;
    float x0 = x-X0, y0 = y-Y0, z0 = z-Z0;
    int i1,j1,k1,i2,j2,k2;
    if(x0>=y0){if(y0>=z0){i1=1;j1=0;k1=0;i2=1;j2=1;k2=0;}
               else if(x0>=z0){i1=1;j1=0;k1=0;i2=1;j2=0;k2=1;}
               else {i1=0;j1=0;k1=1;i2=1;j2=0;k2=1;}}
    else{if(y0<z0){i1=0;j1=0;k1=1;i2=0;j2=1;k2=1;}
         else if(x0<z0){i1=0;j1=1;k1=0;i2=0;j2=1;k2=1;}
         else {i1=0;j1=1;k1=0;i2=1;j2=1;k2=0;}}
    float x1=x0-i1+1.f/6.f, y1=y0-j1+1.f/6.f, z1=z0-k1+1.f/6.f;
    float x2=x0-i2+2.f/6.f, y2=y0-j2+2.f/6.f, z2=z0-k2+2.f/6.f;
    float x3=x0-1.f/2.f, y3=y0-1.f/2.f, z3=z0-1.f/2.f;
    int ii=i&255, jj=j&255, kk=k&255;
    auto p = [&](int a){return perm[(a)&255];};
    int gi0 = p(ii+p(jj+p(kk)))%12;
    float t0 = 0.6f-x0*x0-y0*y0-z0*z0; if(t0>0){t0*=t0; n0=t0*t0*dot3(grad3[gi0],x0,y0,z0);}
    int gi1 = p(ii+i1+p(jj+j1+p(kk+k1)))%12;
    float t1 = 0.6f-x1*x1-y1*y1-z1*z1; if(t1>0){t1*=t1; n1=t1*t1*dot3(grad3[gi1],x1,y1,z1);}
    int gi2 = p(ii+i2+p(jj+j2+p(kk+k2)))%12;
    float t2 = 0.6f-x2*x2-y2*y2-z2*z2; if(t2>0){t2*=t2; n2=t2*t2*dot3(grad3[gi2],x2,y2,z2);}
    int gi3 = p(ii+1+p(jj+1+p(kk+1)))%12;
    float t3 = 0.6f-x3*x3-y3*y3-z3*z3; if(t3>0){t3*=t3; n3=t3*t3*dot3(grad3[gi3],x3,y3,z3);}
    return 32.f*(n0+n1+n2+n3);
}

// ============================================================
// Geology helpers
// ============================================================
static constexpr int CS = 8; // chunk size
static constexpr int CS3 = CS*CS*CS; // 512
static constexpr int CS2 = CS*CS; // 64

static int idx(int x, int y, int z) { return (y<<6)|(z<<3)|x; }

// Geology layers via noise
static std::array<float,CS3> noise_layer(float freq=0.2f, int octaves=2) {
    std::array<float,CS3> out{};
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        float v = 0, amp=1, f=freq;
        for (int o=0; o<octaves; ++o) { v += amp*simplex3(x*f,y*f,z*f); amp*=0.5f; f*=2; }
        out[idx(x,y,z)] = v;
    }
    return out;
}

// ============================================================
// Scene generators
// ============================================================
enum Scene : int {
    SCENE_UNIFORM=0, SCENE_STRATIFIED, SCENE_GRANITE,
    SCENE_FOLDED, SCENE_VOLCANIC, SCENE_COUNT
};
static const char* scene_names[] = {
    "uniform_stone","stratified_3layer","granite_pegmatite",
    "folded_metamorphic","volcanic_complex"
};

static void gen_uniform_stone(std::span<uint8_t,CS3> chunk, uint64_t) {
    for (int i=0; i<CS3; ++i) chunk[i]=1;
}
static void gen_stratified_3layer(std::span<uint8_t,CS3> chunk, uint64_t) {
    auto n = noise_layer(0.15f,2);
    // 3 material types
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        float ny = n[idx(x,y,z)] + y*0.3f;
        if (ny < -0.3f) chunk[idx(x,y,z)] = 2;
        else if (ny < 0.3f) chunk[idx(x,y,z)] = 3;
        else chunk[idx(x,y,z)] = 1;
    }
}
static void gen_granite_pegmatite(std::span<uint8_t,CS3> chunk, uint64_t) {
    for (int i=0; i<CS3; ++i) chunk[i]=1;
    // Intrude pegmatite blobs
    auto n = noise_layer(0.25f,3);
    for (int i=0; i<CS3; ++i) {
        if (n[i] > 0.4f && (i%7!=0)) chunk[i]=4;
    }
}
static void gen_folded_metamorphic(std::span<uint8_t,CS3> chunk, uint64_t seed) {
    std::mt19937 rng(seed);
    auto n = noise_layer(0.2f,3);
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        float s = std::sin(x*0.8f + z*0.5f + y*0.3f)*0.5f + n[idx(x,y,z)]*0.5f;
        if (s < -0.2f) chunk[idx(x,y,z)] = 1;
        else if (s < 0.2f) chunk[idx(x,y,z)] = 5;
        else chunk[idx(x,y,z)] = 3;
    }
}
static void gen_volcanic_complex(std::span<uint8_t,CS3> chunk, uint64_t seed) {
    std::mt19937 rng(seed);
    auto n = noise_layer(0.3f,4);
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        // Columnar basalt structure
        float cx = x-CS/2.f, cz = z-CS/2.f;
        float dist = std::sqrt(cx*cx+cz*cz);
        float col = std::max(0.f, 1.f - dist/(CS*0.4f));
        float v = n[idx(x,y,z)] + col*0.6f - y*0.15f;
        if (v > 0.1f) chunk[idx(x,y,z)] = 1;
        else if (v > -0.1f) chunk[idx(x,y,z)] = 6;
        else chunk[idx(x,y,z)] = 0; // air
    }
}

using SceneFn = void(*)(std::span<uint8_t,CS3>, uint64_t);
static const SceneFn scene_fns[] = {
    gen_uniform_stone, gen_stratified_3layer, gen_granite_pegmatite,
    gen_folded_metamorphic, gen_volcanic_complex
};

// ============================================================
// Strategy interface
// ============================================================
struct DepositInfo {
    int count;           // number of resource voxels placed
    int components;      // connected components (6-connectivity)
    float plausibility;  // 0–1 geological plausibility score
};

using StrategyFn = DepositInfo(*)(std::span<const uint8_t,CS3> chunk, uint8_t resourceType, uint64_t seed, int scene_id);

// ---- Strategy A: Uniform random baseline ----
static DepositInfo strat_A_UniformRandom(std::span<const uint8_t,CS3> chunk,
                                         [[maybe_unused]] uint8_t resourceType, uint64_t seed, int) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0,1);
    float density = 0.12f;
    DepositInfo result{0,0,0};
    uint8_t deposit[CS3]{};

    for (int i=0; i<CS3; ++i) {
        if (chunk[i]==0) continue;
        if (dist(rng) < density) { deposit[i]=1; ++result.count; }
    }

    // Connected component analysis (6-connectivity)
    bool visited[CS3]{};
    int components = 0;
    int queue[CS3];
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i] || visited[i]) continue;
        ++components;
        int head=0, tail=0;
        queue[tail++] = i;
        visited[i] = true;
        while (head < tail) {
            int ci = queue[head++];
            int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
            static const int dx[]={1,-1,0,0,0,0};
            static const int dy[]={0,0,1,-1,0,0};
            static const int dz[]={0,0,0,0,1,-1};
            for (int d=0; d<6; ++d) {
                int nx = cx+dx[d], ny = cy+dy[d], nz = cz+dz[d];
                if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
                int ni = idx(nx,ny,nz);
                if (deposit[ni] && !visited[ni]) { visited[ni]=true; queue[tail++]=ni; }
            }
        }
    }
    result.components = components;

    // Plausibility: components relative to deposit count
    if (result.count > 0)
        result.plausibility = 1.0f - std::min(1.0f, components / std::max(1.0f, result.count*0.15f));
    return result;
}

// ---- Strategy B: Seam-boundary enhanced ----
static DepositInfo strat_B_SeamBoundary(std::span<const uint8_t,CS3> chunk,
                                        [[maybe_unused]] uint8_t resourceType, uint64_t seed, int) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(0,1);
    DepositInfo result{0,0,0};
    uint8_t deposit[CS3]{};
    bool isSeam[CS3]{};

    // Detect seams: voxel with at least one neighbor of different material
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        int i = idx(x,y,z);
        if (chunk[i]==0) continue;
        static const int dx[]={1,-1,0,0,0,0};
        static const int dy[]={0,0,1,-1,0,0};
        static const int dz[]={0,0,0,0,1,-1};
        for (int d=0; d<6; ++d) {
            int nx=x+dx[d], ny=y+dy[d], nz=z+dz[d];
            if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
            if (chunk[idx(nx,ny,nz)] != chunk[i]) { isSeam[i]=true; break; }
        }
    }

    // Manhattan distance to nearest seam via BFS
    int seamDist[CS3];
    std::fill(seamDist, seamDist+CS3, 99);
    unsigned bfsQ[CS3];
    unsigned head=0, tail=0;
    for (int i=0; i<CS3; ++i) if (isSeam[i]) { seamDist[i]=0; bfsQ[tail++]=i; }
    while (head < tail) {
        int ci = bfsQ[head++];
        int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
        static const int dx[]={1,-1,0,0,0,0};
        static const int dy[]={0,0,1,-1,0,0};
        static const int dz[]={0,0,0,0,1,-1};
        for (int d=0; d<6; ++d) {
            int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
            if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
            int ni = idx(nx,ny,nz);
            if (seamDist[ni] > seamDist[ci]+1) { seamDist[ni]=seamDist[ci]+1; bfsQ[tail++]=ni; }
        }
    }

    for (int i=0; i<CS3; ++i) {
        if (chunk[i]==0) continue;
        float prob;
        if (seamDist[i]==0) prob = 0.4f;
        else if (seamDist[i]<=2) prob = 0.12f * (3-seamDist[i])/3.0f;
        else prob = 0.012f;
        if (dist(rng) < prob) { deposit[i]=1; ++result.count; }
    }

    // Component analysis
    bool visited[CS3]{};
    int components = 0;
    int queue[CS3];
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i] || visited[i]) continue;
        ++components;
        int head=0, tail=0;
        queue[tail++] = i;
        visited[i] = true;
        while (head < tail) {
            int ci = queue[head++];
            int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
            static const int dx[]={1,-1,0,0,0,0};
            static const int dy[]={0,0,1,-1,0,0};
            static const int dz[]={0,0,0,0,1,-1};
            for (int d=0; d<6; ++d) {
                int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
                if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
                int ni = idx(nx,ny,nz);
                if (deposit[ni] && !visited[ni]) { visited[ni]=true; queue[tail++]=ni; }
            }
        }
    }
    result.components = components;

    // Plausibility: seam proximity bonus
    float seamScore = 0;
    int seamCount = 0;
    for (int i=0; i<CS3; ++i) {
        if (deposit[i] && seamDist[i] <= 1) { seamScore += 1; seamCount++; }
    }
    float seamRatio = result.count > 0 ? seamCount/float(result.count) : 0;
    if (result.count > 0)
        result.plausibility = 0.3f*seamRatio + 0.7f*(1 - std::min(1.0f, components/std::max(1.0f, result.count*0.2f)));
    return result;
}

// ---- Strategy C: Perlin worm veins ----
static DepositInfo strat_C_PerlinWorm(std::span<const uint8_t,CS3> chunk,
                                      uint8_t resourceType, uint64_t seed, int) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist01(0,1);
    std::uniform_real_distribution<float> wormStart(1,CS-1);
    std::uniform_real_distribution<float> angle(-0.5f,0.5f);
    DepositInfo result{0,0,0};
    uint8_t deposit[CS3]{};

    // Spawn up to 3 worms
    int numWorms = std::uniform_int_distribution<int>(1,3)(rng);
    for (int w=0; w<numWorms; ++w) {
        float wx=wormStart(rng), wy=wormStart(rng), wz=wormStart(rng);
        float dirX=angle(rng), dirY=angle(rng), dirZ=angle(rng);
        int step = 0, maxSteps = CS*4;
        while (step++ < maxSteps) {
            int ix=int(wx), iy=int(wy), iz=int(wz);
            if (ix<0||ix>=CS||iy<0||iy>=CS||iz<0||iz>=CS) break;
            int i = idx(ix,iy,iz);
            if (chunk[i]==resourceType || chunk[i]!=0) {
                deposit[i] = 1;
                ++result.count;
            }
            dirX += angle(rng)*0.3f;
            dirY += angle(rng)*0.3f;
            dirZ += angle(rng)*0.3f;
            float len = std::sqrt(dirX*dirX+dirY*dirY+dirZ*dirZ);
            if (len>0) { dirX/=len; dirY/=len; dirZ/=len; }
            wx += dirX; wy += dirY; wz += dirZ;
        }
    }

    // Component analysis
    bool visited[CS3]{};
    int components = 0;
    int queue[CS3];
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i] || visited[i]) continue;
        ++components;
        int head=0, tail=0;
        queue[tail++] = i;
        visited[i] = true;
        while (head < tail) {
            int ci = queue[head++];
            int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
            static const int dx[]={1,-1,0,0,0,0};
            static const int dy[]={0,0,1,-1,0,0};
            static const int dz[]={0,0,0,0,1,-1};
            for (int d=0; d<6; ++d) {
                int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
                if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
                int ni = idx(nx,ny,nz);
                if (deposit[ni] && !visited[ni]) { visited[ni]=true; queue[tail++]=ni; }
            }
        }
    }
    result.components = components;

    if (result.count > 0 && components > 0) {
        float avgSize = result.count / float(components);
        result.plausibility = std::min(1.0f, avgSize / 10.0f);
    }
    return result;
}

// ---- Strategy D: Voronoi region biomes ----
static DepositInfo strat_D_VoronoiBiome(std::span<const uint8_t,CS3> chunk,
                                        [[maybe_unused]] uint8_t resourceType, uint64_t seed, int scene_id) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> fdist(0,float(CS));
    std::uniform_real_distribution<float> dist01(0,1);
    DepositInfo result{0,0,0};
    uint8_t deposit[CS3]{};

    // 6 Voronoi cell centers (fixed per seed)
    struct Vec3 { float x,y,z; };
    Vec3 cells[6];
    for (int c=0; c<6; ++c) cells[c] = {fdist(rng),fdist(rng),fdist(rng)};

    float density = 0.10f + scene_id*0.02f;
    for (int i=0; i<CS3; ++i) {
        if (chunk[i]==0) continue;
        int x=i&0x7, z=(i>>3)&0x7, y=i>>6;
        float minDist = 999;
        for (int c=0; c<6; ++c) {
            float dx=x-cells[c].x, dy=y-cells[c].y, dz=z-cells[c].z;
            float d = dx*dx+dy*dy+dz*dz;
            if (d < minDist) minDist = d;
        }
        float edgeFactor = std::clamp(minDist/12.0f, 0.0f, 1.0f);
        float prob = density * (1.0f + edgeFactor*0.5f);
        if (dist01(rng) < prob) { deposit[i]=1; ++result.count; }
    }

    // Component analysis
    bool visited[CS3]{};
    int components = 0;
    int queue[CS3];
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i] || visited[i]) continue;
        ++components;
        int head=0, tail=0;
        queue[tail++] = i; visited[i] = true;
        while (head < tail) {
            int ci = queue[head++];
            int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
            static const int dx[]={1,-1,0,0,0,0};
            static const int dy[]={0,0,1,-1,0,0};
            static const int dz[]={0,0,0,0,1,-1};
            for (int d=0; d<6; ++d) {
                int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
                if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
                int ni = idx(nx,ny,nz);
                if (deposit[ni] && !visited[ni]) { visited[ni]=true; queue[tail++]=ni; }
            }
        }
    }
    result.components = components;

    // Plausibility: cluster-around-cells
    float cellProx = 0;
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i]) continue;
        int x=i&0x7, z=(i>>3)&0x7, y=i>>6;
        float minD = 999;
        for (int c=0; c<6; ++c) {
            float d = (x-cells[c].x)*(x-cells[c].x)+(y-cells[c].y)*(y-cells[c].y)+(z-cells[c].z)*(z-cells[c].z);
            if (d<minD) minD=d;
        }
        if (minD < 16) cellProx += 1;
    }
    float proxRatio = result.count>0 ? cellProx/result.count : 0;
    if (result.count > 0)
        result.plausibility = 0.4f*proxRatio + 0.6f*(1-std::min(1.0f, components/std::max(1.0f, result.count*0.15f)));
    return result;
}

// ---- Strategy E: Hybrid worm + seam ----
static DepositInfo strat_E_Hybrid_WormPlusSeam(std::span<const uint8_t,CS3> chunk,
                                               [[maybe_unused]] uint8_t resourceType, uint64_t seed, int) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist01(0,1);
    std::uniform_real_distribution<float> angle(-0.5f,0.5f);
    std::uniform_real_distribution<float> ws(0.5f, CS-0.5f);
    DepositInfo result{0,0,0};
    uint8_t deposit[CS3]{};
    bool isSeam[CS3]{};

    // Seam detection
    for (int y=0; y<CS; ++y) for (int z=0; z<CS; ++z) for (int x=0; x<CS; ++x) {
        int i = idx(x,y,z);
        if (chunk[i]==0) continue;
        static const int dx[]={1,-1,0,0,0,0};
        static const int dy[]={0,0,1,-1,0,0};
        static const int dz[]={0,0,0,0,1,-1};
        for (int d=0; d<6; ++d) {
            int nx=x+dx[d], ny=y+dy[d], nz=z+dz[d];
            if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
            if (chunk[idx(nx,ny,nz)] != chunk[i]) { isSeam[i]=true; break; }
        }
    }

    // Near-seam Manhattan distance BFS
    int seamDist[CS3];
    std::fill(seamDist, seamDist+CS3, 99);
    unsigned bfsQ[CS3];
    unsigned head=0, tail=0;
    for (int i=0; i<CS3; ++i) if (isSeam[i]) { seamDist[i]=0; bfsQ[tail++]=i; }
    while (head < tail) {
        int ci = bfsQ[head++];
        int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
        static const int dx[]={1,-1,0,0,0,0};
        static const int dy[]={0,0,1,-1,0,0};
        static const int dz[]={0,0,0,0,1,-1};
        for (int d=0; d<6; ++d) {
            int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
            if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
            int ni = idx(nx,ny,nz);
            if (seamDist[ni] > seamDist[ci]+1) { seamDist[ni]=seamDist[ci]+1; bfsQ[tail++]=ni; }
        }
    }

    // Seam phase: place deposit at seam nuclei
    for (int i=0; i<CS3; ++i) {
        if (chunk[i]==0 || !isSeam[i]) continue;
        if (dist01(rng) < 0.25f) { deposit[i]=1; ++result.count; }
    }

    // Worm phase: grow from seam nuclei
    int numWorms = std::uniform_int_distribution<int>(1,2)(rng);
    for (int w=0; w<numWorms; ++w) {
        float wx=ws(rng), wy=ws(rng), wz=ws(rng);
        float dirX=angle(rng), dirY=angle(rng), dirZ=angle(rng);
        int step=0, maxSteps=CS*3;
        // Try seeding worm near a seam voxel
        int seamIdx = -1;
        for (int i=0; i<CS3; ++i) if (isSeam[i]) { seamIdx=i; break; }
        if (seamIdx >= 0) {
            wx = float(seamIdx&0x7); wz = float((seamIdx>>3)&0x7); wy = float(seamIdx>>6);
        }
        while (step++ < maxSteps) {
            int ix=int(wx), iy=int(wy), iz=int(wz);
            if (ix<0||ix>=CS||iy<0||iy>=CS||iz<0||iz>=CS) break;
            int i = idx(ix,iy,iz);
            if (chunk[i]!=0 && !deposit[i]) { deposit[i]=1; ++result.count; }
            dirX += angle(rng)*0.25f;
            dirY += angle(rng)*0.25f;
            dirZ += angle(rng)*0.25f;
            float len = std::sqrt(dirX*dirX+dirY*dirY+dirZ*dirZ);
            if (len>0) { dirX/=len; dirY/=len; dirZ/=len; }
            wx += dirX; wy += dirY; wz += dirZ;
        }
    }

    // Expand: seam-near deposit at 30%
    for (int i=0; i<CS3; ++i) {
        if (chunk[i]==0 || deposit[i]) continue;
        if (seamDist[i]<=1 && dist01(rng)<0.3f) { deposit[i]=1; ++result.count; }
    }

    // Component analysis
    bool visited[CS3]{};
    int components = 0;
    int queue[CS3];
    for (int i=0; i<CS3; ++i) {
        if (!deposit[i] || visited[i]) continue;
        ++components;
        int head=0, tail=0;
        queue[tail++] = i; visited[i] = true;
        while (head < tail) {
            int ci = queue[head++];
            int cx = ci&0x7, cz = (ci>>3)&0x7, cy = ci>>6;
            static const int dx[]={1,-1,0,0,0,0};
            static const int dy[]={0,0,1,-1,0,0};
            static const int dz[]={0,0,0,0,1,-1};
            for (int d=0; d<6; ++d) {
                int nx=cx+dx[d], ny=cy+dy[d], nz=cz+dz[d];
                if (nx<0||nx>=CS||ny<0||ny>=CS||nz<0||nz>=CS) continue;
                int ni = idx(nx,ny,nz);
                if (deposit[ni] && !visited[ni]) { visited[ni]=true; queue[tail++]=ni; }
            }
        }
    }
    result.components = components;

    // Plausibility: seam proximity + component coherence
    float seamProx=0;
    for (int i=0; i<CS3; ++i) if (deposit[i] && seamDist[i]<=1) seamProx++;
    float seamRatio = result.count>0 ? seamProx/result.count : 0;
    float coherence = result.count > 0 ? 1-std::min(1.0f, components/std::max(1.0f, result.count*0.1f)) : 0;
    result.plausibility = 0.5f*seamRatio + 0.5f*coherence;
    return result;
}

// ============================================================
// Strategy table
// ============================================================
static constexpr StrategyFn strategies[] = {
    strat_A_UniformRandom,
    strat_B_SeamBoundary,
    strat_C_PerlinWorm,
    strat_D_VoronoiBiome,
    strat_E_Hybrid_WormPlusSeam
};
static constexpr const char* strat_names[] = {
    "A_UniformRandom","B_SeamBoundary","C_PerlinWorm",
    "D_VoronoiBiome","E_Hybrid_WormPlusSeam"
};
static constexpr int NUM_STRATS = 5;
static constexpr int NUM_SCENES = 5;
static constexpr int NUM_SEEDS = 5;
static constexpr int WARMUP = 5;
static constexpr int ITERATIONS = 50;

// ============================================================
// Main
// ============================================================
int main() {
    using Clock = std::chrono::steady_clock;

    std::printf("strategy,scene,seed,iter,time_ns,count,components,plausibility\n");

    for (int si = 0; si < NUM_STRATS; ++si) {
        for (int sc = 0; sc < NUM_SCENES; ++sc) {
            for (int seed = 1; seed <= NUM_SEEDS; ++seed) {
                // Generate scene once per seed
                uint8_t sceneData[CS3];
                auto chunk = std::span<uint8_t,CS3>(sceneData);
                scene_fns[sc](chunk, seed);

                uint8_t resourceType = uint8_t(2 + (seed % 5));  // different resource per seed
                auto chunk_const = std::span<const uint8_t,CS3>(sceneData);

                // Warmup
                for (int w = 0; w < WARMUP; ++w) {
                    strategies[si](chunk_const, resourceType, uint64_t(seed)*1000 + w, sc);
                }

                // Measured iterations
                for (int iter = 0; iter < ITERATIONS; ++iter) {
                    auto t0 = Clock::now();
                    auto info = strategies[si](chunk_const, resourceType, uint64_t(seed)*1000 + iter + WARMUP, sc);
                    auto t1 = Clock::now();
                    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
                    std::printf("%s,%s,%d,%d,%lld,%d,%d,%.4f\n",
                                strat_names[si], scene_names[sc], seed, iter,
                                (long long)ns, info.count, info.components, info.plausibility);
                }
            }
        }
    }
    return 0;
}
