#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <random>
#include <vector>

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
static constexpr int kNumWarmup = 10;
static constexpr int kNumIter  = 1000;
static constexpr int kNumSeeds = 5;
static constexpr int kSeedVals[] = {1, 7, 42, 1234, 31337};

enum Scene : int {
  SCENE_UNIFORM_FLOOR = 0,
  SCENE_FOREST_FLOOR,
  SCENE_CAVE_STRESS,
  SCENE_SUNSET_SKY,
  SCENE_EMISSIVE_BLOCKS,
  kNumScenes
};

static const char* kSceneNames[] = {
  "uniform_floor", "forest_floor", "cave_stress", "sunset_sky", "emissive_blocks"
};

enum Strategy : int {
  STRAT_A_LINEAR       = 0,
  STRAT_B_REINHARD_GL  = 1,
  STRAT_C_REINHARD_LUM = 2,
  STRAT_D_ACES_NARK    = 3,
  STRAT_E_ACES_LUT32   = 4,
  STRAT_F_UNREAL       = 5,
  STRAT_G_HABLE        = 6,
  STRAT_H_UCHIMURA     = 7,
  STRAT_I_HEJL_DAWSON  = 8,
  kNumStrats
};

static const char* kStratNames[] = {
  "A_LinearNoTonemap", "B_ReinhardGlobal", "C_ReinhardLuminance",
  "D_ACES_Narkowicz",  "E_ACES_1p3_LUT32", "F_UnrealFilmic",
  "G_HableColorGrade", "H_Uchimura",        "I_HejlDawson"
};

// ---------------------------------------------------------------------------
// float3
// ---------------------------------------------------------------------------
struct float3 { float x, y, z; };
static float3 operator+(float3 a, float3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static float3 operator-(float3 a, float3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static float3 operator-(float3 a, float b)  { return {a.x-b, a.y-b, a.z-b}; }
static float3 operator*(float3 a, float b)  { return {a.x*b, a.y*b, a.z*b}; }
static float3 operator*(float a, float3 b)  { return {a*b.x, a*b.y, a*b.z}; }
static float3 operator*(float3 a, float3 b) { return {a.x*b.x, a.y*b.y, a.z*b.z}; }
static float3 operator/(float3 a, float b)  { return {a.x/b, a.y/b, a.z/b}; }
static float3 operator/(float3 a, float3 b) { return {a.x/b.x, a.y/b.y, a.z/b.z}; }
static float3 operator+(float3 a, float b)  { return {a.x+b, a.y+b, a.z+b}; }
static float3 clamp(float3 v, float lo, float hi) {
  return {std::clamp(v.x, lo, hi), std::clamp(v.y, lo, hi), std::clamp(v.z, lo, hi)};
}
static float3 max(float3 v, float t) { return {std::max(v.x,t), std::max(v.y,t), std::max(v.z,t)}; }
static float3 lerp(float3 a, float3 b, float t) { return a + (b - a) * t; }
static float smoothstep(float e0, float e1, float x) {
  float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

struct Pixel { float r, g, b; };

// ---------------------------------------------------------------------------
// Scene generators
// ---------------------------------------------------------------------------
using SceneGen = std::vector<Pixel>(*)(std::mt19937&);

static std::vector<Pixel> gen_uniform_floor(std::mt19937& rng) {
  std::vector<Pixel> px(512);
  std::uniform_real_distribution<float> d(0.0f, 1.0f);
  for (auto& p : px) { float b = d(rng); p = {b*0.8f, b*0.6f, b*0.4f}; }
  return px;
}
static std::vector<Pixel> gen_forest_floor(std::mt19937& rng) {
  std::vector<Pixel> px(512);
  std::uniform_real_distribution<float> d(0.0f, 1.5f);
  for (auto& p : px) { float g = d(rng); float r = g*(0.4f+0.6f*d(rng)); float b = g*(0.2f+0.3f*d(rng)); p = {r,g,b}; }
  return px;
}
static std::vector<Pixel> gen_cave_stress(std::mt19937& rng) {
  std::vector<Pixel> px(512);
  std::uniform_real_distribution<float> dark(0.0f, 0.05f), bright(5.0f, 50.0f);
  for (int i = 0; i < 512; ++i) { float v = (i%16<2) ? bright(rng) : dark(rng); px[i] = {v, v*0.8f, v*0.5f}; }
  return px;
}
static std::vector<Pixel> gen_sunset_sky(std::mt19937& rng) {
  std::vector<Pixel> px(512);
  std::uniform_real_distribution<float> sun(1.0f, 20.0f), sky(0.0f, 0.3f);
  for (int i = 0; i < 512; ++i) {
    float h = std::abs((i%32)-16)/16.0f;
    if (h < 0.2f) { float s = sun(rng); px[i] = {s, s*0.6f, s*0.1f}; }
    else { float b = sky(rng); px[i] = {b, b*0.5f, b*1.5f}; }
  }
  return px;
}
static std::vector<Pixel> gen_emissive_blocks(std::mt19937& rng) {
  std::vector<Pixel> px(512);
  std::uniform_real_distribution<float> em(2.0f, 10.0f), dark(0.0f, 0.1f);
  for (int i = 0; i < 512; ++i) {
    if ((i/8)%2==0 && i%8<3) { float e = em(rng); float p = (float)(i%3); px[i] = {p<1?e:dark(rng), p>=1&&p<2?e:dark(rng), p>=2?e:dark(rng)}; }
    else px[i] = {dark(rng), dark(rng), dark(rng)};
  }
  return px;
}
static constexpr SceneGen kSceneGens[] = {gen_uniform_floor, gen_forest_floor, gen_cave_stress, gen_sunset_sky, gen_emissive_blocks};

// ---------------------------------------------------------------------------
// Tonemap operators
// ---------------------------------------------------------------------------
static float3 reinhard_global(float3 c) { return c / (1.0f + c.x); }

static float3 reinhard_luminance(float3 c) {
  float lum = 0.2126f*c.x + 0.7152f*c.y + 0.0722f*c.z;
  float l_out = lum / (1.0f + lum);
  return (lum > 1e-6f) ? c * (l_out / lum) : float3{0,0,0};
}

static float3 aces_narkowicz(float3 x) {
  static constexpr float a=2.51f, b=0.03f, c=2.43f, d=0.59f, e=0.14f;
  return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0f, 1.0f);
}

// ACES 1.3 via 33x33x33 LUT (Hill fit for saturated color accuracy)
static std::array<std::array<std::array<float3, 33>, 33>, 33> g_lut;

static void build_aces_lut() {
  auto hill = [](float3 x) -> float3 {
    float3 ap1 = {0.6954522414f*x.x+0.1406786965f*x.y+0.1638690622f*x.z,
                  0.0447945634f*x.x+0.8596711185f*x.y+0.0955343182f*x.z,
                  0.0080214459f*x.x+0.0956383214f*x.y+0.8963402427f*x.z};
    auto c7 = [](float xv) { constexpr float a=0.645f,b=0.042f,c=1.439f,d=0.221f,e=0.030f; return (xv*(a*xv+b))/(xv*(c*xv+d)+e); };
    float3 t = {c7(ap1.x), c7(ap1.y), c7(ap1.z)};
    float3 ap0 = {1.4514393161f*t.x-0.2365107469f*t.y-0.2149285693f*t.z,
                 -0.0765537734f*t.x+1.1762296998f*t.y-0.0996759264f*t.z,
                  0.0083161484f*t.x-0.0060324498f*t.y+0.9977163014f*t.z};
    auto c9 = [](float xv) { constexpr float a=1.713f,b=0.052f,c=1.544f,d=0.221f,e=0.029f; return (xv*(a*xv+b))/(xv*(c*xv+d)+e); };
    float3 o = {c9(ap0.x), c9(ap0.y), c9(ap0.z)};
    return clamp({std::pow(o.x,1/2.4f), std::pow(o.y,1/2.4f), std::pow(o.z,1/2.4f)}, 0.0f, 1.0f);
  };
  for (int r=0; r<=32; ++r) for (int g=0; g<=32; ++g) for (int b=0; b<=32; ++b)
    g_lut[r][g][b] = hill({r/3.2f, g/3.2f, b/3.2f});
}

static float3 aces_lut32(float3 x) {
  float3 sx = clamp(x, 0.0f, 10.0f) * (32.0f / 10.0f);
  int ri = std::min((int)sx.x, 31), gi = std::min((int)sx.y, 31), bi = std::min((int)sx.z, 31);
  float rf = sx.x - ri, gf = sx.y - gi, bf = sx.z - bi;
  auto& l = g_lut;
  float3 c000=l[ri][gi][bi], c100=l[ri+1][gi][bi], c010=l[ri][gi+1][bi], c110=l[ri+1][gi+1][bi];
  float3 c001=l[ri][gi][bi+1], c101=l[ri+1][gi][bi+1], c011=l[ri][gi+1][bi+1], c111=l[ri+1][gi+1][bi+1];
  return lerp(lerp(lerp(c000,c100,rf), lerp(c010,c110,rf), gf), lerp(lerp(c001,c101,rf), lerp(c011,c111,rf), gf), bf);
}

static float3 unreal_filmic(float3 x) {
  float3 t = max(x - 0.004f, 0.0f);
  return (t * (6.2f*t + 0.5f)) / (t * (6.2f*t + 1.7f) + 0.06f);
}

static float3 hable_color_grade(float3 x) {
  constexpr float A=0.15f, B=0.50f, C=0.10f, D=0.20f, E=0.02f, F=0.30f;
  auto f = [&](float3 v) { return ((v*(A*v+C*B)+D*E)/(v*(A*v+B)+D*F))-E/F; };
  constexpr float W=11.2f;
  float3 curr = f(x*2.0f);
  float ws = 1.0f / f({W,W,W}).x;
  return clamp(curr * ws, 0.0f, 1.0f);
}

static float3 uchimura(float3 x) {
  constexpr float P=1.0f, a=1.0f, m=0.22f, l=0.4f, c=1.33f, b=0.0f;
  auto f = [&](float v) {
    float l0 = ((P-m)*l)/a, S0=m+l0, S1=m+a*l0, C2=(a*P)/(P-S1), CP=-C2/P;
    float T = m*std::pow(std::max(v/m,0.0f),c)+b;
    float S = P-(P-S1)*std::exp(CP*(v-S0));
    float L = m+a*(v-m);
    float w0=1.0f-smoothstep(0.0f,m,v), w2=v>=(m+l0)?1.0f:0.0f, w1=1.0f-w0-w2;
    return T*w0+L*w1+S*w2;
  };
  return {f(x.x), f(x.y), f(x.z)};
}

static float3 hejl_dawson(float3 x) {
  float3 v = max(x*16.0f - 0.004f, 0.0f);
  return (v * (6.2f*v + 0.5f)) / (v * (6.2f*v + 1.7f) + 0.06f);
}

using TonemapFn = float3(*)(float3);
static constexpr TonemapFn kFns[] = {
  [](float3 x){return clamp(x,0.0f,1.0f);}, reinhard_global, reinhard_luminance,
  aces_narkowicz, aces_lut32, unreal_filmic, hable_color_grade, uchimura, hejl_dawson
};

// ---------------------------------------------------------------------------
// Measurement
// ---------------------------------------------------------------------------
struct Result {
  double mean_ns, median_ns, p95_ns, std_ns, psnr_db, mr, mg, mb;
};

static Result measure(Strategy s, const std::vector<Pixel>& px, int iter) {
  std::vector<double> ts(iter);
  std::vector<Pixel> out(px.size());
  auto fn = kFns[s];
  for (int i = 0; i < kNumWarmup; ++i)
    for (size_t j = 0; j < px.size(); ++j) { float3 c = fn({px[j].r, px[j].g, px[j].b}); out[j] = {c.x, c.y, c.z}; }
  for (int t = 0; t < iter; ++t) {
    auto start = std::chrono::steady_clock::now();
    for (size_t j = 0; j < px.size(); ++j) { float3 c = fn({px[j].r, px[j].g, px[j].b}); out[j] = {c.x, c.y, c.z}; }
    auto end = std::chrono::steady_clock::now();
    ts[t] = std::chrono::duration<double, std::nano>(end - start).count();
  }
  std::sort(ts.begin(), ts.end());
  Result r{};
  r.mean_ns = std::accumulate(ts.begin(), ts.end(), 0.0) / iter;
  r.median_ns = ts[iter/2];
  r.p95_ns = ts[(int)(iter*0.95)];
  double sq = 0;
  for (auto t : ts) sq += (t - r.mean_ns)*(t - r.mean_ns);
  r.std_ns = std::sqrt(sq/iter);
  double mse = 0;
  for (size_t j = 0; j < px.size(); ++j) {
    float3 ref = kFns[STRAT_E_ACES_LUT32]({px[j].r, px[j].g, px[j].b});
    float dr = out[j].r - ref.x, dg = out[j].g - ref.y, db = out[j].b - ref.z;
    mse += dr*dr + dg*dg + db*db;
    r.mr += out[j].r; r.mg += out[j].g; r.mb += out[j].b;
  }
  mse /= (3.0 * px.size());
  r.psnr_db = (mse > 1e-12) ? 10.0*std::log10(1.0/mse) : 100.0;
  r.mr /= px.size(); r.mg /= px.size(); r.mb /= px.size();
  return r;
}

int main() {
  build_aces_lut();
  std::printf("strat,scene,seed,mean_ns,median_ns,p95_ns,std_ns,psnr_db,mean_r,mean_g,mean_b\n");
  for (int si = 0; si < kNumStrats; ++si) {
    for (int sci = 0; sci < kNumScenes; ++sci) {
      for (int seed = 0; seed < kNumSeeds; ++seed) {
        std::mt19937 rng(kSeedVals[seed]);
        auto px = kSceneGens[sci](rng);
        auto res = measure((Strategy)si, px, kNumIter);
        std::printf("%s,%s,%d,%.3f,%.3f,%.3f,%.3f,%.2f,%.4f,%.4f,%.4f\n",
          kStratNames[si], kSceneNames[sci], kSeedVals[seed],
          res.mean_ns, res.median_ns, res.p95_ns, res.std_ns,
          res.psnr_db, res.mr, res.mg, res.mb);
      }
    }
  }
  return 0;
}
