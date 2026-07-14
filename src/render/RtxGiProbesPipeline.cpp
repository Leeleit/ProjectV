#include "volk.h"
#include "render/RtxGiProbes.hpp"
#include "core/RuntimeDiagnostics.hpp"
#include "core/ShaderIO.hpp"

#include <array>
#include <vector>

namespace projectv::render {

bool RtxGiProbes::CreateComputePipeline(const VulkanContextState &context)
{
	std::array bindings = {
		VkDescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 3,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 4,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 13,
			.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 14,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 15,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr},
		VkDescriptorSetLayoutBinding{
			.binding = 17,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr}};

	const VkDescriptorSetLayoutCreateInfo layoutInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()};

	if (vkCreateDescriptorSetLayout(context.device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.setLayout", "vkCreateDescriptorSetLayout failed");
		return false;
	}

	VkPushConstantRange pushConstantRange{
		.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
		.offset = 0,
		.size = sizeof(GraphicsPushConstants)};

	const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.setLayoutCount = 1,
		.pSetLayouts = &m_descriptorSetLayout,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange};

	if (vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.pipelineLayout", "vkCreatePipelineLayout failed");
		return false;
	}

	const std::vector<char> shaderCode = ReadShaderFile("probe_update.comp.spv");
	if (shaderCode.empty()) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.shaderFile", "probe_update.comp.spv not found");
		return false;
	}

	const VkShaderModuleCreateInfo moduleCreateInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.codeSize = shaderCode.size(),
		.pCode = reinterpret_cast<const uint32_t *>(shaderCode.data())};

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(context.device, &moduleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.shaderModule", "vkCreateShaderModule failed");
		return false;
	}

	const VkComputePipelineCreateInfo computePipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.module = shaderModule,
			.pName = "main",
			.pSpecializationInfo = nullptr},
		.layout = m_pipelineLayout,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0};

	if (vkCreateComputePipelines(context.device, context.pipelineCache, 1, &computePipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.pipeline", "vkCreateComputePipelines failed");
		vkDestroyShaderModule(context.device, shaderModule, nullptr);
		return false;
	}

	vkDestroyShaderModule(context.device, shaderModule, nullptr);

	std::array poolSizes = {
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5 * MAX_FRAMES_IN_FLIGHT},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * MAX_FRAMES_IN_FLIGHT},
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * MAX_FRAMES_IN_FLIGHT}};

	const VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = MAX_FRAMES_IN_FLIGHT,
		.poolSizeCount = 3,
		.pPoolSizes = poolSizes.data()};

	if (vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.descriptorPool", "vkCreateDescriptorPool failed");
		return false;
	}

	std::array layouts = {m_descriptorSetLayout, m_descriptorSetLayout};
	const VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = m_descriptorPool,
		.descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
		.pSetLayouts = layouts.data()};

	if (vkAllocateDescriptorSets(context.device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
		runtime::LogRuntimeFailure("Render", "RtxGiProbes.CreateComputePipeline.allocateSets", "vkAllocateDescriptorSets failed");
		return false;
	}

	return true;
}

void RtxGiProbes::DestroyComputePipeline(const VulkanContextState &context) noexcept
{
	if (context.device == VK_NULL_HANDLE) {
		return;
	}
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
	if (m_descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(context.device, m_descriptorPool, nullptr);
		m_descriptorPool = VK_NULL_HANDLE;
	}
	m_descriptorSets.fill(VK_NULL_HANDLE);
}

} // namespace projectv::render
