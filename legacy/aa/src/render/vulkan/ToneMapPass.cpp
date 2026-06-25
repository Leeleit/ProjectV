#include "render/vulkan/ToneMapPass.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

namespace projectv::render {

namespace {

constexpr uint32_t kToneMapDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	if (code.empty()) {
		return VK_NULL_HANDLE;
	}
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = static_cast<uint32_t>(code.size());
	info.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}
	return module;
}

} // namespace

bool CreateHdrColorResources(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	if (!context || !swapchain || !render || !context->allocator) {
		return false;
	}
	if (render->hdrColorImage != VK_NULL_HANDLE) {
		return true;
	}
	if (swapchain->extent.width == 0 || swapchain->extent.height == 0) {
		return false;
	}

	const VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = render->hdrColorFormat,
		.extent = {swapchain->extent.width, swapchain->extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	VmaAllocationInfo allocationResultInfo{};

	const VkResult createImageResult = vmaCreateImage(
		context->allocator,
		&imageInfo,
		&allocationInfo,
		&render->hdrColorImage,
		&render->hdrColorAllocation,
		&allocationResultInfo);
	if (createImageResult != VK_SUCCESS) {
		runtime::LogVkFailure("ToneMap.CreateHdrColorResources.vmaCreateImage", createImageResult);
		return false;
	}
	profiling::RecordAllocation(render->hdrColorAllocation, allocationResultInfo.size, "HdrColorImageAllocation");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = render->hdrColorImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = render->hdrColorFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	const VkResult viewResult = vkCreateImageView(context->device, &viewInfo, nullptr, &render->hdrColorImageView);
	if (viewResult != VK_SUCCESS) {
		runtime::LogVkFailure("ToneMap.CreateHdrColorResources.vkCreateImageView", viewResult);
		profiling::RecordFree(render->hdrColorAllocation, "HdrColorImageAllocation");
		vmaDestroyImage(context->allocator, render->hdrColorImage, render->hdrColorAllocation);
		render->hdrColorImage = VK_NULL_HANDLE;
		render->hdrColorAllocation = nullptr;
		return false;
	}

	if (render->hdrColorSampler == VK_NULL_HANDLE) {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		if (vkCreateSampler(context->device, &samplerInfo, nullptr, &render->hdrColorSampler) != VK_SUCCESS) {
			runtime::LogRuntimeFailure("ToneMap", "CreateHdrColorResources.vkCreateSampler", "vkCreateSampler returned non-success");
		}
	}

	render->hdrColorNeedsInit = true;
	render->hdrColorCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->hdrColorImage), VK_OBJECT_TYPE_IMAGE, "HdrColorImage");
	SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->hdrColorImageView), VK_OBJECT_TYPE_IMAGE_VIEW, "HdrColorImageView");
	return true;
}

void DestroyHdrColorResources(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}
	if (render->hdrColorImageView) {
		vkDestroyImageView(context->device, render->hdrColorImageView, nullptr);
		render->hdrColorImageView = VK_NULL_HANDLE;
	}
	if (render->hdrColorImage && render->hdrColorAllocation) {
		profiling::RecordFree(render->hdrColorAllocation, "HdrColorImageAllocation");
		vmaDestroyImage(context->allocator, render->hdrColorImage, render->hdrColorAllocation);
		render->hdrColorImage = VK_NULL_HANDLE;
		render->hdrColorAllocation = nullptr;
	}
	if (render->hdrColorSampler) {
		vkDestroySampler(context->device, render->hdrColorSampler, nullptr);
		render->hdrColorSampler = VK_NULL_HANDLE;
	}
	render->hdrColorNeedsInit = false;
}

bool CreateToneMapPass(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	if (!context || !swapchain || !render || !context->device) {
		return false;
	}

	if (render->toneMapPipeline != VK_NULL_HANDLE) {
		return true;
	}

	const std::vector<char> vertexCode = ReadShaderFile("tone_map.vert.spv");
	const std::vector<char> fragmentCode = ReadShaderFile("tone_map.frag.spv");
	if (vertexCode.empty() || fragmentCode.empty()) {
		runtime::LogRuntimeFailure("ToneMap", "CreateToneMapPass.ReadShader", "tone_map shader blob missing");
		return false;
	}

	VkShaderModule vertexModule = CreateShaderModule(context->device, vertexCode);
	VkShaderModule fragmentModule = CreateShaderModule(context->device, fragmentCode);
	if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE) {
		if (vertexModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, vertexModule, nullptr);
		}
		if (fragmentModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		}
		runtime::LogRuntimeFailure("ToneMap", "CreateToneMapPass.ShaderModule", "vkCreateShaderModule returned non-success");
		return false;
	}

	const std::array stages{
		VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertexModule,
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

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &blendAttachment;

	VkPipelineDynamicStateCreateInfo dynamicState{};
	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(ToneMapPushConstants);

	if (render->toneMapDescriptorSetLayout == VK_NULL_HANDLE) {
		VkDescriptorSetLayoutBinding bindings[2]{};
		bindings[0].binding = 0;
		bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[0].pImmutableSamplers = nullptr;
		bindings[1].binding = 3;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		bindings[1].pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutCreateInfo.bindingCount = static_cast<uint32_t>(std::size(bindings));
		layoutCreateInfo.pBindings = bindings;
		if (vkCreateDescriptorSetLayout(context->device, &layoutCreateInfo, nullptr, &render->toneMapDescriptorSetLayout) != VK_SUCCESS) {
			vkDestroyShaderModule(context->device, vertexModule, nullptr);
			vkDestroyShaderModule(context->device, fragmentModule, nullptr);
			runtime::LogRuntimeFailure("ToneMap", "CreateToneMapPass.DescriptorSetLayout", "vkCreateDescriptorSetLayout returned non-success");
			return false;
		}
	}

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &render->toneMapDescriptorSetLayout;

	if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->toneMapPipelineLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(context->device, vertexModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		runtime::LogRuntimeFailure("ToneMap", "CreateToneMapPass.PipelineLayout", "vkCreatePipelineLayout returned non-success");
		return false;
	}

	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &swapchain->format;
	renderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	pipelineInfo.pStages = stages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render->toneMapPipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &render->toneMapPipeline) != VK_SUCCESS) {
		vkDestroyShaderModule(context->device, vertexModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		runtime::LogRuntimeFailure("ToneMap", "CreateToneMapPass.Pipeline", "vkCreateGraphicsPipelines returned non-success");
		return false;
	}

	vkDestroyShaderModule(context->device, vertexModule, nullptr);
	vkDestroyShaderModule(context->device, fragmentModule, nullptr);

	render->toneMapPipelineEnabled = true;
	return true;
}

void DestroyToneMapPass(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}
	if (render->toneMapPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->toneMapPipeline, nullptr);
		render->toneMapPipeline = VK_NULL_HANDLE;
	}
	if (render->toneMapPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->toneMapPipelineLayout, nullptr);
		render->toneMapPipelineLayout = VK_NULL_HANDLE;
	}
	if (render->toneMapDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->toneMapDescriptorPool, nullptr);
		render->toneMapDescriptorPool = VK_NULL_HANDLE;
		for (auto &set : render->toneMapDescriptorSets) {
			set = VK_NULL_HANDLE;
		}
	}
	if (render->toneMapDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->toneMapDescriptorSetLayout, nullptr);
		render->toneMapDescriptorSetLayout = VK_NULL_HANDLE;
	}
	render->toneMapPipelineEnabled = false;
}

bool RefreshToneMapResourceBindings(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return false;
	}

	if (render->toneMapDescriptorPool == VK_NULL_HANDLE) {
		VkDescriptorPoolSize poolSizes[2]{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[0].descriptorCount = kToneMapDescriptorSetCount;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = kToneMapDescriptorSetCount;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = kToneMapDescriptorSetCount;
		poolInfo.poolSizeCount = 2;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->toneMapDescriptorPool) != VK_SUCCESS) {
			runtime::LogRuntimeFailure("ToneMap", "RefreshToneMapResourceBindings.Pool", "vkCreateDescriptorPool returned non-success");
			return false;
		}
	}

	const VkDescriptorImageInfo hdrImageInfo{
		.sampler = render->hdrColorSampler,
		.imageView = render->hdrColorImageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	for (uint32_t i = 0; i < kToneMapDescriptorSetCount; ++i) {
		const VkDescriptorBufferInfo sceneLightingBufferInfo{
			.buffer = render->sceneFrameResources[i].sceneLightingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};

		if (render->toneMapDescriptorSets[i] == VK_NULL_HANDLE) {
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = render->toneMapDescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &render->toneMapDescriptorSetLayout;
			if (vkAllocateDescriptorSets(context->device, &allocInfo, &render->toneMapDescriptorSets[i]) != VK_SUCCESS) {
				runtime::LogRuntimeFailure("ToneMap", "RefreshToneMapResourceBindings.Alloc", "vkAllocateDescriptorSets returned non-success");
				return false;
			}
		}

		// Skip writes when scene lighting buffer not yet allocated; safe because descriptor
		// set is created on demand once buffer is valid.
		if (sceneLightingBufferInfo.buffer == VK_NULL_HANDLE) {
			continue;
		}

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0] = VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = render->toneMapDescriptorSets[i],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &sceneLightingBufferInfo,
			.pTexelBufferView = nullptr,
		};
		descriptorWrites[1] = VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = render->toneMapDescriptorSets[i],
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &hdrImageInfo,
			.pBufferInfo = nullptr,
			.pTexelBufferView = nullptr,
		};
		vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}
	return true;
}

void RecordToneMapPass(
	VkCommandBuffer cmd,
	const RenderState &render,
	VkImageView swapchainImageView,
	const VkExtent2D &extent,
	uint32_t imageIndex,
	uint32_t frameIndex)
{
	if (render.toneMapPipeline == VK_NULL_HANDLE || render.toneMapPipelineLayout == VK_NULL_HANDLE) {
		return;
	}

	const VkRenderingAttachmentInfo attachment{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = nullptr,
		.imageView = swapchainImageView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.resolveImageView = VK_NULL_HANDLE,
		.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {},
	};

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = {{0, 0}, extent};
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &attachment;

	vkCmdBeginRendering(cmd, &renderingInfo);
	(void)imageIndex;

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
	vkCmdSetViewport(cmd, 0, 1, &viewport);
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	ToneMapPushConstants push{};
	push.renderExtentInverse = {
		1.0f / static_cast<float>(extent.width),
		1.0f / static_cast<float>(extent.height),
	};
	push.toneMapOperator = static_cast<float>(static_cast<uint8_t>(render.lightingDebugControls.toneMapOperator));
	push.exposure = render.currentSceneLighting.postProcess[0];

	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		render.toneMapPipelineLayout,
		0,
		1,
		&render.toneMapDescriptorSets[frameIndex],
		0,
		nullptr);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, render.toneMapPipeline);
	vkCmdPushConstants(
		cmd,
		render.toneMapPipelineLayout,
		VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		sizeof(push),
		&push);
	vkCmdDraw(cmd, 3, 1, 0, 0);

	vkCmdEndRendering(cmd);
}

} // namespace projectv::render
