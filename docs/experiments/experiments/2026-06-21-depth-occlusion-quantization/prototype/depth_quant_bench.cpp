#include "depth_quant_bench.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>

namespace depth_quant {

namespace {

// Generate a synthetic depth buffer for a voxel scene at given resolution + view distance.
// Standard Z: near=0, far=1. More realistic distribution: linear gradient + multiple occluders.
std::vector<float> generateSyntheticDepth(
    const Scene& scene,
    int width, int height,
    int viewDistanceM
) {
    std::vector<float> depth(width * height, 1.0f);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> jitter(-0.02f, 0.02f);

    constexpr float VOXEL_SIZE = 1.0f;
    float cameraDist = static_cast<float>(viewDistanceM) * 0.3f;

    // Base gradient: 0.05 (near) to 1.0 (far) with perspective non-linearity
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;
            float baseDepth = 0.05f + 0.5f * (1.0f - v) * (0.3f + 0.7f * u);
            // Add small noise for realistic variance
            baseDepth += jitter(rng) * 0.01f;
            if (baseDepth < 0.05f) baseDepth = 0.05f;
            if (baseDepth > 1.0f) baseDepth = 1.0f;
            depth[y * width + x] = baseDepth;
        }
    }

    // Add occluders from chunks (foreground blocks)
    for (int cz = 0; cz < Scene::CHUNK_DIM; ++cz) {
        for (int cy = 0; cy < Scene::CHUNK_DIM; ++cy) {
            for (int cx = 0; cx < Scene::CHUNK_DIM; ++cx) {
                const auto& c = scene.chunkAt(cx, cy, cz);
                if (c.isEmpty()) continue;
                float wx = (cx - Scene::CHUNK_DIM/2) * Chunk::SIZE * VOXEL_SIZE;
                float wy = (cy - Scene::CHUNK_DIM/2) * Chunk::SIZE * VOXEL_SIZE;
                float wz = (cz + cameraDist / Chunk::SIZE) * Chunk::SIZE * VOXEL_SIZE;
                float camDist = std::sqrt(wx*wx + wy*wy + wz*wz);
                if (camDist > viewDistanceM) continue;
                float depthNDC = camDist / static_cast<float>(viewDistanceM);
                if (depthNDC < 0.01f) depthNDC = 0.01f;
                if (depthNDC > 1.0f) depthNDC = 1.0f;
                // Project chunk to screen
                int sx = static_cast<int>((wx / viewDistanceM + 0.5f) * width);
                int sy = static_cast<int>((1.0f - (wy / viewDistanceM + 0.5f)) * height);
                int chunkScreenSize = std::max(2, static_cast<int>(Chunk::SIZE * width / (2 * viewDistanceM)));
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= width) sx = width - 1;
                if (sy >= height) sy = height - 1;
                for (int dy = -chunkScreenSize; dy <= chunkScreenSize; ++dy) {
                    for (int dx = -chunkScreenSize; dx <= chunkScreenSize; ++dx) {
                        int xx = sx + dx;
                        int yy = sy + dy;
                        if (xx < 0 || xx >= width || yy < 0 || yy >= height) continue;
                        int idx = yy * width + xx;
                        if (depthNDC < depth[idx]) depth[idx] = depthNDC;
                    }
                }
            }
        }
    }
    return depth;
}

// Quantize a depth buffer to a specific format and back to float.
// For D32_SFLOAT: identity. For D16_UNORM: round-trip precision loss.
std::vector<float> quantizeDepthBuffer(
    const std::vector<float>& srcStandardZ,
    DepthFormat fmt
) {
    std::vector<float> result(srcStandardZ.size());
    for (size_t i = 0; i < srcStandardZ.size(); ++i) {
        float z = srcStandardZ[i];
        if (fmt == DepthFormat::D32_SFLOAT) {
            result[i] = z;
        } else if (fmt == DepthFormat::D16_UNORM) {
            auto d16 = floatToD16(z);
            result[i] = d16ToFloat(d16);
        } else if (fmt == DepthFormat::D16_UNORM_REVERSE_Z) {
            float revZ = toReverseZ(z);
            auto d16 = floatToD16(revZ);
            result[i] = fromReverseZ(d16ToFloat(d16));
        }
    }
    return result;
}

// Build bounding boxes representing chunks (for HZB cull test).
std::vector<BoundingBox> generateTestBoxes(int count, std::uint32_t seed) {
    std::vector<BoundingBox> boxes;
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-0.9f, 0.9f);
    std::uniform_real_distribution<float> sizeDist(0.02f, 0.15f);
    for (int i = 0; i < count; ++i) {
        BoundingBox b;
        float cx = dist(rng);
        float cy = dist(rng) * 0.5f;
        float sz = sizeDist(rng);
        b.minX = cx - sz;
        b.maxX = cx + sz;
        b.minY = cy - sz;
        b.maxY = cy + sz;
        b.minZ = 0.3f;
        b.maxZ = 0.95f;
        boxes.push_back(b);
    }
    return boxes;
}

// HZB-like max reduction over a 2x2 quad.
float maxQuad(const std::vector<float>& buf, int w, int h, int x, int y) {
    float m = 0.0f;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            int xx = std::min(x*2+dx, w-1);
            int yy = std::min(y*2+dy, h-1);
            m = std::max(m, buf[yy * w + xx]);
        }
    }
    return m;
}

std::vector<float> buildHZB(const std::vector<float>& src, int w, int h) {
    std::vector<float> mip0 = src;
    std::vector<float> allMips = mip0;
    int cw = w, ch = h;
    int levels = 1;
    while (cw > 1 || ch > 1) {
        int nw = std::max(1, cw / 2);
        int nh = std::max(1, ch / 2);
        std::vector<float> next(nw * nh);
        for (int y = 0; y < nh; ++y) {
            for (int x = 0; x < nw; ++x) {
                next[y * nw + x] = maxQuad(mip0, cw, ch, x, y);
            }
        }
        allMips.insert(allMips.end(), next.begin(), next.end());
        mip0 = next;
        cw = nw;
        ch = nh;
        ++levels;
        if (levels > 8) break;
    }
    return allMips;
}

int totalHZBSamples(int w, int h);  // (forward decl for unused helper)

int totalHZBSamples(int w, int h) {
    int total = w * h;
    int cw = w, ch = h;
    while (cw > 1 || ch > 1) {
        cw = std::max(1, cw / 2);
        ch = std::max(1, ch / 2);
        total += cw * ch;
    }
    return total;
}

}  // namespace

double computePSNR(const std::vector<float>& ref, const std::vector<float>& test) {
    if (ref.size() != test.size() || ref.empty()) return 0.0;
    double mse = 0.0;
    for (size_t i = 0; i < ref.size(); ++i) {
        double diff = static_cast<double>(ref[i]) - static_cast<double>(test[i]);
        mse += diff * diff;
    }
    mse /= ref.size();
    if (mse < 1e-20) return 100.0;
    constexpr double MAX = 1.0;
    return 10.0 * std::log10(MAX * MAX / mse);
}

CullResult simulateHZBCull(
    const std::vector<float>& depthStandardZ,
    const std::vector<float>& depthHypothesis,
    int width, int height,
    DepthFormat fmt,
    const std::vector<BoundingBox>& boxes,
    bool reverseZ
) {
    CullResult r{};
    r.total = static_cast<int>(boxes.size());
    auto hzbStd = buildHZB(depthStandardZ, width, height);
    auto hzbHyp = buildHZB(depthHypothesis, width, height);
    int mip0Size = width * height;
    int mip1W = std::max(1, width / 2);
    int mip1H = std::max(1, height / 2);

    for (const auto& b : boxes) {
        float bMinZ = b.minZ;
        if (reverseZ) bMinZ = toReverseZ(b.minZ);
        int sx = static_cast<int>((b.minX + 1.0f) * 0.5f * mip1W);
        int sy = static_cast<int>((1.0f - b.maxY) * 0.5f * mip1H);
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        if (sx >= mip1W) sx = mip1W - 1;
        if (sy >= mip1H) sy = mip1H - 1;
        int idx = mip0Size + sy * mip1W + sx;
        if (idx >= static_cast<int>(hzbHyp.size())) idx = hzbHyp.size() - 1;
        float hzbDepthStd = hzbStd[idx];
        float hzbDepthHyp = hzbHyp[idx];
        bool stdVisible = (bMinZ <= hzbDepthStd + 0.001f);
        bool hypVisible = (bMinZ <= hzbDepthHyp + 0.001f);
        if (stdVisible) r.visibleCorrect++;
        if (stdVisible && !hypVisible) r.falseCulled++;
        if (!stdVisible && hypVisible) r.falseVisible++;
    }
    return r;
}

VramSize computeVram(int width, int height, DepthFormat fmt, int hzbLevels) {
    VramSize v{};
    int bytesPerTexel = (fmt == DepthFormat::D32_SFLOAT) ? 4 : 2;
    double depthBytes = static_cast<double>(width) * height * bytesPerTexel;
    v.depthAttachmentMiB = depthBytes / (1024.0 * 1024.0);
    double hzbBytes = depthBytes;
    int cw = width, ch = height;
    for (int i = 1; i < hzbLevels; ++i) {
        cw = std::max(1, cw / 2);
        ch = std::max(1, ch / 2);
        hzbBytes += static_cast<double>(cw) * ch * bytesPerTexel;
        if (cw == 1 && ch == 1) break;
    }
    v.hzbMipchainMiB = hzbBytes / (1024.0 * 1024.0);
    v.totalMiB = v.depthAttachmentMiB + v.hzbMipchainMiB;
    return v;
}

std::vector<BenchResult> runAnalyticalBenchmark(
    const std::vector<BenchConfig>& configs,
    int warmupIters,
    int measureIters
) {
    std::vector<BenchResult> results;
    std::cout << "Running analytical benchmark: "
              << configs.size() << " configs, "
              << warmupIters << " warmup, "
              << measureIters << " measure iters" << std::endl;

    for (const auto& cfg : configs) {
        Scene scene;
        switch (cfg.sceneIdx) {
            case 0: fillSceneForest(scene, 42); break;
            case 1: fillSceneCave(scene, 42); break;
            case 2: fillSceneUniform(scene, 42); break;
            case 3: fillSceneMixed(scene, 42); break;
        }
        std::vector<float> refDepth = generateSyntheticDepth(
            scene, cfg.width, cfg.height, cfg.viewDistanceM);
        std::vector<float> hypDepth = quantizeDepthBuffer(
            refDepth, cfg.depthFormat);
        auto boxes = generateTestBoxes(64, 12345);
        double psnrSum = 0.0;
        int fcSum = 0;
        int totalBoxes = 0;
        double errSum = 0.0;
        VramSize vram{};
        for (int it = 0; it < warmupIters + measureIters; ++it) {
            bool reverseZ = (cfg.depthFormat == DepthFormat::D16_UNORM_REVERSE_Z);
            double psnr = computePSNR(refDepth, hypDepth);
            CullResult cr = simulateHZBCull(
                refDepth, hypDepth, cfg.width, cfg.height,
                cfg.depthFormat, boxes, reverseZ);
            double err = 0.0;
            for (size_t i = 0; i < refDepth.size(); ++i) {
                err += std::abs(refDepth[i] - hypDepth[i]);
            }
            err /= refDepth.size();
            vram = computeVram(cfg.width, cfg.height, cfg.depthFormat);
            if (it >= warmupIters) {
                psnrSum += psnr;
                fcSum += cr.falseCulled;
                totalBoxes += cr.total;
                errSum += err;
            }
        }
        BenchResult res{};
        res.config = cfg;
        res.vramMiB = vram.totalMiB;
        res.psnrDB = psnrSum / measureIters;
        res.falseCulled = fcSum;
        res.totalBoxes = totalBoxes;
        res.meanCullError = errSum / measureIters;
        results.push_back(res);
    }
    return results;
}

}  // namespace depth_quant
