#include "VulkanComputePipeline.hpp"

#include <fstream>
#include <string>
#include <vector>

namespace {
std::vector<char> ReadFileFromPath(const std::string &path)
{
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		return {};
	}

	const std::streamsize fileSize = file.tellg();
	if (fileSize <= 0) {
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(fileSize));
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	return buffer;
}

std::vector<char> ReadFile(const char *filename)
{
	if (std::vector<char> buffer = ReadFileFromPath(filename); !buffer.empty()) {
		return buffer;
	}

	const char *basePath = SDL_GetBasePath();
	if (basePath) {
		std::string path = basePath;
		path += filename;

		if (std::vector<char> buffer = ReadFileFromPath(path); !buffer.empty()) {
			return buffer;
		}
	}

	SDL_Log("Failed to open file: %s", filename);
	return {};
}

VkShaderModule CreateShaderModule(const VkDevice device, const std::vector<char> &code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return VK_NULL_HANDLE;
	}

	return shaderModule;
}
} // namespace

void DestroyComputePipeline(AppState *state)
{
	if (!state->device) {
		return;
	}

	if (state->computePipeline) {
		vkDestroyPipeline(state->device, state->computePipeline, nullptr);
		state->computePipeline = VK_NULL_HANDLE;
	}

	if (state->computePipelineLayout) {
		vkDestroyPipelineLayout(state->device, state->computePipelineLayout, nullptr);
		state->computePipelineLayout = VK_NULL_HANDLE;
	}

	if (state->computeDescriptorPool) {
		vkDestroyDescriptorPool(state->device, state->computeDescriptorPool, nullptr);
		state->computeDescriptorPool = VK_NULL_HANDLE;
	}

	if (state->computeDescriptorSetLayout) {
		vkDestroyDescriptorSetLayout(state->device, state->computeDescriptorSetLayout, nullptr);
		state->computeDescriptorSetLayout = VK_NULL_HANDLE;
	}

	state->computeDescriptorSets.clear();
}

bool CreateComputePipeline(AppState *state)
{
	if (!state || !state->device || state->swapchainImages.empty() || state->swapchainImageViews.empty()) {
		return false;
	}

	const std::vector<char> shaderCode = ReadFile("triangle.comp.spv");
	if (shaderCode.empty()) {
		return false;
	}

	VkShaderModule shaderModule = CreateShaderModule(state->device, shaderCode);
	if (!shaderModule) {
		return false;
	}

	VkDescriptorSetLayoutBinding imageBinding{};
	imageBinding.binding = 0;
	imageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imageBinding.descriptorCount = 1;
	imageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &imageBinding;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	if (vkCreateDescriptorSetLayout(state->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSize.descriptorCount = static_cast<uint32_t>(state->swapchainImageViews.size());

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = static_cast<uint32_t>(state->swapchainImageViews.size());
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(state->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	std::vector layouts(state->swapchainImageViews.size(), descriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(state->swapchainImageViews.size(), VK_NULL_HANDLE);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(state->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		vkDestroyDescriptorPool(state->device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	std::vector<VkDescriptorImageInfo> imageInfos(state->swapchainImageViews.size());
	std::vector<VkWriteDescriptorSet> writes(state->swapchainImageViews.size());
	for (size_t i = 0; i < state->swapchainImageViews.size(); ++i) {
		imageInfos[i].imageView = state->swapchainImageViews[i];
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = descriptorSets[i];
		writes[i].dstBinding = 0;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[i].pImageInfo = &imageInfos[i];
	}
	vkUpdateDescriptorSets(state->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		vkDestroyDescriptorPool(state->device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	VkPipelineShaderStageCreateInfo shaderStage{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = shaderModule,
		.pName = "main",
		.pSpecializationInfo = nullptr,
	};

	VkComputePipelineCreateInfo pipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.stage = shaderStage,
		.layout = pipelineLayout,
		.basePipelineHandle = VK_NULL_HANDLE,
		.basePipelineIndex = 0,
	};

	VkPipeline pipeline = VK_NULL_HANDLE;
	if (vkCreateComputePipelines(state->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
		vkDestroyPipelineLayout(state->device, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(state->device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	vkDestroyShaderModule(state->device, shaderModule, nullptr);

	state->computeDescriptorSetLayout = descriptorSetLayout;
	state->computeDescriptorPool = descriptorPool;
	state->computeDescriptorSets = std::move(descriptorSets);
	state->computePipelineLayout = pipelineLayout;
	state->computePipeline = pipeline;
	return true;
}
