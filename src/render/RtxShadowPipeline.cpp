#include "render/RtxShadowPipeline.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <array>
#include <vector>

#include "SDL3/SDL_log.h"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"
#include "fmt/format.h"

namespace projectv::render {

namespace {

constexpr const char *kRtxShadowRayGenShaderFilename = "voxel_rtx_shadow.rgen.spv";
constexpr const char *kRtxShadowIntersectionShaderFilename = "voxel_rtx_shadow.rint.spv";
constexpr const char *kRtxShadowClosestHitShaderFilename = "voxel_rtx_shadow.rchit.spv";
constexpr const char *kRtxShadowMissShaderFilename = "voxel_rtx_shadow.rmiss.spv";

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code, const char *debugName)
{
	VkShaderModuleCreateInfo moduleInfo{};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = code.size();
	moduleInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
	VkShaderModule module = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &moduleInfo, nullptr, &module) != VK_SUCCESS) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxShadowPipeline.CreateShaderModule",
			fmt::format("vkCreateShaderModule failed for {}", debugName));
		return VK_NULL_HANDLE;
	}
	return module;
}

bool LoadShaderModule(
	const VkDevice device,
	const char *filename,
	VkShaderModule *outModule,
	const char *debugName)
{
	const std::vector<char> code = ReadShaderFile(filename);
	if (code.empty()) {
		runtime::LogRuntimeFailure(
			"Render",
			"RtxShadowPipeline.LoadShaderModule",
			fmt::format("{} not found or empty", filename));
		return false;
	}
	*outModule = CreateShaderModule(device, code, debugName);
	return *outModule != VK_NULL_HANDLE;
}

void DestroyShaderModule(const VkDevice device, VkShaderModule &module) noexcept
{
	if (module != VK_NULL_HANDLE) {
		vkDestroyShaderModule(device, module, nullptr);
		module = VK_NULL_HANDLE;
	}
}

}  // namespace



bool RtxShadowPipeline::Initialize(
	const VulkanContextState &context,
	const RtxShadowPipelineConfig &config)
{
	if (m_pipeline != VK_NULL_HANDLE) {
		return true;
	}
	const VkDevice device = context.device;
	if (device == VK_NULL_HANDLE) {
		return false;
	}

	if (!LoadShaderModule(device, kRtxShadowRayGenShaderFilename, &m_rayGenModule, "rgen")) {
		return false;
	}
	if (!LoadShaderModule(device, kRtxShadowIntersectionShaderFilename, &m_intersectionModule, "rint")) {
		Shutdown(context);
		return false;
	}
	if (!LoadShaderModule(device, kRtxShadowClosestHitShaderFilename, &m_closestHitModule, "rchit")) {
		Shutdown(context);
		return false;
	}
	if (!LoadShaderModule(device, kRtxShadowMissShaderFilename, &m_missModule, "rmiss")) {
		Shutdown(context);
		return false;
	}

	std::array<VkDescriptorSetLayoutBinding, 6> bindings{};
	bindings[0].binding = 1;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	bindings[1].binding = 3;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[2].binding = 4;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;

	bindings[3].binding = 13;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[4].binding = 18;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	bindings[5].binding = 19;
	bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[5].descriptorCount = 1;
	bindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
		runtime::LogVkFailure("RtxShadowPipeline.vkCreateDescriptorSetLayout", VK_ERROR_INITIALIZATION_FAILED);
		Shutdown(context);
		return false;
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1u;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		runtime::LogVkFailure("RtxShadowPipeline.vkCreatePipelineLayout", VK_ERROR_INITIALIZATION_FAILED);
		Shutdown(context);
		return false;
	}

	const std::array<VkPipelineShaderStageCreateInfo, 4> stages{{
		{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR, .module = m_rayGenModule, .pName = "main"},
		{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR, .module = m_intersectionModule, .pName = "main"},
		{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, .module = m_closestHitModule, .pName = "main"},
		{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_MISS_BIT_KHR, .module = m_missModule, .pName = "main"},
	}};

	std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> groups{};
	groups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[0].generalShader = 0;
	groups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
	groups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

	groups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	groups[1].generalShader = 3;
	groups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
	groups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

	groups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
	groups[2].generalShader = VK_SHADER_UNUSED_KHR;
	groups[2].closestHitShader = 2;
	groups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
	groups[2].intersectionShader = 1;

	VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
	pipelineInfo.pStages = stages.data();
	pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
	pipelineInfo.pGroups = groups.data();
	pipelineInfo.maxPipelineRayRecursionDepth = 1u;
	pipelineInfo.layout = m_pipelineLayout;

	if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1u, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
		runtime::LogVkFailure("RtxShadowPipeline.vkCreateRayTracingPipelinesKHR", VK_ERROR_INITIALIZATION_FAILED);
		Shutdown(context);
		return false;
	}

	SDL_Log(
		"Render: RtxShadowPipeline: ready (sbtHandleSize=%u sbtBaseAlign=%u sbtHandleAlign=%u)",
		config.shaderGroupHandleSize,
		config.shaderGroupBaseAlignment,
		config.shaderGroupHandleAlignment);
	return true;
}

void RtxShadowPipeline::Shutdown(const VulkanContextState &context) noexcept
{
	if (context.device != VK_NULL_HANDLE) {
		if (m_pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(context.device, m_pipeline, nullptr);
			m_pipeline = VK_NULL_HANDLE;
		}
		if (m_pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(context.device, m_pipelineLayout, nullptr);
			m_pipelineLayout = VK_NULL_HANDLE;
		}
		if (m_descriptorSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(context.device, m_descriptorSetLayout, nullptr);
			m_descriptorSetLayout = VK_NULL_HANDLE;
		}
	}
	DestroyShaderModule(context.device, m_rayGenModule);
	DestroyShaderModule(context.device, m_intersectionModule);
	DestroyShaderModule(context.device, m_closestHitModule);
	DestroyShaderModule(context.device, m_missModule);
}

}  // namespace projectv::render