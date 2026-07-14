#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanGraphicsPipelineInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

void DestroyGraphicsResourceBindings(
	VulkanContextState &context,
	RenderState &render)
{
	for (SceneFrameResources &frameResources : render.sceneFrameResources) {
		frameResources.graphicsDescriptorSet = VK_NULL_HANDLE;
	}

	if (render.graphicsDescriptorPool) {
		vkDestroyDescriptorPool(context.device, render.graphicsDescriptorPool, nullptr);
		render.graphicsDescriptorPool = VK_NULL_HANDLE;
	}

	if (render.graphicsDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(context.device, render.graphicsDescriptorSetLayout, nullptr);
		render.graphicsDescriptorSetLayout = VK_NULL_HANDLE;
	}
}

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

namespace {

struct DebugOverlayShaderModules {
	VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
};

void DestroyDebugOverlayShaderModules(VulkanContextState &context, DebugOverlayShaderModules &modules)
{
	if (modules.vertexShaderModule) {
		vkDestroyShaderModule(context.device, modules.vertexShaderModule, nullptr);
	}
	if (modules.fragmentShaderModule) {
		vkDestroyShaderModule(context.device, modules.fragmentShaderModule, nullptr);
	}
	modules = {};
}

bool LoadDebugOverlayShaderModules(VulkanContextState &context, DebugOverlayShaderModules &outModules)
{
	std::vector<char> vertexShaderCode = ReadShaderFile("debug_overlay.vert.spv");
	std::vector<char> fragmentShaderCode = ReadShaderFile("debug_overlay.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		LogGraphicsPipelineTextFailure("CreateDebugOverlayPipeline.ReadFile", "debug overlay shader blob is empty");
		return false;
	}

	outModules.vertexShaderModule = CreateShaderModule(context.device, vertexShaderCode);
	outModules.fragmentShaderModule = CreateShaderModule(context.device, fragmentShaderCode);
	if (!outModules.vertexShaderModule || !outModules.fragmentShaderModule) {
		LogGraphicsPipelineTextFailure(
			"CreateDebugOverlayPipeline.CreateShaderModule",
			"debug overlay shader module creation returned null");
		DestroyDebugOverlayShaderModules(context, outModules);
		return false;
	}
	return true;
}

// Selection/placement wireframe boxes render into the same offscreen HDR scene-color target and
// depth format as voxel geometry (before AA resolve), so they get the same MSAA/SMAA/progressive/
// SSAA treatment as the rest of the scene instead of being hard-edged swapchain-space lines.
bool CreateDebugSelectionPipeline(
	VulkanContextState &context,
	RenderState &render,
	const VkShaderModule vertexShaderModule,
	const VkShaderModule fragmentShaderModule)
{
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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

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

	const VkPipelineMultisampleStateCreateInfo multisampling{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.rasterizationSamples = projectv::render::ToVkSampleCount(render.msaaSampleCount),
		.sampleShadingEnable = VK_FALSE,
		.minSampleShading = 0.0f,
		.pSampleMask = nullptr,
		.alphaToCoverageEnable = VK_FALSE,
		.alphaToOneEnable = VK_FALSE,
	};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &kAlphaBlendAttachmentState;

	constexpr VkFormat kSceneColorAttachmentFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32; // must mirror CreateGraphicsPipeline's mainColorAttachmentFormat
	const VkFormat sceneDepthAttachmentFormat = ChooseDepthFormat(context.physicalDevice);
	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &kSceneColorAttachmentFormat,
		.depthAttachmentFormat = sceneDepthAttachmentFormat,
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
	pipelineInfo.layout = render.debugOverlayPipelineLayout;

	const VkResult result = vkCreateGraphicsPipelines(
		context.device,
		context.pipelineCache,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugOverlayPipeline);
	if (result != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugOverlayPipeline.vkCreateGraphicsPipelines.Selection", result);
		return false;
	}

	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugOverlayPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugOverlayPipeline");
	return true;
}

// Crosshair stays a screen-space swapchain-resolution overlay drawn post-blit in the UI pass; it
// has no world-space geometry to alias, so it does not need the voxel scene's AA stack.
bool CreateDebugCrosshairPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render,
	const VkShaderModule vertexShaderModule,
	const VkShaderModule fragmentShaderModule)
{
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

	static constexpr VkPipelineMultisampleStateCreateInfo multisampling{
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
	depthStencil.depthTestEnable = VK_FALSE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

	constexpr VkPipelineColorBlendAttachmentState kCrosshairLogicOpAttachmentState{
		.blendEnable = VK_FALSE,
		.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
		.colorBlendOp = VK_BLEND_OP_ADD,
		.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
		.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
		.alphaBlendOp = VK_BLEND_OP_ADD,
		.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT |
			VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT,
	};
	VkPhysicalDeviceFeatures supportedFeatures{};
	vkGetPhysicalDeviceFeatures(context.physicalDevice, &supportedFeatures);
	const bool useLogicOpCrosshair = supportedFeatures.logicOp == VK_TRUE;
	VkPipelineColorBlendStateCreateInfo crosshairColorBlending{};
	crosshairColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	crosshairColorBlending.attachmentCount = 1;
	crosshairColorBlending.logicOpEnable = useLogicOpCrosshair ? VK_TRUE : VK_FALSE;
	crosshairColorBlending.logicOp = VK_LOGIC_OP_XOR;
	crosshairColorBlending.pAttachments =
		useLogicOpCrosshair ? &kCrosshairLogicOpAttachmentState : &kAlphaBlendAttachmentState;

	const VkFormat overlayColorFormat = swapchain.format;
	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &overlayColorFormat,
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
	pipelineInfo.pColorBlendState = &crosshairColorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = render.debugOverlayPipelineLayout;

	const VkResult result = vkCreateGraphicsPipelines(
		context.device,
		context.pipelineCache,
		1,
		&pipelineInfo,
		nullptr,
		&render.debugCrosshairPipeline);
	if (result != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugOverlayPipeline.vkCreateGraphicsPipelines.Crosshair", result);
		return false;
	}

	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugCrosshairPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"DebugCrosshairPipeline");
	return true;
}

} // namespace

bool CreateDebugOverlayPipeline(
	VulkanContextState &context,
	const SwapchainState &swapchain,
	RenderState &render)
{
	DebugOverlayShaderModules modules{};
	if (!LoadDebugOverlayShaderModules(context, modules)) {
		return false;
	}

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(DebugOverlayPushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
	const VkResult debugOverlayLayoutResult =
		vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &render.debugOverlayPipelineLayout);
	if (debugOverlayLayoutResult != VK_SUCCESS) {
		LogGraphicsPipelineVkFailure("CreateDebugOverlayPipeline.vkCreatePipelineLayout", debugOverlayLayoutResult);
		DestroyDebugOverlayShaderModules(context, modules);
		return false;
	}

	if (!CreateDebugSelectionPipeline(context, render, modules.vertexShaderModule, modules.fragmentShaderModule) ||
		!CreateDebugCrosshairPipeline(context, swapchain, render, modules.vertexShaderModule, modules.fragmentShaderModule)) {
		DestroyDebugOverlayShaderModules(context, modules);
		DestroyDebugOverlayPipeline(context, render);
		return false;
	}

	DestroyDebugOverlayShaderModules(context, modules);
	SetVulkanObjectName(
		context,
		reinterpret_cast<uint64_t>(render.debugOverlayPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"DebugOverlayPipelineLayout");
	return true;
}

bool RecreateDebugSelectionPipeline(
	VulkanContextState &context,
	RenderState &render)
{
	if (render.debugOverlayPipelineLayout == VK_NULL_HANDLE) {
		return false;
	}

	if (render.debugOverlayPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context.device, render.debugOverlayPipeline, nullptr);
		render.debugOverlayPipeline = VK_NULL_HANDLE;
	}

	DebugOverlayShaderModules modules{};
	if (!LoadDebugOverlayShaderModules(context, modules)) {
		return false;
	}
	const bool created =
		CreateDebugSelectionPipeline(context, render, modules.vertexShaderModule, modules.fragmentShaderModule);
	DestroyDebugOverlayShaderModules(context, modules);
	return created;
}
