#include "volk.h"
#include "render/RayTracedShadows.hpp"
#include "SDL3/SDL_log.h"

#include <algorithm>
#include <cstring>

namespace projectv::render {

void RayTracedShadows::UpdateTlas(
	const VulkanContextState &context,
	const std::vector<uint32_t> &visibleChunkIndices,
	const std::vector<VkTransformMatrixKHR> &visibleChunkTransforms)
{
	if (m_config.tlasInstanceMappedData == nullptr) {
		return;
	}
	const size_t count = std::min(visibleChunkIndices.size(), visibleChunkTransforms.size());
	const size_t capacity = m_config.tlasInstanceCapacityBytes / sizeof(VkAccelerationStructureInstanceKHR);
	const size_t clamped = std::min(count, capacity);
	auto *const instances = static_cast<VkAccelerationStructureInstanceKHR *>(m_config.tlasInstanceMappedData);
	for (size_t i = 0; i < clamped; ++i) {
		const uint32_t chunkIndex = visibleChunkIndices[i];
		instances[i].transform = visibleChunkTransforms[i];
		instances[i].instanceCustomIndex = chunkIndex & 0xFFFFFFu;
		instances[i].mask = 0xFFu;
		instances[i].instanceShaderBindingTableRecordOffset = 0u;
		instances[i].flags = 0u;
		if (chunkIndex < m_config.blasDeviceAddresses.size()) {
			instances[i].accelerationStructureReference = m_config.blasDeviceAddresses[chunkIndex];
		} else {
			instances[i].accelerationStructureReference = 0u;
		}
	}
	std::memset(
		instances + clamped,
		0,
		(m_config.tlasInstanceCapacityBytes - clamped * sizeof(VkAccelerationStructureInstanceKHR)));
	m_config.tlasInstanceCount = static_cast<uint32_t>(clamped);
	m_config.tlasRebuildCount += 1u;

	static int capWarnCount = 0;
	if (count > capacity && capWarnCount < 5) {
		SDL_Log("UpdateTlas WARN: visible chunks %zu > TLAS capacity %zu — extra chunks dropped from shadow TLAS", count, capacity);
		++capWarnCount;
	}

	static int diagCount = 0;
	if (diagCount < 3 && clamped > 0) {
		uint32_t withNonZeroAddr = 0u;
		for (size_t i = 0; i < clamped; ++i) {
			if (instances[i].accelerationStructureReference != 0u) {
				++withNonZeroAddr;
			}
		}
		SDL_Log("UpdateTlas diag: clamped=%zu tlasInstanceCount=%u withNonZeroRef=%u tlas=%p",
			clamped, m_config.tlasInstanceCount, withNonZeroAddr, (void*)m_config.tlas);
		++diagCount;
	}

	if (context.allocator != nullptr && m_config.tlasInstanceAllocation != nullptr) {
		vmaFlushAllocation(
			context.allocator,
			m_config.tlasInstanceAllocation,
			0u,
			m_config.tlasInstanceCapacityBytes);
	}
}

void RayTracedShadows::RecordTlasBuild(
	VkCommandBuffer commandBuffer,
	const VulkanContextState &context)
{
	if (!m_config.enabled || commandBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (m_config.tlas == VK_NULL_HANDLE || m_config.tlasInstanceBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (m_config.tlasInstanceCount == 0u) {
		return;
	}
	if (m_config.tlasInstanceDeviceAddress == 0u || m_config.scratchDeviceAddress == 0u) {
		m_config.fallbackCount += 1u;
		return;
	}

	VkBufferMemoryBarrier instanceBarrier{};
	instanceBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	instanceBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
	instanceBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	instanceBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	instanceBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	instanceBarrier.buffer = m_config.tlasInstanceBuffer;
	instanceBarrier.offset = 0u;
	instanceBarrier.size = static_cast<VkDeviceSize>(m_config.tlasInstanceCount) * sizeof(VkAccelerationStructureInstanceKHR);
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		0u,
		0u,
		nullptr,
		1u,
		&instanceBarrier,
		0u,
		nullptr);

	if (m_config.scratchBuffer != VK_NULL_HANDLE && m_config.scratchCapacityBytes > 0u) {
		VkBufferMemoryBarrier scratchBarrier{};
		scratchBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		scratchBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		scratchBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		scratchBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scratchBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		scratchBarrier.buffer = m_config.scratchBuffer;
		scratchBarrier.offset = 0u;
		scratchBarrier.size = m_config.scratchCapacityBytes;
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			0u,
			0u,
			nullptr,
			1u,
			&scratchBarrier,
			0u,
			nullptr);
	}

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
	instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	instancesData.arrayOfPointers = VK_FALSE;
	instancesData.data.deviceAddress = m_config.tlasInstanceDeviceAddress;

	VkAccelerationStructureGeometryKHR instancesGeometry{};
	instancesGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	instancesGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	instancesGeometry.geometry.instances = instancesData;
	instancesGeometry.flags = 0u;

	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
	tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	tlasBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	tlasBuildInfo.dstAccelerationStructure = m_config.tlas;
	tlasBuildInfo.geometryCount = 1u;
	tlasBuildInfo.pGeometries = &instancesGeometry;
	tlasBuildInfo.scratchData.deviceAddress = m_config.scratchDeviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
	tlasRangeInfo.primitiveCount = m_config.tlasInstanceCount;
	tlasRangeInfo.primitiveOffset = 0u;
	tlasRangeInfo.firstVertex = 0u;
	tlasRangeInfo.transformOffset = 0u;
	const VkAccelerationStructureBuildRangeInfoKHR *tlasRangeInfos[1] = { &tlasRangeInfo };

	vkCmdBuildAccelerationStructuresKHR(
		commandBuffer,
		1u,
		&tlasBuildInfo,
		tlasRangeInfos);

	VkMemoryBarrier tlasToFragmentBarrier{};
	tlasToFragmentBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	tlasToFragmentBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	tlasToFragmentBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0u,
		1u,
		&tlasToFragmentBarrier,
		0u,
		nullptr,
		0u,
		nullptr);

	m_config.shadowRayDispatchCount += 1u;
}

} // namespace projectv::render
