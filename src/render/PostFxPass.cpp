#include "render/PostFx.hpp" // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include "core/RuntimeDiagnostics.hpp"
#include "debug/Profiling.hpp"
#include "render/RendererInternal.hpp"
#include "render/vulkan/VulkanDebug.hpp"

#include <array>

namespace {

void PostFxMemoryBarrier(VkCommandBuffer commandBuffer)
{
	VkMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.memoryBarrierCount = 1;
	depInfo.pMemoryBarriers = &barrier;
	vkCmdPipelineBarrier2(commandBuffer, &depInfo);
}

void DispatchPostFx(
	VkCommandBuffer cmd,
	VkPipeline pipeline,
	VkPipelineLayout layout,
	VkDescriptorSet descriptorSet,
	const projectv::render::PostFxPushConstants &pushConstants,
	uint32_t groupCountX,
	uint32_t groupCountY)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(
		cmd,
		VK_PIPELINE_BIND_POINT_COMPUTE,
		layout,
		0,
		1,
		&descriptorSet,
		0,
		nullptr);
	vkCmdPushConstants(
		cmd,
		layout,
		VK_SHADER_STAGE_COMPUTE_BIT,
		0,
		sizeof(pushConstants),
		&pushConstants);
	vkCmdDispatch(cmd, groupCountX, groupCountY, 1);
}

} // namespace

namespace projectv::render {
bool RecordPostFxPass(
	VkCommandBuffer commandBuffer,
	VulkanContextState &context,
	RenderState &render,
	const VoxelSceneLighting &lighting,
	const FrameRenderData &frameRenderData,
	const VkExtent2D extent,
	const uint32_t frameIndex)
{
	PV_PROFILE_ZONE_N("RecordPostFxPass");
	if (commandBuffer == VK_NULL_HANDLE) {
		return false;
	}
	if (!IsPostFxEnabled()) {
		return true;
	}
	if (render.postFxDescriptorSets.empty()) {
		return false;
	}

	const VkExtent2D halfExtent{
		std::max(extent.width / 2u, 1u),
		std::max(extent.height / 2u, 1u)};

	const bool bloomEnabled = IsBloomEnabled();
	const bool aerialEnabled = IsAerialPerspectiveEnabled();

	// Transition sceneColor from COLOR_ATTACHMENT_OPTIMAL to SHADER_READ_ONLY_OPTIMAL.
	::TransitionImage(
		commandBuffer,
		render.sceneColorImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		render.sceneColorCurrentLayout,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	render.sceneColorCurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Transition depth to DEPTH_READ_ONLY_OPTIMAL for composite sampler.
	::TransitionImage(
		commandBuffer,
		render.depthImage,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		render.depthImageCurrentLayout,
		VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
		VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
	render.depthImageCurrentLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;

	// Transition postFxOutput to GENERAL for compute writes.
	::TransitionImage(
		commandBuffer,
		render.postFxOutputImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_2_NONE,
		0,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_WRITE_BIT);

	PostFxPushConstants push{};
	push.params0[0] = 0.8f;								   // threshold
	push.params0[1] = 0.5f;								   // softKnee
	push.params0[2] = 0.5f;								   // bloomIntensity
	push.params1[0] = lighting.skyColorAndFogDensity[3];   // fogDensity
	push.params1[1] = lighting.horizonColorAndFogStart[3]; // fogMax
	push.params1[2] = lighting.postProcess[0];			   // exposure
	push.params2[0] = lighting.sunDirectionAndWrap[0];
	push.params2[1] = lighting.sunDirectionAndWrap[1];
	push.params2[2] = lighting.sunDirectionAndWrap[2];
	push.params3[0] = frameRenderData.graphicsPushConstants.cameraPosition.x;
	push.params3[1] = frameRenderData.graphicsPushConstants.cameraPosition.y;
	push.params3[2] = frameRenderData.graphicsPushConstants.cameraPosition.z;
	push.params4[0] = lighting.horizonColorAndFogStart[0];
	push.params4[1] = lighting.horizonColorAndFogStart[1];
	push.params4[2] = lighting.horizonColorAndFogStart[2];

	constexpr uint32_t kSetsPerFrame = kPostFxDescriptorSetCount / MAX_FRAMES_IN_FLIGHT;
	uint32_t setIndex = frameIndex * kSetsPerFrame;
	const auto allocateSet = [&]() -> VkDescriptorSet {
		if (setIndex >= render.postFxDescriptorSets.size()) {
			return VK_NULL_HANDLE;
		}
		return render.postFxDescriptorSets[setIndex++];
	};

	// Helper to update a descriptor set with up to four bindings.
	const auto updateSet = [&](
							   VkDescriptorSet descriptorSet,
							   VkSampler sampler,
							   VkImageView view0,
							   VkImageLayout layout0,
							   VkImageView view1,
							   VkImageLayout layout1,
							   VkImageView view2,
							   VkImageLayout layout2,
							   VkImageView view3,
							   VkImageLayout layout3,
							   bool useView1,
							   bool useView3) {
		std::array<VkDescriptorImageInfo, 4> infos{};
		std::array<VkWriteDescriptorSet, 4> writes{};
		uint32_t writeCount = 0;

		infos[0] = {sampler, view0, layout0};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[0], nullptr, nullptr};
		++writeCount;

		// Binding 1 is only used by the composite shader; provide a valid fallback view when unused.
		infos[1] = {sampler, useView1 ? view1 : view0, layout1};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[1], nullptr, nullptr};
		++writeCount;

		infos[2] = {VK_NULL_HANDLE, view2, layout2};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &infos[2], nullptr, nullptr};
		++writeCount;

		infos[3] = {sampler, useView3 ? view3 : view0, layout3};
		writes[writeCount] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[3], nullptr, nullptr};
		++writeCount;

		vkUpdateDescriptorSets(context.device, writeCount, writes.data(), 0, nullptr);
	};

	if (bloomEnabled) {
		// Bloom threshold: sceneColor -> bloom scratch mip 0.
		{
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			push.params0[3] = 0.0f;
			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.bloomScratchMipViews[0],
				VK_IMAGE_LAYOUT_GENERAL,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				false,
				false);

			const uint32_t groupsX = (halfExtent.width + 15) / 16;
			const uint32_t groupsY = (halfExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomThresholdPipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
		}

		PostFxMemoryBarrier(commandBuffer);

		// Bloom downsample chain.
		for (uint32_t mip = 0; mip + 1 < kBloomMipCount; ++mip) {
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			const VkExtent2D dstExtent{
				std::max(halfExtent.width >> (mip + 1), 1u),
				std::max(halfExtent.height >> (mip + 1), 1u)};
			push.params0[3] = static_cast<float>(mip);

			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip + 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[mip],
				VK_IMAGE_LAYOUT_GENERAL,
				false,
				false);

			const uint32_t groupsX = (dstExtent.width + 15) / 16;
			const uint32_t groupsY = (dstExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomDownsamplePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);

			PostFxMemoryBarrier(commandBuffer);
		}

		// Bloom upsample chain: start from top mip and write into bloomResultImage.
		{
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			push.params0[3] = static_cast<float>(kBloomMipCount - 1);

			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomResultImageView,
				VK_IMAGE_LAYOUT_GENERAL,
				render.bloomScratchMipViews[kBloomMipCount - 1],
				VK_IMAGE_LAYOUT_GENERAL,
				false,
				false);

			const uint32_t groupsX = (halfExtent.width + 15) / 16;
			const uint32_t groupsY = (halfExtent.height + 15) / 16;
			DispatchPostFx(commandBuffer, render.bloomUpsamplePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
			PostFxMemoryBarrier(commandBuffer);
		}
	}

	// Composite pass: sceneColor + depth + bloomResult -> postFxOutput.
	{
		push.params0[3] = aerialEnabled ? 1.0f : 0.0f;
		const uint32_t groupsX = (extent.width + 15) / 16;
		const uint32_t groupsY = (extent.height + 15) / 16;

		if (render.postFxBindlessEnabled &&
			frameIndex < render.postFxBindlessDescriptorSets.size() &&
			render.postFxBindlessCompositePipeline != VK_NULL_HANDLE) {
			const VkDescriptorSet descriptorSet = render.postFxBindlessDescriptorSets[frameIndex];
			const VkImageView bloomView = bloomEnabled ? render.bloomResultImageView : render.sceneColorImageView;
			const VkImageLayout bloomLayout =
				bloomEnabled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			std::array<VkDescriptorImageInfo, 3> samplerInfos{};
			samplerInfos[0] = {render.postFxLinearSampler, render.sceneColorImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
			samplerInfos[1] = {render.postFxLinearSampler, render.depthImageView, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
			samplerInfos[2] = {render.postFxLinearSampler, bloomView, bloomLayout};
			const VkDescriptorImageInfo storageInfo{VK_NULL_HANDLE, render.postFxOutputImageView, VK_IMAGE_LAYOUT_GENERAL};
			std::array<VkWriteDescriptorSet, 2> writes{};
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].dstSet = descriptorSet;
			writes[0].dstBinding = 0;
			writes[0].descriptorCount = 1u;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[0].pImageInfo = &storageInfo;
			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].dstSet = descriptorSet;
			writes[1].dstBinding = 1;
			writes[1].descriptorCount = 3u;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[1].pImageInfo = samplerInfos.data();
			vkUpdateDescriptorSets(context.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
			DispatchPostFx(
				commandBuffer,
				render.postFxBindlessCompositePipeline,
				render.postFxBindlessPipelineLayout,
				descriptorSet,
				push,
				groupsX,
				groupsY);
		} else {
			const VkDescriptorSet descriptorSet = allocateSet();
			if (descriptorSet == VK_NULL_HANDLE) {
				return false;
			}
			updateSet(
				descriptorSet,
				render.postFxLinearSampler,
				render.sceneColorImageView,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				render.depthImageView,
				VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
				render.postFxOutputImageView,
				VK_IMAGE_LAYOUT_GENERAL,
				bloomEnabled ? render.bloomResultImageView : render.sceneColorImageView,
				bloomEnabled ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				true,
				bloomEnabled);
			DispatchPostFx(commandBuffer, render.bloomCompositePipeline, render.postFxPipelineLayout, descriptorSet, push, groupsX, groupsY);
		}
	}

	// Transition postFxOutput to TRANSFER_SRC for blit.
	::TransitionImage(
		commandBuffer,
		render.postFxOutputImage,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		VK_ACCESS_2_SHADER_WRITE_BIT,
		VK_PIPELINE_STAGE_2_COPY_BIT,
		VK_ACCESS_2_TRANSFER_READ_BIT);

	return true;
}
} // namespace projectv::render
