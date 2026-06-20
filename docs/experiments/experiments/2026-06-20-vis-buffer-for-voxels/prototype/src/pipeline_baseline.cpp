// Combined baseline + vis-buffer pipeline setup and recording.
// Both share scene input (SSBOs) and render targets (color + depth).
// Baseline: forward+ inline (1 main pass + N shadow passes).
// Vis-buffer: 1 geometry pass (writes vis + depth) + N fullscreen resolve passes.

#include "pipeline_baseline.hpp"
#include "vulkan_setup.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace vb {

namespace {

// Load SPIR-V from disk into a vector.
std::vector<uint32_t> ReadSPIRV(const char *path)
{
	FILE *f = std::fopen(path, "rb");
	if (!f) {
		std::fprintf(stderr, "ReadSPIRV: cannot open %s\n", path);
		return {};
	}
	std::fseek(f, 0, SEEK_END);
	long size = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	std::vector<uint32_t> buf(size / 4);
	if (std::fread(buf.data(), 4, buf.size(), f) != buf.size()) {
		std::fprintf(stderr, "ReadSPIRV: short read %s\n", path);
		std::fclose(f);
		return {};
	}
	std::fclose(f);
	return buf;
}

VkShaderModule LoadShaderFromFile(const VkContext &ctx, const char *path)
{
	auto code = ReadSPIRV(path);
	return LoadShaderSPIRV(ctx, code.data(), code.size() * 4);
}

constexpr VkDrawIndirectCommand MakeIndirect(uint32_t instanceCount)
{
	return VkDrawIndirectCommand{
		.vertexCount = 6, // 2 tris per quad, 3 verts each = 6 verts per instance.
		.instanceCount = instanceCount,
		.firstVertex = 0,
		.firstInstance = 0,
	};
}

} // namespace

bool BuildPipelines(VkContext &ctx, Pipeline &p, VkExtent2D extent,
					const std::vector<PackedFace> &faces,
					const std::vector<ChunkDescriptor> &chunks,
					const std::vector<MaterialVisual> &materials)
{
	p.extent = extent;

	if (faces.empty()) {
		std::fprintf(stderr, "BuildPipelines: empty faces\n");
		return false;
	}

	// ---- Upload scene SSBOs ----
	VkDeviceSize facesBytes = faces.size() * sizeof(PackedFace);
	VkDeviceSize chunksBytes = chunks.size() * sizeof(ChunkDescriptor);
	VkDeviceSize matsBytes = materials.size() * sizeof(MaterialVisual);

	p.packedFacesBuf = CreateBuffer(ctx, facesBytes,
									VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
									VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	p.chunkDescriptorsBuf = CreateBuffer(ctx, chunksBytes,
										 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
										 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	p.materialsBuf = CreateBuffer(ctx, std::max<VkDeviceSize>(matsBytes, 64),
								  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
								  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Upload via staging.
	auto uploadTo = [&](AllocatedBuffer &dst, const void *src, VkDeviceSize sz) {
		AllocatedBuffer staging = CreateBuffer(ctx, sz,
											   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
											   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
											   /*hostMap=*/true);
		std::memcpy(staging.mapped, src, sz);
		VkCommandBuffer cmd = BeginOneShot(ctx);
		VkBufferCopy bc{0, 0, sz};
		vkCmdCopyBuffer(cmd, staging.buffer, dst.buffer, 1, &bc);
		EndOneShot(ctx, cmd);
		DestroyBuffer(ctx, staging);
	};
	uploadTo(p.packedFacesBuf, faces.data(), facesBytes);
	uploadTo(p.chunkDescriptorsBuf, chunks.data(), chunksBytes);
	uploadTo(p.materialsBuf, materials.data(), matsBytes);

	// ---- Indirect draw buffer (single draw for all quads) ----
	p.indirectBuf = CreateBuffer(ctx, sizeof(VkDrawIndirectCommand),
								 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	{
		VkDrawIndirectCommand ic = MakeIndirect(uint32_t(faces.size()));
		uploadTo(p.indirectBuf, &ic, sizeof(ic));
	}

	// ---- Render targets ----
	p.colorRT = CreateImage2D(ctx, extent, VK_FORMAT_R8G8B8A8_UNORM,
							  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
							  VK_IMAGE_ASPECT_COLOR_BIT);
	p.depthRT = CreateImage2D(ctx, extent, VK_FORMAT_D32_SFLOAT,
							  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
							  VK_IMAGE_ASPECT_DEPTH_BIT);
	p.visRT = CreateImage2D(ctx, extent, VK_FORMAT_R32_UINT,
							VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
							VK_IMAGE_ASPECT_COLOR_BIT);

	// ---- Readback buffer (host-visible, color copy destination) ----
	p.readbackBuf = CreateBuffer(ctx, extent.width * extent.height * 4,
								 VK_BUFFER_USAGE_TRANSFER_DST_BIT,
								 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
								 /*hostMap=*/true);

	// ---- Descriptor sets: one combined DS used by both pipelines ----
	// Layout: (set 0):
	//   binding 0: storage PackedFace[]
	//   binding 1: storage ChunkDescriptor[]
	//   binding 2: storage MaterialVisual[] (or unsized array)
	//   binding 3: uniform FrameUBO
	//   binding 4: combined sampler vis-buffer (R32_UINT, only used by resolve)

	// Build DescriptorSetLayout, PipelineLayout, DescriptorPool.
	VkDescriptorSetLayoutBinding bindings[5]{};
	bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
	bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr};
	bindings[2] = {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
	bindings[3] = {3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
	bindings[4] = {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};

	VkDescriptorSetLayoutCreateInfo dslci{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 5,
		.pBindings = bindings,
	};
	VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
	vkCreateDescriptorSetLayout(ctx.device, &dslci, nullptr, &dsl);

	VkPipelineLayoutCreateInfo plci{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &dsl,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = nullptr, // filled later
	};
	VkPushConstantRange pcr{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(float) * 16,
	};
	plci.pPushConstantRanges = &pcr;
	vkCreatePipelineLayout(ctx.device, &plci, nullptr, &p.baselineLayout);
	vkCreatePipelineLayout(ctx.device, &plci, nullptr, &p.visLayout);

	// Descriptor pool + set.
	VkDescriptorPoolSize poolSizes[3]{
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
	};
	VkDescriptorPoolCreateInfo dpci{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = 1,
		.poolSizeCount = 3,
		.pPoolSizes = poolSizes,
	};
	VkDescriptorPool dpool = VK_NULL_HANDLE;
	vkCreateDescriptorPool(ctx.device, &dpci, nullptr, &dpool);

	VkDescriptorSetAllocateInfo dsai{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = dpool,
		.descriptorSetCount = 1,
		.pSetLayouts = &dsl,
	};
	VkDescriptorSet ds = VK_NULL_HANDLE;
	vkAllocateDescriptorSets(ctx.device, &dsai, &ds);

	// Allocate UBO for FrameUBO.
	AllocatedBuffer frameUbo = CreateBuffer(ctx, 64,
											VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
											VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
											true);

	// ---- Sampler (for resolve) — must exist before descriptor write ----
	VkSamplerCreateInfo sci{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_NEAREST,
		.minFilter = VK_FILTER_NEAREST,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
	};
	VkSampler sampler = VK_NULL_HANDLE;
	vkCreateSampler(ctx.device, &sci, nullptr, &sampler);

	// Write descriptor set.
	VkDescriptorBufferInfo faceInfo{p.packedFacesBuf.buffer, 0, facesBytes};
	VkDescriptorBufferInfo chunkInfo{p.chunkDescriptorsBuf.buffer, 0, chunksBytes};
	VkDescriptorBufferInfo matInfo{p.materialsBuf.buffer, 0, std::max<VkDeviceSize>(matsBytes, 64)};
	VkDescriptorBufferInfo uboInfo{frameUbo.buffer, 0, 64};
	VkDescriptorImageInfo visInfo{};
	visInfo.sampler = sampler;
	visInfo.imageView = p.visRT.view;
	visInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet writes[5]{};
	writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writes[0].dstSet = ds;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &faceInfo;

	writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writes[1].dstSet = ds;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &chunkInfo;

	writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writes[2].dstSet = ds;
	writes[2].dstBinding = 2;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].pBufferInfo = &matInfo;

	writes[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writes[3].dstSet = ds;
	writes[3].dstBinding = 3;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writes[3].pBufferInfo = &uboInfo;

	writes[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	writes[4].dstSet = ds;
	writes[4].dstBinding = 4;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[4].pImageInfo = &visInfo;
	vkUpdateDescriptorSets(ctx.device, 5, writes, 0, nullptr);

	// ---- Sampler (for resolve) — created above before descriptor write ----

	// ---- Pipelines ----
	// Baseline graphics pipeline.
	{
		auto vsCode = ReadSPIRV("shaders/baseline.vert.spv");
		auto fsCode = ReadSPIRV("shaders/baseline.frag.spv");
		VkShaderModule vs = LoadShaderSPIRV(ctx, vsCode.data(), vsCode.size() * 4);
		VkShaderModule fs = LoadShaderSPIRV(ctx, fsCode.data(), fsCode.size() * 4);

		VkPipelineShaderStageCreateInfo stages[2]{
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", nullptr},
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", nullptr},
		};
		VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo ia{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};
		VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		VkPipelineRasterizationStateCreateInfo rs{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f,
		};
		VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
												.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
		VkPipelineDepthStencilStateCreateInfo ds{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
		};
		VkPipelineColorBlendAttachmentState cba{
			.colorWriteMask = 0xF,
		};
		VkPipelineColorBlendStateCreateInfo cb{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &cba,
		};
		VkDynamicState dynState = VK_DYNAMIC_STATE_VIEWPORT;
		VkPipelineDynamicStateCreateInfo dsi{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 1,
			.pDynamicStates = &dynState,
		};
		VkGraphicsPipelineCreateInfo gpci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = stages,
			.pVertexInputState = &vi,
			.pInputAssemblyState = &ia,
			.pViewportState = &vp,
			.pRasterizationState = &rs,
			.pMultisampleState = &ms,
			.pDepthStencilState = &ds,
			.pColorBlendState = &cb,
			.pDynamicState = &dsi,
			.layout = p.baselineLayout,
			.renderPass = VK_NULL_HANDLE, // dynamic rendering.
		};
		vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &p.baselineGraphics);
		vkDestroyShaderModule(ctx.device, vs, nullptr);
		vkDestroyShaderModule(ctx.device, fs, nullptr);
	}

	// Vis-buffer geometry pipeline.
	{
		auto vsCode = ReadSPIRV("shaders/visbuffer.vert.spv");
		auto fsCode = ReadSPIRV("shaders/visbuffer.frag.spv");
		VkShaderModule vs = LoadShaderSPIRV(ctx, vsCode.data(), vsCode.size() * 4);
		VkShaderModule fs = LoadShaderSPIRV(ctx, fsCode.data(), fsCode.size() * 4);

		VkPipelineShaderStageCreateInfo stages[2]{
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", nullptr},
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", nullptr},
		};
		VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo ia{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};
		VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		VkPipelineRasterizationStateCreateInfo rs{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f,
		};
		VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
												.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
		VkPipelineDepthStencilStateCreateInfo ds{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,
			.depthWriteEnable = VK_TRUE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
		};
		VkPipelineColorBlendAttachmentState cba{
			.colorWriteMask = 0xF,
		};
		VkPipelineColorBlendStateCreateInfo cb{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &cba,
		};
		VkDynamicState dynState = VK_DYNAMIC_STATE_VIEWPORT;
		VkPipelineDynamicStateCreateInfo dsi{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 1,
			.pDynamicStates = &dynState,
		};
		VkGraphicsPipelineCreateInfo gpci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = stages,
			.pVertexInputState = &vi,
			.pInputAssemblyState = &ia,
			.pViewportState = &vp,
			.pRasterizationState = &rs,
			.pMultisampleState = &ms,
			.pDepthStencilState = &ds,
			.pColorBlendState = &cb,
			.pDynamicState = &dsi,
			.layout = p.visLayout,
			.renderPass = VK_NULL_HANDLE,
		};
		vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &p.visGeometry);
		vkDestroyShaderModule(ctx.device, vs, nullptr);
		vkDestroyShaderModule(ctx.device, fs, nullptr);
	}

	// Vis-buffer resolve pipeline (fullscreen triangle).
	{
		auto vsCode = ReadSPIRV("shaders/fullscreen.vert.spv");
		auto fsCode = ReadSPIRV("shaders/resolve.frag.spv");
		VkShaderModule vs = LoadShaderSPIRV(ctx, vsCode.data(), vsCode.size() * 4);
		VkShaderModule fs = LoadShaderSPIRV(ctx, fsCode.data(), fsCode.size() * 4);

		VkPipelineShaderStageCreateInfo stages[2]{
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main", nullptr},
			{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main", nullptr},
		};
		VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		VkPipelineInputAssemblyStateCreateInfo ia{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		};
		VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		VkPipelineRasterizationStateCreateInfo rs{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.lineWidth = 1.0f,
		};
		VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
												.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
		VkPipelineDepthStencilStateCreateInfo ds{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
		};
		VkPipelineColorBlendAttachmentState cba{
			.colorWriteMask = 0xF,
		};
		VkPipelineColorBlendStateCreateInfo cb{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &cba,
		};
		VkDynamicState dynState = VK_DYNAMIC_STATE_VIEWPORT;
		VkPipelineDynamicStateCreateInfo dsi{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = 1,
			.pDynamicStates = &dynState,
		};
		VkGraphicsPipelineCreateInfo gpci{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = 2,
			.pStages = stages,
			.pVertexInputState = &vi,
			.pInputAssemblyState = &ia,
			.pViewportState = &vp,
			.pRasterizationState = &rs,
			.pMultisampleState = &ms,
			.pDepthStencilState = &ds,
			.pColorBlendState = &cb,
			.pDynamicState = &dsi,
			.layout = p.visLayout,
			.renderPass = VK_NULL_HANDLE,
		};
		vkCreateGraphicsPipelines(ctx.device, VK_NULL_HANDLE, 1, &gpci, nullptr, &p.visResolve);
		vkDestroyShaderModule(ctx.device, vs, nullptr);
		vkDestroyShaderModule(ctx.device, fs, nullptr);
	}

	std::fprintf(stderr, "Pipelines OK: faces=%zu chunks=%zu mats=%zu\n",
				 faces.size(), chunks.size(), materials.size());
	return true;
}

// Helper: write UBO with sun + camera + viewport.
struct FrameUBO {
	std::array<float, 4> sunDir;
	std::array<float, 4> sunColor;
	std::array<float, 4> camPos;
	std::array<float, 4> viewport;
};

namespace {

// Helper to record one full rendering pass into a command buffer with given pipelines + RTs.
// Returns cmd that we then submit.
void RecordMainPass(VkContext &ctx, Pipeline &p, VkCommandBuffer cmd,
					VkPipeline graphics, VkPipelineLayout layout,
					VkImageView colorView, VkImageView depthView,
					const std::array<float, 16> &viewProj,
					bool useVisRT)
{
	// Begin dynamic rendering.
	VkRenderingAttachmentInfo colorAtt{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = useVisRT ? p.visRT.view : colorView,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = useVisRT ? VkClearValue{.color = {{0xFF, 0xFF, 0xFF, 0xFF}}} : VkClearValue{.color = {{0, 0, 0, 0}}},
	};
	VkRenderingAttachmentInfo depthAtt{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = depthView,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = {.depthStencil = {1.0f, 0}},
	};
	VkRenderingInfo ri{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea = {{0, 0}, p.extent},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAtt,
		.pDepthAttachment = &depthAtt,
	};
	vkCmdBeginRendering(cmd, &ri);

	VkViewport vp{
		.x = 0,
		.y = 0,
		.width = float(p.extent.width),
		.height = float(p.extent.height),
		.minDepth = 0,
		.maxDepth = 1,
	};
	vkCmdSetViewport(cmd, 0, 1, &vp);

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics);
	vkCmdPushConstants(cmd, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, viewProj.data());
	vkCmdDrawIndirect(cmd, p.indirectBuf.buffer, 0, 1, sizeof(VkDrawIndirectCommand));

	vkCmdEndRendering(cmd);
}

} // namespace

double RecordAndSubmitBaseline(VkContext &ctx, Pipeline &p, uint32_t cascades,
							   const std::array<float, 16> &viewProj,
							   const std::array<float, 4> &sunDir,
							   const std::array<float, 4> &sunColor,
							   const std::array<float, 4> &camPos,
							   std::vector<uint32_t> &colorHashOut)
{
	// cascades: number of CSM shadow raster passes (each re-rasterizes all geometry,
	// writes only depth). This simulates ProjectV's RecordShadowCommands loop.
	VkCommandBuffer cmd = BeginOneShot(ctx);

	// N shadow raster passes (depth-only, no color write).
	for (uint32_t c = 0; c < cascades; ++c) {
		VkRenderingAttachmentInfo depthAtt{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = p.depthRT.view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.depthStencil = {1.0f, 0}},
		};
		VkRenderingInfo ri{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {{0, 0}, p.extent},
			.layerCount = 1,
			.colorAttachmentCount = 0,
			.pDepthAttachment = &depthAtt,
		};
		vkCmdBeginRendering(cmd, &ri);
		VkViewport vp{
			.x = 0,
			.y = 0,
			.width = float(p.extent.width),
			.height = float(p.extent.height),
			.minDepth = 0,
			.maxDepth = 1,
		};
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.baselineGraphics);
		vkCmdPushConstants(cmd, p.baselineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 16, viewProj.data());
		vkCmdDrawIndirect(cmd, p.indirectBuf.buffer, 0, 1, sizeof(VkDrawIndirectCommand));
		vkCmdEndRendering(cmd);
	}

	// Main forward+ pass with full lighting.
	RecordMainPass(ctx, p, cmd, p.baselineGraphics, p.baselineLayout,
				   p.colorRT.view, p.depthRT.view, viewProj, /*useVisRT=*/false);

	// Readback color RT.
	VkBufferImageCopy bic{
		.bufferOffset = 0,
		.bufferRowLength = p.extent.width,
		.bufferImageHeight = p.extent.height,
		.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		.imageOffset = {0, 0, 0},
		.imageExtent = {p.extent.width, p.extent.height, 1},
	};
	vkCmdCopyImageToBuffer(cmd, p.colorRT.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   p.readbackBuf.buffer, 1, &bic);

	// Submit + wait.
	auto t0 = std::chrono::steady_clock::now();
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd, 0, nullptr};
	vkQueueSubmit(ctx.graphicsQueue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(ctx.graphicsQueue);
	auto t1 = std::chrono::steady_clock::now();

	vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1, &cmd);

	// Compute hash of readback.
	uint32_t h = HashColor((const uint8_t *)p.readbackBuf.mapped, p.readbackBuf.size);
	colorHashOut.push_back(h);

	return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

double RecordAndSubmitVisBuffer(VkContext &ctx, Pipeline &p, uint32_t resolvePasses,
								const std::array<float, 16> &viewProj,
								const std::array<float, 4> &sunDir,
								const std::array<float, 4> &sunColor,
								const std::array<float, 4> &camPos,
								std::vector<uint32_t> &colorHashOut)
{
	(void)resolvePasses; // single resolve for prototype (we only model "the second pass" — most expensive case).

	VkCommandBuffer cmd = BeginOneShot(ctx);

	// 1) Geometry pass — writes vis-buffer + depth. ONE TIME only.
	RecordMainPass(ctx, p, cmd, p.visGeometry, p.visLayout,
				   VK_NULL_HANDLE /* ignored */, p.depthRT.view, viewProj, /*useVisRT=*/true);

	// 2) (resolvePasses + 1) resolve passes — fullscreen quad reads vis-buffer.
	//    Each simulates a separate "light" (CSM cascade / AO / point light).
	//    NO re-rasterization of geometry — that's the win.
	uint32_t totalResolves = resolvePasses + 1;
	for (uint32_t r = 0; r < totalResolves; ++r) {
		VkRenderingAttachmentInfo colorAtt{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = p.colorRT.view,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = (r == 0) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {.color = {{0, 0, 0, 0}}},
		};
		VkRenderingInfo ri{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {{0, 0}, p.extent},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAtt,
			.pDepthAttachment = nullptr,
		};
		vkCmdBeginRendering(cmd, &ri);
		VkViewport vp{0, 0, float(p.extent.width), float(p.extent.height), 0, 1};
		vkCmdSetViewport(cmd, 0, 1, &vp);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, p.visResolve);
		vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen tri
		vkCmdEndRendering(cmd);
	}

	// Readback.
	VkBufferImageCopy bic{
		.bufferOffset = 0,
		.bufferRowLength = p.extent.width,
		.bufferImageHeight = p.extent.height,
		.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
		.imageOffset = {0, 0, 0},
		.imageExtent = {p.extent.width, p.extent.height, 1},
	};
	vkCmdCopyImageToBuffer(cmd, p.colorRT.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   p.readbackBuf.buffer, 1, &bic);

	auto t0 = std::chrono::steady_clock::now();
	vkEndCommandBuffer(cmd);
	VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmd, 0, nullptr};
	vkQueueSubmit(ctx.graphicsQueue, 1, &si, VK_NULL_HANDLE);
	vkQueueWaitIdle(ctx.graphicsQueue);
	auto t1 = std::chrono::steady_clock::now();
	vkFreeCommandBuffers(ctx.device, ctx.cmdPool, 1, &cmd);

	uint32_t h = HashColor((const uint8_t *)p.readbackBuf.mapped, p.readbackBuf.size);
	colorHashOut.push_back(h);

	return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void DestroyPipelines(VkContext &ctx, Pipeline &p)
{
	if (p.baselineGraphics)
		vkDestroyPipeline(ctx.device, p.baselineGraphics, nullptr);
	if (p.visGeometry)
		vkDestroyPipeline(ctx.device, p.visGeometry, nullptr);
	if (p.visResolve)
		vkDestroyPipeline(ctx.device, p.visResolve, nullptr);
	if (p.baselineLayout)
		vkDestroyPipelineLayout(ctx.device, p.baselineLayout, nullptr);
	if (p.visLayout)
		vkDestroyPipelineLayout(ctx.device, p.visLayout, nullptr);

	DestroyImage(ctx, p.colorRT);
	DestroyImage(ctx, p.depthRT);
	DestroyImage(ctx, p.visRT);
	DestroyBuffer(ctx, p.packedFacesBuf);
	DestroyBuffer(ctx, p.chunkDescriptorsBuf);
	DestroyBuffer(ctx, p.materialsBuf);
	DestroyBuffer(ctx, p.indirectBuf);
	DestroyBuffer(ctx, p.readbackBuf);
}

uint32_t HashColor(const uint8_t *pixels, size_t bytes)
{
	// FNV-1a 32-bit.
	uint32_t h = 0x811C9DC5u;
	for (size_t i = 0; i < bytes; i += 16) {
		h ^= pixels[i];
		h *= 0x01000193u;
	}
	return h;
}

} // namespace vb
