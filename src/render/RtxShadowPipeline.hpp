#pragma once

#include <vulkan/vulkan.h> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md

#include <cstdint>

#include "core/Types.hpp"

namespace projectv::render {

struct RtxShadowPipelineConfig {
	uint32_t shaderGroupHandleSize = 32u;
	uint32_t shaderGroupBaseAlignment = 64u;
	uint32_t shaderGroupHandleAlignment = 16u;
	bool shaderInvocationReorderEnabled = false;
	bool traversalMetricsEnabled = false;
};

class RtxShadowPipeline {
  public:
	RtxShadowPipeline() = default;
	~RtxShadowPipeline() = default;

	RtxShadowPipeline(const RtxShadowPipeline &) = delete;
	RtxShadowPipeline &operator=(const RtxShadowPipeline &) = delete;

	bool Initialize(
		const VulkanContextState &context,
		const RtxShadowPipelineConfig &config);

	void Shutdown(const VulkanContextState &context) noexcept;

	[[nodiscard]] bool IsReady() const noexcept { return m_pipeline != VK_NULL_HANDLE; }

	[[nodiscard]] VkPipeline GetPipeline() const noexcept { return m_pipeline; }
	[[nodiscard]] VkPipelineLayout GetPipelineLayout() const noexcept { return m_pipelineLayout; }
	[[nodiscard]] const VkDescriptorSetLayout &GetDescriptorSetLayout() const noexcept { return m_descriptorSetLayout; }
	[[nodiscard]] static uint32_t GetRayGenGroupIndex() noexcept { return 0u; }
	[[nodiscard]] static uint32_t GetMissGroupIndex() noexcept { return 1u; }
	[[nodiscard]] static uint32_t GetHitGroupIndex() noexcept { return 2u; }

  private:
	VkShaderModule m_rayGenModule = VK_NULL_HANDLE;
	VkShaderModule m_intersectionModule = VK_NULL_HANDLE;
	VkShaderModule m_closestHitModule = VK_NULL_HANDLE;
	VkShaderModule m_missModule = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;

	// Pipeline-library path. Kept as members so Shutdown can destroy them in order.
	VkPipeline m_rayGenLibrary = VK_NULL_HANDLE;
	VkPipeline m_missLibrary = VK_NULL_HANDLE;
	VkPipeline m_hitGroupLibrary = VK_NULL_HANDLE;
};

} // namespace projectv::render
