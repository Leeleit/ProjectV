#include "render/vulkan/TaaResolvePipeline.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr uint32_t kTaaResolveDescriptorSetCount = MAX_FRAMES_IN_FLIGHT;

// Descriptor set layout for the TAA resolve pass. Matches the
// `layout(set = 0, binding = N)` declarations in
// `src/shaders/taa_resolve.frag`:
//   b0 uniform sampler2D  sceneColor       (R16G16B16A16_SFLOAT, written
//                                          by the main voxel pass this frame)
//   b1 uniform sampler2D  historyColor     (R16G16B16A16_SFLOAT, written
//                                          by the previous frame's
//                                          RecordTaaHistoryCopy)
//   b2 uniform sampler2D  depth            (D32_SFLOAT, sampled via the
//                                          same linear sampler as the
//                                          colour targets)
//   b3 readonly buffer     sceneLighting   (std430 SSBO carrying the
//                                          full VoxelSceneLighting struct
//                                          including prevViewProjectionMatrix
//                                          and taaHistoryParams)
constexpr std::array kTaaResolveDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
};
constexpr VkDescriptorSetLayoutCreateInfo kTaaResolveDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = static_cast<uint32_t>(kTaaResolveDescriptorBindings.size()),
	.pBindings = kTaaResolveDescriptorBindings.data(),
};
constexpr VkDescriptorPoolSize kTaaResolveImageSamplerPoolSize{
	.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	.descriptorCount = kTaaResolveDescriptorSetCount * 3u,
};
constexpr VkDescriptorPoolSize kTaaResolveStorageBufferPoolSize{
	.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
	.descriptorCount = kTaaResolveDescriptorSetCount,
};
constexpr std::array kTaaResolveDescriptorPoolSizes{
	kTaaResolveImageSamplerPoolSize,
	kTaaResolveStorageBufferPoolSize,
};

void LogTaaResolveVkFailure(const char *step, const VkResult result);

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult shaderModuleResult = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (shaderModuleResult != VK_SUCCESS) {
		LogTaaResolveVkFailure("vkCreateShaderModule", shaderModuleResult);
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}

void LogTaaResolveVkFailure(const char *step, const VkResult result)
{
	runtime::LogVkFailure(step, result);
}

void LogTaaResolveTextFailure(const char *step, const char *detail)
{
	runtime::LogRuntimeFailure("TaaResolve", step, detail);
}

} // namespace

bool CreateTaaResolvePipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateTaaResolvePipeline");
	PV_CHECK_OR_RETURN(
		context && swapchain && render && context->device && !swapchain->imageViews.empty(),
		"TaaResolve",
		"CreateTaaResolvePipeline.Preconditions",
		"context/swapchain/render is incomplete");
	PV_CHECK_OR_RETURN(
		render->taaLinearSampler != VK_NULL_HANDLE,
		"TaaResolve",
		"CreateTaaResolvePipeline.LinearSampler",
		"taaLinearSampler must be created by the swapchain recreate path");
	PV_CHECK_OR_RETURN(
		render->taaSceneColorTarget != nullptr && render->taaHistoryColorTarget != nullptr,
		"TaaResolve",
		"CreateTaaResolvePipeline.OffscreenTargets",
		"taa offscreen targets must be created by the swapchain recreate path");
	PV_CHECK_OR_RETURN(
		render->taaSceneColorTarget->imageView != VK_NULL_HANDLE &&
			render->taaHistoryColorTarget->imageView != VK_NULL_HANDLE,
		"TaaResolve",
		"CreateTaaResolvePipeline.OffscreenImageViews",
		"taa offscreen target image views must be non-null");

	std::vector<char> vertexShaderCode = ReadShaderFile("taa_resolve.vert.spv");
	std::vector<char> fragmentShaderCode = ReadShaderFile("taa_resolve.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		LogTaaResolveTextFailure("CreateTaaResolvePipeline.ReadFile", "taa_resolve shader blob is empty");
		return false;
	}

	VkShaderModule vertexShaderModule = CreateShaderModule(context->device, vertexShaderCode);
	VkShaderModule fragmentShaderModule = CreateShaderModule(context->device, fragmentShaderCode);
	if (!vertexShaderModule || !fragmentShaderModule) {
		LogTaaResolveTextFailure(
			"CreateTaaResolvePipeline.CreateShaderModule",
			"taa_resolve shader module creation returned null");
		if (vertexShaderModule) {
			vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
		}
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

	// Vertexless fullscreen triangle. The vertex shader reads
	// `gl_VertexIndex` to emit a single oversized triangle that covers the
	// entire framebuffer, so the pipeline does not bind a vertex buffer
	// and `vertexBindingDescriptionCount` / `vertexAttributeDescriptionCount`
	// both stay at zero.
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

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
	rasterizer.cullMode = VK_CULL_MODE_NONE;
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

	// No depth attachment on the resolve pass. The depth view is sampled
	// from the main pass, not written.
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	// No blend. The resolve pass is the final image-writing stage for the
	// frame; history is blended in the fragment shader, not via fixed-function
	// blending.
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	// Push constant range. The TAA resolve shader takes 144 B of push
	// constants (mat4 inverseViewProj + mat4 viewProj + vec2 renderExtentInverse
	// + vec2 reservedPadding). See `ResolvePushConstants` in `core/Types.hpp`.
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(ResolvePushConstants);

	// Descriptor set layout.
	{
		PV_PROFILE_ZONE_N("CreateTaaResolvePipeline.DescriptorSetLayout");
		const VkResult descriptorSetLayoutResult = vkCreateDescriptorSetLayout(
			context->device,
			&kTaaResolveDescriptorSetLayoutInfo,
			nullptr,
			&render->taaResolveDescriptorSetLayout);
		if (descriptorSetLayoutResult != VK_SUCCESS) {
			LogTaaResolveVkFailure(
				"CreateTaaResolvePipeline.vkCreateDescriptorSetLayout",
				descriptorSetLayoutResult);
			vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
			vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->taaResolveDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"TaaResolveDescriptorSetLayout");

	// Pipeline layout (1 descriptor set + 1 push constant range).
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->taaResolveDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	{
		PV_PROFILE_ZONE_N("CreateTaaResolvePipeline.PipelineLayout");
		const VkResult pipelineLayoutResult = vkCreatePipelineLayout(
			context->device,
			&pipelineLayoutInfo,
			nullptr,
			&render->taaResolvePipelineLayout);
		if (pipelineLayoutResult != VK_SUCCESS) {
			LogTaaResolveVkFailure(
				"CreateTaaResolvePipeline.vkCreatePipelineLayout",
				pipelineLayoutResult);
			vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
			vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->taaResolvePipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"TaaResolvePipelineLayout");

	// Color attachment format is the swapchain format (B8G8R8A8_UNORM on
	// most desktops). The resolve writes directly to the swapchain image
	// — no offscreen resolve target, no blit, no format conversion. The
	// shader handles the linear-SFLOAT input -> UNORM output conversion
	// implicitly through tone-mapping + grading.
	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain->format,
		.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
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
	pipelineInfo.layout = render->taaResolvePipelineLayout;

	{
		PV_PROFILE_ZONE_N("CreateTaaResolvePipeline.Pipeline");
		const VkResult pipelineResult = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			1,
			&pipelineInfo,
			nullptr,
			&render->taaResolvePipeline);
		vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
		if (pipelineResult != VK_SUCCESS) {
			LogTaaResolveVkFailure(
				"CreateTaaResolvePipeline.vkCreateGraphicsPipelines",
				pipelineResult);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->taaResolvePipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"TaaResolvePipeline");

	if (!RefreshTaaResolveResourceBindings(context, render)) {
		LogTaaResolveTextFailure(
			"CreateTaaResolvePipeline.RefreshResourceBindings",
			"taa resolve descriptor rebinding failed");
		return false;
	}

	return true;
}

bool RefreshTaaResolveResourceBindings(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("RefreshTaaResolveResourceBindings");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"TaaResolve",
		"RefreshTaaResolveResourceBindings.Preconditions",
		"context/render/device is incomplete");
	PV_CHECK_OR_RETURN(
		render->taaResolveDescriptorSetLayout != VK_NULL_HANDLE,
		"TaaResolve",
		"RefreshTaaResolveResourceBindings.Layout",
		"taa resolve descriptor set layout is null");
	PV_CHECK_OR_RETURN(
		render->taaLinearSampler != VK_NULL_HANDLE,
		"TaaResolve",
		"RefreshTaaResolveResourceBindings.LinearSampler",
		"taa linear sampler is null");
	PV_CHECK_OR_RETURN(
		render->taaSceneColorTarget != nullptr && render->taaHistoryColorTarget != nullptr &&
			render->depthImageView != VK_NULL_HANDLE,
		"TaaResolve",
		"RefreshTaaResolveResourceBindings.BoundResources",
		"offscreen targets / depth view is null");
	for (const SceneFrameResources &frameResources : render->sceneFrameResources) {
		PV_CHECK_OR_RETURN(
			frameResources.sceneLightingBuffer != VK_NULL_HANDLE,
			"TaaResolve",
			"RefreshTaaResolveResourceBindings.PerFrameLightingBuffer",
			"per-frame scene lighting buffer is null");
	}

	for (VkDescriptorSet &descriptorSet : render->taaResolveDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}

	if (render->taaResolveDescriptorPool) {
		vkDestroyDescriptorPool(context->device, render->taaResolveDescriptorPool, nullptr);
		render->taaResolveDescriptorPool = VK_NULL_HANDLE;
	}

	constexpr VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = kTaaResolveDescriptorSetCount,
		.poolSizeCount = static_cast<uint32_t>(kTaaResolveDescriptorPoolSizes.size()),
		.pPoolSizes = kTaaResolveDescriptorPoolSizes.data(),
	};
	{
		PV_PROFILE_ZONE_N("RefreshTaaResolveResourceBindings.CreatePool");
		const VkResult descriptorPoolResult = vkCreateDescriptorPool(
			context->device,
			&poolInfo,
			nullptr,
			&render->taaResolveDescriptorPool);
		if (descriptorPoolResult != VK_SUCCESS) {
			LogTaaResolveVkFailure(
				"RefreshTaaResolveResourceBindings.vkCreateDescriptorPool",
				descriptorPoolResult);
			return false;
		}
	}

	const std::vector setLayouts(render->taaResolveDescriptorSets.size(), render->taaResolveDescriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(render->taaResolveDescriptorSets.size(), VK_NULL_HANDLE);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = render->taaResolveDescriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(setLayouts.size());
	allocateInfo.pSetLayouts = setLayouts.data();
	{
		PV_PROFILE_ZONE_N("RefreshTaaResolveResourceBindings.AllocateSets");
		const VkResult allocateDescriptorSetsResult = vkAllocateDescriptorSets(
			context->device,
			&allocateInfo,
			descriptorSets.data());
		if (allocateDescriptorSetsResult != VK_SUCCESS) {
			LogTaaResolveVkFailure(
				"RefreshTaaResolveResourceBindings.vkAllocateDescriptorSets",
				allocateDescriptorSetsResult);
			vkDestroyDescriptorPool(context->device, render->taaResolveDescriptorPool, nullptr);
			render->taaResolveDescriptorPool = VK_NULL_HANDLE;
			return false;
		}
	}

	const VkDescriptorImageInfo sceneColorImageInfo{
		.sampler = render->taaLinearSampler,
		.imageView = render->taaSceneColorTarget->imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	const VkDescriptorImageInfo historyColorImageInfo{
		.sampler = render->taaLinearSampler,
		.imageView = render->taaHistoryColorTarget->imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};
	const VkDescriptorImageInfo depthImageInfo{
		.sampler = render->taaLinearSampler,
		.imageView = render->depthImageView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
	};
	for (size_t frameIndex = 0; frameIndex < render->taaResolveDescriptorSets.size(); ++frameIndex) {
		const VkDescriptorBufferInfo sceneLightingBufferInfo{
			.buffer = render->sceneFrameResources[frameIndex].sceneLightingBuffer,
			.offset = 0,
			.range = VK_WHOLE_SIZE,
		};
		render->taaResolveDescriptorSets[frameIndex] = descriptorSets[frameIndex];

		const std::array descriptorWrites{
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = render->taaResolveDescriptorSets[frameIndex],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &sceneColorImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = render->taaResolveDescriptorSets[frameIndex],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &historyColorImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = render->taaResolveDescriptorSets[frameIndex],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &depthImageInfo,
				.pBufferInfo = nullptr,
				.pTexelBufferView = nullptr,
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = nullptr,
				.dstSet = render->taaResolveDescriptorSets[frameIndex],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pImageInfo = nullptr,
				.pBufferInfo = &sceneLightingBufferInfo,
				.pTexelBufferView = nullptr,
			},
		};
		vkUpdateDescriptorSets(
			context->device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr);
	}

	return true;
}

void DestroyTaaResolvePipeline(
	VulkanContextState *context,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("DestroyTaaResolvePipeline");
	if (!context || !render || !context->device) {
		return;
	}

	for (VkDescriptorSet &descriptorSet : render->taaResolveDescriptorSets) {
		descriptorSet = VK_NULL_HANDLE;
	}

	if (render->taaResolveDescriptorPool) {
		vkDestroyDescriptorPool(context->device, render->taaResolveDescriptorPool, nullptr);
		render->taaResolveDescriptorPool = VK_NULL_HANDLE;
	}

	if (render->taaResolvePipeline) {
		vkDestroyPipeline(context->device, render->taaResolvePipeline, nullptr);
		render->taaResolvePipeline = VK_NULL_HANDLE;
	}

	if (render->taaResolvePipelineLayout) {
		vkDestroyPipelineLayout(context->device, render->taaResolvePipelineLayout, nullptr);
		render->taaResolvePipelineLayout = VK_NULL_HANDLE;
	}

	if (render->taaResolveDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(context->device, render->taaResolveDescriptorSetLayout, nullptr);
		render->taaResolveDescriptorSetLayout = VK_NULL_HANDLE;
	}

	render->taaResolveAttachmentImageView = VK_NULL_HANDLE;
}
