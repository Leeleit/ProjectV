#include "render/Cloudscape.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>


namespace {
constexpr char kCloudscapeVertexShaderFilename[] = "cloudscape.vert.spv";
constexpr char kCloudscapeFragmentShaderFilename[] = "cloudscape.frag.spv";

constexpr std::array kCloudscapeDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
	},
};

constexpr VkDescriptorSetLayoutCreateInfo kCloudscapeDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kCloudscapeDescriptorBindings.size()),
	.pBindings = kCloudscapeDescriptorBindings.data(),
};

constexpr std::array kCloudscapeDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT * 3u,
	},
};

float Hash11(float p)
{
	p = p * 0.1031f;
	p = p - std::floor(p);
	p = p * (p + 33.33f);
	p = p * (p + p);
	return glm::fract(p);
}

float ValueNoise2D(float x, float y)
{
	const float xi = std::floor(x);
	const float yi = std::floor(y);
	const float xf = x - xi;
	const float yf = y - yi;
	const float u = xf * xf * (3.0f - 2.0f * xf);
	const float v = yf * yf * (3.0f - 2.0f * yf);
	const float a = Hash11(xi + yi * 57.0f);
	const float b = Hash11(xi + 1.0f + yi * 57.0f);
	const float c = Hash11(xi + (yi + 1.0f) * 57.0f);
	const float d = Hash11(xi + 1.0f + (yi + 1.0f) * 57.0f);
	return std::lerp(std::lerp(a, b, u), std::lerp(c, d, u), v);
}

float Fbm2D(float x, float y, int octaves)
{
	float result = 0.0f;
	float amplitude = 0.5f;
	float frequency = 1.0f;
	for (int i = 0; i < octaves; ++i) {
		result += ValueNoise2D(x * frequency, y * frequency) * amplitude;
		amplitude *= 0.5f;
		frequency *= 2.0f;
	}
	return result;
}

std::vector<uint8_t> GenerateCloudscapeNoiseR8()
{
	std::vector<uint8_t> data(projectv::render::kCloudscapeNoiseTextureSize *
							  projectv::render::kCloudscapeNoiseTextureSize);
	for (uint32_t y = 0; y < projectv::render::kCloudscapeNoiseTextureSize; ++y) {
		for (uint32_t x = 0; x < projectv::render::kCloudscapeNoiseTextureSize; ++x) {
			const float u = static_cast<float>(x) / static_cast<float>(projectv::render::kCloudscapeNoiseTextureSize);
			const float v = static_cast<float>(y) / static_cast<float>(projectv::render::kCloudscapeNoiseTextureSize);
			const float lowFreq = Fbm2D(u * 4.0f, v * 4.0f, 4);
			const float highFreq = Fbm2D(u * 16.0f, v * 16.0f, 3) * 0.35f;
			const float density = glm::clamp((lowFreq + highFreq) * 0.85f - 0.18f, 0.0f, 1.0f);
			data[y * projectv::render::kCloudscapeNoiseTextureSize + x] =
				static_cast<uint8_t>(density * 255.0f);
		}
	}
	return data;
}

bool CreateCloudscapeNoiseImage(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE && context->allocator != nullptr,
		"Render", "CreateCloudscapeNoiseImage.Preconditions", "missing context");
	if (render->cloudscapeNoiseImage != VK_NULL_HANDLE) {
		return true;
	}

	const std::vector<uint8_t> noiseData = GenerateCloudscapeNoiseR8();
	if (noiseData.empty()) {
		return false;
	}

	VkImageCreateInfo imageInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, .samples = VK_SAMPLE_COUNT_1_BIT};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8_UNORM;
	imageInfo.extent = {projectv::render::kCloudscapeNoiseTextureSize, projectv::render::kCloudscapeNoiseTextureSize, 1u};
	imageInfo.mipLevels = 1u;
	imageInfo.arrayLayers = 1u;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

	const VkResult createResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->cloudscapeNoiseImage,
		&render->cloudscapeNoiseAllocation,
		nullptr);
	if (createResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateCloudscapeNoiseImage.vmaCreateImage", createResult);
		return false;
	}

	VmaAllocationInfo allocInfo{};
	vmaGetAllocationInfo(context->allocator, render->cloudscapeNoiseAllocation, &allocInfo);
	void *mapped = nullptr;
	const VkResult mapResult = vmaMapMemory(context->allocator, render->cloudscapeNoiseAllocation, &mapped);
	if (mapResult != VK_SUCCESS || mapped == nullptr) {
		vmaDestroyImage(context->allocator, render->cloudscapeNoiseImage, render->cloudscapeNoiseAllocation);
		render->cloudscapeNoiseImage = VK_NULL_HANDLE;
		render->cloudscapeNoiseAllocation = nullptr;
		runtime::LogVkFailure("CreateCloudscapeNoiseImage.vmaMapMemory", mapResult);
		return false;
	}
	std::memcpy(mapped, noiseData.data(), noiseData.size());
	vmaUnmapMemory(context->allocator, render->cloudscapeNoiseAllocation);
	vmaInvalidateAllocation(context->allocator, render->cloudscapeNoiseAllocation, 0u, VK_WHOLE_SIZE);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->cloudscapeNoiseImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8_UNORM;
	viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	if (vkCreateImageView(context->device, &viewInfo, nullptr, &render->cloudscapeNoiseView) != VK_SUCCESS) {
		vmaDestroyImage(context->allocator, render->cloudscapeNoiseImage, render->cloudscapeNoiseAllocation);
		render->cloudscapeNoiseImage = VK_NULL_HANDLE;
		render->cloudscapeNoiseAllocation = nullptr;
		runtime::LogRuntimeFailure(
			"Render", "CreateCloudscapeNoiseImage.vkCreateImageView", "failed");
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeNoiseImage), VK_OBJECT_TYPE_IMAGE, "CloudscapeNoiseImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeNoiseView), VK_OBJECT_TYPE_IMAGE_VIEW, "CloudscapeNoiseView");
	return true;
}

bool CreateCloudscapeSampler(
	VulkanContextState *context,
	RenderState *render)
{
	PV_CHECK_OR_RETURN(
		context && render && context->device != VK_NULL_HANDLE,
		"Render", "CreateCloudscapeSampler.Preconditions", "missing context");
	if (render->cloudscapeLinearSampler != VK_NULL_HANDLE) {
		return true;
	}

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	if (vkCreateSampler(context->device, &samplerInfo, nullptr, &render->cloudscapeLinearSampler) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render", "CreateCloudscapeSampler.vkCreateSampler", "failed");
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeLinearSampler), VK_OBJECT_TYPE_SAMPLER, "CloudscapeLinearSampler");
	return true;
}

}  // namespace

namespace projectv::render {

void DestroyCloudscapeResources(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->cloudscapePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->cloudscapePipeline, nullptr);
		render->cloudscapePipeline = VK_NULL_HANDLE;
	}
	if (render->cloudscapePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->cloudscapePipelineLayout, nullptr);
		render->cloudscapePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->cloudscapeVertexShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->cloudscapeVertexShaderModule, nullptr);
		render->cloudscapeVertexShaderModule = VK_NULL_HANDLE;
	}
	if (render->cloudscapeFragmentShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->cloudscapeFragmentShaderModule, nullptr);
		render->cloudscapeFragmentShaderModule = VK_NULL_HANDLE;
	}
	for (VkDescriptorSet &descriptorSet : render->cloudscapeDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}
	if (render->cloudscapeDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->cloudscapeDescriptorPool, nullptr);
		render->cloudscapeDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->cloudscapeDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->cloudscapeDescriptorSetLayout, nullptr);
		render->cloudscapeDescriptorSetLayout = VK_NULL_HANDLE;
	}
	if (render->cloudscapeLinearSampler != VK_NULL_HANDLE) {
		vkDestroySampler(context->device, render->cloudscapeLinearSampler, nullptr);
		render->cloudscapeLinearSampler = VK_NULL_HANDLE;
	}
	if (render->cloudscapeNoiseView != VK_NULL_HANDLE) {
		vkDestroyImageView(context->device, render->cloudscapeNoiseView, nullptr);
		render->cloudscapeNoiseView = VK_NULL_HANDLE;
	}
	if (render->cloudscapeNoiseImage != VK_NULL_HANDLE) {
		vmaDestroyImage(context->allocator, render->cloudscapeNoiseImage, render->cloudscapeNoiseAllocation);
		render->cloudscapeNoiseImage = VK_NULL_HANDLE;
		render->cloudscapeNoiseAllocation = nullptr;
	}
	render->cloudscapePipelineEnabled = false;
}

bool CreateCloudscapeResources(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateCloudscapeResources");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateCloudscapeResources.Preconditions", "missing context");
	if (!IsCloudscapeEnabled()) {
		return false;
	}

	DestroyCloudscapeResources(context, render);

	if (!CreateCloudscapeNoiseImage(context, render)) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	if (!CreateCloudscapeSampler(context, render)) {
		DestroyCloudscapeResources(context, render);
		return false;
	}

	const std::vector<char> vertexShaderCode = ReadShaderFile(kCloudscapeVertexShaderFilename);
	if (vertexShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateCloudscapeResources.ReadVertexShader", "cloudscape.vert.spv not found");
		DestroyCloudscapeResources(context, render);
		return false;
	}
	const std::vector<char> fragmentShaderCode = ReadShaderFile(kCloudscapeFragmentShaderFilename);
	if (fragmentShaderCode.empty()) {
		runtime::LogRuntimeFailure(
			"Render", "CreateCloudscapeResources.ReadFragmentShader", "cloudscape.frag.spv not found");
		DestroyCloudscapeResources(context, render);
		return false;
	}

	VkShaderModuleCreateInfo vertexModuleInfo{};
	vertexModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexModuleInfo.codeSize = vertexShaderCode.size();
	vertexModuleInfo.pCode = reinterpret_cast<const uint32_t *>(vertexShaderCode.data());
	if (vkCreateShaderModule(context->device, &vertexModuleInfo, nullptr, &render->cloudscapeVertexShaderModule) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeVertexShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "CloudscapeVertexShader");

	VkShaderModuleCreateInfo fragmentModuleInfo{};
	fragmentModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentModuleInfo.codeSize = fragmentShaderCode.size();
	fragmentModuleInfo.pCode = reinterpret_cast<const uint32_t *>(fragmentShaderCode.data());
	if (vkCreateShaderModule(context->device, &fragmentModuleInfo, nullptr, &render->cloudscapeFragmentShaderModule) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeFragmentShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "CloudscapeFragmentShader");

	const VkResult layoutResult = vkCreateDescriptorSetLayout(
		context->device,
		&kCloudscapeDescriptorSetLayoutInfo,
		nullptr,
		&render->cloudscapeDescriptorSetLayout);
	if (layoutResult != VK_SUCCESS) {
		runtime::LogVkFailure("CreateCloudscapeResources.vkCreateDescriptorSetLayout", layoutResult);
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "CloudscapeDescriptorSetLayout");

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &render->cloudscapeDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(CloudscapePushConstants);
	layoutInfo.pPushConstantRanges = &pushConstantRange;
	if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->cloudscapePipelineLayout) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapePipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "CloudscapePipelineLayout");

	const VkPipelineShaderStageCreateInfo vertexStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = render->cloudscapeVertexShaderModule,
		.pName = "main",
	};
	const VkPipelineShaderStageCreateInfo fragmentStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = render->cloudscapeFragmentShaderModule,
		.pName = "main",
	};

	static constexpr VkPipelineVertexInputStateCreateInfo vertexInputState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	};

	static constexpr VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	};

	static constexpr VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	static constexpr VkPipelineRasterizationStateCreateInfo rasterizationState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.0f,
	};

	static constexpr VkPipelineMultisampleStateCreateInfo multisampleState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	};

	static constexpr VkPipelineDepthStencilStateCreateInfo depthStencilState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,
		.depthWriteEnable = VK_FALSE,
		.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
		.stencilTestEnable = VK_FALSE,
	};

	static constexpr VkPipelineColorBlendAttachmentState colorBlendAttachment{
		.blendEnable = VK_TRUE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo colorBlendState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment,
	};

	static constexpr VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	static constexpr VkPipelineDynamicStateCreateInfo dynamicState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates)),
		.pDynamicStates = dynamicStates,
	};

	VkFormat colorFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &colorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = (VkPipelineShaderStageCreateInfo[2]){vertexStage, fragmentStage};
	pipelineInfo.pVertexInputState = &vertexInputState;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizationState;
	pipelineInfo.pMultisampleState = &multisampleState;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render->cloudscapePipelineLayout;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &render->cloudscapePipeline) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapePipeline), VK_OBJECT_TYPE_PIPELINE, "CloudscapePipeline");

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	poolInfo.poolSizeCount = static_cast<uint32_t>(kCloudscapeDescriptorPoolSizes.size());
	poolInfo.pPoolSizes = kCloudscapeDescriptorPoolSizes.data();
	if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->cloudscapeDescriptorPool) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->cloudscapeDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "CloudscapeDescriptorPool");

	std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		layouts[i] = render->cloudscapeDescriptorSetLayout;
	}
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = render->cloudscapeDescriptorPool;
	allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(context->device, &allocInfo, render->cloudscapeDescriptorSets.data()) != VK_SUCCESS) {
		DestroyCloudscapeResources(context, render);
		return false;
	}

	render->cloudscapePipelineEnabled = true;
	return true;
}

bool RecordCloudscapeRaymarchPass(
	VkCommandBuffer commandBuffer,
	RenderState &render,
	const CloudscapePushConstants &pushConstants,
	VkImageView sceneColorView,
	VkImageView depthView,
	VkExtent2D extent,
	uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordCloudscapeRaymarchPass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (render.cloudscapePipeline == VK_NULL_HANDLE ||
		render.cloudscapePipelineLayout == VK_NULL_HANDLE) {
		return false;
	}
	if (frameIndex >= MAX_FRAMES_IN_FLIGHT) {
		return false;
	}
	if (render.cloudscapeDescriptorSets[frameIndex] == VK_NULL_HANDLE) {
		return false;
	}
	if (sceneColorView == VK_NULL_HANDLE || depthView == VK_NULL_HANDLE) {
		return false;
	}
	if (extent.width == 0u || extent.height == 0u) {
		return false;
	}

	const VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = static_cast<float>(extent.width),
		.height = static_cast<float>(extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};
	const VkRect2D scissor{
		.offset = {0, 0},
		.extent = extent,
	};

	const VkRenderingAttachmentInfo colorAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = sceneColorView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
	};
	const VkRenderingAttachmentInfo depthAttachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
	};

	const VkRenderingInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachment,
		.pDepthAttachment = &depthAttachment,
	};

	vkCmdBeginRendering(commandBuffer, &renderingInfo);
	vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);
	vkCmdSetScissor(commandBuffer, 0u, 1u, &scissor);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render.cloudscapePipeline);
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		render.cloudscapePipelineLayout,
		0u,
		1u,
		&render.cloudscapeDescriptorSets[frameIndex],
		0u,
		nullptr);
	vkCmdPushConstants(
		commandBuffer,
		render.cloudscapePipelineLayout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0u,
		sizeof(CloudscapePushConstants),
		&pushConstants);
	vkCmdDraw(commandBuffer, 3u, 1u, 0u, 0u);
	vkCmdEndRendering(commandBuffer);

	profiling::PlotValue("Cloudscape Pass", 1.0);
	return true;
}

}  // namespace projectv::render
