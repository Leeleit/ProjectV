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

// The model pass reuses the same 128-byte push constant range as
// the main graphics pipeline (`GraphicsPushConstants` in
// `core/Types.hpp`): `viewProjection` at offset 0, four other
// vec4 fields in the middle (cameraPosition/cameraForward/
// worldMinAndChunkSize/chunkGridAndFlags), and our
// `modelTransform` packed into the trailing 64 bytes. The shader
// only reads the first 64 bytes and the last 64 bytes, so the
// middle 64 bytes are not meaningful for the model pass — we just
// don't write them.
struct ModelPushConstants {
	// Fields are not accessed from C++ — they exist solely to give
	// the struct a known size for the SPIR-V `PushConstant` block.
	// The shader reads the first 64 bytes (viewProjection) and the
	// last 64 bytes (modelTransform) but C++ never touches them.
	// `[[maybe_unused]]` makes the field-use intent obvious to the
	// compiler (suppresses -Wunused-private-field) and to IDE DFA
	// analyses that flag "field is never used".
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

	VkPipelineMultisampleStateCreateInfo multisampling{
		// Same Windows clang-cl designated-init rationale as
		// above. `.sampleShadingEnable` is `VK_FALSE` here
		// because the model pass is single-sample (1x MSAA),
		// so `.minSampleShading` is ignored by the driver
		// (set to `0.0f` for completeness — Vulkan says it
		// is only meaningful when `.sampleShadingEnable` is
		// `VK_TRUE`). `.pSampleMask` is `nullptr` and
		// `.alphaToCoverageEnable` / `.alphaToOneEnable`
		// are `VK_FALSE` for the same reason — single-sample
		// MSAA does not need a per-sample coverage mask or
		// alpha-to-coverage / alpha-to-one conversion.
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
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	// M5.2 follow-up fix: the dual-MRT rendering info above
	// (2 attachments) requires a matching `attachmentCount` on
	// `VkPipelineColorBlendStateCreateInfo` per Vulkan spec
	// VUID-VkGraphicsPipelineCreateInfo-renderPass-06055. Both
	// blend attachments are configured the same way (no blend,
	// all color channels written) because the model pass uses
	// opaque writes — slot 0 (swapchain / TAA-off `outColor`) and
	// slot 1 (TAA scene color / TAA-on `outSceneColor`) only ever
	// receive one of the two at runtime, with the other slot
	// bound as `VK_NULL_HANDLE` via the per-frame
	// `VkRenderingAttachmentInfo::imageView`. The first attachment's
	// state is the existing `colorBlendAttachment`; the second is
	// an identical copy.
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	const std::array colorBlendAttachments{
		colorBlendAttachment,
		colorBlendAttachment,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 2;
	colorBlending.pAttachments = colorBlendAttachments.data();

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

	// The model pass reuses the SAME `VkPipelineLayout` as the main
	// graphics pipeline (`render->graphicsPipelineLayout`). This
	// keeps the `graphicsDescriptorSet` bound across the opaque,
	// model, and transparent passes (the validation layer otherwise
	// flags `set 0 is not compatible with the VkPipelineLayout` on
	// every `vkCmdDrawIndexed` after the pipeline switch). The shared
	// layout already declares the push constant range we need
	// (`size = 128`, `offset = 0`, `VERTEX|FRAGMENT` stages), so
	// `model.vert` reads `viewProjection` (offset 0) and
	// `modelTransform` (offset 64) the same way the voxel pass
	// reads `GraphicsPushConstants`.

	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	// M5.2 follow-up fix: model pipeline uses the *same* dual-MRT
	// attachment layout as the main graphics pipeline
	// (`VulkanGraphicsPipeline.cpp:1792-1802`). Slot 0 is the swapchain
	// (TAA-off output of `voxel.frag` / `model.frag`), slot 1 is the
	// TAA-on output (`outSceneColor` at Location 1 in
	// `voxel.frag:78` / `model.frag:33`). The previous single-attachment
	// declaration silently dropped the Location 1 write when the
	// model pipeline ran inside the main pass's
	// `vkCmdBeginRendering(...)` (which has 2 attachments), so with TAA
	// on the model color never reached `taaSceneColorTarget` and the
	// resolve pass sampled an empty image at model pixels — model
	// appeared invisible despite the M5.2 color-distance rejection
	// being correctly raised. `VK_KHR_dynamic_rendering_unused_
	// attachments` allows the rendering to have more attachments
	// than the pipeline declares, but NOT the inverse — the
	// pipeline must declare every format it writes to.
	const VkFormat modelColorAttachmentFormats[2] = {
		colorFormat,
		taa::kTaaSceneColorFormat,
	};
	renderingInfo.colorAttachmentCount = 2;
	renderingInfo.pColorAttachmentFormats = modelColorAttachmentFormats;
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
	// The model pipeline reuses the main `graphicsPipelineLayout`; no
	// separate layout to clean up here. `render->modelPipelineLayout`
	// is intentionally left as `VK_NULL_HANDLE` so the destructor
	// short-circuits the layout-free call below.
	render->modelPipelineLayout = VK_NULL_HANDLE;

	// TAA-on variant: the only difference is the fragment shader
	// module. The shader contract difference is purely "writes to
	// outSceneColor (Location 1) instead of outColor (Location 0)"
	// so the model pass lands on the same color attachment the TAA
	// resolve pass then samples. The rest of the pipeline is
	// identical.
	std::vector<char> fragmentShaderCodeTaaOn = ReadShaderFile("model.frag.taa_on.spv");
	VkShaderModule fragmentModuleTaaOn = CreateModelShaderModule(
		context->device,
		"ModelPass.model.frag.taa_on",
		fragmentShaderCodeTaaOn);
	if (fragmentModuleTaaOn == VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, vertexModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		runtime::LogRuntimeFailure(
			"Model",
			"CreateModelPipeline.TaaOnFragmentShader",
			"TAA-on model fragment shader module creation failed");
		return false;
	}
	const std::array stagesTaaOn{
		VkPipelineShaderStageCreateInfo{
			// Same Windows clang-cl pNext / flags /
			// pSpecializationInfo rationale as the first
			// `stages` block above.
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
			.module = fragmentModuleTaaOn,
			.pName = "main",
			.pSpecializationInfo = nullptr,
		},
	};
	VkGraphicsPipelineCreateInfo pipelineInfoTaaOn = pipelineInfo;
	pipelineInfoTaaOn.stageCount = static_cast<uint32_t>(stagesTaaOn.size());
	pipelineInfoTaaOn.pStages = stagesTaaOn.data();
	if (vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			1,
			&pipelineInfoTaaOn,
			nullptr,
			&render->modelPipelineTaaOn) != VK_SUCCESS) {
		vkDestroyShaderModule(context->device, vertexModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
		vkDestroyShaderModule(context->device, fragmentModuleTaaOn, nullptr);
		runtime::LogRuntimeFailure(
			"Model",
			"CreateModelPipeline.vkCreateGraphicsPipelines.TaaOn",
			"TAA-on model pipeline creation failed");
		return false;
	}

	vkDestroyShaderModule(context->device, vertexModule, nullptr);
	vkDestroyShaderModule(context->device, fragmentModule, nullptr);
	vkDestroyShaderModule(context->device, fragmentModuleTaaOn, nullptr);
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
	if (render->modelPipelineTaaOn != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->modelPipelineTaaOn, nullptr);
		render->modelPipelineTaaOn = VK_NULL_HANDLE;
	}
	// The model pipeline reuses the main `graphicsPipelineLayout`; do
	// not destroy it here (DestroyGraphicsPipeline owns its lifetime).
}

VkPipeline PickModelPipeline(const RenderState &render)
{
	if (render.taaEnabled) {
		return render.modelPipelineTaaOn != VK_NULL_HANDLE
				   ? render.modelPipelineTaaOn
				   : render.modelPipeline;
	}
	return render.modelPipeline;
}

} // namespace projectv::asset
