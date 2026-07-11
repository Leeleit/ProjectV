#include "render/vulkan/VulkanGraphicsPipeline.hpp"
#include "render/vulkan/VulkanGraphicsPipelineInternal.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/RayTracedShadows.hpp"
#include "render/RtxGiProbes.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

namespace {
constexpr std::array kGraphicsDescriptorBindings{
	VkDescriptorSetLayoutBinding{
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	VkDescriptorSetLayoutBinding{
		.binding = 5,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 11 = vctClipmap sampler3D FRAGMENT. Per TODO §5.1 (VCT consume in voxel.frag).
	// Always declared even when VCT gate is OFF (env PROJECTV_VCT_GPU=ON default OFF per
	// `agent/knowledge.md` Step 1) — fallback 1x1x1 RGBA16F dummy bound instead.
	// Type = COMBINED_IMAGE_SAMPLER because shader `sampler3D` = OpTypeSampledImage (VUID-layout-07990).
	VkDescriptorSetLayoutBinding{
		.binding = 11,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 12 = volumetricFog sampler3D FRAGMENT. Per TODO §5.4 (volumetric fog consume
	// in voxel.frag via Wronski 2014 froxel grid). Always declared even when fog gate is OFF
	// (env PROJECTV_FOG=ON default OFF per `agent/knowledge.md` Step 1) — fallback
	// 1x1x1 RGBA16F dummy bound instead.
	// Type = COMBINED_IMAGE_SAMPLER because shader `sampler3D` = OpTypeSampledImage (VUID-layout-07990).
	VkDescriptorSetLayoutBinding{
		.binding = 12,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 13 = RTX top-level acceleration structure (TLAS) for ray-query
	// smooth specular GI per Stage 5.2 (TraceRtxSmoothSpecularRay) AND 5.2.B sun shadow
	// ray query (TraceRtxSunShadowRay) AND 5.4 AO (TraceRtxAmbientOcclusionRay). Bound to
	// scene TLAS when RTX env-gate ON; otherwise (non-RTX compile) binding slot is unused.
	// Per docs/VulkanSDK-Linux-Docs-1.4.350.1/chunked_spec/chap63.html the
	// accelerationStructureEXT uniform is GLSL-side; C++ side uses
	// VkAccelerationStructureKHR via VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR.
	VkDescriptorSetLayoutBinding{
		.binding = 13,
		.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 14 = RTX GI probe irradiance 3D texture (8x8x8 probes x 16x16
	// octahedral R11G11B10F = ~256 KiB). Per Stage 5.5 DDGI (Dynamic Diffuse Global
	// Illumination) shader consume via DDGIGetVolumeIrradiance trilinear sample in
	// voxel.frag vctDiffuseIrradiance path. Bound to RtxGiProbes.irradianceImage when
	// RTX env-gate ON; otherwise (non-RTX compile or RTX alloc failed) binding slot
	// is unused. Type = COMBINED_IMAGE_SAMPLER because shader `sampler3D` =
	// OpTypeSampledImage (VUID-layout-07990).
	VkDescriptorSetLayoutBinding{
		.binding = 14,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 15 = RTX GI probe distance 3D texture (8x8x8 probes x 16x16 RG16F).
	// Per Stage 5.5 DDGI visibility back-face check via DDGIGetProbeDistance. Bound
	// to RtxGiProbes.distanceImage when RTX env-gate ON; otherwise binding slot is
	// unused. Type = COMBINED_IMAGE_SAMPLER (sampler3D requires OpTypeSampledImage).
	VkDescriptorSetLayoutBinding{
		.binding = 15,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 16 = RTX GI probe data 2D texture (1x1 RGBA16F fallback; real
	// layout will be uvec4 per-probe state when DDGI probe classification is wired
	// in 5.5+). Bound to RtxGiProbes.probeDataImage when RTX env-gate ON; otherwise
	// binding slot is unused.
	VkDescriptorSetLayoutBinding{
		.binding = 16,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 17 = RTX GI volume descriptor SSBO (VolumeDescGpu, 64 bytes per
	// std430 layout matching GLSL-side struct). Per Stage 5.5 DDGI consume via
	// DDGIGetVolumeIrradiance world position -> probe grid lookup. Bound to
	// RtxGiProbes.volumeDescBuffer when RTX env-gate ON; otherwise binding slot is
	// unused.
	VkDescriptorSetLayoutBinding{
		.binding = 17,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
	// EVIL: binding 18 = voxel-aware RTX shadow mask (R8_UNORM storage image written
	// by voxel_rtx_shadow.rgen via vkCmdTraceRaysKHR, sampled by voxel.frag as a
	// COMBINED_IMAGE_SAMPLER). Per Stage 5.2.E the AABB BLAS + ray-query path is
	// replaced by an RT pipeline + procedural intersection shader that performs DDA
	// over PackedChunkVoxelPayload and writes a per-pixel shadow factor into this
	// mask. Bound to RayTracedShadows.GetShadowMaskImageView() when voxel-aware path
	// is active; otherwise a 1x1 R8 dummy image so the slot stays valid for shaders
	// that compiled with VOXEL_RTX_ENABLED.
	VkDescriptorSetLayoutBinding{
		.binding = 18,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers = nullptr,
	},
};
} // namespace

bool CreateGraphicsPipeline(
	VulkanContextState *context,
	const SwapchainState *swapchain,
	RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateGraphicsPipeline");
	PV_CHECK_OR_RETURN(
		context && swapchain && render && context->device && !swapchain->imageViews.empty(),
		"Graphics",
		"CreateGraphicsPipeline.Preconditions",
		"context/swapchain/render is incomplete");

	std::vector<char> vertexShaderCode;
	std::vector<char> fragmentShaderCode;
	std::vector<char> fragmentShaderCodeRtx;
	const bool rtxProbeAvailable = context->rayTracing.rayQuery
		&& context->rayTracing.accelerationStructure
		&& projectv::render::IsRayTracedShadowEnabled(*context);
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.ReadShaders");
		vertexShaderCode = ReadShaderFile("voxel.vert.spv");
		fragmentShaderCode = ReadShaderFile("voxel.frag.spv");
		if (rtxProbeAvailable) {
			fragmentShaderCodeRtx = ReadShaderFile("voxel.frag.rtx.spv");
		}
	}
	if (vertexShaderCode.empty() || fragmentShaderCode.empty() ||
		(rtxProbeAvailable && fragmentShaderCodeRtx.empty())) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.ReadShaders", "voxel shader blob is empty");
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	VkShaderModule vertexShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModule = VK_NULL_HANDLE;
	VkShaderModule fragmentShaderModuleRtx = VK_NULL_HANDLE;
	const auto destroyShaderModules = [&] {
		if (fragmentShaderModuleRtx) {
			vkDestroyShaderModule(context->device, fragmentShaderModuleRtx, nullptr);
			fragmentShaderModuleRtx = VK_NULL_HANDLE;
		}
		if (fragmentShaderModule) {
			vkDestroyShaderModule(context->device, fragmentShaderModule, nullptr);
			fragmentShaderModule = VK_NULL_HANDLE;
		}
		if (vertexShaderModule) {
			vkDestroyShaderModule(context->device, vertexShaderModule, nullptr);
			vertexShaderModule = VK_NULL_HANDLE;
		}
	};
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.CreateShaderModules");
		vertexShaderModule = CreateShaderModule(context->device, vertexShaderCode);
		fragmentShaderModule = CreateShaderModule(context->device, fragmentShaderCode);
		if (rtxProbeAvailable) {
			fragmentShaderModuleRtx = CreateShaderModule(context->device, fragmentShaderCodeRtx);
		}
	}
	if (!vertexShaderModule || !fragmentShaderModule ||
		(rtxProbeAvailable && !fragmentShaderModuleRtx)) {
		LogGraphicsPipelineTextFailure(
			"CreateGraphicsPipeline.CreateShaderModules",
			"voxel shader module creation returned null");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	const VkPipelineShaderStageCreateInfo vertexStageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_VERTEX_BIT,
		.module = vertexShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const VkPipelineShaderStageCreateInfo fragStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const VkPipelineShaderStageCreateInfo fragStageRtx{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
		.module = fragmentShaderModuleRtx,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	const std::array shaderStages{vertexStageInfo, fragStage};
	const std::array shaderStagesRtx{vertexStageInfo, fragStageRtx};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

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
	VkPipelineRasterizationStateCreateInfo shadowRasterizer = rasterizer;

	shadowRasterizer.cullMode = VK_CULL_MODE_NONE;
	shadowRasterizer.depthBiasEnable = VK_TRUE;
	shadowRasterizer.depthBiasConstantFactor = 1.25f;
	shadowRasterizer.depthBiasSlopeFactor = 1.75f;
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
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachments[1] = {
		colorBlendAttachment,
	};
	VkPipelineColorBlendAttachmentState transparentColorBlendAttachment = kAlphaBlendAttachmentState;
	VkPipelineColorBlendAttachmentState transparentColorBlendAttachments[1] = {
		transparentColorBlendAttachment,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = colorBlendAttachments;

	VkPipelineColorBlendStateCreateInfo transparentColorBlending = colorBlending;
	transparentColorBlending.pAttachments = transparentColorBlendAttachments;
	VkPipelineColorBlendStateCreateInfo shadowColorBlending{};
	shadowColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	shadowColorBlending.attachmentCount = 0;
	shadowColorBlending.pAttachments = nullptr;

	VkPipelineDepthStencilStateCreateInfo transparentDepthStencil = depthStencil;
	transparentDepthStencil.depthWriteEnable = VK_FALSE;
	VkPipelineDepthStencilStateCreateInfo shadowDepthStencil = depthStencil;
	shadowDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(GraphicsPushConstants);

	const bool rtxLayoutActiveForCreate = context->rayTracing.rayQuery
		&& context->rayTracing.accelerationStructure
		&& projectv::render::IsRayTracedShadowEnabled(*context);
	std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
	layoutBindings.reserve(kGraphicsDescriptorBindings.size() + (rtxLayoutActiveForCreate ? 1u : 0u));
	for (const VkDescriptorSetLayoutBinding &b : kGraphicsDescriptorBindings) {
		layoutBindings.push_back(b);
	}
	if (rtxLayoutActiveForCreate) {
		// RTX-only binding 13 (accelerationStructureKHR) is already declared in
		// kGraphicsDescriptorBindings. No additional layout entry needed here.
	}
	const VkDescriptorSetLayoutCreateInfo graphicsDescriptorSetLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(layoutBindings.size()),
		.pBindings = layoutBindings.data(),
	};

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.DescriptorSetLayout");
		const VkResult descriptorSetLayoutResult = vkCreateDescriptorSetLayout(
			context->device,
			&graphicsDescriptorSetLayoutInfo,
			nullptr,
			&render->graphicsDescriptorSetLayout);
		if (descriptorSetLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure(
				"CreateGraphicsPipeline.vkCreateDescriptorSetLayout",
				descriptorSetLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsDescriptorSetLayout),
		VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
		"VoxelGraphicsDescriptorSetLayout");

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &render->graphicsDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.PipelineLayout");
		const VkResult pipelineLayoutResult =
			vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr, &render->graphicsPipelineLayout);
		if (pipelineLayoutResult != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.vkCreatePipelineLayout", pipelineLayoutResult);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsPipelineLayout),
		VK_OBJECT_TYPE_PIPELINE_LAYOUT,
		"VoxelGraphicsPipelineLayout");

	const VkFormat mainColorAttachmentFormat = VK_FORMAT_B10G11R11_UFLOAT_PACK32;
	const VkPipelineRenderingCreateInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &mainColorAttachmentFormat,
		.depthAttachmentFormat = ChooseDepthFormat(context->physicalDevice),
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};
	const VkPipelineRenderingCreateInfo shadowRenderingInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.pNext = nullptr,
		.viewMask = 0,
		.colorAttachmentCount = 0,
		.pColorAttachmentFormats = nullptr,
		.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT,
		.stencilAttachmentFormat = VK_FORMAT_UNDEFINED,
	};

	VkGraphicsPipelineCreateInfo pipelineBase{};
	pipelineBase.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineBase.pNext = &renderingInfo;
	pipelineBase.pVertexInputState = &vertexInputInfo;
	pipelineBase.pInputAssemblyState = &inputAssembly;
	pipelineBase.pViewportState = &viewportState;
	pipelineBase.pRasterizationState = &rasterizer;
	pipelineBase.pMultisampleState = &multisampling;
	pipelineBase.pDepthStencilState = &depthStencil;
	pipelineBase.pColorBlendState = &colorBlending;
	pipelineBase.pDynamicState = &dynamicState;
	pipelineBase.layout = render->graphicsPipelineLayout;

	VkGraphicsPipelineCreateInfo opaqueInfo = pipelineBase;
	opaqueInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	opaqueInfo.pStages = shaderStages.data();
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.OpaquePipeline");
		const VkResult result = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			1,
			&opaqueInfo,
			nullptr,
			&render->graphicsPipeline);
		if (result != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.Opaque.vkCreateGraphicsPipelines", result);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->graphicsPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelOpaquePipeline");

	if (rtxProbeAvailable) {
		VkGraphicsPipelineCreateInfo opaqueInfoRtx = pipelineBase;
		opaqueInfoRtx.stageCount = static_cast<uint32_t>(shaderStagesRtx.size());
		opaqueInfoRtx.pStages = shaderStagesRtx.data();
		{
			PV_PROFILE_ZONE_N("CreateGraphicsPipeline.OpaqueRtxPipeline");
			const VkResult result = vkCreateGraphicsPipelines(
				context->device,
				VK_NULL_HANDLE,
				1,
				&opaqueInfoRtx,
				nullptr,
				&render->graphicsPipelineRtx);
			if (result != VK_SUCCESS) {
				LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.OpaqueRtx.vkCreateGraphicsPipelines", result);
				destroyShaderModules();
				DestroyGraphicsPipeline(context, render);
				return false;
			}
		}
		SetVulkanObjectName(
			*context,
			reinterpret_cast<uint64_t>(render->graphicsPipelineRtx),
			VK_OBJECT_TYPE_PIPELINE,
			"VoxelOpaquePipelineRtx");
	}

	VkGraphicsPipelineCreateInfo transparentInfo = pipelineBase;
	transparentInfo.pDepthStencilState = &transparentDepthStencil;
	transparentInfo.pColorBlendState = &transparentColorBlending;
	transparentInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	transparentInfo.pStages = shaderStages.data();
	{
		PV_PROFILE_ZONE_N("CreateGraphicsPipeline.TransparentPipeline");
		const VkResult result = vkCreateGraphicsPipelines(
			context->device,
			VK_NULL_HANDLE,
			1,
			&transparentInfo,
			nullptr,
			&render->transparentGraphicsPipeline);
		if (result != VK_SUCCESS) {
			LogGraphicsPipelineVkFailure("CreateGraphicsPipeline.Transparent.vkCreateGraphicsPipelines", result);
			destroyShaderModules();
			DestroyGraphicsPipeline(context, render);
			return false;
		}
	}
	SetVulkanObjectName(
		*context,
		reinterpret_cast<uint64_t>(render->transparentGraphicsPipeline),
		VK_OBJECT_TYPE_PIPELINE,
		"VoxelTransparentPipeline");

	VkGraphicsPipelineCreateInfo shadowPipelineInfo = pipelineBase;
	(void)shadowPipelineInfo;
	// CSM shadow pipeline removed per TODO.md §5.2.D (session 20x). The
	// shadow rendering info / rasterizer / depth-stencil / color-blend
	// structs above are retained for ABI compatibility (they are still
	// constructed but not consumed). No shadow pipeline is created.

	// EVIL: RefreshGraphicsResourceBindings deferred to VulkanInit after
	// CreateVolumetricFogFallbackOnly (8x V C bug: bindings 11/12 fallback image
	// did not exist when CreateGraphicsPipeline wrote descriptor sets, producing
	// VK_NULL_HANDLE imageView writes under VUID-VkWriteDescriptorSet-descriptorType-02997).
	// The descriptor pool + layout are created here; the actual descriptor writes
	// are deferred to VulkanInit::CreateRayTracedShadowResources-safe stage.

	if (!CreateDebugOverlayPipeline(*context, *swapchain, *render)) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.DebugOverlay", "debug overlay pipeline creation failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	if (!CreateDebugHudPipeline(*context, *swapchain, *render)) {
		LogGraphicsPipelineTextFailure("CreateGraphicsPipeline.DebugHud", "debug HUD pipeline creation failed");
		destroyShaderModules();
		DestroyGraphicsPipeline(context, render);
		return false;
	}

	destroyShaderModules();
	return true;
}