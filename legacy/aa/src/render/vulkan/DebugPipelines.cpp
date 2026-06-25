#include "render/vulkan/DebugPipelines.hpp"

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/vulkan/VulkanDebug.hpp"
#include "render/vulkan/VulkanGraphicsPipeline.hpp"

namespace projectv::render {

namespace {

namespace {
bool SupportsDepthAttachment(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0;
}

bool SupportsSampledImage(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

VkFormat LocalChooseDepthFormat(const VkPhysicalDevice physicalDevice)
{
	constexpr std::array candidates{
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D24_UNORM_S8_UINT,
	};
	for (const VkFormat candidate : candidates) {
		if (SupportsDepthAttachment(physicalDevice, candidate) &&
			SupportsSampledImage(physicalDevice, candidate)) {
			return candidate;
		}
	}
	return VK_FORMAT_UNDEFINED;
}
} // namespace

constexpr VkPipelineColorBlendAttachmentState kAlphaBlendAttachmentState{
	.blendEnable = VK_TRUE,
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT,
};

} // namespace

void DestroyDebugOverlayPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	if (render.debugCrosshairPipeline) {
		vkDestroyPipeline(context.device, render.debugCrosshairPipeline, nullptr);
		render.debugCrosshairPipeline = VK_NULL_HANDLE;
	}

	if (render.debugOverlayPipeline) {
		vkDestroyPipeline(context.device, render.debugOverlayPipeline, nullptr);
		render.debugOverlayPipeline = VK_NULL_HANDLE;
	}

	if (render.debugOverlayPipelineLayout) {
		vkDestroyPipelineLayout(context.device, render.debugOverlayPipelineLayout, nullptr);
		render.debugOverlayPipelineLayout = VK_NULL_HANDLE;
	}
}

bool CreateDebugOverlayPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render)
{
	PV_PROFILE_ZONE_N("CreateDebugOverlayPipeline");
	if (render.debugOverlayPipeline != VK_NULL_HANDLE) {
		return true;
	}

	const std::vector<char> vertexCode = ReadShaderFile("debug_overlay.vert.spv");
	const std::vector<char> fragmentCode = ReadShaderFile("debug_overlay.frag.spv");
	if (vertexCode.empty() || fragmentCode.empty()) {
		runtime::LogRuntimeFailure("DebugOverlay", "CreateDebugOverlayPipeline.ReadShader", "shader blob missing");
		return false;
	}

	VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;

	constexpr VkShaderModuleCreateInfo vertexShaderInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = 0,
		.pCode = nullptr,
	};
	VkShaderModuleCreateInfo vertexShaderCreateInfo = vertexShaderInfo;
	vertexShaderCreateInfo.codeSize = vertexCode.size();
	vertexShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(vertexCode.data());
	if (vkCreateShaderModule(context.device, &vertexShaderCreateInfo, nullptr, &vertexShaderModule) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("DebugOverlay", "CreateDebugOverlayPipeline.vkCreateVertexShaderModule", "vkCreateShaderModule returned non-success");
		return false;
	}

	VkShaderModuleCreateInfo fragmentShaderCreateInfo = vertexShaderInfo;
	fragmentShaderCreateInfo.codeSize = fragmentCode.size();
	fragmentShaderCreateInfo.pCode = reinterpret_cast<const uint32_t *>(fragmentCode.data());
	if (vkCreateShaderModule(context.device, &fragmentShaderCreateInfo, nullptr, &fragmentShaderModule) != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		runtime::LogRuntimeFailure("DebugOverlay", "CreateDebugOverlayPipeline.vkCreateFragmentShaderModule", "vkCreateShaderModule returned non-success");
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertexShaderModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragmentShaderModule;
	stages[1].pName = "main";

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

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
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
	dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(DebugOverlayPushConstants);

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;
	layoutInfo.setLayoutCount = 0;
	layoutInfo.pSetLayouts = nullptr;

	const VkResult debugOverlayLayoutResult =
		vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &render.debugOverlayPipelineLayout);
	if (debugOverlayLayoutResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		runtime::LogVkFailure("CreateDebugOverlayPipeline.vkCreatePipelineLayout", debugOverlayLayoutResult);
		return false;
	}

	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &render.hdrColorFormat,
		.depthAttachmentFormat = LocalChooseDepthFormat(context.physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(std::size(stages));
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render.debugOverlayPipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	const VkResult debugOverlayPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugOverlayPipeline);
	if (debugOverlayPipelineResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		runtime::LogVkFailure("CreateDebugOverlayPipeline.vkCreateGraphicsPipelines", debugOverlayPipelineResult);
		return false;
	}

	VkSpecializationInfo crosshairSpecInfo{};
	const VkSpecializationMapEntry crosshairSpecMapEntries[2]{};
	crosshairSpecInfo.mapEntryCount = 2;
	crosshairSpecInfo.pMapEntries = crosshairSpecMapEntries;

	VkPipelineShaderStageCreateInfo crosshairStages[1]{};
	crosshairStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	crosshairStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	crosshairStages[0].module = vertexShaderModule;
	crosshairStages[0].pName = "main";

	VkGraphicsPipelineCreateInfo crosshairInfo = pipelineInfo;
	crosshairInfo.stageCount = 1;
	crosshairInfo.pStages = crosshairStages;
	crosshairInfo.pColorBlendState = &colorBlending;
	crosshairInfo.layout = render.debugOverlayPipelineLayout;

	const VkResult debugCrosshairPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&crosshairInfo,
		nullptr,
		&render.debugCrosshairPipeline);
	if (debugCrosshairPipelineResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		runtime::LogVkFailure("CreateDebugOverlayPipeline.vkCreateGraphicsPipelines.Crosshair", debugCrosshairPipelineResult);
		return false;
	}

	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);

	if (swapchain.extent.width > 0 && swapchain.extent.height > 0) {
		(void)swapchain;
	}

	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(render.debugOverlayPipeline), VK_OBJECT_TYPE_PIPELINE, "DebugOverlayPipeline");
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(render.debugCrosshairPipeline), VK_OBJECT_TYPE_PIPELINE, "DebugCrosshairPipeline");
	return true;
}

void DestroyDebugHudPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	if (render.debugHudPipeline) {
		vkDestroyPipeline(context.device, render.debugHudPipeline, nullptr);
		render.debugHudPipeline = VK_NULL_HANDLE;
	}
	if (render.debugHudPipelineLayout) {
		vkDestroyPipelineLayout(context.device, render.debugHudPipelineLayout, nullptr);
		render.debugHudPipelineLayout = VK_NULL_HANDLE;
	}
}

bool CreateDebugHudPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	PV_PROFILE_ZONE_N("CreateDebugHudPipeline");
	if (render.debugHudPipeline != VK_NULL_HANDLE) {
		return true;
	}

	const std::vector<char> vertexCode = ReadShaderFile("debug_hud.vert.spv");
	const std::vector<char> fragmentCode = ReadShaderFile("debug_hud.frag.spv");
	if (vertexCode.empty() || fragmentCode.empty()) {
		runtime::LogRuntimeFailure("DebugHud", "CreateDebugHudPipeline.ReadShader", "shader blob missing");
		return false;
	}

	VkShaderModuleCreateInfo vertexShaderInfo{};
	vertexShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderInfo.codeSize = vertexCode.size();
	vertexShaderInfo.pCode = reinterpret_cast<const uint32_t *>(vertexCode.data());
	VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(context.device, &vertexShaderInfo, nullptr, &vertexShaderModule) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("DebugHud", "CreateDebugHudPipeline.vkCreateVertexShaderModule", "vkCreateShaderModule returned non-success");
		return false;
	}

	VkShaderModuleCreateInfo fragmentShaderInfo{};
	fragmentShaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderInfo.codeSize = fragmentCode.size();
	fragmentShaderInfo.pCode = reinterpret_cast<const uint32_t *>(fragmentCode.data());
	VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(context.device, &fragmentShaderInfo, nullptr, &fragmentShaderModule) != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		runtime::LogRuntimeFailure("DebugHud", "CreateDebugHudPipeline.vkCreateFragmentShaderModule", "vkCreateShaderModule returned non-success");
		return false;
	}

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertexShaderModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragmentShaderModule;
	stages[1].pName = "main";

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

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
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
	dynamicState.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.pushConstantRangeCount = 0;
	layoutInfo.pPushConstantRanges = nullptr;
	layoutInfo.setLayoutCount = 0;
	layoutInfo.pSetLayouts = nullptr;

	const VkResult debugHudLayoutResult = vkCreatePipelineLayout(context.device, &layoutInfo, nullptr, &render.debugHudPipelineLayout);
	if (debugHudLayoutResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		runtime::LogVkFailure("CreateDebugHudPipeline.vkCreatePipelineLayout", debugHudLayoutResult);
		return false;
	}

	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &render.hdrColorFormat,
		.depthAttachmentFormat = LocalChooseDepthFormat(context.physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &renderingInfo;
	pipelineInfo.stageCount = static_cast<uint32_t>(std::size(stages));
	pipelineInfo.pStages = stages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render.debugHudPipelineLayout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	const VkResult debugHudPipelineResult = vkCreateGraphicsPipelines(
		context.device,
		VK_NULL_HANDLE,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugHudPipeline);
	if (debugHudPipelineResult != VK_SUCCESS) {
		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
		runtime::LogVkFailure("CreateDebugHudPipeline.vkCreateGraphicsPipelines", debugHudPipelineResult);
		return false;
	}

	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);

	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugHudPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugHudPipeline");
	return true;
}

} // namespace projectv::render