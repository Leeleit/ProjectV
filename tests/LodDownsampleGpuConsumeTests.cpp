#include "render/LodDownsampleGpuConsume.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "core/Types.hpp"
#include "voxel/VoxelWorld.hpp"

namespace {

struct TestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

void TestLodGpuConsumeEnvDefaultOff(TestContext &context)
{
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
	if (projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME unset -> false");
	}
}

void TestLodGpuConsumeEnvOn(TestContext &context)
{
	setenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME", "ON", 1);
	if (!projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=ON -> true");
	}
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
}

void TestLodGpuConsumeEnvZeroIsOff(TestContext &context)
{
	setenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME", "0", 1);
	if (projectv::render::IsLodDownsampledGpuConsumeEnabled()) {
		context.Fail(__LINE__, "PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME=0 -> false");
	}
	unsetenv("PROJECTV_LOD_DOWNSAMPLE_GPU_CONSUME");
}

void TestComputeLodDownsampledPayloadBytesScalesWithChunks(TestContext &context)
{
	const uint32_t small = projectv::render::ComputeLodDownsampledVoxelPayloadBytes(8u, 8u);
	const uint32_t large = projectv::render::ComputeLodDownsampledVoxelPayloadBytes(64u, 8u);
	if (large <= small) {
		context.Fail(__LINE__, "LOD payload bytes must grow with chunk count");
	}
}

void TestComputeChunkLodLevelsCapacityAtLeastOne(TestContext &context)
{
	const uint32_t zero = projectv::render::ComputeChunkLodLevelsCapacity(0u);
	if (zero != 1u) {
		context.Fail(__LINE__, "ComputeChunkLodLevelsCapacity(0) must be at least 1 (capacity floor)");
	}
	const uint32_t many = projectv::render::ComputeChunkLodLevelsCapacity(1234u);
	if (many != 1234u) {
		context.Fail(__LINE__, "ComputeChunkLodLevelsCapacity(1234) must equal 1234");
	}
}

void TestRefreshLodDownsampledBuffersRejectsNullContext(TestContext &context)
{
	RenderState render{};
	const VoxelWorld world{};
	if (projectv::render::RefreshLodDownsampledBuffers(nullptr, &render, world)) {
		context.Fail(__LINE__, "RefreshLodDownsampledBuffers(nullptr context) must return false");
	}
}

void TestLodPayloadWordStrideMatchesConstant(TestContext &)
{
	static_assert(projectv::render::kLodPayloadWordStride == 16u,
				  "kLodPayloadWordStride must be 16 for chunkSize=8, LOD 1");
}

void TestLodPayloadWordOffsetForChunkScalesLinearly(TestContext &context)
{
	if (projectv::render::LodPayloadWordOffsetForChunk(0u) != 0u) {
		context.Fail(__LINE__, "LodPayloadWordOffsetForChunk(0) must be 0");
	}
	if (projectv::render::LodPayloadWordOffsetForChunk(1u) != 16u) {
		context.Fail(__LINE__, "LodPayloadWordOffsetForChunk(1) must equal stride (16)");
	}
	if (projectv::render::LodPayloadWordOffsetForChunk(2u) != 32u) {
		context.Fail(__LINE__, "LodPayloadWordOffsetForChunk(2) must equal 2*stride (32)");
	}
}

void TestEncodeDecodeChunkLodMetadataRoundtrip(TestContext &context)
{
	for (uint8_t lod = 0u; lod < 4u; ++lod) {
		for (uint8_t extent = 0u; extent < 8u; ++extent) {
			const uint32_t encoded = projectv::render::EncodeChunkLodMetadata(lod, extent);
			uint8_t outLod = 0xFFu;
			uint8_t outExtent = 0xFFu;
			projectv::render::DecodeChunkLodMetadata(encoded, outLod, outExtent);
			if (outLod != lod) {
				std::fprintf(stderr, "lod=%u extent=%u encoded=0x%x outLod=%u\n",
							 static_cast<unsigned>(lod), static_cast<unsigned>(extent), encoded,
							 static_cast<unsigned>(outLod));
				context.Fail(__LINE__, "DecodeChunkLodMetadata lod roundtrip");
				return;
			}
			if (outExtent != extent) {
				std::fprintf(stderr, "lod=%u extent=%u encoded=0x%x outExtent=%u\n",
							 static_cast<unsigned>(lod), static_cast<unsigned>(extent), encoded,
							 static_cast<unsigned>(outExtent));
				context.Fail(__LINE__, "DecodeChunkLodMetadata extent roundtrip");
				return;
			}
		}
	}
}

void TestBuildLodPayloadWordsPacks4BytesPerWord(TestContext &context)
{
	constexpr std::array<uint8_t, 8> bytes = {0x11u, 0x22u, 0x33u, 0x44u, 0xAAu, 0xBBu, 0xCCu, 0xDDu};
	std::array<uint32_t, 2> words{};
	projectv::render::BuildLodPayloadWordsFromDownsampled(
		bytes.data(),
		bytes.size(),
		words.data());
	if (words[0] != 0x44332211u) {
		std::fprintf(stderr, "words[0]=0x%08x expected 0x44332211\n", words[0]);
		context.Fail(__LINE__, "BuildLodPayloadWords little-endian byte 0-3");
	}
	if (words[1] != 0xDDCCBBAAu) {
		std::fprintf(stderr, "words[1]=0x%08x expected 0xDDCCBBAA\n", words[1]);
		context.Fail(__LINE__, "BuildLodPayloadWords little-endian byte 4-7");
	}
}

void TestBuildLodPayloadWordsZeroPadsShortInput(TestContext &context)
{
	constexpr std::array<uint8_t, 5> bytes = {0x01u, 0x02u, 0x03u, 0x04u, 0x05u};
	std::array<uint32_t, 2> words{};
	projectv::render::BuildLodPayloadWordsFromDownsampled(
		bytes.data(),
		bytes.size(),
		words.data());
	if (words[0] != 0x04030201u) {
		std::fprintf(stderr, "words[0]=0x%08x expected 0x04030201\n", words[0]);
		context.Fail(__LINE__, "BuildLodPayloadWords short input word 0");
	}
	if (words[1] != 0x00000005u) {
		std::fprintf(stderr, "words[1]=0x%08x expected 0x00000005\n", words[1]);
		context.Fail(__LINE__, "BuildLodPayloadWords short input word 1 must zero-pad");
	}
}

void TestBuildLodPayloadWordsHandlesNullSafely(TestContext &context)
{
	std::array words{0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu, 0xDEADBEEFu};
	projectv::render::BuildLodPayloadWordsFromDownsampled(nullptr, 16u, words.data());
	if (words[0] != 0xDEADBEEFu) {
		context.Fail(__LINE__, "BuildLodPayloadWords must guard against null bytes");
	}
	projectv::render::BuildLodPayloadWordsFromDownsampled(reinterpret_cast<const uint8_t *>("\x01\x02"), 2u, nullptr);
}

} // namespace

int main()
{
	TestContext context{};
	TestLodGpuConsumeEnvDefaultOff(context);
	TestLodGpuConsumeEnvOn(context);
	TestLodGpuConsumeEnvZeroIsOff(context);
	TestComputeLodDownsampledPayloadBytesScalesWithChunks(context);
	TestComputeChunkLodLevelsCapacityAtLeastOne(context);
	TestRefreshLodDownsampledBuffersRejectsNullContext(context);
	TestLodPayloadWordStrideMatchesConstant(context);
	TestLodPayloadWordOffsetForChunkScalesLinearly(context);
	TestEncodeDecodeChunkLodMetadataRoundtrip(context);
	TestBuildLodPayloadWordsPacks4BytesPerWord(context);
	TestBuildLodPayloadWordsZeroPadsShortInput(context);
	TestBuildLodPayloadWordsHandlesNullSafely(context);

	if (context.failures > 0) {
		std::fprintf(stderr, "ProjectVLodDownsampleGpuConsumeTests: %d failure(s)\n", context.failures);
		return 1;
	}
	std::puts("ProjectVLodDownsampleGpuConsumeTests passed");
	return 0;
}
