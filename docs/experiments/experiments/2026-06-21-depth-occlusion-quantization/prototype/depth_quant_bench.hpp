#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "voxel_scene.hpp"

namespace depth_quant {

// 16-bit unorm depth conversion (Vulkan spec: maps [0,1] -> [0, 2^16-1])
inline std::uint16_t floatToD16(float z) {
    if (z < 0.0f) z = 0.0f;
    if (z > 1.0f) z = 1.0f;
    return static_cast<std::uint16_t>(z * 65535.0f + 0.5f);
}

inline float d16ToFloat(std::uint16_t d) {
    return static_cast<float>(d) / 65535.0f;
}

// 32-bit float depth (raw bits preserved)
inline std::uint32_t floatToD32(float z) {
    std::uint32_t bits;
    std::memcpy(&bits, &z, sizeof(bits));
    return bits;
}

inline float d32ToFloat(std::uint32_t bits) {
    float z;
    std::memcpy(&z, &bits, sizeof(z));
    return z;
}

// Reverse-Z transform: standard depth (near=0, far=1) -> reversed (near=1, far=0)
inline float toReverseZ(float z) { return 1.0f - z; }
inline float fromReverseZ(float z) { return 1.0f - z; }

// PSNR between two depth arrays (per-pixel). Returns 0.0 if MSE == 0.
double computePSNR(const std::vector<float>& ref, const std::vector<float>& test);

// Simulate HZB mip-chain cull on a depth buffer + a list of bounding boxes (in NDC).
// Returns visible count (correct vs HZB-decided) для measurement of false-cull rate.
struct BoundingBox {
    float minX, minY, minZ, maxX, maxY, maxZ;  // NDC space
};

struct CullResult {
    int total;
    int visibleCorrect;
    int falseCulled;  // HZB says culled but actually visible
    int falseVisible; // HZB says visible but actually occluded (less critical)
};

CullResult simulateHZBCull(
    const std::vector<float>& depthStandardZ,  // ground truth depth (standard Z, near=0, far=1)
    const std::vector<float>& depthHypothesis, // HZB-quantized depth (whatever format)
    int width, int height,
    DepthFormat fmt,  // determines precision loss
    const std::vector<BoundingBox>& boxes,
    bool reverseZ
);

// VRAM per fullscreen depth attachment at given resolution.
struct VramSize {
    double depthAttachmentMiB;
    double hzbMipchainMiB;
    double totalMiB;
};

VramSize computeVram(int width, int height, DepthFormat fmt, int hzbLevels = 8);

// Configuration matrix
struct BenchConfig {
    int sceneIdx;
    int width;
    int height;
    int viewDistanceM;  // simulated draw distance (e.g. 64, 128, 256)
    DepthFormat depthFormat;
    CullPattern cullPattern;
};

struct BenchResult {
    BenchConfig config;
    double vramMiB;
    double psnrDB;
    int falseCulled;
    int totalBoxes;
    double meanCullError;  // avg depth error at HZB cull test point
};

std::vector<BenchResult> runAnalyticalBenchmark(
    const std::vector<BenchConfig>& configs,
    int warmupIters = 100,
    int measureIters = 1000
);

}  // namespace depth_quant
