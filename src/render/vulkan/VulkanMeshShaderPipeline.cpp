#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

#include "SDL3/SDL_log.h"

namespace {
constexpr uint32_t kMeshShaderDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;
constexpr char kMeshCullShaderFilename[] = "voxel_mesh_pre.comp.spv";
constexpr char kMeshShaderFilename[] = "voxel_mesh.mesh.spv";
// EVIL: mesh shader pipeline previously used `voxel.frag.spv` (default, no
// VOXEL_RTX_ENABLED define), which silently disabled RTX shadows when
// `meshShaderEnabled=true`. Switch to `voxel.frag.rtx.spv` (and its TAA variant)
// so the ray query dispatch path runs in both mesh and graphics pipelines.
// Per TODO.md §5.2.B the shadow visibility lives in `voxel.frag`; the mesh
// path inherits it automatically once the right .spv is bound.
constexpr char kVoxelFragmentShaderFilename[] = "voxel.frag.rtx.spv";

// EVIL: kMeshMaxOutputVertices/Primitives=256 = Vulkan 1.3 spec minimum for VkPhysicalDeviceMeshShaderPropertiesEXT. Real hardware (RTX 3060 Ti GA104, RDNA 4, Battlemage) reports 1024/256+. Clamped to 256 here for cross-vendor safety; bump to maxMeshOutputVertices from device if any 8³ chunk exceeds the limit.
constexpr uint32_t kMeshMaxOutputVertices = 256u;
constexpr uint32_t kMeshMaxOutputPrimitives = 256u;
// EVIL: kMeshPushConstantSize=128 = Vulkan spec min. VoxelMeshingPushConstants(64) + viewProjection(64) = 128 exactly. If VoxelMeshingPushConstants grows (e.g. per-chunk cascade count) this overflows the min spec; check first.
constexpr uint32_t kMeshPushConstantSize = 128u;

constexpr std::array kMeshShaderDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kMeshShaderDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kMeshShaderDescriptorBindings.size()),
	.pBindings = kMeshShaderDescriptorBindings.data(),
};

constexpr std::array kMeshShaderDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = kMeshShaderDescriptorSetCount * 4u,
	},
};

VkShaderModule CreateMeshShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderModule", result);
		return VK_NULL_HANDLE;
	}
	return shaderModule;
}
}  // namespace

namespace projectv::render {

bool IsMeshShaderPipelineRequested()
{
	if (const char *value = std::getenv("PROJECTV_MESH_SHADER_PIPELINE")) {
		return value[0] != '\0' && value[0] != '0';
	}
	return false;
}

namespace {
std::array<float, 4> MakeFrustumPlane(
	const math::Vec3 &normal,
	const math::Vec3 &cameraPos,
	float offset)
{
	const float w = -projectv::math::dot(cameraPos, normal) - offset;
	return {normal.x, normal.y, normal.z, w};
}
}  // namespace

MeshCullPushConstants BuildMeshCullPushConstants(
	const ChunkCullingParameters &parameters,
	uint32_t chunkDescriptorCount)
{
	MeshCullPushConstants result{};
	result.dispatchParams = {chunkDescriptorCount, 0u, 0u, 0u};

	const math::Vec3 cameraPos{
		parameters.cameraPositionAndMaxDistance.x,
		parameters.cameraPositionAndMaxDistance.y,
		parameters.cameraPositionAndMaxDistance.z,
		0.0f,
	};
	const math::Vec3 forward{
		parameters.cameraForwardAndTanHalfVerticalFov.x,
		parameters.cameraForwardAndTanHalfVerticalFov.y,
		parameters.cameraForwardAndTanHalfVerticalFov.z,
		0.0f,
	};
	const math::Vec3 right{
		parameters.cameraRightAndTanHalfHorizontalFov.x,
		parameters.cameraRightAndTanHalfHorizontalFov.y,
		parameters.cameraRightAndTanHalfHorizontalFov.z,
		0.0f,
	};
	const math::Vec3 up{
		parameters.cameraUpAndNearPlane.x,
		parameters.cameraUpAndNearPlane.y,
		parameters.cameraUpAndNearPlane.z,
		0.0f,
	};
	const float tanHalfVFov = std::max(parameters.cameraForwardAndTanHalfVerticalFov.w, 0.0f);
	const float tanHalfHFov = std::max(parameters.cameraRightAndTanHalfHorizontalFov.w, 0.0f);
	const float nearPlane = std::max(parameters.cameraUpAndNearPlane.w, 0.0f);
	const float maxDistance = parameters.cameraPositionAndMaxDistance.w;

	const math::Vec3 leftNormal{
		forward.x * tanHalfHFov + right.x,
		forward.y * tanHalfHFov + right.y,
		forward.z * tanHalfHFov + right.z,
		0.0f,
	};
	const math::Vec3 rightNormal{
		forward.x * tanHalfHFov - right.x,
		forward.y * tanHalfHFov - right.y,
		forward.z * tanHalfHFov - right.z,
		0.0f,
	};
	const math::Vec3 bottomNormal{
		forward.x * tanHalfVFov + up.x,
		forward.y * tanHalfVFov + up.y,
		forward.z * tanHalfVFov + up.z,
		0.0f,
	};
	const math::Vec3 topNormal{
		forward.x * tanHalfVFov - up.x,
		forward.y * tanHalfVFov - up.y,
		forward.z * tanHalfVFov - up.z,
		0.0f,
	};

	result.frustumPlanes[0] = MakeFrustumPlane(leftNormal, cameraPos, 0.0f);
	result.frustumPlanes[1] = MakeFrustumPlane(rightNormal, cameraPos, 0.0f);
	result.frustumPlanes[2] = MakeFrustumPlane(bottomNormal, cameraPos, 0.0f);
	result.frustumPlanes[3] = MakeFrustumPlane(topNormal, cameraPos, 0.0f);
	result.frustumPlanes[4] = MakeFrustumPlane(forward, cameraPos, -nearPlane);
	result.frustumPlanes[5] = MakeFrustumPlane(math::Vec3{-forward.x, -forward.y, -forward.z, 0.0f}, cameraPos, -maxDistance);
	return result;
}

bool RefreshMeshShaderResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshMeshShaderResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Render",
		"RefreshMeshShaderResourceBindings.Preconditions",
		"context/render/device is incomplete");
	if (!render->meshShaderDescriptorSetLayout) {
		return true;
	}
	if (render->meshShaderEnabled == false) {
		return true;
	}

	if (render->meshShaderDescriptorPool) {
		vkDestroyDescriptorPool(context->device, render->meshShaderDescriptorPool, nullptr);
		render->meshShaderDescriptorPool = VK_NULL_HANDLE;
	}

	static constexpr VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kMeshShaderDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(kMeshShaderDescriptorPoolSizes.size()),
		.pPoolSizes = kMeshShaderDescriptorPoolSizes.data(),
	};
	const VkResult poolResult = vkCreateDescriptorPool(
		context->device,
		&poolInfo,
		nullptr,
		&render->meshShaderDescriptorPool);
	if (poolResult != VK_SUCCESS) {
		runtime::LogVkFailure("RefreshMeshShaderResourceBindings.vkCreateDescriptorPool", poolResult);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->meshShaderDescriptorPool),
		VK_OBJECT_TYPE_DESCRIPTOR_POOL,
		"MeshShaderDescriptorPool");

	const std::vector setLayouts(render->sceneFrameResources.size(), render->meshShaderDescriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(render->sceneFrameResources.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->meshShaderDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	const VkResult allocResult = vkAllocateDescriptorSets(
		context->device,
		&allocateInfo,
		descriptorSets.data());
	if (allocResult != VK_SUCCESS) {
		runtime::LogVkFailure("RefreshMeshShaderResourceBindings.vkAllocateDescriptorSets", allocResult);
		vkDestroyDescriptorPool(context->device, render->meshShaderDescriptorPool, nullptr);
		render->meshShaderDescriptorPool = VK_NULL_HANDLE;
		return false;
	}

	for (size_t frameIndex = 0; frameIndex < render->sceneFrameResources.size(); ++frameIndex) {
		SceneFrameResources &frameResources = render->sceneFrameResources[frameIndex];
		frameResources.meshShaderDescriptorSet = descriptorSets[frameIndex];

		const VkDescriptorBufferInfo chunkDescriptorInfo{
			.buffer = frameResources.chunkDescriptorBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo chunkVoxelPayloadInfo{
			.buffer = frameResources.chunkVoxelPayloadBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo visibleChunkIdInfo{
			.buffer = frameResources.visibleChunkIdBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		const VkDescriptorBufferInfo visibilityCounterInfo{
			.buffer = frameResources.visibilityCounterBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};

		const std::array writes{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkDescriptorInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &chunkVoxelPayloadInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &visibleChunkIdInfo,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = frameResources.meshShaderDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &visibilityCounterInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(writes.size()),
			writes.data(),
			0u,
			nullptr);
	}

	return true;
}

void DestroyMeshShaderPipelines(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}

	for (SceneFrameResources &frameResources : render->sceneFrameResources) {
		frameResources.meshShaderDescriptorSet = VK_NULL_HANDLE;
	}

	if (context->device != VK_NULL_HANDLE) {
		if (render->meshCullPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->meshCullPipeline, nullptr);
			render->meshCullPipeline = VK_NULL_HANDLE;
		}
		if (render->meshShaderPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context->device, render->meshShaderPipeline, nullptr);
			render->meshShaderPipeline = VK_NULL_HANDLE;
		}
		if (render->meshCullPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->meshCullPipelineLayout, nullptr);
			render->meshCullPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->meshShaderPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context->device, render->meshShaderPipelineLayout, nullptr);
			render->meshShaderPipelineLayout = VK_NULL_HANDLE;
		}
		if (render->meshShaderDescriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context->device, render->meshShaderDescriptorSetLayout, nullptr);
			render->meshShaderDescriptorSetLayout = VK_NULL_HANDLE;
		}
		if (render->meshCullShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->meshCullShaderModule, nullptr);
			render->meshCullShaderModule = VK_NULL_HANDLE;
		}
		if (render->meshShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, render->meshShaderModule, nullptr);
			render->meshShaderModule = VK_NULL_HANDLE;
		}
		if (render->meshShaderDescriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(context->device, render->meshShaderDescriptorPool, nullptr);
			render->meshShaderDescriptorPool = VK_NULL_HANDLE;
		}
	}
	render->meshShaderEnabled = false;
}

bool CreateMeshShaderPipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateMeshShaderPipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Render",
		"CreateMeshShaderPipelines.Preconditions",
		"context/render/device is incomplete");
	if (!IsMeshShaderPipelineRequested()) {
		return false;
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &meshShaderFeatures;
	vkGetPhysicalDeviceFeatures2(context->physicalDevice, &deviceFeatures2);
	if (meshShaderFeatures.meshShader != VK_TRUE) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"PROJECTV_MESH_SHADER_PIPELINE=ON but device lacks meshShader feature; mesh shader path disabled");
		return false;
	}

	VkPhysicalDeviceMeshShaderPropertiesEXT meshProperties{};
	meshProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
	VkPhysicalDeviceProperties2 deviceProperties2{};
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &meshProperties;
	vkGetPhysicalDeviceProperties2(context->physicalDevice, &deviceProperties2);

	if (meshProperties.maxMeshOutputVertices < 256u || meshProperties.maxMeshOutputPrimitives < 256u) {
		SDL_LogWarn(
			SDL_LOG_CATEGORY_APPLICATION,
			"PROJECTV_MESH_SHADER_PIPELINE=ON but device maxMeshOutputVertices=%u/maxMeshOutputPrimitives=%u (need >=256/256); mesh shader path disabled",
			meshProperties.maxMeshOutputVertices,
			meshProperties.maxMeshOutputPrimitives);
		return false;
	}

	render->meshShaderMaxOutputVertices = std::min(meshProperties.maxMeshOutputVertices, kMeshMaxOutputVertices);
	render->meshShaderMaxOutputPrimitives = std::min(meshProperties.maxMeshOutputPrimitives, kMeshMaxOutputPrimitives);

	DestroyMeshShaderPipelines(context, render);

	const std::vector<char> cullShaderCode = ReadShaderFile(kMeshCullShaderFilename);
	const std::vector<char> meshShaderCode = ReadShaderFile(kMeshShaderFilename);
	if (cullShaderCode.empty() || meshShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateMeshShaderPipelines.ReadShaderFile",
			"failed to read mesh shader pipeline SPIR-V");
		DestroyMeshShaderPipelines(context, render);
		return false;
	}

	render->meshCullShaderModule = CreateMeshShaderModule(context->device, cullShaderCode);
	render->meshShaderModule = CreateMeshShaderModule(context->device, meshShaderCode);
	if (render->meshCullShaderModule == VK_NULL_HANDLE || render->meshShaderModule == VK_NULL_HANDLE) {
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshCullShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "MeshCullShaderModule");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "MeshShaderModule");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kMeshShaderDescriptorSetLayoutInfo,
		nullptr,
		&render->meshShaderDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderPipelines.vkCreateDescriptorSetLayout", layoutResult);
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->meshShaderDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"MeshShaderDescriptorSetLayout");

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = kMeshPushConstantSize;

	VkPipelineLayoutCreateInfo cullLayoutInfo{};
	cullLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	cullLayoutInfo.setLayoutCount = 1;
	cullLayoutInfo.pSetLayouts = &render->meshShaderDescriptorSetLayout;
	cullLayoutInfo.pushConstantRangeCount = 1;
	cullLayoutInfo.pPushConstantRanges = &pushConstantRange;
	const VkResult cullLayoutResult = vkCreatePipelineLayout(
		context->device,
		&cullLayoutInfo,
		nullptr,
		&render->meshCullPipelineLayout);
	if (cullLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderPipelines.cull.vkCreatePipelineLayout", cullLayoutResult);
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshCullPipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "MeshCullPipelineLayout");

	VkPipelineLayoutCreateInfo meshLayoutInfo{};
	meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	meshLayoutInfo.setLayoutCount = 1;
	meshLayoutInfo.pSetLayouts = &render->meshShaderDescriptorSetLayout;
	meshLayoutInfo.pushConstantRangeCount = 1;
	meshLayoutInfo.pPushConstantRanges = &pushConstantRange;
	const VkResult meshLayoutResult = vkCreatePipelineLayout(
		context->device,
		&meshLayoutInfo,
		nullptr,
		&render->meshShaderPipelineLayout);
	if (meshLayoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderPipelines.mesh.vkCreatePipelineLayout", meshLayoutResult);
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshShaderPipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "MeshShaderPipelineLayout");

	const VkPipelineShaderStageCreateInfo cullStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = render->meshCullShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	VkComputePipelineCreateInfo cullPipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = cullStage};
	cullPipelineInfo.layout = render->meshCullPipelineLayout;
	cullPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	cullPipelineInfo.basePipelineIndex = 0;
	const VkResult cullPipelineResult = vkCreateComputePipelines(
		context->device,
		VK_NULL_HANDLE,
		1u,
		&cullPipelineInfo,
		nullptr,
		&render->meshCullPipeline);
	if (cullPipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderPipelines.vkCreateComputePipelines", cullPipelineResult);
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshCullPipeline), VK_OBJECT_TYPE_PIPELINE, "MeshCullPipeline");

	const std::vector<char> fragmentCode = ReadShaderFile(kVoxelFragmentShaderFilename);
	if (fragmentCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"CreateMeshShaderPipelines.ReadFragmentShader",
			"failed to read voxel.frag.spv");
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	VkShaderModule fragmentModule = CreateMeshShaderModule(context->device, fragmentCode);
	if (fragmentModule == VK_NULL_HANDLE) {
		DestroyMeshShaderPipelines(context, render);
		return false;
	}

	const VkPipelineShaderStageCreateInfo meshStages[]{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_MESH_BIT_EXT,
			.module = render->meshShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 0u;
	vertexInput.pVertexBindingDescriptions = nullptr;
	vertexInput.vertexAttributeDescriptionCount = 0u;
	vertexInput.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1u;
	viewportState.scissorCount = 1u;

	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlend{};
	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlend.attachmentCount = 1u;
	colorBlend.pAttachments = &colorBlendAttachment;

	VkDynamicState dynamicStates[]{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2u;
	dynamicState.pDynamicStates = dynamicStates;

	static constexpr VkFormat colorFormats[1]{VK_FORMAT_B10G11R11_UFLOAT_PACK32};
	VkPipelineRenderingCreateInfo renderingCreateInfo{};
	renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingCreateInfo.colorAttachmentCount = 1u;
	renderingCreateInfo.pColorAttachmentFormats = colorFormats;
	renderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

	VkGraphicsPipelineCreateInfo meshPipelineInfo{};
	meshPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	meshPipelineInfo.pNext = &renderingCreateInfo;
	meshPipelineInfo.stageCount = 2u;
	meshPipelineInfo.pStages = meshStages;
	meshPipelineInfo.pVertexInputState = &vertexInput;
	meshPipelineInfo.pInputAssemblyState = &inputAssembly;
	meshPipelineInfo.pViewportState = &viewportState;
	meshPipelineInfo.pRasterizationState = &rasterization;
	meshPipelineInfo.pMultisampleState = &multisample;
	meshPipelineInfo.pDepthStencilState = &depthStencil;
	meshPipelineInfo.pColorBlendState = &colorBlend;
	meshPipelineInfo.pDynamicState = &dynamicState;
	meshPipelineInfo.layout = render->meshShaderPipelineLayout;
	meshPipelineInfo.renderPass = VK_NULL_HANDLE;
	meshPipelineInfo.subpass = 0;

	const VkResult meshPipelineResult = vkCreateGraphicsPipelines(
		context->device,
		VK_NULL_HANDLE,
		1u,
		&meshPipelineInfo,
		nullptr,
		&render->meshShaderPipeline);
	if (meshPipelineResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderPipelines.vkCreateGraphicsPipelines", meshPipelineResult);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		DestroyMeshShaderPipelines(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshShaderPipeline), VK_OBJECT_TYPE_PIPELINE, "MeshShaderPipeline");

	vkDestroyShaderModule(context->device, fragmentModule, nullptr);

	// noinspection CppDFAConstantConditions, CppDFAUnreachableCode
	if (!RefreshMeshShaderResourceBindings(context, render)) {
		DestroyMeshShaderPipelines(context, render);
		return false;
	}

	render->meshShaderEnabled = true;
	SDL_LogInfo(
		SDL_LOG_CATEGORY_APPLICATION,
		"Mesh shader pipeline enabled (maxMeshOutputVertices=%u, maxMeshOutputPrimitives=%u)",
		render->meshShaderMaxOutputVertices,
		render->meshShaderMaxOutputPrimitives);
	return true;
}

bool RecordMeshShaderPreCull(
	VkCommandBuffer commandBuffer,
	VulkanContextState *context,
	RenderState &render,
	SceneFrameResources &frameResources,
	const MeshCullPushConstants &cullPushConstants)
{
	if (!render.meshShaderEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE || context == nullptr) {
		return false;
	}
	if (render.meshCullPipeline == VK_NULL_HANDLE ||
		render.meshCullPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.meshShaderDescriptorSet == VK_NULL_HANDLE ||
		frameResources.visibleChunkIdBuffer == VK_NULL_HANDLE ||
		frameResources.visibilityCounterBuffer == VK_NULL_HANDLE) {
		return false;
	}

	std::memset(frameResources.visibilityCounterMappedData, 0, sizeof(uint32_t));

	VkBufferMemoryBarrier2 fillBarrier{};
	fillBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	fillBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	fillBarrier.buffer = frameResources.visibleChunkIdBuffer;
	fillBarrier.offset = 0;
	fillBarrier.size = sizeof(uint32_t) * static_cast<VkDeviceSize>(std::max(render.visibleChunkIdCapacity, 1u));

	{
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = 1u;
		depInfo.pBufferMemoryBarriers = &fillBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, render.meshCullPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		render.meshCullPipelineLayout,
		0u,
		1u,
		&frameResources.meshShaderDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.meshCullPipelineLayout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0u,
		sizeof(MeshCullPushConstants),
		&cullPushConstants);

	const uint32_t chunkCount = cullPushConstants.dispatchParams[0];
	if (chunkCount > 0u) {
		const uint32_t workgroupCount = (chunkCount + 63u) / 64u;
		vkCmdDispatch(commandBuffer, workgroupCount, 1u, 1u);
	}

	VkBufferMemoryBarrier2 meshBarrier{};
	meshBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	meshBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	meshBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	meshBarrier.dstStageMask = VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
	meshBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	meshBarrier.buffer = frameResources.visibleChunkIdBuffer;
	meshBarrier.offset = 0;
	meshBarrier.size = VK_WHOLE_SIZE;
	{
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.bufferMemoryBarrierCount = 1u;
		depInfo.pBufferMemoryBarriers = &meshBarrier;
		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	return true;
}

bool RecordMeshShaderDraw(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	SceneFrameResources &frameResources,
	const MeshDrawPushConstants &drawPushConstants,
	uint32_t chunkCount)
{
	if (!render.meshShaderEnabled) {
		return false;
	}
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.meshShaderPipeline == VK_NULL_HANDLE ||
		render.meshShaderPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameResources.meshShaderDescriptorSet == VK_NULL_HANDLE) {
		return false;
	}

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.meshShaderPipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		render.meshShaderPipelineLayout,
		0u,
		1u,
		&frameResources.meshShaderDescriptorSet,
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.meshShaderPipelineLayout,
		VK_SHADER_STAGE_MESH_BIT_EXT,
		0u,
		sizeof(MeshDrawPushConstants),
		&drawPushConstants);

	if (chunkCount > 0u) {
		vkCmdDrawMeshTasksEXT(commandBuffer, chunkCount, 1u, 1u);
	}

	return true;
}

}  // namespace projectv::render
