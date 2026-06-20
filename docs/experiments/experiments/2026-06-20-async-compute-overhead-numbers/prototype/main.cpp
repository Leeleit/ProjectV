// Async-compute overhead measurement harness.
// Standalone Vulkan 1.4 app per `docs/experiments/AGENTS.md §2` (no ProjectV deps).
// Measures overlap graphics||compute for 3 synthetic ProjectV-style workloads.

#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <span>
#include <string>
#include <vector>

#include "shaders/compute_a.comp.spv.h"
#include "shaders/compute_b.comp.spv.h"
#include "shaders/compute_c.comp.spv.h"
#include "shaders/graphics.vert.spv.h"
#include "shaders/graphics.frag.spv.h"

namespace {

constexpr uint32_t WORKLOAD_A_SIZE = 64;
constexpr uint32_t WORKLOAD_A_SUBSTEPS = 8;
constexpr uint32_t WORKLOAD_B_CHUNKS = 4096;
constexpr uint32_t WORKLOAD_B_MIPS = 8;
constexpr uint32_t WORKLOAD_B_MIP_RES_LOG2 = 10;
constexpr uint32_t WORKLOAD_B_SUBSTEPS = 4;
constexpr uint32_t WORKLOAD_C_SIZE = 64;
constexpr uint32_t WORKLOAD_C_SUBSTEPS = 4;

constexpr uint32_t WARMUP_FRAMES = 30;
constexpr uint32_t MEASURE_FRAMES = 200;

struct Stats {
	double mean;
	double median;
	double p95;
	double p99;
	double stddev;
	double min;
	double max;
	double gpuGfx = 0.0;
	double gpuCompute = 0.0;
};

template <typename T>
Stats ComputeStats(std::vector<T> samples)
{
	Stats s{};
	std::ranges::sort(samples);
	double sum = 0.0;
	for (auto v : samples)
		sum += v;
	s.mean = sum / static_cast<double>(samples.size());
	s.median = samples[samples.size() / 2];
	s.p95 = samples[static_cast<size_t>(samples.size() * 0.95)];
	s.p99 = samples[static_cast<size_t>(samples.size() * 0.99)];
	double var = 0.0;
	for (auto v : samples)
		var += (v - s.mean) * (v - s.mean);
	s.stddev = std::sqrt(var / static_cast<double>(samples.size()));
	s.min = samples.front();
	s.max = samples.back();
	return s;
}

std::string FormatStats(const Stats &s)
{
	char buf[256];
	std::snprintf(buf, sizeof(buf),
				  "mean=%.3f median=%.3f p95=%.3f p99=%.3f std=%.3f min=%.3f max=%.3f",
				  s.mean, s.median, s.p95, s.p99, s.stddev, s.min, s.max);
	return buf;
}

void WriteCsv(const std::string &path,
			  const std::vector<std::string> &rows)
{
	std::ofstream f(path);
	for (const auto &r : rows)
		f << r << "\n";
}

#define VK_CHECK(call)                                              \
	do {                                                            \
		VkResult _r = (call);                                       \
		if (_r != VK_SUCCESS) {                                     \
			std::fprintf(stderr, "VK error %d at %s:%d\n",          \
						 static_cast<int>(_r), __FILE__, __LINE__); \
			std::exit(1);                                           \
		}                                                           \
	} while (0)

struct QueueIndices {
	uint32_t graphics = UINT32_MAX;
	uint32_t compute = UINT32_MAX;
	bool dedicatedCompute = false;
};

QueueIndices FindQueues(VkPhysicalDevice phys)
{
	QueueIndices qi;
	uint32_t count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
	std::vector<VkQueueFamilyProperties> families(count);
	vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, families.data());

	for (uint32_t i = 0; i < count; ++i) {
		if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			qi.graphics == UINT32_MAX) {
			qi.graphics = i;
		}
	}

	// Prefer dedicated compute-only family (no GRAPHICS_BIT).
	for (uint32_t i = 0; i < count; ++i) {
		if ((families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
			!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			qi.compute == UINT32_MAX) {
			qi.compute = i;
			qi.dedicatedCompute = true;
		}
	}

	// Fallback: any compute-capable queue (even if same family as graphics).
	if (qi.compute == UINT32_MAX) {
		for (uint32_t i = 0; i < count; ++i) {
			if (families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
				qi.compute = i;
				qi.dedicatedCompute = false;
				break;
			}
		}
	}

	return qi;
}

uint32_t FindMemoryType(VkPhysicalDevice phys, uint32_t typeBits,
						VkMemoryPropertyFlags flags)
{
	VkPhysicalDeviceMemoryProperties mp;
	vkGetPhysicalDeviceMemoryProperties(phys, &mp);
	for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
		if ((typeBits & (1u << i)) &&
			(mp.memoryTypes[i].propertyFlags & flags) == flags) {
			return i;
		}
	}
	std::fprintf(stderr, "no suitable memory type\n");
	std::exit(1);
}

struct DeviceContext {
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice phys = VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;
	QueueIndices qi;

	VkQueue graphicsQ = VK_NULL_HANDLE;
	VkQueue computeQ = VK_NULL_HANDLE;

	// Timeline semaphores (graphics↔compute sync).
	VkSemaphore graphicsDone = VK_NULL_HANDLE;
	VkSemaphore computeDone = VK_NULL_HANDLE;

	// Timestamp pool (host-resettable).
	VkQueryPool tsPool = VK_NULL_HANDLE;
	uint32_t tsPerFrame = 8; // up to 8 timestamps per frame
};

VkShaderModule CreateShaderModule(VkDevice dev, const uint8_t *code, size_t bytes)
{
	VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	ci.codeSize = bytes;
	ci.pCode = reinterpret_cast<const uint32_t *>(code);
	VkShaderModule sm = VK_NULL_HANDLE;
	VK_CHECK(vkCreateShaderModule(dev, &ci, nullptr, &sm));
	return sm;
}

VkBuffer CreateBuffer(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize size,
					  VkBufferUsageFlags usage, VkMemoryPropertyFlags memFlags,
					  VkDeviceMemory *outMem)
{
	VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkBuffer b = VK_NULL_HANDLE;
	VK_CHECK(vkCreateBuffer(dev, &bci, nullptr, &b));

	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(dev, b, &req);

	VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = FindMemoryType(phys, req.memoryTypeBits, memFlags);
	VK_CHECK(vkAllocateMemory(dev, &mai, nullptr, outMem));
	VK_CHECK(vkBindBufferMemory(dev, b, *outMem, 0));
	return b;
}

VkImage CreateImage3D(VkDevice dev, VkPhysicalDevice phys, uint32_t sx,
					  uint32_t sy, uint32_t sz, VkFormat fmt,
					  VkImageUsageFlags usage, VkMemoryPropertyFlags memFlags,
					  VkDeviceMemory *outMem)
{
	VkImageCreateInfo ici{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ici.imageType = VK_IMAGE_TYPE_3D;
	ici.format = fmt;
	ici.extent = {sx, sy, sz};
	ici.mipLevels = 1;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = usage;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage img = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImage(dev, &ici, nullptr, &img));

	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(dev, img, &req);

	VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = FindMemoryType(phys, req.memoryTypeBits, memFlags);
	VK_CHECK(vkAllocateMemory(dev, &mai, nullptr, outMem));
	VK_CHECK(vkBindImageMemory(dev, img, *outMem, 0));
	return img;
}

} // namespace

int main(int argc, char **argv)
{
	std::string workload = "all";
	std::string mode = "both";
	uint32_t frames = MEASURE_FRAMES;
	std::string csvOut = "results.csv";

	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--workload" && i + 1 < argc)
			workload = argv[++i];
		else if (a == "--mode" && i + 1 < argc)
			mode = argv[++i];
		else if (a == "--frames" && i + 1 < argc)
			frames = std::atoi(argv[++i]);
		else if (a == "--csv" && i + 1 < argc)
			csvOut = argv[++i];
	}

	std::printf("=== async-compute overhead harness ===\n");
	std::printf("workload=%s mode=%s frames=%u csv=%s\n",
				workload.c_str(), mode.c_str(), frames, csvOut.c_str());

	// ---- Vulkan setup ----
	DeviceContext ctx;

	VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
	app.pApplicationName = "async_bench";
	app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app.pEngineName = "experiment";
	app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app.apiVersion = VK_API_VERSION_1_4;

	VkInstanceCreateInfo ic{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
	ic.pApplicationInfo = &app;
	VK_CHECK(vkCreateInstance(&ic, nullptr, &ctx.instance));

	uint32_t devCount = 0;
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, nullptr);
	std::vector<VkPhysicalDevice> phys(devCount);
	vkEnumeratePhysicalDevices(ctx.instance, &devCount, phys.data());
	ctx.phys = phys[0]; // first discrete

	VkPhysicalDeviceProperties dp{};
	vkGetPhysicalDeviceProperties(ctx.phys, &dp);
	std::printf("GPU: %s (Vulkan %u.%u.%u)\n",
				dp.deviceName,
				VK_API_VERSION_MAJOR(dp.apiVersion),
				VK_API_VERSION_MINOR(dp.apiVersion),
				VK_API_VERSION_PATCH(dp.apiVersion));

	ctx.qi = FindQueues(ctx.phys);
	std::printf("Queues: graphics=%u compute=%u (dedicated=%s)\n",
				ctx.qi.graphics, ctx.qi.compute,
				ctx.qi.dedicatedCompute ? "yes" : "no");

	if (ctx.qi.graphics == UINT32_MAX || ctx.qi.compute == UINT32_MAX) {
		std::fprintf(stderr, "no suitable queues\n");
		return 1;
	}

	// Queue create infos (we may need 2 if different families).
	float prio = 0.5f;
	std::vector<VkDeviceQueueCreateInfo> qcis;
	VkDeviceQueueCreateInfo qg{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
	qg.queueFamilyIndex = ctx.qi.graphics;
	qg.queueCount = 1;
	qg.pQueuePriorities = &prio;
	qcis.push_back(qg);

	if (ctx.qi.compute != ctx.qi.graphics) {
		VkDeviceQueueCreateInfo qc{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		qc.queueFamilyIndex = ctx.qi.compute;
		qc.queueCount = 1;
		qc.pQueuePriorities = &prio;
		qcis.push_back(qc);
	}

	// Enable timeline semaphore + sync2 (both core in 1.4 — no feature struct needed).
	VkPhysicalDeviceTimelineSemaphoreProperties ts{};
	ts.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_PROPERTIES;
	// Chain props for completeness (not required for our prototype).
	VkPhysicalDeviceProperties2 dp2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
	dp2.pNext = &ts;
	vkGetPhysicalDeviceProperties2(ctx.phys, &dp2);

	VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
	dci.queueCreateInfoCount = static_cast<uint32_t>(qcis.size());
	dci.pQueueCreateInfos = qcis.data();
	VK_CHECK(vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.dev));

	vkGetDeviceQueue(ctx.dev, ctx.qi.graphics, 0, &ctx.graphicsQ);
	vkGetDeviceQueue(ctx.dev, ctx.qi.compute, 0, &ctx.computeQ);

	// Timeline semaphores (VkSemaphoreTypeCreateInfo pNext).
	VkSemaphoreTypeCreateInfo stci{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
	stci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	stci.initialValue = 0;

	VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	sci.pNext = &stci;
	VK_CHECK(vkCreateSemaphore(ctx.dev, &sci, nullptr, &ctx.graphicsDone));
	VK_CHECK(vkCreateSemaphore(ctx.dev, &sci, nullptr, &ctx.computeDone));

	// Timestamp query pool.
	VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
	qpci.queryType = VK_QUERY_TYPE_TIMESTAMP;
	qpci.queryCount = frames * 4 + 16; // 4 per frame (gfx start/end + compute start/end)
	VK_CHECK(vkCreateQueryPool(ctx.dev, &qpci, nullptr, &ctx.tsPool));

	// Host clock calibration: timestampPeriod (ns per tick).
	double tsPeriod = static_cast<double>(dp.limits.timestampPeriod);

	std::printf("timestampPeriod=%.3f ns\n", tsPeriod);

	// ---- Workload A: VCT-like 3D blur (light) ----
	// Two 3D images (R8UI), input + output, ping-pong-able via descriptor update.
	VkDeviceMemory memA0, memA1, memAParams;
	VkImage imgA0 = CreateImage3D(ctx.dev, ctx.phys, WORKLOAD_A_SIZE,
								  WORKLOAD_A_SIZE, WORKLOAD_A_SIZE,
								  VK_FORMAT_R8_UINT,
								  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memA0);
	VkImage imgA1 = CreateImage3D(ctx.dev, ctx.phys, WORKLOAD_A_SIZE,
								  WORKLOAD_A_SIZE, WORKLOAD_A_SIZE,
								  VK_FORMAT_R8_UINT,
								  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memA1);

	struct AParams {
		uint32_t sizeX, sizeY, sizeZ, substeps;
	};
	VkBuffer bufAParams = CreateBuffer(ctx.dev, ctx.phys, sizeof(AParams),
									   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
									   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									   &memAParams);
	AParams aParams{WORKLOAD_A_SIZE, WORKLOAD_A_SIZE, WORKLOAD_A_SIZE,
					WORKLOAD_A_SUBSTEPS};
	void *p = nullptr;
	vkMapMemory(ctx.dev, memAParams, 0, sizeof(AParams), 0, &p);
	std::memcpy(p, &aParams, sizeof(AParams));
	vkUnmapMemory(ctx.dev, memAParams);

	// Compute shader module + descriptor set layout + pipeline.
	VkShaderModule smA = CreateShaderModule(ctx.dev,
											reinterpret_cast<const uint8_t *>(shaders_compute_a_comp_spv),
											shaders_compute_a_comp_spv_len);

	VkDescriptorSetLayoutBinding ab[3]{};
	ab[0].binding = 0;
	ab[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ab[0].descriptorCount = 1;
	ab[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ab[1].binding = 1;
	ab[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	ab[1].descriptorCount = 1;
	ab[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	ab[2].binding = 2;
	ab[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ab[2].descriptorCount = 1;
	ab[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo aDslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	aDslci.bindingCount = 3;
	aDslci.pBindings = ab;
	VkDescriptorSetLayout aDsl = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(ctx.dev, &aDslci, nullptr, &aDsl));

	VkPipelineLayoutCreateInfo aPlci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	aPlci.setLayoutCount = 1;
	aPlci.pSetLayouts = &aDsl;
	VkPipelineLayout aPl = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(ctx.dev, &aPlci, nullptr, &aPl));

	VkComputePipelineCreateInfo aCpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	aCpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	aCpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	aCpci.stage.module = smA;
	aCpci.stage.pName = "main";
	aCpci.layout = aPl;
	VkPipeline pipeA = VK_NULL_HANDLE;
	VK_CHECK(vkCreateComputePipelines(ctx.dev, VK_NULL_HANDLE, 1, &aCpci, nullptr, &pipeA));

	// Descriptor pool + set.
	VkDescriptorPoolSize aps[2]{};
	aps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	aps[0].descriptorCount = 2;
	aps[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	aps[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo apci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	apci.maxSets = 1;
	apci.poolSizeCount = 2;
	apci.pPoolSizes = aps;
	VkDescriptorPool aPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(ctx.dev, &apci, nullptr, &aPool));

	VkDescriptorSetAllocateInfo asai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	asai.descriptorPool = aPool;
	asai.descriptorSetCount = 1;
	asai.pSetLayouts = &aDsl;
	VkDescriptorSet aSet = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(ctx.dev, &asai, &aSet));

	VkDescriptorImageInfo aImg0{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL};
	VkImageViewCreateInfo aIvci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	aIvci.viewType = VK_IMAGE_VIEW_TYPE_3D;
	aIvci.format = VK_FORMAT_R8_UINT;
	aIvci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	aIvci.image = imgA0;
	VkImageView viewA0 = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImageView(ctx.dev, &aIvci, nullptr, &viewA0));
	aImg0.imageView = viewA0;

	VkDescriptorImageInfo aImg1{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL};
	aIvci.image = imgA1;
	VkImageView viewA1 = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImageView(ctx.dev, &aIvci, nullptr, &viewA1));
	aImg1.imageView = viewA1;

	VkDescriptorBufferInfo aBufInfo{bufAParams, 0, sizeof(AParams)};

	VkWriteDescriptorSet aw[3]{};
	aw[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	aw[0].dstSet = aSet;
	aw[0].dstBinding = 0;
	aw[0].descriptorCount = 1;
	aw[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	aw[0].pImageInfo = &aImg0;
	aw[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	aw[1].dstSet = aSet;
	aw[1].dstBinding = 1;
	aw[1].descriptorCount = 1;
	aw[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	aw[1].pImageInfo = &aImg1;
	aw[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	aw[2].dstSet = aSet;
	aw[2].dstBinding = 2;
	aw[2].descriptorCount = 1;
	aw[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	aw[2].pBufferInfo = &aBufInfo;
	vkUpdateDescriptorSets(ctx.dev, 3, aw, 0, nullptr);

	std::printf("Workload A: 3D blur %u^3 substeps=%u (light)\n",
				WORKLOAD_A_SIZE, WORKLOAD_A_SUBSTEPS);

	// ---- Workload B: HZB cull-like (medium) ----
	struct ChunkAabb {
		float minX, minY, minZ, maxX, maxY, maxZ, pad0, pad1;
	};
	static_assert(sizeof(ChunkAabb) == 32);

	uint32_t mipRes = 1u << WORKLOAD_B_MIP_RES_LOG2;
	uint32_t hzbTotalPixels = 0;
	for (uint32_t m = 0; m < WORKLOAD_B_MIPS; ++m) {
		uint32_t r = std::max(1u, mipRes >> m);
		hzbTotalPixels += r * r;
	}

	VkDeviceMemory memBAabbs, memBHzb, memBMask, memBParams;
	VkBuffer bufBAabbs = CreateBuffer(ctx.dev, ctx.phys,
									  sizeof(ChunkAabb) * WORKLOAD_B_CHUNKS,
									  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
									  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									  &memBAabbs);
	VkBuffer bufBHzb = CreateBuffer(ctx.dev, ctx.phys,
									sizeof(uint32_t) * hzbTotalPixels,
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
									VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									&memBHzb);
	VkBuffer bufBMask = CreateBuffer(ctx.dev, ctx.phys,
									 sizeof(uint32_t) * WORKLOAD_B_CHUNKS,
									 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
									 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
									 &memBMask);
	struct BParams {
		uint32_t chunkCount, mipCount, mipResLog2, substeps;
	};
	VkBuffer bufBParams = CreateBuffer(ctx.dev, ctx.phys, sizeof(BParams),
									   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
									   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									   &memBParams);
	BParams bParams{WORKLOAD_B_CHUNKS, WORKLOAD_B_MIPS,
					WORKLOAD_B_MIP_RES_LOG2, WORKLOAD_B_SUBSTEPS};
	vkMapMemory(ctx.dev, memBParams, 0, sizeof(BParams), 0, &p);
	std::memcpy(p, &bParams, sizeof(BParams));
	vkUnmapMemory(ctx.dev, memBParams);

	// Init AABBs (deterministic synthetic data).
	vkMapMemory(ctx.dev, memBAabbs, 0,
				sizeof(ChunkAabb) * WORKLOAD_B_CHUNKS, 0, &p);
	auto *aabbsPtr = reinterpret_cast<ChunkAabb *>(p);
	for (uint32_t i = 0; i < WORKLOAD_B_CHUNKS; ++i) {
		aabbsPtr[i].minX = (i % 64u) * 4.0f;
		aabbsPtr[i].minY = ((i / 64u) % 64u) * 4.0f;
		aabbsPtr[i].minZ = (i / 4096u) * 4.0f;
		aabbsPtr[i].maxX = aabbsPtr[i].minX + 2.0f;
		aabbsPtr[i].maxY = aabbsPtr[i].minY + 2.0f;
		aabbsPtr[i].maxZ = aabbsPtr[i].minZ + 4.0f;
		aabbsPtr[i].pad0 = aabbsPtr[i].pad1 = 0.0f;
	}
	vkUnmapMemory(ctx.dev, memBAabbs);

	// Init HZB (deterministic).
	vkMapMemory(ctx.dev, memBHzb, 0, sizeof(uint32_t) * hzbTotalPixels, 0, &p);
	auto *hzbPtr = reinterpret_cast<uint32_t *>(p);
	for (uint32_t i = 0; i < hzbTotalPixels; ++i) {
		hzbPtr[i] = (i * 2654435761u) & 0xFFFFu;
	}
	vkUnmapMemory(ctx.dev, memBHzb);

	VkShaderModule smB = CreateShaderModule(ctx.dev,
											reinterpret_cast<const uint8_t *>(shaders_compute_b_comp_spv),
											shaders_compute_b_comp_spv_len);

	VkDescriptorSetLayoutBinding bb[4]{};
	bb[0].binding = 0;
	bb[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bb[0].descriptorCount = 1;
	bb[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bb[1].binding = 1;
	bb[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bb[1].descriptorCount = 1;
	bb[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bb[2].binding = 2;
	bb[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bb[2].descriptorCount = 1;
	bb[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	bb[3].binding = 3;
	bb[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bb[3].descriptorCount = 1;
	bb[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo bDslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	bDslci.bindingCount = 4;
	bDslci.pBindings = bb;
	VkDescriptorSetLayout bDsl = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(ctx.dev, &bDslci, nullptr, &bDsl));

	VkPipelineLayoutCreateInfo bPlci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	bPlci.setLayoutCount = 1;
	bPlci.pSetLayouts = &bDsl;
	VkPipelineLayout bPl = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(ctx.dev, &bPlci, nullptr, &bPl));

	VkComputePipelineCreateInfo bCpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	bCpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	bCpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	bCpci.stage.module = smB;
	bCpci.stage.pName = "main";
	bCpci.layout = bPl;
	VkPipeline pipeB = VK_NULL_HANDLE;
	VK_CHECK(vkCreateComputePipelines(ctx.dev, VK_NULL_HANDLE, 1, &bCpci, nullptr, &pipeB));

	VkDescriptorPoolSize bps[2]{};
	bps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bps[0].descriptorCount = 3;
	bps[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bps[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo bpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	bpci.maxSets = 1;
	bpci.poolSizeCount = 2;
	bpci.pPoolSizes = bps;
	VkDescriptorPool bPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(ctx.dev, &bpci, nullptr, &bPool));

	VkDescriptorSetAllocateInfo bsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	bsai.descriptorPool = bPool;
	bsai.descriptorSetCount = 1;
	bsai.pSetLayouts = &bDsl;
	VkDescriptorSet bSet = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(ctx.dev, &bsai, &bSet));

	VkDescriptorBufferInfo bAi{bufBAabbs, 0, sizeof(ChunkAabb) * WORKLOAD_B_CHUNKS};
	VkDescriptorBufferInfo bHi{bufBHzb, 0, sizeof(uint32_t) * hzbTotalPixels};
	VkDescriptorBufferInfo bMi{bufBMask, 0, sizeof(uint32_t) * WORKLOAD_B_CHUNKS};
	VkDescriptorBufferInfo bPi{bufBParams, 0, sizeof(BParams)};

	VkWriteDescriptorSet bw[4]{};
	bw[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bw[0].dstSet = bSet;
	bw[0].dstBinding = 0;
	bw[0].descriptorCount = 1;
	bw[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bw[0].pBufferInfo = &bAi;
	bw[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bw[1].dstSet = bSet;
	bw[1].dstBinding = 1;
	bw[1].descriptorCount = 1;
	bw[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bw[1].pBufferInfo = &bHi;
	bw[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bw[2].dstSet = bSet;
	bw[2].dstBinding = 2;
	bw[2].descriptorCount = 1;
	bw[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bw[2].pBufferInfo = &bMi;
	bw[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bw[3].dstSet = bSet;
	bw[3].dstBinding = 3;
	bw[3].descriptorCount = 1;
	bw[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bw[3].pBufferInfo = &bPi;
	vkUpdateDescriptorSets(ctx.dev, 4, bw, 0, nullptr);

	std::printf("Workload B: HZB cull %u chunks mips=%u (medium)\n",
				WORKLOAD_B_CHUNKS, WORKLOAD_B_MIPS);

	// ---- Workload C: Fluid CA ping-pong (heavy) ----
	uint32_t cVoxels = WORKLOAD_C_SIZE * WORKLOAD_C_SIZE * WORKLOAD_C_SIZE;
	VkDeviceMemory memC0, memC1, memCParams;
	VkBuffer bufC0 = CreateBuffer(ctx.dev, ctx.phys,
								  sizeof(uint32_t) * cVoxels,
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								  &memC0);
	VkBuffer bufC1 = CreateBuffer(ctx.dev, ctx.phys,
								  sizeof(uint32_t) * cVoxels,
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
								  &memC1);
	struct CParams {
		uint32_t sizeX, sizeY, sizeZ, substeps;
	};
	VkBuffer bufCParams = CreateBuffer(ctx.dev, ctx.phys, sizeof(CParams),
									   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
									   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
										   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
									   &memCParams);
	CParams cParams{WORKLOAD_C_SIZE, WORKLOAD_C_SIZE, WORKLOAD_C_SIZE,
					WORKLOAD_C_SUBSTEPS};
	vkMapMemory(ctx.dev, memCParams, 0, sizeof(CParams), 0, &p);
	std::memcpy(p, &cParams, sizeof(CParams));
	vkUnmapMemory(ctx.dev, memCParams);

	VkShaderModule smC = CreateShaderModule(ctx.dev,
											reinterpret_cast<const uint8_t *>(shaders_compute_c_comp_spv),
											shaders_compute_c_comp_spv_len);

	VkDescriptorSetLayoutBinding cb[3]{};
	cb[0].binding = 0;
	cb[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cb[0].descriptorCount = 1;
	cb[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cb[1].binding = 1;
	cb[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cb[1].descriptorCount = 1;
	cb[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	cb[2].binding = 2;
	cb[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cb[2].descriptorCount = 1;
	cb[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo cDslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
	cDslci.bindingCount = 3;
	cDslci.pBindings = cb;
	VkDescriptorSetLayout cDsl = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorSetLayout(ctx.dev, &cDslci, nullptr, &cDsl));

	VkPipelineLayoutCreateInfo cPlci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	cPlci.setLayoutCount = 1;
	cPlci.pSetLayouts = &cDsl;
	VkPipelineLayout cPl = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(ctx.dev, &cPlci, nullptr, &cPl));

	VkComputePipelineCreateInfo cCpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
	cCpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cCpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cCpci.stage.module = smC;
	cCpci.stage.pName = "main";
	cCpci.layout = cPl;
	VkPipeline pipeC = VK_NULL_HANDLE;
	VK_CHECK(vkCreateComputePipelines(ctx.dev, VK_NULL_HANDLE, 1, &cCpci, nullptr, &pipeC));

	VkDescriptorPoolSize cps[2]{};
	cps[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cps[0].descriptorCount = 2;
	cps[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cps[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo cpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
	cpci.maxSets = 1;
	cpci.poolSizeCount = 2;
	cpci.pPoolSizes = cps;
	VkDescriptorPool cPool = VK_NULL_HANDLE;
	VK_CHECK(vkCreateDescriptorPool(ctx.dev, &cpci, nullptr, &cPool));

	VkDescriptorSetAllocateInfo csai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
	csai.descriptorPool = cPool;
	csai.descriptorSetCount = 1;
	csai.pSetLayouts = &cDsl;
	VkDescriptorSet cSet = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateDescriptorSets(ctx.dev, &csai, &cSet));

	VkDescriptorBufferInfo cInI{bufC0, 0, sizeof(uint32_t) * cVoxels};
	VkDescriptorBufferInfo cOutI{bufC1, 0, sizeof(uint32_t) * cVoxels};
	VkDescriptorBufferInfo cPI{bufCParams, 0, sizeof(CParams)};

	VkWriteDescriptorSet cw[3]{};
	cw[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cw[0].dstSet = cSet;
	cw[0].dstBinding = 0;
	cw[0].descriptorCount = 1;
	cw[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cw[0].pBufferInfo = &cInI;
	cw[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cw[1].dstSet = cSet;
	cw[1].dstBinding = 1;
	cw[1].descriptorCount = 1;
	cw[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cw[1].pBufferInfo = &cOutI;
	cw[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	cw[2].dstSet = cSet;
	cw[2].dstBinding = 2;
	cw[2].descriptorCount = 1;
	cw[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cw[2].pBufferInfo = &cPI;
	vkUpdateDescriptorSets(ctx.dev, 3, cw, 0, nullptr);

	std::printf("Workload C: Fluid CA %u^3 substeps=%u (heavy)\n",
				WORKLOAD_C_SIZE, WORKLOAD_C_SUBSTEPS);

	// ---- Graphics dummy pass (headless via dynamic rendering) ----
	// Offscreen color target; we don't present — just measure GPU time.
	constexpr uint32_t kGfxWidth = 1920;
	constexpr uint32_t kGfxHeight = 1080;

	VkDeviceMemory memGfxColor;
	VkImage imgGfxColor = CreateImage3D(ctx.dev, ctx.phys, kGfxWidth, kGfxHeight,
										1, VK_FORMAT_R8G8B8A8_UNORM,
										VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
											VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										&memGfxColor);
	// Note: CreateImage3D works for 2D images too with extent.depth=1.
	// Actually it's VK_IMAGE_TYPE_3D — we want 2D for color attachment.
	// Recreate as 2D.
	vkDestroyImage(ctx.dev, imgGfxColor, nullptr);
	vkFreeMemory(ctx.dev, memGfxColor, nullptr);

	VkImageCreateInfo ici2d{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	ici2d.imageType = VK_IMAGE_TYPE_2D;
	ici2d.format = VK_FORMAT_R8G8B8A8_UNORM;
	ici2d.extent = {kGfxWidth, kGfxHeight, 1};
	ici2d.mipLevels = 1;
	ici2d.arrayLayers = 1;
	ici2d.samples = VK_SAMPLE_COUNT_1_BIT;
	ici2d.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici2d.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	ici2d.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici2d.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkImage imgGfx = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImage(ctx.dev, &ici2d, nullptr, &imgGfx));

	VkMemoryRequirements greq;
	vkGetImageMemoryRequirements(ctx.dev, imgGfx, &greq);
	VkMemoryAllocateInfo gmai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	gmai.allocationSize = greq.size;
	gmai.memoryTypeIndex = FindMemoryType(ctx.phys, greq.memoryTypeBits,
										  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VkDeviceMemory memGfx = VK_NULL_HANDLE;
	VK_CHECK(vkAllocateMemory(ctx.dev, &gmai, nullptr, &memGfx));
	VK_CHECK(vkBindImageMemory(ctx.dev, imgGfx, memGfx, 0));

	VkImageViewCreateInfo gvci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	gvci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	gvci.format = VK_FORMAT_R8G8B8A8_UNORM;
	gvci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	gvci.image = imgGfx;
	VkImageView viewGfx = VK_NULL_HANDLE;
	VK_CHECK(vkCreateImageView(ctx.dev, &gvci, nullptr, &viewGfx));

	VkShaderModule smVert = CreateShaderModule(ctx.dev,
											   reinterpret_cast<const uint8_t *>(shaders_graphics_vert_spv),
											   shaders_graphics_vert_spv_len);
	VkShaderModule smFrag = CreateShaderModule(ctx.dev,
											   reinterpret_cast<const uint8_t *>(shaders_graphics_frag_spv),
											   shaders_graphics_frag_spv_len);

	VkPipelineShaderStageCreateInfo gfxStages[2]{};
	gfxStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gfxStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	gfxStages[0].module = smVert;
	gfxStages[0].pName = "main";
	gfxStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	gfxStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	gfxStages[1].module = smFrag;
	gfxStages[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo gvi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

	VkPipelineInputAssemblyStateCreateInfo gia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
	gia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo gvp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
	gvp.viewportCount = 1;
	gvp.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo grs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
	grs.polygonMode = VK_POLYGON_MODE_FILL;
	grs.cullMode = VK_CULL_MODE_NONE;
	grs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	grs.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo gms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
	gms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState gcba{};
	gcba.colorWriteMask = 0xF;

	VkPipelineColorBlendStateCreateInfo gcbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
	gcbs.attachmentCount = 1;
	gcbs.pAttachments = &gcba;

	VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
								  VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo gds{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
	gds.dynamicStateCount = 2;
	gds.pDynamicStates = dynStates;

	VkPipelineLayoutCreateInfo gplci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	VkPipelineLayout gfxPl = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(ctx.dev, &gplci, nullptr, &gfxPl));

	VkPipelineRenderingCreateInfo prc{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
	prc.colorAttachmentCount = 1;
	VkFormat colorFmt = VK_FORMAT_R8G8B8A8_UNORM;
	prc.pColorAttachmentFormats = &colorFmt;

	VkGraphicsPipelineCreateInfo gpci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	gpci.pNext = &prc;
	gpci.stageCount = 2;
	gpci.pStages = gfxStages;
	gpci.pVertexInputState = &gvi;
	gpci.pInputAssemblyState = &gia;
	gpci.pViewportState = &gvp;
	gpci.pRasterizationState = &grs;
	gpci.pMultisampleState = &gms;
	gpci.pColorBlendState = &gcbs;
	gpci.pDynamicState = &gds;
	gpci.layout = gfxPl;
	gpci.renderPass = VK_NULL_HANDLE; // dynamic rendering
	VkPipeline pipeGfx = VK_NULL_HANDLE;
	VK_CHECK(vkCreateGraphicsPipelines(ctx.dev, VK_NULL_HANDLE, 1, &gpci, nullptr, &pipeGfx));

	std::printf("Graphics: dynamic rendering, %ux%u color-only (dummy)\n",
				kGfxWidth, kGfxHeight);

	// ---- Command pool + per-frame command buffers + fences ----
	VkCommandPoolCreateInfo cmdPoolCi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	cmdPoolCi.queueFamilyIndex = ctx.qi.graphics;
	cmdPoolCi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VkCommandPool cmdPoolGfx = VK_NULL_HANDLE;
	VK_CHECK(vkCreateCommandPool(ctx.dev, &cmdPoolCi, nullptr, &cmdPoolGfx));

	VkCommandPool cmdPoolCompute = VK_NULL_HANDLE;
	if (ctx.qi.compute != ctx.qi.graphics) {
		cmdPoolCi.queueFamilyIndex = ctx.qi.compute;
		VK_CHECK(vkCreateCommandPool(ctx.dev, &cmdPoolCi, nullptr, &cmdPoolCompute));
	} else {
		cmdPoolCompute = cmdPoolGfx;
	}

	// Per-frame command buffers + fences.
	struct Frame {
		VkCommandBuffer cbGfx = VK_NULL_HANDLE;
		VkCommandBuffer cbCompute = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
	};
	std::vector<Frame> fr(frames);
	for (uint32_t i = 0; i < frames; ++i) {
		VkCommandBufferAllocateInfo cbaiGfx{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cbaiGfx.commandPool = cmdPoolGfx;
		cbaiGfx.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbaiGfx.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(ctx.dev, &cbaiGfx, &fr[i].cbGfx));

		VkCommandBufferAllocateInfo cbaiComp{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		cbaiComp.commandPool = cmdPoolCompute;
		cbaiComp.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbaiComp.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(ctx.dev, &cbaiComp, &fr[i].cbCompute));

		VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		VK_CHECK(vkCreateFence(ctx.dev, &fci, nullptr, &fr[i].fence));
	}

	// Helper to record graphics CB (dynamic rendering).
	auto RecordGfx = [&](VkCommandBuffer cb, uint32_t frameIdx) {
		VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(cb, &cbbi));

		// Timestamp at top of graphics.
		uint32_t tsBase = (frameIdx % frames) * 4 + 0; // 0,1 = gfx; 2,3 = compute start/end.
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ctx.tsPool, tsBase);

		// Image layout transition for color attachment.
		VkImageMemoryBarrier imb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		imb.srcAccessMask = 0;
		imb.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imb.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imb.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imb.image = imgGfx;
		imb.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &imb);

		VkRenderingAttachmentInfo rai{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		rai.imageView = viewGfx;
		rai.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		rai.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		rai.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		VkClearValue clear{};
		clear.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
		rai.clearValue = clear;

		VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
		ri.renderArea = {{0, 0}, {kGfxWidth, kGfxHeight}};
		ri.layerCount = 1;
		ri.colorAttachmentCount = 1;
		ri.pColorAttachments = &rai;

		vkCmdBeginRendering(cb, &ri);
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeGfx);
		VkViewport vp{0.0f, 0.0f, float(kGfxWidth), float(kGfxHeight), 0.0f, 1.0f};
		VkRect2D sc{{0, 0}, {kGfxWidth, kGfxHeight}};
		vkCmdSetViewport(cb, 0, 1, &vp);
		vkCmdSetScissor(cb, 0, 1, &sc);
		vkCmdDraw(cb, 3, 1, 0, 0);
		vkCmdEndRendering(cb);

		// Layout transition back to undefined (we discard).
		imb.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		imb.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imb.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		imb.dstAccessMask = 0;
		vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
							 VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							 0, 0, nullptr, 0, nullptr, 1, &imb);

		// Timestamp at bottom of graphics.
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ctx.tsPool, tsBase + 1);

		VK_CHECK(vkEndCommandBuffer(cb));
	};

	// Helper to record compute CB (3 dispatches, with GPU timestamps).
	constexpr uint32_t kComputeMultiplier = 16; // dispatch each workload N times to make GPU time measurable.

	auto RecordCompute = [&](VkCommandBuffer cb, uint32_t frameIdx) {
		VkCommandBufferBeginInfo cbbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VK_CHECK(vkBeginCommandBuffer(cb, &cbbi));

		// Timestamp at top of compute.
		uint32_t tsBase = (frameIdx % frames) * 4 + 2; // 0,1 = gfx; 2,3 = compute start/end.
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, ctx.tsPool, tsBase);

		for (uint32_t rep = 0; rep < kComputeMultiplier; ++rep) {
			// Workload A: 64^3, workgroup 8^3.
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeA);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, aPl, 0, 1, &aSet, 0, nullptr);
			vkCmdDispatch(cb, WORKLOAD_A_SIZE / 8, WORKLOAD_A_SIZE / 8, WORKLOAD_A_SIZE / 8);

			// Workload B: 4096 chunks / 64 = 64 workgroups.
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeB);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, bPl, 0, 1, &bSet, 0, nullptr);
			vkCmdDispatch(cb, (WORKLOAD_B_CHUNKS + 63) / 64, 1, 1);

			// Workload C: 64^3, workgroup 8^3.
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipeC);
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, cPl, 0, 1, &cSet, 0, nullptr);
			vkCmdDispatch(cb, WORKLOAD_C_SIZE / 8, WORKLOAD_C_SIZE / 8, WORKLOAD_C_SIZE / 8);
		}

		// Timestamp at bottom of compute.
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ctx.tsPool, tsBase + 1);

		VK_CHECK(vkEndCommandBuffer(cb));
	};

	auto RunMode = [&](const char *modeName, bool async) {
		std::printf("\n--- Mode: %s (async=%s) ---\n", modeName, async ? "yes" : "no");

		// Reset semaphores to 0.
		VkSemaphoreWaitInfo swi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
		swi.semaphoreCount = 2;
		VkSemaphore sems[2] = {ctx.graphicsDone, ctx.computeDone};
		uint64_t vals[2] = {0, 0};
		swi.pSemaphores = sems;
		swi.pValues = vals;
		vkWaitSemaphores(ctx.dev, &swi, UINT64_MAX);
		(void)swi;

		// Warmup.
		for (uint32_t i = 0; i < WARMUP_FRAMES; ++i) {
			vkResetFences(ctx.dev, 1, &fr[i % frames].fence);
			RecordGfx(fr[i % frames].cbGfx, i);
			RecordCompute(fr[i % frames].cbCompute, i);

			if (async) {
				// Submit graphics: signal graphicsDone=1.
				uint64_t nextG = 1, nextC = 0;
				VkTimelineSemaphoreSubmitInfo gtsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				gtsi.signalSemaphoreValueCount = 1;
				gtsi.pSignalSemaphoreValues = &nextG;
				VkSubmitInfo gsi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				gsi.pNext = &gtsi;
				gsi.commandBufferCount = 1;
				gsi.pCommandBuffers = &fr[i % frames].cbGfx;
				gsi.signalSemaphoreCount = 1;
				gsi.pSignalSemaphores = &ctx.graphicsDone;
				VK_CHECK(vkQueueSubmit(ctx.graphicsQ, 1, &gsi, VK_NULL_HANDLE));

				// Submit compute: wait graphicsDone>=1, signal computeDone=1.
				uint64_t waitG = 1, sigC = 1;
				VkTimelineSemaphoreSubmitInfo ctsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				ctsi.waitSemaphoreValueCount = 1;
				ctsi.pWaitSemaphoreValues = &waitG;
				ctsi.signalSemaphoreValueCount = 1;
				ctsi.pSignalSemaphoreValues = &sigC;
				VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				VkSubmitInfo csi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				csi.pNext = &ctsi;
				csi.waitSemaphoreCount = 1;
				csi.pWaitSemaphores = &ctx.graphicsDone;
				csi.pWaitDstStageMask = &waitStage;
				csi.commandBufferCount = 1;
				csi.pCommandBuffers = &fr[i % frames].cbCompute;
				csi.signalSemaphoreCount = 1;
				csi.pSignalSemaphores = &ctx.computeDone;
				VK_CHECK(vkQueueSubmit(ctx.computeQ, 1, &csi, fr[i % frames].fence));
			} else {
				// Sequential: graphics first, then compute on SAME queue.
				VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				VkSemaphore depSem = ctx.graphicsDone;

				uint64_t nextG = 1;
				VkTimelineSemaphoreSubmitInfo gtsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				gtsi.signalSemaphoreValueCount = 1;
				gtsi.pSignalSemaphoreValues = &nextG;
				VkSubmitInfo gsi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				gsi.pNext = &gtsi;
				gsi.commandBufferCount = 1;
				gsi.pCommandBuffers = &fr[i % frames].cbGfx;
				gsi.signalSemaphoreCount = 1;
				gsi.pSignalSemaphores = &ctx.graphicsDone;
				VK_CHECK(vkQueueSubmit(ctx.graphicsQ, 1, &gsi, VK_NULL_HANDLE));

				uint64_t waitG = 1;
				VkTimelineSemaphoreSubmitInfo ctsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				ctsi.waitSemaphoreValueCount = 1;
				ctsi.pWaitSemaphoreValues = &waitG;
				VkSubmitInfo csi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				csi.pNext = &ctsi;
				csi.waitSemaphoreCount = 1;
				csi.pWaitSemaphores = &depSem;
				csi.pWaitDstStageMask = &waitStage;
				csi.commandBufferCount = 1;
				csi.pCommandBuffers = &fr[i % frames].cbCompute;
				VK_CHECK(vkQueueSubmit(ctx.computeQ == ctx.graphicsQ ? ctx.graphicsQ : ctx.computeQ,
									   1, &csi, fr[i % frames].fence));
			}

			VK_CHECK(vkWaitForFences(ctx.dev, 1, &fr[i % frames].fence, VK_TRUE, UINT64_MAX));
		}

		// Reset semaphores between warmup and measure.
		uint64_t vals2[2] = {0, 0};
		swi.pValues = vals2;
		vkWaitSemaphores(ctx.dev, &swi, UINT64_MAX);

		// Measure.
		std::vector<double> frameTimesMs(frames, 0.0);
		std::vector<double> gpuGfxTimesMs(frames, 0.0);
		std::vector<double> gpuComputeTimesMs(frames, 0.0);
		for (uint32_t i = 0; i < frames; ++i) {
			vkResetFences(ctx.dev, 1, &fr[i].fence);
			RecordGfx(fr[i].cbGfx, i);
			RecordCompute(fr[i].cbCompute, i);

			auto t0 = std::chrono::steady_clock::now();

			if (async) {
				uint64_t nextG = 1, nextC = 0;
				VkTimelineSemaphoreSubmitInfo gtsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				gtsi.signalSemaphoreValueCount = 1;
				gtsi.pSignalSemaphoreValues = &nextG;
				VkSubmitInfo gsi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				gsi.pNext = &gtsi;
				gsi.commandBufferCount = 1;
				gsi.pCommandBuffers = &fr[i].cbGfx;
				gsi.signalSemaphoreCount = 1;
				gsi.pSignalSemaphores = &ctx.graphicsDone;
				VK_CHECK(vkQueueSubmit(ctx.graphicsQ, 1, &gsi, VK_NULL_HANDLE));

				uint64_t waitG = 1, sigC = 1;
				VkTimelineSemaphoreSubmitInfo ctsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				ctsi.waitSemaphoreValueCount = 1;
				ctsi.pWaitSemaphoreValues = &waitG;
				ctsi.signalSemaphoreValueCount = 1;
				ctsi.pSignalSemaphoreValues = &sigC;
				VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				VkSubmitInfo csi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				csi.pNext = &ctsi;
				csi.waitSemaphoreCount = 1;
				csi.pWaitSemaphores = &ctx.graphicsDone;
				csi.pWaitDstStageMask = &waitStage;
				csi.commandBufferCount = 1;
				csi.pCommandBuffers = &fr[i].cbCompute;
				csi.signalSemaphoreCount = 1;
				csi.pSignalSemaphores = &ctx.computeDone;
				VK_CHECK(vkQueueSubmit(ctx.computeQ, 1, &csi, fr[i].fence));
			} else {
				uint64_t nextG = 1;
				VkTimelineSemaphoreSubmitInfo gtsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				gtsi.signalSemaphoreValueCount = 1;
				gtsi.pSignalSemaphoreValues = &nextG;
				VkSubmitInfo gsi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				gsi.pNext = &gtsi;
				gsi.commandBufferCount = 1;
				gsi.pCommandBuffers = &fr[i].cbGfx;
				gsi.signalSemaphoreCount = 1;
				gsi.pSignalSemaphores = &ctx.graphicsDone;
				VK_CHECK(vkQueueSubmit(ctx.graphicsQ, 1, &gsi, VK_NULL_HANDLE));

				uint64_t waitG = 1;
				VkTimelineSemaphoreSubmitInfo ctsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
				ctsi.waitSemaphoreValueCount = 1;
				ctsi.pWaitSemaphoreValues = &waitG;
				VkSubmitInfo csi{VK_STRUCTURE_TYPE_SUBMIT_INFO};
				csi.pNext = &ctsi;
				VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
				VkSemaphore depSem = ctx.graphicsDone;
				csi.waitSemaphoreCount = 1;
				csi.pWaitSemaphores = &depSem;
				csi.pWaitDstStageMask = &waitStage;
				csi.commandBufferCount = 1;
				csi.pCommandBuffers = &fr[i].cbCompute;
				VK_CHECK(vkQueueSubmit(ctx.computeQ == ctx.graphicsQ ? ctx.graphicsQ : ctx.computeQ,
									   1, &csi, fr[i].fence));
			}

			VK_CHECK(vkWaitForFences(ctx.dev, 1, &fr[i].fence, VK_TRUE, UINT64_MAX));
			auto t1 = std::chrono::steady_clock::now();
			double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
			frameTimesMs[i] = ms;

			// Read GPU timestamps for this frame.
			uint64_t ts[4] = {0, 0, 0, 0};
			VK_CHECK(vkGetQueryPoolResults(ctx.dev, ctx.tsPool, i * 4, 4,
										   sizeof(ts), ts, sizeof(uint64_t),
										   VK_QUERY_RESULT_64_BIT |
											   VK_QUERY_RESULT_WAIT_BIT));
			double gfxMs = (ts[1] - ts[0]) * tsPeriod / 1e6;
			double computeMs = (ts[3] - ts[2]) * tsPeriod / 1e6;
			gpuGfxTimesMs[i] = gfxMs;
			gpuComputeTimesMs[i] = computeMs;
		}

		Stats s = ComputeStats(frameTimesMs);
		Stats sg = ComputeStats(gpuGfxTimesMs);
		Stats sc = ComputeStats(gpuComputeTimesMs);
		std::printf("Frame time (CPU wall clock incl submit): %s ms\n",
					FormatStats(s).c_str());
		std::printf("GPU time graphics:                        %s ms\n",
					FormatStats(sg).c_str());
		std::printf("GPU time compute (x%d):                   %s ms\n",
					kComputeMultiplier, FormatStats(sc).c_str());
		std::printf("GPU time total = graphics + compute:      %.3f ms\n",
					sg.mean + sc.mean);
		s.gpuGfx = sg.mean;
		s.gpuCompute = sc.mean;
		return s;
	};

	std::vector<std::string> csvRows;
	csvRows.push_back("mode,workload,frames,mean_ms,median_ms,p95_ms,p99_ms,stddev_ms,min_ms,max_ms");

	Stats sSeq{}, sAsync{};
	if (mode == "seq" || mode == "both") {
		sSeq = RunMode("sequential", false);
		csvRows.push_back("seq,all," + std::to_string(frames) + "," +
						  std::to_string(sSeq.mean) + "," +
						  std::to_string(sSeq.median) + "," +
						  std::to_string(sSeq.p95) + "," +
						  std::to_string(sSeq.p99) + "," +
						  std::to_string(sSeq.stddev) + "," +
						  std::to_string(sSeq.min) + "," +
						  std::to_string(sSeq.max));
	}
	if (mode == "async" || mode == "both") {
		sAsync = RunMode("async", true);
		csvRows.push_back("async,all," + std::to_string(frames) + "," +
						  std::to_string(sAsync.mean) + "," +
						  std::to_string(sAsync.median) + "," +
						  std::to_string(sAsync.p95) + "," +
						  std::to_string(sAsync.p99) + "," +
						  std::to_string(sAsync.stddev) + "," +
						  std::to_string(sAsync.min) + "," +
						  std::to_string(sAsync.max));
	}

	if ((mode == "both" || mode == "seq" || mode == "async") &&
		(mode == "both")) {
		double speedup = sSeq.mean > 0 ? (sSeq.mean - sAsync.mean) / sSeq.mean * 100.0 : 0.0;
		std::printf("\n=== OVERLAP GAIN ===\nseq mean: %.3f ms\nasync mean: %.3f ms\nspeedup: %.2f%%\n",
					sSeq.mean, sAsync.mean, speedup);
		csvRows.push_back("overlap,gain,," + std::to_string(speedup) + ",,,,,,");
	}

	WriteCsv(csvOut, csvRows);
	std::printf("\nWrote %s\n", csvOut.c_str());

	// Cleanup.
	for (auto &f : fr) {
		vkDestroyFence(ctx.dev, f.fence, nullptr);
	}
	vkDestroyCommandPool(ctx.dev, cmdPoolGfx, nullptr);
	if (cmdPoolCompute != cmdPoolGfx) {
		vkDestroyCommandPool(ctx.dev, cmdPoolCompute, nullptr);
	}
	vkDestroyPipeline(ctx.dev, pipeA, nullptr);
	vkDestroyPipeline(ctx.dev, pipeB, nullptr);
	vkDestroyPipeline(ctx.dev, pipeC, nullptr);
	vkDestroyPipeline(ctx.dev, pipeGfx, nullptr);
	vkDestroyPipelineLayout(ctx.dev, aPl, nullptr);
	vkDestroyPipelineLayout(ctx.dev, bPl, nullptr);
	vkDestroyPipelineLayout(ctx.dev, cPl, nullptr);
	vkDestroyPipelineLayout(ctx.dev, gfxPl, nullptr);
	vkDestroyShaderModule(ctx.dev, smA, nullptr);
	vkDestroyShaderModule(ctx.dev, smB, nullptr);
	vkDestroyShaderModule(ctx.dev, smC, nullptr);
	vkDestroyShaderModule(ctx.dev, smVert, nullptr);
	vkDestroyShaderModule(ctx.dev, smFrag, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, aDsl, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, bDsl, nullptr);
	vkDestroyDescriptorSetLayout(ctx.dev, cDsl, nullptr);
	vkDestroyDescriptorPool(ctx.dev, aPool, nullptr);
	vkDestroyDescriptorPool(ctx.dev, bPool, nullptr);
	vkDestroyDescriptorPool(ctx.dev, cPool, nullptr);
	vkDestroyBuffer(ctx.dev, bufAParams, nullptr);
	vkDestroyBuffer(ctx.dev, bufBAabbs, nullptr);
	vkDestroyBuffer(ctx.dev, bufBHzb, nullptr);
	vkDestroyBuffer(ctx.dev, bufBMask, nullptr);
	vkDestroyBuffer(ctx.dev, bufBParams, nullptr);
	vkDestroyBuffer(ctx.dev, bufC0, nullptr);
	vkDestroyBuffer(ctx.dev, bufC1, nullptr);
	vkDestroyBuffer(ctx.dev, bufCParams, nullptr);
	vkFreeMemory(ctx.dev, memA0, nullptr);
	vkFreeMemory(ctx.dev, memA1, nullptr);
	vkFreeMemory(ctx.dev, memAParams, nullptr);
	vkFreeMemory(ctx.dev, memBAabbs, nullptr);
	vkFreeMemory(ctx.dev, memBHzb, nullptr);
	vkFreeMemory(ctx.dev, memBMask, nullptr);
	vkFreeMemory(ctx.dev, memBParams, nullptr);
	vkFreeMemory(ctx.dev, memC0, nullptr);
	vkFreeMemory(ctx.dev, memC1, nullptr);
	vkFreeMemory(ctx.dev, memCParams, nullptr);
	vkDestroyImage(ctx.dev, imgA0, nullptr);
	vkDestroyImage(ctx.dev, imgA1, nullptr);
	vkDestroyImageView(ctx.dev, viewA0, nullptr);
	vkDestroyImageView(ctx.dev, viewA1, nullptr);
	vkDestroyImage(ctx.dev, imgGfx, nullptr);
	vkDestroyImageView(ctx.dev, viewGfx, nullptr);
	vkFreeMemory(ctx.dev, memGfx, nullptr);
	vkDestroyQueryPool(ctx.dev, ctx.tsPool, nullptr);
	vkDestroySemaphore(ctx.dev, ctx.graphicsDone, nullptr);
	vkDestroySemaphore(ctx.dev, ctx.computeDone, nullptr);
	vkDestroyDevice(ctx.dev, nullptr);
	vkDestroyInstance(ctx.instance, nullptr);

	return 0;
}