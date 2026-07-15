#include "render/vulkan/VulkanMeshShaderPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/EnvUtils.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "debug/Profiling.hpp"
#include "render/AntialiasingSettings.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>
#include <vector>

#include "SDL3/SDL_log.h"

namespace {
constexpr uint32_t kMeshMaxOutputVertices = 256u;
constexpr uint32_t kMeshMaxOutputPrimitives = 256u;
constexpr uint32_t kMeshPushConstantSize = 128u;
constexpr char kMeshClusterizeShaderFilename[] = "voxel_face_cluster.comp.spv";
constexpr char kMeshCullShaderFilename[] = "voxel_mesh_pre.comp.spv";
constexpr char kMeshShaderFilename[] = "voxel_mesh.mesh.spv";
constexpr char kVoxelFragmentShaderFilename[] = "voxel.frag.rtx.spv";

constexpr std::array kMeshVisibilityBindings{
	VkDescriptorSetLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_MESH_BIT_EXT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = nullptr},
};

constexpr std::array kMeshClusterizeBindings{
	VkDescriptorSetLayoutBinding{.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = nullptr},
	VkDescriptorSetLayoutBinding{.binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .pImmutableSamplers = nullptr},
};

VkShaderModule CreateMeshShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule shaderModule = VK_NULL_HANDLE;
	const VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure("CreateMeshShaderModule", result);
		return VK_NULL_HANDLE;
	}
	return shaderModule;
}

bool CreateComputePipeline(
	VulkanContextState &context,
	VkShaderModule module,
	VkPipelineLayout layout,
	VkPipeline *outPipeline,
	const char *name)
{
	const VkPipelineShaderStageCreateInfo stage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = module,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};
	VkComputePipelineCreateInfo info{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = stage};
	info.layout = layout;
	const VkResult result = vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1u, &info, nullptr, outPipeline);
	if (result != VK_SUCCESS) {
		runtime::LogVkFailure(name, result);
		return false;
	}
	SetVulkanObjectName(context, reinterpret_cast<uint64_t>(*outPipeline), VK_OBJECT_TYPE_PIPELINE, name);
	return true;
}
} // namespace

namespace projectv::render {
bool IsMeshShaderPipelineRequested()
{
	const char *value = core::GetEnvVar("PROJECTV_MESH_SHADER_PIPELINE");
	if (value == nullptr) {
		return false;
	}
	return value[0] != '\0' && value[0] != '0';
}

bool CreateMeshShaderPipelines(VulkanContextState *context, RenderState *render)
{
	PV_PROFILE_ZONE_N("CreateMeshShaderPipelines");
	PV_CHECK_OR_RETURN(
		context && render && context->device,
		"Render",
		"CreateMeshShaderPipelines.Preconditions",
		"context/render/device is incomplete");

	bool ok = true;
	bool creationAttempted = false;
	if (!IsMeshShaderPipelineRequested()) {
		return false;
	}

	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	meshShaderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &meshShaderFeatures;
	vkGetPhysicalDeviceFeatures2(context->physicalDevice, &deviceFeatures2);
	if (meshShaderFeatures.meshShader != VK_TRUE) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "PROJECTV_MESH_SHADER_PIPELINE=ON but device lacks meshShader; disabled");
		return false;
	}
	if (render->graphicsDescriptorSetLayout == VK_NULL_HANDLE) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "mesh shader path needs graphicsDescriptorSetLayout first; disabled");
		return false;
	}

	VkPhysicalDeviceMeshShaderPropertiesEXT meshProperties{};
	meshProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;
	VkPhysicalDeviceProperties2 deviceProperties2{};
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &meshProperties;
	vkGetPhysicalDeviceProperties2(context->physicalDevice, &deviceProperties2);
	if (meshProperties.maxMeshOutputVertices < 256u || meshProperties.maxMeshOutputPrimitives < 256u) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "mesh shader output limits too low; disabled");
		return false;
	}
	render->meshShaderMaxOutputVertices = std::min(meshProperties.maxMeshOutputVertices, kMeshMaxOutputVertices);
	render->meshShaderMaxOutputPrimitives = std::min(meshProperties.maxMeshOutputPrimitives, kMeshMaxOutputPrimitives);

	DestroyMeshShaderPipelines(context, render);
	creationAttempted = true;
	render->meshShaderIndirectEnabled = IsMeshShaderIndirectRequested();

	const std::vector<char> clusterizeCode = ReadShaderFile(kMeshClusterizeShaderFilename);
	const std::vector<char> cullCode = ReadShaderFile(kMeshCullShaderFilename);
	const std::vector<char> meshCode = ReadShaderFile(kMeshShaderFilename);
	const std::vector<char> fragmentCode = ReadShaderFile(kVoxelFragmentShaderFilename);
	if (clusterizeCode.empty() || cullCode.empty() || meshCode.empty() || fragmentCode.empty()) {
		runtime::LogRuntimeFailure("Render", "CreateMeshShaderPipelines.ReadShaderFile", "missing mesh SPIR-V");
		ok = false;
	}

	if (ok) {
		render->meshClusterizeShaderModule = CreateMeshShaderModule(context->device, clusterizeCode);
		render->meshCullShaderModule = CreateMeshShaderModule(context->device, cullCode);
		render->meshShaderModule = CreateMeshShaderModule(context->device, meshCode);
		ok = render->meshClusterizeShaderModule && render->meshCullShaderModule && render->meshShaderModule;
	}

	if (ok) {
		const VkDescriptorSetLayoutCreateInfo visibilityLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = static_cast<uint32_t>(kMeshVisibilityBindings.size()),
			.pBindings = kMeshVisibilityBindings.data(),
		};
		ok = vkCreateDescriptorSetLayout(context->device, &visibilityLayoutInfo, nullptr, &render->meshShaderDescriptorSetLayout) == VK_SUCCESS;
		const VkDescriptorSetLayoutCreateInfo clusterizeLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.bindingCount = static_cast<uint32_t>(kMeshClusterizeBindings.size()),
			.pBindings = kMeshClusterizeBindings.data(),
		};
		ok = ok && vkCreateDescriptorSetLayout(context->device, &clusterizeLayoutInfo, nullptr, &render->meshClusterizeDescriptorSetLayout) == VK_SUCCESS;
	}

	VkPushConstantRange meshGraphicsPush{};
	meshGraphicsPush.stageFlags = VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_FRAGMENT_BIT;
	meshGraphicsPush.offset = 0;
	meshGraphicsPush.size = kMeshPushConstantSize;

	VkPushConstantRange meshCullPush{};
	meshCullPush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	meshCullPush.offset = 0;
	meshCullPush.size = kMeshPushConstantSize;

	VkPushConstantRange clusterizePush{};
	clusterizePush.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	clusterizePush.offset = 0;
	clusterizePush.size = sizeof(MeshClusterizePushConstants);

	if (ok) {
		const VkDescriptorSetLayout meshSets[2]{
			render->graphicsDescriptorSetLayout,
			render->meshShaderDescriptorSetLayout,
		};
		VkPipelineLayoutCreateInfo meshLayoutInfo{};
		meshLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		meshLayoutInfo.setLayoutCount = 2u;
		meshLayoutInfo.pSetLayouts = meshSets;
		meshLayoutInfo.pushConstantRangeCount = 1u;
		meshLayoutInfo.pPushConstantRanges = &meshGraphicsPush;
		ok = vkCreatePipelineLayout(context->device, &meshLayoutInfo, nullptr, &render->meshShaderPipelineLayout) == VK_SUCCESS;

		VkPipelineLayoutCreateInfo cullLayoutInfo{};
		cullLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		cullLayoutInfo.setLayoutCount = 1u;
		cullLayoutInfo.pSetLayouts = &render->meshShaderDescriptorSetLayout;
		cullLayoutInfo.pushConstantRangeCount = 1u;
		cullLayoutInfo.pPushConstantRanges = &meshCullPush;
		ok = ok && vkCreatePipelineLayout(context->device, &cullLayoutInfo, nullptr, &render->meshCullPipelineLayout) == VK_SUCCESS;

		VkPipelineLayoutCreateInfo clusterizeLayoutInfo{};
		clusterizeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		clusterizeLayoutInfo.setLayoutCount = 1u;
		clusterizeLayoutInfo.pSetLayouts = &render->meshClusterizeDescriptorSetLayout;
		clusterizeLayoutInfo.pushConstantRangeCount = 1u;
		clusterizeLayoutInfo.pPushConstantRanges = &clusterizePush;
		ok = ok && vkCreatePipelineLayout(context->device, &clusterizeLayoutInfo, nullptr, &render->meshClusterizePipelineLayout) == VK_SUCCESS;
	}

	if (ok) {
		ok = CreateComputePipeline(*context, render->meshClusterizeShaderModule, render->meshClusterizePipelineLayout, &render->meshClusterizePipeline, "MeshClusterizePipeline");
		ok = ok && CreateComputePipeline(*context, render->meshCullShaderModule, render->meshCullPipelineLayout, &render->meshCullPipeline, "MeshCullPipeline");
	}

	VkShaderModule fragmentModule = VK_NULL_HANDLE;
	if (ok) {
		fragmentModule = CreateMeshShaderModule(context->device, fragmentCode);
		ok = fragmentModule != VK_NULL_HANDLE;
	}

	if (ok) {
		const VkPipelineShaderStageCreateInfo meshStages[]{
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MESH_BIT_EXT, .module = render->meshShaderModule, .pName = "main"},
			{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = fragmentModule, .pName = "main"},
		};

		VkPipelineVertexInputStateCreateInfo vertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
		VkPipelineViewportStateCreateInfo viewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1u, .scissorCount = 1u};
		VkPipelineRasterizationStateCreateInfo rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = VK_CULL_MODE_NONE, .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, .lineWidth = 1.0f};
		VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = projectv::render::ToVkSampleCount(render->msaaSampleCount)};
		VkPipelineDepthStencilStateCreateInfo depthStencil{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = VK_TRUE, .depthWriteEnable = VK_TRUE, .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL};
		VkPipelineColorBlendAttachmentState colorBlendAttachment{.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
		VkPipelineColorBlendStateCreateInfo colorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = 1u, .pAttachments = &colorBlendAttachment};
		const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamicState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = 2u, .pDynamicStates = dynamicStates};
		static constexpr VkFormat colorFormats[1]{VK_FORMAT_B10G11R11_UFLOAT_PACK32};
		VkPipelineRenderingCreateInfo renderingCreateInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = 1u, .pColorAttachmentFormats = colorFormats, .depthAttachmentFormat = VK_FORMAT_D32_SFLOAT};
		VkGraphicsPipelineCreateInfo meshPipelineInfo{};
		meshPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		meshPipelineInfo.pNext = &renderingCreateInfo;
		meshPipelineInfo.stageCount = 2u;
		meshPipelineInfo.pStages = meshStages;
		meshPipelineInfo.pVertexInputState = &vertexInput;
		meshPipelineInfo.pInputAssemblyState = &inputAssembly;
		meshPipelineInfo.pViewportState = &viewportState;
		meshPipelineInfo.pRasterizationState = &rasterization;
		meshPipelineInfo.pMultisampleState = &multisample;
		meshPipelineInfo.pDepthStencilState = &depthStencil;
		meshPipelineInfo.pColorBlendState = &colorBlend;
		meshPipelineInfo.pDynamicState = &dynamicState;
		meshPipelineInfo.layout = render->meshShaderPipelineLayout;
		ok = vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1u, &meshPipelineInfo, nullptr, &render->meshShaderPipeline) == VK_SUCCESS;
		if (ok) {
			SetVulkanObjectName(*context, reinterpret_cast<uint64_t>(render->meshShaderPipeline), VK_OBJECT_TYPE_PIPELINE, "MeshShaderPipeline");
		}
	}

	if (fragmentModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context->device, fragmentModule, nullptr);
	}

	if (ok) {
		render->meshShaderEnabled = true;
		ok = RefreshMeshShaderResourceBindings(context, render);
	}
	if (!ok && creationAttempted) {
		DestroyMeshShaderPipelines(context, render);
	}
	return ok;
}
} // namespace projectv::render
