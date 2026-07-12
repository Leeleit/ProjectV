import projectv.math;

#include "core/Types.hpp"
#include "render/SceneResources.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

void Expect(const bool condition, const char *label)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", label);
		std::abort();
	}
}

ChunkCullingParameters MakeParameters(
	const float px, const float py, const float pz,
	const float fx, const float fy, const float fz)
{
	ChunkCullingParameters parameters{};
	parameters.cameraPositionAndMaxDistance =
		projectv::math::Vec4{px, py, pz, 200.0f};
	parameters.cameraForwardAndTanHalfVerticalFov =
		projectv::math::Vec4{fx, fy, fz, 0.829f};
	parameters.cameraRightAndTanHalfHorizontalFov =
		projectv::math::Vec4{1.0f, 0.0f, 0.0f, 1.192f};
	parameters.cameraUpAndNearPlane =
		projectv::math::Vec4{0.0f, 1.0f, 0.0f, 0.5f};
	return parameters;
}

} // namespace

int main()
{
	using projectv::visibility_cache::ComputeVisibilityCacheHash;

	{
		const ChunkCullingParameters parameters =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t hash1 = ComputeVisibilityCacheHash(parameters, 0u, 0u);
		const uint64_t hash2 = ComputeVisibilityCacheHash(parameters, 0u, 0u);
		Expect(hash1 == hash2, "determinism: same input -> same hash");
		Expect(hash1 != 0u, "determinism: hash != 0 for normal input");
	}
	std::printf("[OK] determinism: same input -> same hash\n");

	{
		const ChunkCullingParameters parameters =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t baseHash = ComputeVisibilityCacheHash(parameters, 7u, 100u);

		const ChunkCullingParameters shifted1m =
			MakeParameters(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		Expect(
			ComputeVisibilityCacheHash(shifted1m, 7u, 100u) != baseHash,
			"position: 1m shift (above quantization) changes hash");

		const ChunkCullingParameters shifted10cm =
			MakeParameters(0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t hash10cm =
			ComputeVisibilityCacheHash(shifted10cm, 7u, 100u);
		Expect(
			hash10cm == baseHash,
			"position: 10cm shift (below 0.25m quantization) preserves hash");
	}
	std::printf("[OK] position: quantization groups sub-step shifts\n");

	{
		const ChunkCullingParameters parameters =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t baseHash = ComputeVisibilityCacheHash(parameters, 7u, 100u);

		const ChunkCullingParameters turned =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.01f, 0.0f, 1.0f);
		Expect(
			ComputeVisibilityCacheHash(turned, 7u, 100u) != baseHash,
			"forward: 0.01 turn changes hash");
	}
	std::printf("[OK] forward: distinct forward vectors -> distinct hashes\n");

	{
		const ChunkCullingParameters parameters =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t baseHash = ComputeVisibilityCacheHash(parameters, 7u, 100u);
		Expect(
			ComputeVisibilityCacheHash(parameters, 8u, 100u) != baseHash,
			"version: voxel payload bump changes hash");
		Expect(
			ComputeVisibilityCacheHash(parameters, 7u, 101u) != baseHash,
			"chunks: chunk count change changes hash");
	}
	std::printf("[OK] version+chunks: changes alter hash\n");

	{
		const ChunkCullingParameters origin =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const ChunkCullingParameters bitIdentical =
			MakeParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		const uint64_t h1 = ComputeVisibilityCacheHash(origin, 42u, 256u);
		const uint64_t h2 = ComputeVisibilityCacheHash(bitIdentical, 42u, 256u);
		Expect(h1 == h2, "identical: bytewise identical input -> bytewise identical hash");
	}
	std::printf("[OK] identical: byte-identical input -> identical hash\n");

	std::printf("ProjectVVisibilityCacheHashTests: 5/5 passed\n");
	return EXIT_SUCCESS;
}