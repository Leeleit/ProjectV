#include "c_kernels/FrustumCulling.hpp"
#include "core/Types.hpp"
#include "render/SceneResources.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <vector>

namespace {

constexpr size_t kBatchSize = 300;
constexpr uint32_t kVisibilityRuns = 5;
constexpr uint32_t kTestSeed = 0xC0FFEE01u;

struct Xorshift32 {
    uint32_t state = kTestSeed;
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
    [[nodiscard]] float Range(const float min, const float max) noexcept {
        return min + (max - min) * UnitFloat();
    }
};

struct TestFixture {
    std::vector<ModelInstanceData> instances;
    std::array<ChunkCullingParameters, kVisibilityRuns> parameters{};

    static TestFixture Make() {
        TestFixture f;
        Xorshift32 rng{kTestSeed};

        f.instances.reserve(kBatchSize);
        for (size_t i = 0; i < kBatchSize; ++i) {
            const int gx = static_cast<int>((i % 32u)) - 16;
            const int gz = static_cast<int>((i / 32u)) - 5;
            const float minX = static_cast<float>(gx) * 8.0f;
            const float minZ = static_cast<float>(gz) * 8.0f;
            const float minY = rng.Range(-4.0f, 4.0f);
            const float maxX = minX + 8.0f;
            const float maxY = minY + rng.Range(4.0f, 12.0f);
            const float maxZ = minZ + 8.0f;
            ModelInstanceData instance{};
            instance.worldAabbMin = projectv::math::Vec3{minX, minY, minZ, 0.0f};
            instance.worldAabbMax = projectv::math::Vec3{maxX, maxY, maxZ, 0.0f};

            instance.vertexBuffer = reinterpret_cast<VkBuffer>(0x1u);
            instance.indexBuffer = reinterpret_cast<VkBuffer>(0x2u);
            instance.indexCount = 12u;
            f.instances.push_back(instance);
        }

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
                cosY * cosP, sinP, -sinY * cosP, 0.0f};
            const projectv::math::Vec3 right{
                cosY, 0.0f, -sinY, 0.0f};
            const projectv::math::Vec3 up{
                sinY * sinP, cosP, cosY * sinP, 0.0f};
            f.parameters[r].cameraPositionAndMaxDistance =
                projectv::math::Vec4{64.0f, 64.0f, 64.0f, 200.0f};
            f.parameters[r].cameraForwardAndTanHalfVerticalFov =
                projectv::math::Vec4{forward.x, forward.y, forward.z, 0.829f};
            f.parameters[r].cameraRightAndTanHalfHorizontalFov =
                projectv::math::Vec4{right.x, right.y, right.z, 1.192f};
            f.parameters[r].cameraUpAndNearPlane =
                projectv::math::Vec4{up.x, up.y, up.z, 0.5f};
        }
        return f;
    }
};

[[nodiscard]] std::vector<ModelInstanceData> ReferenceFilter(
    const std::span<const ModelInstanceData> &instances,
    const ChunkCullingParameters &parameters) {
    std::vector<ModelInstanceData> visible;
    visible.reserve(instances.size());
    for (const auto &instance : instances) {
        if (IsAabbVisibleAgainstCameraFrustum(
                instance.worldAabbMin,
                instance.worldAabbMax,
                parameters)) {
            visible.push_back(instance);
        }
    }
    return visible;
}

void ExpectVectorEqual(
    const std::vector<ModelInstanceData> &a,
    const std::vector<ModelInstanceData> &b,
    const char *label) {
    if (a.size() != b.size()) {
        std::fprintf(
            stderr,
            "FAIL: %s: size mismatch %zu vs %zu\n",
            label, a.size(), b.size());
        std::abort();
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const auto &x = a[i];
        const auto &y = b[i];
        if (x.worldAabbMin.x != y.worldAabbMin.x ||
            x.worldAabbMin.y != y.worldAabbMin.y ||
            x.worldAabbMin.z != y.worldAabbMin.z ||
            x.worldAabbMax.x != y.worldAabbMax.x ||
            x.worldAabbMax.y != y.worldAabbMax.y ||
            x.worldAabbMax.z != y.worldAabbMax.z ||
            x.indexCount != y.indexCount) {
            std::fprintf(
                stderr,
                "FAIL: %s: index %zu differs\n", label, i);
            std::abort();
        }
    }
}

} // namespace

int main() {
    const TestFixture fixture = TestFixture::Make();

    if (fixture.instances.size() != kBatchSize) {
        std::fprintf(
            stderr,
            "FAIL: fixture has %zu instances, expected %zu\n",
            fixture.instances.size(), kBatchSize);
        return EXIT_FAILURE;
    }


    for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
        const std::span<const ModelInstanceData> span(
            fixture.instances.data(), fixture.instances.size());
        const std::vector<ModelInstanceData> reference =
            ReferenceFilter(span, fixture.parameters[r]);
        const std::vector<ModelInstanceData> wrapper =
            projectv::c_kernels::FilterVisibleInstances(span, fixture.parameters[r]);
        const std::string label =
            "run " + std::to_string(r) + " (count=" + std::to_string(fixture.instances.size()) + ")";
        ExpectVectorEqual(reference, wrapper, label.c_str());
    }
    std::printf("[OK] wrapper matches reference for 300-AABB batched path (5 runs)\n");


    {
        const std::span<const ModelInstanceData> span(
            fixture.instances.data(), 4);
        const std::vector<ModelInstanceData> reference =
            ReferenceFilter(span, fixture.parameters[0]);
        const std::vector<ModelInstanceData> wrapper =
            projectv::c_kernels::FilterVisibleInstances(span, fixture.parameters[0]);
        ExpectVectorEqual(reference, wrapper, "4-AABB fallback");
    }
    std::printf("[OK] wrapper matches reference for 4-AABB fallback path\n");


    {
        const std::span<const ModelInstanceData> span(
            fixture.instances.data(), 1);
        const std::vector<ModelInstanceData> reference =
            ReferenceFilter(span, fixture.parameters[0]);
        const std::vector<ModelInstanceData> wrapper =
            projectv::c_kernels::FilterVisibleInstances(span, fixture.parameters[0]);
        ExpectVectorEqual(reference, wrapper, "1-AABB fallback");
    }
    std::printf("[OK] wrapper matches reference for 1-AABB fallback path\n");


    for (uint32_t r = 0; r < kVisibilityRuns; ++r) {
        const std::span<const ModelInstanceData> span(
            fixture.instances.data(), fixture.instances.size());
        const std::vector<uint8_t> mask =
            projectv::c_kernels::CullVisibleMask(span, fixture.parameters[r]);
        if (mask.size() != (kBatchSize + 7) / 8) {
            std::fprintf(
                stderr,
                "FAIL: CullVisibleMask size %zu, expected %zu\n",
                mask.size(), (kBatchSize + 7) / 8);
            return EXIT_FAILURE;
        }
        const std::vector<ModelInstanceData> reference =
            ReferenceFilter(span, fixture.parameters[r]);
        std::vector<ModelInstanceData> fromMask;
        fromMask.reserve(reference.size());
        for (size_t i = 0; i < kBatchSize; ++i) {
            const uint8_t bit = static_cast<uint8_t>(1u << (i % 8));
            if ((mask[i / 8] & bit) != 0) {
                fromMask.push_back(span[i]);
            }
        }
        ExpectVectorEqual(reference, fromMask, "CullVisibleMask->rebuild");
    }
    std::printf("[OK] CullVisibleMask matches reference for 300-AABB batched path (5 runs)\n");


    {
        const std::span<const ModelInstanceData> empty;
        const std::vector<ModelInstanceData> result =
            projectv::c_kernels::FilterVisibleInstances(empty, fixture.parameters[0]);
        if (!result.empty()) {
            std::fprintf(
                stderr,
                "FAIL: empty input returned %zu results\n",
                result.size());
            return EXIT_FAILURE;
        }
        const std::vector<uint8_t> mask =
            projectv::c_kernels::CullVisibleMask(empty, fixture.parameters[0]);
        if (!mask.empty()) {
            std::fprintf(
                stderr,
                "FAIL: empty input mask size %zu\n",
                mask.size());
            return EXIT_FAILURE;
        }
    }
    std::printf("[OK] empty input returns empty output (no UB)\n");

    std::printf("ProjectVCFrustumCullingTests: 5/5 passed\n");
    return EXIT_SUCCESS;
}
