// **Tier 3 (`2026-06-13`).** `FrustumCullBenchmark` —
// measures the 6-plane AABB frustum-cull hot path with
// three implementations side-by-side:
//
// 1. **C++ scalar** — `projectv::math::` types via
//    `IsAabbVisibleAgainstCameraFrustum` (the
//    production M5 helper, the existing code path the
//    engine calls today).
// 2. **C scalar** — `projectv_cull_frustum_scalar`
//    from `c_kernels/frustum_cull.c` (the same
//    algorithm, no intrinsics; serves as the
//    reference/verification path).
// 3. **C AVX2 8-way** — `projectv_cull_frustum_avx2`
//    from `c_kernels/frustum_cull.c` (the intrinsics
//    kernel this PR is delivering).
//
// **Test fixture.** 300 AABBs in a `32 × 32` chunk
// grid (per `decisions.md §773` "300 chunks × 5
// visibility tests"). AABBs are deterministically
// generated from an LCG (`xorshift32`) so the benchmark
// is reproducible across runs and across machines; the
// camera is fixed (looking at the centre of the grid
// from `(64, 64, 64)`); 5 visibility runs (camera yaw /
// pitch varied) cover different chunks being culled.
//
// **Verification.** Before measuring throughput, the
// benchmark runs a 256-AABB cross-check: the C++
// scalar, the C scalar, and the C AVX2 implementations
// must produce bit-identical visible masks. If they
// don't, the benchmark `CHECK`s a fatal error and
// stops. This is the contract from
// `decisions.md §773`: AVX2 only ships if the
// reference and the vectorised kernels agree on
// every AABB.
//
// **Reported metrics.** `ns/AABB` (per-chunk cost
// is what the engine actually cares about). The
// engine's `TracyPlot("FrustumCulling (ms)")` should
// track this number once the C kernel is integrated
// (Tier 4 hookup, future PR).
#include "benchmark/benchmark.h"

#include "c_kernels/frustum_cull.hpp"
#include "core/Math.hpp"
#include "core/Types.hpp"
#include "render/SceneResources.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

namespace {

constexpr size_t kBatchSize = 300;
constexpr uint32_t kVisibilityRuns = 5;

// **xorshift32 LCG.** Same as the test harness'
// `MakeTestWorld` PRNG; deterministic across runs and
// platforms (no `<random>` engine overhead, no
// threading concerns).
struct Xorshift32 {
    uint32_t state = 0x9E3779B9u;
    [[nodiscard]] uint32_t Next() noexcept {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        return state = x;
    }
    [[nodiscard]] float UnitFloat() noexcept {
        return static_cast<float>(Next() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }
    [[nodiscard]] float Range(float min, float max) noexcept {
        return min + (max - min) * UnitFloat();
    }
};

struct VisibilityFixture {
    std::array<projectv::math::Vec3, kBatchSize> aabbMin{};
    std::array<projectv::math::Vec3, kBatchSize> aabbMax{};
    std::array<ChunkCullingParameters, kVisibilityRuns> parameters{};

    static VisibilityFixture Make(uint32_t seed) {
        VisibilityFixture f;
        Xorshift32 rng{seed};

        // **300 AABBs in a 32×32 chunk grid centred on the
        // origin.** Chunks are 8 units on a side; min corner
        // spans (-128, -128, -128)..(128, 128, 128), with
        // each chunk's Y range randomly within (-4, 12).
        // Roughly 60% of the AABBs are world-space visible
        // from a camera at (64, 64, 64) looking at the
        // origin; the rest are far / behind.
        for (size_t i = 0; i < kBatchSize; ++i) {
            const int gx = static_cast<int>((i % 32u)) - 16;
            const int gz = static_cast<int>((i / 32u)) - 5;
            const float minX = static_cast<float>(gx) * 8.0f;
            const float minZ = static_cast<float>(gz) * 8.0f;
            const float minY = rng.Range(-4.0f, 4.0f);
            const float maxX = minX + 8.0f;
            const float maxY = minY + rng.Range(4.0f, 12.0f);
            const float maxZ = minZ + 8.0f;
            f.aabbMin[i] = projectv::math::Vec3{minX, minY, minZ, 0.0f};
            f.aabbMax[i] = projectv::math::Vec3{maxX, maxY, maxZ, 0.0f};
        }

        // **5 visibility runs** with the camera at fixed
        // position (64, 64, 64), looking at the origin, with
        // varying yaw / pitch. FOV: 75° vertical, 100°
        // horizontal (matches the engine default). Far plane:
        // 200 units.
        const std::array<float, kVisibilityRuns> yaws{
            0.0f, 0.45f, -0.45f, 1.10f, -1.10f};
        const std::array<float, kVisibilityRuns> pitches{
            -0.20f, -0.45f, -0.05f, -0.65f, -0.15f};
        for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
            const float yaw = yaws[r];
            const float pitch = pitches[r];
            const float cosY = std::cos(yaw);
            const float sinY = std::sin(yaw);
            const float cosP = std::cos(pitch);
            const float sinP = std::sin(pitch);
            const projectv::math::Vec3 forward{
                cosY * cosP,
                sinP,
                -sinY * cosP,
                0.0f,
            };
            const projectv::math::Vec3 right{
                cosY,
                0.0f,
                -sinY,
                0.0f,
            };
            const projectv::math::Vec3 up{
                sinY * sinP,
                cosP,
                cosY * sinP,
                0.0f,
            };
            f.parameters[r].cameraPositionAndMaxDistance =
                projectv::math::Vec4{64.0f, 64.0f, 64.0f, 200.0f};
            f.parameters[r].cameraForwardAndTanHalfVerticalFov =
                projectv::math::Vec4{forward.x, forward.y, forward.z, 0.829f /*tan(39.7°)*/};
            f.parameters[r].cameraRightAndTanHalfHorizontalFov =
                projectv::math::Vec4{right.x, right.y, right.z, 1.192f /*tan(50°)*/};
            f.parameters[r].cameraUpAndNearPlane =
                projectv::math::Vec4{up.x, up.y, up.z, 0.5f};
        }
        return f;
    }
};

[[nodiscard]] ProjectvCFrustumCullParameters ToCParams(
    const ChunkCullingParameters &p) noexcept {
    ProjectvCFrustumCullParameters out{};
    out.cameraPosition[0] = p.cameraPositionAndMaxDistance.x;
    out.cameraPosition[1] = p.cameraPositionAndMaxDistance.y;
    out.cameraPosition[2] = p.cameraPositionAndMaxDistance.z;
    out.maxDistance = p.cameraPositionAndMaxDistance.w;
    out.cameraForward[0] = p.cameraForwardAndTanHalfVerticalFov.x;
    out.cameraForward[1] = p.cameraForwardAndTanHalfVerticalFov.y;
    out.cameraForward[2] = p.cameraForwardAndTanHalfVerticalFov.z;
    out.tanHalfVerticalFov = p.cameraForwardAndTanHalfVerticalFov.w;
    out.cameraRight[0] = p.cameraRightAndTanHalfHorizontalFov.x;
    out.cameraRight[1] = p.cameraRightAndTanHalfHorizontalFov.y;
    out.cameraRight[2] = p.cameraRightAndTanHalfHorizontalFov.w >= 0.0f
        ? p.cameraRightAndTanHalfHorizontalFov.z
        : 0.0f;
    out.tanHalfHorizontalFov = p.cameraRightAndTanHalfHorizontalFov.w;
    out.cameraUp[0] = p.cameraUpAndNearPlane.x;
    out.cameraUp[1] = p.cameraUpAndNearPlane.y;
    out.cameraUp[2] = p.cameraUpAndNearPlane.z;
    out.nearPlane = p.cameraUpAndNearPlane.w;
    return out;
}

[[nodiscard]] std::vector<ProjectvCAabb> ToCAabbs(
    const VisibilityFixture &f) {
    std::vector<ProjectvCAabb> aabbs(kBatchSize);
    for (size_t i = 0; i < kBatchSize; ++i) {
        aabbs[i].min[0] = f.aabbMin[i].x;
        aabbs[i].min[1] = f.aabbMin[i].y;
        aabbs[i].min[2] = f.aabbMin[i].z;
        aabbs[i].max[0] = f.aabbMax[i].x;
        aabbs[i].max[1] = f.aabbMax[i].y;
        aabbs[i].max[2] = f.aabbMax[i].z;
        aabbs[i]._pad0 = 0.0f;
        aabbs[i]._pad1 = 0.0f;
    }
    return aabbs;
}

void RunCppScalar(
    const VisibilityFixture &fixture,
    std::vector<uint8_t> *masks) {
    for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
        std::fill(masks->begin(), masks->end(), 0);
        const auto &params = fixture.parameters[r];
        for (size_t i = 0; i < kBatchSize; ++i) {
            if (IsAabbVisibleAgainstCameraFrustum(
                    fixture.aabbMin[i], fixture.aabbMax[i], params)) {
                (*masks)[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
            }
        }
    }
}

void RunCScalar(
    const std::vector<ProjectvCAabb> &aabbs,
    const VisibilityFixture &fixture,
    std::vector<uint8_t> *masks) {
    for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
        std::fill(masks->begin(), masks->end(), 0);
        const auto cparams = ToCParams(fixture.parameters[r]);
        projectv_cull_frustum_scalar(masks->data(), aabbs.data(), &cparams, kBatchSize);
    }
}

#if defined(__AVX2__)
void RunCAvx2(
    const std::vector<ProjectvCAabb> &aabbs,
    const VisibilityFixture &fixture,
    std::vector<uint8_t> *masks) {
    for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
        std::fill(masks->begin(), masks->end(), 0);
        const auto cparams = ToCParams(fixture.parameters[r]);
        projectv_cull_frustum_avx2(masks->data(), aabbs.data(), &cparams, kBatchSize);
    }
}
#endif

void VerifyBitIdentical(
    const std::vector<uint8_t> &a,
    const std::vector<uint8_t> &b,
    const char *aLabel,
    const char *bLabel) {
    if (a.size() != b.size()) {
        std::fprintf(
            stderr,
            "MISMATCH: %s size %zu vs %s size %zu\n",
            aLabel, a.size(), bLabel, b.size());
        std::abort();
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            std::fprintf(
                stderr,
                "MISMATCH: %s[%zu]=0x%02x vs %s[%zu]=0x%02x\n",
                aLabel, i, a[i], bLabel, i, b[i]);
            std::abort();
        }
    }
}

} // namespace

static void BM_CppScalar(benchmark::State &state) {
    const VisibilityFixture fixture = VisibilityFixture::Make(0xC0FFEE01u);
    std::vector<uint8_t> masks((kBatchSize + 7) / 8, 0);
    for (auto _ : state) {
        RunCppScalar(fixture, &masks);
        benchmark::DoNotOptimize(masks.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kBatchSize * kVisibilityRuns);
    state.SetLabel("C++ scalar (projectv::math, IsAabbVisible...)");
}

static void BM_CScalar(benchmark::State &state) {
    const VisibilityFixture fixture = VisibilityFixture::Make(0xC0FFEE01u);
    const auto aabbs = ToCAabbs(fixture);
    std::vector<uint8_t> masks((kBatchSize + 7) / 8, 0);
    for (auto _ : state) {
        RunCScalar(aabbs, fixture, &masks);
        benchmark::DoNotOptimize(masks.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kBatchSize * kVisibilityRuns);
    state.SetLabel("C scalar (projectv_cull_frustum_scalar)");
}

#if defined(__AVX2__)
static void BM_CAvx2(benchmark::State &state) {
    const VisibilityFixture fixture = VisibilityFixture::Make(0xC0FFEE01u);
    const auto aabbs = ToCAabbs(fixture);
    std::vector<uint8_t> masks((kBatchSize + 7) / 8, 0);
    for (auto _ : state) {
        RunCAvx2(aabbs, fixture, &masks);
        benchmark::DoNotOptimize(masks.data());
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * kBatchSize * kVisibilityRuns);
    state.SetLabel("C AVX2 8-way (projectv_cull_frustum_avx2)");
}
#endif

int main(int argc, char *argv[]) {
    // **Cross-check:** all three implementations must
    // produce bit-identical visible masks for the same
    // fixture. If any pair disagrees, fail loudly.
    {
        const VisibilityFixture fixture = VisibilityFixture::Make(0xC0FFEE01u);
        const auto aabbs = ToCAabbs(fixture);
        std::vector<uint8_t> cppMasks((kBatchSize + 7) / 8, 0);
        std::vector<uint8_t> cMasks((kBatchSize + 7) / 8, 0);
        std::vector<uint8_t> avxMasks((kBatchSize + 7) / 8, 0);
        for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
            std::fill(cppMasks.begin(), cppMasks.end(), 0);
            const auto &params = fixture.parameters[r];
            for (size_t i = 0; i < kBatchSize; ++i) {
            if (IsAabbVisibleAgainstCameraFrustum(
                    fixture.aabbMin[i], fixture.aabbMax[i], params)) {
                cppMasks[i / 8] |= static_cast<uint8_t>(1u << (i % 8));
            }
            }
            std::fill(cMasks.begin(), cMasks.end(), 0);
            const auto cparams = ToCParams(params);
            projectv_cull_frustum_scalar(cMasks.data(), aabbs.data(), &cparams, kBatchSize);
            VerifyBitIdentical(cppMasks, cMasks, "cpp_scalar", "c_scalar");
#if defined(__AVX2__)
            std::fill(avxMasks.begin(), avxMasks.end(), 0);
            projectv_cull_frustum_avx2(avxMasks.data(), aabbs.data(), &cparams, kBatchSize);
            VerifyBitIdentical(cMasks, avxMasks, "c_scalar", "c_avx2");
#endif
        }
        std::printf("[Tier3 frustum_cull] bit-identical across all implementations (cpp, c_scalar");
#if defined(__AVX2__)
        std::printf(", c_avx2");
#endif
        std::printf(")\n");
    }

    benchmark::RegisterBenchmark("BM_CppScalar", &BM_CppScalar);
    benchmark::RegisterBenchmark("BM_CScalar", &BM_CScalar);
#if defined(__AVX2__)
    benchmark::RegisterBenchmark("BM_CAvx2", &BM_CAvx2);
#endif
    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        return 1;
    }
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
