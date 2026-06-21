#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <numeric>
#include <random>
#include <span>
#include <vector>

// ============================================================
// Scene definitions — 5 scenes consistent with prior experiments
// ============================================================

struct Scene {
    const char* name;
    float nearPlane;       // meters
    float farPlane;        // meters
    float avgHeight;       // average pixel height above ground (m)
    float skyVisibility;   // fraction of sky visible from scene (0-1)
    float turbidity;       // atmospheric turbidity (1=clear, 10=hazy)
    float sunZenith;       // sun zenith angle in radians
};

static constexpr Scene kScenes[] = {
    {"uniform_floor",  1.f, 120.f, 1.6f,  0.95f, 2.5f, 0.52f},
    {"forest_floor",   1.f,  80.f, 1.9f,  0.40f, 3.5f, 0.61f},
    {"cave_stress",    1.f,  20.f, 0.2f,  0.05f, 8.0f, 2.09f},
    {"mixed_biome",    1.f, 150.f, 3.5f,  0.60f, 4.0f, 0.70f},
    {"canyon_deep",    1.f, 300.f, 2.8f,  0.75f, 3.0f, 0.48f},
};
static constexpr size_t kSceneCount = std::size(kScenes);

// ============================================================
// Seeds (consistent with prior experiments)
// ============================================================
static constexpr uint64_t kSeeds[] = {1, 7, 42, 1234, 31337};
static constexpr size_t kSeedCount = std::size(kSeeds);

// ============================================================
// Strategy definitions
// ============================================================

enum class StrategyId : uint8_t {
    A_None = 0,
    B_LinearDistance,
    C_ExponentialDistance,
    D_ExponentialHeightFog,
    E_AnalyticPreetham,
};
static constexpr const char* kStrategyNames[] = {
    "A_None",
    "B_LinearDistance",
    "C_ExponentialDistance",
    "D_ExponentialHeightFog",
    "E_AnalyticPreetham",
};
static constexpr size_t kStrategyCount = std::size(kStrategyNames);

struct StrategyCost {
    double aluOps;           // estimated ALU ops per pixel
    double textureSamples;   // estimated texture/SSBO samples per pixel
    double bandwidthBytes;   // estimated bandwidth per pixel (bytes)
    double vramBytes;        // estimated VRAM usage (bytes)
};

// ============================================================
// Optical models
// ============================================================

struct FogResult {
    float opacity;       // fog opacity (0=clear, 1=fully fogged)
    float colorR, colorG, colorB;  // fog RGB color (linear)
};

// Rayleigh phase function
static float rayleighPhase(float cosTheta) {
    return (3.f / (16.f * std::numbers::pi_v<float>)) * (1.f + cosTheta * cosTheta);
}

// Mie phase function (Henyey-Greenstein)
static float miePhase(float cosTheta, float g) {
    float denom = 1.f + g * g - 2.f * g * cosTheta;
    return (1.f / (4.f * std::numbers::pi_v<float>)) * (1.f - g * g) / (denom * std::sqrt(denom));
}

// Beer-Lambert transmittance
static float transmittance(float opticalDepth) {
    return std::exp(-opticalDepth);
}

// ---- Strategy A: None ----
static FogResult evalA([[maybe_unused]] float dist, [[maybe_unused]] float height,
                       [[maybe_unused]] float viewCos, [[maybe_unused]] float sunCos,
                       [[maybe_unused]] const Scene& scene) {
    return {0.f, 0.f, 0.f, 0.f};
}

// ---- Strategy B: Linear Distance ----
static FogResult evalB(float dist, float /*height*/, float /*viewCos*/, float sunCos,
                       const Scene& scene) {
    float start   = scene.nearPlane;
    float range   = scene.farPlane - start;
    float opacity = std::clamp((dist - start) / range, 0.f, 1.f);

    float sunFactor = std::pow(std::max(sunCos, 0.f), 0.5f);
    float skyBlue   = scene.skyVisibility;

    float gray  = 0.5f + 0.3f * sunFactor;
    float r = gray * (1.f - 0.5f * skyBlue);
    float g = gray * (1.f - 0.3f * skyBlue);
    float b = gray * (1.f + 0.5f * skyBlue);

    return {opacity, r, g, b};
}

// ---- Strategy C: Exponential Distance (Wenzel 2006 / Beer-Lambert) ----
static FogResult evalC(float dist, float /*height*/, float /*viewCos*/, float sunCos,
                       const Scene& scene) {
    float density      = 0.008f * (1.f + 0.3f * (scene.turbidity - 2.5f));
    float opticalDepth = density * dist;
    float opacity      = 1.f - transmittance(opticalDepth);

    float sunFactor = std::pow(std::max(sunCos, 0.f), 1.5f);
    float skyBlue   = scene.skyVisibility;

    float fogGray = 0.4f + 0.4f * sunFactor;
    float r = fogGray * (1.f - 0.6f * skyBlue);
    float g = fogGray * (1.f - 0.3f * skyBlue);
    float b = fogGray * (1.f + 0.8f * skyBlue);

    return {opacity, r, g, b};
}

// ---- Strategy D: Exponential Height Fog (Filament style) ----
static FogResult evalD(float dist, float height, float /*viewCos*/, float sunCos,
                       const Scene& scene) {
    float densityAtSea  = 0.012f * (1.f + 0.4f * (scene.turbidity - 2.5f));
    float falloffRate   = 0.15f; // per meter
    float heightFactor  = std::exp(-falloffRate * std::max(height, 0.f));

    float density        = densityAtSea * heightFactor;
    float opticalDepth   = density * dist;
    float opacity        = 1.f - transmittance(opticalDepth);
    opacity              = std::min(opacity, 0.95f);

    float sunFactor      = std::pow(std::max(sunCos, 0.f), 2.0f);
    float skyBlue        = scene.skyVisibility;

    float r = 0.35f - 0.20f * skyBlue + 0.25f * sunFactor;
    float g = 0.38f - 0.10f * skyBlue + 0.20f * sunFactor;
    float b = 0.42f + 0.35f * skyBlue + 0.10f * sunFactor;

    return {opacity, r, g, b};
}

// ---- Strategy E: Analytic Preetham scattering model ----
// Simplified Preetham 1999 analytic model
static FogResult evalE(float dist, float /*height*/, float /*viewCos*/, float sunCos,
                       const Scene& scene) {
    float betaR      = 0.008f;  // Rayleigh scattering coefficient
    float betaM      = 0.012f;  // Mie scattering coefficient
    float gMie       = 0.76f;   // Mie asymmetry factor

    float densityScale = 1.f + 0.5f * (scene.turbidity - 2.5f) / 7.5f;

    float opticalDepthR = betaR * densityScale * dist;
    float opticalDepthM = betaM * densityScale * dist;

    float transmittanceR = std::exp(-opticalDepthR);
    float transmittanceM = std::exp(-opticalDepthM);

    float opacity = 1.f - (transmittanceR * transmittanceM);
    opacity = std::min(opacity, 0.98f);

    float phaseR = rayleighPhase(sunCos);
    float phaseM = miePhase(sunCos, gMie);

    float inscatterR = (1.f - transmittanceR) * phaseR / (opticalDepthR + 1e-6f);
    float inscatterM = (1.f - transmittanceM) * phaseM / (opticalDepthM + 1e-6f);

    float sunIntensity = std::max(sunCos, 0.01f);
    float skyLight = 0.3f + 0.2f * scene.skyVisibility;

    float r = inscatterR * 0.15f + inscatterM * 0.20f * sunIntensity + skyLight * 0.08f;
    float g = inscatterR * 0.30f + inscatterM * 0.22f * sunIntensity + skyLight * 0.08f;
    float b = inscatterR * 0.80f + inscatterM * 0.18f * sunIntensity + skyLight * 0.10f;

    return {opacity, r, g, b};
}

using EvalFn = FogResult (*)(float, float, float, float, const Scene&);

static constexpr std::array<EvalFn, kStrategyCount> kEvalFns = {evalA, evalB, evalC, evalD, evalE};

// ============================================================
// Cost model (analytical)
// ============================================================

static StrategyCost computeCost(StrategyId id, [[maybe_unused]] const Scene& scene) {
    switch (id) {
    case StrategyId::A_None:
        return {0.0, 0.0, 0.0, 0.0};
    case StrategyId::B_LinearDistance: {
        // distance-based lerp: ~30 ALU, 0 texture samples
        double aluPerPixel = 30.0;
        return {aluPerPixel, 0.0, aluPerPixel * 4.0, 0.0};
    }
    case StrategyId::C_ExponentialDistance: {
        // exp(-density*dist): ~50 ALU, 0 texture samples
        double aluPerPixel = 50.0;
        return {aluPerPixel, 0.0, aluPerPixel * 4.0, 0.0};
    }
    case StrategyId::D_ExponentialHeightFog: {
        // height*distance exp: ~70 ALU, 0 texture samples
        double aluPerPixel = 70.0;
        return {aluPerPixel, 0.0, aluPerPixel * 4.0, 0.0};
    }
    case StrategyId::E_AnalyticPreetham: {
        // Rayleigh + Mie phase + 2x exp: ~200 ALU, 0 texture samples
        double aluPerPixel = 200.0;
        return {aluPerPixel, 0.0, aluPerPixel * 4.0, 0.0};
    }
    }
    return {};
}

// ============================================================
// PSNR computation — use E as reference for B/C/D
// ============================================================

static float computePSNR(std::span<const float> signal, std::span<const float> reference) {
    float mse = 0.f;
    for (size_t i = 0; i < signal.size(); ++i) {
        float diff = signal[i] - reference[i];
        mse += diff * diff;
    }
    mse /= static_cast<float>(signal.size());
    if (mse < 1e-10f) return 99.99f;
    return 10.f * std::log10(1.f / mse);
}

// ============================================================
// Measurement harness
// ============================================================

struct Measurement {
    StrategyId strategy;
    int sceneIdx;
    int seed;
    double meanOpMs;      // estimated GPU time per pixel (microseconds * 1e-6)
    double psnr;          // PSNR in dB vs reference
    double vramMiB;       // VRAM usage in MiB
    double opacityMean;   // mean opacity across samples
    double opacityStd;    // std dev of opacity
};

static double estimateGpuTimeUs(const StrategyCost& cost) {
    double aluTime = cost.aluOps * 0.001;  // ~1 ns per ALU op at 1GHz
    double sampleTime = cost.textureSamples * 0.005; // ~5 ns per texel sample
    double bwTime = cost.bandwidthBytes * 0.0005; // ~0.5 ns per byte
    return aluTime + sampleTime + bwTime;
}

int main() {
    std::printf("strategy,scene,seed,meanOpMs,psnr,vramMiB,opacityMean,opacityStd\n");

    for (int s = 0; s < static_cast<int>(kStrategyCount); ++s) {
        StrategyId sid = static_cast<StrategyId>(s);

        for (int sc = 0; sc < static_cast<int>(kSceneCount); ++sc) {
            const Scene& scene = kScenes[sc];

            for (int seedIdx = 0; seedIdx < static_cast<int>(kSeedCount); ++seedIdx) {
                uint64_t seed = kSeeds[seedIdx];
                std::mt19937_64 rng(seed);

                // Generate random sample points for evaluation
                static constexpr int kSamples = 4000;
                std::vector<float> opacities(kSamples);
                std::vector<float> refOpacities(kSamples);
                std::vector<float> sigR(kSamples), sigG(kSamples), sigB(kSamples);
                std::vector<float> refR(kSamples), refG(kSamples), refB(kSamples);

                for (int i = 0; i < kSamples; ++i) {
                    float dist = 0.f, height = 0.f, viewCos = 0.f, sunCos = 0.f;

                    if (sid == StrategyId::A_None) {
                        dist = scene.nearPlane + (scene.farPlane - scene.nearPlane) *
                               static_cast<float>(rng()) / static_cast<float>(rng.max());
                    } else {
                        float u1 = static_cast<float>(rng()) / static_cast<float>(rng.max());
                        float u2 = static_cast<float>(rng()) / static_cast<float>(rng.max());
                        float u3 = static_cast<float>(rng()) / static_cast<float>(rng.max());

                        dist = scene.nearPlane +
                               (scene.farPlane - scene.nearPlane) * u1;

                        height = scene.avgHeight * (0.2f + 1.8f * u2);

                        viewCos = std::cos(0.5f * u3);

                        float sunOffset = (static_cast<float>(rng()) / static_cast<float>(rng.max()) - 0.5f) * 0.3f;
                        sunCos = std::cos(scene.sunZenith + sunOffset);
                    }

                    float h = height;
                    float vc = viewCos;
                    float sc2 = sunCos;

                    FogResult fr = kEvalFns[s](dist, h, vc, sc2, scene);

                    opacities[i] = fr.opacity;
                    sigR[i] = fr.colorR;
                    sigG[i] = fr.colorG;
                    sigB[i] = fr.colorB;

                    FogResult ref;
                    if (sid == StrategyId::E_AnalyticPreetham) {
                        ref = fr;
                    } else if (sid == StrategyId::A_None) {
                        ref = {0.f, 0.f, 0.f, 0.f};
                    } else {
                        ref = evalE(dist, h, vc, sc2, scene);
                    }

                    refOpacities[i] = ref.opacity;
                    refR[i] = ref.colorR;
                    refG[i] = ref.colorG;
                    refB[i] = ref.colorB;
                }

                float meanOp = std::accumulate(opacities.begin(), opacities.end(), 0.f) /
                               static_cast<float>(kSamples);

                float varOp = 0.f;
                for (int i = 0; i < kSamples; ++i) {
                    float d = opacities[i] - meanOp;
                    varOp += d * d;
                }
                varOp /= static_cast<float>(kSamples);
                float stdOp = std::sqrt(varOp);

                std::vector<float> signal(4 * kSamples);
                std::vector<float> reference(4 * kSamples);
                for (int i = 0; i < kSamples; ++i) {
                    signal[4*i+0] = opacities[i];
                    signal[4*i+1] = sigR[i];
                    signal[4*i+2] = sigG[i];
                    signal[4*i+3] = sigB[i];
                    reference[4*i+0] = refOpacities[i];
                    reference[4*i+1] = refR[i];
                    reference[4*i+2] = refG[i];
                    reference[4*i+3] = refB[i];
                }

                float psnr = computePSNR(signal, reference);

                StrategyCost cost = computeCost(sid, scene);
                double gpuTimeUs = estimateGpuTimeUs(cost);
                // Convert to ms at 1080p (2M pixels)
                double frameMs = gpuTimeUs * 1920.0 * 1080.0 / 1000.0;

                double vramMiB = cost.vramBytes / (1024.0 * 1024.0);

                std::printf("%s,%s,%lu,%.6f,%.2f,%.4f,%.4f,%.4f\n",
                    kStrategyNames[s], scene.name, seed,
                    frameMs, psnr, vramMiB,
                    static_cast<double>(meanOp), static_cast<double>(stdOp));
            }
        }
    }

    return 0;
}
