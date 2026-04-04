#include "VulkanComputePipeline.hpp"

#include <array>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr VkFormat kComputeDepthFormat = VK_FORMAT_R32_SFLOAT;

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

// ReSharper disable once CppDFAConstantParameter
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

// ReSharper disable once CppDFAConstantParameter
bool SupportsStorageImage(const VkPhysicalDevice physicalDevice, const VkFormat format)
{
	VkFormatProperties props{};
	vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
	return (props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
}

bool CreateStorageImage(
	AppState *state,
	const VkExtent2D extent,
	// ReSharper disable once CppDFAConstantParameter
	const VkFormat format,
	VkImage *outImage,
	VmaAllocation *outAllocation,
	VkImageView *outImageView)
{
	const VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = format,
		.extent = {extent.width, extent.height, 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_STORAGE_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = nullptr,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	VmaAllocationCreateInfo allocationInfo{};
	allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	if (vmaCreateImage(
			state->allocator,
			&imageInfo,
			&allocationInfo,
			outImage,
			outAllocation,
			nullptr) != VK_SUCCESS) {
		return false;
	}

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = *outImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(state->device, &viewInfo, nullptr, outImageView) != VK_SUCCESS) {
		vmaDestroyImage(state->allocator, *outImage, *outAllocation);
		*outImage = VK_NULL_HANDLE;
		*outAllocation = VK_NULL_HANDLE;
		return false;
	}

	return true;
}
} // namespace

void DestroyComputePipeline(AppState *state)
{
	if (!state || !state->device) {
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

	if (state->computeDepthImageView) {
		vkDestroyImageView(state->device, state->computeDepthImageView, nullptr);
		state->computeDepthImageView = VK_NULL_HANDLE;
	}

	if (state->computeDepthImage && state->computeDepthAllocation) {
		vmaDestroyImage(state->allocator, state->computeDepthImage, state->computeDepthAllocation);
		state->computeDepthImage = VK_NULL_HANDLE;
		state->computeDepthAllocation = VK_NULL_HANDLE;
	}

	state->computeDepthImageNeedsInit = false;
	state->computeDescriptorSets.clear();
}

bool CreateComputePipeline(AppState *state)
{
	if (!state || !state->device || state->swapchainImages.empty() || state->swapchainImageViews.empty()) {
		return false;
	}

	if (!SupportsStorageImage(state->physicalDevice, kComputeDepthFormat)) {
		SDL_Log("Compute depth format VK_FORMAT_R32_SFLOAT does not support storage image usage");
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

	VkDescriptorSetLayoutBinding outputImageBinding{};
	outputImageBinding.binding = 0;
	outputImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	outputImageBinding.descriptorCount = 1;
	outputImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding vertexBinding{};
	vertexBinding.binding = 1;
	vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertexBinding.descriptorCount = 1;
	vertexBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutBinding depthImageBinding{};
	depthImageBinding.binding = 2;
	depthImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	depthImageBinding.descriptorCount = 1;
	depthImageBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	const std::array descriptorBindings{
		outputImageBinding,
		vertexBinding,
		depthImageBinding,
	};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(descriptorBindings.size());
	layoutInfo.pBindings = descriptorBindings.data();

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	if (vkCreateDescriptorSetLayout(state->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	if (!state->sceneVertexBuffer || !state->sceneVertexAllocation || state->sceneTriangleCount == 0) {
		SDL_Log("Scene resources are not initialized");
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	VkImage depthImage = VK_NULL_HANDLE;
	VmaAllocation depthAllocation = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE;
	if (!CreateStorageImage(
			state,
			state->extent,
			kComputeDepthFormat,
			&depthImage,
			&depthAllocation,
			&depthImageView)) {
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	const size_t descriptorSetCount = state->swapchainImageViews.size();
	const std::array poolSizes{
		VkDescriptorPoolSize{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			.descriptorCount = static_cast<uint32_t>(descriptorSetCount * 2),
		},
		VkDescriptorPoolSize{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = static_cast<uint32_t>(descriptorSetCount),
		},
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = static_cast<uint32_t>(descriptorSetCount);
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	if (vkCreateDescriptorPool(state->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		vkDestroyImageView(state->device, depthImageView, nullptr);
		vmaDestroyImage(state->allocator, depthImage, depthAllocation);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	std::vector layouts(descriptorSetCount, descriptorSetLayout);
	std::vector<VkDescriptorSet> descriptorSets(descriptorSetCount, VK_NULL_HANDLE);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(descriptorSetCount);
	allocInfo.pSetLayouts = layouts.data();
	if (vkAllocateDescriptorSets(state->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		vkDestroyDescriptorPool(state->device, descriptorPool, nullptr);
		vkDestroyImageView(state->device, depthImageView, nullptr);
		vmaDestroyImage(state->allocator, depthImage, depthAllocation);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	std::vector<VkDescriptorImageInfo> outputImageInfos(descriptorSetCount);
	std::vector<VkDescriptorBufferInfo> vertexBufferInfos(descriptorSetCount);
	std::vector<VkDescriptorImageInfo> depthImageInfos(descriptorSetCount);
	std::vector<VkWriteDescriptorSet> writes(descriptorSetCount * size_t{3});
	for (size_t i = 0; i < descriptorSetCount; ++i) {
		const size_t writeBaseIndex = i * size_t{3};

		outputImageInfos[i].imageView = state->swapchainImageViews[i];
		outputImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		vertexBufferInfos[i].buffer = state->sceneVertexBuffer;
		vertexBufferInfos[i].offset = 0;
		vertexBufferInfos[i].range = VK_WHOLE_SIZE;

		depthImageInfos[i].imageView = depthImageView;
		depthImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		writes[writeBaseIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeBaseIndex].dstSet = descriptorSets[i];
		writes[writeBaseIndex].dstBinding = 0;
		writes[writeBaseIndex].descriptorCount = 1;
		writes[writeBaseIndex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeBaseIndex].pImageInfo = &outputImageInfos[i];

		writes[writeBaseIndex + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeBaseIndex + 1].dstSet = descriptorSets[i];
		writes[writeBaseIndex + 1].dstBinding = 1;
		writes[writeBaseIndex + 1].descriptorCount = 1;
		writes[writeBaseIndex + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[writeBaseIndex + 1].pBufferInfo = &vertexBufferInfos[i];

		writes[writeBaseIndex + 2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[writeBaseIndex + 2].dstSet = descriptorSets[i];
		writes[writeBaseIndex + 2].dstBinding = 2;
		writes[writeBaseIndex + 2].descriptorCount = 1;
		writes[writeBaseIndex + 2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		writes[writeBaseIndex + 2].pImageInfo = &depthImageInfos[i];
	}
	vkUpdateDescriptorSets(state->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(ComputePushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	if (vkCreatePipelineLayout(state->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		vkDestroyDescriptorPool(state->device, descriptorPool, nullptr);
		vkDestroyImageView(state->device, depthImageView, nullptr);
		vmaDestroyImage(state->allocator, depthImage, depthAllocation);
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
		vkDestroyImageView(state->device, depthImageView, nullptr);
		vmaDestroyImage(state->allocator, depthImage, depthAllocation);
		vkDestroyDescriptorSetLayout(state->device, descriptorSetLayout, nullptr);
		vkDestroyShaderModule(state->device, shaderModule, nullptr);
		return false;
	}

	vkDestroyShaderModule(state->device, shaderModule, nullptr);

	state->computeDescriptorSetLayout = descriptorSetLayout;
	state->computeDescriptorPool = descriptorPool;
	state->computeDescriptorSets = std::move(descriptorSets);
	state->computeDepthImage = depthImage;
	state->computeDepthAllocation = depthAllocation;
	state->computeDepthImageView = depthImageView;
	state->computeDepthImageNeedsInit = true;
	state->computePipelineLayout = pipelineLayout;
	state->computePipeline = pipeline;
	return true;
}
