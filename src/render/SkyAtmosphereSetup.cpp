#include "render/SkyAtmosphere.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/SceneResources.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <cstddef>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>

namespace {
constexpr char kSkyAtmosphereVertexShaderFilename[] = "sky_atmosphere.vert.spv";
constexpr char kSkyAtmosphereFragmentShaderFilename[] = "sky_atmosphere.frag.spv";

constexpr VkDescriptorSetLayoutCreateInfo kSkyAtmosphereDescriptorSetLayoutInfo{
	.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	.pNext = nullptr,
	.flags = 0,
	.bindingCount = 2,
	.pBindings = nullptr,
};

constexpr std::array kSkyAtmosphereDescriptorPoolSizes{
	VkDescriptorPoolSize{
		.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = MAX_FRAMES_IN_FLIGHT * 2u,
	},
};

} // namespace

namespace projectv::render {
void DestroySkyAtmospherePipelines(VulkanContextState *context, RenderState *render)
{
	if (context == nullptr || render == nullptr || context->device == VK_NULL_HANDLE) {
		return;
	}
	if (render->skyAtmospherePipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(context->device, render->skyAtmospherePipeline, nullptr);
		render->skyAtmospherePipeline = VK_NULL_HANDLE;
	}
	if (render->skyAtmospherePipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(context->device, render->skyAtmospherePipelineLayout, nullptr);
		render->skyAtmospherePipelineLayout = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereVertexShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->skyAtmosphereVertexShaderModule, nullptr);
		render->skyAtmosphereVertexShaderModule = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereFragmentShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, render->skyAtmosphereFragmentShaderModule, nullptr);
		render->skyAtmosphereFragmentShaderModule = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context->device, render->skyAtmosphereDescriptorPool, nullptr);
		render->skyAtmosphereDescriptorPool = VK_NULL_HANDLE;
	}
	if (render->skyAtmosphereDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(context->device, render->skyAtmosphereDescriptorSetLayout, nullptr);
		render->skyAtmosphereDescriptorSetLayout = VK_NULL_HANDLE;
	}
	render->skyAtmospherePipelineEnabled = false;
}

bool CreateSkyAtmospherePipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateSkyAtmospherePipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device && context->allocator,
		"Render", "CreateSkyAtmospherePipelines.Preconditions", "missing context");

	bool ok = true;
	if (!IsSkyAtmosphereEnabled()) {
		ok = false;
	} else {
		DestroySkyAtmospherePipelines(context, render);

		const std::vector<char> vertexShaderCode = ReadShaderFile(kSkyAtmosphereVertexShaderFilename);
		if (vertexShaderCode.empty()) {
			runtime::LogRuntimeFailure(
				"Render", "CreateSkyAtmospherePipelines.ReadVertexShader", "sky_atmosphere.vert.spv not found");
			ok = false;
		}

		const std::vector<char> fragmentShaderCode = ReadShaderFile(kSkyAtmosphereFragmentShaderFilename);
		if (fragmentShaderCode.empty()) {
			runtime::LogRuntimeFailure(
				"Render", "CreateSkyAtmospherePipelines.ReadFragmentShader", "sky_atmosphere.frag.spv not found");
			ok = false;
		}

		if (ok) {
			VkShaderModuleCreateInfo vertexModuleInfo{};
			vertexModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			vertexModuleInfo.codeSize = vertexShaderCode.size();
			vertexModuleInfo.pCode = reinterpret_cast<const uint32_t *>(vertexShaderCode.data());
			if (vkCreateShaderModule(context->device, &vertexModuleInfo, nullptr, &render->skyAtmosphereVertexShaderModule) != VK_SUCCESS) {
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereVertexShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "SkyAtmosphereVertexShader");
			}
		}

		if (ok) {
			VkShaderModuleCreateInfo fragmentModuleInfo{};
			fragmentModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			fragmentModuleInfo.codeSize = fragmentShaderCode.size();
			fragmentModuleInfo.pCode = reinterpret_cast<const uint32_t *>(fragmentShaderCode.data());
			if (vkCreateShaderModule(context->device, &fragmentModuleInfo, nullptr, &render->skyAtmosphereFragmentShaderModule) != VK_SUCCESS) {
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereFragmentShaderModule), VK_OBJECT_TYPE_SHADER_MODULE, "SkyAtmosphereFragmentShader");
			}
		}

		if (ok) {
			const VkResult layoutResult = vkCreateDescriptorSetLayout(
				context->device,
				&kSkyAtmosphereDescriptorSetLayoutInfo,
				nullptr,
				&render->skyAtmosphereDescriptorSetLayout);
			if (layoutResult != VK_SUCCESS) {
				runtime::LogVkFailure("CreateSkyAtmospherePipelines.vkCreateDescriptorSetLayout", layoutResult);
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereDescriptorSetLayout), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "SkyAtmosphereDescriptorSetLayout");
			}
		}

		if (ok) {
			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
			poolInfo.poolSizeCount = static_cast<uint32_t>(kSkyAtmosphereDescriptorPoolSizes.size());
			poolInfo.pPoolSizes = kSkyAtmosphereDescriptorPoolSizes.data();
			if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &render->skyAtmosphereDescriptorPool) != VK_SUCCESS) {
				runtime::LogVkFailure("CreateSkyAtmospherePipelines.vkCreateDescriptorPool", VK_ERROR_OUT_OF_DEVICE_MEMORY);
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmosphereDescriptorPool), VK_OBJECT_TYPE_DESCRIPTOR_POOL, "SkyAtmosphereDescriptorPool");
			}
		}

		std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> descriptorSets{};
		if (ok) {
			std::array<VkDescriptorSetLayout, MAX_FRAMES_IN_FLIGHT> layouts{};
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
				layouts[i] = render->skyAtmosphereDescriptorSetLayout;
			}
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = render->skyAtmosphereDescriptorPool;
			allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
			allocInfo.pSetLayouts = layouts.data();
			if (vkAllocateDescriptorSets(context->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
				ok = false;
			}
		}

		if (ok) {
			for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
				const bool useLut = render->skyLutPrecomputeEnabled && render->skyViewLutView != VK_NULL_HANDLE;
				VkDescriptorImageInfo skyViewInfo{};
				skyViewInfo.sampler = render->skyLutLinearSampler;
				skyViewInfo.imageView = useLut ? render->skyViewLutView : VK_NULL_HANDLE;
				skyViewInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				VkDescriptorImageInfo multiScatteringInfo{};
				multiScatteringInfo.sampler = render->skyLutLinearSampler;
				multiScatteringInfo.imageView = useLut ? render->multiScatteringLutView : VK_NULL_HANDLE;
				multiScatteringInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				std::array<VkWriteDescriptorSet, 2> writes{};
				writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[0].dstSet = descriptorSets[i];
				writes[0].dstBinding = 0;
				writes[0].dstArrayElement = 0;
				writes[0].descriptorCount = 1;
				writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writes[0].pImageInfo = &skyViewInfo;

				writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].dstSet = descriptorSets[i];
				writes[1].dstBinding = 1;
				writes[1].dstArrayElement = 0;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writes[1].pImageInfo = &multiScatteringInfo;

				vkUpdateDescriptorSets(context->device, static_cast<uint32_t>(writes.size()), writes.data(), 0u, nullptr);
			}
		}

		if (ok) {
			VkPipelineLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layoutInfo.setLayoutCount = 1;
			layoutInfo.pSetLayouts = &render->skyAtmosphereDescriptorSetLayout;
			layoutInfo.pushConstantRangeCount = 1;
			VkPushConstantRange pushConstantRange{};
			pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
			pushConstantRange.offset = 0;
			pushConstantRange.size = sizeof(SkyAtmospherePushConstants);
			layoutInfo.pPushConstantRanges = &pushConstantRange;
			if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &render->skyAtmospherePipelineLayout) != VK_SUCCESS) {
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmospherePipelineLayout), VK_OBJECT_TYPE_PIPELINE_LAYOUT, "SkyAtmospherePipelineLayout");
			}
		}

		if (ok) {
			const VkPipelineShaderStageCreateInfo vertexStage{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = render->skyAtmosphereVertexShaderModule,
				.pName = "main",
			};
			const VkPipelineShaderStageCreateInfo fragmentStage{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = render->skyAtmosphereFragmentShaderModule,
				.pName = "main",
			};

			static constexpr VkPipelineVertexInputStateCreateInfo vertexInputState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.vertexBindingDescriptionCount = 0,
				.pVertexBindingDescriptions = nullptr,
				.vertexAttributeDescriptionCount = 0,
				.pVertexAttributeDescriptions = nullptr,
			};

			static constexpr VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				.primitiveRestartEnable = VK_FALSE,
			};

			static constexpr VkPipelineViewportStateCreateInfo viewportState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount = 1,
				.pViewports = nullptr,
				.scissorCount = 1,
				.pScissors = nullptr,
			};

			static constexpr VkPipelineRasterizationStateCreateInfo rasterizationState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_NONE,
				.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
				.lineWidth = 1.0f,
			};

			VkPipelineMultisampleStateCreateInfo multisampleState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.rasterizationSamples = projectv::render::ToVkSampleCount(render->msaaSampleCount),
			};

			static constexpr VkPipelineDepthStencilStateCreateInfo depthStencilState{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
				.depthTestEnable = VK_TRUE,
				.depthWriteEnable = VK_TRUE,
				.depthCompareOp = VK_COMPARE_OP_ALWAYS,
				.depthBoundsTestEnable = VK_FALSE,
				.stencilTestEnable = VK_FALSE,
			};

			static constexpr VkPipelineColorBlendAttachmentState colorBlendAttachment{
				.blendEnable = VK_FALSE,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT,
			};

			constexpr VkPipelineColorBlendStateCreateInfo colorBlendState{
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
			pipelineInfo.layout = render->skyAtmospherePipelineLayout;
			pipelineInfo.subpass = 0;

			if (vkCreateGraphicsPipelines(context->device, context->pipelineCache, 1u, &pipelineInfo, nullptr, &render->skyAtmospherePipeline) != VK_SUCCESS) {
				ok = false;
			} else {
				SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->skyAtmospherePipeline), VK_OBJECT_TYPE_PIPELINE, "SkyAtmospherePipeline");
			}
		}

		if (ok) {
			render->skyAtmospherePipelineEnabled = true;
		}
	}

	if (!ok) {
		DestroySkyAtmospherePipelines(context, render);
	}
	return ok;
}
} // namespace projectv::render
