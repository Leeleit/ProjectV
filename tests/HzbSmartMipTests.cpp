#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct SmartMipTestContext {
	int failures = 0;
	void Fail(const int line, const std::string_view message)
	{
		std::fprintf(stderr, "Test failure at line %d: %.*s\n", line, static_cast<int>(message.size()), message.data());
		++failures;
	}
};

bool IsHzbSmartMipEnabled()
{
	const char *value = std::getenv("PROJECTV_HZB_SMART_MIP");
	if (value == nullptr) {
		return false;
	}
	return value[0] != '\0' && value[0] != '0';
}

uint32_t ComputePerChunkMipLevelCpu(
	const float projectedExtentXTexels,
	const float projectedExtentYTexels,
	const uint32_t maxMipLevel)
{
	const float maxExtent = std::max(projectedExtentXTexels, projectedExtentYTexels);
	if (maxExtent <= 1.0f) {
		return 0u;
	}
	const float logVal = std::log2(maxExtent);
	const int32_t floored = static_cast<int32_t>(logVal);
	if (floored < 0) {
		return 0u;
	}
	const uint32_t capped = static_cast<uint32_t>(floored);
	return std::min(capped, maxMipLevel);
}

uint32_t ComputePerChunkMipLevelsFromAabbs(
	const std::vector<std::array<float, 4>> &chunkCenters,
	const std::vector<std::array<float, 4>> &chunkHalfExtents,
	const std::array<float, 16> &viewProjection,
	const uint32_t baseWidth,
	const uint32_t baseHeight,
	const uint32_t maxMipLevel,
	std::vector<uint32_t> &outMipLevels)
{
	const size_t count = std::min(chunkCenters.size(), chunkHalfExtents.size());
	outMipLevels.assign(count, 0u);
	if (count == 0u) {
		return 0u;
	}
	for (size_t i = 0; i < count; ++i) {
		const float centerX = chunkCenters[i][0];
		const float centerY = chunkCenters[i][1];
		const float centerZ = chunkCenters[i][2];
		const float halfExtent = chunkHalfExtents[i][0];
		float minX = std::numeric_limits<float>::infinity();
		float minY = std::numeric_limits<float>::infinity();
		float maxX = -std::numeric_limits<float>::infinity();
		float maxY = -std::numeric_limits<float>::infinity();
		for (int sx = -1; sx <= 1; sx += 2) {
			for (int sy = -1; sy <= 1; sy += 2) {
				for (int sz = -1; sz <= 1; sz += 2) {
					const float cornerX = centerX + static_cast<float>(sx) * halfExtent;
					const float cornerY = centerY + static_cast<float>(sy) * halfExtent;
					const float cornerZ = centerZ + static_cast<float>(sz) * halfExtent;
					const float clipX = viewProjection[0] * cornerX + viewProjection[4] * cornerY + viewProjection[8] * cornerZ + viewProjection[12];
					const float clipY = viewProjection[1] * cornerX + viewProjection[5] * cornerY + viewProjection[9] * cornerZ + viewProjection[13];
					const float clipW = viewProjection[3] * cornerX + viewProjection[7] * cornerY + viewProjection[11] * cornerZ + viewProjection[15];
					if (clipW <= 0.0001f) {
						outMipLevels[i] = 0u;
						goto next_chunk;
					}
					const float ndcX = clipX / clipW;
					const float ndcY = clipY / clipW;
					const float uvX = ndcX * 0.5f + 0.5f;
					const float uvY = ndcY * 0.5f + 0.5f;
					if (uvX < minX) minX = uvX;
					if (uvY < minY) minY = uvY;
					if (uvX > maxX) maxX = uvX;
					if (uvY > maxY) maxY = uvY;
				}
			}
		}
		{
			const float projectedXTexels = (maxX - minX) * static_cast<float>(baseWidth);
			const float projectedYTexels = (maxY - minY) * static_cast<float>(baseHeight);
			outMipLevels[i] = ComputePerChunkMipLevelCpu(
				projectedXTexels,
				projectedYTexels,
				maxMipLevel);
		}
		next_chunk:;
	}
	return static_cast<uint32_t>(count);
}

void TestEnvGateDefault(SmartMipTestContext &test)
{
	const char *value = std::getenv("PROJECTV_HZB_SMART_MIP");
	const bool wasSet = value != nullptr;
	if (wasSet) {
		unsetenv("PROJECTV_HZB_SMART_MIP");
	}
	if (IsHzbSmartMipEnabled()) {
		test.Fail(__LINE__, "Default env gate must disable HZB smart mip (off by default per experiment)");
	}
	if (wasSet) {
		setenv("PROJECTV_HZB_SMART_MIP", value, 1);
	}
}

void TestEnvGateOff(SmartMipTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_HZB_SMART_MIP", "0", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (IsHzbSmartMipEnabled()) {
		test.Fail(__LINE__, "env=0 must disable HZB smart mip");
	}
	unsetenv("PROJECTV_HZB_SMART_MIP");
}

void TestEnvGateOn(SmartMipTestContext &test)
{
	const int setenvResult = setenv("PROJECTV_HZB_SMART_MIP", "1", 1);
	if (setenvResult != 0) {
		test.Fail(__LINE__, "setenv failed");
		return;
	}
	if (!IsHzbSmartMipEnabled()) {
		test.Fail(__LINE__, "env=1 must enable HZB smart mip");
	}
	unsetenv("PROJECTV_HZB_SMART_MIP");
}

void TestComputePerChunkMipLevelCpuCloseChunk(SmartMipTestContext &test)
{
	const uint32_t mip = ComputePerChunkMipLevelCpu(64.0f, 64.0f, 6u);
	if (mip != 6u) {
		test.Fail(__LINE__, "Mip 6 expected for 64x64 projected extent");
	}
	const uint32_t mip2 = ComputePerChunkMipLevelCpu(8.0f, 8.0f, 6u);
	if (mip2 != 3u) {
		test.Fail(__LINE__, "Mip 3 expected for 8x8 projected extent");
	}
	const uint32_t mip3 = ComputePerChunkMipLevelCpu(0.5f, 0.5f, 6u);
	if (mip3 != 0u) {
		test.Fail(__LINE__, "Mip 0 expected for sub-texel projected extent");
	}
}

void TestComputePerChunkMipLevelCpuFarChunk(SmartMipTestContext &test)
{
	const uint32_t mip = ComputePerChunkMipLevelCpu(2.0f, 1.0f, 6u);
	if (mip != 1u) {
		test.Fail(__LINE__, "Mip 1 expected for 2x1 projected extent");
	}
	const uint32_t mip2 = ComputePerChunkMipLevelCpu(4.0f, 3.0f, 6u);
	if (mip2 != 2u) {
		test.Fail(__LINE__, "Mip 2 expected for 4x3 projected extent");
	}
	const uint32_t mipCapped = ComputePerChunkMipLevelCpu(256.0f, 256.0f, 5u);
	if (mipCapped != 5u) {
		test.Fail(__LINE__, "Mip must be capped at maxMipLevel");
	}
}

void TestComputePerChunkMipLevelsFromAabbs(SmartMipTestContext &test)
{
	std::vector<std::array<float, 4>> centers;
	std::vector<std::array<float, 4>> halfExtents;
	centers.push_back({0.5f, 0.5f, -1.0f, 0.0f});
	halfExtents.push_back({0.1f, 0.1f, 0.1f, 0.1f});
	centers.push_back({0.5f, 0.5f, -1.0f, 0.0f});
	halfExtents.push_back({0.01f, 0.01f, 0.01f, 0.01f});
	constexpr std::array viewProjection = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	std::vector<uint32_t> outMipLevels;
	const uint32_t count = ComputePerChunkMipLevelsFromAabbs(
		centers,
		halfExtents,
		viewProjection,
		1920u,
		1080u,
		6u,
		outMipLevels);
	if (count != 2u) {
		test.Fail(__LINE__, "Expected 2 chunks processed");
	}
	if (outMipLevels.size() != 2u) {
		test.Fail(__LINE__, "Expected 2 mip levels computed");
	}
	if (outMipLevels[0] <= outMipLevels[1]) {
		std::fprintf(stderr, "  mipLevels: %u %u\n", outMipLevels[0], outMipLevels[1]);
		test.Fail(__LINE__, "Bigger chunk must have mip >= smaller chunk");
	}
}

uint32_t ComputeBlendWidthForChunkMipLocal(
	const uint32_t projectedExtentXTexels,
	const uint32_t projectedExtentYTexels,
	const uint32_t mipLevel,
	const uint32_t maxBlendWidth)
{
	if (maxBlendWidth == 0u) {
		return 0u;
	}
	if (mipLevel == 0u) {
		return 0u;
	}
	const uint32_t maxExtent = std::max(projectedExtentXTexels, projectedExtentYTexels);
	if (maxExtent == 0u) {
		return 0u;
	}
	const uint32_t texelsAtMip = std::max<uint32_t>(maxExtent >> std::min<uint32_t>(mipLevel, 16u), 1u);
	const uint32_t frac = maxExtent & (1u << std::min<uint32_t>(mipLevel, 16u)) - 1u;  // noinspection CppRedundantParentheses
	return std::min<uint32_t>(texelsAtMip / 4u + frac / 8u, maxBlendWidth);
}

void TestBlendWidthZeroMaxBlendWidthReturnsZero(SmartMipTestContext &test)
{
	if (ComputeBlendWidthForChunkMipLocal(64u, 64u, 3u, 0u) != 0u) {
		test.Fail(__LINE__, "maxBlendWidth=0 must return 0");
	}
}

void TestBlendWidthZeroMipReturnsZero(SmartMipTestContext &test)
{
	if (ComputeBlendWidthForChunkMipLocal(64u, 64u, 0u, 16u) != 0u) {
		test.Fail(__LINE__, "mipLevel=0 must return 0 (no blend needed at full res)");
	}
}

void TestBlendWidthIsBoundedByMax(SmartMipTestContext &test)
{
	const uint32_t result = ComputeBlendWidthForChunkMipLocal(64u, 64u, 3u, 4u);
	if (result > 4u) {
		test.Fail(__LINE__, "Blend width must never exceed maxBlendWidth");
	}
}

void WritePerChunkMipAndBlendWidthsToBuffer(
	void *mappedData,
	const uint32_t *mipAndBlendWidths,
	const uint32_t chunkCount)
{
	if (mappedData == nullptr || mipAndBlendWidths == nullptr) {
		return;
	}
	auto *dest = static_cast<uint32_t *>(mappedData);
	constexpr uint32_t stride = 2u;
	for (uint32_t i = 0u; i < chunkCount; ++i) {
		const uint32_t baseIndex = i * stride;
		dest[baseIndex] = mipAndBlendWidths[baseIndex];
		dest[baseIndex + 1u] = mipAndBlendWidths[baseIndex + 1u];
	}
}

void TestWritePerChunkMipAndBlendWidthsToBufferPackedLayout(SmartMipTestContext &test)
{
	constexpr uint32_t chunkCount = 4u;
	constexpr uint32_t source[chunkCount * 2u] = {
		0u, 0u,
		2u, 3u,
		5u, 8u,
		1u, 1u,
	};
	uint32_t buffer[chunkCount * 2u] = {};
	WritePerChunkMipAndBlendWidthsToBuffer(buffer, source, chunkCount);
	for (uint32_t i = 0u; i < chunkCount * 2u; ++i) {
		if (buffer[i] != source[i]) {
			std::fprintf(stderr, "buffer[%u]=%u expected=%u\n", i, buffer[i], source[i]);
			test.Fail(__LINE__, "WritePerChunkMipAndBlendWidthsToBuffer must pack [mip, blendWidth] verbatim");
			return;
		}
	}
}

void TestWritePerChunkMipAndBlendWidthsToBufferHandlesNull(SmartMipTestContext &test)
{
	constexpr uint32_t source[2u] = {1u, 2u};
	uint32_t buffer[2u] = {0xDEADBEEFu, 0xDEADBEEFu};
	WritePerChunkMipAndBlendWidthsToBuffer(nullptr, source, 1u);
	WritePerChunkMipAndBlendWidthsToBuffer(buffer, nullptr, 1u);
	if (buffer[0] != 0xDEADBEEFu || buffer[1] != 0xDEADBEEFu) {
		test.Fail(__LINE__, "WritePerChunkMipAndBlendWidthsToBuffer must guard against null inputs");
	}
}

void TestWritePerChunkMipAndBlendWidthsToBufferZeroChunkCount(SmartMipTestContext &test)
{
	uint32_t buffer[2u] = {0xDEADBEEFu, 0xDEADBEEFu};
	WritePerChunkMipAndBlendWidthsToBuffer(buffer, nullptr, 0u);
	if (buffer[0] != 0xDEADBEEFu || buffer[1] != 0xDEADBEEFu) {
		test.Fail(__LINE__, "Zero chunk count must be a no-op");
	}
}

}  // namespace

int main()
{
	SmartMipTestContext test{};
	TestEnvGateDefault(test);
	TestEnvGateOff(test);
	TestEnvGateOn(test);
	TestComputePerChunkMipLevelCpuCloseChunk(test);
	TestComputePerChunkMipLevelCpuFarChunk(test);
	TestComputePerChunkMipLevelsFromAabbs(test);
	TestBlendWidthZeroMaxBlendWidthReturnsZero(test);
	TestBlendWidthZeroMipReturnsZero(test);
	TestBlendWidthIsBoundedByMax(test);
	TestWritePerChunkMipAndBlendWidthsToBufferPackedLayout(test);
	TestWritePerChunkMipAndBlendWidthsToBufferHandlesNull(test);
	TestWritePerChunkMipAndBlendWidthsToBufferZeroChunkCount(test);

	if (test.failures > 0) {
		std::fprintf(stderr, "ProjectVHzbSmartMipTests: %d failure(s)\n", test.failures);
		return 1;
	}
	std::puts("ProjectVHzbSmartMipTests passed");
	return 0;
}
