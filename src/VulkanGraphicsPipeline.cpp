#include "VulkanGraphicsPipeline.hpp"

#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace {
std::vector<char> ReadFileFromPath(const std::string &path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return {};
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize <= 0) {
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(fileSize));
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	return buffer;
}

std::vector<char> ReadFile(const char *filename)
{
	if (std::vector<char> buffer = ReadFileFromPath(filename); !buffer.empty()) {
		return buffer;
	}

	const char *basePath = SDL_GetBasePath();
	if (basePath) {
		std::string path = basePath;
		path += filename;

		if (std::vector<char> buffer = ReadFileFromPath(path); !buffer.empty()) {
			return buffer;
		}
	}

	SDL_Log("Failed to open file: %s", filename);
	return {};
}

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

bool SupportsDepthAttachment(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

VkFormat ChooseDepthFormat(const VkPhysicalDevice physicalDevice)
{
	constexpr std::array candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (const VkFormat candidate : candidates) {
		if (SupportsDepthAttachment(physicalDevice, candidate)) {
			return candidate;
		}
	}
	return VK_FORMAT_UNDEFINED;
}

bool CreateDepthResources(AppState *state)
{
	const VkFormat depthFormat = ChooseDepthFormat(state->physicalDevice);
	if (depthFormat == VK_FORMAT_UNDEFINED) {
		SDL_Log("No supported depth format found");
		return false;
	}

	const VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depthFormat,
		.extent = {state->extent.width, state->extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (vmaCreateImage(
			state->allocator,
			&imageInfo,
			&allocationInfo,
			&state->depthImage,
			&state->depthAllocation,
			nullptr) != VK_SUCCESS) {
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = state->depthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	if (vkCreateImageView(state->device, &viewInfo, nullptr, &state->depthImageView) != VK_SUCCESS) {
		vmaDestroyImage(state->allocator, state->depthImage, state->depthAllocation);
		state->depthImage = VK_NULL_HANDLE;
		state->depthAllocation = VK_NULL_HANDLE;
		return false;
	}

	state->depthImageNeedsInit = true;
	state->swapchainColorSpace = state->swapchainColorSpace;
	return true;
}
} // namespace

void DestroyGraphicsPipeline(AppState *state)
{
	if (!state || !state->device) {
		return;
	}

	if (state->transparentGraphicsPipeline) {
		vkDestroyPipeline(state->device, state->transparentGraphicsPipeline, nullptr);
		state->transparentGraphicsPipeline = VK_NULL_HANDLE;
	}

	if (state->graphicsPipeline) {
		vkDestroyPipeline(state->device, state->graphicsPipeline, nullptr);
		state->graphicsPipeline = VK_NULL_HANDLE;
	}

	if (state->graphicsPipelineLayout) {
		vkDestroyPipelineLayout(state->device, state->graphicsPipelineLayout, nullptr);
		state->graphicsPipelineLayout = VK_NULL_HANDLE;
	}

	if (state->depthImageView) {
		vkDestroyImageView(state->device, state->depthImageView, nullptr);
		state->depthImageView = VK_NULL_HANDLE;
	}

	if (state->depthImage && state->depthAllocation) {
		vmaDestroyImage(state->allocator, state->depthImage, state->depthAllocation);
		state->depthImage = VK_NULL_HANDLE;
		state->depthAllocation = VK_NULL_HANDLE;
	}

	state->depthImageNeedsInit = false;
}

bool CreateGraphicsPipeline(AppState *state)
{
	if (!state || !state->device || state->swapchainImageViews.empty()) {
		return false;
	}

	if (!CreateDepthResources(state)) {
		return false;
	}

	const std::vector<char> vertexShaderCode = ReadFile("voxel.vert.spv");
	const std::vector<char> fragmentShaderCode = ReadFile("voxel.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		DestroyGraphicsPipeline(state);
		return false;
	}

	VkShaderModule vertexShaderModule = CreateShaderModule(state->device, vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(state->device, fragmentShaderCode);
	if (!vertexShaderModule || !fragmentShaderModule) {
		if (vertexShaderModule) {
			vkDestroyShaderModule(state->device, vertexShaderModule, nullptr);
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(state->device, fragmentShaderModule, nullptr);
		}
		DestroyGraphicsPipeline(state);
		return false;
	}

	const std::array shaderStages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragmentShaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};

	constexpr VkVertexInputBindingDescription bindingDescription{
		.binding = 0,
		.stride = sizeof(RenderVertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};
	constexpr std::array attributeDescriptions{
		VkVertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(RenderVertex, position),
		},
		VkVertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(RenderVertex, normal),
		},
		VkVertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
			.offset = offsetof(RenderVertex, color),
		},
		VkVertexInputAttributeDescription{
			.location = 3,
			.binding = 0,
			.format = VK_FORMAT_R32_SFLOAT,
			.offset = offsetof(RenderVertex, materialKind),
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	constexpr VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendAttachmentState transparentColorBlendAttachment = colorBlendAttachment;
	transparentColorBlendAttachment.blendEnable = VK_TRUE;
	transparentColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	transparentColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	transparentColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	transparentColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	transparentColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	transparentColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineColorBlendStateCreateInfo transparentColorBlending = colorBlending;
	transparentColorBlending.pAttachments = &transparentColorBlendAttachment;

	VkPipelineDepthStencilStateCreateInfo transparentDepthStencil = depthStencil;
	transparentDepthStencil.depthWriteEnable = VK_FALSE;

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(GraphicsPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &state->graphicsPipelineLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(state->device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(state->device, fragmentShaderModule, nullptr);
		DestroyGraphicsPipeline(state);
		return false;
	}

	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &state->swapchainFormat,
		.depthAttachmentFormat = ChooseDepthFormat(state->physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = state->graphicsPipelineLayout;

	if (vkCreateGraphicsPipelines(state->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &state->graphicsPipeline) != VK_SUCCESS) {
		vkDestroyShaderModule(state->device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(state->device, fragmentShaderModule, nullptr);
		DestroyGraphicsPipeline(state);
		return false;
	}

	VkGraphicsPipelineCreateInfo transparentPipelineInfo = pipelineInfo;
	transparentPipelineInfo.pDepthStencilState = &transparentDepthStencil;
	transparentPipelineInfo.pColorBlendState = &transparentColorBlending;
	if (vkCreateGraphicsPipelines(
			state->device,
			VK_NULL_HANDLE,
			1,
			&transparentPipelineInfo,
			nullptr,
			&state->transparentGraphicsPipeline) != VK_SUCCESS) {
		vkDestroyShaderModule(state->device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(state->device, fragmentShaderModule, nullptr);
		DestroyGraphicsPipeline(state);
		return false;
	}

	vkDestroyShaderModule(state->device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(state->device, fragmentShaderModule, nullptr);
	return true;
}
