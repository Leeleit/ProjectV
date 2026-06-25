#include "asset/ModelPass.hpp"

#include <array>
#include <vector>

#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"

namespace projectv::asset {

namespace {

constexpr VkFormat kModelVertexPositionFormat = VK_FORMAT_R32G32B32_SFLOAT;
constexpr VkFormat kModelVertexNormalFormat = VK_FORMAT_R32G32B32_SFLOAT;
constexpr VkFormat kModelVertexUvFormat = VK_FORMAT_R32G32_SFLOAT;

struct ModelPushConstants {

	[[maybe_unused]] std::array<float, 16> viewProjection{};
	[[maybe_unused]] std::array<float, 16> modelTransform{};
};
static_assert(sizeof(ModelPushConstants) == 128);

VkShaderModule CreateModelShaderModule(const VkDevice device, const char *label, const std::vector<char> &code)
{
	if (code.empty()) {
		runtime::LogRuntimeFailure("Model", label, "shader blob is empty");
		return VK_NULL_HANDLE;
	}
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = static_cast<uint32_t>(code.size());
	info.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Model", label, "vkCreateShaderModule returned non-success");
		return VK_NULL_HANDLE;
	}
	return module;
}

} // namespace

bool CreateModelPipeline(
	VulkanContextState *context,
	VkPipelineLayout sharedPipelineLayout,
	VkFormat colorFormat,
	VkFormat depthFormat,
	RenderState *render)
{
	if (!context || !render) {
		return false;
	}
	if (render->modelPipeline != VK_NULL_HANDLE) {
		return true;
	}

	const std::vector<char> vertexShaderCode = ReadShaderFile("model.vert.spv");
	const std::vector<char> fragmentShaderCode = ReadShaderFile("model.frag.spv");
	if (vertexShaderCode.empty() || fragmentShaderCode.empty()) {
		runtime::LogRuntimeFailure("Model", "CreateModelPipeline.ReadShaderFile", "shader blob missing");
		return false;
	}

	VkShaderModule vertexModule = CreateModelShaderModule(context->device, "ModelPass.model.vert", vertexShaderCode);
	VkShaderModule fragmentModule = CreateModelShaderModule(context->device, "ModelPass.model.frag", fragmentShaderCode);
	if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE) {
		if (vertexModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, vertexModule, nullptr);
		}
		if (fragmentModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		}
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

	VkVertexInputBindingDescription vertexBinding{};
	vertexBinding.binding = 0;
	vertexBinding.stride = kBakedVertexStride;
	vertexBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	constexpr std::array vertexAttributes{
		VkVertexInputAttributeDescription{
			.location = 0,
			.binding = 0,
			.format = kModelVertexPositionFormat,
			.offset = 0,
		},
		VkVertexInputAttributeDescription{
			.location = 1,
			.binding = 0,
			.format = kModelVertexNormalFormat,
			.offset = sizeof(float) * 3,
		},
		VkVertexInputAttributeDescription{
			.location = 2,
			.binding = 0,
			.format = kModelVertexUvFormat,
			.offset = sizeof(float) * 6,
		},
	};

	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &vertexBinding;
	vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
	vertexInput.pVertexAttributeDescriptions = vertexAttributes.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

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
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.pNext = nullptr;
	multisampling.flags = 0;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.minSampleShading = 0.0f;
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineDynamicStateCreateInfo dynamicState{};
	constexpr std::array dynamicStates{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(ModelPushConstants);

	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachmentFormats = &colorFormat;
	renderingInfo.depthAttachmentFormat = depthFormat;
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
	pipelineInfo.layout = sharedPipelineLayout;

	if (vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &render->modelPipeline) != VK_SUCCESS) {
		vkDestroyShaderModule(context->device, vertexModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		runtime::LogRuntimeFailure("Model", "CreateModelPipeline.vkCreateGraphicsPipelines", "model pipeline creation failed");
		return false;
	}

	render->modelPipelineLayout = VK_NULL_HANDLE;

	vkDestroyShaderModule(context->device, vertexModule, nullptr);
	vkDestroyShaderModule(context->device, fragmentModule, nullptr);
	return true;
}

void DestroyModelPipeline(VulkanContextState *context, RenderState *render)
{
	if (!context || !render) {
		return;
	}
	if (render->modelPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->modelPipeline, nullptr);
		render->modelPipeline = VK_NULL_HANDLE;
	}
}

VkPipeline PickModelPipeline(const RenderState &render)
{
	return render.modelPipeline;
}

} // namespace projectv::asset
