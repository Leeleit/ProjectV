#pragma once

#include <expected> // pre-reset rationale: legacy/docs/archive/2026-06-24-pre-reset-snapshot/COMMENTS.md
#include <string_view>

#include "render/vulkan/VulkanSwapchain.hpp"

namespace projectv::vulkan_init {

enum class VulkanInitError : std::uint8_t {
	PreconditionFailed = 0,
	BootstrapFailed,
	BootstrapFailureProbe,
	TracyContextFailed,
	SwapchainFailed,
	WorldCreationFailed,
	WorldFailureProbe,
	EcsSyncFailed,
	PhysicsStateFailed,
	SceneResourcesFailed,
	SceneResourcesFailureProbe,
	GraphicsPipelineProbe,
	GraphicsPipelineFailed,
	ShadowResourcesFailed,
	VoxelMeshingPipelineProbe,
	VoxelMeshingPipelineFailed,
	ModelPipelineFailed,
	ModelManifestFailed,
};

constexpr std::string_view toString(VulkanInitError const e) noexcept {
	switch (e) {
	case VulkanInitError::PreconditionFailed: return "PreconditionFailed";
	case VulkanInitError::BootstrapFailed: return "BootstrapFailed";
	case VulkanInitError::BootstrapFailureProbe: return "BootstrapFailureProbe";
	case VulkanInitError::TracyContextFailed: return "TracyContextFailed";
	case VulkanInitError::SwapchainFailed: return "SwapchainFailed";
	case VulkanInitError::WorldCreationFailed: return "WorldCreationFailed";
	case VulkanInitError::WorldFailureProbe: return "WorldFailureProbe";
	case VulkanInitError::EcsSyncFailed: return "EcsSyncFailed";
	case VulkanInitError::PhysicsStateFailed: return "PhysicsStateFailed";
	case VulkanInitError::SceneResourcesFailed: return "SceneResourcesFailed";
	case VulkanInitError::SceneResourcesFailureProbe: return "SceneResourcesFailureProbe";
	case VulkanInitError::GraphicsPipelineProbe: return "GraphicsPipelineProbe";
	case VulkanInitError::GraphicsPipelineFailed: return "GraphicsPipelineFailed";
	case VulkanInitError::ShadowResourcesFailed: return "ShadowResourcesFailed";
	case VulkanInitError::VoxelMeshingPipelineProbe: return "VoxelMeshingPipelineProbe";
	case VulkanInitError::VoxelMeshingPipelineFailed: return "VoxelMeshingPipelineFailed";
	case VulkanInitError::ModelPipelineFailed: return "ModelPipelineFailed";
	case VulkanInitError::ModelManifestFailed: return "ModelManifestFailed";
	}
	return "Unknown";
}
} // namespace projectv::vulkan_init

std::expected<void, projectv::vulkan_init::VulkanInitError> InitVulkan(AppState *state);

