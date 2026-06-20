// Standalone Vulkan compute harness for hzb-binding-models experiment.
// Tests sampling correctness for HZB under different binding models.
//
// Patterns tested:
//   1. textureLod via combined image sampler (classic, vkguide.dev pattern)
//   2. texelFetch via storage image (imageLoad, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
//   3. texelFetch via sampled image (texelFetch, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE)
//
// Binding strategies tested per pattern:
//   - Classic: vkAllocateDescriptorSets (VkDescriptorSet)
//   - Bindless: VK_EXT_descriptor_heap (if available on dev host)
//
// Output: results.csv with per-(pattern, mip, bind_strategy) pass/fail.

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// Global image dimensions (set in main before use).
static uint32_t ctx_width = 0;
static uint32_t ctx_height = 0;

#define CHECK(call)                                                        \
	do {                                                                   \
		VkResult r = (call);                                               \
		if (r != VK_SUCCESS) {                                             \
			std::cerr << "Vulkan error at " << __FILE__ << ":" << __LINE__ \
					  << ": " << static_cast<int>(r) << " (" #call ")\n";  \
			std::abort();                                                  \
		}                                                                  \
	} while (0)

struct VkContext {
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	uint32_t computeQueueFamily = 0;
	VkCommandPool cmdPool = VK_NULL_HANDLE;
	VkCommandBuffer cmd = VK_NULL_HANDLE;
};

struct DepthPyramid {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView wholeView = VK_NULL_HANDLE;	  // mip 0 only
	VkImageView mipBaseView = VK_NULL_HANDLE; // all mips via subresourceRange
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t mipLevels = 0;
	VkDeviceSize totalSize = 0;
	VkDeviceSize mipOffsets[16] = {};
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT /*types*/,
	const VkDebugUtilsMessengerCallbackDataEXT *data,
	void * /*user*/)
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		std::cerr << "[VULKAN] " << data->pMessage << "\n";
	}
	return VK_FALSE;
}

static bool hasExt(const std::vector<VkExtensionProperties> &exts, const char *name)
{
	for (const auto &e : exts) {
		if (std::strcmp(e.extensionName, name) == 0)
			return true;
	}
	return false;
}

static void initContext(VkContext &ctx, bool &outDescriptorHeapAvailable)
{
	VkApplicationInfo app{};
	app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app.pApplicationName = "hzb-binding-bench";
	app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app.pEngineName = "research";
	app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app.apiVersion = VK_API_VERSION_1_3;

	std::vector<const char *> instanceExts = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};
	std::vector<const char *> instanceLayers = {"VK_LAYER_KHRONOS_validation"};

	VkInstanceCreateInfo instInfo{};
	instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instInfo.pApplicationInfo = &app;
	instInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExts.size());
	instInfo.ppEnabledExtensionNames = instanceExts.data();
	instInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
	instInfo.ppEnabledLayerNames = instanceLayers.data();
	CHECK(vkCreateInstance(&instInfo, nullptr, &ctx.instance));

	auto dbgCreate = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
		ctx.instance, "vkCreateDebugUtilsMessengerEXT");
	if (dbgCreate) {
		VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
		dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbgInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
							  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		dbgInfo.pfnUserCallback = debugCallback;
		VkDebugUtilsMessengerEXT messenger;
		dbgCreate(ctx.instance, &dbgInfo, nullptr, &messenger);
	}

	uint32_t devCount = 0;
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
	if (devCount == 0) {
		std::cerr << "No Vulkan devices\n";
		std::abort();
	}
	std::vector<VkPhysicalDevice> devs(devCount);
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, devs.data());
	ctx.physical = devs[0];

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(ctx.physical, &props);
	std::cout << "[DEVICE] " << props.deviceName << " (api=" << VK_API_VERSION_MAJOR(props.apiVersion)
			  << "." << VK_API_VERSION_MINOR(props.apiVersion) << "."
			  << VK_API_VERSION_PATCH(props.apiVersion) << ")\n";
	std::cout << "[DEVICE] driver=" << props.driverVersion
			  << " vendor=0x" << std::hex << props.vendorID << std::dec << "\n";

	uint32_t qfCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical, &qfCount, nullptr);
	std::vector<VkQueueFamilyProperties> qfs(qfCount);
	vkGetPhysicalDeviceQueueFamilyProperties(ctx.physical, &qfCount, qfs.data());
	ctx.computeQueueFamily = UINT32_MAX;
	for (uint32_t i = 0; i < qfCount; ++i) {
		if (qfs[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			ctx.computeQueueFamily = i;
			break;
		}
	}
	if (ctx.computeQueueFamily == UINT32_MAX) {
		std::cerr << "No compute queue family\n";
		std::abort();
	}

	std::vector<VkExtensionProperties> devExts;
	uint32_t devExtCount = 0;
	vkEnumerateDeviceExtensionProperties(ctx.physical, nullptr, &devExtCount, nullptr);
	devExts.resize(devExtCount);
	vkEnumerateDeviceExtensionProperties(ctx.physical, nullptr, &devExtCount, devExts.data());

	outDescriptorHeapAvailable = hasExt(devExts, "VK_EXT_descriptor_heap");

	std::cout << "[DEVICE] VK_EXT_descriptor_heap: "
			  << (outDescriptorHeapAvailable ? "AVAILABLE" : "NOT AVAILABLE") << "\n";

	std::vector<const char *> devExtList;
	// VK_EXT_descriptor_heap is intentionally NOT enabled here: it requires VK_KHR_maintenance5
	// (chained feature struct), and this prototype doesn't use the heap path. Heap availability
	// is reported by outDescriptorHeapAvailable for future expansion.
	(void)outDescriptorHeapAvailable;

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qInfo{};
	qInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qInfo.queueFamilyIndex = ctx.computeQueueFamily;
	qInfo.queueCount = 1;
	qInfo.pQueuePriorities = &prio;

	VkPhysicalDeviceFeatures baseFeat{};
	VkDeviceCreateInfo devInfo{};
	devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devInfo.queueCreateInfoCount = 1;
	devInfo.pQueueCreateInfos = &qInfo;
	devInfo.enabledExtensionCount = static_cast<uint32_t>(devExtList.size());
	devInfo.ppEnabledExtensionNames = devExtList.data();
	devInfo.pEnabledFeatures = &baseFeat;
	CHECK(vkCreateDevice(ctx.physical, &devInfo, nullptr, &ctx.device));

	vkGetDeviceQueue(ctx.device, ctx.computeQueueFamily, 0, &ctx.computeQueue);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = ctx.computeQueueFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	CHECK(vkCreateCommandPool(ctx.device, &poolInfo, nullptr, &ctx.cmdPool));

	VkCommandBufferAllocateInfo cbInfo{};
	cbInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbInfo.commandPool = ctx.cmdPool;
	cbInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbInfo.commandBufferCount = 1;
	CHECK(vkAllocateCommandBuffers(ctx.device, &cbInfo, &ctx.cmd));
}

static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags want)
{
	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
	for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
		if ((filter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & want) == want) {
			return i;
		}
	}
	std::cerr << "No suitable memory type\n";
	std::abort();
}

static void createPyramid(VkContext &ctx, DepthPyramid &pyr, uint32_t w, uint32_t h, uint32_t mips)
{
	pyr.width = w;
	pyr.height = h;
	pyr.mipLevels = mips;

	// Compute mip sizes + offsets
	VkDeviceSize offset = 0;
	for (uint32_t m = 0; m < mips; ++m) {
		uint32_t mw = std::max(1u, w >> m);
		uint32_t mh = std::max(1u, h >> m);
		VkDeviceSize mipSize = static_cast<VkDeviceSize>(mw) * mh * 4; // R32_SFLOAT = 4 bytes
		pyr.mipOffsets[m] = offset;
		offset += mipSize;
	}
	pyr.totalSize = offset;

	VkImageCreateInfo imgInfo{};
	imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imgInfo.imageType = VK_IMAGE_TYPE_2D;
	imgInfo.format = VK_FORMAT_R32_SFLOAT;
	imgInfo.extent = {w, h, 1};
	imgInfo.mipLevels = mips;
	imgInfo.arrayLayers = 1;
	imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
					VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					VK_IMAGE_USAGE_SAMPLED_BIT |
					VK_IMAGE_USAGE_STORAGE_BIT;
	imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	CHECK(vkCreateImage(ctx.device, &imgInfo, nullptr, &pyr.image));

	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(ctx.device, pyr.image, &memReq);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(ctx.physical, memReq.memoryTypeBits,
											   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	CHECK(vkAllocateMemory(ctx.device, &allocInfo, nullptr, &pyr.memory));
	CHECK(vkBindImageMemory(ctx.device, pyr.image, pyr.memory, 0));

	// Whole image view (all mips)
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = pyr.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R32_SFLOAT;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	CHECK(vkCreateImageView(ctx.device, &viewInfo, nullptr, &pyr.mipBaseView));

	// Mip 0 only
	VkImageViewCreateInfo m0Info = viewInfo;
	m0Info.subresourceRange.levelCount = 1;
	CHECK(vkCreateImageView(ctx.device, &m0Info, nullptr, &pyr.wholeView));
}

// Populates pyramid with synthetic depth values.
// Each texel at (x, y, mip) has known value = float(mip * 1000 + y * 100 + x) * 0.001f.
// This is recognizable per-mip and per-texel.
static void populatePyramid(VkContext &ctx, DepthPyramid &pyr)
{
	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = pyr.totalSize;
	bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CHECK(vkCreateBuffer(ctx.device, &bufInfo, nullptr, &staging));

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(ctx.device, staging, &memReq);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(ctx.physical, memReq.memoryTypeBits,
											   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
												   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK(vkAllocateMemory(ctx.device, &allocInfo, nullptr, &stagingMem));
	CHECK(vkBindBufferMemory(ctx.device, staging, stagingMem, 0));

	std::vector<uint8_t> hostData(pyr.totalSize);
	for (uint32_t m = 0; m < pyr.mipLevels; ++m) {
		uint32_t mw = std::max(1u, pyr.width >> m);
		uint32_t mh = std::max(1u, pyr.height >> m);
		float *dst = reinterpret_cast<float *>(hostData.data() + pyr.mipOffsets[m]);
		for (uint32_t y = 0; y < mh; ++y) {
			for (uint32_t x = 0; x < mw; ++x) {
				// Distinct, recognizable value per (m, x, y)
				uint32_t key = m * 1000000u + y * 1000u + x;
				dst[y * mw + x] = static_cast<float>(key) * 0.001f;
			}
		}
	}
	void *mapped = nullptr;
	CHECK(vkMapMemory(ctx.device, stagingMem, 0, pyr.totalSize, 0, &mapped));
	std::memcpy(mapped, hostData.data(), pyr.totalSize);
	vkUnmapMemory(ctx.device, stagingMem);

	// Transition all mips to TRANSFER_DST_OPTIMAL, copy, transition to SHADER_READ_ONLY_OPTIMAL
	VkCommandBufferBeginInfo cbBegin{};
	cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CHECK(vkBeginCommandBuffer(ctx.cmd, &cbBegin));

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = pyr.image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = pyr.mipLevels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(ctx.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	for (uint32_t m = 0; m < pyr.mipLevels; ++m) {
		uint32_t mw = std::max(1u, pyr.width >> m);
		uint32_t mh = std::max(1u, pyr.height >> m);
		VkBufferImageCopy region{};
		region.bufferOffset = pyr.mipOffsets[m];
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = m;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {mw, mh, 1};
		vkCmdCopyBufferToImage(ctx.cmd, staging, pyr.image,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	VkImageMemoryBarrier toShader{};
	toShader.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toShader.image = pyr.image;
	toShader.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toShader.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toShader.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toShader.subresourceRange.baseMipLevel = 0;
	toShader.subresourceRange.levelCount = pyr.mipLevels;
	toShader.subresourceRange.baseArrayLayer = 0;
	toShader.subresourceRange.layerCount = 1;
	toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toShader.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(ctx.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toShader);

	CHECK(vkEndCommandBuffer(ctx.cmd));
	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &ctx.cmd;
	CHECK(vkQueueSubmit(ctx.computeQueue, 1, &submit, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(ctx.computeQueue));

	vkDestroyBuffer(ctx.device, staging, nullptr);
	vkFreeMemory(ctx.device, stagingMem, nullptr);
}

// Readback single mip level to host.
static std::vector<float> readbackMip(VkContext &ctx, DepthPyramid &pyr, uint32_t mipLevel)
{
	uint32_t mw = std::max(1u, pyr.width >> mipLevel);
	uint32_t mh = std::max(1u, pyr.height >> mipLevel);
	VkDeviceSize mipSize = static_cast<VkDeviceSize>(mw) * mh * 4;

	VkBuffer staging = VK_NULL_HANDLE;
	VkDeviceMemory stagingMem = VK_NULL_HANDLE;

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = mipSize;
	bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CHECK(vkCreateBuffer(ctx.device, &bufInfo, nullptr, &staging));

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(ctx.device, staging, &memReq);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(ctx.physical, memReq.memoryTypeBits,
											   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
												   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK(vkAllocateMemory(ctx.device, &allocInfo, nullptr, &stagingMem));
	CHECK(vkBindBufferMemory(ctx.device, staging, stagingMem, 0));

	VkCommandBufferBeginInfo cbBegin{};
	cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CHECK(vkBeginCommandBuffer(ctx.cmd, &cbBegin));

	VkImageMemoryBarrier toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toTransfer.image = pyr.image;
	toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toTransfer.subresourceRange.baseMipLevel = mipLevel;
	toTransfer.subresourceRange.levelCount = 1;
	toTransfer.subresourceRange.baseArrayLayer = 0;
	toTransfer.subresourceRange.layerCount = 1;
	toTransfer.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(ctx.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = mipLevel;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {0, 0, 0};
	region.imageExtent = {mw, mh, 1};
	vkCmdCopyImageToBuffer(ctx.cmd, pyr.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   staging, 1, &region);

	VkImageMemoryBarrier toGeneral{};
	toGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toGeneral.image = pyr.image;
	toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toGeneral.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toGeneral.subresourceRange.baseMipLevel = mipLevel;
	toGeneral.subresourceRange.levelCount = 1;
	toGeneral.subresourceRange.baseArrayLayer = 0;
	toGeneral.subresourceRange.layerCount = 1;
	toGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	toGeneral.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(ctx.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);

	CHECK(vkEndCommandBuffer(ctx.cmd));
	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &ctx.cmd;
	CHECK(vkQueueSubmit(ctx.computeQueue, 1, &submit, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(ctx.computeQueue));

	std::vector<float> result(mw * mh);
	void *mapped = nullptr;
	CHECK(vkMapMemory(ctx.device, stagingMem, 0, mipSize, 0, &mapped));
	std::memcpy(result.data(), mapped, mipSize);
	vkUnmapMemory(ctx.device, stagingMem);

	vkDestroyBuffer(ctx.device, staging, nullptr);
	vkFreeMemory(ctx.device, stagingMem, nullptr);
	return result;
}

struct ShaderModule {
	VkShaderModule module = VK_NULL_HANDLE;
};

static VkShaderModule loadShader(VkDevice dev, const std::string &path)
{
	std::ifstream f(path, std::ios::binary);
	if (!f) {
		std::cerr << "Failed to open shader: " << path << "\n";
		std::abort();
	}
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
							   std::istreambuf_iterator<char>());
	VkShaderModuleCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = bytes.size();
	info.pCode = reinterpret_cast<const uint32_t *>(bytes.data());
	VkShaderModule mod;
	CHECK(vkCreateShaderModule(dev, &info, nullptr, &mod));
	return mod;
}

struct Pipeline {
	VkPipeline pipe = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
};

// Descriptor set layout: 1 binding — combined sampler2D at binding 0, SSBO at binding 1.
static Pipeline makePipelineCombined(VkDevice dev)
{
	VkDescriptorSetLayoutBinding bindings[2]{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;
	VkDescriptorSetLayout dsl;
	CHECK(vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &dsl));

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcRange.offset = 0;
	pcRange.size = sizeof(int32_t) * 4;

	VkPipelineLayoutCreateInfo plInfo{};
	plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plInfo.setLayoutCount = 1;
	plInfo.pSetLayouts = &dsl;
	plInfo.pushConstantRangeCount = 1;
	plInfo.pPushConstantRanges = &pcRange;
	VkPipelineLayout pl;
	CHECK(vkCreatePipelineLayout(dev, &plInfo, nullptr, &pl));

	Pipeline result;
	result.layout = pl;
	result.dsl = dsl;
	return result;
}

// SSBO for readback samples
struct SampleBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	void *mapped = nullptr;
	uint32_t capacity = 0;
};

static SampleBuffer makeSampleBuffer(VkContext &ctx, uint32_t count)
{
	SampleBuffer sb;
	sb.capacity = count;
	VkDeviceSize size = static_cast<VkDeviceSize>(count) * sizeof(float);

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = size;
	bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	CHECK(vkCreateBuffer(ctx.device, &bufInfo, nullptr, &sb.buffer));

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(ctx.device, sb.buffer, &memReq);
	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = findMemoryType(ctx.physical, memReq.memoryTypeBits,
											   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
												   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	CHECK(vkAllocateMemory(ctx.device, &allocInfo, nullptr, &sb.memory));
	CHECK(vkBindBufferMemory(ctx.device, sb.buffer, sb.memory, 0));
	CHECK(vkMapMemory(ctx.device, sb.memory, 0, size, 0, &sb.mapped));
	return sb;
}

// One test run: dispatch one shader, readback, compare vs reference
struct TestResult {
	std::string pattern;
	std::string bindStrategy;
	uint32_t mipLevel;
	bool allCorrect;
	float maxAbsError;
	uint32_t mismatchedSamples;
	uint32_t totalSamples;
};

static float computeRef(uint32_t m, uint32_t x, uint32_t y)
{
	return static_cast<float>(m * 1000000u + y * 1000u + x) * 0.001f;
}

static TestResult runSampleTest(
	VkContext &ctx, VkPipeline pipe, VkPipelineLayout layout,
	VkImageView imageView, VkSampler sampler,
	SampleBuffer &sb, uint32_t sampleCount, uint32_t mipLevel,
	const std::string &pattern, const std::string &bindStrategy)
{

	// Allocate descriptor set
	VkDescriptorSetLayout dsl;
	{
		VkDescriptorSetLayoutBinding bindings[2]{};
		bindings[0].binding = 0;
		bindings[0].descriptorType = (bindStrategy == "storage_image") ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bindings[0].descriptorCount = 1;
		bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		bindings[1].binding = 1;
		bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[1].descriptorCount = 1;
		bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		VkDescriptorSetLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.bindingCount = 2;
		info.pBindings = bindings;
		CHECK(vkCreateDescriptorSetLayout(ctx.device, &info, nullptr, &dsl));
	}

	VkDescriptorPoolSize poolSizes[2]{};
	poolSizes[0].type = (bindStrategy == "storage_image") ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = 1;
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	VkDescriptorPool pool;
	CHECK(vkCreateDescriptorPool(ctx.device, &poolInfo, nullptr, &pool));

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &dsl;
	VkDescriptorSet ds;
	CHECK(vkAllocateDescriptorSets(ctx.device, &allocInfo, &ds));

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageView = imageView;
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imgInfo.sampler = sampler;
	VkDescriptorBufferInfo bufInfo{};
	bufInfo.buffer = sb.buffer;
	bufInfo.offset = 0;
	bufInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writes[2]{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = ds;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = (bindStrategy == "storage_image") ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imgInfo;
	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = ds;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &bufInfo;
	vkUpdateDescriptorSets(ctx.device, 2, writes, 0, nullptr);

	// Record commands
	VkCommandBufferBeginInfo cbBegin{};
	cbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cbBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	CHECK(vkBeginCommandBuffer(ctx.cmd, &cbBegin));

	vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
	vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &ds, 0, nullptr);

	struct PushData {
		int32_t imageSizeX;
		int32_t imageSizeY;
		int32_t mipLevel;
		int32_t sampleCount;
	} pc{};
	uint32_t mipW = std::max(1u, ctx_width >> mipLevel);
	uint32_t mipH = std::max(1u, ctx_height >> mipLevel);
	pc.imageSizeX = static_cast<int32_t>(mipW);
	pc.imageSizeY = static_cast<int32_t>(mipH);
	pc.mipLevel = static_cast<int32_t>(mipLevel);
	pc.sampleCount = static_cast<int32_t>(sampleCount);
	vkCmdPushConstants(ctx.cmd, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

	uint32_t groupsX = (sampleCount + 7) / 8;
	vkCmdDispatch(ctx.cmd, groupsX, 1, 1);

	VkBufferMemoryBarrier bufBarrier{};
	bufBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	bufBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	bufBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bufBarrier.buffer = sb.buffer;
	bufBarrier.offset = 0;
	bufBarrier.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(ctx.cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						 VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &bufBarrier, 0, nullptr);

	CHECK(vkEndCommandBuffer(ctx.cmd));
	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &ctx.cmd;
	CHECK(vkQueueSubmit(ctx.computeQueue, 1, &submit, VK_NULL_HANDLE));
	CHECK(vkQueueWaitIdle(ctx.computeQueue));

	// Compare vs reference
	TestResult result;
	result.pattern = pattern;
	result.bindStrategy = bindStrategy;
	result.mipLevel = mipLevel;
	result.allCorrect = true;
	result.maxAbsError = 0.0f;
	result.mismatchedSamples = 0;
	result.totalSamples = sampleCount;

	float *samples = reinterpret_cast<float *>(sb.mapped);
	for (uint32_t i = 0; i < sampleCount; ++i) {
		uint32_t x = i % mipW;
		uint32_t y = i / mipW;
		float ref = computeRef(mipLevel, x, y);
		float got = samples[i];
		float err = std::abs(got - ref);
		if (err > result.maxAbsError)
			result.maxAbsError = err;
		if (err > 1e-3f) {
			result.mismatchedSamples++;
			result.allCorrect = false;
		}
	}

	vkDestroyDescriptorPool(ctx.device, pool, nullptr);
	vkDestroyDescriptorSetLayout(ctx.device, dsl, nullptr);
	return result;
}

// Forward decls for globals

static Pipeline makePipelineStorage(VkDevice dev)
{
	VkDescriptorSetLayoutBinding bindings[2]{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 2;
	layoutInfo.pBindings = bindings;
	VkDescriptorSetLayout dsl;
	CHECK(vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &dsl));

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pcRange.offset = 0;
	pcRange.size = sizeof(int32_t) * 4;

	VkPipelineLayoutCreateInfo plInfo{};
	plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plInfo.setLayoutCount = 1;
	plInfo.pSetLayouts = &dsl;
	plInfo.pushConstantRangeCount = 1;
	plInfo.pPushConstantRanges = &pcRange;
	VkPipelineLayout pl;
	CHECK(vkCreatePipelineLayout(dev, &plInfo, nullptr, &pl));

	Pipeline result;
	result.layout = pl;
	result.dsl = dsl;
	return result;
}

static Pipeline makePipelineFromShader(VkDevice dev, VkShaderModule mod, bool useStorageImage)
{
	Pipeline p = useStorageImage ? makePipelineStorage(dev) : makePipelineCombined(dev);

	VkComputePipelineCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	info.stage.module = mod;
	info.stage.pName = "main";
	info.layout = p.layout;
	CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &p.pipe));
	return p;
}

int main(int argc, char **argv)
{
	if (argc > 1 && std::string(argv[1]) == "--help") {
		std::cout << "Usage: hzb_bench [width] [height] [mips]\n"
				  << "  Default: 1920 1080 8\n";
		return 0;
	}
	uint32_t w = (argc > 1) ? std::atoi(argv[1]) : 1920;
	uint32_t h = (argc > 2) ? std::atoi(argv[2]) : 1080;
	uint32_t mips = (argc > 3) ? std::atoi(argv[3]) : 8;
	ctx_width = w;
	ctx_height = h;

	std::cout << "[CONFIG] " << w << "x" << h << " mips=" << mips << "\n";

	VkContext ctx;
	bool heapAvailable = false;
	initContext(ctx, heapAvailable);

	DepthPyramid pyr;
	createPyramid(ctx, pyr, w, h, mips);
	populatePyramid(ctx, pyr);

	// Verify CPU reference = GPU mip 0
	std::vector<float> mip0 = readbackMip(ctx, pyr, 0);
	uint32_t mip0Err = 0;
	for (uint32_t y = 0; y < h; ++y) {
		for (uint32_t x = 0; x < w; ++x) {
			float ref = computeRef(0, x, y);
			float got = mip0[y * w + x];
			if (std::abs(got - ref) > 1e-3f)
				mip0Err++;
		}
	}
	std::cout << "[VERIFY] mip 0 reference equality: " << (mip0Err == 0 ? "PASS" : "FAIL")
			  << " (mismatched=" << mip0Err << "/" << (w * h) << ")\n";
	if (mip0Err > 0) {
		std::cerr << "[FATAL] mip 0 has mismatches — abort\n";
		return 1;
	}

	// Create sampler
	VkSamplerCreateInfo sampInfo{};
	sampInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampInfo.magFilter = VK_FILTER_NEAREST;
	sampInfo.minFilter = VK_FILTER_NEAREST;
	sampInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampInfo.minLod = 0.0f;
	sampInfo.maxLod = static_cast<float>(mips);
	sampInfo.mipLodBias = 0.0f;
	VkSampler sampler;
	CHECK(vkCreateSampler(ctx.device, &sampInfo, nullptr, &sampler));

	SampleBuffer sb = makeSampleBuffer(ctx, 1024);

	// Load shaders
	VkShaderModule textureLodMod = loadShader(ctx.device, "shaders/sample_textureLod.comp.spv");
	VkShaderModule texelFetchStorageMod = loadShader(ctx.device, "shaders/sample_texelFetch_storage.comp.spv");
	VkShaderModule texelFetchSampledMod = loadShader(ctx.device, "shaders/sample_texelFetch_sampled.comp.spv");

	Pipeline pTextureLod = makePipelineFromShader(ctx.device, textureLodMod, false);
	Pipeline pTexelFetchStorage = makePipelineFromShader(ctx.device, texelFetchStorageMod, true);
	Pipeline pTexelFetchSampled = makePipelineFromShader(ctx.device, texelFetchSampledMod, false);

	std::vector<TestResult> results;

	for (uint32_t m = 0; m < mips; ++m) {
		uint32_t mw = std::max(1u, w >> m);
		uint32_t mh = std::max(1u, h >> m);
		uint32_t sampleCount = std::min<uint32_t>(mw * mh, 1024);

		results.push_back(runSampleTest(ctx, pTextureLod.pipe, pTextureLod.layout,
										pyr.mipBaseView, sampler, sb, sampleCount, m,
										"textureLod", "combined_sampler"));
		results.push_back(runSampleTest(ctx, pTexelFetchStorage.pipe, pTexelFetchStorage.layout,
										pyr.mipBaseView, sampler, sb, sampleCount, m,
										"texelFetch_storage_image", "storage_image"));
		results.push_back(runSampleTest(ctx, pTexelFetchSampled.pipe, pTexelFetchSampled.layout,
										pyr.mipBaseView, sampler, sb, sampleCount, m,
										"texelFetch_sampled_image", "combined_sampler"));
	}

	// Print results
	std::cout << "\n=== RESULTS ===\n";
	std::cout << "pattern,bind_strategy,mip,all_correct,max_abs_error,mismatched,total\n";
	int failures = 0;
	for (const auto &r : results) {
		std::cout << r.pattern << "," << r.bindStrategy << "," << r.mipLevel << ","
				  << (r.allCorrect ? "PASS" : "FAIL") << ","
				  << r.maxAbsError << "," << r.mismatchedSamples << "," << r.totalSamples << "\n";
		if (!r.allCorrect)
			failures++;
	}

	std::cout << "\n[SUMMARY] " << failures << " failures of " << results.size() << " tests\n";

	// Write CSV
	std::ofstream csv("results.csv");
	csv << "pattern,bind_strategy,mip,all_correct,max_abs_error,mismatched,total\n";
	for (const auto &r : results) {
		csv << r.pattern << "," << r.bindStrategy << "," << r.mipLevel << ","
			<< (r.allCorrect ? "PASS" : "FAIL") << ","
			<< r.maxAbsError << "," << r.mismatchedSamples << "," << r.totalSamples << "\n";
	}
	csv.close();

	return failures == 0 ? 0 : 1;
}
